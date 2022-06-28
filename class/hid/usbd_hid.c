/**
 * @file usbd_hid.c
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#include "usbd_core.h"
#include "usbd_hid.h"

#define HID_STATE_IDLE 0
#define HID_STATE_BUSY 1

struct usbd_hid {
    const uint8_t *hid_descriptor;
    const uint8_t *hid_report_descriptor;
    uint32_t hid_report_descriptor_len;
    uint8_t intf_num;
    uint8_t hid_state;
    uint8_t report;
    uint8_t idle_state;
    uint8_t protocol;

    usb_slist_t list;
};

static usb_slist_t usbd_hid_head = USB_SLIST_OBJECT_INIT(usbd_hid_head);

static void usbd_hid_reset(void)
{
    usb_slist_t *i;
    usb_slist_for_each(i, &usbd_hid_head)
    {
        struct usbd_hid *hid_intf = usb_slist_entry(i, struct usbd_hid, list);
        hid_intf->hid_state = HID_STATE_IDLE;
        hid_intf->report = 0;
        hid_intf->idle_state = 0;
        hid_intf->protocol = 0;
    }
}

static int hid_custom_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("HID Custom request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    if (((setup->bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_IN) &&
        setup->bRequest == USB_REQUEST_GET_DESCRIPTOR) {
        uint8_t value = (uint8_t)(setup->wValue >> 8);
        uint8_t intf_num = (uint8_t)setup->wIndex;

        struct usbd_hid *current_hid_class = NULL;
        usb_slist_t *i;
        usb_slist_for_each(i, &usbd_hid_head)
        {
            struct usbd_hid *hid_class = usb_slist_entry(i, struct usbd_hid, list);

            if (hid_class->intf_num == intf_num) {
                current_hid_class = hid_class;
                break;
            }
        }

        if (current_hid_class == NULL) {
            return -2;
        }

        switch (value) {
            case HID_DESCRIPTOR_TYPE_HID:
                USB_LOG_INFO("get HID Descriptor\r\n");
                *data = (uint8_t *)current_hid_class->hid_descriptor;
                *len = current_hid_class->hid_descriptor[0];
                break;

            case HID_DESCRIPTOR_TYPE_HID_REPORT:
                USB_LOG_INFO("get Report Descriptor\r\n");
                *data = (uint8_t *)current_hid_class->hid_report_descriptor;
                *len = current_hid_class->hid_report_descriptor_len;
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

static int hid_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("HID Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    struct usbd_hid *current_hid_class = NULL;
    usb_slist_t *i;
    uint8_t intf = LO_BYTE(setup->wIndex);

    usb_slist_for_each(i, &usbd_hid_head)
    {
        struct usbd_hid *hid_class = usb_slist_entry(i, struct usbd_hid, list);
        if (hid_class->intf_num == intf) {
            current_hid_class = hid_class;
            break;
        }
    }

    if (current_hid_class == NULL) {
        return -2;
    }

    switch (setup->bRequest) {
        case HID_REQUEST_GET_REPORT:
            current_hid_class->report = usbh_hid_get_report(intf, LO_BYTE(setup->wValue), HI_BYTE(setup->wValue)); /*report id ,report type*/
            *data = (uint8_t *)&current_hid_class->report;
            *len = 1;
            break;
        case HID_REQUEST_GET_IDLE:
            current_hid_class->idle_state = usbh_hid_get_idle(intf, LO_BYTE(setup->wValue));
            *data = (uint8_t *)&current_hid_class->idle_state;
            *len = 1;
            break;
        case HID_REQUEST_GET_PROTOCOL:
            current_hid_class->protocol = usbh_hid_get_protocol(intf);
            *data = (uint8_t *)&current_hid_class->protocol;
            *len = 1;
            break;
        case HID_REQUEST_SET_REPORT:
            usbh_hid_set_report(intf, LO_BYTE(setup->wValue), HI_BYTE(setup->wValue), *data, *len); /*report id ,report type,report,report len*/
            current_hid_class->report = **data;
            break;
        case HID_REQUEST_SET_IDLE:
            usbh_hid_set_idle(intf, LO_BYTE(setup->wValue), HI_BYTE(setup->wIndex)); /*report id ,duration*/
            current_hid_class->idle_state = HI_BYTE(setup->wIndex);
            break;
        case HID_REQUEST_SET_PROTOCOL:
            usbh_hid_set_protocol(intf, LO_BYTE(setup->wValue)); /*protocol*/
            current_hid_class->protocol = LO_BYTE(setup->wValue);
            break;

        default:
            USB_LOG_WRN("Unhandled HID Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void hid_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            usbd_hid_reset();
            break;

        default:
            break;
    }
}

int usbd_hid_alloc(uint8_t intf)
{
    struct usbd_hid *hid_class = usb_malloc(sizeof(struct usbd_hid));

    if (hid_class == NULL) {
        USB_LOG_ERR("no memory to alloc hid_class\r\n");
        return -1;
    }

    memset(hid_class, 0, sizeof(struct usbd_hid));
    hid_class->intf_num = intf;
    usb_slist_add_tail(&usbd_hid_head, &hid_class->list);
    return 0;
}

void usbd_hid_add_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = hid_class_request_handler;
    intf->custom_handler = hid_custom_request_handler;
    intf->vendor_handler = NULL;
    intf->notify_handler = hid_notify_handler;
    usbd_class_add_interface(devclass, intf);
    usbd_hid_alloc(intf->intf_num);
}

void usbd_hid_descriptor_register(uint8_t intf_num, const uint8_t *desc)
{
    // usbd_hid_cfg.hid_descriptor = desc;
}

void usbd_hid_report_descriptor_register(uint8_t intf_num, const uint8_t *desc, uint32_t desc_len)
{
    usb_slist_t *i;
    usb_slist_for_each(i, &usbd_hid_head)
    {
        struct usbd_hid *hid_class = usb_slist_entry(i, struct usbd_hid, list);

        if (hid_class->intf_num == intf_num) {
            hid_class->hid_report_descriptor = desc;
            hid_class->hid_report_descriptor_len = desc_len;
            return;
        }
    }
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