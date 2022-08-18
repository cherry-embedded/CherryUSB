/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_RNDIS_H
#define USBD_RNDIS_H

#include "usb_cdc.h"

#define ETH_HEADER_SIZE 14
#define RNDIS_MTU            1500 /* MTU value */

#ifdef CONFIG_USB_HS
#define RNDIS_LINK_SPEED     12000000 /* Link baudrate (12Mbit/s for USB-FS) */
#else
#define RNDIS_LINK_SPEED     480000000 /* Link baudrate (480Mbit/s for USB-HS) */
#endif

#define CONFIG_RNDIS_RESP_BUFFER_SIZE 128

#ifdef __cplusplus
extern "C" {
#endif

void usbd_rndis_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* USBD_RNDIS_H */
