/**
 * @file usbd_mtp.h
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
#include "usbd_mtp.h"

/* Device data structure */
struct mtp_cfg_priv {
    USB_MEM_ALIGN32 uint8_t device_status;
} usbd_mtp_cfg;

/* max USB packet size */
#ifndef CONFIG_USB_HS
#define MTP_BULK_EP_MPS 64
#else
#define MTP_BULK_EP_MPS 512
#endif

#define MSD_OUT_EP_IDX 0
#define MSD_IN_EP_IDX  1

/* Describe EndPoints configuration */
static usbd_endpoint_t mtp_ep_data[2];

/**
 * @brief Handler called for Class requests not handled by the USB stack.
 *
 * @param setup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
static int mtp_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("MTP Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    switch (setup->bRequest) {
        case MTP_REQUEST_CANCEL:

            break;
        case MTP_REQUEST_GET_EXT_EVENT_DATA:

            break;
        case MTP_REQUEST_RESET:

            break;
        case MTP_REQUEST_GET_DEVICE_STATUS:

            break;

        default:
            USB_LOG_WRN("Unhandled MTP Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void usbd_mtp_bulk_out(uint8_t ep)
{
}

static void usbd_mtp_bulk_in(uint8_t ep)
{
}

static void mtp_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;

        default:
            break;
    }
}