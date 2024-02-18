#include "usb_config.h"
#include "stdint.h"
#include "usb_dwc2_reg.h"

/* you can find this config in function: USB_DevInit, file:stm32xxx_ll_usb.c, for example:
 *
 *  USBx->GCCFG |= USB_OTG_GCCFG_PWRDWN;
 *  USBx->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
 *  USBx->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
 *  USBx->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
 * 
*/

uint32_t usbd_get_dwc2_gccfg_conf(uint32_t reg_base)
{
#ifdef CONFIG_USB_HS
    return 0;
#else
#if __has_include("stm32h7xx.h") || __has_include("stm32f7xx.h") || __has_include("stm32l4xx.h")
#define USB_OTG_GLB ((USB_OTG_GlobalTypeDef *)(reg_base))
    /* B-peripheral session valid override enable */
    USB_OTG_GLB->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG_GLB->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
    return (1 << 16);
#else
    return ((1 << 16) | (1 << 21));
#endif
#endif
}

uint32_t usbh_get_dwc2_gccfg_conf(uint32_t reg_base)
{
#ifdef CONFIG_USB_DWC2_ULPI_PHY
    return 0;
#else
#if __has_include("stm32h7xx.h") || __has_include("stm32f7xx.h") || __has_include("stm32l4xx.h")
#define USB_OTG_GLB ((USB_OTG_GlobalTypeDef *)(reg_base))
    /* B-peripheral session valid override enable */
    USB_OTG_GLB->GOTGCTL &= ~USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG_GLB->GOTGCTL &= ~USB_OTG_GOTGCTL_BVALOVAL;
    return (1 << 16);
#else
    return ((1 << 16) | (1 << 21));
#endif
#endif
}