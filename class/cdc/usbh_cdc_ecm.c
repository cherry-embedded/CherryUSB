/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_cdc_ecm.h"

#define DEV_FORMAT                       "/dev/cdc_ether"

/* general descriptor field offsets */
#define DESC_bLength                     0 /** Length offset */
#define DESC_bDescriptorType             1 /** Descriptor type offset */
#define DESC_bDescriptorSubType          2 /** Descriptor subtype offset */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber       2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting      3 /** Alternate setting offset */

#define CONFIG_USBHOST_MAX_CDC_ECM_CLASS 1
static struct usbh_cdc_ecm g_cdc_ecm_class[CONFIG_USBHOST_MAX_CDC_ECM_CLASS];
static uint32_t g_devinuse = 0;

static struct usbh_cdc_ecm *usbh_cdc_ecm_class_alloc(void)
{
    int devno;

    for (devno = 0; devno < CONFIG_USBHOST_MAX_CDC_ECM_CLASS; devno++) {
        if ((g_devinuse & (1 << devno)) == 0) {
            g_devinuse |= (1 << devno);
            memset(&g_cdc_ecm_class[devno], 0, sizeof(struct usbh_cdc_ecm));
            g_cdc_ecm_class[devno].minor = devno;
            return &g_cdc_ecm_class[devno];
        }
    }
    return NULL;
}

static void usbh_cdc_ecm_class_free(struct usbh_cdc_ecm *cdc_ecm_class)
{
    int devno = cdc_ecm_class->minor;

    if (devno >= 0 && devno < 32) {
        g_devinuse &= ~(1 << devno);
    }
    memset(cdc_ecm_class, 0, sizeof(struct usbh_cdc_ecm));
}

int usbh_cdc_ecm_set_eth_packet_filter(struct usbh_cdc_ecm *cdc_ecm_class, uint16_t filter_value)
{
    struct usb_setup_packet *setup = cdc_ecm_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SET_ETHERNET_PACKET_FILTER;
    setup->wValue = filter_value;
    setup->wIndex = cdc_ecm_class->ctrl_intf;
    setup->wLength = 0;

    return usbh_control_transfer(cdc_ecm_class->hport->ep0, setup, NULL);
}

int usbh_cdc_ecm_select_altsetting(struct usbh_cdc_ecm *cdc_ecm_class, uint16_t alt_setting)
{
    struct usb_setup_packet *setup = cdc_ecm_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_SET_INTERFACE;
    setup->wValue = alt_setting;
    setup->wIndex = cdc_ecm_class->data_intf;
    setup->wLength = 0;

    return usbh_control_transfer(cdc_ecm_class->hport->ep0, setup, NULL);
}

static int usbh_cdc_ecm_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint8_t altsetting = 0;
    uint8_t mac_buffer[32];
    uint8_t *p;
    uint8_t cur_iface = 0xff;
    uint8_t mac_str_idx = 0xff;

    struct usbh_cdc_ecm *cdc_ecm_class = usbh_cdc_ecm_class_alloc();
    if (cdc_ecm_class == NULL) {
        USB_LOG_ERR("Fail to alloc cdc_ecm_class\r\n");
        return -ENOMEM;
    }

    cdc_ecm_class->hport = hport;
    cdc_ecm_class->ctrl_intf = intf;
    cdc_ecm_class->data_intf = intf + 1;

    hport->config.intf[intf].priv = cdc_ecm_class;
    hport->config.intf[intf + 1].priv = NULL;

    p = hport->raw_config_desc;
    while (p[DESC_bLength]) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                cur_iface = p[INTF_DESC_bInterfaceNumber];
                //cur_alt_setting = p[INTF_DESC_bAlternateSetting];
                break;
            case CDC_CS_INTERFACE:
                if ((cur_iface == cdc_ecm_class->ctrl_intf) && p[DESC_bDescriptorSubType] == CDC_FUNC_DESC_ETHERNET_NETWORKING) {
                    mac_str_idx = p[3];
                    goto get_mac;
                }
                break;

            default:
                break;
        }
        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

get_mac:
    if (mac_str_idx == 0xff) {
        USB_LOG_ERR("Do not find cdc ecm mac string\r\n");
        return -1;
    }

    memset(mac_buffer, 0, 32);
    ret = usbh_get_string_desc(cdc_ecm_class->hport, mac_str_idx, mac_buffer);
    if (ret < 0) {
        return ret;
    }

    uint8_t len = strlen(mac_buffer);

    for (int i = 0, j = 0; i < len; i += 2, j++) {
        char byte_str[3];
        byte_str[0] = mac_buffer[i];
        byte_str[1] = mac_buffer[i + 1];
        byte_str[2] = '\0';

        uint32_t byte = strtoul(byte_str, NULL, 16);
        cdc_ecm_class->mac[j] = (unsigned char)byte;
    }

    USB_LOG_INFO("CDC ECM mac address %02x: %02x: %02x: %02x: %02x: %02x\r\n",
                 cdc_ecm_class->mac[0],
                 cdc_ecm_class->mac[1],
                 cdc_ecm_class->mac[2],
                 cdc_ecm_class->mac[3],
                 cdc_ecm_class->mac[4],
                 cdc_ecm_class->mac[5]);

    /* enable int ep */
    ep_desc = &hport->config.intf[intf].altsetting[0].ep[0].ep_desc;
    usbh_hport_activate_epx(&cdc_ecm_class->intin, hport, ep_desc);

    if (hport->config.intf[intf + 1].altsetting_num > 1) {
        altsetting = hport->config.intf[intf + 1].altsetting_num - 1;

        for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[altsetting].intf_desc.bNumEndpoints; i++) {
            ep_desc = &hport->config.intf[intf + 1].altsetting[altsetting].ep[i].ep_desc;

            if (ep_desc->bEndpointAddress & 0x80) {
                usbh_hport_activate_epx(&cdc_ecm_class->bulkin, hport, ep_desc);
            } else {
                usbh_hport_activate_epx(&cdc_ecm_class->bulkout, hport, ep_desc);
            }
        }

        USB_LOG_INFO("Select cdc ecm altsetting: %d\r\n", altsetting);
        usbh_cdc_ecm_select_altsetting(cdc_ecm_class, altsetting);
    } else {
        for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[0].intf_desc.bNumEndpoints; i++) {
            ep_desc = &hport->config.intf[intf + 1].altsetting[0].ep[i].ep_desc;

            if (ep_desc->bEndpointAddress & 0x80) {
                usbh_hport_activate_epx(&cdc_ecm_class->bulkin, hport, ep_desc);
            } else {
                usbh_hport_activate_epx(&cdc_ecm_class->bulkout, hport, ep_desc);
            }
        }
    }

    // ret = usbh_cdc_ecm_set_eth_packet_filter(cdc_ecm_class, 0x000c);
    // if (ret < 0) {
    //     return ret;
    // }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cdc_ecm_class->minor);

    USB_LOG_INFO("Register CDC ECM Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_cdc_ecm_run(cdc_ecm_class);
    return ret;
}

static int usbh_cdc_ecm_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_cdc_ecm *cdc_ecm_class = (struct usbh_cdc_ecm *)hport->config.intf[intf].priv;

    if (cdc_ecm_class) {
        if (cdc_ecm_class->bulkin) {
            usbh_pipe_free(cdc_ecm_class->bulkin);
        }

        if (cdc_ecm_class->bulkout) {
            usbh_pipe_free(cdc_ecm_class->bulkout);
        }

        if (cdc_ecm_class->intin) {
            usbh_pipe_free(cdc_ecm_class->intin);
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            USB_LOG_INFO("Unregister CDC ECM Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_cdc_ecm_stop(cdc_ecm_class);
        }

        usbh_cdc_ecm_class_free(cdc_ecm_class);
    }

    return ret;
}

__WEAK void usbh_cdc_ecm_run(struct usbh_cdc_ecm *cdc_ecm_class)
{
}

__WEAK void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class)
{
}

const struct usbh_class_driver cdc_ecm_class_driver = {
    .driver_name = "cdc_ecm",
    .connect = usbh_cdc_ecm_connect,
    .disconnect = usbh_cdc_ecm_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info cdc_ecm_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_CDC,
    .subclass = CDC_ETHERNET_NETWORKING_CONTROL_MODEL,
    .protocol = CDC_COMMON_PROTOCOL_NONE,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &cdc_ecm_class_driver
};