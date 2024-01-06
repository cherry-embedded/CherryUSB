/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_BLUETOOTH_H
#define USBH_BLUETOOTH_H

#define USB_BLUETOOTH_HCI_CMD 1
#define USB_BLUETOOTH_HCI_EVT 2
#define USB_BLUETOOTH_HCI_ACL 4
#define USB_BLUETOOTH_HCI_ISO 5

struct usbh_bluetooth {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usb_endpoint_descriptor *intin;   /* INTR endpoint */
    struct usb_endpoint_descriptor *isoin;   /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *isoout;  /* Bulk OUT endpoint */
    struct usbh_urb bulkin_urb;              /* Bulk IN urb */
    struct usbh_urb bulkout_urb;             /* Bulk OUT urb */
    struct usbh_urb intin_urb;               /* INTR IN urb */
    struct usbh_urb *isoin_urb;              /* Bulk IN urb */
    struct usbh_urb *isoout_urb;             /* Bulk OUT urb */
    uint8_t intf;
    uint8_t num_of_intf_altsettings;
};

#ifdef __cplusplus
extern "C" {
#endif

void usbh_bluetooth_run(struct usbh_bluetooth *bluetooth_class);
void usbh_bluetooth_stop(struct usbh_bluetooth *bluetooth_class);

/* OpCode(OCF+OGF:2bytes) + ParamLength + Paramas */
int usbh_bluetooth_hci_cmd(struct usbh_bluetooth *bluetooth_class, uint8_t *buffer, uint32_t buflen);
/* Handle (12bits) + Packet_Boundary_Flag(2bits) + BC_flag(2bits) + data_len(2bytes) + data */
int usbh_bluetooth_hci_acl_out(struct usbh_bluetooth *bluetooth_class, uint8_t *buffer, uint32_t buflen);
void usbh_bluetooth_hci_event_rx_thread(void *argument);
void usbh_bluetooth_hci_acl_rx_thread(void *argument);

/*  USB_BLUETOOTH_HCI_EVT : EventCode(1byte) + ParamLength + Parama0 + Parama1 + Parama2;
    USB_BLUETOOTH_HCI_ACL : Handle (12bits) + Packet_Boundary_Flag(2bits) + BC_flag(2bits) + data_len(2bytes) + data
*/
void usbh_bluetooth_hci_rx_callback(uint8_t hci_type, uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* USBH_BLUETOOTH_H */
