/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_HUB_H
#define USBH_HUB_H

#include "usbh_core.h"
#include "usb_hub.h"

#define USBH_HUB_MAX_PORTS 4
/* Maximum size of an interrupt IN transfer */
#define USBH_HUB_INTIN_BUFSIZE ((USBH_HUB_MAX_PORTS + 8) >> 3)

extern usb_slist_t hub_class_head;

#ifdef __cplusplus
extern "C" {
#endif
void usbh_roothub_thread_wakeup(uint8_t port);
int usbh_hub_initialize(void);
#ifdef __cplusplus
}
#endif

#endif /* USBH_HUB_H */
