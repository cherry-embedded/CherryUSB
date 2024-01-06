/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_bluetooth.h"

#define DEV_FORMAT "/dev/bluetooth"

static struct usbh_bluetooth g_bluetooth_class;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_bluetooth_cmd_buf[512];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_bluetooth_event_buf[512];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_bluetooth_acl_buf[1024];

static int usbh_bluetooth_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint8_t mult;
    uint16_t mps;

    struct usbh_bluetooth *bluetooth_class = &g_bluetooth_class;

    if (hport->config.config_desc.bNumInterfaces == (intf + 1)) {
        return 0;
    }

    memset(bluetooth_class, 0, sizeof(struct usbh_bluetooth));

    bluetooth_class->hport = hport;
    bluetooth_class->intf = intf;
    bluetooth_class->num_of_intf_altsettings = hport->config.intf[intf + 1].altsetting_num;

    hport->config.intf[intf].priv = bluetooth_class;

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;

        if (USB_GET_ENDPOINT_TYPE(ep_desc->bmAttributes) == USB_ENDPOINT_TYPE_INTERRUPT) {
            if (ep_desc->bEndpointAddress & 0x80) {
                USBH_EP_INIT(bluetooth_class->intin, ep_desc);
            } else {
                return -USB_ERR_NOTSUPP;
            }
        } else {
            if (ep_desc->bEndpointAddress & 0x80) {
                USBH_EP_INIT(bluetooth_class->bulkin, ep_desc);
            } else {
                USBH_EP_INIT(bluetooth_class->bulkout, ep_desc);
            }
        }
    }

    USB_LOG_INFO("Num of altsettings:%u\r\n", bluetooth_class->num_of_intf_altsettings);

    for (uint8_t i = 0; i < bluetooth_class->num_of_intf_altsettings; i++) {
        USB_LOG_INFO("Altsetting:%u\r\n", i);
        for (uint8_t j = 0; j < hport->config.intf[intf + 1].altsetting[i].intf_desc.bNumEndpoints; j++) {
            ep_desc = &bluetooth_class->hport->config.intf[intf + 1].altsetting[i].ep[j].ep_desc;

            mult = USB_GET_MULT(ep_desc->wMaxPacketSize);
            mps = USB_GET_MAXPACKETSIZE(ep_desc->wMaxPacketSize);

            USB_LOG_INFO("\tEp=%02x Attr=%02u Mps=%d Interval=%02u Mult=%02u\r\n",
                         ep_desc->bEndpointAddress,
                         ep_desc->bmAttributes,
                         mps,
                         ep_desc->bInterval,
                         mult);
        }
    }

    ret = usbh_set_interface(hport, intf, 0);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("Bluetooth select altsetting 0\r\n");

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT);
    USB_LOG_INFO("Register Bluetooth Class:%s\r\n", hport->config.intf[intf].devname);
    usbh_bluetooth_run(bluetooth_class);
    return ret;
}

static int usbh_bluetooth_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_bluetooth *bluetooth_class = (struct usbh_bluetooth *)hport->config.intf[intf].priv;

    if (hport->config.config_desc.bNumInterfaces == (intf + 1)) {
        return 0;
    }

    if (bluetooth_class) {
        if (bluetooth_class->bulkin) {
            usbh_kill_urb(&bluetooth_class->bulkin_urb);
        }

        if (bluetooth_class->bulkout) {
            usbh_kill_urb(&bluetooth_class->bulkout_urb);
        }

        if (bluetooth_class->intin) {
            usbh_kill_urb(&bluetooth_class->intin_urb);
        }

        // if (bluetooth_class->isoin) {
        //     usbh_kill_urb(&bluetooth_class->isoin_urb);
        // }

        // if (bluetooth_class->isoin) {
        //     usbh_kill_urb(&bluetooth_class->isoinin_urb);
        // }

        if (hport->config.intf[intf].devname[0] != '\0') {
            USB_LOG_INFO("Unregister Bluetooth Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_bluetooth_stop(bluetooth_class);
        }

        memset(bluetooth_class, 0, sizeof(struct usbh_bluetooth));
    }

    return ret;
}

int usbh_bluetooth_hci_cmd(struct usbh_bluetooth *bluetooth_class, uint8_t *buffer, uint32_t buflen)
{
    struct usb_setup_packet *setup = bluetooth_class->hport->setup;

    uint16_t opcode = (((uint16_t)buffer[1] << 8) | (uint16_t)buffer[0]);

    USB_LOG_DBG("opcode:%04x, param len:%d\r\n", opcode, buffer[2]);

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = 0x00;
    setup->wValue = 0;
    setup->wIndex = bluetooth_class->intf;
    setup->wLength = buflen;

    memcpy(g_bluetooth_cmd_buf, buffer, buflen);
    return usbh_control_transfer(bluetooth_class->hport, setup, g_bluetooth_cmd_buf);
}

int usbh_bluetooth_hci_acl_out(struct usbh_bluetooth *bluetooth_class, uint8_t *buffer, uint32_t buflen)
{
    int ret;
    struct usbh_urb *urb = &bluetooth_class->bulkout_urb;

    usbh_bulk_urb_fill(urb, bluetooth_class->hport, bluetooth_class->bulkout, buffer, buflen, USB_OSAL_WAITING_FOREVER, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

void usbh_bluetooth_hci_event_rx_thread(void *argument)
{
    int ret;
    uint32_t ep_mps;
    uint32_t interval;
    uint8_t retry = 0;

    ep_mps = USB_GET_MAXPACKETSIZE(g_bluetooth_class.intin->wMaxPacketSize);
    interval = g_bluetooth_class.intin->bInterval;

    while (1) {
        usbh_int_urb_fill(&g_bluetooth_class.intin_urb, g_bluetooth_class.hport, g_bluetooth_class.intin, g_bluetooth_event_buf, ep_mps, USB_OSAL_WAITING_FOREVER, NULL, NULL);
        ret = usbh_submit_urb(&g_bluetooth_class.intin_urb);
        if (ret < 0) {
            if (ret == -USB_ERR_SHUTDOWN) {
                goto delete;
            } else if (ret == -USB_ERR_NAK) {
                usb_osal_msleep(interval);
                continue;
            } else {
                retry++;
                if (retry == 3) {
                    retry = 0;
                    goto delete;
                }
                usb_osal_msleep(interval);
                continue;
            }
        }
        usbh_bluetooth_hci_rx_callback(USB_BLUETOOTH_HCI_EVT, g_bluetooth_event_buf, g_bluetooth_class.intin_urb.actual_length);
        usb_osal_msleep(interval);
    }
    // clang-format off
delete : USB_LOG_INFO("Delete hc event rx thread\r\n");
    usb_osal_thread_delete(NULL);
    // clang-format om
}

void usbh_bluetooth_hci_acl_rx_thread(void *argument)
{
    int ret;
    uint32_t ep_mps;
    uint8_t retry = 0;

    ep_mps = USB_GET_MAXPACKETSIZE(g_bluetooth_class.bulkin->wMaxPacketSize);

    while (1) {
        usbh_bulk_urb_fill(&g_bluetooth_class.bulkin_urb, g_bluetooth_class.hport, g_bluetooth_class.bulkin, g_bluetooth_acl_buf, ep_mps, USB_OSAL_WAITING_FOREVER, NULL, NULL);
        ret = usbh_submit_urb(&g_bluetooth_class.bulkin_urb);
        if (ret < 0) {
            if (ret == -USB_ERR_SHUTDOWN) {
                goto delete;
            } else {
                retry++;
                if (retry == 3) {
                    retry = 0;
                    goto delete;
                }
                continue;
            }
        }
        usbh_bluetooth_hci_rx_callback(USB_BLUETOOTH_HCI_ACL, g_bluetooth_acl_buf, g_bluetooth_class.bulkin_urb.actual_length);
    }
    // clang-format off
delete : USB_LOG_INFO("Delete hc acl rx thread\r\n");
    usb_osal_thread_delete(NULL);
    // clang-format om
}

__WEAK void usbh_bluetooth_hci_rx_callback(uint8_t hci_type, uint8_t *data, uint32_t len)
{
}

__WEAK void usbh_bluetooth_run(struct usbh_bluetooth *bluetooth_class)
{
}

__WEAK void usbh_bluetooth_stop(struct usbh_bluetooth *bluetooth_class)
{
}

static const struct usbh_class_driver bluetooth_class_driver = {
    .driver_name = "bluetooth",
    .connect = usbh_bluetooth_connect,
    .disconnect = usbh_bluetooth_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info bluetooth_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_WIRELESS,
    .subclass = 0x01,
    .protocol = 0x01,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &bluetooth_class_driver
};
