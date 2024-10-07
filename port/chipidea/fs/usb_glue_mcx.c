/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "fsl_common.h"
#include "usb_chipidea_fs_reg.h"

#define USB_OTG_DEV ((CHIPIDEA_FS_MCX_TypeDef *)g_usbdev_bus[busid].reg_base)

void USB0_FS_IRQHandler(void)
{
    extern void USBD_IRQHandler(uint8_t busid);
    USBD_IRQHandler(0);
}

void USB_ClockInit(void)
{
    CLOCK_AttachClk(kCLK_48M_to_USB0);
    CLOCK_EnableClock(kCLOCK_Usb0Ram);
    CLOCK_EnableClock(kCLOCK_Usb0Fs);
    CLOCK_EnableUsbfsClock();
}

void usb_dc_low_level_init(uint8_t busid)
{
    USB_ClockInit();
    /* Install isr, set priority, and enable IRQ. */
    NVIC_SetPriority((IRQn_Type)USB0_FS_IRQn, 3);
    EnableIRQ((IRQn_Type)USB0_FS_IRQn);

    USB_OTG_DEV->USBTRC0 |= USB_USBTRC0_USBRESET_MASK;
    while (USB_OTG_DEV->USBTRC0 & USB_USBTRC0_USBRESET_MASK)
        ;

    USB_OTG_DEV->USBTRC0 |= USB_USBTRC0_VREGIN_STS(1); /* software must set this bit to 1 */
    USB_OTG_DEV->USBCTRL = 0;
    USB_OTG_DEV->CONTROL |= USB_CONTROL_DPPULLUPNONOTG_MASK;
}

void usb_dc_low_level_deinit(uint8_t busid)
{
    USB_OTG_DEV->CONTROL &= ~USB_CONTROL_DPPULLUPNONOTG_MASK;
    DisableIRQ((IRQn_Type)USB0_FS_IRQn);
}

void usbd_chipidea_fs_delay_ms(uint8_t ms)
{

}