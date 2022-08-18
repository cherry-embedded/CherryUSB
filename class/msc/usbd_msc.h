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

void usbd_msc_class_init(uint8_t out_ep, uint8_t in_ep);
void usbd_msc_get_cap(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
int usbd_msc_sector_read(uint32_t sector, uint8_t *buffer, uint32_t length);
int usbd_msc_sector_write(uint32_t sector, uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* USBD_MSC_H */
