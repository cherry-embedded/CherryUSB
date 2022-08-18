/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_DFU_H
#define USBD_DFU_H

#include "usb_dfu.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_dfu_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* USBD_DFU_H */
