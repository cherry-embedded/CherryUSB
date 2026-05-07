/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "stdint.h"
#include "usb_dwc2_reg.h"
#include "usb_dwc2_param.h"
#include "board_config.h"

static const struct dwc2_user_params param_pa11_pa12 = {
    .phy_type = DWC2_PHY_TYPE_PARAM_FS,
    .device_dma_enable = false,
    .device_dma_desc_enable = false,
    .device_rx_fifo_size = (320 - 16 * 4),
    .device_tx_fifo_size = {
        [0] = 16, // 64 byte
        [1] = 16, // 64 byte
        [2] = 16, // 64 byte
        [3] = 16, // 64 byte
        [4] = 0,
        [5] = 0,
        [6] = 0,
        [7] = 0,
        [8] = 0,
        [9] = 0,
        [10] = 0,
        [11] = 0,
        [12] = 0,
        [13] = 0,
        [14] = 0,
        [15] = 0 },
    .device_gccfg = ((1 << 16) | (1 << 19) |(1 << 21)),
    .total_fifo_size = 320 // 1280 byte
};

#if CONFIG_USBDEV_EP_NUM != 4 
#error "gd32 only has 4 endpoints for pa11/pa12"
#endif

void usb_dc_low_level_init(uint8_t busid) {
  NVIC_EnableIRQ(USBFS_IRQn);
}

void usb_dc_low_level_deinit(uint8_t busid) {
  NVIC_DisableIRQ(USBFS_IRQn);
}

#ifndef CONFIG_USB_DWC2_CUSTOM_PARAM
void dwc2_get_user_params(uint32_t reg_base, struct dwc2_user_params *params)
{
    memcpy(params, &param_pa11_pa12, sizeof(struct dwc2_user_params));
    
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
void dwc2_override_hw_params(uint32_t reg_base, struct dwc2_hw_params *hw) {
    /* HWCFG2 reads 0, this is unknown why */
    if(reg_base == 0x50000000UL) {
        hw->num_dev_ep = CONFIG_USBDEV_EP_NUM - 1;
    }
    /* TODO: For other GD32, potentially will need to update like this as well */
}

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