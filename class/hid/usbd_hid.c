/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_hid.h"

static int hid_custom_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("HID Custom request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    if (((setup->bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_IN) &&
        setup->bRequest == USB_REQUEST_GET_DESCRIPTOR) {
        uint8_t value = (uint8_t)(setup->wValue >> 8);
        uint8_t intf_num = (uint8_t)setup->wIndex;

        usb_slist_t *i;
        struct usbd_interface *match_intf = NULL;

        usb_slist_for_each(i, &usbd_intf_head)
        {
            struct usbd_interface *intf = usb_slist_entry(i, struct usbd_interface, list);

            if (intf->intf_num == intf_num) {
                match_intf = intf;
                break;
            }
        }

        if (match_intf == NULL) {
            return -2;
        }

        switch (value) {
            case HID_DESCRIPTOR_TYPE_HID:
                USB_LOG_INFO("get HID Descriptor\r\n");
                // *data = (uint8_t *)match_intf->hid_descriptor;
                // *len = match_intf->hid_descriptor[0];
                break;

            case HID_DESCRIPTOR_TYPE_HID_REPORT:
                USB_LOG_INFO("get Report Descriptor\r\n");
                *data = (uint8_t *)match_intf->hid_report_descriptor;
                *len = match_intf->hid_report_descriptor_len;
                break;

            case HID_DESCRIPTOR_TYPE_HID_PHYSICAL:
                USB_LOG_INFO("get PHYSICAL Descriptor\r\n");

                break;

            default:
                return -2;
        }

        return 0;
    }

    return -1;
}

static int hid_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("HID Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    uint8_t intf_num = LO_BYTE(setup->wIndex);

    switch (setup->bRequest) {
        case HID_REQUEST_GET_REPORT:
            /* report id ,report type */
            (*data)[0] = usbh_hid_get_report(intf_num, LO_BYTE(setup->wValue), HI_BYTE(setup->wValue));
            *len = 1;
            break;
        case HID_REQUEST_GET_IDLE:
            (*data)[0] = usbh_hid_get_idle(intf_num, LO_BYTE(setup->wValue));
            *len = 1;
            break;
        case HID_REQUEST_GET_PROTOCOL:
            (*data)[0] = usbh_hid_get_protocol(intf_num);
            *len = 1;
            break;
        case HID_REQUEST_SET_REPORT:
            /* report id ,report type, report, report len */
            usbh_hid_set_report(intf_num, LO_BYTE(setup->wValue), HI_BYTE(setup->wValue), *data, *len);
            break;
        case HID_REQUEST_SET_IDLE:
            /* report id, duration */
            usbh_hid_set_idle(intf_num, LO_BYTE(setup->wValue), HI_BYTE(setup->wIndex));
            break;
        case HID_REQUEST_SET_PROTOCOL:
            /* protocol */
            usbh_hid_set_protocol(intf_num, LO_BYTE(setup->wValue));
            break;

        default:
            USB_LOG_WRN("Unhandled HID Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

struct usbd_interface *usbd_hid_alloc_intf(const uint8_t *desc, uint32_t desc_len)
{
    struct usbd_interface *intf = usb_malloc(sizeof(struct usbd_interface));
    if (intf == NULL) {
        USB_LOG_ERR("no mem to alloc intf\r\n");
        return NULL;
    }

    intf->class_interface_handler = hid_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = NULL;

    intf->hid_report_descriptor = desc;
    intf->hid_report_descriptor_len = desc_len;
    return intf;
}

__WEAK uint8_t usbh_hid_get_report(uint8_t intf, uint8_t report_id, uint8_t report_type)
{
    return 0;
}

__WEAK uint8_t usbh_hid_get_idle(uint8_t intf, uint8_t report_id)
{
    return 0;
}

__WEAK uint8_t usbh_hid_get_protocol(uint8_t intf)
{
    return 0;
}

__WEAK void usbh_hid_set_report(uint8_t intf, uint8_t report_id, uint8_t report_type, uint8_t *report, uint8_t report_len)
{
}

__WEAK void usbh_hid_set_idle(uint8_t intf, uint8_t report_id, uint8_t duration)
{
}

__WEAK void usbh_hid_set_protocol(uint8_t intf, uint8_t protocol)
{
}