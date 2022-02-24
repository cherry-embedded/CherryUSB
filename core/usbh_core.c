/**
 * @file usbh_core.c
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
#include "usbh_cdc_acm.h"
#include "usbh_hid.h"
#include "usbh_msc.h"

static const char *speed_table[] = { "error speed", "low speed", "full speed", "high speed" };

static const struct usbh_class_driver *usbh_find_class_driver(uint8_t class, uint8_t subcalss, uint8_t protocol, uint16_t vid, uint16_t pid);
void usbh_hport_activate(struct usbh_hubport *hport);
void usbh_hport_deactivate(struct usbh_hubport *hport);

/* general descriptor field offsets */
#define DESC_bLength         0 /** Length offset */
#define DESC_bDescriptorType 1 /** Descriptor type offset */

#define USB_DEV_ADDR_MAX         0x7f
#define USB_DEV_ADDR_MARK_OFFSET 5
#define USB_DEV_ADDR_MARK_MASK   0x1f

struct usbh_devaddr_priv {
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

struct usbh_roothubport_priv {
    struct usbh_hubport hport;       /* Common hub port definitions */
    struct usbh_devaddr_priv devgen; /* Address generation data */
};

struct usbh_core_priv {
    struct usbh_roothubport_priv rhport[CONFIG_USBHOST_RHPORTS];
    volatile struct usbh_hubport *active_hport; /* Used to pass external hub port events */
    volatile bool pscwait;                      /* TRUE: Thread is waiting for port status change event */
    usb_osal_sem_t pscsem;                      /* Semaphore to wait for a port event */
} usbh_core_cfg;

static inline struct usbh_roothubport_priv *usbh_find_roothub_port(struct usbh_hubport *hport)
{
    while (hport->parent != NULL) {
        hport = hport->parent->parent;
    }
    return (struct usbh_roothubport_priv *)hport;
}

static int usbh_allocate_devaddr(struct usbh_devaddr_priv *devgen)
{
    uint8_t startaddr = devgen->next;
    uint8_t devaddr;
    int index;
    int bitno;

    /* Loop until we find a valid device address */

    for (;;) {
        /* Try the next device address */

        devaddr = devgen->next;
        if (devgen->next >= 0x7f) {
            devgen->next = 1;
        } else {
            devgen->next++;
        }

        /* Is this address already allocated? */

        index = devaddr >> 5;
        bitno = devaddr & 0x1f;
        if ((devgen->alloctab[index] & (1 << bitno)) == 0) {
            /* No... allocate it now */

            devgen->alloctab[index] |= (1 << bitno);
            return (int)devaddr;
        }

        /* This address has already been allocated.  The following logic will
       * prevent (unexpected) infinite loops.
       */

        if (startaddr == devaddr) {
            /* We are back where we started... the are no free device address */

            return -ENOMEM;
        }
    }
}

static int usbh_free_devaddr(struct usbh_devaddr_priv *devgen, uint8_t devaddr)
{
    int index;
    int bitno;

    if ((devaddr > 0) && (devaddr < USB_DEV_ADDR_MAX)) {
        index = devaddr >> USB_DEV_ADDR_MARK_OFFSET;
        bitno = devaddr & USB_DEV_ADDR_MARK_MASK;

        /* Free the address by clearing the associated bit in the alloctab[]; */
        if ((devgen->alloctab[index] |= (1 << bitno)) != 0) {
            devgen->alloctab[index] &= ~(1 << bitno);
        } else {
            return -1;
        }
        /* Reset the next pointer if the one just released has a lower value */

        if (devaddr < devgen->next) {
            devgen->next = devaddr;
        }
    }

    return 0;
}

static int usbh_devaddr_create(struct usbh_hubport *hport)
{
    struct usbh_roothubport_priv *rhport;
    rhport = usbh_find_roothub_port(hport);

    return usbh_allocate_devaddr(&rhport->devgen);
}

static int usbh_devaddr_destroy(struct usbh_hubport *hport, uint8_t dev_addr)
{
    struct usbh_roothubport_priv *rhport;
    rhport = usbh_find_roothub_port(hport);

    return usbh_free_devaddr(&rhport->devgen, dev_addr);
}

static int parse_device_descriptor(struct usbh_hubport *hport, struct usb_device_descriptor *desc, uint16_t length)
{
    if (desc->bLength != USB_SIZEOF_DEVICE_DESC) {
        USB_LOG_ERR("invalid device bLength 0x%02x\r\n", desc->bLength);
        return -EINVAL;
    } else if (desc->bDescriptorType != USB_DESCRIPTOR_TYPE_DEVICE) {
        USB_LOG_ERR("unexpected descriptor 0x%02x\r\n", desc->bDescriptorType);
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
    uint32_t total_len = 0;
    uint8_t ep_num = 0;
    uint8_t intf_num = 0;
    uint8_t *p = (uint8_t *)desc;
    if (desc->bLength != USB_SIZEOF_CONFIG_DESC) {
        USB_LOG_ERR("invalid device bLength 0x%02x\r\n", desc->bLength);
        return -EINVAL;
    } else if (desc->bDescriptorType != USB_DESCRIPTOR_TYPE_CONFIGURATION) {
        USB_LOG_ERR("unexpected descriptor 0x%02x\r\n", desc->bDescriptorType);
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

        if (length > USB_SIZEOF_CONFIG_DESC) {
            while (p[DESC_bLength] && (total_len < desc->wTotalLength) && (intf_num < desc->bNumInterfaces)) {
                p += p[DESC_bLength];
                total_len += p[DESC_bLength];
                if (p[DESC_bDescriptorType] == USB_DESCRIPTOR_TYPE_INTERFACE) {
                    struct usb_interface_descriptor *intf_desc = (struct usb_interface_descriptor *)p;
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
                    memset(&hport->config.intf[intf_num], 0, sizeof(struct usbh_interface));

                    hport->config.intf[intf_num].intf_desc.bLength = intf_desc->bLength;
                    hport->config.intf[intf_num].intf_desc.bDescriptorType = intf_desc->bDescriptorType;
                    hport->config.intf[intf_num].intf_desc.bInterfaceNumber = intf_desc->bInterfaceNumber;
                    hport->config.intf[intf_num].intf_desc.bAlternateSetting = intf_desc->bAlternateSetting;
                    hport->config.intf[intf_num].intf_desc.bNumEndpoints = intf_desc->bNumEndpoints;
                    hport->config.intf[intf_num].intf_desc.bInterfaceClass = intf_desc->bInterfaceClass;
                    hport->config.intf[intf_num].intf_desc.bInterfaceSubClass = intf_desc->bInterfaceSubClass;
                    hport->config.intf[intf_num].intf_desc.bInterfaceProtocol = intf_desc->bInterfaceProtocol;
                    hport->config.intf[intf_num].intf_desc.iInterface = intf_desc->iInterface;
                    ep_num = 0;
                    while (p[DESC_bLength] && (total_len < desc->wTotalLength) && (ep_num < intf_desc->bNumEndpoints)) {
                        p += p[DESC_bLength];
                        total_len += p[DESC_bLength];
                        if (p[DESC_bDescriptorType] == USB_DESCRIPTOR_TYPE_ENDPOINT) {
                            struct usb_endpoint_descriptor *ep_desc = (struct usb_endpoint_descriptor *)p;
#if 0
                            USB_LOG_DBG("Endpoint Descriptor:\r\n");
                            USB_LOG_DBG("bLength: 0x%02x          \r\n", ep_desc->bLength);
                            USB_LOG_DBG("bDescriptorType: 0x%02x  \r\n", ep_desc->bDescriptorType);
                            USB_LOG_DBG("bEndpointAddress: 0x%02x \r\n", ep_desc->bEndpointAddress);
                            USB_LOG_DBG("bmAttributes: 0x%02x     \r\n", ep_desc->bmAttributes);
                            USB_LOG_DBG("wMaxPacketSize: 0x%04x   \r\n", ep_desc->wMaxPacketSize);
                            USB_LOG_DBG("bInterval: 0x%02x        \r\n", ep_desc->bInterval);
#endif
                            memset(&hport->config.intf[intf_num].ep[ep_num], 0, sizeof(struct usbh_endpoint));

                            hport->config.intf[intf_num].ep[ep_num].ep_desc.bLength = ep_desc->bLength;
                            hport->config.intf[intf_num].ep[ep_num].ep_desc.bDescriptorType = ep_desc->bDescriptorType;
                            hport->config.intf[intf_num].ep[ep_num].ep_desc.bEndpointAddress = ep_desc->bEndpointAddress;
                            hport->config.intf[intf_num].ep[ep_num].ep_desc.bmAttributes = ep_desc->bmAttributes;
                            hport->config.intf[intf_num].ep[ep_num].ep_desc.wMaxPacketSize = ep_desc->wMaxPacketSize;
                            hport->config.intf[intf_num].ep[ep_num].ep_desc.bInterval = ep_desc->bInterval;
                            ep_num++;
                        }
                    }
                    intf_num++;
                }
            }
        }
    }
    return 0;
}

#ifdef CONFIG_USBHOST_GET_STRING_DESC
static int parse_string_descriptor(struct usbh_hubport *hport, struct usb_string_descriptor *desc, uint8_t str_idx)
{
    uint8_t string[64 + 1] = { 0 };
    uint8_t *p = (uint8_t *)desc;

    if (desc->bDescriptorType != USB_DESCRIPTOR_TYPE_STRING) {
        USB_LOG_ERR("unexpected descriptor 0x%02x\r\n", desc->bDescriptorType);
        return -2;
    } else {
        p += 2;
        for (uint32_t i = 0; i < (desc->bLength - 2) / 2; i++) {
            string[i] = *p;
            p += 2;
        }
        if (str_idx == USB_STRING_MFC_INDEX) {
            USB_LOG_INFO("Manufacturer :%s\r\n", string);
        } else if (str_idx == USB_STRING_PRODUCT_INDEX) {
            USB_LOG_INFO("Product :%s\r\n", string);

        } else if (str_idx == USB_STRING_SERIAL_INDEX) {
            USB_LOG_INFO("SerialNumber :%s\r\n", string);
        } else {
        }
    }
    return 0;
}
#endif

static void usbh_print_hubport_info(struct usbh_hubport *hport)
{
    printf("Device Descriptor:\r\n");
    printf("bLength: 0x%02x           \r\n", hport->device_desc.bLength);
    printf("bDescriptorType: 0x%02x   \r\n", hport->device_desc.bDescriptorType);
    printf("bcdUSB: 0x%04x            \r\n", hport->device_desc.bcdUSB);
    printf("bDeviceClass: 0x%02x      \r\n", hport->device_desc.bDeviceClass);
    printf("bDeviceSubClass: 0x%02x   \r\n", hport->device_desc.bDeviceSubClass);
    printf("bDeviceProtocol: 0x%02x   \r\n", hport->device_desc.bDeviceProtocol);
    printf("bMaxPacketSize0: 0x%02x   \r\n", hport->device_desc.bMaxPacketSize0);
    printf("idVendor: 0x%04x          \r\n", hport->device_desc.idVendor);
    printf("idProduct: 0x%04x         \r\n", hport->device_desc.idProduct);
    printf("bcdDevice: 0x%04x         \r\n", hport->device_desc.bcdDevice);
    printf("iManufacturer: 0x%02x     \r\n", hport->device_desc.iManufacturer);
    printf("iProduct: 0x%02x          \r\n", hport->device_desc.iProduct);
    printf("iSerialNumber: 0x%02x     \r\n", hport->device_desc.iSerialNumber);
    printf("bNumConfigurations: 0x%02x\r\n", hport->device_desc.bNumConfigurations);

    printf("Config Descriptor:\r\n");
    printf("bLength: 0x%02x             \r\n", hport->config.config_desc.bLength);
    printf("bDescriptorType: 0x%02x     \r\n", hport->config.config_desc.bDescriptorType);
    printf("wTotalLength: 0x%04x        \r\n", hport->config.config_desc.wTotalLength);
    printf("bNumInterfaces: 0x%02x      \r\n", hport->config.config_desc.bNumInterfaces);
    printf("bConfigurationValue: 0x%02x \r\n", hport->config.config_desc.bConfigurationValue);
    printf("iConfiguration: 0x%02x      \r\n", hport->config.config_desc.iConfiguration);
    printf("bmAttributes: 0x%02x        \r\n", hport->config.config_desc.bmAttributes);
    printf("bMaxPower: 0x%02x           \r\n", hport->config.config_desc.bMaxPower);

    for (uint8_t i = 0; i < hport->config.config_desc.bNumInterfaces; i++) {
        printf("Interface Descriptor:\r\n");
        printf("bLength: 0x%02x            \r\n", hport->config.intf[i].intf_desc.bLength);
        printf("bDescriptorType: 0x%02x    \r\n", hport->config.intf[i].intf_desc.bDescriptorType);
        printf("bInterfaceNumber: 0x%02x   \r\n", hport->config.intf[i].intf_desc.bInterfaceNumber);
        printf("bAlternateSetting: 0x%02x  \r\n", hport->config.intf[i].intf_desc.bAlternateSetting);
        printf("bNumEndpoints: 0x%02x      \r\n", hport->config.intf[i].intf_desc.bNumEndpoints);
        printf("bInterfaceClass: 0x%02x    \r\n", hport->config.intf[i].intf_desc.bInterfaceClass);
        printf("bInterfaceSubClass: 0x%02x \r\n", hport->config.intf[i].intf_desc.bInterfaceSubClass);
        printf("bInterfaceProtocol: 0x%02x \r\n", hport->config.intf[i].intf_desc.bInterfaceProtocol);
        printf("iInterface: 0x%02x         \r\n", hport->config.intf[i].intf_desc.iInterface);

        for (uint8_t j = 0; j < hport->config.intf[i].intf_desc.bNumEndpoints; j++) {
            printf("Endpoint Descriptor:\r\n");
            printf("bLength: 0x%02x          \r\n", hport->config.intf[i].ep[j].ep_desc.bLength);
            printf("bDescriptorType: 0x%02x  \r\n", hport->config.intf[i].ep[j].ep_desc.bDescriptorType);
            printf("bEndpointAddress: 0x%02x \r\n", hport->config.intf[i].ep[j].ep_desc.bEndpointAddress);
            printf("bmAttributes: 0x%02x     \r\n", hport->config.intf[i].ep[j].ep_desc.bmAttributes);
            printf("wMaxPacketSize: 0x%04x   \r\n", hport->config.intf[i].ep[j].ep_desc.wMaxPacketSize);
            printf("bInterval: 0x%02x        \r\n", hport->config.intf[i].ep[j].ep_desc.bInterval);
        }
    }
}

static int usbh_enumerate(struct usbh_hubport *hport)
{
    struct usb_interface_descriptor *intf_desc;
    struct usb_setup_packet *setup;
    uint8_t *ep0_buffer;
    uint8_t descsize;
    int dev_addr;
    uint8_t ep_mps;
    int ret;

#define USB_REQUEST_BUFFER_SIZE 256
    /* Allocate buffer for setup and data buffer */
    if (hport->setup == NULL) {
        hport->setup = usb_iomalloc(sizeof(struct usb_setup_packet));
        if (hport->setup == NULL) {
            USB_LOG_ERR("Fail to alloc setup\r\n");
            return -ENOMEM;
        }
    }

    setup = hport->setup;

    ep0_buffer = usb_iomalloc(USB_REQUEST_BUFFER_SIZE);
    if (ep0_buffer == NULL) {
        USB_LOG_ERR("Fail to alloc ep0_buffer\r\n");
        return -ENOMEM;
    }

    /* Pick an appropriate packet size for this device
     *
     * USB 2.0, Paragraph 5.5.3 "Control Transfer Packet Size Constraints"
     *
     *  "An endpoint for control transfers specifies the maximum data
     *   payload size that the endpoint can accept from or transmit to
     *   the bus. The allowable maximum control transfer data payload
     *   sizes for full-speed devices is 8, 16, 32, or 64 bytes; for
     *   high-speed devices, it is 64 bytes and for low-speed devices,
     *   it is 8 bytes. This maximum applies to the data payloads of the
     *   Data packets following a Setup..."
     */

    if (hport->speed == USB_SPEED_HIGH) {
        /* For high-speed, we must use 64 bytes */
        ep_mps = 64;
        descsize = USB_SIZEOF_DEVICE_DESC;
    } else {
        /* Eight will work for both low- and full-speed */
        ep_mps = 8;
        descsize = 8;
    }

    /* Configure EP0 with the initial maximum packet size */
    usbh_ep0_reconfigure(hport->ep0, 0, ep_mps, hport->speed);

    /* Read the first 8 bytes of the device descriptor */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_DEVICE << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = descsize;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get device descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_device_descriptor(hport, (struct usb_device_descriptor *)ep0_buffer, descsize);

    /* Extract the correct max packetsize from the device descriptor */
    ep_mps = ((struct usb_device_descriptor *)ep0_buffer)->bMaxPacketSize0;

    /* And reconfigure EP0 with the correct maximum packet size */
    usbh_ep0_reconfigure(hport->ep0, 0, ep_mps, hport->speed);

    /* Assign a function address to the device connected to this port */
    dev_addr = usbh_devaddr_create(hport);
    if (dev_addr < 0) {
        USB_LOG_ERR("Failed to allocate devaddr,errorcode:%d\r\n", ret);
        goto errout;
    }

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

    /* wait device address set completely */
    usb_osal_msleep(2);

    /* Assign the function address to the port */
    hport->dev_addr = dev_addr;

    /* And reconfigure EP0 with the correct address */
    usbh_ep0_reconfigure(hport->ep0, dev_addr, ep_mps, hport->speed);

    /* Read the full device descriptor if hport is not in high speed*/
    if (descsize < USB_SIZEOF_DEVICE_DESC) {
        setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
        setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
        setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_DEVICE << 8) | 0);
        setup->wIndex = 0;
        setup->wLength = USB_SIZEOF_DEVICE_DESC;

        ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
        if (ret < 0) {
            USB_LOG_ERR("Failed to get full device descriptor,errorcode:%d\r\n", ret);
            goto errout;
        }

        parse_device_descriptor(hport, (struct usb_device_descriptor *)ep0_buffer, USB_SIZEOF_DEVICE_DESC);
    }

    /* Read the first 9 bytes of the config descriptor */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_CONFIGURATION << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = USB_SIZEOF_CONFIG_DESC;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get config descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_config_descriptor(hport, (struct usb_configuration_descriptor *)ep0_buffer, USB_SIZEOF_CONFIG_DESC);

    /* Read the full size of the configuration data */
    uint16_t wTotalLength = ((struct usb_configuration_descriptor *)ep0_buffer)->wTotalLength;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_CONFIGURATION << 8) | 0);
    setup->wIndex = 0;
    setup->wLength = wTotalLength;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get full config descriptor,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_config_descriptor(hport, (struct usb_configuration_descriptor *)ep0_buffer, wTotalLength);

#ifdef CONFIG_USBHOST_GET_STRING_DESC
    /* Get Manufacturer string */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | USB_STRING_MFC_INDEX);
    setup->wIndex = 0x0409;
    setup->wLength = 255;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get Manufacturer string,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_string_descriptor(hport, (struct usb_string_descriptor *)ep0_buffer, USB_STRING_MFC_INDEX);

    /* Get Product string */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | USB_STRING_PRODUCT_INDEX);
    setup->wIndex = 0x0409;
    setup->wLength = 255;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get get Product string,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_string_descriptor(hport, (struct usb_string_descriptor *)ep0_buffer, USB_STRING_PRODUCT_INDEX);

    /* Get SerialNumber string */
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = USB_REQUEST_GET_DESCRIPTOR;
    setup->wValue = (uint16_t)((USB_DESCRIPTOR_TYPE_STRING << 8) | USB_STRING_SERIAL_INDEX);
    setup->wIndex = 0x0409;
    setup->wLength = 255;

    ret = usbh_control_transfer(hport->ep0, setup, ep0_buffer);
    if (ret < 0) {
        USB_LOG_ERR("Failed to get get SerialNumber string,errorcode:%d\r\n", ret);
        goto errout;
    }

    parse_string_descriptor(hport, (struct usb_string_descriptor *)ep0_buffer, USB_STRING_SERIAL_INDEX);
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
        intf_desc = &hport->config.intf[i].intf_desc;

        struct usbh_class_driver *class_driver = (struct usbh_class_driver *)usbh_find_class_driver(intf_desc->bInterfaceClass, intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol, hport->device_desc.idVendor, hport->device_desc.idProduct);

        if (class_driver == NULL) {
            USB_LOG_ERR("do not support Class:0x%02x,Subclass:0x%02x,Protocl:0x%02x\r\n",
                        intf_desc->bInterfaceClass,
                        intf_desc->bInterfaceSubClass,
                        intf_desc->bInterfaceProtocol);

            continue;
        }
        hport->config.intf[i].class_driver = class_driver;

        if (hport->config.intf[i].class_driver->connect) {
            ret = CLASS_CONNECT(hport, i);
            if (ret < 0) {
                ret = CLASS_DISCONNECT(hport, i);
                goto errout;
            }
        }
    }

errout:
    if (ret < 0) {
        usbh_hport_deactivate(hport);
    }

    if (ep0_buffer) {
        usb_iofree(ep0_buffer);
    }

    return ret;
}

static int usbh_portchange_wait(struct usbh_hubport **hport)
{
    struct usbh_hubport *connport = NULL;
    uint32_t flags;
    int ret;

    /* Loop until a change in connection state is detected */
    while (1) {
        /* Check for a change in the connection state on any root hub port */
        flags = usb_osal_enter_critical_section();
        for (uint8_t port = USBH_HUB_PORT_START_INDEX; port <= CONFIG_USBHOST_RHPORTS; port++) {
            connport = &usbh_core_cfg.rhport[port - 1].hport;

            if (connport->port_change) {
                connport->port_change = false;
                *hport = connport;
                usb_osal_leave_critical_section(flags);
                return 0;
            }
        }
        /* Is a device connected to an external hub? */
        if (usbh_core_cfg.active_hport) {
            connport = (struct usbh_hubport *)usbh_core_cfg.active_hport;
            usbh_core_cfg.active_hport = NULL;
            *hport = connport;
            usb_osal_leave_critical_section(flags);
            return 0;
        }
        /* No changes on any port. Wait for a connection/disconnection event and check again */
        usbh_core_cfg.pscwait = true;
        usb_osal_leave_critical_section(flags);
        ret = usb_osal_sem_take(usbh_core_cfg.pscsem);
        if (ret < 0) {
            return ret;
        }
    }
}

static void usbh_portchange_detect_thread(void *argument)
{
    struct usbh_hubport *hport = NULL;
    uint32_t flags;

    flags = usb_osal_enter_critical_section();
    usb_hc_init();

    for (uint8_t port = USBH_HUB_PORT_START_INDEX; port <= CONFIG_USBHOST_RHPORTS; port++) {
        usbh_core_cfg.rhport[port - 1].hport.port = port;
        usbh_core_cfg.rhport[port - 1].devgen.next = 1;
        usbh_hport_activate(&usbh_core_cfg.rhport[port - 1].hport);
    }
    usb_osal_leave_critical_section(flags);

    while (1) {
        usbh_portchange_wait(&hport);
        if (hport->connected) {
            /*if roothub port,reset port first*/
            if (ROOTHUB(hport)) {
                /* Reset the host port */
                usbh_reset_port(hport->port);
                usb_osal_msleep(200);
                /* Get the current device speed */
                hport->speed = usbh_get_port_speed(hport->port);
                USB_LOG_INFO("Hub %u, Port %u connected, %s\r\n", 1, hport->port, speed_table[hport->speed]);
            } else {
                USB_LOG_INFO("Hub %u, Port %u connected, %s\r\n", hport->parent->index, hport->port, speed_table[hport->speed]);
            }
            usb_osal_thread_suspend(g_lpworkq.thread);
            usbh_enumerate(hport);
            usb_osal_thread_resume(g_lpworkq.thread);
        } else {
            usbh_hport_deactivate(hport);
            for (uint8_t i = 0; i < hport->config.config_desc.bNumInterfaces; i++) {
                if (hport->config.intf[i].class_driver && hport->config.intf[i].class_driver->disconnect) {
                    CLASS_DISCONNECT(hport, i);
                }
            }

            hport->config.config_desc.bNumInterfaces = 0;

            if (ROOTHUB(hport)) {
                USB_LOG_INFO("Hub %u,Port:%u disconnected\r\n", 1, hport->port);
            } else {
                USB_LOG_INFO("Hub %u,Port:%u disconnected\r\n", hport->parent->index, hport->port);
            }
        }
    }
}

void usbh_external_hport_connect(struct usbh_hubport *hport)
{
    uint32_t flags;

    usbh_hport_activate(hport);

    flags = usb_osal_enter_critical_section();

    hport->connected = true;
    usbh_core_cfg.active_hport = hport;

    if (usbh_core_cfg.pscwait) {
        usbh_core_cfg.pscwait = false;
        usb_osal_sem_give(usbh_core_cfg.pscsem);
    }

    usb_osal_leave_critical_section(flags);
}

void usbh_external_hport_disconnect(struct usbh_hubport *hport)
{
    uint32_t flags;

    flags = usb_osal_enter_critical_section();
    hport->connected = false;
    usbh_core_cfg.active_hport = hport;

    if (usbh_core_cfg.pscwait) {
        usbh_core_cfg.pscwait = false;
        usb_osal_sem_give(usbh_core_cfg.pscsem);
    }

    usb_osal_leave_critical_section(flags);
}

void usbh_hport_activate(struct usbh_hubport *hport)
{
    struct usbh_endpoint_cfg ep0_cfg;
    uint32_t flags;

    flags = usb_osal_enter_critical_section();
    memset(&ep0_cfg, 0, sizeof(struct usbh_endpoint_cfg));

    ep0_cfg.ep_addr = 0x00;
    ep0_cfg.ep_interval = 0x00;
    ep0_cfg.ep_mps = 0x08;
    ep0_cfg.ep_type = USB_ENDPOINT_TYPE_CONTROL;
    ep0_cfg.hport = hport;
    /* Allocate memory for roothub port control endpoint */
    usbh_ep_alloc(&hport->ep0, &ep0_cfg);

    usb_osal_leave_critical_section(flags);
}

void usbh_hport_deactivate(struct usbh_hubport *hport)
{
    uint32_t flags;

    flags = usb_osal_enter_critical_section();
    /* Don't free the control pipe of root hub ports! */
    if (hport->parent != NULL && hport->ep0 != NULL) {
        usb_ep_cancel(hport->ep0);
        usbh_ep_free(hport->ep0);
        hport->ep0 = NULL;
    }
    /* Free the device address if one has been assigned */
    usbh_devaddr_destroy(hport, hport->dev_addr);

    if (hport->setup)
        usb_iofree(hport->setup);

    hport->setup = NULL;
    hport->dev_addr = 0;

    usb_osal_leave_critical_section(flags);
}

void usbh_event_notify_handler(uint8_t event, uint8_t rhport)
{
    switch (event) {
        case USBH_EVENT_ATTACHED:
            if (!usbh_core_cfg.rhport[rhport - 1].hport.connected) {
                usbh_core_cfg.rhport[rhport - 1].hport.connected = true;
                usbh_core_cfg.rhport[rhport - 1].hport.port_change = true;
                if (usbh_core_cfg.pscwait) {
                    usbh_core_cfg.pscwait = false;
                    usb_osal_sem_give(usbh_core_cfg.pscsem);
                }
            }
            break;
        case USBH_EVENT_REMOVED:
            if (usbh_core_cfg.rhport[rhport - 1].hport.connected) {
                usbh_core_cfg.rhport[rhport - 1].hport.connected = false;
                usbh_core_cfg.rhport[rhport - 1].hport.port_change = true;
                if (usbh_core_cfg.pscwait) {
                    usbh_core_cfg.pscwait = false;
                    usb_osal_sem_give(usbh_core_cfg.pscsem);
                }
            }
            break;
        default:
            break;
    }
}

int usbh_initialize(void)
{
    usb_osal_thread_t usb_thread;

    memset(&usbh_core_cfg, 0, sizeof(struct usbh_core_priv));

    usbh_workq_initialize();

    usbh_core_cfg.pscsem = usb_osal_sem_create(0);
    if (usbh_core_cfg.pscsem == NULL) {
        return -1;
    }

    usb_thread = usb_osal_thread_create("usbh_psc", CONFIG_USBHOST_PSC_STACKSIZE, CONFIG_USBHOST_PSC_PRIO, usbh_portchange_detect_thread, NULL);
    if (usb_thread == NULL) {
        return -1;
    }

    return 0;
}

int lsusb(int argc, char **argv)
{
    usb_slist_t *hub_list;
    uint8_t port;

    if (argc < 2) {
        printf("Usage: lsusb [options]...\r\n");
        printf("List USB devices\r\n");
        printf("  -v, --verbose\r\n");
        printf("      Increase verbosity (show descriptors)\r\n");
        printf("  -s [[bus]:[devnum]]\r\n");
        printf("      Show only devices with specified device and/or bus numbers (in decimal)\r\n");
        printf("  -d vendor:[product]\r\n");
        printf("      Show only devices with the specified vendor and product ID numbers (in hexadecimal)\r\n");
        printf("  -t, --tree\r\n");
        printf("      Dump the physical USB device hierachy as a tree\r\n");
        printf("  -V, --version\r\n");
        printf("      Show version of program\r\n");
        printf("  -h, --help\r\n");
        printf("      Show usage and help\r\n");
        return 0;
    }

    if (argc > 3) {
        return 0;
    }

    if (strcmp(argv[1], "-t") == 0) {
        for (port = USBH_HUB_PORT_START_INDEX; port <= CONFIG_USBHOST_RHPORTS; port++) {
            if (usbh_core_cfg.rhport[port - 1].hport.connected) {
                printf("/: Hub %02u,VID:PID 0x%04x:0x%04x\r\n", USBH_ROOT_HUB_INDEX, usbh_core_cfg.rhport[port - 1].hport.device_desc.idVendor, usbh_core_cfg.rhport[port - 1].hport.device_desc.idProduct);

                for (uint8_t i = 0; i < usbh_core_cfg.rhport[port - 1].hport.config.config_desc.bNumInterfaces; i++) {
                    if (usbh_core_cfg.rhport[port - 1].hport.config.intf[i].class_driver->driver_name) {
                        printf("    |__Port %u,Port addr:0x%02x,If %u,ClassDriver=%s\r\n", usbh_core_cfg.rhport[port - 1].hport.port, usbh_core_cfg.rhport[port - 1].hport.dev_addr,
                               i, usbh_core_cfg.rhport[port - 1].hport.config.intf[i].class_driver->driver_name);
                    }
                }
            }
        }
        usb_slist_for_each(hub_list, &hub_class_head)
        {
            usbh_hub_t *hub_class = usb_slist_entry(hub_list, struct usbh_hub, list);

            for (port = USBH_HUB_PORT_START_INDEX; port <= hub_class->nports; port++) {
                if (hub_class->child[port - 1].connected) {
                    printf("/: Hub %02u,VID:PID 0x%04x:0x%04x\r\n", hub_class->index, hub_class->child[port - 1].device_desc.idVendor, hub_class->child[port - 1].device_desc.idProduct);

                    for (uint8_t i = 0; i < hub_class->child[port - 1].config.config_desc.bNumInterfaces; i++) {
                        if (hub_class->child[port - 1].config.intf[i].class_driver->driver_name) {
                            printf("    |__Port %u,Port addr:0x%02x,If %u,ClassDriver=%s\r\n", hub_class->child[port - 1].port, hub_class->child[port - 1].dev_addr,
                                   i, hub_class->child[port - 1].config.intf[i].class_driver->driver_name);
                        }
                    }
                }
            }
        }
    } else if (strcmp(argv[1], "-v") == 0) {
        for (port = USBH_HUB_PORT_START_INDEX; port <= CONFIG_USBHOST_RHPORTS; port++) {
            if (usbh_core_cfg.rhport[port - 1].hport.connected) {
                printf("Hub %02u,Port %u,Port addr:0x%02x,VID:PID 0x%04x:0x%04x\r\n", USBH_ROOT_HUB_INDEX, usbh_core_cfg.rhport[port - 1].hport.port, usbh_core_cfg.rhport[port - 1].hport.dev_addr,
                       usbh_core_cfg.rhport[port - 1].hport.device_desc.idVendor, usbh_core_cfg.rhport[port - 1].hport.device_desc.idProduct);
                usbh_print_hubport_info(&usbh_core_cfg.rhport[port - 1].hport);
            }
        }

        usb_slist_for_each(hub_list, &hub_class_head)
        {
            usbh_hub_t *hub_class = usb_slist_entry(hub_list, struct usbh_hub, list);

            for (port = USBH_HUB_PORT_START_INDEX; port <= hub_class->nports; port++) {
                if (hub_class->child[port - 1].connected) {
                    printf("Hub %02u,Port %u,Port addr:0x%02x,VID:PID 0x%04x:0x%04x\r\n", hub_class->index, hub_class->child[port - 1].port, hub_class->child[port - 1].dev_addr,
                           hub_class->child[port - 1].device_desc.idVendor, hub_class->child[port - 1].device_desc.idProduct);
                    usbh_print_hubport_info(&hub_class->child[port - 1]);
                }
            }
        }
    }

    return 0;
}

struct usbh_hubport *usbh_find_hubport(uint8_t dev_addr)
{
    usb_slist_t *hub_list;
    uint8_t port;

    for (port = USBH_HUB_PORT_START_INDEX; port <= CONFIG_USBHOST_RHPORTS; port++) {
        if (usbh_core_cfg.rhport[port - 1].hport.connected) {
            if (usbh_core_cfg.rhport[port - 1].hport.dev_addr == dev_addr) {
                return &usbh_core_cfg.rhport[port - 1].hport;
            }
        }
    }
    usb_slist_for_each(hub_list, &hub_class_head)
    {
        usbh_hub_t *hub_class = usb_slist_entry(hub_list, struct usbh_hub, list);

        for (port = USBH_HUB_PORT_START_INDEX; port <= hub_class->nports; port++) {
            if (hub_class->child[port - 1].connected) {
                if (hub_class->child[port - 1].dev_addr == dev_addr) {
                    return &hub_class->child[port - 1];
                }
            }
        }
    }
    return NULL;
}

void *usbh_find_class_instance(const char *devname)
{
    usb_slist_t *hub_list;
    struct usbh_hubport *hport;
    uint8_t port;

    for (port = USBH_HUB_PORT_START_INDEX; port <= CONFIG_USBHOST_RHPORTS; port++) {
        hport = &usbh_core_cfg.rhport[port - 1].hport;
        if (hport->connected) {
            for (uint8_t itf = 0; itf < hport->config.config_desc.bNumInterfaces; itf++) {
                if (strncmp(hport->config.intf[itf].devname, devname, CONFIG_USBHOST_DEV_NAMELEN) == 0)
                    return hport->config.intf[itf].priv;
            }
        }
    }
    usb_slist_for_each(hub_list, &hub_class_head)
    {
        usbh_hub_t *hub_class = usb_slist_entry(hub_list, struct usbh_hub, list);

        for (port = USBH_HUB_PORT_START_INDEX; port <= hub_class->nports; port++) {
            hport = &hub_class->child[port - 1];
            if (hport->connected) {
                for (uint8_t itf = 0; itf < hport->config.config_desc.bNumInterfaces; itf++) {
                    if (strncmp(hport->config.intf[itf].devname, devname, CONFIG_USBHOST_DEV_NAMELEN) == 0)
                        return hport->config.intf[itf].priv;
                }
            }
        }
    }
    return NULL;
}

const struct usbh_class_info class_info_table[] = {

    { .class = USB_DEVICE_CLASS_CDC,
      .subclass = CDC_ABSTRACT_CONTROL_MODEL,
      .protocol = CDC_COMMON_PROTOCOL_AT_COMMANDS,
      .vid = 0x00,
      .pid = 0x00,
      .class_driver = &cdc_acm_class_driver },
    { .class = USB_DEVICE_CLASS_HID,
      .subclass = HID_SUBCLASS_BOOTIF,
      .protocol = HID_PROTOCOL_KEYBOARD,
      .vid = 0x00,
      .pid = 0x00,
      .class_driver = &hid_class_driver },
    { .class = USB_DEVICE_CLASS_HID,
      .subclass = HID_SUBCLASS_BOOTIF,
      .protocol = HID_PROTOCOL_MOUSE,
      .vid = 0x00,
      .pid = 0x00,
      .class_driver = &hid_class_driver },
    { .class = USB_DEVICE_CLASS_MASS_STORAGE,
      .subclass = MSC_SUBCLASS_SCSI,
      .protocol = MSC_PROTOCOL_BULK_ONLY,
      .vid = 0x00,
      .pid = 0x00,
      .class_driver = &msc_class_driver },
#ifdef CONFIG_USBHOST_HUB
    { .class = USB_DEVICE_CLASS_HUB,
      .subclass = 0,
      .protocol = 0,
      .vid = 0x00,
      .pid = 0x00,
      .class_driver = &hub_class_driver },
    { .class = USB_DEVICE_CLASS_HUB,
      .subclass = 0,
      .protocol = 1,
      .vid = 0x00,
      .pid = 0x00,
      .class_driver = &hub_class_driver },
#endif
};

static const struct usbh_class_driver *usbh_find_class_driver(uint8_t class, uint8_t subcalss, uint8_t protocol, uint16_t vid, uint16_t pid)
{
    for (uint8_t i = 0; i < sizeof(class_info_table) / sizeof(class_info_table[0]); i++) {
        if (class == class_info_table[i].class &&
            subcalss == class_info_table[i].subclass &&
            protocol == class_info_table[i].protocol) {
            /* If this is a vendor-specific class ID, then the VID and PID have to match as well. */
            if (class == USB_DEVICE_CLASS_VEND_SPECIFIC) {
                if (vid == class_info_table[i].vid &&
                    pid == class_info_table[i].pid) {
                    return class_info_table[i].class_driver;
                }
            }
            return class_info_table[i].class_driver;
        }
    }

    return NULL;
}
