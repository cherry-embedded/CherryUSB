/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Initial AIC8800 FMAC command-channel bring-up for CherryUSB/FreeRTOS.
 * This brings the D80 runtime through the vendor driver's early initialization
 * sequence. Scan and association are added on top of this path.
 */
#include "aic8800_fmac.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "usb_config.h"
#include "usb_errno.h"
#include "usb_osal.h"
#include "aic8800_wpa2.h"
#include "aic8800_wifi.h"
#include "aic8800_wifi_priv.h"
#include "aic8800_lwip.h"
#include "lwip/priv/tcp_priv.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "aic8800.fmac"
#include "usb_log.h"

#ifndef AIC8800_WIFI_AUTO_CONNECT
#define AIC8800_WIFI_AUTO_CONNECT 1
#endif

#ifndef AIC8800_FMAC_OSAL_PRIO
#define AIC8800_FMAC_OSAL_PRIO ((uint32_t)(configMAX_PRIORITIES - 1U - 4U))
#endif

enum aic_fmac_request {
    AIC_FMAC_REQ_SCAN = 1U,
    AIC_FMAC_REQ_CONNECT = 2U,
    AIC_FMAC_REQ_DISCONNECT = 3U
};

#define AIC_USB_TYPE_CONFIG                 0x10U
#define AIC_USB_TYPE_COMMAND                0x11U
#define AIC_USB_TYPE_PRINT                  0x13U
#define AIC_USB_TYPE_DATA_TX                0x01U
#define AIC_WIRE_TASK_MM                       0U
#define AIC_WIRE_DRIVER_TASK                 100U
#define AIC_WIRE_MSG(task, index) \
    ((uint16_t)(((uint16_t)(task) << 10) | (uint16_t)(index)))

#define AIC_MM_SET_STACK_START_REQ AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 123U)
#define AIC_MM_SET_STACK_START_CFM AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 124U)
#define AIC_MM_GET_FW_VERSION_REQ  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 128U)
#define AIC_MM_GET_FW_VERSION_CFM  AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 129U)
#define AIC_MM_SET_TXPWR_REQ       AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 119U)
#define AIC_MM_SET_TXPWR_CFM       AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 120U)
#define AIC_MM_SET_RF_CALIB_REQ    AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 105U)
#define AIC_MM_SET_RF_CALIB_CFM    AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 106U)
#define AIC_MM_GET_MAC_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 115U)
#define AIC_MM_GET_MAC_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 116U)
#define AIC_MM_RESET_REQ           AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 0U)
#define AIC_MM_RESET_CFM           AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 1U)
#define AIC_MM_START_REQ           AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 2U)
#define AIC_MM_START_CFM           AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 3U)
#define AIC_MM_VERSION_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 4U)
#define AIC_MM_VERSION_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 5U)
#define AIC_MM_ADD_IF_REQ          AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 6U)
#define AIC_MM_ADD_IF_CFM          AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 7U)
#define AIC_MM_KEY_ADD_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 36U)
#define AIC_MM_KEY_ADD_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 37U)
#define AIC_MM_SET_COEX_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 101U)
#define AIC_MM_SET_COEX_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_MM, 102U)
#define AIC_WIRE_TASK_ME                       5U
#define AIC_ME_CONFIG_REQ          AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 0U)
#define AIC_ME_CONFIG_CFM          AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 1U)
#define AIC_ME_CHAN_CONFIG_REQ     AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 2U)
#define AIC_ME_CHAN_CONFIG_CFM     AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 3U)
#define AIC_ME_CONTROL_PORT_REQ    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 4U)
#define AIC_ME_CONTROL_PORT_CFM    AIC_WIRE_MSG(AIC_WIRE_TASK_ME, 5U)
#define AIC_WIRE_TASK_SCANU                    4U
#define AIC_SCANU_START_REQ        AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 0U)
#define AIC_SCANU_START_CFM        AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 1U)
#define AIC_SCANU_RESULT_IND       AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 4U)
#define AIC_SCANU_START_ACCEPTED   AIC_WIRE_MSG(AIC_WIRE_TASK_SCANU, 9U)
#define AIC_WIRE_TASK_SM                       6U
#define AIC_SM_CONNECT_REQ         AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 0U)
#define AIC_SM_CONNECT_CFM         AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 1U)
#define AIC_SM_CONNECT_IND         AIC_WIRE_MSG(AIC_WIRE_TASK_SM, 2U)

/* Vendor sm_connect_ind is not packed. These offsets are shared by the D80
 * USB firmware and the reference RT-Smart driver. */
#define AIC_SM_CONNECT_IND_BAND_OFFSET       822U
#define AIC_SM_CONNECT_IND_FREQUENCY_OFFSET  824U
#define AIC_SM_CONNECT_IND_WIDTH_OFFSET      826U
#define AIC_SM_CONNECT_IND_CENTER1_OFFSET    828U
#define AIC_SM_CONNECT_IND_CENTER2_OFFSET    832U
#define AIC_SM_CONNECT_IND_CHANNEL_END       836U

#define AIC_FMAC_TX_SIZE                    416U
#define AIC_FMAC_CONTROL_RX_SIZE           4096U
#define AIC_FMAC_RX_SIZE AIC8800_FMAC_RX_BUFFER_SIZE
#define AIC_FMAC_RX_PIPE_COUNT AIC8800_FMAC_RX_PIPE_COUNT
#define AIC_FMAC_RX_FREE_ALL \
    ((uint8_t)((1U << AIC_FMAC_RX_PIPE_COUNT) - 1U))
#define AIC_FMAC_TIMEOUT_MS                3000U
#define AIC_FMAC_SCAN_TIMEOUT_MS          10000U
#define AIC_FMAC_CONNECT_TIMEOUT_MS       10000U
#define AIC_FMAC_MAX_RX_TRANSFERS             8U
#define AIC_FMAC_MAX_SCAN_TRANSFERS          128U
#define AIC_FMAC_EAPOL_RX_TRANSFERS           32U
#define AIC_FMAC_USB_RX_HEADER_SIZE           60U
#define AIC_FMAC_ETHERNET_MAX               1514U
#define AIC_FMAC_DATA_TX_SIZE               1600U
#define AIC_FMAC_DATA_TX_TIMEOUT_MS          1000U
#define AIC_FMAC_TX_QUEUE_DEPTH                64U
#define AIC_FMAC_TX_PRIORITY_RESERVE            4U
#define AIC_FMAC_TX_TASK_STACK                768U
#define AIC_FMAC_TX_TASK_PRIORITY (tskIDLE_PRIORITY + 5U)
#define AIC_FMAC_USB_SLOW_MS                    10U
#define AIC_FMAC_TX_WAIT_MS                     20U
#define AIC_FMAC_TCP_ACK_PACE_HZ              1000U
#define AIC_TCP_FIN                           0x01U
#define AIC_TCP_SYN                           0x02U
#define AIC_TCP_RST                           0x04U

struct aic_fmac_tx_record {
    uint16_t length;
    uint8_t frame[AIC_FMAC_ETHERNET_MAX];
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_fmac_tx[AIC_FMAC_TX_SIZE];

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_fmac_rx[AIC_FMAC_CONTROL_RX_SIZE];

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_data_rx[AIC_FMAC_RX_PIPE_COUNT][AIC_FMAC_RX_SIZE];

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_data_tx[AIC_FMAC_DATA_TX_SIZE];

/* CPU-only staging records live in the HyperRAM range reserved by
 * freertos_external_heap.c.  The worker copies one record at a time into the
 * internal, non-cacheable USB DMA buffer above. */
static struct aic_fmac_tx_record
    g_aic_tx_records[AIC_FMAC_TX_QUEUE_DEPTH]
    __attribute__((section(AIC8800_FMAC_TX_QUEUE_SECTION), aligned(32)));

static volatile bool g_aic_fmac_task_running;
static volatile bool g_aic_radio_ready;
static usb_osal_mq_t g_aic_fmac_mq;
static struct aic8800_usb_device *g_aic_fmac_device;
static uint8_t g_aic_station_mac[6];
static uint8_t g_aic_station_vif = 0xffU;
static uint8_t g_aic_ap_station = 0xffU;
static uint32_t g_aic_scan_results_2ghz;
static uint32_t g_aic_scan_results_5ghz;

static struct aic8800_wifi_ap g_aic_target_ap;
static uint8_t g_aic_target_pmk[32];

struct aic_eapol_m1 {
    uint8_t protocol_version;
    uint8_t descriptor_type;
    uint8_t key_length[2];
    uint8_t replay_counter[8];
    uint8_t anonce[32];
};

static struct aic_eapol_m1 g_aic_eapol_m1;
static uint8_t g_aic_snonce[32];
static uint8_t g_aic_ptk[64];
static volatile bool g_aic_wpa2_connected;
static SemaphoreHandle_t g_aic_tx_mutex;
static QueueHandle_t g_aic_tx_free_queue;
static QueueHandle_t g_aic_tx_pending_queue;
static struct aic_fmac_tx_record g_aic_tx_ack_hold;
static volatile bool g_aic_tx_ack_hold_valid;
static volatile uint32_t g_aic_tx_ack_flush_cycles;
static volatile bool g_aic_data_tx_active;
static volatile bool g_aic_data_tx_stop;
static volatile bool g_aic_data_tx_task_running;
static struct aic8800_fmac_tx_stats g_aic_tx_stats;

struct aic_fmac_rx_done {
    uint8_t index;
    int nbytes;
    uint32_t ready_cycles;
};

static TaskHandle_t g_aic_rx_task;
static struct aic8800_usb_device *g_aic_rx_device;
static volatile bool g_aic_rx_stop;
static volatile uint8_t g_aic_rx_free_mask;
static volatile uint8_t g_aic_rx_in_flight;
static volatile uint8_t g_aic_rx_q_head;
static volatile uint8_t g_aic_rx_q_count;
static volatile struct aic_fmac_rx_done g_aic_rx_q[AIC_FMAC_RX_PIPE_COUNT];
static uint64_t g_aic_rx_process_cycles;
static uint32_t g_aic_rx_process_max_cycles;
static uint32_t g_aic_rx_schedule_max_cycles;
static uint32_t g_aic_rx_lwip_lock_max_cycles;
static uint32_t g_aic_rx_rearm_max_cycles;

/* USB uses the complete 95-byte TX-power union even though the D80 v3 member
 * occupies only the first 69 bytes. The unused v4 tail must remain zero. */
static const uint8_t g_aic_d80_tx_power[95] = {
    1U,
    20U, 20U, 20U, 20U, 20U, 20U, 20U, 20U, 18U, 18U, 16U, 16U,
    20U, 20U, 20U, 20U, 18U, 18U, 16U, 16U, 16U, 16U,
    20U, 20U, 20U, 20U, 18U, 18U, 16U, 16U, 16U, 16U, 15U, 15U,
    0x80U, 0x80U, 0x80U, 0x80U, 20U, 20U, 20U, 20U,
    18U, 18U, 16U, 16U,
    20U, 20U, 20U, 20U, 18U, 18U, 16U, 16U, 16U, 15U,
    20U, 20U, 20U, 20U, 18U, 18U, 16U, 16U, 16U, 15U, 14U, 14U,
};

static const uint16_t g_aic_channels_2ghz[14] = {
    2412U, 2417U, 2422U, 2427U, 2432U, 2437U, 2442U,
    2447U, 2452U, 2457U, 2462U, 2467U, 2472U, 2484U,
};

static const uint16_t g_aic_channels_5ghz[25] = {
    5180U, 5200U, 5220U, 5240U, 5260U, 5280U, 5300U, 5320U,
    5500U, 5520U, 5540U, 5560U, 5580U, 5600U, 5620U, 5640U,
    5660U, 5680U, 5700U, 5720U, 5745U, 5765U, 5785U, 5805U,
    5825U,
};

static uint16_t aic_get_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint16_t aic_get_be16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static uint32_t aic_get_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static uint32_t aic_fmac_channel_width_mhz(uint8_t width)
{
    switch (width) {
    case 1U: return 40U;
    case 2U: return 80U;
    case 3U:
    case 4U: return 160U;
    default: return 20U;
    }
}

static void aic_put_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void aic_put_be16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void aic_put_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

/* RFC 1624 incremental update of a 16-bit one's-complement checksum. */
static uint16_t aic_checksum_adjust(uint16_t checksum, uint16_t old_value,
                                    uint16_t new_value)
{
    uint32_t sum;

    if (old_value == new_value) {
        return checksum;
    }
    sum = ((uint32_t)(~checksum) & 0xffffU) +
          ((uint32_t)(~old_value) & 0xffffU) + new_value;
    sum = (sum >> 16) + (sum & 0xffffU);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

static uint32_t aic_get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void aic_put_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static bool aic_mac_valid(const uint8_t address[6])
{
    return ((address[0] & 1U) == 0U) &&
           ((address[0] | address[1] | address[2] |
             address[3] | address[4] | address[5]) != 0U);
}

static void aic_build_me_config(uint8_t request[112])
{
    static const uint8_t he_mac_capability[6] = {
        0x00U, 0x00U, 0x02U, 0x00U, 0x00U, 0x00U,
    };
    static const uint8_t he_phy_capability[11] = {
        0x06U, 0xe0U, 0x2bU, 0x58U, 0x0dU, 0xc0U,
        0xcfU, 0x04U, 0x02U, 0x30U, 0x00U,
    };
    static const uint8_t he_ppe_thresholds[25] = {
        0x38U, 0x1cU, 0xc7U, 0x01U,
    };

    memset(request, 0, 112U);

    /* struct aic_wire_ht_capability, offset 0, ABI size 32. */
    aic_put_le16(request + 0U, 0x0963U);
    request[2] = 0x1fU;
    request[3] = 0xffU;
    request[7] = 0x01U;
    aic_put_le16(request + 13U, 150U);
    request[15] = 0x01U;

    /* struct aic_wire_vht_capability, offset 32, ABI size 12. The v9
     * compact feature map advertises VHT, SU/MU beamformee and 80 MHz. */
    aic_put_le32(request + 32U, 0x03907132U);
    aic_put_le16(request + 36U, 0xfffeU);
    aic_put_le16(request + 38U, 390U);
    aic_put_le16(request + 40U, 0xfffeU);
    aic_put_le16(request + 42U, 390U);

    /* struct aic_wire_he_capability, offset 44, ABI size 56. */
    memcpy(request + 44U, he_mac_capability, sizeof(he_mac_capability));
    memcpy(request + 50U, he_phy_capability, sizeof(he_phy_capability));
    aic_put_le16(request + 62U, 0xfffeU);
    aic_put_le16(request + 64U, 0xfffeU);
    aic_put_le16(request + 66U, 0xffffU);
    aic_put_le16(request + 68U, 0xffffU);
    aic_put_le16(request + 70U, 0xffffU);
    aic_put_le16(request + 72U, 0xffffU);
    memcpy(request + 74U, he_ppe_thresholds, sizeof(he_ppe_thresholds));

    aic_put_le16(request + 100U, 1000U);
    request[102] = 2U; /* 80 MHz */
    request[103] = 1U; /* HT */
    request[104] = 1U; /* VHT */
    request[105] = 1U; /* HE */
    /* Keep HE uplink, PS, antenna diversity and DPSM disabled initially. */
}

static void aic_encode_channel(uint8_t *destination, uint16_t frequency,
                               uint8_t band, uint8_t flags)
{
    aic_put_le16(destination, frequency);
    destination[2] = band;
    destination[3] = flags;
    destination[4] = 20U;
    destination[5] = 0U;
}

static void aic_build_channel_config(uint8_t request[254])
{
    uint32_t index;

    memset(request, 0, 254U);
    for (index = 0U; index < 14U; index++) {
        uint8_t flags = (index >= 11U) ? 1U : 0U; /* NO_IR ch12-14 */
        aic_encode_channel(request + index * 6U, g_aic_channels_2ghz[index],
                           0U, flags);
    }
    for (index = 0U; index < 25U; index++) {
        /* DFS ranges are RADAR | NO_IR; the other channels are active. */
        uint8_t flags = ((index >= 4U) && (index <= 19U)) ? 5U : 0U;
        aic_encode_channel(request + (14U + index) * 6U,
                           g_aic_channels_5ghz[index], 1U, flags);
    }
    request[252] = 14U;
    request[253] = 25U;
}

static void aic_build_scan_request(uint8_t request[376], bool five_ghz)
{
    const uint16_t *channels = five_ghz ? g_aic_channels_5ghz :
                                          g_aic_channels_2ghz;
    uint32_t channel_count = five_ghz ? 25U : 14U;
    uint32_t index;

    memset(request, 0, 376U);
    for (index = 0U; index < channel_count; index++) {
        uint8_t flags;

        if (five_ghz) {
            flags = ((index >= 4U) && (index <= 19U)) ? 5U : 0U;
        } else {
            flags = (index >= 11U) ? 1U : 0U;
        }
        aic_encode_channel(request + index * 6U, channels[index],
                           five_ghz ? 1U : 0U, flags);
    }
    memset(request + 352U, 0xff, 6U); /* wildcard BSSID */
    request[366] = g_aic_station_vif;
    request[367] = (uint8_t)channel_count;
    request[368] = 1U; /* one zero-length SSID means active wildcard scan */
}

static void aic_handle_scan_result(const uint8_t *parameter, uint32_t length)
{
    uint32_t frame_length;
    const uint8_t *frame;
    const uint8_t *ies;
    const uint8_t *bssid;
    uint32_t ies_length;
    uint32_t offset;
    uint16_t frequency;
    int8_t rssi;
    char ssid[33];
    uint32_t ssid_length = 0U;
    uint8_t rsn_ie[AIC8800_WIFI_RSN_IE_MAX];
    uint32_t rsn_length = 0U;

    if ((parameter == NULL) || (length < 12U)) {
        return;
    }
    frame_length = aic_get_le16(parameter);
    if ((frame_length < 36U) || (frame_length > length - 12U)) {
        USB_LOG_WRN("invalid scan result frame=%u parameter=%u\r\n",
                    (unsigned int)frame_length, (unsigned int)length);
        return;
    }
    frame = parameter + 12U;
    bssid = frame + 16U;
    ies = frame + 36U;
    ies_length = frame_length - 36U;
    memset(ssid, 0, sizeof(ssid));
    memset(rsn_ie, 0, sizeof(rsn_ie));
    for (offset = 0U; offset + 2U <= ies_length; ) {
        uint32_t ie_id = ies[offset];
        uint32_t ie_length = ies[offset + 1U];
        uint32_t total;
        uint32_t index;

        if (offset + 2U + ie_length > ies_length) {
            break;
        }
        total = 2U + ie_length;
        if (ie_id == 0U) {
            ssid_length = (ie_length < sizeof(ssid) - 1U) ?
                          ie_length : sizeof(ssid) - 1U;
            for (index = 0U; index < ssid_length; index++) {
                uint8_t character = ies[offset + 2U + index];
                ssid[index] = ((character >= 0x20U) && (character < 0x7fU)) ?
                              (char)character : '.';
            }
        } else if ((ie_id == 0x30U) && (total <= sizeof(rsn_ie))) {
            memcpy(rsn_ie, ies + offset, total);
            rsn_length = total;
        }
        offset += total;
    }
    frequency = aic_get_le16(parameter + 4U);
    rssi = (int8_t)parameter[9U];
    if (frequency < 5000U) {
        g_aic_scan_results_2ghz++;
    } else {
        g_aic_scan_results_5ghz++;
    }
    USB_LOG_INFO("scan AP %02x:%02x:%02x:%02x:%02x:%02x freq=%u RSSI=%d SSID=\"%.*s\"\r\n",
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
                 frequency, rssi, (int)ssid_length, ssid);
    aic8800_wifi_offer_scan_ap(bssid, frequency, rssi, ssid, ssid_length,
                               rsn_ie, rsn_length);
}

static int aic_fmac_find_confirmation(const uint8_t *buffer, uint32_t length,
                                      uint16_t expected_id, void *response,
                                      uint32_t response_capacity,
                                      uint32_t *response_length)
{
    const uint8_t *cursor = buffer;
    uint32_t remaining = length;

    while (remaining >= 4U) {
        uint16_t packet_length = aic_get_le16(cursor);
        uint8_t type = cursor[2] & 0x7fU;
        uint32_t raw_length;
        uint32_t record_length;

        if ((packet_length == 0U) && (type == 0U)) {
            break;
        }
        raw_length = ((type & AIC_USB_TYPE_CONFIG) != 0U) ?
                     (uint32_t)packet_length + 4U :
                     (uint32_t)packet_length + 8U;
        if ((raw_length < 4U) || (raw_length > remaining)) {
            USB_LOG_ERR("invalid runtime record type=%02x packet=%u rx=%u\r\n",
                        type, packet_length, (unsigned int)remaining);
            return -USB_ERR_INVAL;
        }
        record_length = (raw_length + 3U) & ~3U;
        if (record_length > remaining) {
            record_length = raw_length;
        }

        if ((type == AIC_USB_TYPE_COMMAND) && (raw_length >= 16U)) {
            uint16_t message_id = aic_get_le16(cursor + 4U);
            uint16_t parameter_length = aic_get_le16(cursor + 10U);

            if (parameter_length > (raw_length - 16U)) {
                return -USB_ERR_INVAL;
            }
            USB_LOG_INFO("RX command=0x%04x parameter=%u\r\n",
                         message_id, parameter_length);
            if (message_id == AIC_SCANU_RESULT_IND) {
                aic_handle_scan_result(cursor + 16U, parameter_length);
            }
            if (message_id == expected_id) {
                if ((response != NULL) &&
                    (parameter_length > response_capacity)) {
                    return -USB_ERR_RANGE;
                }
                if ((parameter_length != 0U) && (response != NULL)) {
                    memcpy(response, cursor + 16U, parameter_length);
                }
                if (response_length != NULL) {
                    *response_length = parameter_length;
                }
                return 1;
            }
        } else if ((type == AIC_USB_TYPE_PRINT) && (raw_length > 4U)) {
            USB_LOG_INFO("firmware: %.*s\r\n", (int)(raw_length - 4U),
                         (const char *)(cursor + 4U));
        } else {
            USB_LOG_WRN("ignored runtime record type=0x%02x length=%u\r\n",
                        type, (unsigned int)raw_length);
        }

        cursor += record_length;
        remaining -= record_length;
    }
    return 0;
}

static int aic_fmac_command(struct aic8800_usb_device *device,
                            uint16_t request_id, uint16_t confirmation_id,
                            const void *request, uint32_t request_length,
                            void *response, uint32_t response_capacity,
                            uint32_t *response_length)
{
    uint32_t frame_length = 16U + request_length;
    uint32_t receive_index;
    int result;

    if ((device == NULL) || (frame_length > sizeof(g_aic_fmac_tx)) ||
        ((request_length != 0U) && (request == NULL))) {
        return -USB_ERR_INVAL;
    }
    if (response_length != NULL) {
        *response_length = 0U;
    }

    memset(g_aic_fmac_tx, 0, frame_length);
    aic_put_le16(g_aic_fmac_tx, (uint16_t)(request_length + 12U));
    g_aic_fmac_tx[2] = AIC_USB_TYPE_COMMAND;
    aic_put_le16(g_aic_fmac_tx + 8U, request_id);
    aic_put_le16(g_aic_fmac_tx + 10U, request_id >> 10);
    aic_put_le16(g_aic_fmac_tx + 12U, AIC_WIRE_DRIVER_TASK);
    aic_put_le16(g_aic_fmac_tx + 14U, (uint16_t)request_length);
    if (request_length != 0U) {
        memcpy(g_aic_fmac_tx + 16U, request, request_length);
    }

    USB_LOG_INFO("TX command=0x%04x parameter=%u\r\n",
                 request_id, (unsigned int)request_length);
    result = aic8800_usb_bulk_send(device, g_aic_fmac_tx, frame_length,
                                   AIC_FMAC_TIMEOUT_MS, true);
    if (result != (int)frame_length) {
        return (result < 0) ? result : -USB_ERR_IO;
    }

    for (receive_index = 0U;
         receive_index < AIC_FMAC_MAX_RX_TRANSFERS;
         receive_index++) {
        memset(g_aic_fmac_rx, 0, sizeof(g_aic_fmac_rx));
        result = aic8800_usb_bulk_receive(device, g_aic_fmac_rx,
                                          sizeof(g_aic_fmac_rx),
                                          AIC_FMAC_TIMEOUT_MS, true);
        if (result < 0) {
            return result;
        }
        result = aic_fmac_find_confirmation(g_aic_fmac_rx, (uint32_t)result,
                                            confirmation_id, response,
                                            response_capacity,
                                            response_length);
        if (result != 0) {
            return (result > 0) ? 0 : result;
        }
    }
    return -USB_ERR_TIMEOUT;
}

static int aic_fmac_scan_band(struct aic8800_usb_device *device,
                              bool five_ghz)
{
    uint8_t scan_request[376];
    /* D80 firmware appends the scanned-band byte after vif/status. */
    uint8_t scan_confirmation[4] = { 0U, 0xffU, 0U, 0U };
    uint32_t response_length = 0U;
    uint32_t receive_index;
    int result;

    aic_build_scan_request(scan_request, five_ghz);
    USB_LOG_INFO("starting active %s scan: channels=%u vif=%u\r\n",
                 five_ghz ? "5GHz" : "2.4GHz",
                 five_ghz ? 25U : 14U, g_aic_station_vif);
    result = aic_fmac_command(device, AIC_SCANU_START_REQ,
                              AIC_SCANU_START_ACCEPTED,
                              scan_request, sizeof(scan_request),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("%s scan was not accepted: %d\r\n",
                    five_ghz ? "5GHz" : "2.4GHz", result);
        return result;
    }

    for (receive_index = 0U;
         receive_index < AIC_FMAC_MAX_SCAN_TRANSFERS;
         receive_index++) {
        memset(g_aic_fmac_rx, 0, sizeof(g_aic_fmac_rx));
        result = aic8800_usb_bulk_receive(device, g_aic_fmac_rx,
                                          sizeof(g_aic_fmac_rx),
                                          AIC_FMAC_SCAN_TIMEOUT_MS, true);
        if (result < 0) {
            USB_LOG_ERR("%s scan receive failed: %d\r\n",
                        five_ghz ? "5GHz" : "2.4GHz", result);
            return result;
        }
        response_length = 0U;
        result = aic_fmac_find_confirmation(
            g_aic_fmac_rx, (uint32_t)result, AIC_SCANU_START_CFM,
            scan_confirmation, sizeof(scan_confirmation), &response_length);
        if (result < 0) {
            return result;
        }
        if (result > 0) {
            if (response_length < 2U) {
                return -USB_ERR_IO;
            }
            if (scan_confirmation[1] != 0U) {
                USB_LOG_ERR("%s scan completed with firmware status=%u\r\n",
                            five_ghz ? "5GHz" : "2.4GHz",
                            scan_confirmation[1]);
                return -USB_ERR_IO;
            }
            USB_LOG_INFO("%s scan complete\r\n",
                         five_ghz ? "5GHz" : "2.4GHz");
            return 0;
        }
    }
    return -USB_ERR_TIMEOUT;
}

static int aic_fmac_wait_message(struct aic8800_usb_device *device,
                                 uint16_t expected_id, void *response,
                                 uint32_t response_capacity,
                                 uint32_t *response_length,
                                 uint32_t timeout_ms)
{
    uint32_t receive_index;
    int result;

    for (receive_index = 0U; receive_index < 64U; receive_index++) {
        memset(g_aic_fmac_rx, 0, sizeof(g_aic_fmac_rx));
        result = aic8800_usb_bulk_receive(device, g_aic_fmac_rx,
                                          sizeof(g_aic_fmac_rx), timeout_ms,
                                          true);
        if (result < 0) {
            return result;
        }
        result = aic_fmac_find_confirmation(
            g_aic_fmac_rx, (uint32_t)result, expected_id, response,
            response_capacity, response_length);
        if (result != 0) {
            return (result > 0) ? 0 : result;
        }
    }
    return -USB_ERR_TIMEOUT;
}

static int aic_fmac_associate_target(struct aic8800_usb_device *device)
{
    uint8_t request[320] = { 0U };
    uint8_t confirmation[1] = { 0xffU };
    uint8_t indication[1024];
    uint32_t response_length = 0U;
    uint32_t flags = (1UL << 0) | (1UL << 1) | (1UL << 3);
    uint32_t ssid_length = g_aic_target_ap.ssid_len;
    int result;

    if ((ssid_length == 0U) || (ssid_length > 32U) ||
        (g_aic_target_ap.rsn_len == 0U) ||
        (g_aic_target_ap.rsn_len > AIC8800_WIFI_RSN_IE_MAX)) {
        return -USB_ERR_INVAL;
    }
    request[0] = (uint8_t)ssid_length;
    memcpy(request + 1U, g_aic_target_ap.ssid, ssid_length);
    memcpy(request + 34U, g_aic_target_ap.bssid, sizeof(g_aic_target_ap.bssid));
    aic_encode_channel(request + 40U, g_aic_target_ap.freq_mhz,
                       (g_aic_target_ap.freq_mhz >= 5000U) ? 1U : 0U, 0U);
    aic_put_le32(request + 48U, flags);
    request[52] = 0x88U;
    request[53] = 0x8eU;
    aic_put_le16(request + 54U, g_aic_target_ap.rsn_len);
    request[60] = 1U; /* vendor default U-APSD voice queue */
    request[61] = g_aic_station_vif;
    memcpy(request + 64U, g_aic_target_ap.rsn_ie, g_aic_target_ap.rsn_len);

    USB_LOG_INFO("associating with SSID=\"%s\" BSSID=%02x:%02x:%02x:%02x:%02x:%02x freq=%u WPA2-PSK/CCMP\r\n",
                 g_aic_target_ap.ssid, g_aic_target_ap.bssid[0],
                 g_aic_target_ap.bssid[1], g_aic_target_ap.bssid[2],
                 g_aic_target_ap.bssid[3], g_aic_target_ap.bssid[4],
                 g_aic_target_ap.bssid[5],
                 (unsigned int)g_aic_target_ap.freq_mhz);
    result = aic_fmac_command(device, AIC_SM_CONNECT_REQ,
                              AIC_SM_CONNECT_CFM,
                              request, sizeof(request), confirmation,
                              sizeof(confirmation), &response_length);
    if ((result < 0) || (response_length < 1U) ||
        (confirmation[0] != 0U)) {
        USB_LOG_ERR("connect request rejected: %d length=%u status=%u\r\n",
                    result, (unsigned int)response_length, confirmation[0]);
        return (result < 0) ? result : -USB_ERR_IO;
    }
    USB_LOG_INFO("connect request accepted; waiting for association result\r\n");

    memset(indication, 0, sizeof(indication));
    response_length = 0U;
    result = aic_fmac_wait_message(device, AIC_SM_CONNECT_IND,
                                   indication, sizeof(indication),
                                   &response_length,
                                   AIC_FMAC_CONNECT_TIMEOUT_MS);
    if ((result < 0) || (response_length < 12U)) {
        USB_LOG_ERR("association indication failed: %d length=%u\r\n",
                    result, (unsigned int)response_length);
        return (result < 0) ? result : -USB_ERR_IO;
    }
    if (aic_get_le16(indication) != 0U) {
        USB_LOG_ERR("association rejected: IEEE status=%u\r\n",
                    (unsigned int)aic_get_le16(indication));
        return -USB_ERR_IO;
    }
    USB_LOG_INFO("802.11 associated: AP station=%u channel-context=%u; WPA2 four-way handshake pending\r\n",
                 indication[10], indication[11]);
    if (response_length >= AIC_SM_CONNECT_IND_CHANNEL_END) {
        uint16_t frequency = aic_get_le16(
            indication + AIC_SM_CONNECT_IND_FREQUENCY_OFFSET);
        uint8_t width = indication[AIC_SM_CONNECT_IND_WIDTH_OFFSET];
        uint32_t center1 = aic_get_le32(
            indication + AIC_SM_CONNECT_IND_CENTER1_OFFSET);
        uint32_t center2 = aic_get_le32(
            indication + AIC_SM_CONNECT_IND_CENTER2_OFFSET);

        USB_LOG_INFO("association channel: band=%u frequency=%u MHz width=%u MHz center1=%lu center2=%lu\r\n",
                     indication[AIC_SM_CONNECT_IND_BAND_OFFSET],
                     (unsigned int)frequency,
                     (unsigned int)aic_fmac_channel_width_mhz(width),
                     (unsigned long)center1, (unsigned long)center2);
    }
    g_aic_ap_station = indication[10];
    return 0;
}

static bool aic_fmac_find_eapol(const uint8_t *buffer, uint32_t received)
{
    const uint8_t *record = buffer;
    uint32_t remaining = received;

    while (remaining >= AIC_FMAC_USB_RX_HEADER_SIZE) {
        uint32_t frame_length = aic_get_le16(record);
        uint32_t record_length = frame_length + AIC_FMAC_USB_RX_HEADER_SIZE;
        const uint8_t *frame;
        uint32_t header_length;
        uint32_t offset;

        if ((frame_length == 0U) || (record_length > remaining)) {
            return false;
        }
        frame = record + AIC_FMAC_USB_RX_HEADER_SIZE;
        if (frame_length >= 24U) {
            header_length = ((frame[1] & 3U) == 3U) ? 30U : 24U;
            if ((frame[0] & 0x80U) != 0U) {
                header_length += 2U;
            }
            if (((frame[0] & 0x80U) != 0U) &&
                ((frame[1] & 0x80U) != 0U)) {
                header_length += 4U;
            }
            for (offset = header_length; offset + 15U <= frame_length; offset++) {
                if ((frame[offset + 0U] == 0xaaU) &&
                    (frame[offset + 1U] == 0xaaU) &&
                    (frame[offset + 2U] == 0x03U) &&
                    (frame[offset + 3U] == 0x00U) &&
                    (frame[offset + 4U] == 0x00U) &&
                    (frame[offset + 5U] == 0x00U) &&
                    (frame[offset + 6U] == 0x88U) &&
                    (frame[offset + 7U] == 0x8eU)) {
                    const uint8_t *eapol = frame + offset + 8U;
                    uint32_t eapol_length;
                    uint16_t key_info;

                    if (offset + 15U > frame_length) {
                        break;
                    }
                    eapol_length = 4U + ((uint32_t)eapol[2] << 8) + eapol[3];
                    if ((offset + 8U + eapol_length > frame_length) ||
                        (eapol_length < 99U) || (eapol[1] != 3U)) {
                        break;
                    }
                    key_info = ((uint16_t)eapol[5] << 8) | eapol[6];
                    if ((key_info & 0x0088U) != 0x0088U) {
                        break;
                    }
                    g_aic_eapol_m1.protocol_version = eapol[0];
                    g_aic_eapol_m1.descriptor_type = eapol[4];
                    memcpy(g_aic_eapol_m1.key_length, eapol + 7U, 2U);
                    memcpy(g_aic_eapol_m1.replay_counter, eapol + 9U, 8U);
                    memcpy(g_aic_eapol_m1.anonce, eapol + 17U, 32U);
                    USB_LOG_INFO("received EAPOL-Key message 1/4: version=%u descriptor=%u key-info=0x%04x\r\n",
                                 eapol[0], eapol[4], key_info);
                    return true;
                }
            }
        }
        record += record_length;
        remaining -= record_length;
    }
    return false;
}

static void aic_fmac_make_snonce(uint8_t snonce[32])
{
    uint8_t seed[43];
    uint8_t digest[20];
    uint32_t tick = (uint32_t)xTaskGetTickCount();

    memcpy(seed, g_aic_eapol_m1.anonce, 32U);
    memcpy(seed + 32U, g_aic_station_mac, 6U);
    aic_put_le32(seed + 38U, tick);
    seed[42] = 0U;
    aic8800_hmac_sha1(g_aic_target_pmk, sizeof(g_aic_target_pmk),
                      seed, sizeof(seed), digest);
    memcpy(snonce, digest, 20U);
    seed[42] = 1U;
    aic8800_hmac_sha1(g_aic_target_pmk, sizeof(g_aic_target_pmk),
                      seed, sizeof(seed), digest);
    memcpy(snonce + 20U, digest, 12U);
    memset(seed, 0, sizeof(seed));
    memset(digest, 0, sizeof(digest));
}

static int aic_fmac_send_eapol_m2(struct aic8800_usb_device *device)
{
    uint8_t ptk_data[76];
    uint8_t mic[20];
    uint8_t *descriptor = g_aic_fmac_tx + 4U;
    uint8_t *eapol = descriptor + 28U;
    const uint8_t *first;
    const uint8_t *second;
    uint32_t eapol_length = 99U + g_aic_target_ap.rsn_len;
    uint32_t total_length = 4U + 28U + eapol_length;
    int result;

    if ((g_aic_station_vif == 0xffU) || (g_aic_ap_station == 0xffU)) {
        return -USB_ERR_INVAL;
    }
    aic_fmac_make_snonce(g_aic_snonce);

    if (memcmp(g_aic_target_ap.bssid, g_aic_station_mac, 6U) < 0) {
        first = g_aic_target_ap.bssid;
        second = g_aic_station_mac;
    } else {
        first = g_aic_station_mac;
        second = g_aic_target_ap.bssid;
    }
    memcpy(ptk_data, first, 6U);
    memcpy(ptk_data + 6U, second, 6U);
    if (memcmp(g_aic_eapol_m1.anonce, g_aic_snonce, 32U) < 0) {
        first = g_aic_eapol_m1.anonce;
        second = g_aic_snonce;
    } else {
        first = g_aic_snonce;
        second = g_aic_eapol_m1.anonce;
    }
    memcpy(ptk_data + 12U, first, 32U);
    memcpy(ptk_data + 44U, second, 32U);
    memset(g_aic_ptk, 0, sizeof(g_aic_ptk));
    aic8800_wpa_prf(g_aic_target_pmk, sizeof(g_aic_target_pmk),
                    "Pairwise key expansion", ptk_data, sizeof(ptk_data),
                    g_aic_ptk, sizeof(g_aic_ptk));

    memset(g_aic_fmac_tx, 0, total_length);
    aic_put_le16(g_aic_fmac_tx, (uint16_t)total_length);
    g_aic_fmac_tx[2] = AIC_USB_TYPE_DATA_TX;
    aic_put_le16(descriptor, (uint16_t)eapol_length);
    memcpy(descriptor + 8U, g_aic_target_ap.bssid, 6U);
    memcpy(descriptor + 14U, g_aic_station_mac, 6U);
    descriptor[20] = 0x88U;
    descriptor[21] = 0x8eU;
    descriptor[22] = 1U; /* best-effort access category */
    descriptor[23] = 0U; /* best-effort TID */
    descriptor[24] = g_aic_station_vif;
    descriptor[25] = g_aic_ap_station;

    eapol[0] = g_aic_eapol_m1.protocol_version;
    eapol[1] = 3U;
    eapol[2] = (uint8_t)((eapol_length - 4U) >> 8);
    eapol[3] = (uint8_t)(eapol_length - 4U);
    eapol[4] = g_aic_eapol_m1.descriptor_type;
    eapol[5] = 0x01U;
    eapol[6] = 0x0aU; /* descriptor v2, pairwise, MIC */
    memcpy(eapol + 7U, g_aic_eapol_m1.key_length, 2U);
    memcpy(eapol + 9U, g_aic_eapol_m1.replay_counter, 8U);
    memcpy(eapol + 17U, g_aic_snonce, sizeof(g_aic_snonce));
    eapol[97] = 0U;
    eapol[98] = (uint8_t)g_aic_target_ap.rsn_len;
    memcpy(eapol + 99U, g_aic_target_ap.rsn_ie, g_aic_target_ap.rsn_len);
    aic8800_hmac_sha1(g_aic_ptk, 16U, eapol, eapol_length, mic);
    memcpy(eapol + 81U, mic, 16U);

    result = aic8800_usb_bulk_send(device, g_aic_fmac_tx, total_length,
                                   AIC_FMAC_TIMEOUT_MS, false);
    memset(ptk_data, 0, sizeof(ptk_data));
    memset(mic, 0, sizeof(mic));
    if (result != (int)total_length) {
        USB_LOG_ERR("EAPOL-Key message 2/4 transmit failed: %d/%u\r\n",
                    result, (unsigned int)total_length);
        return (result < 0) ? result : -USB_ERR_IO;
    }
    USB_LOG_INFO("sent EAPOL-Key message 2/4: %u bytes\r\n",
                 (unsigned int)eapol_length);
    return 0;
}

static int aic_fmac_add_ccmp_key(struct aic8800_usb_device *device,
                                 uint8_t index, bool pairwise,
                                 const uint8_t key[16])
{
    uint8_t request[44];
    uint8_t confirmation[2] = { 0U, 0U };
    uint32_t response_length = 0U;
    uint32_t attempt;
    int result;

    memset(request, 0, sizeof(request));
    request[0] = index;
    request[1] = pairwise ? g_aic_ap_station : 0xffU;
    request[4] = 16U;
    memcpy(request + 8U, key, 16U);
    request[40] = 2U; /* MAC_CIPHER_CCMP */
    request[41] = g_aic_station_vif;
    request[43] = pairwise ? 1U : 0U;
    result = -USB_ERR_TIMEOUT;
    for (attempt = 0U; attempt < 3U; attempt++) {
        memset(confirmation, 0, sizeof(confirmation));
        response_length = 0U;
        result = aic_fmac_command(device, AIC_MM_KEY_ADD_REQ,
                                  AIC_MM_KEY_ADD_CFM,
                                  request, sizeof(request), confirmation,
                                  sizeof(confirmation), &response_length);
        if ((result == 0) && (response_length >= 2U)) break;
        USB_LOG_WRN("%s CCMP key confirmation retry %u/3: result=%d\r\n",
                    pairwise ? "pairwise" : "group",
                    (unsigned int)(attempt + 1U), result);
    }
    memset(request, 0, sizeof(request));
    if ((result < 0) || (response_length < 2U) ||
        (confirmation[0] != 0U)) {
        USB_LOG_ERR("%s CCMP key install failed: result=%d length=%u status=%u\r\n",
                    pairwise ? "pairwise" : "group", result,
                    (unsigned int)response_length, confirmation[0]);
        return (result < 0) ? result : -USB_ERR_IO;
    }
    USB_LOG_INFO("%s CCMP key installed: index=%u hardware=%u\r\n",
                 pairwise ? "pairwise" : "group", index, confirmation[1]);
    return 0;
}

static int aic_fmac_send_eapol_m4(struct aic8800_usb_device *device,
                                  const uint8_t replay_counter[8],
                                  const uint8_t key_length[2],
                                  uint16_t descriptor_version)
{
    uint8_t *descriptor = g_aic_fmac_tx + 4U;
    uint8_t *eapol = descriptor + 28U;
    uint8_t mic[20];
    const uint32_t eapol_length = 99U;
    const uint32_t total_length = 4U + 28U + eapol_length;
    uint16_t key_info = (uint16_t)(descriptor_version & 7U) |
                        0x0008U | 0x0100U | 0x0200U;
    int result;

    memset(g_aic_fmac_tx, 0, total_length);
    aic_put_le16(g_aic_fmac_tx, (uint16_t)total_length);
    g_aic_fmac_tx[2] = AIC_USB_TYPE_DATA_TX;
    aic_put_le16(descriptor, (uint16_t)eapol_length);
    memcpy(descriptor + 8U, g_aic_target_ap.bssid, 6U);
    memcpy(descriptor + 14U, g_aic_station_mac, 6U);
    descriptor[20] = 0x88U;
    descriptor[21] = 0x8eU;
    descriptor[22] = 1U;
    descriptor[23] = 0U;
    descriptor[24] = g_aic_station_vif;
    descriptor[25] = g_aic_ap_station;
    eapol[0] = g_aic_eapol_m1.protocol_version;
    eapol[1] = 3U;
    eapol[2] = 0U;
    eapol[3] = 95U;
    eapol[4] = g_aic_eapol_m1.descriptor_type;
    eapol[5] = (uint8_t)(key_info >> 8);
    eapol[6] = (uint8_t)key_info;
    memcpy(eapol + 7U, key_length, 2U);
    memcpy(eapol + 9U, replay_counter, 8U);
    aic8800_hmac_sha1(g_aic_ptk, 16U, eapol, eapol_length, mic);
    memcpy(eapol + 81U, mic, 16U);
    memset(mic, 0, sizeof(mic));
    result = aic8800_usb_bulk_send(device, g_aic_fmac_tx, total_length,
                                   AIC_FMAC_TIMEOUT_MS, false);
    if (result != (int)total_length) {
        USB_LOG_ERR("EAPOL-Key message 4/4 transmit failed: %d/%u\r\n",
                    result, (unsigned int)total_length);
        return (result < 0) ? result : -USB_ERR_IO;
    }
    USB_LOG_INFO("sent EAPOL-Key message 4/4\r\n");
    return 0;
}

static int aic_fmac_finish_wpa2(struct aic8800_usb_device *device,
                                const uint8_t *eapol,
                                uint32_t eapol_length,
                                uint16_t key_info)
{
    uint16_t wrapped_length = ((uint16_t)eapol[97] << 8) | eapol[98];
    uint8_t plain[64];
    size_t plain_length = sizeof(plain);
    uint8_t gtk[16];
    uint8_t gtk_index = 0xffU;
    uint8_t replay_counter[8];
    uint8_t key_length[2];
    uint32_t offset;
    uint8_t control_port[2];
    uint32_t response_length = 0U;
    int result;

    if (aic8800_aes_unwrap(g_aic_ptk + 16U, eapol + 99U,
                           wrapped_length, plain, &plain_length) < 0) {
        USB_LOG_ERR("EAPOL M3 AES key unwrap failed\r\n");
        return -USB_ERR_IO;
    }
    /* aic_fmac_command() below reuses g_aic_fmac_rx, which owns eapol. */
    memcpy(replay_counter, eapol + 9U, sizeof(replay_counter));
    memcpy(key_length, eapol + 7U, sizeof(key_length));
    memset(gtk, 0, sizeof(gtk));
    for (offset = 0U; offset + 2U <= plain_length;) {
        uint32_t element_length = plain[offset + 1U];

        if ((plain[offset] == 0U) && (element_length == 0U)) break;
        if (offset + 2U + element_length > plain_length) break;
        if ((plain[offset] == 0xddU) && (element_length >= 22U) &&
            (plain[offset + 2U] == 0x00U) &&
            (plain[offset + 3U] == 0x0fU) &&
            (plain[offset + 4U] == 0xacU) &&
            (plain[offset + 5U] == 0x01U)) {
            gtk_index = plain[offset + 6U] & 3U;
            memcpy(gtk, plain + offset + 8U, sizeof(gtk));
            break;
        }
        offset += 2U + element_length;
    }
    memset(plain, 0, sizeof(plain));
    if (gtk_index == 0xffU) {
        USB_LOG_ERR("GTK KDE missing from EAPOL M3\r\n");
        return -USB_ERR_IO;
    }
    USB_LOG_INFO("EAPOL M3 GTK unwrapped: index=%u length=16\r\n", gtk_index);
    /* Send M4 before MM_KEY_ADD. This firmware starts encrypting ordinary
     * data records as soon as the pairwise key exists; despite the host
     * control-port/no-encryption connect flags, that also affected the M4
     * record in v16 and the AP immediately disconnected with IEEE reason 15.
     * The EAPOL MIC already authenticates M4, so complete the air handshake
     * first, then commit the hardware keys before opening the data port. */
    result = aic_fmac_send_eapol_m4(device, replay_counter, key_length,
                                    key_info);
    if (result < 0) return result;
    result = aic_fmac_add_ccmp_key(device, gtk_index, false, gtk);
    memset(gtk, 0, sizeof(gtk));
    if (result < 0) return result;
    result = aic_fmac_add_ccmp_key(device, 0U, true, g_aic_ptk + 32U);
    if (result < 0) return result;
    control_port[0] = g_aic_ap_station;
    control_port[1] = 1U;
    result = aic_fmac_command(device, AIC_ME_CONTROL_PORT_REQ,
                              AIC_ME_CONTROL_PORT_CFM,
                              control_port, sizeof(control_port),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("opening station control port failed: %d\r\n", result);
        return result;
    }
    USB_LOG_INFO("WPA2 four-way handshake complete; station control port open\r\n");
    g_aic_wpa2_connected = true;
    return 0;
}

static bool aic_fmac_check_eapol_m3(struct aic8800_usb_device *device,
                                    const uint8_t *buffer,
                                    uint32_t received)
{
    const uint8_t *record = buffer;
    uint32_t remaining = received;

    while (remaining >= AIC_FMAC_USB_RX_HEADER_SIZE) {
        uint32_t frame_length = aic_get_le16(record);
        uint32_t record_length = frame_length + AIC_FMAC_USB_RX_HEADER_SIZE;
        const uint8_t *frame;
        uint32_t header_length;
        uint32_t offset;

        if ((frame_length == 0U) || (record_length > remaining)) {
            return false;
        }
        frame = record + AIC_FMAC_USB_RX_HEADER_SIZE;
        if (frame_length >= 24U) {
            header_length = ((frame[1] & 3U) == 3U) ? 30U : 24U;
            if ((frame[0] & 0x80U) != 0U) {
                header_length += 2U;
            }
            if (((frame[0] & 0x80U) != 0U) &&
                ((frame[1] & 0x80U) != 0U)) {
                header_length += 4U;
            }
            for (offset = header_length; offset + 107U <= frame_length; offset++) {
                const uint8_t *eapol;
                uint32_t eapol_length;
                uint16_t key_info;
                uint16_t key_data_length;
                uint8_t copy[512];
                uint8_t received_mic[16];
                uint8_t computed_mic[20];

                if ((frame[offset + 0U] != 0xaaU) ||
                    (frame[offset + 1U] != 0xaaU) ||
                    (frame[offset + 2U] != 0x03U) ||
                    (frame[offset + 3U] != 0x00U) ||
                    (frame[offset + 4U] != 0x00U) ||
                    (frame[offset + 5U] != 0x00U) ||
                    (frame[offset + 6U] != 0x88U) ||
                    (frame[offset + 7U] != 0x8eU)) {
                    continue;
                }
                eapol = frame + offset + 8U;
                eapol_length = 4U + ((uint32_t)eapol[2] << 8) + eapol[3];
                if ((eapol_length < 99U) || (eapol_length > sizeof(copy)) ||
                    (offset + 8U + eapol_length > frame_length) ||
                    (eapol[1] != 3U) || (eapol[4] != 2U)) {
                    return false;
                }
                key_info = ((uint16_t)eapol[5] << 8) | eapol[6];
                if ((key_info & 0x0088U) == 0x0088U &&
                    (key_info & 0x0100U) == 0U) {
                    USB_LOG_WRN("AP retransmitted EAPOL message 1/4 while waiting for M3\r\n");
                    return false;
                }
                if ((key_info & 0x03c8U) != 0x03c8U) {
                    USB_LOG_WRN("unexpected EAPOL-Key while waiting for M3: key-info=0x%04x\r\n",
                                key_info);
                    return false;
                }
                key_data_length = ((uint16_t)eapol[97] << 8) | eapol[98];
                if (99U + key_data_length != eapol_length) {
                    USB_LOG_ERR("invalid EAPOL M3 key-data length: %u/%u\r\n",
                                key_data_length, (unsigned int)eapol_length);
                    return false;
                }
                memcpy(copy, eapol, eapol_length);
                memcpy(received_mic, copy + 81U, sizeof(received_mic));
                memset(copy + 81U, 0, sizeof(received_mic));
                aic8800_hmac_sha1(g_aic_ptk, 16U, copy, eapol_length,
                                  computed_mic);
                if (memcmp(received_mic, computed_mic,
                           sizeof(received_mic)) != 0) {
                    USB_LOG_ERR("EAPOL message 3/4 MIC mismatch\r\n");
                    return false;
                }
                USB_LOG_INFO("received EAPOL-Key message 3/4: key-info=0x%04x key-data=%u MIC valid\r\n",
                             key_info, key_data_length);
                return aic_fmac_finish_wpa2(device, eapol, eapol_length,
                                            key_info) == 0;
            }
        }
        record += record_length;
        remaining -= record_length;
    }
    return false;
}

static void aic_fmac_wait_eapol_m3(struct aic8800_usb_device *device)
{
    uint32_t transfer;

    for (transfer = 0U; transfer < AIC_FMAC_EAPOL_RX_TRANSFERS; transfer++) {
        int result;

        memset(g_aic_fmac_rx, 0, sizeof(g_aic_fmac_rx));
        result = aic8800_usb_bulk_receive(device, g_aic_fmac_rx,
                                          sizeof(g_aic_fmac_rx),
                                          AIC_FMAC_TIMEOUT_MS, false);
        if (result < 0) {
            if (!device->online) {
                USB_LOG_ERR("device detached while waiting for EAPOL M3\r\n");
                return;
            }
            continue;
        }
        if (aic_fmac_check_eapol_m3(device, g_aic_fmac_rx,
                                    (uint32_t)result)) {
            return;
        }
    }
    USB_LOG_ERR("EAPOL message 3/4 was not captured after %u data transfers\r\n",
                AIC_FMAC_EAPOL_RX_TRANSFERS);
}

static bool aic_fmac_tx_is_priority(const uint8_t *frame, uint16_t length)
{
    uint16_t ethernet_type;

    ethernet_type = ((uint16_t)frame[12] << 8) | frame[13];
    if ((ethernet_type == 0x0806U) || (ethernet_type == 0x888eU)) {
        return true;
    }
    if ((ethernet_type == 0x0800U) && (length >= 34U)) {
        uint16_t ip_header_length = (uint16_t)(frame[14] & 0x0fU) * 4U;

        if ((ip_header_length < 20U) ||
            (length < (uint16_t)(14U + ip_header_length))) {
            return false;
        }
        if (frame[23] == 1U) { /* ICMP */
            return true;
        }
        if (frame[23] == 17U) { /* Keep DHCP/DNS/control UDP ahead of bulk UDP. */
            return length <= 512U;
        }
        if ((frame[23] == 6U) &&
            (length >= (uint16_t)(14U + ip_header_length + 20U))) {
            uint16_t tcp_offset = 14U + ip_header_length;
            uint16_t tcp_header_length =
                (uint16_t)(frame[tcp_offset + 12U] >> 4) * 4U;
            uint16_t ip_total_length =
                ((uint16_t)frame[16] << 8) | frame[17];

            /* Prioritize SYN/FIN/RST and pure ACKs, but never move a short
             * TCP data segment ahead of older queued data. */
            return (tcp_header_length >= 20U) &&
                   (ip_total_length <= ip_header_length + tcp_header_length);
        }
    }
    return (ethernet_type == 0x86ddU) && (length <= 256U);
}

static bool aic_fmac_tx_is_pure_ack(const uint8_t *frame, uint16_t length)
{
    uint16_t ip_header_length;
    uint16_t tcp_offset;
    uint16_t tcp_header_length;
    uint16_t ip_total_length;

    if ((length < 54U) || (frame[12] != 0x08U) || (frame[13] != 0x00U) ||
        (frame[23] != 6U)) {
        return false;
    }
    ip_header_length = (uint16_t)(frame[14] & 0x0fU) * 4U;
    tcp_offset = 14U + ip_header_length;
    if ((ip_header_length < 20U) ||
        (length < (uint16_t)(tcp_offset + 20U))) {
        return false;
    }
    tcp_header_length = (uint16_t)(frame[tcp_offset + 12U] >> 4) * 4U;
    ip_total_length = ((uint16_t)frame[16] << 8) | frame[17];
    if ((tcp_header_length < 20U) ||
        (ip_total_length > ip_header_length + tcp_header_length)) {
        return false;
    }
    /* Hold delayed ACKs only. SYN/FIN/RST must go out immediately. */
    return (frame[tcp_offset + 13U] & (AIC_TCP_SYN | AIC_TCP_FIN | AIC_TCP_RST)) == 0U;
}

static void aic_fmac_tx_note_drop(void)
{
    taskENTER_CRITICAL();
    g_aic_tx_stats.queue_drops++;
    taskEXIT_CRITICAL();
}

void aic8800_fmac_get_tx_stats(struct aic8800_fmac_tx_stats *stats)
{
    uint64_t process_cycles;
    uint32_t process_max_cycles;
    uint32_t schedule_max_cycles;
    uint32_t lwip_lock_max_cycles;
    uint32_t rearm_max_cycles;
    uint32_t cycles_per_ms;
    uint32_t cycles_per_us;

    if (stats == NULL) {
        return;
    }
    taskENTER_CRITICAL();
    *stats = g_aic_tx_stats;
    process_cycles = g_aic_rx_process_cycles;
    process_max_cycles = g_aic_rx_process_max_cycles;
    schedule_max_cycles = g_aic_rx_schedule_max_cycles;
    lwip_lock_max_cycles = g_aic_rx_lwip_lock_max_cycles;
    rearm_max_cycles = g_aic_rx_rearm_max_cycles;
    taskEXIT_CRITICAL();
    cycles_per_ms = SystemCoreClock / 1000U;
    cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_ms != 0U) {
        stats->rx_process_ms = (uint32_t)(process_cycles / cycles_per_ms);
    }
    if (cycles_per_us != 0U) {
        stats->rx_process_max_us = process_max_cycles / cycles_per_us;
        stats->rx_schedule_max_us = schedule_max_cycles / cycles_per_us;
        stats->rx_lwip_lock_max_us = lwip_lock_max_cycles / cycles_per_us;
        stats->rx_rearm_max_us = rearm_max_cycles / cycles_per_us;
    }
}

static void aic_fmac_tx_usb_send(struct aic8800_usb_device *device,
                                 const uint8_t *frame, uint16_t frame_length)
{
    static uint32_t tx_log_count;
    uint8_t *descriptor = g_aic_data_tx + 4U;
    uint32_t payload_length;
    uint32_t total_length;
    int result = -USB_ERR_TIMEOUT;
    TickType_t started = xTaskGetTickCount();
    TickType_t usb_started = started;
    uint32_t mutex_wait_ms = 0U;
    uint32_t usb_elapsed_ms = 0U;
    bool mutex_acquired = false;

    if ((frame == NULL) || (frame_length < 14U)) {
        return;
    }
    payload_length = (uint32_t)(frame_length - 14U);
    total_length = 4U + 28U + payload_length;

    if (g_aic_data_tx_active && g_aic_wpa2_connected &&
        (xSemaphoreTake(g_aic_tx_mutex,
                        pdMS_TO_TICKS(AIC_FMAC_DATA_TX_TIMEOUT_MS)) == pdTRUE)) {
        mutex_acquired = true;
        usb_started = xTaskGetTickCount();
        mutex_wait_ms =
            (uint32_t)(((usb_started - started) * 1000U) /
                       configTICK_RATE_HZ);
        memset(g_aic_data_tx, 0, 32U);
        aic_put_le16(g_aic_data_tx, (uint16_t)total_length);
        g_aic_data_tx[2] = AIC_USB_TYPE_DATA_TX;
        aic_put_le16(descriptor, (uint16_t)payload_length);
        memcpy(descriptor + 8U, frame, 6U);
        memcpy(descriptor + 14U, frame + 6U, 6U);
        memcpy(descriptor + 20U, frame + 12U, 2U);
        descriptor[22] = 1U;
        descriptor[23] = 0U;
        descriptor[24] = g_aic_station_vif;
        descriptor[25] = g_aic_ap_station;
        memcpy(g_aic_data_tx + 32U, frame + 14U, payload_length);
        result = aic8800_usb_bulk_send(device, g_aic_data_tx, total_length,
                                       AIC_FMAC_DATA_TX_TIMEOUT_MS, false);
        usb_elapsed_ms =
            (uint32_t)(((xTaskGetTickCount() - usb_started) * 1000U) /
                       configTICK_RATE_HZ);
        xSemaphoreGive(g_aic_tx_mutex);
    }

    taskENTER_CRITICAL();
    if (!mutex_acquired && g_aic_data_tx_active && g_aic_wpa2_connected) {
        g_aic_tx_stats.mutex_timeouts++;
    }
    if (mutex_wait_ms > g_aic_tx_stats.mutex_max_ms) {
        g_aic_tx_stats.mutex_max_ms = mutex_wait_ms;
    }
    if (usb_elapsed_ms > g_aic_tx_stats.usb_max_ms) {
        g_aic_tx_stats.usb_max_ms = usb_elapsed_ms;
    }
    if (usb_elapsed_ms >= AIC_FMAC_USB_SLOW_MS) {
        g_aic_tx_stats.usb_slow_transfers++;
    }
    if (result == (int)total_length) {
        g_aic_tx_stats.sent_frames++;
        g_aic_tx_stats.sent_bytes += frame_length;
    } else if (g_aic_data_tx_active) {
        g_aic_tx_stats.usb_errors++;
    }
    taskEXIT_CRITICAL();

    if ((result == (int)total_length) && (tx_log_count < 8U)) {
        USB_LOG_INFO("AIC data TX Ethernet type=0x%02x%02x length=%u\r\n",
                     frame[12], frame[13], (unsigned int)frame_length);
        tx_log_count++;
    }
}

static bool aic_fmac_tx_take_ack_hold(uint8_t *frame, uint16_t *length)
{
    bool have = false;

    taskENTER_CRITICAL();
    if (g_aic_tx_ack_hold_valid) {
        *length = g_aic_tx_ack_hold.length;
        if (*length > AIC_FMAC_ETHERNET_MAX) {
            *length = AIC_FMAC_ETHERNET_MAX;
        }
        memcpy(frame, g_aic_tx_ack_hold.frame, *length);
        g_aic_tx_ack_hold_valid = false;
        have = true;
    }
    taskEXIT_CRITICAL();
    return have;
}

static bool aic_fmac_tx_ack_due(void)
{
    uint32_t now = DWT->CYCCNT;
    uint32_t pace;

    if (g_aic_tx_ack_flush_cycles == 0U) {
        return true;
    }
    pace = SystemCoreClock / AIC_FMAC_TCP_ACK_PACE_HZ;
    if (pace == 0U) {
        pace = 1200000U;
    }
    return (uint32_t)(now - g_aic_tx_ack_flush_cycles) >= pace;
}

static void aic_fmac_tx_enqueue_ack_hold(void)
{
    struct aic_fmac_tx_record *record;
    UBaseType_t queued;
    uint8_t index;

    if (!g_aic_tx_ack_hold_valid) {
        return;
    }
    if (xQueueReceive(g_aic_tx_free_queue, &index, 0U) != pdTRUE) {
        return;
    }
    record = &g_aic_tx_records[index];
    taskENTER_CRITICAL();
    if (!g_aic_tx_ack_hold_valid) {
        taskEXIT_CRITICAL();
        (void)xQueueSend(g_aic_tx_free_queue, &index, 0U);
        return;
    }
    record->length = g_aic_tx_ack_hold.length;
    if (record->length > AIC_FMAC_ETHERNET_MAX) {
        record->length = AIC_FMAC_ETHERNET_MAX;
    }
    memcpy(record->frame, g_aic_tx_ack_hold.frame, record->length);
    g_aic_tx_ack_hold_valid = false;
    taskEXIT_CRITICAL();
    if (xQueueSendToBack(g_aic_tx_pending_queue, &index, 0U) != pdTRUE) {
        (void)xQueueSend(g_aic_tx_free_queue, &index, 0U);
        return;
    }
    g_aic_tx_ack_flush_cycles = DWT->CYCCNT;
    queued = uxQueueMessagesWaiting(g_aic_tx_pending_queue);
    taskENTER_CRITICAL();
    g_aic_tx_stats.enqueued_frames++;
    if (queued > g_aic_tx_stats.queue_high_water) {
        g_aic_tx_stats.queue_high_water = queued;
    }
    taskEXIT_CRITICAL();
}

static void aic_fmac_tx_flush_ack_hold(bool force)
{
    if (!g_aic_tx_ack_hold_valid) {
        return;
    }
    if (!force && !aic_fmac_tx_ack_due()) {
        return;
    }
    aic_fmac_tx_enqueue_ack_hold();
}

static void aic_fmac_tx_refresh_ack_hold(void)
{
    struct tcp_pcb *pcb;
    uint8_t *frame;
    uint8_t *ip;
    uint8_t *tcp;
    uint16_t ip_header_length;
    uint16_t hold_length;
    uint16_t window;
    uint16_t checksum;
    uint16_t old_ack_hi;
    uint16_t old_ack_lo;
    uint16_t old_window;
    uint32_t rcv_nxt;

    if (!g_aic_tx_ack_hold_valid || (g_aic_tx_ack_hold.length < 54U)) {
        return;
    }
    frame = g_aic_tx_ack_hold.frame;
    ip = frame + 14U;
    ip_header_length = (uint16_t)(ip[0] & 0x0fU) * 4U;
    hold_length = g_aic_tx_ack_hold.length;
    if ((ip_header_length < 20U) ||
        (hold_length < (uint16_t)(14U + ip_header_length + 20U))) {
        return;
    }
    tcp = ip + ip_header_length;
    pcb = NULL;
    for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
        if ((pcb->state == ESTABLISHED) &&
            (pcb->local_port == aic_get_be16(tcp)) &&
            (pcb->remote_port == aic_get_be16(tcp + 2U))) {
            break;
        }
    }
    if (pcb == NULL) {
        return;
    }

    rcv_nxt = pcb->rcv_nxt;
    window = TCPWND_MIN16(RCV_WND_SCALE(pcb, pcb->rcv_ann_wnd));
    old_ack_hi = aic_get_be16(tcp + 8U);
    old_ack_lo = aic_get_be16(tcp + 10U);
    old_window = aic_get_be16(tcp + 14U);
    if ((old_ack_hi == (uint16_t)(rcv_nxt >> 16)) &&
        (old_ack_lo == (uint16_t)rcv_nxt) &&
        (old_window == window)) {
        return;
    }

    taskENTER_CRITICAL();
    if (!g_aic_tx_ack_hold_valid || (g_aic_tx_ack_hold.length != hold_length)) {
        taskEXIT_CRITICAL();
        return;
    }
    /* IP header is unchanged, so leave its checksum alone. Only ACK and
     * window move; fold those 16-bit words into the existing TCP checksum. */
    checksum = aic_get_be16(tcp + 16U);
    checksum = aic_checksum_adjust(checksum, old_ack_hi,
                                   (uint16_t)(rcv_nxt >> 16));
    checksum = aic_checksum_adjust(checksum, old_ack_lo, (uint16_t)rcv_nxt);
    checksum = aic_checksum_adjust(checksum, old_window, window);
    if (checksum == 0U) {
        checksum = 0xffffU;
    }
    aic_put_be32(tcp + 8U, rcv_nxt);
    aic_put_be16(tcp + 14U, window);
    aic_put_be16(tcp + 16U, checksum);
    taskEXIT_CRITICAL();
}

static void aic_fmac_data_tx_task(void *parameter)
{
    struct aic8800_usb_device *device = parameter;
    static uint8_t snapshot[AIC_FMAC_ETHERNET_MAX];

    while (!g_aic_data_tx_stop && device->online) {
        uint8_t index;
        uint16_t hold_length = 0U;
        BaseType_t got;

        got = xQueueReceive(g_aic_tx_pending_queue, &index,
                            pdMS_TO_TICKS(AIC_FMAC_TX_WAIT_MS));
        if (got != pdTRUE) {
            if (aic_fmac_tx_take_ack_hold(snapshot, &hold_length)) {
                aic_fmac_tx_usb_send(device, snapshot, hold_length);
                g_aic_tx_ack_flush_cycles = DWT->CYCCNT;
            }
            continue;
        }
        if (index < AIC_FMAC_TX_QUEUE_DEPTH) {
            struct aic_fmac_tx_record *record = &g_aic_tx_records[index];
            uint16_t frame_length;

            taskENTER_CRITICAL();
            frame_length = record->length;
            if (frame_length > AIC_FMAC_ETHERNET_MAX) {
                frame_length = AIC_FMAC_ETHERNET_MAX;
            }
            memcpy(snapshot, record->frame, frame_length);
            taskEXIT_CRITICAL();
            (void)xQueueSend(g_aic_tx_free_queue, &index, portMAX_DELAY);
            aic_fmac_tx_usb_send(device, snapshot, frame_length);
        }
    }
    g_aic_data_tx_active = false;
    g_aic_data_tx_task_running = false;
    vTaskDelete(NULL);
}

static bool aic_fmac_data_tx_start(struct aic8800_usb_device *device)
{
    uint8_t index;

    if (g_aic_data_tx_task_running) {
        USB_LOG_ERR("previous AIC data TX task is still running\r\n");
        return false;
    }
    if (g_aic_tx_free_queue == NULL) {
        g_aic_tx_free_queue =
            xQueueCreate(AIC_FMAC_TX_QUEUE_DEPTH, sizeof(uint8_t));
    }
    if (g_aic_tx_pending_queue == NULL) {
        g_aic_tx_pending_queue =
            xQueueCreate(AIC_FMAC_TX_QUEUE_DEPTH, sizeof(uint8_t));
    }
    if ((g_aic_tx_free_queue == NULL) || (g_aic_tx_pending_queue == NULL)) {
        USB_LOG_ERR("failed to create AIC data TX queues\r\n");
        return false;
    }
    (void)xQueueReset(g_aic_tx_free_queue);
    (void)xQueueReset(g_aic_tx_pending_queue);
    for (index = 0U; index < AIC_FMAC_TX_QUEUE_DEPTH; index++) {
        if (xQueueSend(g_aic_tx_free_queue, &index, 0U) != pdTRUE) {
            USB_LOG_ERR("failed to initialize AIC data TX free queue\r\n");
            return false;
        }
    }
    memset(&g_aic_tx_stats, 0, sizeof(g_aic_tx_stats));
    g_aic_tx_ack_hold_valid = false;
    g_aic_tx_ack_flush_cycles = 0U;
    g_aic_rx_process_cycles = 0U;
    g_aic_rx_process_max_cycles = 0U;
    g_aic_rx_schedule_max_cycles = 0U;
    g_aic_rx_lwip_lock_max_cycles = 0U;
    g_aic_rx_rearm_max_cycles = 0U;
    g_aic_data_tx_stop = false;
    g_aic_data_tx_active = true;
    g_aic_data_tx_task_running = true;
    if (xTaskCreate(aic_fmac_data_tx_task, "aic_tx",
                    AIC_FMAC_TX_TASK_STACK, device,
                    AIC_FMAC_TX_TASK_PRIORITY, NULL) != pdPASS) {
        g_aic_data_tx_active = false;
        g_aic_data_tx_task_running = false;
        USB_LOG_ERR("failed to create AIC data TX task\r\n");
        return false;
    }
    USB_LOG_INFO("AIC data TX queue ready: depth=%u reserve=%u HyperRAM=0x70400000\r\n",
                 AIC_FMAC_TX_QUEUE_DEPTH, AIC_FMAC_TX_PRIORITY_RESERVE);
    return true;
}

static void aic_fmac_data_tx_stop(void)
{
    g_aic_data_tx_active = false;
    g_aic_data_tx_stop = true;
}

static bool aic_fmac_tx_store_ack_pbuf(const struct pbuf *packet)
{
    uint8_t frame[128];
    uint16_t length;

    length = packet->tot_len;
    if ((length < 54U) || (length > sizeof(frame))) {
        return false;
    }
    if (pbuf_copy_partial(packet, frame, length, 0U) != length) {
        return false;
    }
    if (!aic_fmac_tx_is_pure_ack(frame, length)) {
        return false;
    }
    taskENTER_CRITICAL();
    memcpy(g_aic_tx_ack_hold.frame, frame, length);
    g_aic_tx_ack_hold.length = length;
    g_aic_tx_ack_hold_valid = true;
    taskEXIT_CRITICAL();
    return true;
}

err_t aic8800_lwip_transmit(const struct pbuf *packet)
{
    struct aic8800_usb_device *device = aic8800_usb_get_device();
    struct aic_fmac_tx_record *record;
    UBaseType_t queued;
    uint8_t index;
    bool priority;

    if ((packet == NULL) || (packet->tot_len < 14U) ||
        (packet->tot_len > AIC_FMAC_ETHERNET_MAX) ||
        (device == NULL) || !device->online || !g_aic_wpa2_connected ||
        !g_aic_data_tx_active || (g_aic_tx_free_queue == NULL) ||
        (g_aic_tx_pending_queue == NULL)) {
        return ERR_IF;
    }
    if (aic_fmac_tx_store_ack_pbuf(packet)) {
        /* Keep the latest ACK. Flush after the RX URB so USB TX cannot
         * preempt 802.11/lwIP parse of the remaining aggregate records. */
        return ERR_OK;
    }
    if (xQueueReceive(g_aic_tx_free_queue, &index, 0U) != pdTRUE) {
        aic_fmac_tx_note_drop();
        return ERR_MEM;
    }
    record = &g_aic_tx_records[index];
    record->length = packet->tot_len;
    if (pbuf_copy_partial(packet, record->frame, packet->tot_len, 0U) !=
        packet->tot_len) {
        (void)xQueueSend(g_aic_tx_free_queue, &index, 0U);
        return ERR_BUF;
    }
    if (aic_fmac_tx_is_pure_ack(record->frame, record->length)) {
        taskENTER_CRITICAL();
        g_aic_tx_ack_hold.length = record->length;
        memcpy(g_aic_tx_ack_hold.frame, record->frame, record->length);
        g_aic_tx_ack_hold_valid = true;
        taskEXIT_CRITICAL();
        (void)xQueueSend(g_aic_tx_free_queue, &index, 0U);
        return ERR_OK;
    }
    aic_fmac_tx_flush_ack_hold(true);
    priority = aic_fmac_tx_is_priority(record->frame, record->length);
    if (!priority &&
        (uxQueueMessagesWaiting(g_aic_tx_free_queue) <
         AIC_FMAC_TX_PRIORITY_RESERVE)) {
        (void)xQueueSend(g_aic_tx_free_queue, &index, 0U);
        aic_fmac_tx_note_drop();
        return ERR_MEM;
    }
    /* Preserve global Ethernet ordering. Priority frames benefit from the
     * reserved capacity, but must not jump ahead of an older TCP segment. */
    if (xQueueSendToBack(g_aic_tx_pending_queue, &index, 0U) != pdTRUE) {
        (void)xQueueSend(g_aic_tx_free_queue, &index, 0U);
        aic_fmac_tx_note_drop();
        return ERR_MEM;
    }
    queued = uxQueueMessagesWaiting(g_aic_tx_pending_queue);
    taskENTER_CRITICAL();
    g_aic_tx_stats.enqueued_frames++;
    if (queued > g_aic_tx_stats.queue_high_water) {
        g_aic_tx_stats.queue_high_water = queued;
    }
    taskEXIT_CRITICAL();
    return ERR_OK;
}

static void aic_fmac_note_rx_drop(void)
{
    taskENTER_CRITICAL();
    g_aic_tx_stats.rx_input_drops++;
    taskEXIT_CRITICAL();
}

static bool aic_fmac_llc_is_snap(const uint8_t *llc)
{
    return (llc[0] == 0xaaU) && (llc[1] == 0xaaU) && (llc[2] == 0x03U) &&
           (llc[3] == 0x00U) && (llc[4] == 0x00U) && (llc[5] == 0x00U);
}

static bool aic_fmac_is_iperf_payload(const uint8_t *ethertype,
                                      const uint8_t *payload,
                                      uint32_t payload_length)
{
    uint32_t ip_header_length;
    uint16_t fragment;
    uint8_t protocol;
    uint32_t transport_header;

    if ((ethertype[0] != 0x08U) || (ethertype[1] != 0x00U) ||
        (payload_length < 28U) || ((payload[0] >> 4) != 4U)) {
        return false;
    }
    protocol = payload[9];
    /* UDP/5001 can reference the DMA buffer because lwiperf frees the
     * datagram in the same input call. TCP may queue or reorder, so it
     * must copy into PBUF_POOL and must not use this path. */
    if (protocol != 17U) {
        return false;
    }
    ip_header_length = (uint32_t)(payload[0] & 0x0fU) * 4U;
    transport_header = 8U;
    if ((ip_header_length < 20U) ||
        (ip_header_length + transport_header > payload_length)) {
        return false;
    }
    fragment = aic_get_be16(payload + 6U);
    if ((fragment & 0x3fffU) != 0U) {
        return false;
    }
    return aic_get_be16(payload + ip_header_length + 2U) == 5001U;
}

static bool aic_fmac_tcp_can_zerocopy(const uint8_t *ethertype,
                                      const uint8_t *payload,
                                      uint32_t payload_length)
{
    struct tcp_pcb *pcb;
    const uint8_t *tcp;
    uint32_t ip_header_length;
    uint32_t seq;
    uint16_t fragment;

    if ((ethertype[0] != 0x08U) || (ethertype[1] != 0x00U) ||
        (payload_length < 40U) || ((payload[0] >> 4) != 4U) ||
        (payload[9] != 6U)) {
        return false;
    }
    ip_header_length = (uint32_t)(payload[0] & 0x0fU) * 4U;
    if ((ip_header_length < 20U) ||
        ((ip_header_length + 20U) > payload_length)) {
        return false;
    }
    fragment = aic_get_be16(payload + 6U);
    if ((fragment & 0x3fffU) != 0U) {
        return false;
    }
    tcp = payload + ip_header_length;
    if (aic_get_be16(tcp + 2U) != 5001U) {
        return false;
    }
    seq = aic_get_be32(tcp + 4U);
    /* In-order (and duplicate) segments are freed by lwiperf before this
     * DMA buffer is recycled. Future sequence numbers stay on ooseq and
     * must be copied. Walk is safe: inject already holds the lwIP lock. */
    for (pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
        if ((pcb->state == ESTABLISHED) && (pcb->local_port == 5001U)) {
            return !TCP_SEQ_GT(seq, pcb->rcv_nxt);
        }
    }
    return false;
}

static void aic_fmac_inject_ethernet(const uint8_t *destination,
                                     const uint8_t *source,
                                     const uint8_t *ethertype,
                                     const uint8_t *payload,
                                     uint32_t payload_length)
{
    static uint32_t rx_log_count;
    struct pbuf *packet;
    uint8_t *ethernet;
    uint16_t length;
    bool zerocopy;

    if (payload_length > (AIC_FMAC_ETHERNET_MAX - 14U)) {
        return;
    }
    length = (uint16_t)(14U + payload_length);
    zerocopy = aic_fmac_is_iperf_payload(ethertype, payload, payload_length);
    if (!zerocopy) {
        zerocopy = aic_fmac_tcp_can_zerocopy(ethertype, payload, payload_length);
    }
    if (zerocopy) {
        /* UDP and in-order TCP/5001 reuse the bytes before the IP packet
         * as the Ethernet header. Out-of-order TCP is copied so it can
         * outlive this DMA buffer. */
        ethernet = (uint8_t *)payload - 14U;
        packet = pbuf_alloc_reference(ethernet, length, PBUF_REF);
    } else {
        packet = aic8800_lwip_alloc_frame(length);
        ethernet = (packet != NULL) ? (uint8_t *)packet->payload : NULL;
    }
    if (packet == NULL) {
        aic_fmac_note_rx_drop();
        return;
    }
    memcpy(ethernet, destination, 6U);
    memcpy(ethernet + 6U, source, 6U);
    memcpy(ethernet + 12U, ethertype, 2U);
    if (!zerocopy && (payload_length > 0U)) {
        memcpy(ethernet + 14U, payload, payload_length);
    } else if (zerocopy) {
        g_aic_tx_stats.rx_zerocopy_frames++;
    }
    if (rx_log_count < 8U) {
        USB_LOG_INFO("AIC data RX Ethernet type=0x%02x%02x length=%u\r\n",
                     ethernet[12], ethernet[13], (unsigned int)length);
        rx_log_count++;
    }
    if (aic8800_lwip_inject_locked(packet) != ERR_OK) {
        aic_fmac_note_rx_drop();
    }
}

static void aic_fmac_deliver_snap(const uint8_t *destination,
                                  const uint8_t *source,
                                  const uint8_t *llc,
                                  uint32_t llc_and_payload)
{
    if ((llc == NULL) || (llc_and_payload < 8U) ||
        !aic_fmac_llc_is_snap(llc)) {
        return;
    }
    aic_fmac_inject_ethernet(destination, source, llc + 6U, llc + 8U,
                             llc_and_payload - 8U);
}

static bool aic_fmac_looks_like_amsdu(const uint8_t *body, uint32_t body_length)
{
    uint32_t msdu_length;

    if (body_length < 22U) {
        return false;
    }
    msdu_length = aic_get_be16(body + 12U);
    if ((msdu_length < 8U) || (msdu_length > 1518U) ||
        ((14U + msdu_length) > body_length)) {
        return false;
    }
    return aic_fmac_llc_is_snap(body + 14U);
}

static void aic_fmac_deliver_amsdu(const uint8_t *body, uint32_t body_length)
{
    while (body_length >= 14U) {
        uint32_t msdu_length = aic_get_be16(body + 12U);
        uint32_t subframe;
        uint32_t padded;

        if ((msdu_length < 8U) || (msdu_length > 1518U) ||
            ((14U + msdu_length) > body_length)) {
            break;
        }
        aic_fmac_deliver_snap(body, body + 6U, body + 14U, msdu_length);
        g_aic_tx_stats.rx_amsdu_msdus++;
        subframe = 14U + msdu_length;
        padded = (subframe + 3U) & ~3U;
        if (padded > body_length) {
            break;
        }
        body += padded;
        body_length -= padded;
    }
}

static void aic_fmac_deliver_data_record(const uint8_t *record,
                                         uint32_t record_length)
{
    static const uint8_t iv_lengths[] = { 8U, 0U, 4U, 18U };
    const uint8_t *frame = record + AIC_FMAC_USB_RX_HEADER_SIZE;
    uint32_t frame_length = aic_get_le16(record);
    uint32_t header_length;
    uint32_t llc_offset = 0U;
    uint32_t index;
    uint8_t ds;
    const uint8_t *destination;
    const uint8_t *source;
    bool qos;
    bool amsdu = false;

    if ((record_length < AIC_FMAC_USB_RX_HEADER_SIZE + 24U) ||
        (frame_length > record_length - AIC_FMAC_USB_RX_HEADER_SIZE) ||
        ((frame[0] & 0x0cU) != 0x08U)) {
        return;
    }
    ds = frame[1] & 3U;
    qos = (frame[0] & 0x80U) != 0U;
    header_length = (ds == 3U) ? 30U : 24U;
    if (qos) {
        if ((header_length + 2U <= frame_length) &&
            ((frame[header_length] & 0x80U) != 0U)) {
            amsdu = true;
        }
        header_length += 2U;
    }
    if (qos && ((frame[1] & 0x80U) != 0U)) {
        header_length += 4U;
    }
    if (frame_length < header_length + 8U) {
        return;
    }
    switch (ds) {
    case 0U: destination = frame + 4U;  source = frame + 10U; break;
    case 1U: destination = frame + 16U; source = frame + 10U; break;
    case 2U: destination = frame + 4U;  source = frame + 16U; break;
    default: destination = frame + 16U; source = frame + 24U; break;
    }
    if (amsdu) {
        const uint8_t *body = frame + header_length;
        uint32_t body_length = frame_length - header_length;

        if (((frame[1] & 0x40U) != 0U) && (body_length > 8U) &&
            aic_fmac_looks_like_amsdu(body + 8U, body_length - 8U)) {
            body += 8U;
            body_length -= 8U;
        }
        aic_fmac_deliver_amsdu(body, body_length);
        return;
    }
    for (index = 0U; index < sizeof(iv_lengths); index++) {
        uint32_t candidate = header_length + iv_lengths[index];

        if ((candidate + 8U <= frame_length) &&
            aic_fmac_llc_is_snap(frame + candidate)) {
            llc_offset = candidate;
            break;
        }
    }
    if (llc_offset == 0U) {
        return;
    }
    aic_fmac_deliver_snap(destination, source, frame + llc_offset,
                          frame_length - llc_offset);
}

static uint32_t aic_fmac_process_rx_buffer(const uint8_t *record,
                                           uint32_t remaining)
{
    uint32_t lock_started = DWT->CYCCNT;
    uint32_t lock_cycles;
    uint32_t data_records = 0U;
    bool malformed = false;

    aic8800_lwip_lock();
    lock_cycles = DWT->CYCCNT - lock_started;
    taskENTER_CRITICAL();
    if (lock_cycles > g_aic_rx_lwip_lock_max_cycles) {
        g_aic_rx_lwip_lock_max_cycles = lock_cycles;
    }
    taskEXIT_CRITICAL();
    while (remaining >= 4U) {
        uint32_t packet_length = aic_get_le16(record);
        uint8_t type = record[2] & 0x7fU;
        uint32_t raw_length;
        uint32_t consumed;

        if ((packet_length == 0U) && (type == 0U)) {
            break;
        }
        raw_length = ((type & AIC_USB_TYPE_CONFIG) != 0U) ?
                     packet_length + 4U :
                     packet_length + AIC_FMAC_USB_RX_HEADER_SIZE;
        if ((raw_length > remaining) || (raw_length < 4U)) {
            malformed = true;
            break;
        }
        if ((type & AIC_USB_TYPE_CONFIG) == 0U) {
            aic_fmac_deliver_data_record(record, raw_length);
            data_records++;
        }
        /* D80 firmware pads CONFIG records to a 4-byte boundary, but DATA
         * records are concatenated at their exact hardware-record length.
         * Aligning DATA here skips 1..3 bytes of the next aggregate member,
         * which previously produced a large malformed-record count. */
        consumed = ((type & AIC_USB_TYPE_CONFIG) != 0U) ?
                   ((raw_length + 3U) & ~3U) : raw_length;
        if (consumed > remaining) {
            consumed = raw_length;
        }
        record += consumed;
        remaining -= consumed;
    }
    aic_fmac_tx_refresh_ack_hold();
    aic8800_lwip_unlock();
    aic_fmac_tx_flush_ack_hold(false);
    if (malformed) {
        g_aic_tx_stats.rx_malformed_records++;
    }
    g_aic_tx_stats.rx_usb_records += data_records;
    if (data_records > g_aic_tx_stats.rx_usb_max_records) {
        g_aic_tx_stats.rx_usb_max_records = data_records;
    }
    return data_records;
}

static void aic_fmac_rx_complete(void *arg, int nbytes);

static uint8_t aic_fmac_rx_take_free(uint8_t start)
{
    uint8_t offset;

    for (offset = 0U; offset < AIC_FMAC_RX_PIPE_COUNT; offset++) {
        uint8_t index = (uint8_t)((start + offset) % AIC_FMAC_RX_PIPE_COUNT);
        uint8_t bit = (uint8_t)(1U << index);

        if ((g_aic_rx_free_mask & bit) != 0U) {
            g_aic_rx_free_mask = (uint8_t)(g_aic_rx_free_mask & ~bit);
            return index;
        }
    }
    return 0xffU;
}

static int aic_fmac_rx_submit(struct aic8800_usb_device *device, uint8_t index)
{
    return aic8800_usb_bulk_receive_async(device,
                                          g_aic_data_rx[index],
                                          AIC_FMAC_RX_SIZE,
                                          aic_fmac_rx_complete,
                                          (void *)(uintptr_t)index,
                                          false);
}

static void aic_fmac_rx_note_stall(void)
{
    g_aic_tx_stats.rx_stalls++;
}

static int aic_fmac_rx_arm_next(struct aic8800_usb_device *device,
                                uint8_t start)
{
    uint8_t index;

    index = aic_fmac_rx_take_free(start);
    if (index == 0xffU) {
        aic_fmac_rx_note_stall();
        return -USB_ERR_NOMEM;
    }
    g_aic_rx_in_flight = index;
    return (int)index;
}

static int aic_fmac_rx_submit_index(struct aic8800_usb_device *device,
                                    uint8_t index)
{
    int result;

    result = aic_fmac_rx_submit(device, index);
    if (result < 0) {
        size_t flags = usb_osal_enter_critical_section();

        if (g_aic_rx_in_flight == index) {
            g_aic_rx_in_flight = 0xffU;
        }
        g_aic_rx_free_mask =
            (uint8_t)(g_aic_rx_free_mask | (uint8_t)(1U << index));
        aic_fmac_rx_note_stall();
        usb_osal_leave_critical_section(flags);
    }
    return result;
}

static void aic_fmac_rx_complete(void *arg, int nbytes)
{
    uint8_t done_index = (uint8_t)(uintptr_t)arg;
    BaseType_t woken = pdFALSE;
    struct aic8800_usb_device *device = g_aic_rx_device;
    size_t flags;
    uint8_t arm_index = 0xffU;
    uint8_t queued_slot = 0xffU;

    if ((g_aic_rx_task == NULL) || g_aic_rx_stop ||
        (nbytes == -USB_ERR_SHUTDOWN) || (device == NULL) ||
        (done_index >= AIC_FMAC_RX_PIPE_COUNT)) {
        return;
    }

    flags = usb_osal_enter_critical_section();
    if (g_aic_rx_q_count < AIC_FMAC_RX_PIPE_COUNT) {
        uint8_t tail = (uint8_t)((g_aic_rx_q_head + g_aic_rx_q_count) %
                                 AIC_FMAC_RX_PIPE_COUNT);

        g_aic_rx_q[tail].index = done_index;
        g_aic_rx_q[tail].nbytes = nbytes;
        g_aic_rx_q[tail].ready_cycles = DWT->CYCCNT;
        queued_slot = tail;
        g_aic_rx_q_count++;
        if (nbytes > 0) {
            g_aic_tx_stats.rx_usb_transfers++;
            g_aic_tx_stats.rx_usb_bytes += (uint32_t)nbytes;
            if ((uint32_t)nbytes > g_aic_tx_stats.rx_usb_max) {
                g_aic_tx_stats.rx_usb_max = (uint32_t)nbytes;
            }
        } else if (nbytes < 0) {
            g_aic_tx_stats.rx_usb_errors++;
        }
        if (g_aic_rx_q_count > g_aic_tx_stats.rx_high_water) {
            g_aic_tx_stats.rx_high_water = g_aic_rx_q_count;
        }
    } else {
        aic_fmac_rx_note_stall();
    }
    if (g_aic_rx_in_flight == done_index) {
        g_aic_rx_in_flight = 0xffU;
    }
    if ((g_aic_rx_in_flight == 0xffU) && device->online && !g_aic_rx_stop) {
        int taken = aic_fmac_rx_arm_next(device, (uint8_t)(done_index + 1U));

        if (taken >= 0) {
            arm_index = (uint8_t)taken;
        }
    }
    usb_osal_leave_critical_section(flags);

    if (arm_index != 0xffU) {
        uint32_t rearm_started = DWT->CYCCNT;
        uint32_t rearm_cycles;

        (void)aic_fmac_rx_submit_index(device, arm_index);
        rearm_cycles = DWT->CYCCNT - rearm_started;
        flags = usb_osal_enter_critical_section();
        if (rearm_cycles > g_aic_rx_rearm_max_cycles) {
            g_aic_rx_rearm_max_cycles = rearm_cycles;
        }
        usb_osal_leave_critical_section(flags);
    }
    if (queued_slot != 0xffU) {
        /* Start scheduler-latency timing after ISR-side URB re-submission. */
        g_aic_rx_q[queued_slot].ready_cycles = DWT->CYCCNT;
    }

    if (xPortIsInsideInterrupt() != pdFALSE) {
        vTaskNotifyGiveFromISR(g_aic_rx_task, &woken);
        portYIELD_FROM_ISR(woken);
    } else {
        (void)xTaskNotifyGive(g_aic_rx_task);
    }
}

static void aic_fmac_rx_reset_pipeline(struct aic8800_usb_device *device)
{
    g_aic_rx_stop = true;
    aic8800_usb_abort_receive(device);
    while (ulTaskNotifyTake(pdTRUE, 0U) != 0U) {
    }
    taskENTER_CRITICAL();
    g_aic_rx_q_head = 0U;
    g_aic_rx_q_count = 0U;
    g_aic_rx_free_mask = AIC_FMAC_RX_FREE_ALL;
    g_aic_rx_in_flight = 0xffU;
    taskEXIT_CRITICAL();
    g_aic_rx_stop = false;
}

static void aic_fmac_rx_release(struct aic8800_usb_device *device,
                                uint8_t index)
{
    size_t flags;
    uint8_t arm_index = 0xffU;

    if (index >= AIC_FMAC_RX_PIPE_COUNT) {
        return;
    }
    flags = usb_osal_enter_critical_section();
    g_aic_rx_free_mask =
        (uint8_t)(g_aic_rx_free_mask | (uint8_t)(1U << index));
    if ((g_aic_rx_in_flight == 0xffU) && device->online &&
        !g_aic_rx_stop && g_aic_wpa2_connected) {
        int taken = aic_fmac_rx_arm_next(device, (uint8_t)(index + 1U));

        if (taken >= 0) {
            arm_index = (uint8_t)taken;
        }
    }
    usb_osal_leave_critical_section(flags);
    if (arm_index != 0xffU) {
        (void)aic_fmac_rx_submit_index(device, arm_index);
    }
}

static void aic_fmac_data_loop(struct aic8800_usb_device *device)
{
    int result;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    g_aic_rx_device = device;
    g_aic_rx_task = xTaskGetCurrentTaskHandle();
    aic_fmac_rx_reset_pipeline(device);

    USB_LOG_INFO("AIC data receive loop started (ISR re-arm %ux%u, single URB)\r\n",
                 (unsigned int)AIC_FMAC_RX_PIPE_COUNT,
                 (unsigned int)AIC_FMAC_RX_SIZE);
    {
        size_t flags = usb_osal_enter_critical_section();
        int taken = aic_fmac_rx_arm_next(device, 0U);

        usb_osal_leave_critical_section(flags);
        result = (taken >= 0) ? aic_fmac_rx_submit_index(device, (uint8_t)taken)
                              : taken;
    }
    if (result < 0) {
        USB_LOG_ERR("AIC RX arm failed: %d\r\n", result);
        g_aic_rx_task = NULL;
        g_aic_rx_device = NULL;
        return;
    }

    while (device->online && g_aic_wpa2_connected) {
        struct aic_fmac_rx_done done;
        bool have_done = false;

        if (ulTaskNotifyTake(pdFALSE,
                             pdMS_TO_TICKS(AIC_FMAC_TIMEOUT_MS)) == 0U) {
            aic_fmac_rx_reset_pipeline(device);
            if (!device->online || !g_aic_wpa2_connected) {
                break;
            }
            {
                size_t flags = usb_osal_enter_critical_section();
                int taken = aic_fmac_rx_arm_next(device, 0U);

                usb_osal_leave_critical_section(flags);
                result = (taken >= 0) ?
                         aic_fmac_rx_submit_index(device, (uint8_t)taken) :
                         taken;
            }
            if (result < 0) {
                vTaskDelay(pdMS_TO_TICKS(1U));
            }
            continue;
        }
        if (!device->online || !g_aic_wpa2_connected) {
            break;
        }

        taskENTER_CRITICAL();
        if (g_aic_rx_q_count > 0U) {
            done.index = g_aic_rx_q[g_aic_rx_q_head].index;
            done.nbytes = g_aic_rx_q[g_aic_rx_q_head].nbytes;
            done.ready_cycles = g_aic_rx_q[g_aic_rx_q_head].ready_cycles;
            g_aic_rx_q_head = (uint8_t)((g_aic_rx_q_head + 1U) %
                                        AIC_FMAC_RX_PIPE_COUNT);
            g_aic_rx_q_count--;
            have_done = true;
        }
        taskEXIT_CRITICAL();
        if (!have_done) {
            continue;
        }
        {
            uint32_t schedule_cycles = DWT->CYCCNT - done.ready_cycles;

            taskENTER_CRITICAL();
            if (schedule_cycles > g_aic_rx_schedule_max_cycles) {
                g_aic_rx_schedule_max_cycles = schedule_cycles;
            }
            taskEXIT_CRITICAL();
        }
        if (done.nbytes > 0) {
            uint32_t nbytes = (uint32_t)done.nbytes;
            uint32_t started;
            uint32_t elapsed;

            if (nbytes > AIC_FMAC_RX_SIZE) {
                nbytes = AIC_FMAC_RX_SIZE;
            }
            started = DWT->CYCCNT;
            (void)aic_fmac_process_rx_buffer(g_aic_data_rx[done.index], nbytes);
            elapsed = DWT->CYCCNT - started;
            taskENTER_CRITICAL();
            g_aic_rx_process_cycles += elapsed;
            if (elapsed > g_aic_rx_process_max_cycles) {
                g_aic_rx_process_max_cycles = elapsed;
            }
            taskEXIT_CRITICAL();
        }
        aic_fmac_rx_release(device, done.index);
    }

    aic_fmac_rx_reset_pipeline(device);
    g_aic_rx_task = NULL;
    g_aic_rx_device = NULL;
}

static void aic_fmac_probe_eapol(struct aic8800_usb_device *device)
{
    uint32_t transfer;

    for (transfer = 0U; transfer < AIC_FMAC_EAPOL_RX_TRANSFERS; transfer++) {
        int result;

        memset(g_aic_fmac_rx, 0, sizeof(g_aic_fmac_rx));
        result = aic8800_usb_bulk_receive(device, g_aic_fmac_rx,
                                          sizeof(g_aic_fmac_rx),
                                          AIC_FMAC_TIMEOUT_MS, false);
        if (result < 0) {
            if (!device->online) {
                USB_LOG_ERR("device detached while waiting for EAPOL\r\n");
                return;
            }
            continue;
        }
        if (aic_fmac_find_eapol(g_aic_fmac_rx, (uint32_t)result)) {
            if (aic_fmac_send_eapol_m2(device) == 0) {
                aic_fmac_wait_eapol_m3(device);
            }
            return;
        }
    }
    USB_LOG_ERR("EAPOL message 1/4 was not captured after %u data transfers\r\n",
                AIC_FMAC_EAPOL_RX_TRANSFERS);
}

static int aic_fmac_post_request(uintptr_t request)
{
    if (!g_aic_radio_ready || (g_aic_fmac_mq == NULL)) {
        return -USB_ERR_NOTCONN;
    }
    return usb_osal_mq_send(g_aic_fmac_mq, request);
}

int aic8800_fmac_request_scan(void)
{
    return aic_fmac_post_request(AIC_FMAC_REQ_SCAN);
}

int aic8800_fmac_request_connect(void)
{
    return aic_fmac_post_request(AIC_FMAC_REQ_CONNECT);
}

int aic8800_fmac_request_disconnect(void)
{
    g_aic_wpa2_connected = false;
    if (g_aic_fmac_device != NULL) {
        aic8800_usb_abort_receive(g_aic_fmac_device);
    }
    return aic_fmac_post_request(AIC_FMAC_REQ_DISCONNECT);
}

static int aic_fmac_run_scan(struct aic8800_usb_device *device)
{
    const struct aic8800_wifi_sta_config *sta = aic8800_wifi_get_sta_config();
    int result = 0;

    aic8800_wifi_reset_scan();
    g_aic_scan_results_2ghz = 0U;
    g_aic_scan_results_5ghz = 0U;
    if (sta->band != AIC8800_WIFI_BAND_5GHZ) {
        result = aic_fmac_scan_band(device, false);
        if (result < 0) {
            return result;
        }
    }
    if ((sta->band != AIC8800_WIFI_BAND_2GHZ) &&
        (AIC8800_FMAC_USE_5GHZ != 0U)) {
        result = aic_fmac_scan_band(device, true);
        if (result < 0) {
            return result;
        }
    }
    USB_LOG_INFO("active scan finished: 2.4GHz=%u 5GHz=%u results\r\n",
                 (unsigned int)g_aic_scan_results_2ghz,
                 (unsigned int)g_aic_scan_results_5ghz);
    return 0;
}

static int aic_fmac_run_connect(struct aic8800_usb_device *device)
{
    err_t lwip_result;
    int result;

    memset(&g_aic_target_ap, 0, sizeof(g_aic_target_ap));
    memset(g_aic_target_pmk, 0, sizeof(g_aic_target_pmk));
    if (aic8800_wifi_select_best_ap(&g_aic_target_ap) != 0) {
        USB_LOG_ERR("no scanned AP matches SSID=\"%s\"\r\n",
                    aic8800_wifi_get_sta_config()->ssid);
        return -USB_ERR_NODEV;
    }
    if (aic8800_wifi_get_pmk(g_aic_target_pmk) != 0) {
        return -USB_ERR_INVAL;
    }
    result = aic_fmac_associate_target(device);
    if (result < 0) {
        return result;
    }
    aic_fmac_probe_eapol(device);
    if (!g_aic_wpa2_connected) {
        return -USB_ERR_IO;
    }
    if (!aic_fmac_data_tx_start(device)) {
        return -USB_ERR_NOMEM;
    }
    aic8800_wifi_set_connected_ap(&g_aic_target_ap);
    aic8800_wifi_notify(AIC8800_WIFI_EVENT_CONNECTED);
    lwip_result = aic8800_lwip_init(g_aic_station_mac);
    if ((lwip_result != ERR_OK) && (lwip_result != ERR_ALREADY)) {
        USB_LOG_ERR("AIC lwIP initialization failed: %d\r\n",
                    (int)lwip_result);
        return -USB_ERR_IO;
    }
    aic8800_lwip_set_link(true);
    aic_fmac_data_loop(device);
    aic8800_lwip_set_link(false);
    aic_fmac_data_tx_stop();
    return 0;
}

static void aic8800_fmac_task(void *parameter)
{
    struct aic8800_usb_device *device = parameter;
    uint8_t stack_request[4] = { 1U, 0U, 1U << 5, 0U };
    uint8_t stack_response[2] = { 0U, 0U };
    uint8_t version_request = 0U;
    uint8_t version_response[64];
    uint8_t rf_calibration[24] = { 0U };
    uint8_t get_mac_request[4] = { 0U };
    uint8_t mac_address[8] = { 0U };
    uint8_t mm_version[32] = { 0U };
    uint8_t me_config[112];
    uint8_t channel_config[254];
#if AIC8800_FMAC_WIFI_BT_COEX_ENABLE
    uint8_t coexistence[16] = { 0U };
#endif
    uint8_t lmac_start[72] = { 0U };
    uint8_t add_interface[10] = { 0U };
    uint8_t add_interface_cfm[2] = { 0U };
    uint32_t response_length = 0U;
    uint32_t version_length;
    int result;

    /* Let the hub thread finish binding the remaining composite interfaces. */
    usb_osal_msleep(20U);
    USB_LOG_INFO("starting runtime FMAC command probe\r\n");
    result = aic_fmac_command(device, AIC_MM_SET_STACK_START_REQ,
                              AIC_MM_SET_STACK_START_CFM,
                              stack_request, sizeof(stack_request),
                              stack_response, sizeof(stack_response),
                              &response_length);
    if ((result < 0) || (response_length < sizeof(stack_response))) {
        USB_LOG_ERR("stack-start failed: %d length=%u\r\n", result,
                    (unsigned int)response_length);
        goto done;
    }
    USB_LOG_INFO("firmware stack started: 5GHz=%u vendor=0x%02x\r\n",
                 stack_response[0], stack_response[1]);

    memset(version_response, 0, sizeof(version_response));
    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_GET_FW_VERSION_REQ,
                              AIC_MM_GET_FW_VERSION_CFM,
                              &version_request, sizeof(version_request),
                              version_response, sizeof(version_response),
                              &response_length);
    if (result < 0) {
        USB_LOG_WRN("runtime firmware version query failed: %d\r\n", result);
        goto done;
    }
    version_length = (response_length > 0U) ? version_response[0] : 0U;
    if (version_length > sizeof(version_response) - 1U) {
        version_length = sizeof(version_response) - 1U;
    }
    if ((response_length > 0U) && (version_length > response_length - 1U)) {
        version_length = response_length - 1U;
    }
    USB_LOG_INFO("runtime firmware: %.*s\r\n", (int)version_length,
                 (const char *)(version_response + 1U));

    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_SET_TXPWR_REQ,
                              AIC_MM_SET_TXPWR_CFM,
                              g_aic_d80_tx_power,
                              sizeof(g_aic_d80_tx_power),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("TX-power initialization failed: %d\r\n", result);
        goto done;
    }

    aic_put_le32(rf_calibration + 0U, 0x00000f8fU);
    aic_put_le32(rf_calibration + 4U, 0x00000f0fU);
    aic_put_le32(rf_calibration + 8U, 0x0c34c008U);
    aic_put_le32(rf_calibration + 12U, 0U);
    aic_put_le32(rf_calibration + 16U, 0x00264203U);
    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_SET_RF_CALIB_REQ,
                              AIC_MM_SET_RF_CALIB_CFM,
                              rf_calibration, sizeof(rf_calibration),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("RF calibration initialization failed: %d\r\n", result);
        goto done;
    }
    USB_LOG_INFO("D80 RF defaults accepted\r\n");

    aic_put_le32(get_mac_request, 1U);
    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_GET_MAC_REQ,
                              AIC_MM_GET_MAC_CFM,
                              get_mac_request, sizeof(get_mac_request),
                              mac_address, sizeof(mac_address),
                              &response_length);
    if ((result < 0) || (response_length < 6U) ||
        !aic_mac_valid(mac_address)) {
        USB_LOG_ERR("firmware MAC query failed: %d length=%u\r\n", result,
                    (unsigned int)response_length);
        goto done;
    }
    USB_LOG_INFO("Wi-Fi MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 mac_address[0], mac_address[1], mac_address[2],
                 mac_address[3], mac_address[4], mac_address[5]);

    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_RESET_REQ, AIC_MM_RESET_CFM,
                              NULL, 0U, NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("firmware MM reset failed: %d\r\n", result);
        goto done;
    }

    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_VERSION_REQ, AIC_MM_VERSION_CFM,
                              NULL, 0U, mm_version, sizeof(mm_version),
                              &response_length);
    if ((result < 0) || (response_length < 28U)) {
        USB_LOG_ERR("firmware MM version failed: %d length=%u\r\n", result,
                    (unsigned int)response_length);
        goto done;
    }
    USB_LOG_INFO("MM version lmac=0x%08x phy=0x%08x features=0x%08x stations=%u vifs=%u\r\n",
                 (unsigned int)aic_get_le32(mm_version + 0U),
                 (unsigned int)aic_get_le32(mm_version + 12U),
                 (unsigned int)aic_get_le32(mm_version + 20U),
                 (unsigned int)aic_get_le16(mm_version + 24U),
                 mm_version[26]);

    aic_build_me_config(me_config);
    response_length = 0U;
    result = aic_fmac_command(device, AIC_ME_CONFIG_REQ, AIC_ME_CONFIG_CFM,
                              me_config, sizeof(me_config),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("ME capability configuration failed: %d\r\n", result);
        goto done;
    }
    USB_LOG_INFO("ME capabilities accepted: HT/VHT/HE, max bandwidth 80 MHz\r\n");

    aic_build_channel_config(channel_config);
    response_length = 0U;
    result = aic_fmac_command(device, AIC_ME_CHAN_CONFIG_REQ,
                              AIC_ME_CHAN_CONFIG_CFM,
                              channel_config, sizeof(channel_config),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("ME channel configuration failed: %d\r\n", result);
        goto done;
    }
    USB_LOG_INFO("ME channel table accepted: 2.4GHz=14 5GHz=25\r\n");

    /* The vendor driver starts LMAC after ME/channel configuration and before
     * creating the first VIF. phy_parameters remain at their firmware
     * defaults; only U-APSD timeout and low-power-clock accuracy are set. */
    aic_put_le32(lmac_start + 64U, 300U);
    aic_put_le16(lmac_start + 68U, 20U);
    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_START_REQ, AIC_MM_START_CFM,
                              lmac_start, sizeof(lmac_start),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("LMAC start failed: %d\r\n", result);
        goto done;
    }
    USB_LOG_INFO("LMAC started\r\n");

#if AIC8800_FMAC_WIFI_BT_COEX_ENABLE
    /* mm_set_coex_req, as emitted by the AIC Windows/Linux driver:
     * bt_on=1, disable_coexnull=0, enable_nullcts=1, periodic_timer=0,
     * coex_timeslot_set=0, coex_timeslot[0..1]=0.  The five byte fields are
     * followed by three bytes of ABI padding and two little-endian words.
     *
     * This path is disabled by default: v64 sent it before MM_START and v65
     * sent it here after MM_START; both were accepted by firmware but were
     * followed by repeated EAPOL M1 frames and no M3. */
    coexistence[0] = 1U;
    coexistence[2] = 1U;
    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_SET_COEX_REQ,
                              AIC_MM_SET_COEX_CFM,
                              coexistence, sizeof(coexistence),
                              NULL, 0U, &response_length);
    if (result < 0) {
        USB_LOG_ERR("Wi-Fi/BT coexistence initialization failed: %d\r\n",
                    result);
        goto done;
    }
    USB_LOG_INFO("Wi-Fi/BT coexistence accepted after LMAC start: NULL/CTS enabled\r\n");
#endif

    add_interface[0] = 0U; /* NL80211_IFTYPE_STATION */
    memcpy(add_interface + 2U, mac_address, 6U);
    response_length = 0U;
    result = aic_fmac_command(device, AIC_MM_ADD_IF_REQ, AIC_MM_ADD_IF_CFM,
                              add_interface, sizeof(add_interface),
                              add_interface_cfm, sizeof(add_interface_cfm),
                              &response_length);
    if ((result < 0) || (response_length < 2U) ||
        (add_interface_cfm[0] != 0U)) {
        USB_LOG_ERR("station VIF creation failed: %d length=%u status=%u\r\n",
                    result, (unsigned int)response_length,
                    add_interface_cfm[0]);
        goto done;
    }
    memcpy(g_aic_station_mac, mac_address, sizeof(g_aic_station_mac));
    g_aic_station_vif = add_interface_cfm[1];
    aic8800_wifi_set_mac(g_aic_station_mac);
    USB_LOG_INFO("station VIF %u enabled, MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 g_aic_station_vif, g_aic_station_mac[0],
                 g_aic_station_mac[1], g_aic_station_mac[2],
                 g_aic_station_mac[3], g_aic_station_mac[4],
                 g_aic_station_mac[5]);
    aic8800_wifi_notify(AIC8800_WIFI_EVENT_READY);
    g_aic_radio_ready = true;
#if AIC8800_WIFI_AUTO_CONNECT
    aic8800_wifi_mark_connecting();
    (void)usb_osal_mq_send(g_aic_fmac_mq, AIC_FMAC_REQ_CONNECT);
#endif
    while (device->online) {
        uintptr_t request = 0U;

        if (usb_osal_mq_recv(g_aic_fmac_mq, &request,
                             USB_OSAL_WAITING_FOREVER) != 0) {
            continue;
        }
        if (!device->online) {
            break;
        }
        if (request == AIC_FMAC_REQ_DISCONNECT) {
            g_aic_wpa2_connected = false;
            continue;
        }
        if (request == AIC_FMAC_REQ_SCAN) {
            if (aic_fmac_run_scan(device) == 0) {
                aic8800_wifi_notify(AIC8800_WIFI_EVENT_SCAN_DONE);
            }
            continue;
        }
        if (request != AIC_FMAC_REQ_CONNECT) {
            continue;
        }
        result = aic_fmac_run_scan(device);
        if (result < 0) {
            aic8800_wifi_notify(AIC8800_WIFI_EVENT_CONNECT_FAILED);
            continue;
        }
        aic8800_wifi_notify(AIC8800_WIFI_EVENT_SCAN_DONE);
        result = aic_fmac_run_connect(device);
        if (result < 0) {
            aic8800_wifi_notify(AIC8800_WIFI_EVENT_CONNECT_FAILED);
            continue;
        }
        aic8800_wifi_notify(AIC8800_WIFI_EVENT_DISCONNECTED);
    }

done:
    g_aic_radio_ready = false;
    aic_fmac_data_tx_stop();
    aic8800_wifi_on_transport_lost();
    g_aic_fmac_task_running = false;
    usb_osal_thread_delete(NULL);
}

void aic8800_fmac_attach(struct aic8800_usb_device *device)
{
    if ((device == NULL) || !device->runtime || !device->online) {
        return;
    }
    if (g_aic_fmac_task_running) {
        USB_LOG_WRN("FMAC attach task is already running\r\n");
        return;
    }
    g_aic_station_vif = 0xffU;
    g_aic_ap_station = 0xffU;
    g_aic_wpa2_connected = false;
    g_aic_radio_ready = false;
    g_aic_fmac_device = device;
    memset(&g_aic_target_ap, 0, sizeof(g_aic_target_ap));
    memset(g_aic_target_pmk, 0, sizeof(g_aic_target_pmk));
    memset(g_aic_station_mac, 0, sizeof(g_aic_station_mac));
    memset(g_aic_snonce, 0, sizeof(g_aic_snonce));
    memset(g_aic_ptk, 0, sizeof(g_aic_ptk));
    if (g_aic_tx_mutex == NULL) {
        g_aic_tx_mutex = xSemaphoreCreateMutex();
    }
    if (g_aic_tx_mutex == NULL) {
        USB_LOG_ERR("failed to create AIC TX mutex\r\n");
        return;
    }
    if (g_aic_fmac_mq == NULL) {
        g_aic_fmac_mq = usb_osal_mq_create(4U);
    }
    if (g_aic_fmac_mq == NULL) {
        USB_LOG_ERR("failed to create FMAC request queue\r\n");
        return;
    }
    {
        uintptr_t discard = 0U;

        while (usb_osal_mq_recv(g_aic_fmac_mq, &discard, 0U) == 0) {
        }
    }
    g_aic_fmac_task_running = true;
    if (usb_osal_thread_create("aic_fmac", 1536U * 4U, AIC8800_FMAC_OSAL_PRIO,
                               aic8800_fmac_task, device) == NULL) {
        g_aic_fmac_task_running = false;
        USB_LOG_ERR("failed to create FMAC attach task\r\n");
    }
}

void aic8800_fmac_detach(struct aic8800_usb_device *device)
{
    g_aic_radio_ready = false;
    g_aic_wpa2_connected = false;
    aic_fmac_data_tx_stop();
    aic8800_usb_abort_receive(device);
    if (g_aic_fmac_mq != NULL) {
        (void)usb_osal_mq_send(g_aic_fmac_mq, AIC_FMAC_REQ_DISCONNECT);
    }
    g_aic_station_vif = 0xffU;
    g_aic_ap_station = 0xffU;
    g_aic_fmac_device = NULL;
    memset(g_aic_station_mac, 0, sizeof(g_aic_station_mac));
    memset(&g_aic_target_ap, 0, sizeof(g_aic_target_ap));
    memset(g_aic_target_pmk, 0, sizeof(g_aic_target_pmk));
    aic8800_wifi_on_transport_lost();
}
