/**
  *********************************************************************************
  *
  * @file    ald_conf.h
  * @brief   Enable/Disable the peripheral module.
  *
  * @version V1.0
  * @date    26 Jun 2019
  * @author  AE Team
  * @note
  *          Change Logs:
  *          Date            Author          Notes
  *          26 Jun 2019     AE Team         The first version
  *
  * Copyright (C) Shanghai Eastsoft Microelectronics Co. Ltd. All rights reserved.
  *
  * SPDX-License-Identifier: Apache-2.0
  *
  * Licensed under the Apache License, Version 2.0 (the License); you may
  * not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an AS IS BASIS, WITHOUT
  * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  **********************************************************************************
  */


#ifndef __ALD_CONF_H__
#define __ALD_CONF_H__


#define ALD_DMA
#define ALD_GPIO
#define ALD_UART
#define ALD_I2C
#define ALD_CMU
#define ALD_RMU
#define ALD_PMU
#define ALD_WDT
#define ALD_LCD
#define ALD_RTC
#define ALD_CAN
#define ALD_FLASH
#define ALD_ADC
#define ALD_CRC
#define ALD_CRYPT
#define ALD_TIMER
#define ALD_LPTIM
#define ALD_PIS
#define ALD_SPI
#define ALD_CALC
#define ALD_ACMP
#define ALD_OPAMP
#define ALD_TRNG
#define ALD_TSENSE
#define ALD_BKPC
#define ALD_DAC
#define ALD_IAP
#define ALD_I2S
#define ALD_ECC
#define ALD_NAND
#define ALD_QSPI
#define ALD_NOR
#define ALD_SRAM
#define ALD_USB
#define ALD_NOR_LCD
#define ALD_SYSCFG
#define ALD_RTCHW
#if defined(ALD_NAND) || defined(ALD_NOR) || defined(ALD_SRAM)
#define ALD_EBI
#endif

#ifdef ALD_GPIO
	#include "ald_gpio.h"
#endif	/*ALD_GPIO */

#ifdef ALD_DMA
	#include "ald_dma.h"
#endif	/* ALD_DMA */

#ifdef ALD_UART
	#include "ald_uart.h"
#endif	/* ALD_UART */

#ifdef ALD_I2C
	#include "ald_i2c.h"
#endif	/* ALD_I2C */

#ifdef ALD_CMU
	#include "ald_cmu.h"
#endif	/* ALD_CMU */

#ifdef ALD_RMU
	#include "ald_rmu.h"
#endif	/* ALD_RMU */

#ifdef ALD_PMU
	#include "ald_pmu.h"
#endif	/* ALD_PUM */

#ifdef ALD_WDT
	#include "ald_wdt.h"
#endif	/* ALD_WDT */

#ifdef ALD_RTC
	#include "ald_rtc.h"
#endif	/* ALD_RTC */

#ifdef ALD_FLASH
	#include "ald_flash.h"
#endif	/*ALD_FLASH*/

#ifdef ALD_ADC
	#include "ald_adc.h"
#endif	/* ALD_ADC */

#ifdef ALD_CRC
	#include "ald_crc.h"
#endif	/* ALD_CRC */

#ifdef ALD_CRYPT
	#include "ald_crypt.h"
#endif	/* ALD_CRYPT */

#ifdef ALD_TIMER
	#include "ald_timer.h"
#endif	/* ALD_TIMER */

#ifdef ALD_PIS
	#include "ald_pis.h"
#endif	/* ALD_PIS */

#ifdef ALD_SPI
	#include "ald_spi.h"
#endif	/* ALD_SPI */

#ifdef ALD_CALC
	#include "ald_calc.h"
#endif	/* ALD_CALC */

#ifdef ALD_ACMP
	#include "ald_acmp.h"
#endif	/* ALD_ACMP */

#ifdef ALD_TRNG
	#include "ald_trng.h"
#endif	/* ALD_TRNG */

#ifdef ALD_TSENSE
	#include "ald_tsense.h"
#endif	/* ALD_TSENSE */

#ifdef ALD_BKPC
	#include "ald_bkpc.h"
#endif	/* ALD_BKPC */

#ifdef ALD_DAC
	#include "ald_dac.h"
#endif	/* ALD_DAC */

#ifdef ALD_IAP
	#include "ald_iap.h"
#endif	/* ALD_IAP */

#ifdef ALD_CAN
	#include "ald_can.h"
#endif	/* ALD_CAN */

#ifdef ALD_QSPI
	#include "ald_qspi.h"
#endif	/* ALD_QSPI */

#ifdef ALD_USB
	#include "ald_usb.h"
#endif	/* ALD_USB */

#ifdef ALD_SRAM
	#include "ald_sram.h"
#endif	/* ALD_SRAM */

#ifdef ALD_EBI
	#include "ald_ebi.h"
#endif	/* ALD_EBI */

#ifdef ALD_NAND 
	#include "ald_nand.h"
#endif	/* ALD_NAND */

#ifdef ALD_NOR_LCD
	#include "ald_nor_lcd.h"
#endif	/* ALD_NOR_LCD */	

#ifdef ALD_I2S
	#include "ald_i2s.h"
#endif	/* ALD_I2S */

#ifdef ALD_SYSCFG
	#include "ald_syscfg.h"
#endif	/* ALD_SYSCFG */

#ifdef ALD_RTCHW
	#include "ald_rtchw.h"
#endif	/* ALD_RTCHW */

#define TICK_INT_PRIORITY	3

#endif
