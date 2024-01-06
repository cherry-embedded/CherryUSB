#include "usbh_core.h"
#include "hpm_common.h"
#include "hpm_soc.h"
#include "hpm_usb_drv.h"

#if !defined(CONFIG_USB_EHCI_HPMICRO) || !CONFIG_USB_EHCI_HPMICRO
#error "hpm ehci must set CONFIG_USB_EHCI_HPMICRO=1"
#endif

#if !defined(CONFIG_USB_EHCI_HCOR_OFFSET) || CONFIG_USB_EHCI_HCOR_OFFSET != 0x140
#error "hpm ehci must config CONFIG_USB_EHCI_HCOR_OFFSET to 0x140"
#endif

#if defined(CONFIG_USB_EHCI_PRINT_HW_PARAM) || !defined(CONFIG_USB_EHCI_PORT_POWER)
#error "hpm ehci must enable CONFIG_USB_EHCI_PORT_POWER and disable CONFIG_USB_EHCI_PRINT_HW_PARAM"
#endif

struct usbh_bus *hpm_usb_bus0;

#ifdef HPM_USB1_BASE
struct usbh_bus *hpm_usb_bus1;
#endif

const uint8_t hpm_irq_table[] = {
    IRQn_USB0,
#ifdef HPM_USB1_BASE
    IRQn_USB1
#endif
};

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

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    usb_phy_init((USB_Type *)(bus->hcd.reg_base));
    intc_m_enable_irq(hpm_irq_table[bus->hcd.hcd_id]);
}

void usb_hc_low_level2_init(struct usbh_bus *bus)
{
    usb_host_mode_init((USB_Type *)(bus->hcd.reg_base));
}

uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port)
{
    (void)port;
    uint8_t speed;

    speed = usb_get_port_speed((USB_Type *)(bus->hcd.reg_base));

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

extern void USBH_IRQHandler(struct usbh_bus *bus);

void isr_usbh0(void)
{
    USBH_IRQHandler(hpm_usb_bus0);
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB0, isr_usbh0)

#ifdef HPM_USB1_BASE
void isr_usbh1(void)
{
    USBH_IRQHandler(hpm_usb_bus1);
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB1, isr_usbh1)
#endif
