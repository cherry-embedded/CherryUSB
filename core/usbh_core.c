/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_hub.h"

struct usbh_class_info *usbh_class_info_table_begin = NULL;
struct usbh_class_info *usbh_class_info_table_end = NULL;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t ep0_request_buffer[CONFIG_USBHOST_REQUEST_BUFFER_LEN];

/* general descriptor field offsets */
#define DESC_bLength         0 /** Length offset */
#define DESC_bDescriptorType 1 /** Descriptor type offset */

#define USB_DEV_ADDR_MAX         0x7f
#define USB_DEV_ADDR_MARK_OFFSET 5
#define USB_DEV_ADDR_MARK_MASK   0x1f

struct usbh_devaddr_map {
    /**
     * alloctab[0]:addr from 0~31
     * alloctab[1]:addr from 32~63
     * alloctab[2]:addr from 64~95
     * alloctab[3]:addr from 96~127
     *
     */
    uint8_t next;         /* Next device address */
    uint32_t alloctab[4]; /* Bit allocation table */
};

struct usbh_bus {
    struct usbh_devaddr_map devgen;
} g_usbh_bus;

static int usbh_allocate_devaddr(struct usbh_devaddr_map *devgen)
{
    uint8_t startaddr = devgen->next;
    uint8_t devaddr;
    int index;
    int bitno;

    for (;;) {
        devaddr = devgen->next;
        if (devgen->next >= 0x7f) {
            devgen->next = 2;
        } else {
            devgen->next++;
        }

        index = devaddr >> 5;
        bitno = devaddr & 0x1f;
        if ((devgen->alloctab[index] & (1 << bitno)) == 0) {
            devgen->alloctab[index] |= (1 << bitno);
            return (int)devaddr;
        }

        if (startaddr == devaddr) {
            return -ENOMEM;
        }
    }
}

static int usbh_free_devaddr(struct usbh_devaddr_map *devgen, uint8_t devaddr)
{
    int index;
    int bitno;

    if ((devaddr > 0) && (devaddr < USB_DEV_ADDR_MAX)) {
        index = devaddr >> USB_DEV_ADDR_MARK_OFFSET;
        bitno = devaddr & USB_DEV_ADDR_MARK_MASK;

        /* Free the address  */
        if ((devgen->alloctab[index] |= (1 << bitno)) != 0) {
            devgen->alloctab[index] &= ~(1 << bitno);
        } else {
            return -1;
        }

        if (devaddr < devgen->next) {
            devgen->next = devaddr;
        }
    }

    return 0;
}

static const struct usbh_class_driver *usbh_find_class_driver(uint8_t class, uint8_t subclass, uint8_t protocol,
                                                              uint16_t vid, uint16_t pid)
{
    struct usbh_class_info *index = NULL;

    for (index = usbh_class_info_table_begin; index < usbh_class_info_table_end; index++) {
        if ((index->match_flags & (USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_PRODUCT | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL)) ==
            (USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_PRODUCT | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL)) {
            if (index->vid == vid && index->pid == pid &&
                index->class == class && index->subclass == subclass && index->protocol == protocol) {
                return index->class_driver;
            }
        } else if ((index->match_flags & (USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL)) ==
                   (USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL)) {
            if (index->class == class && index->subclass == subclass && index->protocol == protocol) {
                return index->class_driver;
            }
        } else if ((index->match_flags & (USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_PRODUCT | USB_CLASS_MATCH_INTF_CLASS)) ==
                   (USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_PRODUCT | USB_CLASS_MATCH_INTF_CLASS)) {
            if (index->vid == vid && index->pid == pid && index->class == class) {
                return index->class_driver;
            }
        } else if ((index->match_flags & (USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS)) == (USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS)) {
            if (index->class == class && index->subclass == subclass) {
                return index->class_driver;
            }
        } else if ((index->match_flags & (USB_CLASS_MATCH_INTF_CLASS)) == USB_CLASS_MATCH_INTF_CLASS) {
            if (index->class == class) {
                return index->class_driver;
            }
        }
    }
    return NULL;
}

static int parse_device_descriptor(struct usbh_hubport *hport, struct usb_device_descriptor *desc, uint16_t length)
{
    if (desc->bLength != USB_SIZEOF_DEVICE_DESC) {
        USB_LOG_ERR("invalid device bLength 0x%02x\r\n", desc->bLength);
        return -EINVAL;
    } else if (desc->bDescriptorType != USB_DESCRIPTOR_TYPE_DEVICE) {
        USB_LOG_ERR("unexpected device descriptor 0x%02x\r\n", desc->bDescriptorType);
        return -EINVAL;
    } else {
        if (length <= 8) {
            return 0;
        }
#if 0
        USB_LOG_DBG("Device Descriptor:\r\n");
        USB_LOG_DBG("bLength: 0x%02x           \r\n", desc->bLength);
        USB_LOG_DBG("bDescriptorType: 0x%02x   \r\n", desc->bDescriptorType);
        USB_LOG_DBG("bcdUSB: 0x%04x            \r\n", desc->bcdUSB);
        USB_LOG_DBG("bDeviceClass: 0x%02x      \r\n", desc->bDeviceClass);
        USB_LOG_DBG("bDeviceSubClass: 0x%02x   \r\n", desc->bDeviceSubClass);
        USB_LOG_DBG("bDeviceProtocol: 0x%02x   \r\n", desc->bDeviceProtocol);
        USB_LOG_DBG("bMaxPacketSize0: 0x%02x   \r\n", desc->bMaxPacketSize0);
        USB_LOG_DBG("idVendor: 0x%04x          \r\n", desc->idVendor);
        USB_LOG_DBG("idProduct: 0x%04x         \r\n", desc->idProduct);
        USB_LOG_DBG("bcdDevice: 0x%04x         \r\n", desc->bcdDevice);
        USB_LOG_DBG("iManufacturer: 0x%02x     \r\n", desc->iManufacturer);
        USB_LOG_DBG("iProduct: 0x%02x          \r\n", desc->iProduct);
        USB_LOG_DBG("iSerialNumber: 0x%02x     \r\n", desc->iSerialNumber);
        USB_LOG_DBG("bNumConfigurations: 0x%02x\r\n", desc->bNumConfigurations);
#endif
        hport->device_desc.bLength = desc->bLength;
        hport->device_desc.bDescriptorType = desc->bDescriptorType;
        hport->device_desc.bcdUSB = desc->bcdUSB;
        hport->device_desc.bDeviceClass = desc->bDeviceClass;
        hport->device_desc.bDeviceSubClass = desc->bDeviceSubClass;
        hport->device_desc.bDeviceProtocol = desc->bDeviceProtocol;
        hport->device_desc.bMaxPacketSize0 = desc->bMaxPacketSize0;
        hport->device_desc.idVendor = desc->idVendor;
        hport->device_desc.idProduct = desc->idProduct;
        hport->device_desc.bcdDevice = desc->bcdDevice;
        hport->device_desc.iManufacturer = desc->iManufacturer;
        hport->device_desc.iProduct = desc->iProduct;
        hport->device_desc.iSerialNumber = desc->iSerialNumber;
        hport->device_desc.bNumConfigurations = desc->bNumConfigurations;
    }
    return 0;
}

static int parse_config_descriptor(struct usbh_hubport *hport, struct usb_configuration_descriptor *desc, uint16_t length)
{
    struct usb_interface_descriptor *intf_desc;
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t cur_alt_setting = 0xff;
    uint8_t cur_iface = 0xff;
    uint8_t cur_ep = 0xff;
    uint8_t cur_ep_num = 0xff;
    uint32_t desc_len = 0;
    uint8_t *p;

    if (desc->bLength != USB_SIZEOF_CONFIG_DESC) {
        USB_LOG_ERR("invalid config bLength 0x%02x\r\n", desc->bLength);
        return -EINVAL;
    } else if (desc->bDescriptorType != USB_DESCRIPTOR_TYPE_CONFIGURATION) {
        USB_LOG_ERR("unexpected config descriptor 0x%02x\r\n", desc->bDescriptorType);
        return -EINVAL;
    } else {
        if (length <= USB_SIZEOF_CONFIG_DESC) {
            return 0;
        }
#if 0
        USB_LOG_DBG("Config Descriptor:\r\n");
        USB_LOG_DBG("bLength: 0x%02x             \r\n", desc->bLength);
        USB_LOG_DBG("bDescriptorType: 0x%02x     \r\n", desc->bDescriptorType);
        USB_LOG_DBG("wTotalLength: 0x%04x        \r\n", desc->wTotalLength);
        USB_LOG_DBG("bNumInterfaces: 0x%02x      \r\n", desc->bNumInterfaces);
        USB_LOG_DBG("bConfigurationValue: 0x%02x \r\n", desc->bConfigurationValue);
        USB_LOG_DBG("iConfiguration: 0x%02x      \r\n", desc->iConfiguration);
        USB_LOG_DBG("bmAttributes: 0x%02x        \r\n", desc->bmAttributes);
        USB_LOG_DBG("bMaxPower: 0x%02x           \r\n", desc->bMaxPower);
#endif
        hport->config.config_desc.bLength = desc->bLength;
        hport->config.config_desc.bDescriptorType = desc->bDescriptorType;
        hport->config.config_desc.wTotalLength = desc->wTotalLength;
        hport->config.config_desc.bNumInterfaces = desc->bNumInterfaces;
        hport->config.config_desc.bConfigurationValue = desc->bConfigurationValue;
        hport->config.config_desc.iConfiguration = desc->iConfiguration;
        hport->config.config_desc.iConfiguration = desc->iConfiguration;
        hport->config.config_desc.bmAttributes = desc->bmAttributes;
        hport->config.config_desc.bMaxPower = desc->bMaxPower;

        p = (uint8_t *)desc;
        p += USB_SIZEOF_CONFIG_DESC;
        desc_len = USB_SIZEOF_CONFIG_DESC;

        memset(hport->config.intf, 0, sizeof(struct usbh_interface) * CONFIG_USBHOST_MAX_INTERFACES);

        while (p[DESC_bLength] && (desc_len <= length)) {
            switch (p[DESC_bDescriptorType]) {
                case USB_DESCRIPTOR_TYPE_INTERFACE:
                    intf_desc = (struct usb_interface_descriptor *)p;
                    cur_iface = intf_desc->bInterfaceNumber;
                    cur_alt_setting = intf_desc->bAlternateSetting;
                    cur_ep_num = intf_desc->bNumEndpoints;
                    cur_ep = 0;
                    if (cur_iface > (CONFIG_USBHOST_MAX_INTERFACES - 1)) {
                        USB_LOG_ERR("Interface num overflow\r\n");
                        return -ENOMEM;
                    }
                    if (cur_alt_setting > (CONFIG_USBHOST_MAX_INTF_ALTSETTINGS - 1)) {
                        USB_LOG_ERR("Interface altsetting num overflow\r\n");
                        return -ENOMEM;
                    }
                    if (cur_ep_num > CONFIG_USBHOST_MAX_ENDPOINTS) {
                        USB_LOG_ERR("Endpoint num overflow\r\n");
                        return -ENOMEM;
                    }
#if 0
                    USB_LOG_DBG("Interface Descriptor:\r\n");
                    USB_LOG_DBG("bLength: 0x%02x            \r\n", intf_desc->bLength);
                    USB_LOG_DBG("bDescriptorType: 0x%02x    \r\n", intf_desc->bDescriptorType);
                    USB_LOG_DBG("bInterfaceNumber: 0x%02x   \r\n", intf_desc->bInterfaceNumber);
                    USB_LOG_DBG("bAlternateSetting: 0x%02x  \r\n", intf_desc->bAlternateSetting);
                    USB_LOG_DBG("bNumEndpoints: 0x%02x      \r\n", intf_desc->bNumEndpoints);
                    USB_LOG_DBG("bInterfaceClass: 0x%02x    \r\n", intf_desc->bInterfaceClass);
                    USB_LOG_DBG("bInterfaceSubClass: 0x%02x \r\n", intf_desc->bInterfaceSubClass);
                    USB_LOG_DBG("bInterfaceProtocol: 0x%02x \r\n", intf_desc->bInterfaceProtocol);
                    USB_LOG_DBG("iInterface: 0x%02x         \r\n", intf_desc->iInterface);
#endif
                    memcpy(&hport->config.intf[cur_iface].altsetting[cur_alt_setting].intf_desc, intf_desc, 9);
                    hport->config.intf[cur_iface].altsetting_num = cur_alt_setting + 1;
                    break;
                case USB_DESCRIPTOR_TYPE_ENDPOINT:
                    ep_desc = (struct usb_endpoint_descriptor *)p;
                    memcpy(&hport->config.intf[cur_iface].altsetting[cur_alt_setting].ep[cur_ep].ep_desc, ep_desc, 7);
                    cur_ep++;
                    break;

                default:
                    break;
            }
            /* skip to next descriptor */
            p += p[DESC_bLength];
            desc_len += p[DESC_bLength];
        }
    }
    return 0;
}

#ifdef CONFIG_USBHOST_GET_STRING_DESC
void usbh_print_string(char *lead, uint8_t *str)
{
    uint8_t string[64 + 1] = { 0 };

    int len, i = 2, j = 0;

    len = str[0];
    while (i < len) {
        string[j] = str[i];
        i += 2;
        j++;
    }
    USB_LOG_RAW("%s%s\r\n", lead, string);
}
#endif

static void usbh_print_hubport_info(struct usbh_hubport *hport)
{
    USB_LOG_RAW("Device Descriptor:\r\n");
    USB_LOG_RAW("bLength: 0x%02x           \r\n", hport->device_desc.bLength);
    USB_LOG_RAW("bDescriptorType: 0x%02x   \r\n", hport->device_desc.bDescriptorType);
    USB_LOG_RAW("bcdUSB: 0x%04x            \r\n", hport->device_desc.bcdUSB);
    USB_LOG_RAW("bDeviceClass: 0x%02x      \r\n", hport->device_desc.bDeviceClass);
    USB_LOG_RAW("bDeviceSubClass: 0x%02x   \r\n", hport->device_desc.bDeviceSubClass);
    USB_LOG_RAW("bDeviceProtocol: 0x%02x   \r\n", hport->device_desc.bDeviceProtocol);
    USB_LOG_RAW("bMaxPacketSize0: 0x%02x   \r\n", hport->device_desc.bMaxPacketSize0);
    USB_LOG_RAW("idVendor: 0x%04x          \r\n", hport->device_desc.idVendor);
    USB_LOG_RAW("idProduct: 0x%04x         \r\n", hport->device_desc.idProduct);
    USB_LOG_RAW("bcdDevice: 0x%04x         \r\n", hport->device_desc.bcdDevice);
    USB_LOG_RAW("iManufacturer: 0x%02x     \r\n", hport->device_desc.iManufacturer);
    USB_LOG_RAW("iProduct: 0x%02x          \r\n", hport->device_desc.iProduct);
    USB_LOG_RAW("iSerialNumber: 0x%02x     \r\n", hport->device_desc.iSerialNumber);
    USB_LOG_RAW("bNumConfigurations: 0x%02x\r\n", hport->device_desc.bNumConfigurations);

    USB_LOG_RAW("Config Descriptor:\r\n");
    USB_LOG_RAW("bLength: 0x%02x             \r\n", hport->config.config_desc.bLength);
    USB_LOG_RAW("bDescriptorType: 0x%02x     \r\n", hport->config.config_desc.bDescriptorType);
    USB_LOG_RAW("wTotalLength: 0x%04x        \r\n", hport->config.config_desc.wTotalLength);
    USB_LOG_RAW("bNumInterfaces: 0x%02x      \r\n", hport->config.config_desc.bNumInterfaces);
    USB_LOG_RAW("bConfigurationValue: 0x%02x \r\n", hport->config.config_desc.bConfigurationValue);
    USB_LOG_RAW("iConfiguration: 0x%02x      \r\n", hport->config.config_desc.iConfiguration);
    USB_LOG_RAW("bmAttributes: 0x%02x        \r\n", hport->config.config_desc.bmAttributes);
    USB_LOG_RAW("bMaxPower: 0x%02x           \r\n", hport->config.config_desc.bMaxPower);

    for (uint8_t i = 0; i < hport->config.config_desc.bNumInterfaces; i++) {
        for (uint8_t j = 0; j < hport->config.intf[i].altsetting_num; j++) {
            USB_LOG_RAW("Interface Descriptor:\r\n");
            USB_LOG_RAW("bLength: 0x%02x            \r\n", hport->config.intf[i].altsetting[j].intf_desc.bLength);
            USB_LOG_RAW("bDescriptorType: 0x%02x    \r\n", hport->config.intf[i].altsetting[j].intf_desc.bDescriptorType);
            USB_LOG_RAW("bInterfaceNumber: 0x%02x   \r\n", hport->config.intf[i].altsetting[j].intf_desc.bInterfaceNumber);
            USB_LOG_RAW("bAlternateSetting: 0x%02x  \r\n", hport->config.intf[i].altsetting[j].intf_desc.bAlternateSetting);
            USB_LOG_RAW("bNumEndpoints: 0x%02x      \r\n", hport->config.intf[i].altsetting[j].intf_desc.bNumEndpoints);
            USB_LOG_RAW("bInterfaceClass: 0x%02x    \r\n", hport->config.intf[i].altsetting[j].intf_desc.bInterfaceClass);
            USB_LOG_RAW("bInterfaceSubClass: 0x%02x \r\n", hport->config.intf[i].altsetting[j].intf_desc.bInterfaceSubClass);
            USB_LOG_RAW("bInterfaceProtocol: 0x%02x \r\n", hport->config.intf[i].altsetting[j].intf_desc.bInterfaceProtocol);
            USB_LOG_RAW("iInterface: 0x%02x         \r\n", hport->config.intf[i].altsetting[j].intf_desc.iInterface);

            for (uint8_t k = 0; k < hport->config.intf[i].altsetting[j].intf_desc.bNumEndpoints; k++) {
                USB_LOG_RAW("Endpoint Descriptor:\r\n");
                USB_LOG_RAW("bLength: 0x%02x          \r\n", hport->config.intf[i].altsetting[j].ep[k].ep_desc.bLength);
                USB_LOG_RAW("bDescriptorType: 0x%02x  \r\n", hport->config.intf[i].altsetting[j].ep[k].ep_desc.bDescriptorType);
                USB_LOG_RAW("bEndpointAddress: 0x%02x \r\n", hport->config.intf[i].altsetting[j].ep[k].ep_desc.bEndpointAddress);
                USB_LOG_RAW("bmAttributes: 0x%02x     \r\n", hport->config.intf[i].altsetting[j].ep[k].ep_desc.bmAttributes);
                USB_LOG_RAW("wMaxPacketSize: 0x%04x   \r\n", hport->config.intf[i].altsetting[j].ep[k].ep_desc.wMaxPacketSize);
                USB_LOG_RAW("bInterval: 0x%02x        \r\n", hport->config.intf[i].altsetting[j].ep[k].ep_desc.bInterval);
            }
        }
    }
}

static int usbh_get_default_mps(int speed)
{
    switch (speed) {
        case USB_SPEED_LOW: /* For low speed, we use 8 bytes */
            return 8;
        case USB_SPEED_FULL: /* For full or high speed, we use 64 bytes */
        case USB_SPEED_HIGH:
            return 64;
        case USB_SPEED_SUPER: /* For super speed , we must use 512 bytes */
        case USB_SPEED_SUPER_PLUS:
            return 512;
        default:
            return 64;
    }
}

int usbh_hport_activate_ep0(struct usbh_hubport *hport)
{
    struct usbh_endpoint_cfg ep0_cfg = { 0 };

    ep0_cfg.ep_addr = 0x00;
    ep0_cfg.ep_interval = 0x00;
    ep0_cfg.ep_mps = usbh_get_default_mps(hport->speed);
    ep0_cfg.ep_type = USB_ENDPOINT_TYPE_CONTROL;
    ep0_cfg.hport = hport;

    usbh_pipe_alloc(&hport->ep0, &ep0_cfg);
    return 0;
}

int usbh_hport_deactivate_ep0(struct usbh_hubport *hport)
{
#ifndef CONFIG_USBHOST_XHCI
    if (hport->dev_addr > 0) {
        usbh_free_devaddr(&g_usbh_bus.devgen, hport->dev_addr);
    }
#endif
    if (hport->ep0) {
        usbh_pipe_free(hport->ep0);
    }

    hport->ep0 = NULL;
    hport->dev_addr = 0;
    return 0;
}

int usbh_hport_activate_epx(usbh_pipe_t *pipe, struct usbh_hubport *hport, struct usb_endpoint_descriptor *ep_desc)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };

    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.mult = (ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_MASK) >> USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_SHIFT;
    ep_cfg.hport = hport;

    USB_LOG_INFO("Ep=%02x Attr=%02u Mps=%d Interval=%02u Mult=%02u\r\n",
                 ep_cfg.ep_addr,
                 ep_desc->bmAttributes,
                 ep_cfg.ep_mps,
                 ep_cfg.ep_interval,
                 ep_cfg.mult);

    return usbh_pipe_alloc(pipe, &ep_cfg);
}

int usbh_enumerate(struct usbh_hubport *hport)
{
    struct usb_interface_descriptor *intf_desc;
    struct usb_setup_packet *setup;
    int dev_addr;
    uint16_t ep_mps;
    int ret;

#define USB_REQUEST_BUFFER_SIZE 256
    setup = &hport->setup;

    /* Configure EP0 with the default maximum packet size */
    usbh_hport_activate_ep0(hport);

    /* Read the first 8 bytes of the device descriptor */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_DEVICE << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = 8;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get device descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_device_descriptor(hport, (struct usb_device_descriptor *)ep0_request_buffer, 8);

    /* Extract the correct max packetsize from the device descriptor */
    ep_mps = ((struct usb_device_descriptor *)ep0_request_buffer)->bMaxPacketSize0;

    /* Reconfigure EP0 with the correct maximum packet size */
    usbh_ep0_pipe_reconfigure(hport->ep0, 0, ep_mps, hport->speed);

#ifdef CONFIG_USBHOST_XHCI
    extern int usbh_get_xhci_devaddr(usbh_pipe_t * pipe);

    /* Assign a function address to the device connected to this port */
    dev_addr = usbh_get_xhci_devaddr(hport->ep0);
    if (dev_addr < 0) {
        USB_LOG_ERR("Failed to allocate devaddr,errorcode:%d\r\n", ret);
        goto errout;
    }
#else
    /* Assign a function address to the device connected to this port */
    dev_addr = usbh_allocate_devaddr(&g_usbh_bus.devgen);
    if (dev_addr < 0) {
        USB_LOG_ERR("Failed to allocate devaddr,errorcode:%d\r\n", ret);
        goto errout;
    }
#endif

    /* Set the USB device address */
    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_SET_ADDRESS;
    setup->wValue = dev_addr;
    setup->wIndex = 0;
    setup->wLength = 0;

    ret = usbh_control_transfer(hport->ep0, setup, NULL);
    if (ret < 0) {
        USB_LOG_ERR("Failed to set devaddr,errorcode:%d\r\n", ret);
        goto errout;
    }

    /* Wait device set address completely */
    usb_osal_msleep(2);

    /* Assign the function address to the port */
    hport->dev_addr = dev_addr;

    /* And reconfigure EP0 with the correct address */
    usbh_ep0_pipe_reconfigure(hport->ep0, dev_addr, ep_mps, hport->speed);

    /* Read the full device descriptor */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_DEVICE << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = USB_SIZEOF_DEVICE_DESC;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get full device descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_device_descriptor(hport, (struct usb_device_descriptor *)ep0_request_buffer, USB_SIZEOF_DEVICE_DESC);
    USB_LOG_INFO("New device found,idVendor:%04x,idProduct:%04x,bcdDevice:%04x\r\n",
                 ((struct usb_device_descriptor *)ep0_request_buffer)->idVendor,
                 ((struct usb_device_descriptor *)ep0_request_buffer)->idProduct,
                 ((struct usb_device_descriptor *)ep0_request_buffer)->bcdDevice);

    /* Read the first 9 bytes of the config descriptor */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_CONFIGURATION << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = USB_SIZEOF_CONFIG_DESC;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get config descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_config_descriptor(hport, (struct usb_configuration_descriptor *)ep0_request_buffer, USB_SIZEOF_CONFIG_DESC);

    /* Read the full size of the configuration data */
    uint16_t wTotalLength = ((struct usb_configuration_descriptor *)ep0_request_buffer)->wTotalLength;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_CONFIGURATION << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = wTotalLength;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get full config descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    ret = parse_config_descriptor(hport, (struct usb_configuration_descriptor *)ep0_request_buffer, wTotalLength);
    if (ret < 0) {
        USB_LOG_ERR("Parse config fail\r\n");
        goto errout;
    }
    USB_LOG_INFO("The device has %d interfaces\r\n", ((struct usb_configuration_descriptor *)ep0_request_buffer)->bNumInterfaces);
    hport->raw_config_desc = usb_malloc(wTotalLength);
    if (hport->raw_config_desc == NULL) {
        ret = -ENOMEM;
        USB_LOG_ERR("No memory to alloc for raw_config_desc\r\n");
        goto errout;
    }
    memcpy(hport->raw_config_desc, ep0_request_buffer, wTotalLength);
#ifdef CONFIG_USBHOST_GET_STRING_DESC
    /* Get Manufacturer string */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | USB_STRING_MFC_INDEX);
    setup->wIndex = 0x0409;
    setup->wLength = 255;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get Manufacturer string,errorcode:%d\r\n", ret);
        goto errout;
    }

    usbh_print_string("Manufacturer: ", ep0_request_buffer);

    /* Get Product string */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | USB_STRING_PRODUCT_INDEX);
    setup->wIndex = 0x0409;
    setup->wLength = 255;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get get Product string,errorcode:%d\r\n", ret);
        goto errout;
    }

    usbh_print_string("Product: ", ep0_request_buffer);

    /* Get SerialNumber string */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | USB_STRING_SERIAL_INDEX);
    setup->wIndex = 0x0409;
    setup->wLength = 255;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_request_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get get SerialNumber string,errorcode:%d\r\n", ret);
        goto errout;
    }

    usbh_print_string("SerialNumber: ", ep0_request_buffer);
#endif
    /* Select device configuration 1 */
    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_SET_CONFIGURATION;
    setup->wValue = 1;
    setup->wIndex = 0;
    setup->wLength = 0;

    ret = usbh_control_transfer(hport->ep0, setup, NULL);
    if (ret < 0) {
        USB_LOG_ERR("Failed to set configuration,errorcode:%d\r\n", ret);
        goto errout;
    }

    USB_LOG_INFO("Enumeration success, start loading class driver\r\n");
    /*search supported class driver*/
    for (uint8_t i = 0; i < hport->config.config_desc.bNumInterfaces; i++) {
        intf_desc = &hport->config.intf[i].altsetting[0].intf_desc;

        struct usbh_class_driver *class_driver = (struct usbh_class_driver *)usbh_find_class_driver(intf_desc->bInterfaceClass, intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol, hport->device_desc.idVendor, hport->device_desc.idProduct);

        if (class_driver == NULL) {
            USB_LOG_ERR("do not support Class:0x%02x,Subclass:0x%02x,Protocl:0x%02x\r\n",
                        intf_desc->bInterfaceClass,
                        intf_desc->bInterfaceSubClass,
                        intf_desc->bInterfaceProtocol);

            continue;
        }
        hport->config.intf[i].class_driver = class_driver;
        USB_LOG_INFO("Loading %s class driver\r\n", class_driver->driver_name);
        ret = CLASS_CONNECT(hport, i);
        if (ret < 0) {
            CLASS_DISCONNECT(hport, i);
            goto errout;
        }
    }

errout:
    if (ret < 0) {
        hport->config.config_desc.bNumInterfaces = 0;
        usbh_hport_deactivate_ep0(hport);
    }
    if (hport->raw_config_desc) {
        usb_free(hport->raw_config_desc);
        hport->raw_config_desc = NULL;
    }
    return ret;
}

struct usbh_hubport *usbh_find_hubport(uint8_t dev_addr)
{
    struct usbh_hubport *hport;
    usb_slist_t *hub_list;
    usb_slist_for_each(hub_list, &hub_class_head)
    {
        struct usbh_hub *hub = usb_slist_entry(hub_list, struct usbh_hub, list);
        for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
            hport = &hub->child[port];
            if (hport->connected) {
                if (hport->dev_addr == dev_addr) {
                    return &hub->child[port];
                }
            }
        }
    }
    return NULL;
}

void *usbh_find_class_instance(const char *devname)
{
    struct usbh_hubport *hport;
    usb_slist_t *hub_list;

    usb_slist_for_each(hub_list, &hub_class_head)
    {
        struct usbh_hub *hub = usb_slist_entry(hub_list, struct usbh_hub, list);
        for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
            hport = &hub->child[port];
            if (hport->connected) {
                for (uint8_t itf = 0; itf < hport->config.config_desc.bNumInterfaces; itf++) {
                    if ((strncmp(hport->config.intf[itf].devname, devname, CONFIG_USBHOST_DEV_NAMELEN) == 0) && hport->config.intf[itf].priv)
                        return hport->config.intf[itf].priv;
                }
            }
        }
    }
    return NULL;
}

int usbh_initialize(void)
{
    memset(&g_usbh_bus, 0, sizeof(struct usbh_bus));

#ifdef __ARMCC_VERSION /* ARM C Compiler */
    extern const int usbh_class_info$$Base;
    extern const int usbh_class_info$$Limit;
    usbh_class_info_table_begin = (struct usbh_class_info *)&usbh_class_info$$Base;
    usbh_class_info_table_end = (struct usbh_class_info *)&usbh_class_info$$Limit;
#elif defined(__GNUC__)
    extern uint32_t __usbh_class_info_start__;
    extern uint32_t __usbh_class_info_end__;
    usbh_class_info_table_begin = (struct usbh_class_info *)&__usbh_class_info_start__;
    usbh_class_info_table_end = (struct usbh_class_info *)&__usbh_class_info_end__;
#elif defined(__ICCARM__) || defined(__ICCRX__)
    usbh_class_info_table_begin = (struct usbh_class_info *)__section_begin("usbh_class_info");
    usbh_class_info_table_end = (struct usbh_class_info *)__section_end("usbh_class_info");
#endif

    /* devaddr 1 is for roothub */
    g_usbh_bus.devgen.next = 2;

    usbh_hub_initialize();
    return 0;
}

int usbh_control_transfer(usbh_pipe_t pipe, struct usb_setup_packet *setup, uint8_t *buffer)
{
    struct usbh_urb *urb;
    int ret;

    urb = usb_malloc(sizeof(struct usbh_urb));
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_control_urb_fill(urb, pipe, setup, buffer, setup->wLength, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT, NULL, NULL);

    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    usb_free(urb);
    return ret;
}

int lsusb(int argc, char **argv)
{
    usb_slist_t *i;
    struct usbh_hubport *hport;

    if (argc < 2) {
        USB_LOG_RAW("Usage: lsusb [options]...\r\n");
        USB_LOG_RAW("List USB devices\r\n");
        USB_LOG_RAW("  -v, --verbose\r\n");
        USB_LOG_RAW("      Increase verbosity (show descriptors)\r\n");
        // USB_LOG_RAW("  -s [[bus]:[devnum]]\r\n");
        // USB_LOG_RAW("      Show only devices with specified device and/or bus numbers (in decimal)\r\n");
        // USB_LOG_RAW("  -d vendor:[product]\r\n");
        // USB_LOG_RAW("      Show only devices with the specified vendor and product ID numbers (in hexadecimal)\r\n");
        USB_LOG_RAW("  -t, --tree\r\n");
        USB_LOG_RAW("      Dump the physical USB device hierachy as a tree\r\n");
        USB_LOG_RAW("  -V, --version\r\n");
        USB_LOG_RAW("      Show version of program\r\n");
        USB_LOG_RAW("  -h, --help\r\n");
        USB_LOG_RAW("      Show usage and help\r\n");
        return 0;
    }
    if (argc > 3) {
        return 0;
    }

    if (strcmp(argv[1], "-t") == 0) {
        usb_slist_for_each(i, &hub_class_head)
        {
            struct usbh_hub *hub = usb_slist_entry(i, struct usbh_hub, list);

            if (hub->is_roothub) {
                USB_LOG_RAW("/: Hub %02u, ports=%u, is roothub\r\n", hub->index, hub->hub_desc.bNbrPorts);
            } else {
                USB_LOG_RAW("/: Hub %02u, ports=%u, mounted on Hub %02u:Port %u\r\n",
                            hub->index,
                            hub->hub_desc.bNbrPorts,
                            hub->parent->parent->index,
                            hub->parent->port);
            }

            for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
                hport = &hub->child[port];
                if (hport->connected) {
                    for (uint8_t i = 0; i < hport->config.config_desc.bNumInterfaces; i++) {
                        if (hport->config.intf[i].class_driver->driver_name) {
                            USB_LOG_RAW("    |__Port %u,Port addr:0x%02x,If %u,ClassDriver=%s\r\n",
                                        hport->port,
                                        hport->dev_addr,
                                        i,
                                        hport->config.intf[i].class_driver->driver_name);
                        }
                    }
                }
            }
        }
    }

    if (strcmp(argv[1], "-v") == 0) {
        usb_slist_for_each(i, &hub_class_head)
        {
            struct usbh_hub *hub = usb_slist_entry(i, struct usbh_hub, list);
            for (uint8_t port = 0; port < hub->hub_desc.bNbrPorts; port++) {
                hport = &hub->child[port];
                if (hport->connected) {
                    USB_LOG_RAW("Hub %02u,Port %u,Port addr:0x%02x,VID:PID 0x%04x:0x%04x\r\n",
                                hub->index,
                                hport->port,
                                hport->dev_addr,
                                hport->device_desc.idVendor,
                                hport->device_desc.idProduct);
                    usbh_print_hubport_info(hport);
                }
            }
        }
    }

    return 0;
}