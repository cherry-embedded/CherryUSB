/**
 * @file usbh_cdc_acm.c
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
#include "usbh_cdc_acm.h"

#define DEV_FORMAT "/dev/ttyACM%d"

static uint32_t g_devinuse = 0;

/****************************************************************************
 * Name: usbh_cdc_acm_devno_alloc
 *
 * Description:
 *   Allocate a unique /dev/ttyACM[n] minor number in the range 0-31.
 *
 ****************************************************************************/

static int usbh_cdc_acm_devno_alloc(struct usbh_cdc_acm *priv)
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
 * Name: usbh_cdc_acm_devno_free
 *
 * Description:
 *   Free a /dev/ttyACM[n] minor number so that it can be used.
 *
 ****************************************************************************/

static void usbh_cdc_acm_devno_free(struct usbh_cdc_acm *priv)
{
    int devno = priv->minor;

    if (devno >= 0 && devno < 32) {
        uint32_t flags = usb_osal_enter_critical_section();
        g_devinuse &= ~(1 << devno);
        usb_osal_leave_critical_section(flags);
    }
}

int usbh_cdc_acm_set_line_coding(struct usbh_hubport *hport, uint8_t intf, struct cdc_line_coding *line_coding)
{
    int ret;
    struct usb_setup_packet *setup;
    struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)hport->config.intf[intf].priv;

    setup = hport->setup;

    if (cdc_acm_class->ctrl_intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = intf;
    setup->wLength = 7;

    ret = usbh_control_transfer(hport->ep0, setup, (uint8_t *)line_coding);
    if (ret < 0) {
        return ret;
    }
    memcpy(cdc_acm_class->linecoding, line_coding, sizeof(struct cdc_line_coding));
    return 0;
}

int usbh_cdc_acm_get_line_coding(struct usbh_hubport *hport, uint8_t intf, struct cdc_line_coding *line_coding)
{
    int ret;
    struct usb_setup_packet *setup;
    struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)hport->config.intf[intf].priv;

    setup = hport->setup;

    if (cdc_acm_class->ctrl_intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = intf;
    setup->wLength = 7;

    ret = usbh_control_transfer(hport->ep0, setup, (uint8_t *)line_coding);
    if (ret < 0) {
        return ret;
    }
    memcpy(cdc_acm_class->linecoding, line_coding, sizeof(struct cdc_line_coding));
    return 0;
}

int usbh_cdc_acm_set_line_state(struct usbh_hubport *hport, uint8_t intf, bool dtr, bool rts)
{
    int ret;
    struct usb_setup_packet *setup;
    struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)hport->config.intf[intf].priv;

    setup = hport->setup;

    if (cdc_acm_class->ctrl_intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE;
    setup->wValue = (dtr << 0) | (rts << 1);
    setup->wIndex = intf;
    setup->wLength = 0;

    ret = usbh_control_transfer(hport->ep0, setup, NULL);
    if (ret < 0) {
        return ret;
    }

    cdc_acm_class->dtr = dtr;
    cdc_acm_class->rts = rts;

    return 0;
}

int usbh_cdc_acm_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_cdc_acm *cdc_acm_class = usb_malloc(sizeof(struct usbh_cdc_acm));
    if (cdc_acm_class == NULL) {
        USB_LOG_ERR("Fail to alloc cdc_acm_class\r\n");
        return -ENOMEM;
    }

    memset(cdc_acm_class, 0, sizeof(struct usbh_cdc_acm));

    usbh_cdc_acm_devno_alloc(cdc_acm_class);
    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cdc_acm_class->minor);

    hport->config.intf[intf].priv = cdc_acm_class;
    hport->config.intf[intf + 1].priv = NULL;

    cdc_acm_class->linecoding = usb_iomalloc(sizeof(struct cdc_line_coding));
    if (cdc_acm_class->linecoding == NULL) {
        USB_LOG_ERR("Fail to alloc linecoding\r\n");
        return -ENOMEM;
    }
    cdc_acm_class->ctrl_intf = intf;
    cdc_acm_class->data_intf = intf + 1;

    cdc_acm_class->linecoding->dwDTERate = 115200;
    cdc_acm_class->linecoding->bDataBits = 8;
    cdc_acm_class->linecoding->bParityType = 0;
    cdc_acm_class->linecoding->bCharFormat = 0;
    ret = usbh_cdc_acm_set_line_coding(hport, intf, cdc_acm_class->linecoding);
    if (ret < 0) {
        return ret;
    }

    ret = usbh_cdc_acm_set_line_state(hport, intf, true, true);
    if (ret < 0) {
        return ret;
    }

#ifdef CONFIG_USBHOST_CDC_ACM_NOTIFY
    ep_desc = &hport->config.intf[intf].ep[0].ep_desc;
    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.hport = hport;
    usbh_ep_alloc(&cdc_acm_class->intin, &ep_cfg);

#endif
    for (uint8_t i = 0; i < hport->config.intf[intf + 1].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_ep_alloc(&cdc_acm_class->bulkin, &ep_cfg);
        } else {
            usbh_ep_alloc(&cdc_acm_class->bulkout, &ep_cfg);
        }
    }

    USB_LOG_INFO("Register CDC ACM Class:%s\r\n", hport->config.intf[intf].devname);

    extern int cdc_acm_test();
    cdc_acm_test();
    return ret;
}

int usbh_cdc_acm_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)hport->config.intf[intf].priv;

    if (cdc_acm_class) {
        usbh_cdc_acm_devno_free(cdc_acm_class);

        if (cdc_acm_class->bulkin) {
            ret = usb_ep_cancel(cdc_acm_class->bulkin);
            if (ret < 0) {
            }
            usbh_ep_free(cdc_acm_class->bulkin);
        }

        if (cdc_acm_class->bulkout) {
            ret = usb_ep_cancel(cdc_acm_class->bulkout);
            if (ret < 0) {
            }
            usbh_ep_free(cdc_acm_class->bulkout);
        }

        if (cdc_acm_class->linecoding)
            usb_iofree(cdc_acm_class->linecoding);

        usb_free(cdc_acm_class);

        USB_LOG_INFO("Unregister CDC ACM Class:%s\r\n", hport->config.intf[intf].devname);
        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);

        hport->config.intf[intf].priv = NULL;
        hport->config.intf[intf + 1].priv = NULL;
    }

    return ret;
}

const struct usbh_class_driver cdc_acm_class_driver = {
    .driver_name = "cdc_acm",
    .connect = usbh_cdc_acm_connect,
    .disconnect = usbh_cdc_acm_disconnect
};