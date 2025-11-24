/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBOTG_CORE_H
#define USBOTG_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_core.h"
#include "usbh_core.h"
#include "usb_otg.h"

int usbotg_initialize(uint8_t busid, uint32_t reg_base, void *device_event_callback, void *host_event_callback);
int usbotg_deinitialize(uint8_t busid);

/* called by user */
void usbotg_trigger_role_change(uint8_t busid, uint8_t mode);

void USBOTG_IRQHandler(uint8_t busid);

#ifdef __cplusplus
}
#endif

#endif /* USBOTG_CORE_H */