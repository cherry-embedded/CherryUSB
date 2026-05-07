/*
* Copyright (c) 2020 - 2024 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef R_USB_DEVICE_CFG_H_
#define R_USB_DEVICE_CFG_H_
#ifdef __cplusplus
extern "C" {
#endif

#define USBHS_PHY_CLOCK_SOURCE_IS_XTAL    1

#define USBD_CFG_PARAM_CHECKING_ENABLE    (BSP_CFG_PARAM_CHECKING_ENABLE)
#define USBD_CFG_BUS_WAIT_TIME            (0x000FU) /* 17 access cycles */

#if USBHS_PHY_CLOCK_SOURCE_IS_XTAL
#if BSP_CFG_XTAL_HZ == 12000000
#define USBD_CFG_PHYSET_CLKSEL            (0U)
#elif BSP_CFG_XTAL_HZ == 48000000
#define USBD_CFG_PHYSET_CLKSEL            (0x1U)
#elif BSP_CFG_XTAL_HZ == 20000000
#define USBD_CFG_PHYSET_CLKSEL            (0x2U)
#elif BSP_CFG_XTAL_HZ == 24000000
#define USBD_CFG_PHYSET_CLKSEL            (0x3U)
#else
#error "Available XTAL clocks are 12-20-24-48Mhz"
#endif
#else   /* Use 48Mhz UCK as clock source for PHY clock */
#define USBD_CFG_PHYSET_CLKSEL            (-1)
#endif

/* Enable auto status for control write/read endpoint */
#define USBD_CFG_CONTROL_EP_AUTO_STATUS_ENABLE (1U)

#ifdef __cplusplus
}
#endif
#endif                                 /* R_USB_DEVICE_CFG_H_ */
