/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "board.h"
#include "bsp_delay.h"
#include "usb_dwc2_reg.h"
#include "usbd_core.h"
 /* you can find this config in function: usb_global_init, file:at32fxxx_usb.c, for example:
  *
  *  usbx->GGCCFG_B.PWEN = BIT_RESET;
  *  usbdh->usbGlobal->GGCCFG_B.ADVBSEN = BIT_RESET;
  *  usbdh->usbGlobal->GGCCFG_B.BDVBSEN = BIT_RESET;
  *
 */
uint32_t usbd_get_dwc2_gccfg_conf(uint32_t reg_base)
{
#if defined(APM32F40X)||defined(APM32F41X)
	return ((1 << 16) | (1 << 21));
#endif
}

uint32_t usbh_get_dwc2_gccfg_conf(uint32_t reg_base)
{
#if defined(APM32F40X)||defined(APM32F41X)
	return ((1 << 16) | (1 << 21));
#endif
}

void usbd_dwc2_delay_ms(uint8_t ms)
{
	APM_DelayUs(ms);
}

/**
* @brief  Initializes the low level portion of the device.
* @param  busid: Bus identifier
*
* @retval None
* @note   This function should be called before the driver is used.
*	//#INFO The design structure of USB is as follows: <image url="http://mcu.eetrend.com/files/2022-01/wen_zhang_/100556839-238940-1.jpg" scale="0.4" />
*/

void usb_dc_low_level_init(uint8_t busid)
{
	GPIO_Config_T gpioConfig;

	struct usbd_bus* bus;
	bus = &g_usbdev_bus[busid];

	APM_DelayInit();

	if (bus->reg_base == USB_OTG_FS_BASE)
	{
		RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SYSCFG);
		/* Configure USB OTG */
		RCM_EnableAHB2PeriphClock(RCM_AHB2_PERIPH_OTG_FS);

		/* Configure USB OTG GPIO */
		RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOA);

		GPIO_ConfigPinAF(GPIOA, GPIO_PIN_SOURCE_11, GPIO_AF_OTG1_FS);
		GPIO_ConfigPinAF(GPIOA, GPIO_PIN_SOURCE_12, GPIO_AF_OTG1_FS);

		/* USB DM, DP pin configuration */
		gpioConfig.mode = GPIO_MODE_AF;
		gpioConfig.speed = GPIO_SPEED_100MHz;
		gpioConfig.otype = GPIO_OTYPE_PP;
		gpioConfig.pupd = GPIO_PUPD_NOPULL;
		gpioConfig.pin = GPIO_PIN_11 | GPIO_PIN_12;
		GPIO_Config(GPIOA, &gpioConfig);

		/* NVIC */
		NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_4);
		NVIC_EnableIRQRequest(OTG_FS_IRQn, 1, 0);
	}
	else if (bus->reg_base == USB_OTG_HS_BASE)
	{
		/* Configure USB OTG*/
		RCM_EnableAPB2PeriphClock(RCM_APB2_PERIPH_SYSCFG);

		RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_OTG_HS);

		/* Configure USB OTG GPIO */
		RCM_EnableAHB1PeriphClock(RCM_AHB1_PERIPH_GPIOB);

		GPIO_ConfigPinAF(GPIOB, GPIO_PIN_SOURCE_14, GPIO_AF_OTG_HS_FS);
		GPIO_ConfigPinAF(GPIOB, GPIO_PIN_SOURCE_15, GPIO_AF_OTG_HS_FS);

		/* USB DM, DP pin configuration */
		gpioConfig.mode = GPIO_MODE_AF;
		gpioConfig.speed = GPIO_SPEED_100MHz;
		gpioConfig.otype = GPIO_OTYPE_PP;
		gpioConfig.pupd = GPIO_PUPD_NOPULL;
		gpioConfig.pin = GPIO_PIN_14 | GPIO_PIN_15;
		GPIO_Config(GPIOB, &gpioConfig);

		/* NVIC */
		NVIC_ConfigPriorityGroup(NVIC_PRIORITY_GROUP_4);
		NVIC_EnableIRQRequest(OTG_HS1_IRQn, 1, 0);
	}
}

/**@brief  Deinitializes the low level portion of the device
 *
 * @param busid
 */
void usb_dc_low_level_deinit(uint8_t busid)
{
	///GPIO_Config_T gpioConfig;

}

/**
 * @brief  Get USB device speed
 * @param  busid: Bus identifier
 * @param  fsphy_type: Full-speed PHY type
 * @param  hsphy_type: High-speed PHY type
 *
 * @retval USB device speed
 *
 * @note   Type of USB device speed:
 *         Full-Speed PHY Interface Type (FSPhyType)
 *		`2'b00`: Full-speed interface not supported
 *		`2'b01`: Dedicated full-speed interface
 *		`2'b10`: FS pins shared with UTMI+ pins
 *		`2'b11`: FS pins shared with ULPI pins
 *
 *		High-Speed PHY Interface Type (HSPhyType)
 *		`2'b00`: High-Speed interface not supported
 *		`2'b01`: UTMI+
 *		`2'b10`: ULPI
 *		`2'b11`: UTMI+ and ULPI
 *
 */
uint8_t usbd_get_speed_config(uint8_t busid, uint8_t fsphy_type, uint8_t hsphy_type)
{
	uint8_t ret;

	struct usbd_bus* bus;
	bus = &g_usbdev_bus[busid];

	if (bus->reg_base == USB_OTG_FS_BASE)
	{
		ret = USB_OTG_SPEED_FULL;
	}
	else if (bus->reg_base == USB_OTG_HS_BASE)
	{
		if (hsphy_type == 0)
		{
			ret = USB_OTG_SPEED_FULL;
		}
		else
		{
#if defined(CONFIG_USB_HS)
			ret = USB_OTG_SPEED_HIGH;
#else
			ret = USB_OTG_SPEED_HIGH_IN_FULL;
#endif
		}
	}
	return ret;
}

typedef enum
{
	OTG_HS1_EXT,
	OTG_HS2_EMB,
} USB_OTG_HS_PHY_T;

#define HS_USB_CONFIG OTG_HS2_EMB

/**@brief  Select the USB PHY to be used.
 * @details
 *
 * @param busid
 */

void usb_phy_config(uint8_t busid)
{
	struct usbd_bus* bus;
	bus = &g_usbdev_bus[busid];

	if (bus->reg_base == USB_OTG_HS_BASE) // Port.B
	{
		//!? PB  HS2->UTMI(internal)， HS1->ULPI(external)
		/// HS2 and HS1 share the same registers, `but when using hs2, need to turn on imdependent registers: usb_switch, poweron_core, otg_suspendm, sw_rref_i2c`
		//# HS1与HS2共用基地址，且共用寄存器，但是启用HS2需要另外配置四个寄存器
		if (HS_USB_CONFIG == OTG_HS2_EMB)
		{
			/* Init The UTMI Interface */
			USB_OTG_HS2->USB_SWITCH_B.usb_switch = BIT_SET;
			USB_OTG_HS2->POWERON_CORE_B.poweron_core = BIT_SET;
			USB_OTG_HS2->OTG_SUSPENDM_B.otg_suspendm = BIT_SET;
			USB_OTG_HS2->SW_RREF_I2C_B.sw_rref_i2c = 0x05;
		}
		/// if using ULPI(HS1), need to configure the ULPI interface
		else if (HS_USB_CONFIG == OTG_HS1_EXT)// hs1 only config with ULPI
		{
			/* Init The ULPI Interface */
			((DWC2_GlobalTypeDef*)(g_usbdev_bus[busid].reg_base))->GUSBCFG &= ~(USB_OTG_GUSBCFG_TSDPS | USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_PHYSEL);
			/* Select vbus source */
			((DWC2_GlobalTypeDef*)(g_usbdev_bus[busid].reg_base))->GUSBCFG &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);
		}
	}
	else // Port.A
	{
		/* Select FS Embedded PHY */
		((DWC2_GlobalTypeDef*)(g_usbdev_bus[busid].reg_base))->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL; // 1<<6     0:hs 1:fs
	}
}
