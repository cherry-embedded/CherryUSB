#include "usbh_core.h"
#include "hpm_common.h"
#include "hpm_soc.h"
#include "hpm_usb_drv.h"

#if !defined(CONFIG_USB_EHCI_HPMICRO) || !CONFIG_USB_EHCI_HPMICRO
#error "hpm ehci must set CONFIG_USB_EHCI_HPMICRO=1"
#endif

#if !defined(CONFIG_HPM_USBH_BASE) || !defined(CONFIG_HPM_USBH_IRQn)
#error "hpm ehci must config CONFIG_HPM_USBH_BASE and CONFIG_HPM_USBH_IRQn"
#endif

static void usb_host_mode_init(USB_Type *ptr)
{
    /* Set mode to host, must be set immediately after reset */
    ptr->USBMODE &= ~USB_USBMODE_CM_MASK;
    ptr->USBMODE |= USB_USBMODE_CM_SET(3);

    /* Set the endian */
    ptr->USBMODE &= ~USB_USBMODE_ES_MASK;

    /* Set parallel interface signal */
    ptr->PORTSC1 &= ~USB_PORTSC1_STS_MASK;

    /* Set parallel transceiver width */
    ptr->PORTSC1 &= ~USB_PORTSC1_PTW_MASK;

    /* Not use interrupt threshold. */
    ptr->USBCMD &= ~USB_USBCMD_ITC_MASK;
}

void usb_hc_low_level_init()
{
    usb_phy_init((USB_Type *)CONFIG_HPM_USBH_BASE);
    intc_m_enable_irq(CONFIG_HPM_USBH_IRQn);
}

void usb_hc_low_level2_init()
{
    usb_host_mode_init((USB_Type *)CONFIG_HPM_USBH_BASE);
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    uint8_t speed;

    speed = usb_get_port_speed((USB_Type *)CONFIG_HPM_USBH_BASE);

    if (speed == 0x00) {
        return USB_SPEED_FULL;
    }
    if (speed == 0x01) {
        return USB_SPEED_LOW;
    }
    if (speed == 0x02) {
        return USB_SPEED_HIGH;
    }

    return 0;
}

extern void USBH_IRQHandler(void);

void isr_usb(void)
{
    USBH_IRQHandler();
}
SDK_DECLARE_EXT_ISR_M(CONFIG_HPM_USBH_IRQn, isr_usb)
