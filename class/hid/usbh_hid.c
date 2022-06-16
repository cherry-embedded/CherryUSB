/**
 * @file usbh_hid.c
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
#include "usbh_core.h"
#include "usbh_hid.h"

#define DEV_FORMAT "/dev/input%d"

static uint32_t g_devinuse = 0;

/****************************************************************************
 * Name: usbh_hid_devno_alloc
 *
 * Description:
 *   Allocate a unique /dev/input[n] minor number in the range 0-31.
 *
 ****************************************************************************/

static int usbh_hid_devno_alloc(struct usbh_hid *hid_class)
{
    size_t flags;
    int devno;

    flags = usb_osal_enter_critical_section();
    for (devno = 0; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            hid_class->minor = devno;
            usb_osal_leave_critical_section(flags);
            return 0;
        }
    }

    usb_osal_leave_critical_section(flags);
    return -EMFILE;
}

/****************************************************************************
 * Name: usbh_hid_devno_free
 *
 * Description:
 *   Free a /dev/input[n] minor number so that it can be used.
 *
 ****************************************************************************/

static void usbh_hid_devno_free(struct usbh_hid *hid_class)
{
    int devno = hid_class->minor;

    if (devno >= 0 && devno < 32) {
        size_t flags = usb_osal_enter_critical_section();
        g_devinuse &= ~(1 << devno);
        usb_osal_leave_critical_section(flags);
    }
}

static int usbh_hid_get_report_descriptor(struct usbh_hid *hid_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = HID_DESCRIPTOR_TYPE_HID_REPORT << 8;
    setup->wIndex = hid_class->intf;
    setup->wLength = 128;

    return usbh_control_transfer(hid_class->hport->ep0, setup, buffer);
}

int usbh_hid_set_idle(struct usbh_hid *hid_class, uint8_t report_id, uint8_t duration)
{
    struct usb_setup_packet *setup = hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_IDLE;
    setup->wValue = report_id;
    setup->wIndex = (duration << 8) | hid_class->intf;
    setup->wLength = 0;

    return usbh_control_transfer(hid_class->hport->ep0, setup, NULL);
}

int usbh_hid_get_idle(struct usbh_hid *hid_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_GET_IDLE;
    setup->wValue = 0;
    setup->wIndex = hid_class->intf;
    setup->wLength = 1;

    return usbh_control_transfer(hid_class->hport->ep0, setup, buffer);
}

int usbh_hid_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_hid *hid_class = usb_malloc(sizeof(struct usbh_hid));
    if (hid_class == NULL) {
        USB_LOG_ERR("Fail to alloc hid_class\r\n");
        return -ENOMEM;
    }

    memset(hid_class, 0, sizeof(struct usbh_hid));
    usbh_hid_devno_alloc(hid_class);
    hid_class->hport = hport;
    hid_class->intf = intf;

    hport->config.intf[intf].priv = hid_class;

    ret = usbh_hid_set_idle(hid_class, 0, 0);
    if (ret < 0) {
        return ret;
    }

    uint8_t *report_buffer = usb_iomalloc(128);
    ret = usbh_hid_get_report_descriptor(hid_class, report_buffer);
    if (ret < 0) {
        usb_iofree(report_buffer);
        return ret;
    }
    usb_iofree(report_buffer);

    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;
        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_ep_alloc(&hid_class->intin, &ep_cfg);
        } else {
            usbh_ep_alloc(&hid_class->intout, &ep_cfg);
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, hid_class->minor);

    USB_LOG_INFO("Register HID Class:%s\r\n", hport->config.intf[intf].devname);

    extern int hid_test();
    hid_test();
    return 0;
}

int usbh_hid_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_hid *hid_class = (struct usbh_hid *)hport->config.intf[intf].priv;

    if (hid_class) {
        usbh_hid_devno_free(hid_class);

        if (hid_class->intin) {
            ret = usb_ep_cancel(hid_class->intin);
            if (ret < 0) {
            }
            usbh_ep_free(hid_class->intin);
        }

        if (hid_class->intout) {
            ret = usb_ep_cancel(hid_class->intout);
            if (ret < 0) {
            }
            usbh_ep_free(hid_class->intout);
        }

        usb_free(hid_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister HID Class:%s\r\n", hport->config.intf[intf].devname);

        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);
        hport->config.intf[intf].priv = NULL;
    }

    return ret;
}

const struct usbh_class_driver hid_class_driver = {
    .driver_name = "hid",
    .connect = usbh_hid_connect,
    .disconnect = usbh_hid_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info hid_keyboard_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_HID,
    .subclass = HID_SUBCLASS_BOOTIF,
    .protocol = HID_PROTOCOL_KEYBOARD,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &hid_class_driver
};

CLASS_INFO_DEFINE const struct usbh_class_info hid_mouse_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_HID,
    .subclass = HID_SUBCLASS_BOOTIF,
    .protocol = HID_PROTOCOL_MOUSE,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &hid_class_driver
};
