/*
 * Copyright (c) 2024 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_adb.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbd_adb"
#include "usb_log.h"

#define ADB_OUT_EP_IDX 0u
#define ADB_IN_EP_IDX  1u

#define ADB_RX_STATE_READ_MSG  0u /* armed, waiting for the 24-byte header   */
#define ADB_RX_STATE_READ_DATA 1u /* armed, waiting for the payload          */
#define ADB_RX_STATE_COMPLETE  2u /* full packet ready, ISR already notified */

#define ADB_TX_STATE_MSG      0u /* 24-byte header in flight                    */
#define ADB_TX_STATE_DATA     1u /* payload in flight                           */
#define ADB_TX_STATE_COMPLETE 2u /* transfer finished, the next send may start  */

#define ADB_DAEMON_THREAD_STACK 4096u
#define ADB_DAEMON_THREAD_PRIO  10u

struct usbd_adb_priv {
    struct usbd_endpoint ep_data[2];
    uint8_t busid;

    const struct adb_service *svc_tab[ADB_MAX_SERVICE];
    uint32_t remote_id[ADB_MAX_SERVICE];
    bool svc_opened[ADB_MAX_SERVICE];

    volatile uint8_t rx_state;
    volatile uint8_t tx_state;

    usb_osal_sem_t rx_sem;
    usb_osal_sem_t tx_sem;
    usb_osal_mutex_t tx_lock;
    usb_osal_thread_t rx_thread;
} g_usbd_adb;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX struct adb_packet adb_tx_packet;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX struct adb_packet adb_rx_packet;

static uint32_t adb_packet_checksum(const struct adb_packet *packet)
{
    uint32_t sum = 0;
    uint32_t i;

    for (i = 0; i < packet->msg.data_length; i++) {
        sum += (uint32_t)packet->payload[i];
    }

    return sum;
}

static int adb_svc_slot(uint32_t localid)
{
    uint32_t i;

    for (i = 0; i < ADB_MAX_SERVICE; i++) {
        if (g_usbd_adb.svc_tab[i] && (g_usbd_adb.svc_tab[i]->localid == localid)) {
            return (int)i;
        }
    }

    return -1;
}

int usbd_adb_service_register(const struct adb_service *svc)
{
    uint32_t i;

    if (!svc || !svc->name || (svc->localid == 0u)) {
        return -1;
    }

    if (adb_svc_slot(svc->localid) >= 0) {
        return -1; /* local id already taken */
    }

    for (i = 0; i < ADB_MAX_SERVICE; i++) {
        if (g_usbd_adb.svc_tab[i] == NULL) {
            g_usbd_adb.svc_tab[i] = svc;
            g_usbd_adb.remote_id[i] = 0;
            g_usbd_adb.svc_opened[i] = false;
            return 0;
        }
    }

    return -1; /* table full */
}

static uint32_t usbd_adb_service_get_remoteid(uint32_t localid)
{
    int slot = adb_svc_slot(localid);

    return (slot >= 0) ? g_usbd_adb.remote_id[slot] : 0u;
}

static bool usbd_adb_service_is_opened(uint32_t localid)
{
    int slot = adb_svc_slot(localid);

    return (slot >= 0) ? g_usbd_adb.svc_opened[slot] : false;
}

static int usbd_adb_send_internal(uint32_t command,
                                  uint32_t arg0,
                                  uint32_t arg1,
                                  const uint8_t *data,
                                  uint32_t len)
{
    int ret;

    if (len && !data) {
        return -1;
    }

    /* one transmitter at a time */
    usb_osal_mutex_take(g_usbd_adb.tx_lock);

    adb_tx_packet.msg.command = command;
    adb_tx_packet.msg.arg0 = arg0;
    adb_tx_packet.msg.arg1 = arg1;
    adb_tx_packet.msg.data_length = len;
    if (len) {
        memcpy(adb_tx_packet.payload, data, len);
    }
    adb_tx_packet.msg.data_crc32 = adb_packet_checksum(&adb_tx_packet);
    adb_tx_packet.msg.magic = command ^ 0xffffffffu;

    /* drop a stale completion left behind by a previous timeout */
    usb_osal_sem_reset(g_usbd_adb.tx_sem);

    g_usbd_adb.tx_state = ADB_TX_STATE_MSG;
    usbd_ep_start_write(g_usbd_adb.busid, g_usbd_adb.ep_data[ADB_IN_EP_IDX].ep_addr,
                        (uint8_t *)&adb_tx_packet.msg, sizeof(struct adb_msg));

    ret = usb_osal_sem_take(g_usbd_adb.tx_sem, ADB_TX_TIMEOUT_MS);
    if (ret != 0) {
        USB_LOG_ERR("tx timeout, cmd %08x len %u\r\n", (unsigned)command, (unsigned)len);
        g_usbd_adb.tx_state = ADB_TX_STATE_COMPLETE;
        ret = -2;
    } else {
        ret = 0;
    }

    usb_osal_mutex_give(g_usbd_adb.tx_lock);

    return ret;
}

static void adb_reply_okay(uint32_t localid, uint32_t remoteid)
{
    usbd_adb_send_internal(A_OKAY, localid, remoteid, NULL, 0);
}

static void adb_reply_close(uint32_t localid, uint32_t remoteid)
{
    usbd_adb_send_internal(A_CLSE, localid, remoteid, NULL, 0);
}

static void adb_handle_cnxn(struct adb_packet *pkt)
{
    static const char support_feature[] =
        "device::"
        "ro.product.name=cherryadb;"
        "ro.product.model=cherrysh;"
        "ro.product.device=cherryadb;"
        "features=cmd,shell_v1";

    (void)pkt;

    usbd_adb_send_internal(A_CNXN, ADB_VERSION, ADB_MAX_PAYLOAD,
                           (const uint8_t *)support_feature, strlen(support_feature));
}

static void adb_handle_open(struct adb_packet *pkt)
{
    uint32_t remoteid = pkt->msg.arg0;
    char *dest = (char *)pkt->payload;
    uint32_t i;

    if ((pkt->msg.data_length == 0) || (pkt->msg.data_length >= ADB_MAX_PAYLOAD)) {
        adb_reply_close(0, remoteid);
        return;
    }

    /* the destination string is not guaranteed to be NUL terminated */
    dest[pkt->msg.data_length] = '\0';

    for (i = 0; i < ADB_MAX_SERVICE; i++) {
        const struct adb_service *svc = g_usbd_adb.svc_tab[i];
        size_t name_len;

        if (!svc) {
            continue;
        }

        name_len = strlen(svc->name);
        if (strncmp(dest, svc->name, name_len) != 0) {
            continue;
        }

        g_usbd_adb.remote_id[i] = remoteid;
        g_usbd_adb.svc_opened[i] = true;

        if (svc->on_open) {
            svc->on_open(remoteid);
        }

        adb_reply_okay(svc->localid, remoteid);
        return;
    }

    USB_LOG_WRN("unsupported destination '%s'\r\n", dest);
    adb_reply_close(0, remoteid);
}

static void adb_handle_write(struct adb_packet *pkt)
{
    uint32_t remoteid = pkt->msg.arg0;
    uint32_t localid = pkt->msg.arg1;
    int slot = adb_svc_slot(localid);

    if ((slot < 0) || !g_usbd_adb.svc_opened[slot] || (g_usbd_adb.remote_id[slot] != remoteid)) {
        adb_reply_close(0, remoteid);
        return;
    }

    if (g_usbd_adb.svc_tab[slot]->on_write) {
        g_usbd_adb.svc_tab[slot]->on_write(remoteid, pkt->payload, pkt->msg.data_length);
    }

    adb_reply_okay(localid, remoteid);
}

static void adb_handle_close(struct adb_packet *pkt)
{
    uint32_t remoteid = pkt->msg.arg0;
    uint32_t localid = pkt->msg.arg1;
    int slot = adb_svc_slot(localid);

    if (slot < 0) {
        return;
    }

    g_usbd_adb.svc_opened[slot] = false;

    if (g_usbd_adb.svc_tab[slot]->on_close) {
        g_usbd_adb.svc_tab[slot]->on_close(remoteid);
    }

    g_usbd_adb.remote_id[slot] = 0;
}

static void adb_dispatch(struct adb_packet *pkt)
{
    switch (pkt->msg.command) {
        case A_CNXN:
            adb_handle_cnxn(pkt);
            break;
        case A_OPEN:
            adb_handle_open(pkt);
            break;
        case A_WRTE:
            adb_handle_write(pkt);
            break;
        case A_CLSE:
            adb_handle_close(pkt);
            break;
        case A_OKAY:
            /* the host acknowledged one of our writes, nothing to answer */
            break;
        case A_SYNC:
        case A_AUTH:
            break;
        default:
            USB_LOG_WRN("unknown command 0x%08x\r\n", (unsigned)pkt->msg.command);
            break;
    }
}

static void adb_handle_packet(struct adb_packet *pkt)
{
    if (pkt->msg.magic != (pkt->msg.command ^ 0xffffffffu)) {
        USB_LOG_ERR("bad magic 0x%08x for command 0x%08x\r\n",
                    (unsigned)pkt->msg.magic, (unsigned)pkt->msg.command);
        return;
    }

    adb_dispatch(pkt);
}

static void adb_rx_thread_entry(CONFIG_USB_OSAL_THREAD_SET_ARGV)
{
    (void)argument;

    while (1) {
        if (usb_osal_sem_take(g_usbd_adb.rx_sem, USB_OSAL_WAITING_FOREVER) != 0) {
            continue;
        }

        adb_handle_packet(&adb_rx_packet);

        /* the packet has been consumed, arm the OUT endpoint again */
        g_usbd_adb.rx_state = ADB_RX_STATE_READ_MSG;
        usbd_ep_start_read(g_usbd_adb.busid, g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr,
                           (uint8_t *)&adb_rx_packet.msg, sizeof(struct adb_msg));
    }
}

void usbd_adb_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;

    if (nbytes == 0) {
        g_usbd_adb.rx_state = ADB_RX_STATE_READ_MSG;
        usbd_ep_start_read(busid, g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr, (uint8_t *)&adb_rx_packet.msg,
                           sizeof(struct adb_msg));
        return;
    }

    if (g_usbd_adb.rx_state == ADB_RX_STATE_READ_MSG) {
        if (nbytes != sizeof(struct adb_msg)) {
            USB_LOG_ERR("short header %u bytes\r\n", (unsigned)nbytes);
            g_usbd_adb.rx_state = ADB_RX_STATE_READ_MSG;
            usbd_ep_start_read(busid, g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr, (uint8_t *)&adb_rx_packet.msg,
                               sizeof(struct adb_msg));
            return;
        }

        if (adb_rx_packet.msg.data_length > ADB_MAX_PAYLOAD) {
            USB_LOG_ERR("header length %u too large\r\n",
                        (unsigned)adb_rx_packet.msg.data_length);
            g_usbd_adb.rx_state = ADB_RX_STATE_READ_MSG;
            usbd_ep_start_read(busid, g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr, (uint8_t *)&adb_rx_packet.msg,
                               sizeof(struct adb_msg));
            return;
        }

        if (adb_rx_packet.msg.data_length) {
            USB_LOG_DBG("rx cmd 0x%08x arg0 0x%x arg1 0x%x len %u\r\n",
                        (unsigned)adb_rx_packet.msg.command,
                        (unsigned)adb_rx_packet.msg.arg0,
                        (unsigned)adb_rx_packet.msg.arg1,
                        (unsigned)adb_rx_packet.msg.data_length);

            g_usbd_adb.rx_state = ADB_RX_STATE_READ_DATA;
            usbd_ep_start_read(busid, g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr, adb_rx_packet.payload,
                               adb_rx_packet.msg.data_length);
        } else {
            /* no payload -> the packet is already complete */
            g_usbd_adb.rx_state = ADB_RX_STATE_COMPLETE;
            usb_osal_sem_give(g_usbd_adb.rx_sem);
        }
    } else if (g_usbd_adb.rx_state == ADB_RX_STATE_READ_DATA) {
        /* payload complete -> the packet is complete */
        g_usbd_adb.rx_state = ADB_RX_STATE_COMPLETE;
        usb_osal_sem_give(g_usbd_adb.rx_sem);
    } else {
    }
}

void usbd_adb_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)ep;
    (void)nbytes;

    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(busid, g_usbd_adb.ep_data[ADB_IN_EP_IDX].ep_addr, NULL, 0);
        return;
    }

    if (g_usbd_adb.tx_state == ADB_TX_STATE_MSG) {
        if (adb_tx_packet.msg.data_length) {
            g_usbd_adb.tx_state = ADB_TX_STATE_DATA;
            usbd_ep_start_write(busid, g_usbd_adb.ep_data[ADB_IN_EP_IDX].ep_addr,
                                adb_tx_packet.payload, adb_tx_packet.msg.data_length);
        } else {
            g_usbd_adb.tx_state = ADB_TX_STATE_COMPLETE;
            usb_osal_sem_give(g_usbd_adb.tx_sem);
        }
    } else if (g_usbd_adb.tx_state == ADB_TX_STATE_DATA) {
        g_usbd_adb.tx_state = ADB_TX_STATE_COMPLETE;
        usb_osal_sem_give(g_usbd_adb.tx_sem);
    }
}

static void adb_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    (void)arg;

    switch (event) {
        case USBD_EVENT_CONFIGURED:
            g_usbd_adb.rx_state = ADB_RX_STATE_READ_MSG;
            g_usbd_adb.tx_state = ADB_TX_STATE_COMPLETE;
            usbd_ep_start_read(busid, g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr,
                               (uint8_t *)&adb_rx_packet.msg, sizeof(struct adb_msg));
            break;

        case USBD_EVENT_RESET:
            for (uint32_t i = 0; i < ADB_MAX_SERVICE; i++) {
                g_usbd_adb.svc_opened[i] = false;
                g_usbd_adb.remote_id[i] = 0;
            }
            break;

        case USBD_EVENT_INIT:
            break;
        case USBD_EVENT_DEINIT:
            break;
        default:
            break;
    }
}

/* -------------------------------------------------------------------------
 * initialisation
 * ---------------------------------------------------------------------- */
struct usbd_interface *usbd_adb_init_intf(uint8_t busid, struct usbd_interface *intf,
                                          uint8_t in_ep, uint8_t out_ep)
{
    g_usbd_adb.busid = busid;

    g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_addr = out_ep;
    g_usbd_adb.ep_data[ADB_OUT_EP_IDX].ep_cb = usbd_adb_bulk_out;
    g_usbd_adb.ep_data[ADB_IN_EP_IDX].ep_addr = in_ep;
    g_usbd_adb.ep_data[ADB_IN_EP_IDX].ep_cb = usbd_adb_bulk_in;

    usbd_add_endpoint(busid, &g_usbd_adb.ep_data[ADB_OUT_EP_IDX]);
    usbd_add_endpoint(busid, &g_usbd_adb.ep_data[ADB_IN_EP_IDX]);

    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = adb_notify_handler;

    g_usbd_adb.rx_sem = usb_osal_sem_create(0);
    USB_ASSERT_MSG(g_usbd_adb.rx_sem, "adb rx_sem fail");
    g_usbd_adb.tx_sem = usb_osal_sem_create(0);
    USB_ASSERT_MSG(g_usbd_adb.tx_sem, "adb tx_sem fail");
    g_usbd_adb.tx_lock = usb_osal_mutex_create();
    USB_ASSERT_MSG(g_usbd_adb.tx_lock, "adb tx_lock fail");

    g_usbd_adb.rx_thread = usb_osal_thread_create("usbd_adb", ADB_DAEMON_THREAD_STACK,
                                                  ADB_DAEMON_THREAD_PRIO, adb_rx_thread_entry, NULL);
    USB_ASSERT_MSG(g_usbd_adb.rx_thread, "adb rx_thread fail");
    usbd_adb_shell_init();
    usbd_adb_sync_init();

    return intf;
}

int usbd_adb_write(uint32_t localid,
                   const uint8_t *data,
                   uint32_t len)
{
    uint32_t sent = 0;

    if (usbd_adb_service_is_opened(localid) == false) {
        return -1;
    }

    uint32_t remoteid = usbd_adb_service_get_remoteid(localid);
    if (!remoteid) {
        return -1;
    }

    if (!len) {
        return 0;
    }

    while (sent < len) {
        uint32_t chunk = len - sent;
        int ret;

        if (chunk > ADB_MAX_PAYLOAD) {
            chunk = ADB_MAX_PAYLOAD;
        }

        ret = usbd_adb_send_internal(A_WRTE,
                                     localid,
                                     remoteid,
                                     data + sent,
                                     chunk);
        if (ret != 0) {
            return ret;
        }

        sent += chunk;
    }

    return 0;
}

void usbd_adb_close(uint32_t localid)
{
    if (usbd_adb_service_is_opened(localid) == false) {
        return;
    }

    uint32_t remoteid = usbd_adb_service_get_remoteid(localid);
    if (!remoteid) {
        return;
    }

    usbd_adb_send_internal(A_CLSE, localid, remoteid, NULL, 0);
}

__WEAK void usbd_adb_shell_init(void)
{
}

__WEAK void usbd_adb_sync_init(void)
{
}