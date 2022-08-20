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

struct usbd_interface *usbd_dfu_alloc_intf(void)
{
    struct usbd_interface *intf = usb_malloc(sizeof(struct usbd_interface));
    if (intf == NULL) {
        USB_LOG_ERR("no mem to alloc intf\r\n");
        return NULL;
    }

    intf->class_handler = dfu_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = dfu_notify_handler;

    return intf;
}