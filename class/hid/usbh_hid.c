/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_hid.h"

#define DEV_FORMAT "/dev/input%d"

static uint32_t g_devinuse = 0;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_hid_buf[128];

/****************************************************************************
 * Name: usbh_hid_devno_alloc
 *
 * Description:
 *   Allocate a unique /dev/input[n] minor number in the range 0-31.
 *
 ****************************************************************************/

static int usbh_hid_devno_alloc(struct usbh_hid *hid_class)
{
    size_t flags;
    int devno;

    flags = usb_osal_enter_critical_section();
    for (devno = 0; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            hid_class->minor = devno;
            usb_osal_leave_critical_section(flags);
            return 0;
        }
    }

    usb_osal_leave_critical_section(flags);
    return -EMFILE;
}

/****************************************************************************
 * Name: usbh_hid_devno_free
 *
 * Description:
 *   Free a /dev/input[n] minor number so that it can be used.
 *
 ****************************************************************************/

static void usbh_hid_devno_free(struct usbh_hid *hid_class)
{
    int devno = hid_class->minor;

    if (devno >= 0 && devno < 32) {
        size_t flags = usb_osal_enter_critical_section();
        g_devinuse &= ~(1 << devno);
        usb_osal_leave_critical_section(flags);
    }
}

static int usbh_hid_get_report_descriptor(struct usbh_hid *hid_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = &hid_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = HID_DESCRIPTOR_TYPE_HID_REPORT << 8;
    setup->wIndex = hid_class->intf;
    setup->wLength = 128;

    ret = usbh_control_transfer(hid_class->hport->ep0, setup, g_hid_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(buffer, g_hid_buf, ret - 8);
    return ret;
}

int usbh_hid_set_idle(struct usbh_hid *hid_class, uint8_t report_id, uint8_t duration)
{
    struct usb_setup_packet *setup = &hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_IDLE;
    //setup->wValue = report_id;
    //setup->wIndex = (duration << 8) | hid_class->intf;
    /* wValue, high-bytes sets the duration, or the max amount of time between reports 
                  0x00 = hid will send a report only when report data has changed or duration time elapsed
               low-bytes indicate the report id that the requeset applies to */
    setup->wValue = (duration << 8) | report_id;
    /* wIndex, num of interface that supports the request */
    setup->wIndex = hid_class->intf;
    setup->wLength = 0;

    return usbh_control_transfer(hid_class->hport->ep0, setup, NULL);
}

int usbh_hid_get_idle(struct usbh_hid *hid_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = &hid_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_GET_IDLE;
    setup->wValue = 0;
    setup->wIndex = hid_class->intf;
    setup->wLength = 1;

    ret = usbh_control_transfer(hid_class->hport->ep0, setup, g_hid_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(buffer, g_hid_buf, 1);
    return ret;
}

int usbh_hid_set_protocol(struct usbh_hid *hid_class, uint8_t protocol)
{
    struct usb_setup_packet *setup = &hid_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = HID_REQUEST_SET_PROTOCOL;
    setup->wValue = protocol;
    setup->wIndex = 0;
    setup->wLength = 0;   

    return usbh_control_transfer(hid_class->hport->ep0, setup, NULL);     
}

int usbh_hid_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_hid *hid_class = usb_malloc(sizeof(struct usbh_hid));
    if (hid_class == NULL) {
        USB_LOG_ERR("Fail to alloc hid_class\r\n");
        return -ENOMEM;
    }

    memset(hid_class, 0, sizeof(struct usbh_hid));
    usbh_hid_devno_alloc(hid_class);
    hid_class->hport = hport;
    hid_class->intf = intf;

    hport->config.intf[intf].priv = hid_class;

    // /* 0x0 = boot protocol, 0x1 = report protocol */
    // ret = usbh_hid_set_protocol(hid_class, 0x1);
    // if (ret < 0) {
    //     return ret;
    // }

    ret = usbh_hid_set_idle(hid_class, 0, 0);
    if (ret < 0) {
        return ret;
    }

    ret = usbh_hid_get_report_descriptor(hid_class, hid_class->report_desc);
    if (ret < 0) {
        return ret;
    }

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_hport_activate_epx(&hid_class->intin, hport, ep_desc);
        } else {
            usbh_hport_activate_epx(&hid_class->intout, hport, ep_desc);
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, hid_class->minor);

    USB_LOG_INFO("Register HID Class:%s\r\n", hport->config.intf[intf].devname);

    return 0;
}

int usbh_hid_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_hid *hid_class = (struct usbh_hid *)hport->config.intf[intf].priv;

    if (hid_class) {
        usbh_hid_devno_free(hid_class);

        if (hid_class->intin) {
            usbh_pipe_free(hid_class->intin);
        }

        if (hid_class->intout) {
            usbh_pipe_free(hid_class->intout);
        }

        memset(hid_class, 0, sizeof(struct usbh_hid));
        usb_free(hid_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister HID Class:%s\r\n", hport->config.intf[intf].devname);
    }

    return ret;
}

const struct usbh_class_driver hid_class_driver = {
    .driver_name = "hid",
    .connect = usbh_hid_connect,
    .disconnect = usbh_hid_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info hid_keyboard_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_HID,
    .subclass = HID_SUBCLASS_BOOTIF,
    .protocol = HID_PROTOCOL_KEYBOARD,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &hid_class_driver
};

CLASS_INFO_DEFINE const struct usbh_class_info hid_mouse_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_HID,
    .subclass = HID_SUBCLASS_BOOTIF,
    .protocol = HID_PROTOCOL_MOUSE,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &hid_class_driver
};
