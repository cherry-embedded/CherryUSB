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

uint32_t usbd_get_dwc2_gccfg_conf(void)
{
#ifdef CONFIG_USB_HS
    return 0;
#else
    return ((1 << 16) | (1 << 21));
#endif
}

uint32_t usbh_get_dwc2_gccfg_conf(void)
{
#ifdef CONFIG_USB_DWC2_ULPI_PHY
    return 0;
#else
    return ((1 << 16) | (1 << 21));
#endif
}