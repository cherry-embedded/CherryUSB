/**
******************************************************************************
* @file    uart.h
* @author  AE Team
* @version V1.3.9
* @date    28/08/2019
* @brief   This file contains all the functions prototypes for the uart_usb firmware
*          library.
******************************************************************************
* @copy
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MindMotion SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2019 MindMotion</center></h2>
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __UART_H
#define __UART_H
#include "sys.h"
#include "stdio.h"
//////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////

#define UART_REC_LEN  			200  	//定义最大接收字节数 200
#define EN_UART1_RX 			1		//使能（1）/禁止（0）串口1接收


/** @defgroup UART_Interrupt_definition
  * @{
  */


#define UART_IT_ERR            ((uint16_t)0x0020)
#define UART_IT_PE             ((uint16_t)0x0010)
#define UART_OVER_ERR          ((uint16_t)0x0008)
#define UART_TIMEOUT_ERR       ((uint16_t)0x0004)
#define UART_IT_RXIEN          ((uint16_t)0x0002)
#define UART_IT_TXIEN          ((uint16_t)0x0001)




/** @defgroup UART_Flags
  * @{
  */


#define UART_FLAG_TXEMPTY                   ((uint16_t)0x0008)
#define UART_FLAG_TXFULL                    ((uint16_t)0x0004)
#define UART_FLAG_RXAVL                     ((uint16_t)0x0002)
#define UART_FLAG_TXEPT                     ((uint16_t)0x0001)


extern u8  UART_RX_BUF[UART_REC_LEN]; //接收缓冲,最大UART_REC_LEN个字节.末字节为换行符
extern u16 UART_RX_STA;         		//接收状态标记
//如果想串口中断接收，请不要注释以下宏定义
void uart_initwBaudRate(u32 pclk2, u32 bound);
void uart_sendArray(unsigned char *dataBuf, unsigned int len);

#endif


/**
* @}
*/

/**
* @}
*/

/**
* @}
*/

/*-------------------------(C) COPYRIGHT 2019 MindMotion ----------------------*/














