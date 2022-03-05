/****************************************************************************//**
 * @file     startup_NUC472_442.S
 * @version  V1.00
 * @brief    CMSIS Cortex-M4 Core Device Startup File for NUC472_442
 *
 * SPDX-License-Identifier: Apache-2.0
 * @copyright (C) 2018 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/



	.syntax	unified
	.arch	armv7-m

	.section .stack
	.align	3
#ifdef __STACK_SIZE
	.equ	Stack_Size, __STACK_SIZE
#else
	.equ	Stack_Size, 0x00000800
#endif
	.globl	__StackTop
	.globl	__StackLimit
__StackLimit:
	.space	Stack_Size
	.size	__StackLimit, . - __StackLimit
__StackTop:
	.size	__StackTop, . - __StackTop

	.section .heap
	.align	3
#ifdef __HEAP_SIZE
	.equ	Heap_Size, __HEAP_SIZE
#else
	.equ	Heap_Size, 0x00000100
#endif
	.globl	__HeapBase
	.globl	__HeapLimit
__HeapBase:
	.if	Heap_Size
	.space	Heap_Size
	.endif
	.size	__HeapBase, . - __HeapBase
__HeapLimit:
	.size	__HeapLimit, . - __HeapLimit

	.section .vectors
	.align	2
	.globl	__Vectors
__Vectors:
	.long	__StackTop            /* Top of Stack */
	.long	Reset_Handler         /* Reset Handler */
	.long	NMI_Handler           /* NMI Handler */
	.long	HardFault_Handler     /* Hard Fault Handler */
	.long	MemManage_Handler     /* MPU Fault Handler */
	.long	BusFault_Handler      /* Bus Fault Handler */
	.long	UsageFault_Handler    /* Usage Fault Handler */
	.long	0                     /* Reserved */
	.long	0                     /* Reserved */
	.long	0                     /* Reserved */
	.long	0                     /* Reserved */
	.long	SVC_Handler           /* SVCall Handler */
	.long	DebugMon_Handler      /* Debug Monitor Handler */
	.long	0                     /* Reserved */
	.long	PendSV_Handler        /* PendSV Handler */
	.long	SysTick_Handler       /* SysTick Handler */

	/* External interrupts */
	.long	BOD_IRQHandler        /*  0: BOD                        */
	.long	IRC_IRQHandler        /*  1: IRC                        */
	.long	PWRWU_IRQHandler      /*  2: PWRWU                      */
	.long	RAMPE_IRQHandler      /*  3: RAMPE                      */
	.long	CKFAIL_IRQHandler     /*  4: CKFAIL                     */
	.long	0                     /*  5: Reserved                   */
	.long	RTC_IRQHandler        /*  6: RTC                        */
	.long	TAMPER_IRQHandler     /*  7: TAMPER                     */
	.long	EINT0_IRQHandler      /*  8: EINT0                      */
	.long	EINT1_IRQHandler      /*  9: EINT1                      */
	.long	EINT2_IRQHandler      /* 10: EINT2                      */
	.long	EINT3_IRQHandler      /* 11: EINT3                      */
	.long	EINT4_IRQHandler      /* 12: EINT4                      */
	.long	EINT5_IRQHandler      /* 13: EINT5                      */
	.long	EINT6_IRQHandler      /* 14: EINT6                      */
	.long	EINT7_IRQHandler      /* 15: EINT7                      */
	.long	GPA_IRQHandler        /* 16: GPA                        */
	.long	GPB_IRQHandler        /* 17: GPB                        */
	.long	GPC_IRQHandler        /* 18: GPC                        */
	.long	GPD_IRQHandler        /* 19: GPD                        */
	.long	GPE_IRQHandler        /* 20: GPE                        */
	.long	GPF_IRQHandler        /* 21: GPF                        */
	.long	GPG_IRQHandler        /* 22: GPG                        */
	.long	GPH_IRQHandler        /* 23: GPH                        */
	.long	GPI_IRQHandler        /* 24: GPI                        */
	.long	0                     /* 25: Reserved                   */
	.long	0                     /* 26: Reserved                   */
	.long	0                     /* 27: Reserved                   */
	.long	0                     /* 28: Reserved                   */
	.long	0                     /* 29: Reserved                   */
	.long	0                     /* 30: Reserved                   */
	.long	0                     /* 31: Reserved                   */
	.long	TMR0_IRQHandler       /* 32: TIMER0                     */
	.long	TMR1_IRQHandler       /* 33: TIMER1                     */
	.long	TMR2_IRQHandler       /* 34: TIMER2                     */
	.long	TMR3_IRQHandler       /* 35: TIMER3                     */
	.long	0                     /* 36: Reserved                   */
	.long	0                     /* 37: Reserved                   */
	.long	0                     /* 38: Reserved                   */
	.long	0                     /* 39: Reserved                   */
	.long	PDMA_IRQHandler       /* 40: PDMA                       */
	.long	0                     /* 41: Reserved                   */
	.long	ADC_IRQHandler        /* 42: ADC                        */
	.long	0                     /* 43: Reserved                   */
	.long	0                     /* 44: Reserved                   */
	.long	0                     /* 45: Reserved                   */
	.long	WDT_IRQHandler        /* 46: WDT                        */
	.long	WWDT_IRQHandler       /* 47: WWDT                       */
	.long	EADC0_IRQHandler      /* 48: EADC0                      */
	.long	EADC1_IRQHandler      /* 49: EADC1                      */
	.long	EADC2_IRQHandler      /* 50: EADC2                      */
	.long	EADC3_IRQHandler      /* 51: EADC3                      */
	.long	0                     /* 52: Reserved                   */
	.long	0                     /* 53: Reserved                   */
	.long	0                     /* 54: Reserved                   */
	.long	0                     /* 55: Reserved                   */
	.long	ACMP_IRQHandler       /* 56: ACMP                       */
	.long	0                     /* 57: Reserved                   */
	.long	0                     /* 58: Reserved                   */
	.long	0                     /* 59: Reserved                   */
	.long	OPA0_IRQHandler       /* 60: OPA0                       */
	.long	OPA1_IRQHandler       /* 61: OPA1                       */
	.long	ICAP0_IRQHandler      /* 62: ICAP0                      */
	.long	ICAP1_IRQHandler      /* 63: ICAP1                      */
	.long	PWM0CH0_IRQHandler    /* 64: PWM00                      */
	.long	PWM0CH1_IRQHandler    /* 65: PWM01                      */
	.long	PWM0CH2_IRQHandler    /* 66: PWM02                      */
	.long	PWM0CH3_IRQHandler    /* 67: PWM03                      */
	.long	PWM0CH4_IRQHandler    /* 68: PWM04                      */
	.long	PWM0CH5_IRQHandler    /* 69: PWM05                      */
	.long	PWM0_BRK_IRQHandler   /* 70: PWM0BRK                    */
	.long	QEI0_IRQHandler       /* 71: QEI0                       */
	.long	PWM1CH0_IRQHandler    /* 72: PWM10                      */
	.long	PWM1CH1_IRQHandler    /* 73: PWM11                      */
	.long	PWM1CH2_IRQHandler    /* 74: PWM12                      */
	.long	PWM1CH3_IRQHandler    /* 75: PWM13                      */
	.long	PWM1CH4_IRQHandler    /* 76: PWM14                      */
	.long	PWM1CH5_IRQHandler    /* 77: PWM15                      */
	.long	PWM1_BRK_IRQHandler   /* 78: PWM1BRK                    */
	.long	QEI1_IRQHandler       /* 79: QEI1                       */
	.long	EPWM0_IRQHandler      /* 80: EPWM0                      */
	.long	EPWM0BRK_IRQHandler   /* 81: EPWM0BRK                   */
	.long	EPWM1_IRQHandler      /* 82: EPWM1                      */
	.long	EPWM1BRK_IRQHandler   /* 83: EPWM1BRK                   */
	.long	0                     /* 84: Reserved                   */
	.long	0                     /* 85: Reserved                   */
	.long	0                     /* 86: Reserved                   */
	.long	0                     /* 87: Reserved                   */
	.long	USBD_IRQHandler       /* 88: USBD                       */
	.long	USBH_IRQHandler       /* 89: USBH                       */
	.long	USB_OTG_IRQHandler    /* 90: USBOTG                     */
	.long	0                     /* 91: Reserved                   */
	.long	EMAC_TX_IRQHandler    /* 92: EMAXTC                     */
	.long	EMAC_RX_IRQHandler    /* 93: EMACRX                     */
	.long	0                     /* 94: Reserved                   */
	.long	0                     /* 95: Reserved                   */
	.long	SPI0_IRQHandler       /* 96: SPI0                       */
	.long	SPI1_IRQHandler       /* 97: SPI1                       */
	.long	SPI2_IRQHandler       /* 98: SPI2                       */
	.long	SPI3_IRQHandler       /* 99: SPI3                       */
	.long	0                     /* 100: Reserved                  */
	.long	0                     /* 101: Reserved                  */
	.long	0                     /* 102: Reserved                  */
	.long	0                     /* 103: Reserved                  */
	.long	UART0_IRQHandler      /* 104: UART0                     */
	.long	UART1_IRQHandler      /* 105: UART1                     */
	.long	UART2_IRQHandler      /* 106: UART2                     */
	.long	UART3_IRQHandler      /* 107: UART3                     */
	.long	UART4_IRQHandler      /* 108: UART4                     */
	.long	UART5_IRQHandler      /* 109: UART5                     */
	.long	0                     /* 110: Reserved                  */
	.long	0                     /* 111: Reserved                  */
	.long	I2C0_IRQHandler       /* 112: I2C0                      */
	.long	I2C1_IRQHandler       /* 113: I2C1                      */
	.long	I2C2_IRQHandler       /* 114: I2C2                      */
	.long	I2C3_IRQHandler       /* 115: I2C3                      */
	.long	I2C4_IRQHandler       /* 116: I2C4                      */
	.long	0                     /* 117: Reserved                  */
	.long	0                     /* 118: Reserved                  */
	.long	0                     /* 119: Reserved                  */
	.long	SC0_IRQHandler        /* 120: SC0                       */
	.long	SC1_IRQHandler        /* 121: SC1                       */
	.long	SC2_IRQHandler        /* 122: SC2                       */
	.long	SC3_IRQHandler        /* 123: SC3                       */
	.long	SC4_IRQHandler        /* 124: SC4                       */
	.long	SC5_IRQHandler        /* 125: SC5                       */
	.long	0                     /* 126: Reserved                  */
	.long	0                     /* 127: Reserved                  */
	.long	CAN0_IRQHandler       /* 128: CAN0                      */
	.long	CAN1_IRQHandler       /* 129: CAN1                      */
	.long	0                     /* 130: Reserved                  */
	.long	0                     /* 131: Reserved                  */
	.long	I2S0_IRQHandler       /* 132: I2S0                      */
	.long	I2S1_IRQHandler       /* 133: I2S1                      */
	.long	0                     /* 134: Reserved                  */
	.long	0                     /* 135: Reserved                  */
	.long	SD_IRQHandler         /* 136: SD                        */
	.long	0                     /* 137: Reserved                  */
	.long	PS2D_IRQHandler       /* 138: PS2D                      */
	.long	CAP_IRQHandler        /* 139: CAP                       */
	.long	CRYPTO_IRQHandler     /* 140: CRYPTO                    */
	.long	CRC_IRQHandler        /* 141: CRC                       */
	.size	__Vectors, . - __Vectors

	.text
	.thumb
	.thumb_func
	.align	2
	.globl	Reset_Handler
	.type	Reset_Handler, %function
Reset_Handler:
/*  Firstly it copies data from read only memory to RAM. There are two schemes
 *  to copy. One can copy more than one sections. Another can only copy
 *  one section.  The former scheme needs more instructions and read-only
 *  data to implement than the latter.
 *  Macro __STARTUP_COPY_MULTIPLE is used to choose between two schemes.  */

#ifdef __STARTUP_COPY_MULTIPLE
/*  Multiple sections scheme.
 *
 *  Between symbol address __copy_table_start__ and __copy_table_end__,
 *  there are array of triplets, each of which specify:
 *    offset 0: LMA of start of a section to copy from
 *    offset 4: VMA of start of a section to copy to
 *    offset 8: size of the section to copy. Must be multiply of 4
 *
 *  All addresses must be aligned to 4 bytes boundary.
 */
	ldr	r4, =__copy_table_start__
	ldr	r5, =__copy_table_end__

.L_loop0:
	cmp	r4, r5
	bge	.L_loop0_done
	ldr	r1, [r4]
	ldr	r2, [r4, #4]
	ldr	r3, [r4, #8]

.L_loop0_0:
	subs	r3, #4
	ittt	ge
	ldrge	r0, [r1, r3]
	strge	r0, [r2, r3]
	bge	.L_loop0_0

	adds	r4, #12
	b	.L_loop0

.L_loop0_done:
#else
/*  Single section scheme.
 *
 *  The ranges of copy from/to are specified by following symbols
 *    __etext: LMA of start of the section to copy from. Usually end of text
 *    __data_start__: VMA of start of the section to copy to
 *    __data_end__: VMA of end of the section to copy to
 *
 *  All addresses must be aligned to 4 bytes boundary.
 */
	ldr	r1, =__etext
	ldr	r2, =__data_start__
	ldr	r3, =__data_end__

.L_loop1:
	cmp	r2, r3
	ittt	lt
	ldrlt	r0, [r1], #4
	strlt	r0, [r2], #4
	blt	.L_loop1
#endif /*__STARTUP_COPY_MULTIPLE */

/*  This part of work usually is done in C library startup code. Otherwise,
 *  define this macro to enable it in this startup.
 *
 *  There are two schemes too. One can clear multiple BSS sections. Another
 *  can only clear one section. The former is more size expensive than the
 *  latter.
 *
 *  Define macro __STARTUP_CLEAR_BSS_MULTIPLE to choose the former.
 *  Otherwise efine macro __STARTUP_CLEAR_BSS to choose the later.
 */
#ifdef __STARTUP_CLEAR_BSS_MULTIPLE
/*  Multiple sections scheme.
 *
 *  Between symbol address __copy_table_start__ and __copy_table_end__,
 *  there are array of tuples specifying:
 *    offset 0: Start of a BSS section
 *    offset 4: Size of this BSS section. Must be multiply of 4
 */
	ldr	r3, =__zero_table_start__
	ldr	r4, =__zero_table_end__

.L_loop2:
	cmp	r3, r4
	bge	.L_loop2_done
	ldr	r1, [r3]
	ldr	r2, [r3, #4]
	movs	r0, 0

.L_loop2_0:
	subs	r2, #4
	itt	ge
	strge	r0, [r1, r2]
	bge	.L_loop2_0

	adds	r3, #8
	b	.L_loop2
.L_loop2_done:
#elif defined (__STARTUP_CLEAR_BSS)
/*  Single BSS section scheme.
 *
 *  The BSS section is specified by following symbols
 *    __bss_start__: start of the BSS section.
 *    __bss_end__: end of the BSS section.
 *
 *  Both addresses must be aligned to 4 bytes boundary.
 */
	ldr	r1, =__bss_start__
	ldr	r2, =__bss_end__

	movs	r0, 0
.L_loop3:
	cmp	r1, r2
	itt	lt
	strlt	r0, [r1], #4
	blt	.L_loop3
#endif /* __STARTUP_CLEAR_BSS_MULTIPLE || __STARTUP_CLEAR_BSS */

/*  Unlock Register */
	ldr	r0, =0x40000100
	ldr	r1, =0x59
	str	r1, [r0]
	ldr	r1, =0x16
	str	r1, [r0]
	ldr	r1, =0x88
	str	r1, [r0]

#ifndef __NO_SYSTEM_INIT
	bl	SystemInit
#endif

/* Init POR */
	ldr	r0, =0x40000024
	ldr	r1, =0x00005AA5
	str	r1, [r0]

/* Disable branch buffer if VCID is 0 */
	ldr	r0, =0x40000020
	ldr	r1, [r0]
	cmp	r1, #0
	bne	Lock
	ldr	r0, =0x4000C018
	ldr	r1, [r0]
	ORR	r1, #0x80
	str	r1, [r0]
Lock:

/* Lock register */
	ldr	r0, =0x40000100
	ldr	r1, =0
	str	r1, [r0]

#ifndef __START
#define __START _start
#endif
	bl	__START

	.pool
	.size	Reset_Handler, . - Reset_Handler

	.align	1
	.thumb_func
	.weak	Default_Handler
	.type	Default_Handler, %function
Default_Handler:
	b	.
	.size	Default_Handler, . - Default_Handler

/*    Macro to define default handlers. Default handler
 *    will be weak symbol and just dead loops. They can be
 *    overwritten by other handlers */
	.macro	def_irq_handler	handler_name
	.weak	\handler_name
	.set	\handler_name, Default_Handler
	.endm

	def_irq_handler	NMI_Handler
	def_irq_handler	HardFault_Handler
	def_irq_handler	MemManage_Handler
	def_irq_handler	BusFault_Handler
	def_irq_handler	UsageFault_Handler
	def_irq_handler	SVC_Handler
	def_irq_handler	DebugMon_Handler
	def_irq_handler	PendSV_Handler
	def_irq_handler	SysTick_Handler

	def_irq_handler	BOD_IRQHandler
	def_irq_handler	IRC_IRQHandler
	def_irq_handler	PWRWU_IRQHandler
	def_irq_handler	RAMPE_IRQHandler
	def_irq_handler	CKFAIL_IRQHandler
	def_irq_handler	RTC_IRQHandler
	def_irq_handler	TAMPER_IRQHandler
	def_irq_handler	EINT0_IRQHandler
	def_irq_handler	EINT1_IRQHandler
	def_irq_handler	EINT2_IRQHandler
	def_irq_handler	EINT3_IRQHandler
	def_irq_handler	EINT4_IRQHandler
	def_irq_handler	EINT5_IRQHandler
	def_irq_handler	EINT6_IRQHandler
	def_irq_handler	EINT7_IRQHandler
	def_irq_handler	GPA_IRQHandler
	def_irq_handler	GPB_IRQHandler
	def_irq_handler	GPC_IRQHandler
	def_irq_handler	GPD_IRQHandler
	def_irq_handler	GPE_IRQHandler
	def_irq_handler	GPF_IRQHandler
	def_irq_handler	GPG_IRQHandler
	def_irq_handler	GPH_IRQHandler
	def_irq_handler	GPI_IRQHandler
	def_irq_handler	TMR0_IRQHandler
	def_irq_handler	TMR1_IRQHandler
	def_irq_handler	TMR2_IRQHandler
	def_irq_handler	TMR3_IRQHandler
	def_irq_handler	PDMA_IRQHandler
	def_irq_handler	ADC_IRQHandler
	def_irq_handler	WDT_IRQHandler
	def_irq_handler	WWDT_IRQHandler
	def_irq_handler	EADC0_IRQHandler
	def_irq_handler	EADC1_IRQHandler
	def_irq_handler	EADC2_IRQHandler
	def_irq_handler	EADC3_IRQHandler
	def_irq_handler	ACMP_IRQHandler
	def_irq_handler	OPA0_IRQHandler
	def_irq_handler	OPA1_IRQHandler
	def_irq_handler	ICAP0_IRQHandler
	def_irq_handler	ICAP1_IRQHandler
	def_irq_handler	PWM0CH0_IRQHandler
	def_irq_handler	PWM0CH1_IRQHandler
	def_irq_handler	PWM0CH2_IRQHandler
	def_irq_handler	PWM0CH3_IRQHandler
	def_irq_handler	PWM0CH4_IRQHandler
	def_irq_handler	PWM0CH5_IRQHandler
	def_irq_handler	PWM0_BRK_IRQHandler
	def_irq_handler	QEI0_IRQHandler
	def_irq_handler	PWM1CH0_IRQHandler
	def_irq_handler	PWM1CH1_IRQHandler
	def_irq_handler	PWM1CH2_IRQHandler
	def_irq_handler	PWM1CH3_IRQHandler
	def_irq_handler	PWM1CH4_IRQHandler
	def_irq_handler	PWM1CH5_IRQHandler
	def_irq_handler	PWM1_BRK_IRQHandler
	def_irq_handler	QEI1_IRQHandler
	def_irq_handler	EPWM0_IRQHandler
	def_irq_handler	EPWM0BRK_IRQHandler
	def_irq_handler	EPWM1_IRQHandler
	def_irq_handler	EPWM1BRK_IRQHandler
	def_irq_handler	USBD_IRQHandler
	def_irq_handler	USBH_IRQHandler
	def_irq_handler	USB_OTG_IRQHandler
	def_irq_handler	EMAC_TX_IRQHandler
	def_irq_handler	EMAC_RX_IRQHandler
	def_irq_handler	SPI0_IRQHandler
	def_irq_handler	SPI1_IRQHandler
	def_irq_handler	SPI2_IRQHandler
	def_irq_handler	SPI3_IRQHandler
	def_irq_handler	UART0_IRQHandler
	def_irq_handler	UART1_IRQHandler
	def_irq_handler	UART2_IRQHandler
	def_irq_handler	UART3_IRQHandler
	def_irq_handler	UART4_IRQHandler
	def_irq_handler	UART5_IRQHandler
	def_irq_handler	I2C0_IRQHandler
	def_irq_handler	I2C1_IRQHandler
	def_irq_handler	I2C2_IRQHandler
	def_irq_handler	I2C3_IRQHandler
	def_irq_handler	I2C4_IRQHandler
	def_irq_handler	SC0_IRQHandler
	def_irq_handler	SC1_IRQHandler
	def_irq_handler	SC2_IRQHandler
	def_irq_handler	SC3_IRQHandler
	def_irq_handler	SC4_IRQHandler
	def_irq_handler	SC5_IRQHandler
	def_irq_handler	CAN0_IRQHandler
	def_irq_handler	CAN1_IRQHandler
	def_irq_handler	I2S0_IRQHandler
	def_irq_handler	I2S1_IRQHandler
	def_irq_handler	SD_IRQHandler
	def_irq_handler	PS2D_IRQHandler
	def_irq_handler	CAP_IRQHandler
	def_irq_handler	CRYPTO_IRQHandler
	def_irq_handler	CRC_IRQHandler

	.end
