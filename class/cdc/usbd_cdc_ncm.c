/*
 * Copyright (c) 2026, 54fire
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "usbd_core.h"
#include "usbd_cdc_ncm.h"

#define CDC_NCM_OUT_EP_IDX 0
#define CDC_NCM_IN_EP_IDX  1
#define CDC_NCM_INT_EP_IDX 2

#define CONFIG_CDC_NCM_ETH_MAX_SEGSZE 1514U
#define CONFIG_CDC_NCM_NTB_MAX_SIZE   2048U
#define CDC_NCM_NTH16_LEN             12U
#define CDC_NCM_NDP16_LEN             16U
#define CDC_NCM_DATAGRAM_OFFSET       16U

static struct usbd_endpoint cdc_ncm_ep_data[3];

#ifdef CONFIG_USBDEV_CDC_NCM_USING_LWIP
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ncm_rx_buffer[USB_ALIGN_UP(CONFIG_CDC_NCM_NTB_MAX_SIZE, CONFIG_USB_ALIGN_SIZE)];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ncm_tx_buffer[USB_ALIGN_UP(CONFIG_CDC_NCM_NTB_MAX_SIZE, CONFIG_USB_ALIGN_SIZE)];
#endif
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ncm_ctrl_buf[USB_ALIGN_UP(32, CONFIG_USB_ALIGN_SIZE)];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ncm_notify_buf[USB_ALIGN_UP(16, CONFIG_USB_ALIGN_SIZE)];

static volatile uint32_t g_cdc_ncm_rx_ntb_length = 0;
static volatile uint32_t g_cdc_ncm_tx_ntb_length = 0;
static volatile bool g_cdc_ncm_data_alt_active = false;
static uint8_t *g_cdc_ncm_rx_data_buffer = NULL;
static uint32_t g_cdc_ncm_rx_total_length = 0;
static uint16_t g_cdc_ncm_rx_datagram_pos = 0;
static uint16_t g_cdc_ncm_rx_ndp_index = 0;
static uint16_t g_cdc_ncm_rx_sequence = 0;
static uint16_t g_cdc_ncm_tx_sequence = 0;
static uint16_t g_cdc_ncm_max_datagram_size = CONFIG_CDC_NCM_ETH_MAX_SEGSZE;
static uint32_t g_cdc_ncm_ntb_in_max_size = CONFIG_CDC_NCM_NTB_MAX_SIZE;
static uint32_t g_cdc_ncm_ntb_out_max_size = CONFIG_CDC_NCM_NTB_MAX_SIZE;
static uint16_t g_cdc_ncm_ntb_format = 0;
static uint16_t g_cdc_ncm_packet_filter = 0;
static volatile uint8_t g_current_net_status = 0;
static volatile uint8_t g_cmd_intf = 0;
static uint8_t g_cdc_ncm_mac[6] = { 0 };
static bool g_cdc_ncm_ep_registered = false;

static uint32_t g_connect_speed_table[2] = { CDC_NCM_CONNECT_SPEED_UPSTREAM, CDC_NCM_CONNECT_SPEED_DOWNSTREAM };

static void usbd_cdc_ncm_send_notify(uint8_t notifycode, uint8_t value, uint32_t *speed)
{
    struct cdc_eth_notification *notify = (struct cdc_eth_notification *)g_cdc_ncm_notify_buf;
    uint8_t bytes2send = 0;

    memset(g_cdc_ncm_notify_buf, 0, 16);
    notify->bmRequestType = CDC_ECM_BMREQUEST_TYPE_ECM;
    notify->bNotificationType = notifycode;
    notify->wIndex = g_cmd_intf;

    switch (notifycode) {
    case CDC_ECM_NOTIFY_CODE_NETWORK_CONNECTION:
        notify->wValue = value;
        bytes2send = 8;
        break;
    case CDC_ECM_NOTIFY_CODE_RESPONSE_AVAILABLE:
        bytes2send = 8;
        break;
    case CDC_ECM_NOTIFY_CODE_CONNECTION_SPEED_CHANGE:
        notify->wLength = 8;
        if (speed) {
            memcpy(notify->data, speed, 8);
        }
        bytes2send = 16;
        break;
    default:
        break;
    }

    if (usb_device_is_configured(0) && bytes2send) {
        usbd_ep_start_write(0, cdc_ncm_ep_data[CDC_NCM_INT_EP_IDX].ep_addr, g_cdc_ncm_notify_buf, bytes2send);
    }
}

static void usbd_cdc_ncm_fill_ntb_parameters(void)
{
    memset(g_cdc_ncm_ctrl_buf, 0, 32);
    SET_LE16(&g_cdc_ncm_ctrl_buf[0], 28);                         /* wLength */
    SET_LE16(&g_cdc_ncm_ctrl_buf[2], 0x0001);                     /* NTB16 only */
    SET_LE32(&g_cdc_ncm_ctrl_buf[4], g_cdc_ncm_ntb_in_max_size);  /* device-to-host NTB */
    SET_LE16(&g_cdc_ncm_ctrl_buf[8], 4);                          /* wNdbInDivisor */
    SET_LE16(&g_cdc_ncm_ctrl_buf[10], 0);                         /* wNdbInPayloadRemainder */
    SET_LE16(&g_cdc_ncm_ctrl_buf[12], 4);                         /* wNdbInAlignment */
    SET_LE16(&g_cdc_ncm_ctrl_buf[14], 0);                         /* wReserved */
    SET_LE32(&g_cdc_ncm_ctrl_buf[16], g_cdc_ncm_ntb_out_max_size);/* host-to-device NTB */
    SET_LE16(&g_cdc_ncm_ctrl_buf[20], 4);                         /* wNdbOutDivisor */
    SET_LE16(&g_cdc_ncm_ctrl_buf[22], 0);                         /* wNdbOutPayloadRemainder */
    SET_LE16(&g_cdc_ncm_ctrl_buf[24], 4);                         /* wNdbOutAlignment */
    SET_LE16(&g_cdc_ncm_ctrl_buf[26], 1);                         /* one datagram preferred */
}

static int cdc_ncm_class_interface_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    (void)busid;

    g_cmd_intf = LO_BYTE(setup->wIndex);

    switch (setup->bRequest) {
    case CDC_REQUEST_GET_NTB_PARAMETERS:
        usbd_cdc_ncm_fill_ntb_parameters();
        *data = g_cdc_ncm_ctrl_buf;
        *len = 28;
        break;
    case CDC_REQUEST_GET_NTB_FORMAT:
        SET_LE16(g_cdc_ncm_ctrl_buf, g_cdc_ncm_ntb_format);
        *data = g_cdc_ncm_ctrl_buf;
        *len = 2;
        break;
    case CDC_REQUEST_SET_NTB_FORMAT:
        if (setup->wValue != 0) {
            return -1;
        }
        g_cdc_ncm_ntb_format = 0;
        break;
    case CDC_REQUEST_GET_NTB_INPUT_SIZE:
        SET_LE32(g_cdc_ncm_ctrl_buf, g_cdc_ncm_ntb_in_max_size);
        *data = g_cdc_ncm_ctrl_buf;
        *len = 4;
        break;
    case CDC_REQUEST_SET_NTB_INPUT_SIZE:
        if (setup->wLength >= 4 && data && *data) {
            uint32_t ntb_size = GET_LE32(*data);
            if (ntb_size >= (CDC_NCM_DATAGRAM_OFFSET + CDC_NCM_NDP16_LEN) && ntb_size <= CONFIG_CDC_NCM_NTB_MAX_SIZE) {
                g_cdc_ncm_ntb_in_max_size = ntb_size;
            }
        }
        break;
    case CDC_REQUEST_GET_MAX_DATAGRAM_SIZE:
        SET_LE16(g_cdc_ncm_ctrl_buf, g_cdc_ncm_max_datagram_size);
        *data = g_cdc_ncm_ctrl_buf;
        *len = 2;
        break;
    case CDC_REQUEST_SET_MAX_DATAGRAM_SIZE:
        if (setup->wLength >= 2 && data && *data) {
            uint16_t max_datagram_size = GET_LE16(*data);
            if (max_datagram_size >= 64 && max_datagram_size <= CONFIG_CDC_NCM_ETH_MAX_SEGSZE) {
                g_cdc_ncm_max_datagram_size = max_datagram_size;
            }
        } else if (setup->wValue >= 64 && setup->wValue <= CONFIG_CDC_NCM_ETH_MAX_SEGSZE) {
            g_cdc_ncm_max_datagram_size = setup->wValue;
        }
        break;
    case CDC_REQUEST_GET_CRC_MODE:
        SET_LE16(g_cdc_ncm_ctrl_buf, 0);
        *data = g_cdc_ncm_ctrl_buf;
        *len = 2;
        break;
    case CDC_REQUEST_SET_CRC_MODE:
        if (setup->wValue != 0) {
            return -1;
        }
        break;
    case CDC_REQUEST_GET_NET_ADDRESS:
        memcpy(g_cdc_ncm_ctrl_buf, g_cdc_ncm_mac, 6);
        *data = g_cdc_ncm_ctrl_buf;
        *len = 6;
        break;
    case CDC_REQUEST_SET_NET_ADDRESS:
        if (setup->wLength >= 6 && data && *data) {
            memcpy(g_cdc_ncm_mac, *data, 6);
        }
        break;
    case CDC_REQUEST_SET_ETHERNET_PACKET_FILTER:
        g_cdc_ncm_packet_filter = setup->wValue;
        g_connect_speed_table[0] = 100000000;
        g_connect_speed_table[1] = 100000000;
        usbd_cdc_ncm_set_connect(g_cdc_ncm_packet_filter != 0, g_connect_speed_table);
        break;
    case CDC_REQUEST_SET_ETHERNET_MULTICAST_FILTERS:
    case CDC_REQUEST_SET_ETHERNET_PMP_FILTER:
        break;
    default:
        USB_LOG_WRN("Unhandled CDC NCM Class bRequest 0x%02x\r\n", setup->bRequest);
        return -1;
    }

    return 0;
}

static void cdc_ncm_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    (void)busid;

    switch (event) {
    case USBD_EVENT_RESET:
    case USBD_EVENT_DISCONNECTED:
        g_current_net_status = 0;
        g_cdc_ncm_rx_ntb_length = 0;
        g_cdc_ncm_tx_ntb_length = 0;
        g_cdc_ncm_data_alt_active = false;
        g_cdc_ncm_rx_datagram_pos = 0;
        g_cdc_ncm_rx_ndp_index = 0;
        break;
    case USBD_EVENT_SET_INTERFACE: {
#ifdef CONFIG_USBDEV_CDC_NCM_USING_LWIP
        struct usb_interface_descriptor *desc = (struct usb_interface_descriptor *)arg;

        if (desc && desc->bAlternateSetting == 1) {
            g_cdc_ncm_data_alt_active = true;
            usbd_cdc_ncm_start_read(g_cdc_ncm_rx_buffer, g_cdc_ncm_ntb_out_max_size);
        } else {
            g_cdc_ncm_data_alt_active = false;
            g_cdc_ncm_tx_ntb_length = 0;
        }
#else
        (void)arg;
#endif
        break;
    }
    default:
        break;
    }
}

static void cdc_ncm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;

    g_cdc_ncm_rx_ntb_length = nbytes;
    g_cdc_ncm_rx_datagram_pos = 0;
    g_cdc_ncm_rx_ndp_index = 0;
    usbd_cdc_ncm_data_recv_done(g_cdc_ncm_rx_ntb_length);
}

static void cdc_ncm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;

    if ((nbytes % usbd_get_ep_mps(0, ep)) == 0 && nbytes) {
        usbd_ep_start_write(0, ep, NULL, 0);
    } else {
        usbd_cdc_ncm_data_send_done(g_cdc_ncm_tx_ntb_length);
        g_cdc_ncm_tx_ntb_length = 0;
    }
}

static void cdc_ncm_int_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    if (g_current_net_status == 2) {
        g_current_net_status = 3;
        usbd_cdc_ncm_send_notify(CDC_ECM_NOTIFY_CODE_CONNECTION_SPEED_CHANGE, 0, g_connect_speed_table);
    } else {
        g_current_net_status = 0;
    }
}

int usbd_cdc_ncm_start_write(uint8_t *buf, uint32_t len)
{
    int ret;

    if (!usb_device_is_configured(0) || !g_cdc_ncm_data_alt_active) {
        return -USB_ERR_NOTCONN;
    }

    if (g_cdc_ncm_tx_ntb_length > 0) {
        return -USB_ERR_BUSY;
    }

    /* mark busy before submitting: the transfer may complete in interrupt
       context before usbd_ep_start_write() returns */
    g_cdc_ncm_tx_ntb_length = len;
    ret = usbd_ep_start_write(0, cdc_ncm_ep_data[CDC_NCM_IN_EP_IDX].ep_addr, buf, len);
    if (ret != 0) {
        g_cdc_ncm_tx_ntb_length = 0;
        return ret;
    }

    USB_LOG_DBG("txlen:%d\r\n", g_cdc_ncm_tx_ntb_length);
    return 0;
}

int usbd_cdc_ncm_start_read(uint8_t *buf, uint32_t len)
{
    if (!usb_device_is_configured(0)) {
        return -USB_ERR_NOTCONN;
    }

    g_cdc_ncm_rx_data_buffer = buf;
    g_cdc_ncm_rx_total_length = len;
    g_cdc_ncm_rx_ntb_length = 0;
    g_cdc_ncm_rx_datagram_pos = 0;
    g_cdc_ncm_rx_ndp_index = 0;
    return usbd_ep_start_read(0, cdc_ncm_ep_data[CDC_NCM_OUT_EP_IDX].ep_addr, buf, len);
}

#ifdef CONFIG_USBDEV_CDC_NCM_USING_LWIP
static bool cdc_ncm_check_ndp16(uint16_t ndp_index)
{
    struct cdc_ncm_ndp16 *ndp16;
    uint32_t signature;
    uint16_t ndp_len;

    if (ndp_index == 0 || (uint32_t)ndp_index + sizeof(struct cdc_ncm_ndp16) > g_cdc_ncm_rx_ntb_length) {
        return false;
    }

    ndp16 = (struct cdc_ncm_ndp16 *)&g_cdc_ncm_rx_data_buffer[ndp_index];
    signature = GET_LE32((uint8_t *)&ndp16->dwSignature);
    ndp_len = GET_LE16((uint8_t *)&ndp16->wLength);

    if ((signature != CDC_NCM_NDP16_SIGNATURE_NCM0) &&
        (signature != CDC_NCM_NDP16_SIGNATURE_NCM1)) {
        USB_LOG_WRN("invalid ncm ndp16 signature\r\n");
        return false;
    }

    if (ndp_len < 12 || (uint32_t)ndp_index + ndp_len > g_cdc_ncm_rx_ntb_length) {
        USB_LOG_WRN("invalid ncm ndp16 len\r\n");
        return false;
    }

    return true;
}

static bool cdc_ncm_prepare_rx_ndp(void)
{
    struct cdc_ncm_nth16 *nth16;
    uint16_t ndp_index;

    if (g_cdc_ncm_rx_ntb_length < (CDC_NCM_NTH16_LEN + CDC_NCM_NDP16_LEN)) {
        return false;
    }

    nth16 = (struct cdc_ncm_nth16 *)g_cdc_ncm_rx_data_buffer;
    ndp_index = GET_LE16((uint8_t *)&nth16->wNdpIndex);
    if (GET_LE32((uint8_t *)&nth16->dwSignature) != CDC_NCM_NTH16_SIGNATURE ||
        GET_LE16((uint8_t *)&nth16->wHeaderLength) != CDC_NCM_NTH16_LEN ||
        (uint32_t)GET_LE16((uint8_t *)&nth16->wBlockLength) > g_cdc_ncm_rx_ntb_length ||
        !cdc_ncm_check_ndp16(ndp_index)) {
        USB_LOG_WRN("invalid ncm nth16\r\n");
        return false;
    }

    g_cdc_ncm_rx_ndp_index = ndp_index;
    g_cdc_ncm_rx_sequence = GET_LE16((uint8_t *)&nth16->wSequence);
    (void)g_cdc_ncm_rx_sequence;
    return true;
}

struct pbuf *usbd_cdc_ncm_eth_rx(void)
{
    struct cdc_ncm_ndp16 *ndp16;
    uint16_t ndp_len;
    uint16_t next_ndp_index;

    if (g_cdc_ncm_rx_ntb_length == 0 || g_cdc_ncm_rx_data_buffer == NULL) {
        return NULL;
    }

    if (g_cdc_ncm_rx_ndp_index == 0 && !cdc_ncm_prepare_rx_ndp()) {
        usbd_cdc_ncm_start_read(g_cdc_ncm_rx_buffer, g_cdc_ncm_ntb_out_max_size);
        return NULL;
    }

    for (;;) {
        ndp16 = (struct cdc_ncm_ndp16 *)&g_cdc_ncm_rx_data_buffer[g_cdc_ncm_rx_ndp_index];
        ndp_len = GET_LE16((uint8_t *)&ndp16->wLength);

        while ((8 + 4 * (g_cdc_ncm_rx_datagram_pos + 1)) <= ndp_len) {
            struct cdc_ncm_ndp16_datagram *datagram;
            uint16_t index;
            uint16_t length;
            struct pbuf *p;

            datagram = (struct cdc_ncm_ndp16_datagram *)&g_cdc_ncm_rx_data_buffer[g_cdc_ncm_rx_ndp_index + 8 + 4 * g_cdc_ncm_rx_datagram_pos];
            g_cdc_ncm_rx_datagram_pos++;
            index = GET_LE16((uint8_t *)&datagram->wDatagramIndex);
            length = GET_LE16((uint8_t *)&datagram->wDatagramLength);

            if (index == 0 || length == 0) {
                break;
            }
            if ((uint32_t)index + length > g_cdc_ncm_rx_ntb_length || length > g_cdc_ncm_max_datagram_size) {
                USB_LOG_WRN("invalid ncm datagram index:%u len:%u\r\n", index, length);
                continue;
            }

            p = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
            if (p == NULL) {
                USB_LOG_WRN("ncm pbuf alloc failed\r\n");
                usbd_cdc_ncm_start_read(g_cdc_ncm_rx_buffer, g_cdc_ncm_ntb_out_max_size);
                return NULL;
            }
            if (pbuf_take(p, &g_cdc_ncm_rx_data_buffer[index], length) != ERR_OK) {
                pbuf_free(p);
                continue;
            }

            USB_LOG_DBG("rxlen:%d\r\n", length);
            return p;
        }

        /* current ndp is exhausted, follow the ndp chain if present */
        next_ndp_index = GET_LE16((uint8_t *)&ndp16->wNextNdpIndex);
        if (next_ndp_index == 0 || !cdc_ncm_check_ndp16(next_ndp_index)) {
            break;
        }
        g_cdc_ncm_rx_ndp_index = next_ndp_index;
        g_cdc_ncm_rx_datagram_pos = 0;
    }

    usbd_cdc_ncm_start_read(g_cdc_ncm_rx_buffer, g_cdc_ncm_ntb_out_max_size);
    return NULL;
}

int usbd_cdc_ncm_eth_tx(struct pbuf *p)
{
    struct cdc_ncm_nth16 *nth16;
    struct cdc_ncm_ndp16 *ndp16;
    struct cdc_ncm_ndp16_datagram *datagram;
    struct pbuf *q;
    uint8_t *buffer;
    uint16_t payload_len;
    uint16_t aligned_payload_len;
    uint16_t ndp_index;
    uint16_t block_len;

    if (!usb_device_is_configured(0)) {
        return -USB_ERR_NOTCONN;
    }

    if (g_cdc_ncm_tx_ntb_length > 0) {
        return -USB_ERR_BUSY;
    }

    if (p->tot_len > g_cdc_ncm_max_datagram_size) {
        USB_LOG_WRN("ncm tx frame too large:%u\r\n", p->tot_len);
        return -USB_ERR_INVAL;
    }

    payload_len = p->tot_len;
    aligned_payload_len = USB_ALIGN_UP(payload_len, 4);
    ndp_index = CDC_NCM_DATAGRAM_OFFSET + aligned_payload_len;
    block_len = ndp_index + CDC_NCM_NDP16_LEN;
    if (block_len > sizeof(g_cdc_ncm_tx_buffer) || block_len > g_cdc_ncm_ntb_in_max_size) {
        return -USB_ERR_INVAL;
    }

    memset(g_cdc_ncm_tx_buffer, 0, block_len);
    buffer = &g_cdc_ncm_tx_buffer[CDC_NCM_DATAGRAM_OFFSET];
    for (q = p; q != NULL && payload_len > 0; q = q->next) {
        uint16_t copy_len = MIN(q->len, payload_len);
        usb_memcpy(buffer, q->payload, copy_len);
        buffer += copy_len;
        payload_len -= copy_len;
    }
    payload_len = p->tot_len;

    nth16 = (struct cdc_ncm_nth16 *)&g_cdc_ncm_tx_buffer[0];
    SET_LE32((uint8_t *)&nth16->dwSignature, CDC_NCM_NTH16_SIGNATURE);
    SET_LE16((uint8_t *)&nth16->wHeaderLength, CDC_NCM_NTH16_LEN);
    SET_LE16((uint8_t *)&nth16->wSequence, g_cdc_ncm_tx_sequence++);
    SET_LE16((uint8_t *)&nth16->wBlockLength, block_len);
    SET_LE16((uint8_t *)&nth16->wNdpIndex, ndp_index);

    ndp16 = (struct cdc_ncm_ndp16 *)&g_cdc_ncm_tx_buffer[ndp_index];
    SET_LE32((uint8_t *)&ndp16->dwSignature, CDC_NCM_NDP16_SIGNATURE_NCM0);
    SET_LE16((uint8_t *)&ndp16->wLength, CDC_NCM_NDP16_LEN);
    SET_LE16((uint8_t *)&ndp16->wNextNdpIndex, 0);

    datagram = (struct cdc_ncm_ndp16_datagram *)&g_cdc_ncm_tx_buffer[ndp_index + 8];
    SET_LE16((uint8_t *)&datagram[0].wDatagramIndex, CDC_NCM_DATAGRAM_OFFSET);
    SET_LE16((uint8_t *)&datagram[0].wDatagramLength, payload_len);
    datagram[1].wDatagramIndex = 0;
    datagram[1].wDatagramLength = 0;

    return usbd_cdc_ncm_start_write(g_cdc_ncm_tx_buffer, block_len);
}
#endif

struct usbd_interface *usbd_cdc_ncm_init_intf(struct usbd_interface *intf,
                                               const uint8_t int_ep,
                                               const uint8_t out_ep,
                                               const uint8_t in_ep,
                                               uint8_t mac[6])
{
    memcpy(g_cdc_ncm_mac, mac, 6);

    intf->class_interface_handler = cdc_ncm_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = cdc_ncm_notify_handler;

    /* control and data interfaces share the same endpoints, register them only once */
    if (!g_cdc_ncm_ep_registered) {
        g_cdc_ncm_ep_registered = true;

        cdc_ncm_ep_data[CDC_NCM_OUT_EP_IDX].ep_addr = out_ep;
        cdc_ncm_ep_data[CDC_NCM_OUT_EP_IDX].ep_cb = cdc_ncm_bulk_out;
        cdc_ncm_ep_data[CDC_NCM_IN_EP_IDX].ep_addr = in_ep;
        cdc_ncm_ep_data[CDC_NCM_IN_EP_IDX].ep_cb = cdc_ncm_bulk_in;
        cdc_ncm_ep_data[CDC_NCM_INT_EP_IDX].ep_addr = int_ep;
        cdc_ncm_ep_data[CDC_NCM_INT_EP_IDX].ep_cb = cdc_ncm_int_in;

        usbd_add_endpoint(0, &cdc_ncm_ep_data[CDC_NCM_OUT_EP_IDX]);
        usbd_add_endpoint(0, &cdc_ncm_ep_data[CDC_NCM_IN_EP_IDX]);
        usbd_add_endpoint(0, &cdc_ncm_ep_data[CDC_NCM_INT_EP_IDX]);
    }

    return intf;
}

int usbd_cdc_ncm_set_connect(bool connect, uint32_t speed[2])
{
    if (!usb_device_is_configured(0)) {
        return -USB_ERR_NOTCONN;
    }

    if (connect) {
        g_current_net_status = 2;
        if (speed) {
            memcpy(g_connect_speed_table, speed, 8);
        }
        usbd_cdc_ncm_send_notify(CDC_ECM_NOTIFY_CODE_NETWORK_CONNECTION, CDC_ECM_NET_CONNECTED, NULL);
    } else {
        g_current_net_status = 1;
        usbd_cdc_ncm_send_notify(CDC_ECM_NOTIFY_CODE_NETWORK_CONNECTION, CDC_ECM_NET_DISCONNECTED, NULL);
    }

    return 0;
}

__WEAK void usbd_cdc_ncm_data_recv_done(uint32_t len)
{
    (void)len;
}

__WEAK void usbd_cdc_ncm_data_send_done(uint32_t len)
{
    (void)len;
}
