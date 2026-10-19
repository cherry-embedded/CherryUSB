/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_GS_USB_H
#define USBD_GS_USB_H

#include "gs_usb.h"

/* Init gs usb interface driver */
struct usbd_interface *usbd_gs_usb_init_intf(struct usbd_interface *intf);

/* Setup request command callback api */
int usbd_gs_usb_can_get(uint16_t can_channel, uint8_t bRequest, uint8_t *data, uint32_t len);
int usbd_gs_usb_can_set(uint16_t can_channel, uint8_t bRequest, uint8_t *data, uint32_t len);

#endif // USBD_GS_USB_H