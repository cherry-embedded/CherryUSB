#include "usbh_core.h"
#include "hpm_common.h"
#include "hpm_soc.h"
#include "hpm_usb_drv.h"

#define USB_PHY_INIT_DELAY_COUNT (16U) /**< a delay count for USB phy initialization */

/* Initialize USB phy */
static void usb_phy_init(USB_Type *ptr)
{
    uint32_t status;

    ptr->OTG_CTRL0 |= USB_OTG_CTRL0_OTG_UTMI_RESET_SW_MASK;     /* set otg_utmi_reset_sw for naneng usbphy */
    ptr->OTG_CTRL0 &= ~USB_OTG_CTRL0_OTG_UTMI_SUSPENDM_SW_MASK; /* clr otg_utmi_suspend_m for naneng usbphy */
    ptr->PHY_CTRL1 &= ~USB_PHY_CTRL1_UTMI_CFG_RST_N_MASK;       /* clr cfg_rst_n */

    do {
        status = USB_OTG_CTRL0_OTG_UTMI_RESET_SW_GET(ptr->OTG_CTRL0); /* wait for reset status */
    } while (status == 0);

    ptr->OTG_CTRL0 |= USB_OTG_CTRL0_OTG_UTMI_SUSPENDM_SW_MASK; /* set otg_utmi_suspend_m for naneng usbphy */

    for (int i = 0; i < USB_PHY_INIT_DELAY_COUNT; i++) {
        ptr->PHY_CTRL0 = USB_PHY_CTRL0_GPIO_ID_SEL_N_SET(0); /* used for delay */
    }

    ptr->OTG_CTRL0 &= ~USB_OTG_CTRL0_OTG_UTMI_RESET_SW_MASK; /* clear otg_utmi_reset_sw for naneng usbphy */

    /* otg utmi clock detection */
    ptr->PHY_STATUS |= USB_PHY_STATUS_UTMI_CLK_VALID_MASK; /* write 1 to clear valid status */
    do {
        status = USB_PHY_STATUS_UTMI_CLK_VALID_GET(ptr->PHY_STATUS); /* get utmi clock status */
    } while (status == 0);

    ptr->PHY_CTRL1 |= USB_PHY_CTRL1_UTMI_CFG_RST_N_MASK; /* set cfg_rst_n */

    ptr->PHY_CTRL1 |= USB_PHY_CTRL1_UTMI_OTG_SUSPENDM_MASK; /* set otg_suspendm */
}

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
    usb_phy_init((USB_Type *)HPM_USB0_BASE);
    intc_m_enable_irq(IRQn_USB0);
}

void usb_hc_low_level2_init()
{
    usb_host_mode_init((USB_Type *)HPM_USB0_BASE);
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    uint8_t speed;

    speed = usb_get_port_speed((USB_Type *)HPM_USB0_BASE);

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

void isr_usb0(void)
{
    USBH_IRQHandler();
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB0, isr_usb0)

#ifdef HPM_USB1_BASE
void isr_usb1(void)
{
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB1, isr_usb1)
#endif