#include "usb_def.h"
#include "vc0768.h"
#include "usb_hc_ehci.h"
#include "usb_ehci_priv.h"

#define VC0728_USB_EHCI_CLK_BASE        (0x60055310)
#define VC0728_USB_EHCI_CLK1_BASE       (0x60055314)
#define VC0728_USB_EHCI_PHYCKL_BASE     (0x60055440)

/*vc0728 uhost irq*/
#define EHCI_BIT_MASK		(1 <<0)
#define UHOSTEOC_UNSET_MASK 0x1c
#define UHOSTEOC_SRC_PND    0x10

void usb_hc_low_level_init(struct usbh_bus *bus)
{
	/*start clock*/
	writel(0x1e22, VC0728_USB_EHCI_CLK_BASE);
	writel(0x1e22, VC0728_USB_EHCI_CLK1_BASE);
	writel(0x0,    VC0728_USB_EHCI_PHYCKL_BASE);  

    writel(EHCI_BIT_MASK, VC0728_USB_HOST_BASE + UHOSTEOC_UNSET_MASK);//unmask irq
}

uint8_t usbh_get_port_speed(struct usbh_bus *bus, const uint8_t port)
{
    return USB_SPEED_HIGH;
}
  
void VC0728IrqHandler(int irq, void *param){
    writel(EHCI_BIT_MASK, VC0728_USB_HOST_BASE + UHOSTEOC_SRC_PND);
	USBH_IRQHandler(0);
}