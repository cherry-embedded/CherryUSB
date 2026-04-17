/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbh_core.h"
#include "usbh_bluetooth.h"

#include <nimble/ble.h>
#include "nimble/hci_common.h"
#include "nimble/transport.h"
#include "nimble/transport/hci_h4.h"

struct hci_h4_sm g_hci_h4sm;

void ble_transport_ll_init(void)
{
    /* nothing here */
}

static int hci_usb_frame_cb(uint8_t pkt_type, void *data)
{
    switch (pkt_type) {
        case HCI_H4_EVT:
            return ble_transport_to_hs_evt(data);
        case HCI_H4_ACL:
            return ble_transport_to_hs_acl(data);
        default:
            assert(0);
            break;
    }
    return -1;
}

void usbh_bluetooth_hci_read_callback(uint8_t *data, uint32_t len)
{
    hci_h4_sm_rx(&g_hci_h4sm, data, (uint16_t)len);
}

int ble_transport_to_ll_cmd_impl(void *buf)
{
    const struct ble_hci_cmd *cmd = buf;
    size_t pkt_len = sizeof(*cmd) + cmd->length;
    int ret;

    ret = usbh_bluetooth_hci_write(USB_BLUETOOTH_HCI_CMD, buf, pkt_len);
    if (ret < 0) {
        ret = BLE_ERR_MEM_CAPACITY;
    } else {
        ret = 0;
    }
    ble_transport_free(buf);

    return ret;
}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
    uint8_t buf[BLE_HCI_DATA_HDR_SZ + MYNEWT_VAL(BLE_TRANSPORT_ACL_SIZE)];
    uint16_t pkt_len = OS_MBUF_PKTLEN(om);
    int ret;

    if (pkt_len > sizeof(buf)) {
        os_mbuf_free_chain(om);
        return BLE_ERR_MEM_CAPACITY;
    }

    os_mbuf_copydata(om, 0, pkt_len, buf);
    ret = usbh_bluetooth_hci_write(USB_BLUETOOTH_HCI_ACL, buf, pkt_len);
    os_mbuf_free_chain(om);
    return ret < 0 ? BLE_ERR_MEM_CAPACITY : 0;
}

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
    (void)bluetooth_class;
    hci_h4_sm_init(&g_hci_h4sm, &hci_h4_allocs_from_ll, hci_usb_frame_cb);

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
    (void)bluetooth_class;
    usbh_bluetooth_stop_callback();
}
