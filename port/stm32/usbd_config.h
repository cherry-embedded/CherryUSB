#ifndef __USBD_CONFIG_H__
#define __USBD_CONFIG_H__

#if defined(STM32F0)
#include "stm32f0xx.h"
#elif defined(STM32F1)
#include "stm32f1xx.h"
#elif STM32F3
#include "stm32f3xx.h"
#elif defined(STM32F4)
#include "stm32f4xx.h"
#elif defined(STM32H7)
#include "stm32h7xx.h"
#endif

/*other do not process，users need to modify byself*/
#if defined(STM32F0)
#define USBD_IRQHandler USB_IRQHandler
#elif defined(STM32F1) || defined(STM32F3)
#define USBD_IRQHandler USB_LP_CAN1_RX0_IRQHandler
#else
#ifdef CONFIG_USB_HS
#define USBD_IRQHandler OTG_HS_IRQHandler
#else
#define USBD_IRQHandler OTG_FS_IRQHandler
#endif
#endif

#endif