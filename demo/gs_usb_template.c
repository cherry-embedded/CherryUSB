/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_gs_usb.h"

#define GSUSB_ENDPOINT_IN  0x81
#define GSUSB_ENDPOINT_OUT 0x02

#define USBD_VID           0x1d50
#define USBD_PID           0x606f
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#define USB_CONFIG_SIZE (9 + (9 + 7 + 7))

#define LINUX_USBD_GS_CAN_VENDOR_CODE 0x20

static const uint8_t linux_WCID_StringDescriptor_MSOS[18] = {
    USB_MSOSV1_STRING_DESCRIPTOR_INIT(LINUX_USBD_GS_CAN_VENDOR_CODE)
};

static const uint8_t linux_WINUSB_WCIDDescriptor[] = {
    /*
     * WCID descriptor
     */
    0x10 + 0x18, 0x00, 0x00, 0x00,            /* dwLength */
    0x00, 0x01,                               /* bcdVersion */
    0x04, 0x00,                               /* wIndex */
    0x01,                                     /* bCount */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* bReserved_7 */

    /*
     * WCID function descriptor
     */
    0x00, /* bFirstInterfaceNumber */
    0x01, /* bReserved */
    /* WINUSB */
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, /* cCID_8 */
    /*  */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* cSubCID_8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             /* bReserved_6 */
};

static const uint8_t linux_WINUSB_IF0_WCIDProperties[] = {
    0x92, 0x00, 0x00, 0x00, /* length */
    0x00, 0x01,             /* version 1.0 */
    0x05, 0x00,             /* descr index (0x0005) */
    0x01, 0x00,             /* number of sections */
    0x88, 0x00, 0x00, 0x00, /* property section size */
    0x07, 0x00, 0x00, 0x00, /* property data type 7: Unicode REG_MULTI_SZ */
    0x2a, 0x00,             /* property name length */

    0x44, 0x00, 0x65, 0x00, /* property name "DeviceInterfaceGUIDs" */
    0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00, 0x49, 0x00, 0x6e, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00, 0x61, 0x00, 0x63, 0x00, 0x65, 0x00,
    0x47, 0x00, 0x55, 0x00, 0x49, 0x00, 0x44, 0x00, 0x73, 0x00, 0x00, 0x00,

    0x50, 0x00, 0x00, 0x00, /* property data length */

    0x7b, 0x00, 0x63, 0x00, /* property name: "{c15b4308-04d3-11e6-b3ea-6057189e6443}\0\0" */
    0x31, 0x00, 0x35, 0x00, 0x62, 0x00, 0x34, 0x00, 0x33, 0x00, 0x30, 0x00, 0x38, 0x00, 0x2d, 0x00, 0x30, 0x00, 0x34, 0x00, 0x64, 0x00, 0x33, 0x00, 0x2d, 0x00,
    0x31, 0x00, 0x31, 0x00, 0x65, 0x00, 0x36, 0x00, 0x2d, 0x00, 0x62, 0x00, 0x33, 0x00, 0x65, 0x00, 0x61, 0x00, 0x2d, 0x00, 0x36, 0x00, 0x30, 0x00, 0x35, 0x00,
    0x37, 0x00, 0x31, 0x00, 0x38, 0x00, 0x39, 0x00, 0x65, 0x00, 0x36, 0x00, 0x34, 0x00, 0x34, 0x00, 0x33, 0x00, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t *linux_WINUSB_IFx_WCIDProperties[] = {
    linux_WINUSB_IF0_WCIDProperties,
};

struct usb_msosv1_descriptor linux_msosv1_desc = {
    .string = linux_WCID_StringDescriptor_MSOS,
    .vendor_code = LINUX_USBD_GS_CAN_VENDOR_CODE,
    .compat_id = linux_WINUSB_WCIDDescriptor,
    .comp_id_property = linux_WINUSB_IFx_WCIDProperties,
};

static const uint8_t linux_device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0000, 0x01),
};

static const uint8_t linux_config_descriptor_hs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_HS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_HS, 0x00),
};

static const uint8_t linux_config_descriptor_fs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_FS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_FS, 0x00),
};

static const uint8_t linux_device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, 0x01),
};

static const uint8_t linux_other_speed_config_descriptor_hs[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 1, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_FS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_FS, 0x00),
};

static const uint8_t linux_other_speed_config_descriptor_fs[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 1, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xff, 0xff, 0x00, 0x04),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_IN, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_HS, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(GSUSB_ENDPOINT_OUT, USB_ENDPOINT_TYPE_BULK, USB_BULK_EP_MPS_HS, 0x00),
};

static const char *linux_string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "HScanT",                     /* Manufacturer */
    "HScanT USB to CAN adapter",  /* Product */
    "2024051701",                 /* Serial Number */
    "HScanT GS can0",             /* intf */
    "HScanT GS can1",             /* intf */
    "HScanT GS can2",             /* intf */
    "HScanT GS can3",             /* intf */
};

static const uint8_t *linux_device_descriptor_callback(uint8_t speed)
{
    (void)speed;

    return linux_device_descriptor;
}

static const uint8_t *linux_config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
        return linux_config_descriptor_hs;
    } else if (speed == USB_SPEED_FULL) {
        return linux_config_descriptor_fs;
    } else {
        return NULL;
    }
}

static const uint8_t *linux_device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;

    return linux_device_quality_descriptor;
}

static const uint8_t *linux_other_speed_config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
        return linux_other_speed_config_descriptor_hs;
    } else if (speed == USB_SPEED_FULL) {
        return linux_other_speed_config_descriptor_fs;
    } else {
        return NULL;
    }
}

static const char *linux_string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(linux_string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return linux_string_descriptors[index];
}

const struct usb_descriptor linux_gs_usb_descriptor = {
    .device_descriptor_callback = linux_device_descriptor_callback,
    .config_descriptor_callback = linux_config_descriptor_callback,
    .device_quality_descriptor_callback = linux_device_quality_descriptor_callback,
    .other_speed_descriptor_callback = linux_other_speed_config_descriptor_callback,
    .string_descriptor_callback = linux_string_descriptor_callback,
    .msosv1_descriptor = &linux_msosv1_desc,
};

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    (void)busid;

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

            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}

void linux_gs_usb_winusb_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
}

void linux_gs_usb_winusb_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(busid, ep, NULL, 0);
    } else {
    }
}

struct usbd_endpoint linux_gs_usb_out_ep = {
    .ep_addr = GSUSB_ENDPOINT_OUT,
    .ep_cb = linux_gs_usb_winusb_out
};

struct usbd_endpoint linux_gs_usb_in_ep = {
    .ep_addr = GSUSB_ENDPOINT_IN,
    .ep_cb = linux_gs_usb_winusb_in
};

struct usbd_interface linux_gs_usb_intf0;

void linux_gs_usb_init(uint8_t busid, uint32_t reg_base)
{
    usbd_desc_register(busid, &linux_gs_usb_descriptor);

    usbd_add_interface(busid, usbd_gs_can_init_intf(busid, &linux_gs_usb_intf0));
    usbd_add_endpoint(busid, &linux_gs_usb_out_ep);
    usbd_add_endpoint(busid, &linux_gs_usb_in_ep);

    usbd_initialize(busid, reg_base, usbd_event_handler);
}
