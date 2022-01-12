

#ifndef __CH58x_COMM_H__
#define __CH58x_COMM_H__

#ifdef __cplusplus
 extern "C" {
#endif


#ifndef  NULL
#define  NULL     0
#endif
#define  ALL			0xFFFF

#ifndef __HIGH_CODE
#define __HIGH_CODE   __attribute__((section(".highcode")))
#endif

#ifndef __INTERRUPT
#ifdef INT_SOFT
#define __INTERRUPT   __attribute__((interrupt()))
#else
#define __INTERRUPT   __attribute__((interrupt("WCH-Interrupt-fast")))
#endif
#endif

#define Debug_UART0        0
#define Debug_UART1        1
#define Debug_UART2        2
#define Debug_UART3        3
 
#ifndef DEBUG
#define DEBUG Debug_UART0
#endif
#ifdef DEBUG
#include <stdio.h>
#endif
   
#ifndef	 FREQ_SYS  
#define  FREQ_SYS		60000000
#endif   

#ifndef  SAFEOPERATE
#define  SAFEOPERATE   __nop();__nop()
#endif

#if ( CLK_OSC32K == 1 )
#define CAB_LSIFQ     	32000
#else
#define CAB_LSIFQ     	32768
#endif

#include <string.h>
#include <stdint.h>
#include "CH583SFR.h"
#include "core_riscv.h"
#include "CH58x_clk.h"
#include "CH58x_uart.h"
#include "CH58x_gpio.h"
#include "CH58x_i2c.h"
#include "CH58x_flash.h"
#include "CH58x_pwr.h"
#include "CH58x_pwm.h"
#include "CH58x_adc.h"
#include "CH58x_sys.h"
#include "CH58x_timer.h"
#include "CH58x_spi.h"
#include "CH58x_usbdev.h"
#include "CH58x_usbhost.h"
#include "ISP583.h"

  
#define DelayMs(x)      mDelaymS(x)	  
#define DelayUs(x)      mDelayuS(x)	  


#ifdef __cplusplus
}
#endif

#endif  // __CH58x_COMM_H__

