/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_dfu.h"

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
    intf->class_handler = dfu_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = dfu_notify_handler;

    usbd_class_add_interface(devclass, intf);
}
