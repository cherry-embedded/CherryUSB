/**
 * @file usbh_hid.c
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

#define DEV_FORMAT  "/dev/input%d"
#define DEV_NAMELEN 16

static uint32_t g_devinuse = 0;

/****************************************************************************
 * Name: usbh_hid_devno_alloc
 *
 * Description:
 *   Allocate a unique /dev/hid[n] minor number in the range 0-31.
 *
 ****************************************************************************/

static int usbh_hid_devno_alloc(struct usbh_hid *priv)
{
    uint32_t flags;
    int devno;

    flags = usb_osal_enter_critical_section();
    for (devno = 0; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            priv->minor = devno;
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
 *   Free a /dev/hid[n] minor number so that it can be used.
 *
 ****************************************************************************/

static void usbh_hid_devno_free(struct usbh_hid *priv)
{
    int devno = priv->minor;

    if (devno >= 0 && devno < 32) {
        uint32_t flags = usb_osal_enter_critical_section();
        g_devinuse &= ~(1 << devno);
        usb_osal_leave_critical_section(flags);
    }
}

/****************************************************************************
 * Name: usbh_hid_mkdevname
 *
 * Description:
 *   Format a /dev/hid[n] device name given a minor number.
 *
 ****************************************************************************/

static inline void usbh_hid_mkdevname(struct usbh_hid *priv, char *devname)
{
    snprintf(devname, DEV_NAMELEN, DEV_FORMAT, priv->minor);
}

int usbh_hid_get_report_descriptor(struct usbh_hubport *hport, uint8_t intf, uint8_t *buffer)
{
    struct usb_setup_packet *setup;
    struct usbh_hid *hid_class = (struct usbh_hid *)hport->config.intf[intf].priv;

    setup = hid_class->setup;

    if (hid_class->intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = HID_DESCRIPTOR_TYPE_HID_REPORT << 8;
    setup->wIndex = intf;
    setup->wLength = 128;

    return usbh_control_transfer(hport->ep0, setup, buffer);
}

int usbh_hid_set_idle(struct usbh_hubport *hport, uint8_t intf, uint8_t report_id, uint8_t duration)
{
    int ret;
    struct usb_setup_packet *setup;
    struct usbh_hid *hid_class = (struct usbh_hid *)hport->config.intf[intf].priv;

    setup = hid_class->setup;

    if (hid_class->intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_IDLE;
    setup->wValue = report_id;
    setup->wIndex = (duration << 8) | intf;
    setup->wLength = 0;

    ret = usbh_control_transfer(hport->ep0, setup, NULL);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int usbh_hid_get_idle(struct usbh_hubport *hport, uint8_t intf, uint8_t *buffer)
{
    int ret;
    struct usb_setup_packet *setup;
    struct usbh_hid *hid_class = (struct usbh_hid *)hport->config.intf[intf].priv;

    setup = hid_class->setup;

    if (hid_class->intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_GET_IDLE;
    setup->wValue = 0;
    setup->wIndex = intf;
    setup->wLength = 1;

    ret = usbh_control_transfer(hport->ep0, setup, buffer);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

USB_NOCACHE_RAM_SECTION uint8_t hid_buffer[128];

void usbh_hid_callback(void *arg, int nbytes)
{
    struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            printf("0x%02x ", hid_buffer[i]);
        }
    }

    printf("nbytes:%d\r\n", nbytes);
}

int usbh_hid_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    char devname[DEV_NAMELEN];
    int ret;

    struct usbh_hid *hid_class = usb_malloc(sizeof(struct usbh_hid));
    if (hid_class == NULL) {
        USB_LOG_ERR("Fail to alloc hid_class\r\n");
        return -ENOMEM;
    }
    memset(hid_class, 0, sizeof(struct usbh_hid));

    usbh_hid_devno_alloc(hid_class);
    usbh_hid_mkdevname(hid_class, devname);

    hport->config.intf[intf].priv = hid_class;

    hid_class->setup = usb_iomalloc(sizeof(struct usb_setup_packet));
    if (hid_class->setup == NULL) {
        USB_LOG_ERR("Fail to alloc setup\r\n");
        return -ENOMEM;
    }
    hid_class->intf = intf;

    ret = usbh_hid_set_idle(hport, intf, 0, 0);
    if (ret < 0) {
        return ret;
    }

    ret = usbh_hid_get_report_descriptor(hport, intf, hid_buffer);
    if (ret < 0) {
        return ret;
    }

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

    USB_LOG_INFO("Register HID Class:%s\r\n", devname);

#if 1
    ret = usbh_ep_intr_async_transfer(hid_class->intin, hid_buffer, 128, usbh_hid_callback, hid_class);
    if (ret < 0) {
        return ret;
    }
#else
    ret = usbh_ep_intr_transfer(hid_class->intin, hid_buffer, 128);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("recv len:%d\r\n", ret);
#endif
    return 0;
}

int usbh_hid_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    char devname[DEV_NAMELEN];
    int ret = 0;

    struct usbh_hid *hid_class = (struct usbh_hid *)hport->config.intf[intf].priv;

    if (hid_class) {
        usbh_hid_devno_free(hid_class);
        usbh_hid_mkdevname(hid_class, devname);

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
        if (hid_class->setup)
            usb_iofree(hid_class->setup);

        usb_free(hid_class);
        hport->config.intf[intf].priv = NULL;

        USB_LOG_INFO("Unregister HID Class:%s\r\n", devname);
    }

    return ret;
}

const struct usbh_class_driver hid_class_driver = {
    .driver_name = "hid",
    .connect = usbh_hid_connect,
    .disconnect = usbh_hid_disconnect
};