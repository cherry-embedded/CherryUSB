/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbh_core.h"
#include "usbh_bluetooth.h"

#include "byteorder.h"
#include "hci_host.h"
#include "hci_driver.h"

static void hci_dump(uint8_t hci_type, uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    USB_LOG_DBG("hci type:%u\r\n", hci_type);

    for (i = 0; i < len; i++) {
        if (i % 16 == 0) {
            USB_LOG_DBG("\r\n");
        }

        USB_LOG_DBG("%02x ", data[i]);
    }

    USB_LOG_DBG("\r\n");
}

static bool is_hci_event_discardable(const uint8_t *evt_data)
{
    uint8_t evt_type = evt_data[0];

    switch (evt_type) {
#if defined(CONFIG_BT_BREDR)
        case BT_HCI_EVT_INQUIRY_RESULT_WITH_RSSI:
        case BT_HCI_EVT_EXTENDED_INQUIRY_RESULT:
            return true;
#endif
        case BT_HCI_EVT_LE_META_EVENT: {
            uint8_t subevt_type = evt_data[sizeof(struct bt_hci_evt_hdr)];

            switch (subevt_type) {
                case BT_HCI_EVT_LE_ADVERTISING_REPORT:
                    return true;
                default:
                    return false;
            }
        }
        default:
            return false;
    }
}

static struct net_buf *usbh_bt_evt_recv(uint8_t *data, size_t remaining)
{
    bool discardable = false;
    struct bt_hci_evt_hdr hdr;
    struct net_buf *buf;
    size_t buf_tailroom;

    if (remaining < sizeof(hdr)) {
        USB_LOG_ERR("Not enough data for event header\r\n");
        return NULL;
    }

    discardable = is_hci_event_discardable(data);

    memcpy((void *)&hdr, data, sizeof(hdr));
    data += sizeof(hdr);
    remaining -= sizeof(hdr);

    if (remaining != hdr.len) {
        USB_LOG_ERR("Event payload length is not correct\r\n");
        return NULL;
    }

    buf = bt_buf_get_evt(hdr.evt, discardable, K_NO_WAIT);
    if (!buf) {
        if (discardable) {
            USB_LOG_DBG("Discardable buffer pool full, ignoring event\r\n");
        } else {
            USB_LOG_ERR("No available event buffers!\r\n");
        }
        return buf;
    }

    net_buf_add_mem(buf, &hdr, sizeof(hdr));

    buf_tailroom = net_buf_tailroom(buf);
    if (buf_tailroom < remaining) {
        USB_LOG_ERR("Not enough space in buffer %zu/%zu\r\n", remaining, buf_tailroom);
        net_buf_unref(buf);
        return NULL;
    }

    net_buf_add_mem(buf, data, remaining);

    return buf;
}

static struct net_buf *usbh_bt_acl_recv(uint8_t *data, size_t remaining)
{
    struct bt_hci_acl_hdr hdr;
    struct net_buf *buf;
    size_t buf_tailroom;

    if (remaining < sizeof(hdr)) {
        USB_LOG_ERR("Not enough data for ACL header\r\n");
        return NULL;
    }

    buf = bt_buf_get_rx(BT_BUF_ACL_IN, K_NO_WAIT);
    if (buf) {
        memcpy((void *)&hdr, data, sizeof(hdr));
        data += sizeof(hdr);
        remaining -= sizeof(hdr);

        net_buf_add_mem(buf, &hdr, sizeof(hdr));
    } else {
        USB_LOG_ERR("No available ACL buffers!\r\n");
        return NULL;
    }

    if (remaining != sys_le16_to_cpu(hdr.len)) {
        USB_LOG_ERR("ACL payload length is not correct\r\n");
        net_buf_unref(buf);
        return NULL;
    }

    buf_tailroom = net_buf_tailroom(buf);
    if (buf_tailroom < remaining) {
        USB_LOG_ERR("Not enough space in buffer %zu/%zu\r\n", remaining, buf_tailroom);
        net_buf_unref(buf);
        return NULL;
    }

    USB_LOG_DBG("len %u", remaining);
    net_buf_add_mem(buf, data, remaining);

    return buf;
}

static struct net_buf *usbh_bt_iso_recv(uint8_t *data, size_t remaining)
{
    struct bt_hci_iso_hdr hdr;
    struct net_buf *buf;
    size_t buf_tailroom;

    if (remaining < sizeof(hdr)) {
        USB_LOG_ERR("Not enough data for ISO header\r\n");
        return NULL;
    }

    buf = bt_buf_get_rx(BT_BUF_ISO_IN, K_NO_WAIT);
    if (buf) {
        memcpy((void *)&hdr, data, sizeof(hdr));
        data += sizeof(hdr);
        remaining -= sizeof(hdr);

        net_buf_add_mem(buf, &hdr, sizeof(hdr));
    } else {
        USB_LOG_ERR("No available ISO buffers!\r\n");
        return NULL;
    }

    // if (remaining != bt_iso_hdr_len(sys_le16_to_cpu(hdr.len))) {
    //     USB_LOG_ERR("ISO payload length is not correct\r\n");
    //     net_buf_unref(buf);
    //     return NULL;
    // }

    buf_tailroom = net_buf_tailroom(buf);
    if (buf_tailroom < remaining) {
        USB_LOG_ERR("Not enough space in buffer %zu/%zu\r\n", remaining, buf_tailroom);
        net_buf_unref(buf);
        return NULL;
    }

    USB_LOG_DBG("len %zu", remaining);
    net_buf_add_mem(buf, data, remaining);

    return buf;
}

static int usbh_hci_host_rcv_pkt(uint8_t pkt_indicator, uint8_t *data, uint16_t len)
{
    struct net_buf *buf = NULL;
    size_t remaining = len;
    bool prio = true;

    switch (pkt_indicator) {
        case USB_BLUETOOTH_HCI_EVT:
            buf = usbh_bt_evt_recv(data, remaining);
            break;

        case USB_BLUETOOTH_HCI_ACL_IN:
            buf = usbh_bt_acl_recv(data, remaining);
            prio = false;
            break;

        case USB_BLUETOOTH_HCI_ISO_IN:
            buf = usbh_bt_iso_recv(data, remaining);
            break;

        default:
            USB_LOG_ERR("Unknown HCI type %u\r\n", pkt_indicator);
            return -1;
    }

    hci_dump(pkt_indicator, buf->data, buf->len);

    if (buf) {
        bt_recv(buf);
    }

    return 0;
}

static int bt_usbh_open(void)
{
    return 0;
}

static int bt_usbh_send(struct net_buf *buf)
{
    int err = 0;
    uint8_t pkt_indicator = bt_buf_get_type(buf);

    hci_dump(pkt_indicator, buf->data, buf->len);

    switch (pkt_indicator) {
        case BT_BUF_ACL_OUT:
            usbh_bluetooth_hci_acl_out(buf->data, buf->len);
            break;
        case BT_BUF_CMD:
            usbh_bluetooth_hci_cmd(buf->data, buf->len);
            break;
        case BT_BUF_ISO_OUT:
            break;
        default:
            USB_LOG_ERR("Unknown type %u\r\n", pkt_indicator);
            goto done;
    }
done:
    net_buf_unref(buf);

    return err;
}

static const struct bt_hci_driver usbh_drv = {
    .name = "usbhost btble",
    .open = bt_usbh_open,
    .send = bt_usbh_send,
    .bus = BT_HCI_DRIVER_BUS_USB,
#if defined(CONFIG_BT_DRIVER_QUIRK_NO_AUTO_DLE)
    .quirks = BT_QUIRK_NO_AUTO_DLE,
#endif
};

__WEAK void usbh_bluetooth_run_callback(void)
{
    /* bt_enable() */
}

__WEAK void usbh_bluetooth_stop_callback(void)
{
    /* bt_disable() */
}

void usbh_bluetooth_run(struct usbh_bluetooth *bluetooth_class)
{
    bt_hci_driver_register(&usbh_drv);

    usb_osal_thread_create("ble_event", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_bluetooth_hci_event_rx_thread, NULL);
    usb_osal_thread_create("ble_acl", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_bluetooth_hci_acl_rx_thread, NULL);

    usbh_bluetooth_run_callback();
}

void usbh_bluetooth_stop(struct usbh_bluetooth *bluetooth_class)
{
    usbh_bluetooth_stop_callback();
}

void usbh_bluetooth_hci_rx_callback(uint8_t hci_type, uint8_t *data, uint32_t len)
{
    usbh_hci_host_rcv_pkt(hci_type, data, len);
}