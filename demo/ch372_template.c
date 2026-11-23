/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_ch372.h"

const uint8_t USBD_BinaryObjectStoreDescriptor[] = {
    USB_BOS_HEADER_DESCRIPTOR_INIT(5 + USB_BOS_CAP_USB2_0_EXTENSION_DESCRIPTOR_LEN + USB_BOS_CAP_SUPERSPEED_USB_DESCRIPTOR_LEN, 2),
    USB_BOS_CAP_USB2_0_EXTENSION_DESCRIPTOR_INIT(0x00000002),
    USB_BOS_CAP_SUPERSPEED_USB_DESCRIPTOR_INIT(0x00, 0x000E, 0x01, 0x0A, 0x07FF)
};

const struct usb_bos_descriptor bos_desc = {
    .string = USBD_BinaryObjectStoreDescriptor,
    .string_len = sizeof(USBD_BinaryObjectStoreDescriptor),
};

/*!< endpoint address */
#define IN_EP1  0x81
#define OUT_EP1 0x01
#define IN_EP2  0x82
#define OUT_EP2 0x02
#define IN_EP3  0x83
#define OUT_EP3 0x03

#define USBD_VID       0x1A86
#define USBD_PID       0x5537
#define USBD_MAX_POWER 100

/*!< config descriptor size */
#define USB_CONFIG_SIZE  (9 + CH372_DESCRIPTOR_LEN)
#define USB3_CONFIG_SIZE (9 + USB3_CH372_DESCRIPTOR_LEN)

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xFF, 0x80, 0x55, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t device_descriptor_usb3[] = {
    USB3_DEVICE_DESCRIPTOR_INIT(USB_3_0, 0xFF, 0x80, 0x55, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor_ss[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB3_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB3_CH372_DESCRIPTOR_INIT(IN_EP1, OUT_EP1, IN_EP2, OUT_EP2, IN_EP3, OUT_EP3, 16, 0)
};

static const uint8_t config_descriptor_hs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CH372_DESCRIPTOR_INIT(IN_EP1, OUT_EP1, IN_EP2, OUT_EP2, IN_EP3, OUT_EP3, USB_BULK_EP_MPS_HS, 0)
};

static const uint8_t config_descriptor_fs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CH372_DESCRIPTOR_INIT(IN_EP1, OUT_EP1, IN_EP2, OUT_EP2, IN_EP3, OUT_EP3, USB_BULK_EP_MPS_FS, 0)
};

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0xFF, 0x80, 0x55, 0x01),
};

static const uint8_t other_speed_config_descriptor_hs[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CH372_DESCRIPTOR_INIT(IN_EP1, OUT_EP1, IN_EP2, OUT_EP2, IN_EP3, OUT_EP3, USB_BULK_EP_MPS_FS, 0)
};

static const uint8_t other_speed_config_descriptor_fs[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CH372_DESCRIPTOR_INIT(IN_EP1, OUT_EP1, IN_EP2, OUT_EP2, IN_EP3, OUT_EP3, USB_BULK_EP_MPS_HS, 0)
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB CH372 DEMO",       /* Product */
    "2026123456",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_SUPER) {
        return device_descriptor_usb3;
    } else {
        return device_descriptor;
    }
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_SUPER) {
        return config_descriptor_ss;
    } else if (speed == USB_SPEED_HIGH) {
        return config_descriptor_hs;
    } else if (speed == USB_SPEED_FULL) {
        return config_descriptor_fs;
    } else {
        return NULL;
    }
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;

    return device_quality_descriptor;
}

static const uint8_t *other_speed_config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
        return other_speed_config_descriptor_hs;
    } else if (speed == USB_SPEED_FULL) {
        return other_speed_config_descriptor_fs;
    } else {
        return NULL;
    }
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor ch372_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .other_speed_descriptor_callback = other_speed_config_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
    .bos_descriptor = &bos_desc
};

USB_MEM_ALIGNX uint8_t pipe_buf1[16384];
USB_MEM_ALIGNX uint8_t pipe_buf2[16384];
USB_MEM_ALIGNX uint8_t pipe_buf3[16384];

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            usbd_ep_start_read(busid, OUT_EP1, pipe_buf1, 16384);
            usbd_ep_start_read(busid, OUT_EP2, pipe_buf2, 16384);
            usbd_ep_start_read(busid, OUT_EP3, pipe_buf3, 16384);
            usbd_ep_start_write(busid, IN_EP3, pipe_buf3, 16384);
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;
        default:
            break;
    }
}

void usbd_ch372_in_ep1(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_read(busid, OUT_EP2, pipe_buf2, 16384);
}

void usbd_ch372_out_ep1(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_write(busid, IN_EP2, pipe_buf1, nbytes);
}

void usbd_ch372_in_ep2(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_read(busid, OUT_EP1, pipe_buf1, 16384);
}

void usbd_ch372_out_ep2(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_write(busid, IN_EP1, pipe_buf2, nbytes);
}

void usbd_ch372_in_ep3(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_write(busid, IN_EP3, pipe_buf3, 16384);
}

void usbd_ch372_out_ep3(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    usbd_ep_start_read(busid, OUT_EP3, pipe_buf3, 16384);
}

/*!< endpoint call back */
struct usbd_endpoint ch372_in_ep1 = {
    .ep_addr = IN_EP1,
    .ep_cb = usbd_ch372_in_ep1
};

struct usbd_endpoint ch372_out_ep1 = {
    .ep_addr = OUT_EP1,
    .ep_cb = usbd_ch372_out_ep1
};

struct usbd_endpoint ch372_in_ep2 = {
    .ep_addr = IN_EP2,
    .ep_cb = usbd_ch372_in_ep2
};

struct usbd_endpoint ch372_out_ep2 = {
    .ep_addr = OUT_EP2,
    .ep_cb = usbd_ch372_out_ep2
};

struct usbd_endpoint ch372_in_ep3 = {
    .ep_addr = IN_EP3,
    .ep_cb = usbd_ch372_in_ep3
};

struct usbd_endpoint ch372_out_ep3 = {
    .ep_addr = OUT_EP3,
    .ep_cb = usbd_ch372_out_ep3
};

static struct usbd_interface intf;

void ch372_init(uint8_t busid, uintptr_t reg_base)
{
    usbd_desc_register(busid, &ch372_descriptor);

    usbd_add_interface(busid, usbd_ch372_init_intf(busid, &intf));
    usbd_add_endpoint(busid, &ch372_in_ep1);
    usbd_add_endpoint(busid, &ch372_out_ep1);
    usbd_add_endpoint(busid, &ch372_in_ep2);
    usbd_add_endpoint(busid, &ch372_out_ep2);
    usbd_add_endpoint(busid, &ch372_in_ep3);
    usbd_add_endpoint(busid, &ch372_out_ep3);
    usbd_initialize(busid, reg_base, usbd_event_handler);
}
