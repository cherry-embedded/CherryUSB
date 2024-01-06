/*
 * Copyright (c) 2022, Artinchip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aic_core.h>
#include <aic_hal.h>
#include <hal_syscfg.h>
#include "usbh_core.h"
#include "usb_ehci_priv.h"

extern void USBH_IRQHandler(struct usbh_bus *bus);

const uint8_t aic_irq_table[] = {
    USB_HOST0_EHCI_IRQn,
#ifdef HPM_USB1_BASE
    USB_HOST1_EHCI_IRQn
#endif
};

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    uint32_t val;

    /* set usb0 phy switch: Host/Device */
#ifdef AIC_USING_USB0_HOST
    syscfg_usb_phy0_sw_host(1);
#endif

    /* set phy type: UTMI/ULPI */
    val = readl((volatile void *)(unsigned long)(bus->hcd.reg_base+0x800));
#ifdef FPGA_BOARD_ARTINCHIP
    /* fpga phy type = ULPI */
    writel((val  & ~0x1U), (volatile void *)(unsigned long)(bus->hcd.reg_base+0x800));
#else
    /* board phy type = UTMI */
    writel((val | 0x1), (volatile void *)(unsigned long)(bus->hcd.reg_base+0x800));
#endif

#if 0
    /* Set AHB2STBUS_INSREG01
        Set EHCI packet buffer IN/OUT threshold (in DWORDs)
        Must increase the OUT threshold to avoid underrun. (FIFO size - 4)
    */
#ifdef FPGA_BOARD_ARTINCHIP
    writel((32 | (127 << 16)), (volatile void *)(unsigned long)(bus->hcd.reg_base+0x94));
#else
    writel((32 | (32 << 16)), (volatile void *)(unsigned long)(bus->hcd.reg_base+0x94));
#endif
#endif

    /* enable clock */
    hal_clk_enable(CONFIG_USB_EHCI_PHY_CLK);
    hal_clk_enable(CONFIG_USB_EHCI_CLK);
    aicos_udelay(300);
    hal_reset_assert(CONFIG_USB_EHCI_PHY_RESET);
    hal_reset_assert(CONFIG_USB_EHCI_RESET);
    aicos_udelay(300);
    hal_reset_deassert(CONFIG_USB_EHCI_PHY_RESET);
    hal_reset_deassert(CONFIG_USB_EHCI_RESET);
    aicos_udelay(300);

    /* register interrupt callback */
    aicos_request_irq(aic_irq_table[bus->hcd.hcd_id], (irq_handler_t)USBH_IRQHandler,
                      0, "usb_host_ehci", bus);
    aicos_irq_enable(aic_irq_table[bus->hcd.hcd_id]);
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

void usb_ehci_dcache_clean(uintptr_t addr, uint32_t len)
{
    aicos_dcache_clean_range((size_t *)addr, len);
}

void usb_ehci_dcache_invalidate(uintptr_t addr, uint32_t len)
{
    aicos_dcache_invalid_range((size_t *)addr, len);
}

void usb_ehci_dcache_clean_invalidate(uintptr_t addr, uint32_t len)
{
    aicos_dcache_clean_invalid_range((size_t *)addr, len);
}
