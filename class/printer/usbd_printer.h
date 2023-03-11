/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_PRINTER_H
#define USBD_PRINTER_H

#include "usb_printer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init printer interface driver */
struct usbd_interface *usbd_printer_init_intf(struct usbd_interface *intf, const uint8_t *device_id, uint8_t device_id_len);

#ifdef __cplusplus
}
#endif

#endif /* USBD_PRINTER_H */
