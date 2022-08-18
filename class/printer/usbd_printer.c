/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_printer.h"

struct printer_cfg_priv {
    uint8_t *device_id;
    uint8_t port_status;
} usbd_printer_cfg;

static int printer_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("Printer Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    switch (setup->bRequest) {
        case PRINTER_REQUEST_GET_DEVICE_ID:

            break;
        case PRINTER_REQUEST_GET_PORT_SATTUS:

            break;
        case PRINTER_REQUEST_SOFT_RESET:

            break;
        default:
            USB_LOG_WRN("Unhandled Printer Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void printer_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;

        default:
            break;
    }
}

void usbd_printer_add_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = printer_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = printer_notify_handler;

    usbd_class_add_interface(devclass, intf);
}
