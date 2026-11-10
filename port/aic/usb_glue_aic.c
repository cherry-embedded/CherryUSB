/*
 * Copyright (c) 2022-2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aic_core.h>
#include <aic_hal.h>
#include <hal_syscfg.h>
#include "usbh_core.h"
#include "usb_ehci_priv.h"

#if defined(LPKG_CHERRYUSB_HOST_OHCI)
#include "../ohci/usb_hc_ohci.h"

static void aic_ohci_isr(int vector, void *arg)
{
    struct usbh_bus *bus = (struct usbh_bus *)arg;
    extern void OHCI_IRQHandler(struct usbh_bus *bus);
    OHCI_IRQHandler(bus);
}
#endif

static void aic_ehci_isr(int vector, void *arg)
{
    struct usbh_bus *bus = (struct usbh_bus *)arg;
    extern void usbh_irq_handler(struct usbh_bus *bus);
    usbh_irq_handler(bus);
}

typedef struct aic_ehci_config {
    uint32_t base_addr;
    uint32_t clk_id;
    uint32_t rst_id;
    uint32_t phy_clk_id;
    uint32_t phy_rst_id;
    uint32_t irq_num;
}aic_ehci_config_t;

aic_ehci_config_t config[] = {
#if defined(AIC_USING_USB0_HOST) || defined(AIC_USING_USB0_OTG)
    {
        USB_HOST0_BASE,
        CLK_USBH0,
        RESET_USBH0,
        CLK_USB_PHY0,
        RESET_USBPHY0,
        USB_HOST0_EHCI_IRQn
    },
#else
    {
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF,
        0xFFFFFFFF
    },
#endif
#ifdef AIC_USING_USB1_HOST
    {
        USB_HOST1_BASE,
        CLK_USBH1,
        RESET_USBH1,
        CLK_USB_PHY1,
        RESET_USBPHY1,
        USB_HOST1_EHCI_IRQn
    }
#endif
};

#if defined(AIC_SYSCFG_DRV_V16)
#define PLL_COM                         (USB_HOST0_BASE + 0x878)
#define PLL_LOCKED                      BIT(23)

int usbh_wait_pll_locked(void)
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
void usb_hc_low_level_init(struct usbh_bus *bus)
{
    uint32_t val;
    int i = 0;

#if defined(AIC_SYSCFG_DRV_V16)
    hal_syscfg_usb_pll_en();
#endif

    for (i=0; i<sizeof(config)/sizeof(aic_ehci_config_t); i++) {
        if (bus->hcd.reg_base == config[i].base_addr)
            break;
    }

    if (i == sizeof(config)/sizeof(aic_ehci_config_t))
        return;

    /* set usb0 phy switch: Host/Device */
    if (i == 0)
        hal_syscfg_usb_phy0_sw_host(1);

    /* enable clock */
    hal_clk_enable(config[i].phy_clk_id);
    hal_clk_enable(config[i].clk_id);
    aicos_udelay(300);
    hal_reset_assert(config[i].phy_rst_id);
    hal_reset_assert(config[i].rst_id);
    aicos_udelay(300);
    hal_reset_deassert(config[i].phy_rst_id);
    hal_reset_deassert(config[i].rst_id);
    aicos_udelay(300);

#if defined(AIC_SYSCFG_DRV_V16)
    if (usbh_wait_pll_locked() < 0)
        USB_LOG_ERR("usb pll unlock :%#lx \n", (long)(readl(PLL_COM)));
#endif

    /* set phy type: UTMI/ULPI */
    val = readl((volatile void *)(unsigned long)(config[i].base_addr+0x800));
#ifdef FPGA_BOARD_ARTINCHIP
    /* fpga phy type = ULPI */
    writel((val  & ~0x1U), (volatile void *)(unsigned long)(config[i].base_addr+0x800));
#else
    /* board phy type = UTMI */
    writel((val | 0x1 | 0xe << 8), (volatile void *)(unsigned long)(config[i].base_addr+0x800));
#endif

    /* Set AHB2STBUS_INSREG01
        Set EHCI packet buffer IN/OUT threshold (in DWORDs)
        Must increase the OUT threshold to avoid underrun. (FIFO size - 4)
    */
    writel((32 | (127 << 16)), (volatile void *)(unsigned long)(config[i].base_addr+0x94));

    /* register interrupt callback */
    aicos_request_irq(config[i].irq_num, (irq_handler_t)aic_ehci_isr,
                       0, "usb_host_ehci", bus);
    aicos_irq_enable(config[i].irq_num);
#if defined(LPKG_CHERRYUSB_HOST_OHCI)
    aicos_request_irq(config[i].irq_num + 1, (irq_handler_t)aic_ohci_isr,
                      0, "usb_host_ohci", bus);
    aicos_irq_enable(config[i].irq_num + 1);
#endif
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    int i = 0;

    for (i=0; i<sizeof(config)/sizeof(aic_ehci_config_t); i++) {
        if (bus->hcd.reg_base == config[i].base_addr)
            break;
    }

    if (i == sizeof(config)/sizeof(aic_ehci_config_t))
        return;

    aicos_irq_disable(config[i].irq_num);
#if defined(LPKG_CHERRYUSB_HOST_OHCI)
    aicos_irq_disable(config[i].irq_num + 1);
#endif
    hal_reset_assert(config[i].phy_rst_id);
    hal_reset_assert(config[i].rst_id);
    hal_clk_disable(config[i].phy_clk_id);
    hal_clk_disable(config[i].clk_id);
}

uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port)
{
    /* Defined by individual manufacturers */
    uint32_t regval;

    regval = EHCI_HCOR->portsc[port-1];
    if ((regval & EHCI_PORTSC_LSTATUS_MASK) == EHCI_PORTSC_LSTATUS_KSTATE)
        return USB_SPEED_LOW;

    if (regval & EHCI_PORTSC_PE)
        return USB_SPEED_HIGH;
    else
        return USB_SPEED_FULL;
}

void usb_dcache_clean(uintptr_t addr, uint32_t len)
{
    aicos_dcache_clean_range((size_t *)addr, len);
}

void usb_dcache_invalidate(uintptr_t addr, uint32_t len)
{
    aicos_dcache_invalid_range((size_t *)addr, len);
}

void usb_dcache_clean_invalidate(uintptr_t addr, uint32_t len)
{
    aicos_dcache_clean_invalid_range((size_t *)addr, len);
}


