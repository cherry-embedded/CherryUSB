/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_MSC_H
#define USBD_MSC_H

#include "usb_msc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init msc interface driver */
struct usbd_interface *usbd_msc_init_intf(struct usbd_interface *intf,
                                          const uint8_t out_ep,
                                          const uint8_t in_ep);

void usbd_msc_get_cap(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
int usbd_msc_sector_read(uint32_t sector, uint8_t *buffer, uint32_t length);
int usbd_msc_sector_write(uint32_t sector, uint8_t *buffer, uint32_t length);

void usbd_msc_set_readonly(bool readonly);

#ifdef __cplusplus
}
#endif

#endif /* USBD_MSC_H */
