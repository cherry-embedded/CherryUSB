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

/* Alloc dfu interface driver */
struct usbd_interface *usbd_dfu_alloc_intf(void);

#ifdef __cplusplus
}
#endif

#endif /* USBD_DFU_H */
