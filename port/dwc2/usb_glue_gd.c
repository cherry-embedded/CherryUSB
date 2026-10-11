/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbh_core.h"
#include "usb_dwc2_param.h"

#if !defined(CONFIG_USB_DWC2_GD)
#error must define CONFIG_USB_DWC2_GD when use GD32 chips
#endif

// TODO: Implement this function to read the GD32-specific USB controller registers.
void dwc2_get_gd_reg(uint32_t reg_base,
                     uint32_t *snpsid,
                     uint32_t *hwcfg1,
                     uint32_t *hwcfg2,
                     uint32_t *hwcfg3,
                     uint32_t *hwcfg4)
{
}

// TODO
void usb_dc_low_level_init(uint8_t busid)
{
}

// TODO
void usb_dc_low_level_deinit(uint8_t busid)
{
}

// TODO
void usb_hc_low_level_init(struct usbh_bus *bus)
{
}

// TODO
void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
}

// TODO
#ifndef CONFIG_USB_DWC2_CUSTOM_PARAM
void dwc2_get_user_params(uint32_t reg_base, struct dwc2_user_params *params)
{
    // if (reg_base == 0x40040000UL) { // USB_OTG_HS_PERIPH_BASE
    //     memcpy(params, &param_pb14_pb15, sizeof(struct dwc2_user_params));
    // } else {
    //     memcpy(params, &param_pa11_pa12, sizeof(struct dwc2_user_params));
    // }
#ifdef CONFIG_USB_DWC2_CUSTOM_FIFO
    struct usb_dwc2_user_fifo_config s_dwc2_fifo_config;

    dwc2_get_user_fifo_config(reg_base, &s_dwc2_fifo_config);

    params->device_rx_fifo_size = s_dwc2_fifo_config.device_rx_fifo_size;
    for (uint8_t i = 0; i < MAX_EPS_CHANNELS; i++) {
        params->device_tx_fifo_size[i] = s_dwc2_fifo_config.device_tx_fifo_size[i];
    }
#endif
}
#endif

extern uint32_t SystemCoreClock;

void usbd_dwc2_delay_ms(uint8_t ms)
{
    uint32_t count = SystemCoreClock / 1000 * ms;
    while (count--) {
        __asm volatile("nop");
    }
}

uint32_t usbd_dwc2_get_system_clock(void)
{
    return SystemCoreClock;
}

#ifdef CONFIG_USB_DCACHE_ENABLE
void usb_dcache_clean(uintptr_t addr, size_t size)
{
    SCB_CleanDCache_by_Addr((void *)addr, size);
}

void usb_dcache_invalidate(uintptr_t addr, size_t size)
{
    SCB_InvalidateDCache_by_Addr((void *)addr, size);
}

void usb_dcache_flush(uintptr_t addr, size_t size)
{
    SCB_CleanInvalidateDCache_by_Addr((void *)addr, size);
}
#endif