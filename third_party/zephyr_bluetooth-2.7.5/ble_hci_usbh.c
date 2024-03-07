#include "usbh_core.h"
#include "usbh_bluetooth.h"

#include <zephyr.h>

/* compatible with low version that less than v2.7.5 */

/* @brief The HCI event shall be given to bt_recv_prio */
#define __BT_HCI_EVT_FLAG_RECV_PRIO BIT(0)
/* @brief  The HCI event shall be given to bt_recv. */
#define __BT_HCI_EVT_FLAG_RECV      BIT(1)

/** @brief Get HCI event flags.
 *
 * Helper for the HCI driver to get HCI event flags that describes rules that.
 * must be followed.
 *
 * When CONFIG_BT_RECV_IS_RX_THREAD is enabled the flags
 * __BT_HCI_EVT_FLAG_RECV and __BT_HCI_EVT_FLAG_RECV_PRIO indicates if the event
 * should be given to bt_recv or bt_recv_prio.
 *
 * @param evt HCI event code.
 *
 * @return HCI event flags for the specified event.
 */
static inline uint8_t __bt_hci_evt_get_flags(uint8_t evt)
{
    switch (evt) {
        case BT_HCI_EVT_DISCONN_COMPLETE:
            return __BT_HCI_EVT_FLAG_RECV | __BT_HCI_EVT_FLAG_RECV_PRIO;
            /* fallthrough */
#if defined(CONFIG_BT_CONN) || defined(CONFIG_BT_ISO)
        case BT_HCI_EVT_NUM_COMPLETED_PACKETS:
#if defined(CONFIG_BT_CONN)
        case BT_HCI_EVT_DATA_BUF_OVERFLOW:
#endif /* defined(CONFIG_BT_CONN) */
#endif /* CONFIG_BT_CONN ||  CONFIG_BT_ISO */
        case BT_HCI_EVT_CMD_COMPLETE:
        case BT_HCI_EVT_CMD_STATUS:
            return __BT_HCI_EVT_FLAG_RECV_PRIO;
        default:
            return __BT_HCI_EVT_FLAG_RECV;
    }
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

static int usbh_hci_host_rcv_pkt(uint8_t *data, uint32_t len)
{
    struct net_buf *buf = NULL;
    size_t remaining = len;
    uint8_t pkt_indicator;

    pkt_indicator = *data++;
    remaining -= sizeof(pkt_indicator);

    switch (pkt_indicator) {
        case USB_BLUETOOTH_HCI_EVT:
            buf = usbh_bt_evt_recv(data, remaining);
            break;

        case USB_BLUETOOTH_HCI_ACL:
            buf = usbh_bt_acl_recv(data, remaining);
            break;

        case USB_BLUETOOTH_HCI_SCO:
            buf = usbh_bt_iso_recv(data, remaining);
            break;

        default:
            USB_LOG_ERR("Unknown HCI type %u\r\n", pkt_indicator);
            return -1;
    }

    if (buf) {
#ifdef CONFIG_BT_RECV_IS_RX_THREAD
        if (pkt_indicator == USB_BLUETOOTH_HCI_EVT) {
            struct bt_hci_evt_hdr *hdr = (void *)buf->data;
            uint8_t evt_flags = __bt_hci_evt_get_flags(hdr->evt);

            if (evt_flags & __BT_HCI_EVT_FLAG_RECV_PRIO) {
                bt_recv_prio(buf);
            } else {
                bt_recv(buf);
            }
        } else {
            bt_recv(buf);
        }
#else
        bt_recv(buf);
#endif
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
    uint8_t pkt_indicator;

    switch (bt_buf_get_type(buf)) {
        case BT_BUF_ACL_OUT:
            pkt_indicator = USB_BLUETOOTH_HCI_ACL;
            break;
        case BT_BUF_CMD:
            pkt_indicator = USB_BLUETOOTH_HCI_CMD;
            break;
        case BT_BUF_ISO_OUT:
            pkt_indicator = USB_BLUETOOTH_HCI_ISO;
            break;
        default:
            USB_LOG_ERR("Unknown type %u", bt_buf_get_type(buf));
            goto done;
    }

    err = usbh_bluetooth_hci_write(pkt_indicator, buf->data, buf->len);
    if (err < 0) {
        err = 255;
    } else {
        err = 0;
    }
done:
    net_buf_unref(buf);

    return err;
}

static const struct bt_hci_driver usbh_drv = {
    .name = "hci_usb",
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

#ifdef CONFIG_USBHOST_BLUETOOTH_HCI_H4
    usb_osal_thread_create("ble_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_bluetooth_hci_rx_thread, NULL);
#else
    usb_osal_thread_create("ble_evt", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_bluetooth_hci_evt_rx_thread, NULL);
    usb_osal_thread_create("ble_acl", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_bluetooth_hci_acl_rx_thread, NULL);
#endif
    usbh_bluetooth_run_callback();
}

void usbh_bluetooth_stop(struct usbh_bluetooth *bluetooth_class)
{
    usbh_bluetooth_stop_callback();
}

void usbh_bluetooth_hci_read_callback(uint8_t *data, uint32_t len)
{
    usbh_hci_host_rcv_pkt(data, len);
}