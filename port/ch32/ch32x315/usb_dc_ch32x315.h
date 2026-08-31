/*
 * Copyright (c) 2026, CherryUSB contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_DC_CH32X315_H
#define USB_DC_CH32X315_H

#include <stdint.h>

int usb_dc_ch32x315_low_level_init(uint8_t busid);
int usb_dc_ch32x315_low_level_deinit(uint8_t busid);
void usb_dc_ch32x315_bus_reset(void);
void usb_dc_ch32x315_irq_enable(uint8_t busid);
void usb_dc_ch32x315_irq_disable(uint8_t busid);
void usb_dc_ch32x315_delay_ms(uint32_t ms);
void usb_dc_ch32x315_irq_handler(void);

#endif /* USB_DC_CH32X315_H */
