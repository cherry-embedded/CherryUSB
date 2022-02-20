/**
  *********************************************************************************
  *
  * @file    irq.c
  * @brief   Interrupt handler
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

#include "main.h"
#include "utils.h"
#include "ald_cmu.h"
#ifdef ALD_DMA
#include "ald_dma.h"
#endif


/** @addtogroup Projects_Examples_ALD
  * @{
  */

/** @addtogroup Examples
  * @{
  */

/**
  * @brief  NMI IRQ handler
  * @retval None
  */
void NMI_Handler(void)
{
	/* Added Emergency operation */
	return;
}

/**
  * @brief  Hardfault IRQ handler
  * @retval None
  */
void HardFault_Handler(void)
{
	/* Added debug information */
	while (1)
		;
}

/**
  * @brief  MemManage IRQ handler
  * @retval None
  */
void MemManage_Handler(void)
{
	/* Added debug information */
	while (1)
		;
}

/**
  * @brief  BusFault IRQ handler
  * @retval None
  */
void BusFault_Handler(void)
{
	/* Added debug information */
	while (1)
		;
}

/**
  * @brief  UsageFault IRQ handler
  * @retval None
  */
void UsageFault_Handler(void)
{
	/* Added debug information */
	while (1)
		;
}

/**
  * @brief  Supervisor Call IRQ handler
  * @retval None
  */
void SVC_Handler(void)
{
	/* Added system callback */
	return;
}

/**
  * @brief  Debug Monitor IRQ handler
  * @retval None
  */
void DebugMon_Handler(void)
{
	/* Added debug operation */
	return;
}

/**
  * @brief  PendSV IRQ handler
  * @retval None
  */
void PendSV_Handler(void)
{
	/* Added thread switching operation */
	return;
}

/**
  * @brief  SysTick IRQ handler
  * @retval None
  */
void SysTick_Handler(void)
{
	ald_inc_tick();
	return;
}

#ifdef ALD_DMA
/**
  * @brief  DMA IRQ#66 handler
  * @retval None
  */
void DMA_Handler(void)
{
	ald_dma_irq_handler();
}
#endif


/**
  * @brief  USB IRQ#70 handler
  * @retval None
  */
//void USB_INT_Handler()
//{
//	usb0_device_int_handler();
//}

/**
  * @brief  USB_DMA IRQ#71 handler
  * @retval None
  */
//void USB_DMA_Handler()
//{
//	usb0_dma_int_handler();
//}
/**
  * @}
  */
/**
  * @}
  */
