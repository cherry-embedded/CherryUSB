/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_cdc_acm.h"

#define DEV_FORMAT "/dev/ttyACM%d"

static uint32_t g_devinuse = 0;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX struct cdc_line_coding g_cdc_line_coding;

static int usbh_cdc_acm_devno_alloc(struct usbh_cdc_acm *cdc_acm_class)
{
    int devno;

    for (devno = 0; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            cdc_acm_class->minor = devno;
            return 0;
        }
    }
    return -EMFILE;
}

static void usbh_cdc_acm_devno_free(struct usbh_cdc_acm *cdc_acm_class)
{
    int devno = cdc_acm_class->minor;

    if (devno >= 0 && devno < 32) {
        g_devinuse &= ~(1 << devno);
    }
}

int usbh_cdc_acm_set_line_coding(struct usbh_cdc_acm *cdc_acm_class, struct cdc_line_coding *line_coding)
{
    struct usb_setup_packet *setup = &cdc_acm_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = cdc_acm_class->ctrl_intf;
    setup->wLength = 7;

    memcpy((uint8_t *)&g_cdc_line_coding, line_coding, sizeof(struct cdc_line_coding));

    return usbh_control_transfer(cdc_acm_class->hport->ep0, setup, (uint8_t *)&g_cdc_line_coding);
}

int usbh_cdc_acm_get_line_coding(struct usbh_cdc_acm *cdc_acm_class, struct cdc_line_coding *line_coding)
{
    struct usb_setup_packet *setup = &cdc_acm_class->hport->setup;
    int ret;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_LINE_CODING;
    setup->wValue = 0;
    setup->wIndex = cdc_acm_class->ctrl_intf;
    setup->wLength = 7;

    ret = usbh_control_transfer(cdc_acm_class->hport->ep0, setup, (uint8_t *)&g_cdc_line_coding);
    if (ret < 0) {
        return ret;
    }
    memcpy(line_coding, (uint8_t *)&g_cdc_line_coding, sizeof(struct cdc_line_coding));
    return ret;
}

int usbh_cdc_acm_set_line_state(struct usbh_cdc_acm *cdc_acm_class, bool dtr, bool rts)
{
    struct usb_setup_packet *setup = &cdc_acm_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_CONTROL_LINE_STATE;
    setup->wValue = (dtr << 0) | (rts << 1);
    setup->wIndex = cdc_acm_class->ctrl_intf;
    setup->wLength = 0;

    cdc_acm_class->dtr = dtr;
    cdc_acm_class->rts = rts;

    return usbh_control_transfer(cdc_acm_class->hport->ep0, setup, NULL);
}

static int usbh_cdc_acm_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_cdc_acm *cdc_acm_class = usb_malloc(sizeof(struct usbh_cdc_acm));
    if (cdc_acm_class == NULL) {
        USB_LOG_ERR("Fail to alloc cdc_acm_class\r\n");
        return -ENOMEM;
    }

    memset(cdc_acm_class, 0, sizeof(struct usbh_cdc_acm));
    usbh_cdc_acm_devno_alloc(cdc_acm_class);
    cdc_acm_class->hport = hport;
    cdc_acm_class->ctrl_intf = intf;
    cdc_acm_class->data_intf = intf + 1;

    hport->config.intf[intf].priv = cdc_acm_class;
    hport->config.intf[intf + 1].priv = NULL;

    cdc_acm_class->linecoding.dwDTERate = 115200;
    cdc_acm_class->linecoding.bDataBits = 8;
    cdc_acm_class->linecoding.bParityType = 0;
    cdc_acm_class->linecoding.bCharFormat = 0;
    ret = usbh_cdc_acm_set_line_coding(cdc_acm_class, &cdc_acm_class->linecoding);
    if (ret < 0) {
        USB_LOG_ERR("Fail to set linecoding\r\n");
        return ret;
    }

    ret = usbh_cdc_acm_set_line_state(cdc_acm_class, true, true);
    if (ret < 0) {
        USB_LOG_ERR("Fail to set line state\r\n");
        return ret;
    }

#ifdef CONFIG_USBHOST_CDC_ACM_NOTIFY
    ep_desc = &hport->config.intf[intf].altsetting[0].ep[0].ep_desc;
    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.hport = hport;
    usbh_pipe_alloc(&cdc_acm_class->intin, &ep_cfg);

#endif
    for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].altsetting[0].ep[i].ep_desc;

        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_hport_activate_epx(&cdc_acm_class->bulkin, hport, ep_desc);
        } else {
            usbh_hport_activate_epx(&cdc_acm_class->bulkout, hport, ep_desc);
        }
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cdc_acm_class->minor);

    USB_LOG_INFO("Register CDC ACM Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_cdc_acm_run(cdc_acm_class);
    return ret;
}

static int usbh_cdc_acm_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)hport->config.intf[intf].priv;

    if (cdc_acm_class) {
        usbh_cdc_acm_devno_free(cdc_acm_class);

        if (cdc_acm_class->bulkin) {
            usbh_pipe_free(cdc_acm_class->bulkin);
        }

        if (cdc_acm_class->bulkout) {
            usbh_pipe_free(cdc_acm_class->bulkout);
        }

        usbh_cdc_acm_stop(cdc_acm_class);
        memset(cdc_acm_class, 0, sizeof(struct usbh_cdc_acm));
        usb_free(cdc_acm_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister CDC ACM Class:%s\r\n", hport->config.intf[intf].devname);
    }

    return ret;
}

static int usbh_cdc_data_connect(struct usbh_hubport *hport, uint8_t intf)
{
    return 0;
}

static int usbh_cdc_data_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    return 0;
}

__WEAK void usbh_cdc_acm_run(struct usbh_cdc_acm *cdc_acm_class)
{

}

__WEAK void usbh_cdc_acm_stop(struct usbh_cdc_acm *cdc_acm_class)
{

}

const struct usbh_class_driver cdc_acm_class_driver = {
    .driver_name = "cdc_acm",
    .connect = usbh_cdc_acm_connect,
    .disconnect = usbh_cdc_acm_disconnect
};

const struct usbh_class_driver cdc_data_class_driver = {
    .driver_name = "cdc_data",
    .connect = usbh_cdc_data_connect,
    .disconnect = usbh_cdc_data_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info cdc_acm_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_CDC,
    .subclass = CDC_ABSTRACT_CONTROL_MODEL,
    .protocol = CDC_COMMON_PROTOCOL_AT_COMMANDS,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &cdc_acm_class_driver
};

CLASS_INFO_DEFINE const struct usbh_class_info cdc_data_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS,
    .class = USB_DEVICE_CLASS_CDC_DATA,
    .subclass = 0x00,
    .protocol = 0x00,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &cdc_data_class_driver
};
