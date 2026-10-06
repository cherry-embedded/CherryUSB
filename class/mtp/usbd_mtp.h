/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_MTP_H
#define USBD_MTP_H

#include "usb_mtp.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_interface *usbd_mtp_init_intf(struct usbd_interface *intf,
                                          const uint8_t out_ep,
                                          const uint8_t in_ep,
                                          const uint8_t int_ep);

int usbd_mtp_notify_object_add(const char *path);
int usbd_mtp_notify_object_remove(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* USBD_MTP_H */
