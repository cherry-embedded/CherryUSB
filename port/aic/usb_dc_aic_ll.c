﻿/*
 * Copyright (c) 2022-2024, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aic_core.h>
#include <aic_hal.h>
#include <hal_syscfg.h>
#include "usbd_core.h"
#include "usb_dc_aic_reg.h"

extern irqreturn_t USBD_IRQHandler(int irq, void * data);

uint32_t usbd_clk;
static unsigned char dma_sync_buffer[CACHE_LINE_SIZE] __attribute__((aligned(CACHE_LINE_SIZE)));

void usb_dc_sync_dma(void)
{
    asm volatile("sw t0, (%0)" : : "r"(dma_sync_buffer));
    csi_dcache_clean_range((phy_addr_t)(ptr_t)dma_sync_buffer, CACHE_LINE_SIZE);
}

void usb_dc_low_level_init(void)
{
    /* set usb0 phy switch: Host/Device */
#if defined(AIC_USING_USB0_DEVICE) || defined(AIC_USING_USB0_OTG)
    hal_syscfg_usb_phy0_sw_host(0);
#endif
    /* set pin-mux */

    /* enable clock */
    hal_clk_enable(CONFIG_USB_AIC_DC_PHY_CLK);
    hal_clk_enable(CONFIG_USB_AIC_DC_CLK);
    aicos_udelay(300);
    hal_reset_assert(CONFIG_USB_AIC_DC_PHY_RESET);
    hal_reset_assert(CONFIG_USB_AIC_DC_RESET);
    aicos_udelay(300);
    hal_reset_deassert(CONFIG_USB_AIC_DC_PHY_RESET);
    hal_reset_deassert(CONFIG_USB_AIC_DC_RESET);
    aicos_udelay(300);

    usbd_clk = hal_clk_get_freq(CONFIG_USB_AIC_DC_CLK);

    /* register interrupt callback */
    aicos_request_irq(CONFIG_USB_AIC_DC_IRQ_NUM, USBD_IRQHandler,
                      0, "usb_device", NULL);
    aicos_irq_enable(CONFIG_USB_AIC_DC_IRQ_NUM);
}

void usb_dc_low_level_deinit(void)
{
    aicos_irq_disable(CONFIG_USB_AIC_DC_IRQ_NUM);

    hal_reset_assert(CONFIG_USB_AIC_DC_PHY_RESET);
    hal_reset_assert(CONFIG_USB_AIC_DC_RESET);
    hal_clk_disable(CONFIG_USB_AIC_DC_PHY_CLK);
    hal_clk_disable(CONFIG_USB_AIC_DC_CLK);
}

