#include "usb_config.h"
#include "stdint.h"
#include "usb_dwc2_reg.h"

/* you can find this config in function: usb_global_init, file:at32fxxx_usb.c, for example:
 *
 *  usbx->gccfg_bit.pwrdown = TRUE;
 *  usbx->gccfg_bit.avalidsesen = TRUE;
 *  usbx->gccfg_bit.bvalidsesen = TRUE;
 * 
*/

uint32_t usbd_get_dwc2_gccfg_conf(void)
{
#ifdef CONFIG_USB_HS
    return (1 << 16);
#else
    // return ((1 << 16) | (1 << 18) | (1 << 19));
    return (1 << 16);
#endif
}

uint32_t usbh_get_dwc2_gccfg_conf(void)
{
#ifdef CONFIG_USB_DWC2_ULPI_PHY
    return (1 << 16);
#else
    return (1 << 16);
#endif
}