/**
 * @file usbd_dfu.c
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
#include "usbd_dfu.h"

/* Device data structure */
struct dfu_cfg_priv {
    struct dfu_info info;
} usbd_dfu_cfg;

static int dfu_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_WRN("DFU Class request: "
                 "bRequest 0x%02x\r\n",
                 setup->bRequest);

    switch (setup->bRequest) {
        case DFU_REQUEST_DETACH:
            break;
        case DFU_REQUEST_DNLOAD:
            break;
        case DFU_REQUEST_UPLOAD:
            break;
        case DFU_REQUEST_GETSTATUS:
            break;
        case DFU_REQUEST_CLRSTATUS:
            break;
        case DFU_REQUEST_GETSTATE:
            break;
        case DFU_REQUEST_ABORT:
            break;
        default:
            USB_LOG_WRN("Unhandled DFU Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void dfu_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:

            break;

        default:
            break;
    }
}

void usbd_dfu_add_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = dfu_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = dfu_notify_handler;

    usbd_class_add_interface(devclass, intf);
}
