/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_printer.h"

struct printer_cfg_priv {
    const uint8_t *device_id;
    uint8_t device_id_len;
    uint8_t port_status;
} usbd_printer_cfg;

static int printer_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("Printer Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    switch (setup->bRequest) {
        case PRINTER_REQUEST_GET_DEVICE_ID:
            memcpy(*data, usbd_printer_cfg.device_id, usbd_printer_cfg.device_id_len);
            *len = usbd_printer_cfg.device_id_len;
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

struct usbd_interface *usbd_printer_init_intf(struct usbd_interface *intf, const uint8_t *device_id, uint8_t device_id_len)
{
    intf->class_interface_handler = printer_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = printer_notify_handler;

    usbd_printer_cfg.device_id = device_id;
    usbd_printer_cfg.device_id_len = device_id_len;
    return intf;
}
