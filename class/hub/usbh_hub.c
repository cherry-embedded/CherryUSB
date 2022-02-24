/**
 * @file usbh_hub.c
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#include "usbh_core.h"
#include "usbh_hub.h"

#define DEV_FORMAT "/dev/hub%d"

static uint32_t g_devinuse = 0;

usb_slist_t hub_class_head = USB_SLIST_OBJECT_INIT(hub_class_head);

USB_NOCACHE_RAM_SECTION uint8_t int_buffer[6][USBH_HUB_INTIN_BUFSIZE];
extern void usbh_external_hport_connect(struct usbh_hubport *hport);
extern void usbh_external_hport_disconnect(struct usbh_hubport *hport);
extern void usbh_hport_activate(struct usbh_hubport *hport);
extern void usbh_hport_deactivate(struct usbh_hubport *hport);

static void usbh_external_hub_callback(void *arg, int nbytes);

static inline void usbh_hub_register(struct usbh_hub *hub)
{
    usb_slist_add_tail(&hub_class_head, &hub->list);
}

static inline void usbh_hub_unregister(struct usbh_hub *hub)
{
    usb_slist_remove(&hub_class_head, &hub->list);
}

/****************************************************************************
 * Name: usbh_hub_devno_alloc
 *
 * Description:
 *   Allocate a unique /dev/hub[n] minor number in the range 2-31.
 *
 ****************************************************************************/

static int usbh_hub_devno_alloc(struct usbh_hub *hub)
{
    uint32_t flags;
    int devno;

    flags = usb_osal_enter_critical_section();
    for (devno = 2; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            hub->index = devno;
            usb_osal_leave_critical_section(flags);
            return 0;
        }
    }

    usb_osal_leave_critical_section(flags);
    return -EMFILE;
}

/****************************************************************************
 * Name: usbh_hub_devno_free
 *
 * Description:
 *   Free a /dev/hub[n] minor number so that it can be used.
 *
 ****************************************************************************/

static void usbh_hub_devno_free(struct usbh_hub *hub)
{
    int devno = hub->index;

    if (devno >= 2 && devno < 32) {
        uint32_t flags = usb_osal_enter_critical_section();
        g_devinuse &= ~(1 << devno);
        usb_osal_leave_critical_section(flags);
    }
}

int usbh_hub_get_hub_descriptor(struct usbh_hub *hub, uint8_t *buffer)
{
    struct usb_setup_packet *setup;

    setup = hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = HUB_DESCRIPTOR_TYPE_HUB << 8;
    setup->wIndex = 0;
    setup->wLength = USB_SIZEOF_HUB_DESC;

    return usbh_control_transfer(hub->parent->ep0, setup, buffer);
}

int usbh_hub_get_status(struct usbh_hub *hub, uint8_t *buffer)
{
    struct usb_setup_packet *setup;

    setup = hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = HUB_REQUEST_GET_STATUS;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 2;

    return usbh_control_transfer(hub->parent->ep0, setup, buffer);
}

int usbh_hub_get_portstatus(struct usbh_hub *hub, uint8_t port, struct hub_port_status *port_status)
{
    struct usb_setup_packet *setup;

    setup = hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
    setup->bRequest = HUB_REQUEST_GET_STATUS;
    setup->wValue = 0;
    setup->wIndex = port;
    setup->wLength = 4;

    return usbh_control_transfer(hub->parent->ep0, setup, (uint8_t *)port_status);
}

int usbh_hub_set_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature)
{
    struct usb_setup_packet *setup;

    setup = hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
    setup->bRequest = HUB_REQUEST_SET_FEATURE;
    setup->wValue = feature;
    setup->wIndex = port;
    setup->wLength = 0;

    return usbh_control_transfer(hub->parent->ep0, setup, NULL);
}

int usbh_hub_clear_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature)
{
    struct usb_setup_packet *setup;

    setup = hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
    setup->bRequest = HUB_REQUEST_CLEAR_FEATURE;
    setup->wValue = feature;
    setup->wIndex = port;
    setup->wLength = 0;

    return usbh_control_transfer(hub->parent->ep0, setup, NULL);
}

static int parse_hub_descriptor(struct usb_hub_descriptor *desc, uint16_t length)
{
    if (desc->bLength != USB_SIZEOF_HUB_DESC) {
        USB_LOG_ERR("invalid device bLength 0x%02x\r\n", desc->bLength);
        return -1;
    } else if (desc->bDescriptorType != HUB_DESCRIPTOR_TYPE_HUB) {
        USB_LOG_ERR("unexpected descriptor 0x%02x\r\n", desc->bDescriptorType);
        return -2;
    } else {
        USB_LOG_INFO("Device Descriptor:\r\n");
        USB_LOG_INFO("bLength: 0x%02x             \r\n", desc->bLength);
        USB_LOG_INFO("bDescriptorType: 0x%02x     \r\n", desc->bDescriptorType);
        USB_LOG_INFO("bNbrPorts: 0x%02x           \r\n", desc->bNbrPorts);
        USB_LOG_INFO("wHubCharacteristics: 0x%04x \r\n", desc->wHubCharacteristics);
        USB_LOG_INFO("bPwrOn2PwrGood: 0x%02x      \r\n", desc->bPwrOn2PwrGood);
        USB_LOG_INFO("bHubContrCurrent: 0x%02x    \r\n", desc->bHubContrCurrent);
        USB_LOG_INFO("DeviceRemovable: 0x%02x     \r\n", desc->DeviceRemovable);
        USB_LOG_INFO("PortPwrCtrlMask: 0x%02x     \r\n", desc->PortPwrCtrlMask);
    }
    return 0;
}

int usbh_hub_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_hub *hub_class = usb_malloc(sizeof(struct usbh_hub));
    if (hub_class == NULL) {
        USB_LOG_ERR("Fail to alloc hub_class\r\n");
        return -ENOMEM;
    }

    memset(hub_class, 0, sizeof(struct usbh_hub));

    hub_class->port_status = usb_iomalloc(sizeof(struct hub_port_status));
    if (hub_class->port_status == NULL) {
        USB_LOG_ERR("Fail to alloc port_status\r\n");
        return -ENOMEM;
    }

    usbh_hub_devno_alloc(hub_class);
    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, hub_class->index);

    hport->config.intf[intf].priv = hub_class;
    hub_class->dev_addr = hport->dev_addr;
    hub_class->parent = hport;

    uint8_t *hub_desc_buffer = usb_iomalloc(32);

    ret = usbh_hub_get_hub_descriptor(hub_class, hub_desc_buffer);
    if (ret != 0) {
        usb_iofree(hub_desc_buffer);
        return ret;
    }

    parse_hub_descriptor((struct usb_hub_descriptor *)hub_desc_buffer, USB_SIZEOF_HUB_DESC);
    memcpy(&hub_class->hub_desc, hub_desc_buffer, USB_SIZEOF_HUB_DESC);
    usb_iofree(hub_desc_buffer);

    hub_class->nports = hub_class->hub_desc.bNbrPorts;

    for (uint8_t port = 1; port <= hub_class->nports; port++) {
        hub_class->child[port - 1].port = port;
        hub_class->child[port - 1].parent = hub_class;
    }

    hub_class->int_buffer = int_buffer[hub_class->index - 2];
    usbh_hub_register(hub_class);

    ep_desc = &hport->config.intf[intf].ep[0].ep_desc;
    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.hport = hport;
    if (ep_desc->bEndpointAddress & 0x80) {
        usbh_ep_alloc(&hub_class->intin, &ep_cfg);
    } else {
        return -1;
    }

    for (uint8_t port = 1; port <= hub_class->nports; port++) {
        ret = usbh_hub_set_feature(hub_class, 1, HUB_PORT_FEATURE_POWER);
        if (ret < 0) {
            return ret;
        }
    }

    for (uint8_t port = 1; port <= hub_class->nports; port++) {
        ret = usbh_hub_get_portstatus(hub_class, port, hub_class->port_status);
        USB_LOG_INFO("Port:%d, status:0x%02x, change:0x%02x\r\n", port, hub_class->port_status->wPortStatus, hub_class->port_status->wPortChange);
        if (ret < 0) {
            return ret;
        }
    }

    USB_LOG_INFO("Register HUB Class:%s\r\n", hport->config.intf[intf].devname);

    ret = usbh_ep_intr_async_transfer(hub_class->intin, hub_class->int_buffer, USBH_HUB_INTIN_BUFSIZE, usbh_external_hub_callback, hub_class);
    return 0;
}

int usbh_hub_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_hubport *child;
    int ret = 0;

    struct usbh_hub *hub_class = (struct usbh_hub *)hport->config.intf[intf].priv;

    if (hub_class) {
        usbh_hub_devno_free(hub_class);

        if (hub_class->intin) {
            ret = usb_ep_cancel(hub_class->intin);
            if (ret < 0) {
            }
            usbh_ep_free(hub_class->intin);
        }

        if (hub_class->port_status)
            usb_iofree(hub_class->port_status);

        for (uint8_t port = 1; port <= hub_class->nports; port++) {
            child = &hub_class->child[port - 1];
            usbh_hport_deactivate(child);
            for (uint8_t i = 0; i < child->config.config_desc.bNumInterfaces; i++) {
                if (child->config.intf[i].class_driver && child->config.intf[i].class_driver->disconnect) {
                    ret = CLASS_DISCONNECT(child, i);
                }
            }

            child->config.config_desc.bNumInterfaces = 0;
            child->parent = NULL;
        }

        usbh_hub_unregister(hub_class);
        usb_free(hub_class);

        USB_LOG_INFO("Unregister HUB Class:%s\r\n", hport->config.intf[intf].devname);

        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);
        hport->config.intf[intf].priv = NULL;
    }
    return ret;
}

static void usbh_extern_hub_psc_event(void *arg)
{
    struct usbh_hub *hub_class;
    struct usbh_hubport *connport;
    uint8_t port_change;
    uint16_t status;
    uint16_t change;
    uint16_t mask;
    uint16_t feat;
    uint32_t flags;
    int ret;

    hub_class = (struct usbh_hub *)arg;

    /* Has the hub been disconnected? */
    if (!hub_class->parent->connected) {
        return;
    }

    port_change = hub_class->int_buffer[0];
    USB_LOG_DBG("port_change:0x%02x\r\n", port_change);

    /* Check for status change on any port */
    for (uint8_t port = USBH_HUB_PORT_START_INDEX; port <= hub_class->nports; port++) {
        /* Check if port status has changed */
        if ((port_change & (1 << port)) == 0) {
            continue;
        }
        USB_LOG_DBG("Port %d change\r\n", port);

        /* Port status changed, check what happened */
        port_change &= ~(1 << port);

        /* Read hub port status */
        ret = usbh_hub_get_portstatus(hub_class, port, hub_class->port_status);
        if (ret < 0) {
            USB_LOG_ERR("Failed to read port:%d status, errorcode: %d\r\n", port, ret);
            continue;
        }
        status = hub_class->port_status->wPortStatus;
        change = hub_class->port_status->wPortChange;

        USB_LOG_DBG("Port:%d, status:0x%02x, change:0x%02x\r\n", port, status, change);

        /* First, clear all change bits */
        mask = 1;
        feat = HUB_PORT_FEATURE_C_CONNECTION;
        while (change) {
            if (change & mask) {
                ret = usbh_hub_clear_feature(hub_class, port, feat);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to clear port:%d, change mask:%04x, errorcode:%d\r\n", port, mask, ret);
                }
                change &= (~mask);
            }
            mask <<= 1;
            feat++;
        }

        change = hub_class->port_status->wPortChange;

        /* Handle connect or disconnect, no power management */
        if (change & HUB_PORT_STATUS_C_CONNECTION) {
            uint16_t debouncetime = 0;
            uint16_t debouncestable = 0;
            uint16_t connection = 0xffff;

            /* Debounce */
            while (debouncetime < 1500) {
                ret = usbh_hub_get_portstatus(hub_class, port, hub_class->port_status);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to read port:%d status, errorcode: %d\r\n", port, ret);
                    break;
                }
                status = hub_class->port_status->wPortStatus;
                change = hub_class->port_status->wPortChange;

                if ((change & HUB_PORT_STATUS_C_CONNECTION) == 0 &&
                    (status & HUB_PORT_STATUS_CONNECTION) == connection) {
                    debouncestable += 25;
                    if (debouncestable >= 100) {
                        USB_LOG_DBG("Port %d debouncestable=%d\r\n",
                                    port, debouncestable);
                        break;
                    }
                } else {
                    debouncestable = 0;
                    connection = status & HUB_PORT_STATUS_CONNECTION;
                }

                if ((change & HUB_PORT_STATUS_C_CONNECTION) != 0) {
                    ret = usbh_hub_clear_feature(hub_class, port, HUB_PORT_FEATURE_C_CONNECTION);
                    if (ret < 0) {
                        USB_LOG_ERR("Failed to clear port:%d, change mask:%04x, errorcode:%d\r\n", port, mask, ret);
                    }
                }
                debouncetime += 25;
                usb_osal_msleep(25);
            }

            if (ret < 0 || debouncetime >= 1500) {
                USB_LOG_ERR("ERROR: Failed to debounce port %d: %d\r\n", port, ret);
                continue;
            }

            if (status & HUB_PORT_STATUS_CONNECTION) {
                /* Device connected to a port on the hub */
                USB_LOG_DBG("Connection on port:%d\n", port);

                ret = usbh_hub_set_feature(hub_class, port, HUB_PORT_FEATURE_RESET);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to reset port:%d,errorcode:%d\r\n", port, ret);
                    continue;
                }

                usb_osal_msleep(100);

                ret = usbh_hub_get_portstatus(hub_class, port, hub_class->port_status);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to read port:%d status, errorcode: %d\r\n", port, ret);
                    continue;
                }
                status = hub_class->port_status->wPortStatus;
                change = hub_class->port_status->wPortChange;

                USB_LOG_DBG("Port:%d, status:0x%02x, change:0x%02x after reset\r\n", port, status, change);

                if ((status & HUB_PORT_STATUS_RESET) == 0 && (status & HUB_PORT_STATUS_ENABLE) != 0) {
                    if (change & HUB_PORT_STATUS_C_RESET) {
                        ret = usbh_hub_clear_feature(hub_class, port, HUB_PORT_FEATURE_C_RESET);
                        if (ret < 0) {
                            USB_LOG_ERR("Failed to clear port:%d reset change, errorcode: %d\r\n", port, ret);
                        }
                    }
                    connport = &hub_class->child[port - 1];

                    if (status & HUB_PORT_STATUS_HIGH_SPEED) {
                        connport->speed = USB_SPEED_HIGH;
                    } else if (status & HUB_PORT_STATUS_LOW_SPEED) {
                        connport->speed = USB_SPEED_LOW;
                    } else {
                        connport->speed = USB_SPEED_FULL;
                    }

                    /* Device connected from a port on the hub, wakeup psc thread. */
                    usbh_external_hport_connect(connport);

                } else {
                    USB_LOG_ERR("Failed to enable port:%d\r\n", port);
                    continue;
                }
            } else {
                /* Device disconnected from a port on the hub, wakeup psc thread. */
                connport = &hub_class->child[port - 1];
                usbh_external_hport_disconnect(connport);
            }
        } else {
            USB_LOG_WRN("status %04x change %04x not handled\r\n", status, change);
        }
    }

    /* Check for hub status change */

    if ((port_change & 1) != 0) {
        /* Hub status changed */
        USB_LOG_WRN("Hub status changed, not handled\n");
    }
    flags = usb_osal_enter_critical_section();
    if (hub_class->parent->connected) {
        ret = usbh_ep_intr_async_transfer(hub_class->intin, hub_class->int_buffer, USBH_HUB_INTIN_BUFSIZE, usbh_external_hub_callback, hub_class);
    }
    usb_osal_leave_critical_section(flags);
}

static void usbh_external_hub_callback(void *arg, int nbytes)
{
    struct usbh_hub *hub_class = (struct usbh_hub *)arg;
    uint32_t delay = 0;
    if (nbytes < 0) {
        hub_class->int_buffer[0] = 0;
        delay = 100;
    }
    if (hub_class->parent->connected) {
        usb_workqueue_submit(&g_lpworkq, &hub_class->work, usbh_extern_hub_psc_event, (void *)hub_class, delay);
    }
}

const struct usbh_class_driver hub_class_driver = {
    .driver_name = "hub",
    .connect = usbh_hub_connect,
    .disconnect = usbh_hub_disconnect
};