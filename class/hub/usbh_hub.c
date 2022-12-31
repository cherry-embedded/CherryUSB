/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_hub.h"

#define DEV_FORMAT "/dev/hub%d"

#define HUB_DEBOUNCE_TIMEOUT   1500
#define HUB_DEBOUNCE_STEP      25
#define HUB_DEBOUNCE_STABLE    100
#define DELAY_TIME_AFTER_RESET 200

#define EXTHUB_FIRST_INDEX 2

static uint32_t g_devinuse = 0;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_hub_buf[32];

usb_slist_t hub_class_head = USB_SLIST_OBJECT_INIT(hub_class_head);

usb_osal_thread_t hub_thread;
usb_osal_mq_t hub_mq;

USB_NOCACHE_RAM_SECTION struct usbh_hub roothub;

#if CONFIG_USBHOST_MAX_EXTHUBS > 0
USB_NOCACHE_RAM_SECTION struct usbh_hub exthub[CONFIG_USBHOST_MAX_EXTHUBS];
#endif
extern int usbh_hport_activate_ep0(struct usbh_hubport *hport);
extern int usbh_hport_deactivate_ep0(struct usbh_hubport *hport);
extern int usbh_enumerate(struct usbh_hubport *hport);
static void usbh_hub_thread_wakeup(struct usbh_hub *hub);

static const char *speed_table[] = { "error-speed", "low-speed", "full-speed", "high-speed", "wireless-speed", "super-speed", "superplus-speed" };

#if CONFIG_USBHOST_MAX_EXTHUBS > 0
static int usbh_hub_devno_alloc(void)
{
    int devno;

    for (devno = EXTHUB_FIRST_INDEX; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            return devno;
        }
    }

    return -EMFILE;
}

static void usbh_hub_devno_free(uint8_t devno)
{
    if (devno >= EXTHUB_FIRST_INDEX && devno < 32) {
        g_devinuse &= ~(1 << devno);
    }
}

static void usbh_hub_register(struct usbh_hub *hub)
{
    usb_slist_add_tail(&hub_class_head, &hub->list);
}

static void usbh_hub_unregister(struct usbh_hub *hub)
{
    usb_slist_remove(&hub_class_head, &hub->list);
}

#endif
static int _usbh_hub_get_hub_descriptor(struct usbh_hub *hub, uint8_t *buffer)
{
    struct usb_setup_packet *setup;
    int ret;

    setup = &hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = HUB_DESCRIPTOR_TYPE_HUB << 8;
    setup->wIndex = 0;
    setup->wLength = USB_SIZEOF_HUB_DESC;

    ret = usbh_control_transfer(hub->parent->ep0, setup, g_hub_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(buffer, g_hub_buf, USB_SIZEOF_HUB_DESC);
    return ret;
}

static int _usbh_hub_get_status(struct usbh_hub *hub, uint8_t *buffer)
{
    struct usb_setup_packet *setup;
    int ret;

    setup = &hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = HUB_REQUEST_GET_STATUS;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 2;

    ret = usbh_control_transfer(hub->parent->ep0, setup, g_hub_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(buffer, g_hub_buf, 2);
    return ret;
}

static int _usbh_hub_get_portstatus(struct usbh_hub *hub, uint8_t port, struct hub_port_status *port_status)
{
    struct usb_setup_packet *setup;
    int ret;

    setup = &hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
    setup->bRequest = HUB_REQUEST_GET_STATUS;
    setup->wValue = 0;
    setup->wIndex = port;
    setup->wLength = 4;

    ret = usbh_control_transfer(hub->parent->ep0, setup, g_hub_buf);
    if (ret < 0) {
        return ret;
    }
    memcpy(port_status, g_hub_buf, 4);
    return ret;
}

static int _usbh_hub_set_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature)
{
    struct usb_setup_packet *setup;

    setup = &hub->parent->setup;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
    setup->bRequest = HUB_REQUEST_SET_FEATURE;
    setup->wValue = feature;
    setup->wIndex = port;
    setup->wLength = 0;

    return usbh_control_transfer(hub->parent->ep0, setup, NULL);
}

static int _usbh_hub_clear_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature)
{
    struct usb_setup_packet *setup;

    setup = &hub->parent->setup;

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
        USB_LOG_RAW("Hub Descriptor:\r\n");
        USB_LOG_RAW("bLength: 0x%02x             \r\n", desc->bLength);
        USB_LOG_RAW("bDescriptorType: 0x%02x     \r\n", desc->bDescriptorType);
        USB_LOG_RAW("bNbrPorts: 0x%02x           \r\n", desc->bNbrPorts);
        USB_LOG_RAW("wHubCharacteristics: 0x%04x \r\n", desc->wHubCharacteristics);
        USB_LOG_RAW("bPwrOn2PwrGood: 0x%02x      \r\n", desc->bPwrOn2PwrGood);
        USB_LOG_RAW("bHubContrCurrent: 0x%02x    \r\n", desc->bHubContrCurrent);
        USB_LOG_RAW("DeviceRemovable: 0x%02x     \r\n", desc->DeviceRemovable);
        USB_LOG_RAW("PortPwrCtrlMask: 0x%02x     \r\n", desc->PortPwrCtrlMask);
    }
    return 0;
}

static int usbh_hub_get_portstatus(struct usbh_hub *hub, uint8_t port, struct hub_port_status *port_status)
{
    struct usb_setup_packet roothub_setup;
    struct usb_setup_packet *setup;

    if (hub->is_roothub) {
        setup = &roothub_setup;
        setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
        setup->bRequest = HUB_REQUEST_GET_STATUS;
        setup->wValue = 0;
        setup->wIndex = port;
        setup->wLength = 4;
        return usbh_roothub_control(&roothub_setup, (uint8_t *)port_status);
    } else {
        return _usbh_hub_get_portstatus(hub, port, port_status);
    }
}

static int usbh_hub_set_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature)
{
    struct usb_setup_packet roothub_setup;
    struct usb_setup_packet *setup;

    if (hub->is_roothub) {
        setup = &roothub_setup;
        setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
        setup->bRequest = HUB_REQUEST_SET_FEATURE;
        setup->wValue = feature;
        setup->wIndex = port;
        setup->wLength = 0;
        return usbh_roothub_control(setup, NULL);
    } else {
        return _usbh_hub_set_feature(hub, port, feature);
    }
}

static int usbh_hub_clear_feature(struct usbh_hub *hub, uint8_t port, uint8_t feature)
{
    struct usb_setup_packet roothub_setup;
    struct usb_setup_packet *setup;

    if (hub->is_roothub) {
        setup = &roothub_setup;
        setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_OTHER;
        setup->bRequest = HUB_REQUEST_CLEAR_FEATURE;
        setup->wValue = feature;
        setup->wIndex = port;
        setup->wLength = 0;
        return usbh_roothub_control(setup, NULL);
    } else {
        return _usbh_hub_clear_feature(hub, port, feature);
    }
}

#if CONFIG_USBHOST_MAX_EXTHUBS > 0
static void hub_int_complete_callback(void *arg, int nbytes)
{
    struct usbh_hub *hub = (struct usbh_hub *)arg;

    if (nbytes > 0) {
        usbh_hub_thread_wakeup(hub);
    }
}

static int usbh_hub_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    struct hub_port_status port_status;
    int ret;
    int index;

    index = usbh_hub_devno_alloc();
    if (index > (CONFIG_USBHOST_MAX_EXTHUBS + EXTHUB_FIRST_INDEX - 1)) {
        USB_LOG_ERR("No memory to alloc hub class\r\n");
        usbh_hub_devno_free(index);
        return -ENOMEM;
    }

    struct usbh_hub *hub = &exthub[index - EXTHUB_FIRST_INDEX];

    memset(hub, 0, sizeof(struct usbh_hub));
    hub->hub_addr = hport->dev_addr;
    hub->parent = hport;
    hub->index = index;

    hport->config.intf[intf].priv = hub;

    ret = _usbh_hub_get_hub_descriptor(hub, (uint8_t *)&hub->hub_desc);
    if (ret < 0) {
        return ret;
    }

    parse_hub_descriptor(&hub->hub_desc, USB_SIZEOF_HUB_DESC);

    for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
        hub->child[port].port = port + 1;
        hub->child[port].parent = hub;
    }

    ep_desc = &hport->config.intf[intf].altsetting[0].ep[0].ep_desc;
    if (ep_desc->bEndpointAddress & 0x80) {
        usbh_hport_activate_epx(&hub->intin, hport, ep_desc);
    } else {
        return -1;
    }

    for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
        ret = usbh_hub_set_feature(hub, port + 1, HUB_PORT_FEATURE_POWER);
        if (ret < 0) {
            return ret;
        }
    }

    for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
        ret = usbh_hub_get_portstatus(hub, port + 1, &port_status);
        USB_LOG_INFO("port %u, status:0x%02x, change:0x%02x\r\n", port + 1, port_status.wPortStatus, port_status.wPortChange);
        if (ret < 0) {
            return ret;
        }
    }

    hub->connected = true;
    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, hub->index);
    usbh_hub_register(hub);
    USB_LOG_INFO("Register HUB Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_int_urb_fill(&hub->intin_urb, hub->intin, hub->int_buffer, 1, 0, hub_int_complete_callback, hub);
    usbh_submit_urb(&hub->intin_urb);
    return 0;
}

static int usbh_hub_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_hubport *child;
    int ret = 0;

    struct usbh_hub *hub = (struct usbh_hub *)hport->config.intf[intf].priv;

    if (hub) {
        usbh_hub_devno_free(hub->index);

        if (hub->intin) {
            usbh_pipe_free(hub->intin);
        }

        for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
            child = &hub->child[port];
            usbh_hport_deactivate_ep0(child);
            for (uint8_t i = 0; i < child->config.config_desc.bNumInterfaces; i++) {
                if (child->config.intf[i].class_driver && child->config.intf[i].class_driver->disconnect) {
                    CLASS_DISCONNECT(child, i);
                }
            }

            child->config.config_desc.bNumInterfaces = 0;
            child->parent = NULL;
        }

        usbh_hub_unregister(hub);
        memset(hub, 0, sizeof(struct usbh_hub));

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister HUB Class:%s\r\n", hport->config.intf[intf].devname);
    }
    return ret;
}
#endif

static void usbh_hubport_release(struct usbh_hubport *child)
{
    if (child->connected) {
        child->connected = false;
        usbh_hport_deactivate_ep0(child);
        for (uint8_t i = 0; i < child->config.config_desc.bNumInterfaces; i++) {
            if (child->config.intf[i].class_driver && child->config.intf[i].class_driver->disconnect) {
                CLASS_DISCONNECT(child, i);
            }
        }
        child->config.config_desc.bNumInterfaces = 0;
    }
}

static void usbh_hub_events(struct usbh_hub *hub)
{
    struct usbh_hubport *child;
    struct hub_port_status port_status;
    uint8_t portchange_index;
    uint16_t portstatus;
    uint16_t portchange;
    uint16_t mask;
    uint16_t feat;
    uint8_t speed;
    int ret;

    if (!hub->connected) {
        return;
    }

    for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
        portchange_index = hub->int_buffer[0];

        USB_LOG_DBG("Port change:0x%02x\r\n", portchange_index);

        if (!(portchange_index & (1 << (port + 1)))) {
            continue;
        }
        portchange_index &= ~(1 << (port + 1));
        USB_LOG_DBG("Port %d change\r\n", port + 1);

        /* Read hub port status */
        ret = usbh_hub_get_portstatus(hub, port + 1, &port_status);
        if (ret < 0) {
            USB_LOG_ERR("Failed to read port %u status, errorcode: %d\r\n", port + 1, ret);
            continue;
        }

        portstatus = port_status.wPortStatus;
        portchange = port_status.wPortChange;

        USB_LOG_DBG("port %u, status:0x%02x, change:0x%02x\r\n", port + 1, portstatus, portchange);

        /* First, clear all change bits */
        mask = 1;
        feat = HUB_PORT_FEATURE_C_CONNECTION;
        while (portchange) {
            if (portchange & mask) {
                ret = usbh_hub_clear_feature(hub, port + 1, feat);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to clear port %u, change mask:%04x, errorcode:%d\r\n", port + 1, mask, ret);
                    continue;
                }
                portchange &= (~mask);
            }
            mask <<= 1;
            feat++;
        }

        portchange = port_status.wPortChange;

        /* Second, if port changes, debounces first */
        if (portchange & HUB_PORT_STATUS_C_CONNECTION) {
            uint16_t connection = 0;
            uint16_t debouncestable = 0;
            for (uint32_t debouncetime = 0; debouncetime < HUB_DEBOUNCE_TIMEOUT; debouncetime += HUB_DEBOUNCE_STEP) {
                /* Read hub port status */
                ret = usbh_hub_get_portstatus(hub, port + 1, &port_status);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to read port %u status, errorcode: %d\r\n", port + 1, ret);
                    continue;
                }

                portstatus = port_status.wPortStatus;
                portchange = port_status.wPortChange;

                USB_LOG_DBG("Port %u, status:0x%02x, change:0x%02x\r\n", port + 1, portstatus, portchange);

                if (!(portchange & HUB_PORT_STATUS_C_CONNECTION) &&
                    ((portstatus & HUB_PORT_STATUS_CONNECTION) == connection)) {
                    debouncestable += HUB_DEBOUNCE_STEP;
                    if (debouncestable >= HUB_DEBOUNCE_STABLE) {
                        break;
                    }
                } else {
                    debouncestable = 0;
                    connection = portstatus & HUB_PORT_STATUS_CONNECTION;
                }

                if (portchange & HUB_PORT_STATUS_C_CONNECTION) {
                    usbh_hub_clear_feature(hub, port + 1, HUB_PORT_FEATURE_C_CONNECTION);
                }

                usb_osal_msleep(HUB_DEBOUNCE_STEP);
            }

            /** check if debounce ok */
            if (debouncestable < HUB_DEBOUNCE_STABLE) {
                USB_LOG_ERR("Failed to debounce port %u\r\n", port + 1);
                break;
            }

            /* Last, check connect status */
            if (portstatus & HUB_PORT_STATUS_CONNECTION) {
                ret = usbh_hub_set_feature(hub, port + 1, HUB_PORT_FEATURE_RESET);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to reset port %u,errorcode:%d\r\n", port, ret);
                    continue;
                }

                usb_osal_msleep(DELAY_TIME_AFTER_RESET);
                /* Read hub port status */
                ret = usbh_hub_get_portstatus(hub, port + 1, &port_status);
                if (ret < 0) {
                    USB_LOG_ERR("Failed to read port %u status, errorcode: %d\r\n", port + 1, ret);
                    continue;
                }

                portstatus = port_status.wPortStatus;
                portchange = port_status.wPortChange;
                if (!(portstatus & HUB_PORT_STATUS_RESET) && (portstatus & HUB_PORT_STATUS_ENABLE)) {
                    if (portchange & HUB_PORT_STATUS_C_RESET) {
                        ret = usbh_hub_clear_feature(hub, port + 1, HUB_PORT_FEATURE_C_RESET);
                        if (ret < 0) {
                            USB_LOG_ERR("Failed to clear port %u reset change, errorcode: %d\r\n", port, ret);
                        }
                    }

                    if (portstatus & HUB_PORT_STATUS_HIGH_SPEED) {
                        speed = USB_SPEED_HIGH;
                    } else if (portstatus & HUB_PORT_STATUS_LOW_SPEED) {
                        speed = USB_SPEED_LOW;
                    } else {
                        speed = USB_SPEED_FULL;
                    }

                    child = &hub->child[port];
                    /** release child sources first */
                    usbh_hubport_release(child);

                    memset(child, 0, sizeof(struct usbh_hubport));
                    child->parent = hub;
                    child->connected = true;
                    child->port = port + 1;
                    child->speed = speed;

                    USB_LOG_INFO("New %s device on Hub %u, Port %u connected\r\n", speed_table[speed], hub->index, port + 1);

                    if (usbh_enumerate(child) < 0) {
                        USB_LOG_ERR("Port %u enumerate fail\r\n", port + 1);
                    }
                } else {
                    USB_LOG_ERR("Failed to enable port %u\r\n", port + 1);

                    child = &hub->child[port];
                    /** release child sources */
                    usbh_hubport_release(child);

                    continue;
                }
            } else {
                child = &hub->child[port];
                /** release child sources */
                usbh_hubport_release(child);
            }
        }
    }

    hub->int_buffer[0] = 0;
    /* Start next hub int transfer */
    if (!hub->is_roothub && hub->connected) {
        usbh_submit_urb(&hub->intin_urb);
    }
}

static void usbh_hub_thread(void *argument)
{
    struct usbh_hub *hub;
    int ret = 0;

    usb_hc_init();
    while (1) {
        ret = usb_osal_mq_recv(hub_mq, (uint32_t *)&hub, 0xffffffff);
        if (ret < 0) {
            continue;
        }
        usbh_hub_events(hub);
    }
}

static void usbh_roothub_register(void)
{
    memset(&roothub, 0, sizeof(struct usbh_hub));

    roothub.connected = true;
    roothub.index = 1;
    roothub.is_roothub = true;
    roothub.parent = NULL;
    roothub.hub_addr = 1;
    roothub.hub_desc.bNbrPorts = CONFIG_USBHOST_MAX_RHPORTS;
    usbh_hub_register(&roothub);
}

static void usbh_hub_thread_wakeup(struct usbh_hub *hub)
{
    usb_osal_mq_send(hub_mq, (uint32_t)hub);
}

void usbh_roothub_thread_wakeup(uint8_t port)
{
    roothub.int_buffer[0] |= (1 << port);
    usbh_hub_thread_wakeup(&roothub);
}

int usbh_hub_initialize(void)
{
    usbh_roothub_register();

    hub_mq = usb_osal_mq_create(7);
    if (hub_mq == NULL) {
        return -1;
    }

    hub_thread = usb_osal_thread_create("usbh_hub", CONFIG_USBHOST_PSC_STACKSIZE, CONFIG_USBHOST_PSC_PRIO, usbh_hub_thread, NULL);
    if (hub_thread == NULL) {
        return -1;
    }
    return 0;
}
#if CONFIG_USBHOST_MAX_EXTHUBS > 0
const struct usbh_class_driver hub_class_driver = {
    .driver_name = "hub",
    .connect = usbh_hub_connect,
    .disconnect = usbh_hub_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info hub_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS,
    .class = USB_DEVICE_CLASS_HUB,
    .subclass = 0,
    .protocol = 0,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &hub_class_driver
};
#endif
