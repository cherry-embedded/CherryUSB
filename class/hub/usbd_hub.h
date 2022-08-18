/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_HUB_H
#define USBD_HUB_H

#include "usb_hub.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_hub_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* USBD_HUB_H */
