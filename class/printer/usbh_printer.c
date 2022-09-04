/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_printer.h"

#define DEV_FORMAT "/dev/printer"

static int usbh_printer_get_device_id(struct usbh_printer *printer_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = &printer_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = PRINTER_REQUEST_GET_DEVICE_ID;
    setup->wValue = 0;
    setup->wIndex = printer_class->intf;
    setup->wLength = 256;

    return usbh_control_transfer(printer_class->hport->ep0, setup, buffer);
}

static int usbh_printer_get_port_status(struct usbh_printer *printer_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = &printer_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = PRINTER_REQUEST_GET_PORT_SATTUS;
    setup->wValue = 0;
    setup->wIndex = printer_class->intf;
    setup->wLength = 1;

    return usbh_control_transfer(printer_class->hport->ep0, setup, buffer);
}

static int usbh_printer_soft_reset(struct usbh_printer *printer_class)
{
    struct usb_setup_packet *setup = &printer_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = PRINTER_REQUEST_SOFT_RESET;
    setup->wValue = 0;
    setup->wIndex = printer_class->intf;
    setup->wLength = 0;

    return usbh_control_transfer(printer_class->hport->ep0, setup, NULL);
}

static int usbh_printer_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_printer *printer_class = usb_malloc(sizeof(struct usbh_printer));
    if (printer_class == NULL) {
        USB_LOG_ERR("Fail to alloc printer_class\r\n");
        return -ENOMEM;
    }

    memset(printer_class, 0, sizeof(struct usbh_printer));

    printer_class->hport = hport;
    printer_class->intf = intf;

    hport->config.intf[intf].priv = printer_class;

    for (uint8_t i = 0; i < hport->config.intf[intf + 1].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_pipe_alloc(&printer_class->bulkin, &ep_cfg);
        } else {
            usbh_pipe_alloc(&printer_class->bulkout, &ep_cfg);
        }
    }

    // uint8_t *device_id = usb_iomalloc(256);
    // ret = usbh_printer_get_device_id(printer_class, device_id);
    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    USB_LOG_INFO("Register Printer Class:%s\r\n", hport->config.intf[intf].devname);

    return ret;
}

static int usbh_printer_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_printer *printer_class = (struct usbh_printer *)hport->config.intf[intf].priv;

    if (printer_class) {
        if (printer_class->bulkin) {
            usbh_pipe_free(printer_class->bulkin);
        }

        if (printer_class->bulkout) {
            usbh_pipe_free(printer_class->bulkout);
        }

        usb_free(printer_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister Printer Class:%s\r\n", hport->config.intf[intf].devname);

        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);
        hport->config.intf[intf].priv = NULL;
    }

    return ret;
}

static const struct usbh_class_driver printer_class_driver = {
    .driver_name = "printer",
    .connect = usbh_printer_connect,
    .disconnect = usbh_printer_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info printer_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_PRINTER,
    .subclass = PRINTER_SUBCLASS,
    .protocol = 0x00,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &printer_class_driver
};
