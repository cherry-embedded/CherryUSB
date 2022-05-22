/**
 * @file usbh_mtp.c
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
#include "usbh_mtp.h"

#define DEV_FORMAT "/dev/mtp"

static int usbh_mtp_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_mtp *mtp_class = usb_malloc(sizeof(struct usbh_mtp));
    if (mtp_class == NULL) {
        USB_LOG_ERR("Fail to alloc mtp_class\r\n");
        return -ENOMEM;
    }

    memset(mtp_class, 0, sizeof(struct usbh_mtp));

    mtp_class->hport = hport;
    mtp_class->ctrl_intf = intf;
    mtp_class->data_intf = intf + 1;

    hport->config.intf[intf].priv = mtp_class;
    hport->config.intf[intf + 1].priv = NULL;
    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

#ifdef CONFIG_USBHOST_MTP_NOTIFY
    ep_desc = &hport->config.intf[intf].ep[0].ep_desc;
    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.hport = hport;
    usbh_ep_alloc(&mtp_class->intin, &ep_cfg);

#endif
    for (uint8_t i = 0; i < hport->config.intf[intf + 1].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_ep_alloc(&mtp_class->bulkin, &ep_cfg);
        } else {
            usbh_ep_alloc(&mtp_class->bulkout, &ep_cfg);
        }
    }

    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    USB_LOG_INFO("Register MTP Class:%s\r\n", hport->config.intf[intf].devname);

    return ret;
}

static int usbh_mtp_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_mtp *mtp_class = (struct usbh_mtp *)hport->config.intf[intf].priv;

    if (mtp_class) {
        if (mtp_class->bulkin) {
            ret = usb_ep_cancel(mtp_class->bulkin);
            if (ret < 0) {
            }
            usbh_ep_free(mtp_class->bulkin);
        }

        if (mtp_class->bulkout) {
            ret = usb_ep_cancel(mtp_class->bulkout);
            if (ret < 0) {
            }
            usbh_ep_free(mtp_class->bulkout);
        }

        usb_free(mtp_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister MTP Class:%s\r\n", hport->config.intf[intf].devname);

        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);
        hport->config.intf[intf].priv = NULL;
        hport->config.intf[intf + 1].priv = NULL;
    }

    return ret;
}

static const struct usbh_class_driver mtp_class_driver = {
    .driver_name = "mtp",
    .connect = usbh_mtp_connect,
    .disconnect = usbh_mtp_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info mtp_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_MTP_CLASS,
    .subclass = USB_MTP_SUB_CLASS,
    .protocol = USB_MTP_PROTOCOL,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &mtp_class_driver
};
