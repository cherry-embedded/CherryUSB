/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbh_core.h"
#include "usbh_bluetooth.h"

#include <nimble/ble.h>
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
    size_t remaining = len;
    uint8_t pkt_indicator;

    pkt_indicator = *data++;
    remaining -= sizeof(pkt_indicator);

    hci_h4_sm_rx(&g_hci_h4sm, &pkt_indicator, 1);
    hci_h4_sm_rx(&g_hci_h4sm, data, remaining);
}

int ble_transport_to_ll_cmd_impl(void *buf)
{
    int ret = 0;
    uint8_t *cmd_pkt_data = (uint8_t *)buf;
    size_t pkt_len = cmd_pkt_data[2] + 3;

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
    struct os_mbuf *x = om;
    int ret = 0;

    while (x != NULL) {
        ret = usbh_bluetooth_hci_write(USB_BLUETOOTH_HCI_ACL, x->om_data, x->om_len);
        if (ret < 0) {
            ret = BLE_ERR_MEM_CAPACITY;
            break;
        } else {
            ret = 0;
        }
        x = SLIST_NEXT(x, om_next);
    }

    os_mbuf_free_chain(om);

    return ret;
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
    usbh_bluetooth_stop_callback();
}
