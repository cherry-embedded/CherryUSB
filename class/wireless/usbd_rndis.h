/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_RNDIS_H
#define USBD_RNDIS_H

#include "usb_cdc.h"

#define ETH_HEADER_SIZE         14
#define CONFIG_USBDEV_RNDIS_MTU 1500 /* MTU value */

#ifndef CONFIG_USB_HS
#define RNDIS_LINK_SPEED 12000000 /* Link baudrate (12Mbit/s for USB-FS) */
#else
#define RNDIS_LINK_SPEED 480000000 /* Link baudrate (480Mbit/s for USB-HS) */
#endif

#define CONFIG_USBDEV_RNDIS_RESP_BUFFER_SIZE 128

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_interface *usbd_rndis_alloc_intf(uint8_t int_ep, uint8_t mac[6], uint32_t vendor_id, uint8_t *vendor_desc);

#ifdef __cplusplus
}
#endif

#endif /* USBD_RNDIS_H */
