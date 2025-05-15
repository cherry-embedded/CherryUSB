/*
 * Copyright (c) 2022-2025 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "hpm_common.h"
#include "hpm_soc.h"

void (*g_usb_hpm_irq[2])(uint8_t busid);
uint8_t g_usb_hpm_busid[2] = { 0, 0 };

SDK_DECLARE_EXT_ISR_M(IRQn_USB0, isr_usbh0)
void isr_usbh0(void)
{
    g_usb_hpm_irq[0](g_usb_hpm_busid[0]);
}

#ifdef HPM_USB1_BASE
SDK_DECLARE_EXT_ISR_M(IRQn_USB1, isr_usbh1)
void isr_usbh1(void)
{
    g_usb_hpm_irq[1](g_usb_hpm_busid[1]);
}
#endif