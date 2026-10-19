/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_gs_usb.h"

static int gs_usb_vendor_class_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("GS USB Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    uint16_t can_channel = setup->wValue;

    switch (setup->bRequest) {
        case GS_USB_BREQ_BT_CONST:
            *len = sizeof(struct gs_device_bt_const);
            return usbd_gs_usb_can_get(can_channel, setup->bRequest, *data, *len);
        case GS_USB_BREQ_DEVICE_CONFIG:
            *len = sizeof(struct gs_device_config);
            return usbd_gs_usb_can_get(can_channel, setup->bRequest, *data, *len);
        case GS_USB_BREQ_TIMESTAMP:
            *len = sizeof(uint32_t);
            return usbd_gs_usb_can_get(can_channel, setup->bRequest, *data, *len);
        case GS_USB_BREQ_BT_CONST_EXT:
            *len = sizeof(struct gs_device_bt_const_extended);
            return usbd_gs_usb_can_get(can_channel, setup->bRequest, *data, *len);
        case GS_USB_BREQ_GET_TERMINATION:
            *len = sizeof(struct gs_device_termination_state);
            return usbd_gs_usb_can_get(can_channel, setup->bRequest, *data, *len);
        case GS_USB_BREQ_GET_STATE:
            *len = sizeof(struct gs_device_state);
            return usbd_gs_usb_can_get(can_channel, setup->bRequest, *data, *len);
        case GS_USB_BREQ_HOST_FORMAT:
        case GS_USB_BREQ_BITTIMING:
        case GS_USB_BREQ_MODE:
        case GS_USB_BREQ_IDENTIFY:
        case GS_USB_BREQ_DATA_BITTIMING:
        case GS_USB_BREQ_SET_TERMINATION:
            return usbd_gs_usb_can_set(can_channel, setup->bRequest, *data, *len);
        default:
            return -1;
    }
    return 0;
}

struct usbd_interface *usbd_gs_usb_init_intf(struct usbd_interface *intf)
{
    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = gs_usb_vendor_class_request_handler;
    intf->notify_handler = NULL;

    return intf;
}

__WEAK int usbd_gs_usb_can_get(uint16_t can_channel, uint8_t bRequest, uint8_t *data, uint32_t len)
{
    (void)can_channel;
    (void)bRequest;
    (void)data;
    (void)len;

    switch (bRequest) {
        case GS_USB_BREQ_BT_CONST:
            break;
        case GS_USB_BREQ_DEVICE_CONFIG:
            break;
        case GS_USB_BREQ_TIMESTAMP:
            break;
        case GS_USB_BREQ_BT_CONST_EXT:
            break;
        case GS_USB_BREQ_GET_TERMINATION:
            break;
        case GS_USB_BREQ_GET_STATE:
            break;

        default:
            break;
    }

    return 0;
}

__WEAK int usbd_gs_usb_can_set(uint16_t can_channel, uint8_t bRequest, uint8_t *data, uint32_t len)
{
    (void)can_channel;
    (void)bRequest;
    (void)data;
    (void)len;

    switch (bRequest) {
        case GS_USB_BREQ_HOST_FORMAT:
            break;
        case GS_USB_BREQ_BITTIMING:
            break;
        case GS_USB_BREQ_MODE:
            break;
        case GS_USB_BREQ_IDENTIFY:
            break;
        case GS_USB_BREQ_DATA_BITTIMING:
            break;
        case GS_USB_BREQ_SET_TERMINATION:
            break;

        default:
            break;
    }

    return 0;
}