#include "usb_config.h"
#include "stdint.h"
#include "usb_dwc2_reg.h"

/* st different chips maybe have a little difference in this register, you should check this */

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