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

void usbd_printer_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* USBD_PRINTER_H */
