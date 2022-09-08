/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_mtp.h"

struct mtp_cfg_priv {
    uint8_t device_status;
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
static struct usbd_interface mtp_ep_data[2];

static int mtp_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
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