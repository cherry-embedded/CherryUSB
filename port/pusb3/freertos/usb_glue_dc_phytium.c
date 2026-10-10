#include "usbd_core.h"
#include "ftypes.h"
#include "fparameters.h"

void usb_sys_mem_init(void);
void usb_sys_mem_deinit(void);
void usb_dc_setup_pusb3_interrupt(u32 id);
void usb_dc_revoke_pusb3_interrupt(u32 id);

extern u8 usb_otg_is_active(void);
extern void usb_dc_setup_pusb3_dev_interrupt(u32 id);

void usb_dc_low_level_init(void)
{
    usb_sys_mem_init();

    if (usb_otg_is_active()) {
        /* OTG IRQ already installed by usb_otg_init.
         * Only install the DC IRQ here. */
        usb_dc_setup_pusb3_dev_interrupt(FUSB_ID_0);
    } else {
        usb_dc_setup_pusb3_interrupt(FUSB_ID_0);
    }
}

void usb_dc_low_level_deinit(void)
{
    usb_dc_revoke_pusb3_interrupt(FUSB_ID_0);
    usb_sys_mem_deinit();
}

__WEAK void USBOTG_IRQHandler(uint8_t busid)
{
}

__WEAK void USBD_IRQHandler(uint8_t busid)
{
}
