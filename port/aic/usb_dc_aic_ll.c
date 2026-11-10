/*
 * Copyright (c) 2022-2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aic_core.h>
#include <aic_hal.h>
#include <hal_syscfg.h>
#include "usbd_core.h"
#include "usb_dc_aic_reg.h"

extern irqreturn_t usbd_irq_handler(int irq, void * data);

uint32_t usbd_clk;
static unsigned char dma_sync_buffer[CACHE_LINE_SIZE] __attribute__((aligned(CACHE_LINE_SIZE)));
#define USB_PHY_TUNE_REG                0x8018
#define USB_PHY_TUNE_TXREF_OFFSET       8
#define USB_PHY_TUNE_TXREF_MASK         (0xF << USB_PHY_TUNE_TXREF_OFFSET)
#define USB_PHY_TUNE_TXPREEMPAMP_OFFSET 0
#define USB_PHY_TUNE_TXPREEMPAMP_MASK   3

void usb_dc_sync_dma(void)
{
    asm volatile("sw t0, (%0)" : : "r"(dma_sync_buffer));
    csi_dcache_clean_range((phy_addr_t)(ptr_t)dma_sync_buffer, CACHE_LINE_SIZE);
}
#ifndef AIC_USB_DEVICE_DRV_V10
int usbd_wait_pll_locked(void)
{
    uint32_t i = 0;

    while((readl(PLL_COM) & PLL_LOCKED) == 0) {
        i++;
        if (i >= 20000) {
            return -1;
        }
    }
    return 0;
}
#endif

void usbd_dc_set_phytune(void)
{
    uint32_t reg_val;

    /* Register default value is 0x8 */
    if (AIC_USB_PHY_TXREF != 0x8) {
        reg_val = readl((volatile void *)(USB_DEV_BASE + USB_PHY_TUNE_REG));
        reg_val &= ~USB_PHY_TUNE_TXREF_MASK;
        reg_val |= AIC_USB_PHY_TXREF << USB_PHY_TUNE_TXREF_OFFSET;
        writel(reg_val, (volatile void *)(USB_DEV_BASE + USB_PHY_TUNE_REG));
    }

    /* Register default value is 0 */
    if (AIC_USB_TXPREEMPAMP != 0) {
        reg_val = readl((volatile void *)(USB_DEV_BASE + USB_PHY_TUNE_REG));
        reg_val &= ~USB_PHY_TUNE_TXPREEMPAMP_MASK;
        reg_val |= AIC_USB_TXPREEMPAMP << USB_PHY_TUNE_TXPREEMPAMP_OFFSET;
        writel(reg_val, (volatile void *)(USB_DEV_BASE + USB_PHY_TUNE_REG));
    }
}

void usb_dc_low_level_init(void)
{
    /* set usb0 phy switch: Host/Device */
#if defined(AIC_USING_USB0_DEVICE) || defined(AIC_USING_USB0_OTG)
    hal_syscfg_usb_phy0_sw_host(0);
#endif

#ifndef AIC_USB_DEVICE_DRV_V10
    hal_syscfg_usb_pll_en();
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
#ifndef AIC_USB_DEVICE_DRV_V10
    if (usbd_wait_pll_locked() < 0)
        USB_LOG_ERR("usb pll unlock :%#lx \n", (long)(readl(PLL_COM)));

#endif

    /* Set USB PHY tune, default params are usually sufficient */
    usbd_dc_set_phytune();

    usbd_clk = hal_clk_get_freq(CONFIG_USB_AIC_DC_CLK);

    /* register interrupt callback */
    aicos_request_irq(CONFIG_USB_AIC_DC_IRQ_NUM, usbd_irq_handler,
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

