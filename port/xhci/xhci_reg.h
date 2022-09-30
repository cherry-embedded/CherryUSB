/*
 * Copyright : (C) 2022 Phytium Information Technology, Inc. 
 * All Rights Reserved.
 *  
 * This program is OPEN SOURCE software: you can redistribute it and/or modify it  
 * under the terms of the Phytium Public License as published by the Phytium Technology Co.,Ltd,  
 * either version 1.0 of the License, or (at your option) any later version. 
 *  
 * This program is distributed in the hope that it will be useful,but WITHOUT ANY WARRANTY;  
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the Phytium Public License for more details. 
 *  
 * 
 * FilePath: xhci_reg.h
 * Date: 2022-07-19 09:26:25
 * LastEditTime: 2022-07-19 09:26:25
 * Description:  This files is for xhci register definition
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 */
#ifndef  XHCI_REG_H
#define  XHCI_REG_H

/***************************** Include Files *********************************/
#include "usbh_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/************************** Constant Definitions *****************************/
#if defined(__aarch64__)
#define BITS_PER_LONG 64U
#define XHCI_AARCH64
#else
#define BITS_PER_LONG 32U
#define XHCI_AARCH32
#endif

#define XHCI_GENMASK(h, l) \
	(((~0UL) - (1UL << (l)) + 1) & (~0UL >> (BITS_PER_LONG - 1 - (h))))

#define XHCI_GENMASK_ULL(h, l)            \
	(((~0ULL) - (1ULL << (l)) + 1) & \
	 (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#define XHCI32_GET_BITS(x, a, b)                  (uint32_t)((((uint32_t)(x)) & XHCI_GENMASK(a, b)) >> b)
#define XHCI32_SET_BITS(x, a, b)                  (uint32_t)((((uint32_t)(x)) << b) & XHCI_GENMASK(a, b))
#define XHCI64_GET_BITS(x, a, b)                  (uint64_t)((((uint64_t)(x)) & XHCI_GENMASK_ULL(a, b)) >> b)
#define XHCI64_SET_BITS(x, a, b)                  (uint64_t)((((uint64_t)(x)) << b) & XHCI_GENMASK_ULL(a, b))

/** @name Register Map
 *
 * Register offsets from the base address of an XHCI device.
 * @{
 */
#define XHCI_REG_CAP_CAPLENGTH		    0x00    /* specify the limits, restrictions and capabilities */
#define XHCI_REG_CAP_HCIVERSION		    0x02    /* Interface Version Number */
#define XHCI_REG_CAP_HCS1		        0x04	/* Host Controller Structural Parameters 1 */
#define XHCI_REG_CAP_HCS2		        0x08	/* Host Controller Structural Parameters 2 */
#define XHCI_REG_CAP_HCS3		        0x0C	/* Host Controller Structural Parameters 3 */
#define XHCI_REG_CAP_HCC		        0x10	/* Capability Parameters 1 */
#define XHCI_REG_CAP_DBOFF		        0x14	/* Doorbell Offset Register */
#define XHCI_REG_CAP_RTSOFF		        0x18	/* Runtime Register Space Offset Register */

/***************** Host Controller Operational Registers ***********************/
#define XHCI_REG_OP_USBCMD		        0x00	/* USB Command Register */
#define XHCI_REG_OP_USBSTS		        0x04	/* USB Status Register */
#define XHCI_REG_OP_PAGESIZE	        0x08	/* Page Size Register */
#define XHCI_REG_OP_DNCTRL		        0x14	/* Device Notification Control Register */
#define XHCI_REG_OP_CRCR		        0x18	/* Command Ring Control Register */
#define XHCI_REG_OP_DCBAAP		        0x30	/* Device Context Base Address Array Pointer Register */
#define XHCI_REG_OP_CONFIG		        0x38	/* Configure Register */

/* Port Status and Ctrl Register : OP Base + (400h + (10h * (n–1))) 'n' is port num */
#define XHCI_REG_OP_PORTS_BASE		    0x400	/* Port Status and Control Register Base */
#define XHCI_REG_OP_PORTS_SIZE          0x10    /* Size of one Port SC Register */
#define XHCI_REG_OP_PORTS_OFF(port, off)   ((port) * XHCI_REG_OP_PORTS_SIZE + offset)

#define XHCI_REG_OP_PORTS_PORTSC	    0x00	/* Port Status and Control Register */
#define XHCI_REG_OP_PORTS_PORTPMSC	    0x04	/* USB3 Port Power Management Status and Control Register */
#define XHCI_REG_OP_PORTS_PORTLI	    0x08	/* Port Link Info Register */

/***************** Host Controller Runtime Registers ***********************/
#define XHCI_REG_RT_MFINDEX		        0x00	/* Microframe Index */
#define XHCI_REG_RT_IR0			        0x20	/* Interrupter Register Set 0 */
#define XHCI_REG_RT_IR1023		        0x8000	/* Interrupter Register Set 1023 */

/* Interrupter Register Set : RT Base + 020h + (32 * Interrupter) */
#define XHCI_REG_RT_IR_IMAN		        0x00	/* Interrupter Management Register */
#define XHCI_REG_RT_IR_IMOD		        0x04	/* Interrupter Moderation Register */
#define XHCI_REG_RT_IR_ERSTSZ		    0x08	/* Event Ring Segment Table Size Register */
#define XHCI_REG_RT_IR_ERSTBA   	    0x10	/* Event Ring Segment Table Base Address Register */
#define XHCI_REG_RT_IR_ERDP		        0x18	/* Event Ring Dequeue Pointer Register */
#define XHCI_REG_RT_IR_SIZE             0x20    /* Size of one IR Register */

/***************** Doorbell Register ***********************/
#define XHCI_REG_DB_SIZE                4       /* Doorbell registers are 32 bits in length */

/***************** eXtensible Host Controller Capability Registers ***********************/

/** @name XHCI_REG_CAP_HCS1 Register 
 */
#define XHCI_REG_CAP_HCS1_MAX_SLOTS_GET(x)    XHCI32_GET_BITS(x, 7, 0)  /* Number of Device Slots (MaxSlots) */
#define XHCI_REG_CAP_HCS1_MAX_INTRS_GET(x)	  XHCI32_GET_BITS(x, 18, 8)  /* Number of Interrupters (MaxIntrs) */
#define XHCI_REG_CAP_HCS1_MAX_PORTS_GET(x)	  XHCI32_GET_BITS(x, 31, 24) /* Number of Ports (MaxPorts) */

/** @name XHCI_REG_CAP_HCS2 Register 
 */
#define XHCI_REG_CAP_HCS2_IST_GET(x)			        XHCI32_GET_BITS(x, 3, 0)	/* Isochronous Scheduling Threshold (IST) */
#define XHCI_REG_CAP_HCS2_ERST_MAX_GET(x)		        XHCI32_GET_BITS(x, 7, 4)  /* Event Ring Segment Table Max (ERST Max) */
#define XHCI_REG_CAP_HCS2_SPR					        (1 << 26)                   /* Scratchpad Restore (SPR) */
#define XHCI_REG_CAP_HCS2_MAX_SCRATCHPAD_BUFS_GET(x)	XHCI32_GET_BITS(x, 25, 21) | XHCI32_GET_BITS(x, 31, 27)    /* Max Scratchpad Buffers (Max Scratchpad Bufs) */

/** @name XHCI_REG_CAP_HCS3 Register 
 */
#define XHCI_REG_CAP_HCS3_U1_DEV_EXIT_LATENCY_GET(x)	XHCI32_GET_BITS(x, 7, 0)	/* U1 Device Exit Latency */
#define XHCI_REG_CAP_HCS3_U2_DEV_EXIT_LATENCY_GET(x)	XHCI32_GET_BITS(x, 31, 16) /* U2 Device Exit Latency */

/** @name XHCI_REG_CAP_HCC Register 
 */
#define XHCI_REG_CAP_HCC_AC64		                (1 << 0)	/* 64-bit Addressing Capabilitya 1: 64-bit */
#define XHCI_REG_CAP_HCC_BNC			            (1 << 1)	/* BW Negotiation Capability (BNC) 1: support */
#define XHCI_REG_CAP_HCC_CSZ			            (1 << 2)	/* Context Size (CSZ) 1: 64 byte context data */
#define XHCI_REG_CAP_HCC_PPC			            (1 << 3)	/* Port Power Control (PPC) 1: support */
#define XHCI_REG_CAP_HCC_PIND		                (1 << 4)	/* Port Indicators (PIND) 1: support */
#define XHCI_REG_CAP_HCC_LHRC		                (1 << 5)	/* Light HC Reset Capability (LHRC) 1: support */
#define XHCI_REG_CAP_HCC_LTC			            (1 << 6)	/* Latency Tolerance Messaging Capability (LTC) */
#define XHCI_REG_CAP_HCC_NSS			            (1 << 7)	/* No Secondary SID Support (NSS) */
#define XHCI_REG_CAP_HCC_MAX_PSA_SIZE_GET(x)	    XHCI32_GET_BITS(x, 15, 12)    /* Maximum Primary Stream Array Size (MaxPSASize) */
#define XHCI_REG_CAP_HCC_XECP_GET(x)			    XHCI32_GET_BITS(x, 31, 16)    /* xHCI Extended Capabilities Pointer (xECP) */

/** @name XHCI_REG_CAP_DBOFF Register 
 */
#define XHCI_REG_CAP_DBOFF_GET(x)        ((x) & XHCI_GENMASK(31, 2))  /* 32-byte offset of the Doorbell Array base address from the Base */

/** @name XHCI_REG_CAP_RTSOFF Register 
 */
#define XHCI_REG_CAP_RTSOFF_GET(x)       ((x) & XHCI_GENMASK(31, 5)) /* 32-byte offset of the xHCI Runtime Registers */


/***************** Host Controller Operational Registers ***********************/

/** @name XHCI_REG_OP_USBCMD Register 
 */
#define XHCI_REG_OP_USBCMD_RUN_STOP		    (1 << 0)	/* Run/Stop (R/S) 1: RUN, 0: STOP - RW */
#define XHCI_REG_OP_USBCMD_HCRST			(1 << 1)	/* Host Controller Reset (HCRST) 1: RESET - RW */
#define XHCI_REG_OP_USBCMD_INTE			    (1 << 2)	/* Interrupter Enable (INTE) 1: enabled - RW */
#define XHCI_REG_OP_USBCMD_HSEE			    (1 << 3)	/* Host System Error Enable (HSEE) - RW */
#define XHCI_REG_OP_USBCMD_LHCRST			(1 << 7)	/* Light Host Controller Reset (LHCRST) - RW */
#define XHCI_REG_OP_USBCMD_CSS				(1 << 8)	/* Controller Save State (CSS) - RW */
#define XHCI_REG_OP_USBCMD_CRS				(1 << 9)	/* Controller Restore State (CRS) - RW */
#define XHCI_REG_OP_USBCMD_EWE				(1 << 10)	/* Enable Wrap Event (EWE) - RW */
#define XHCI_REG_OP_USBCMD_EU3S			    (1 << 11)	/* Enable U3 MFINDEX Stop (EU3S) - RW */

/** @name XHCI_REG_OP_USBSTS Register 
 */
#define XHCI_REG_OP_USBSTS_HCH				(1 << 0)	/* 1: Stopped executing */
#define XHCI_REG_OP_USBSTS_HSE				(1 << 2)	/* 1: Serious error detected */
#define XHCI_REG_OP_USBSTS_EINT				(1 << 3)	/* 1: Interrupt Pending (IP) */
#define XHCI_REG_OP_USBSTS_PCD				(1 << 4)	/* 1: Port Change Detect */
#define XHCI_REG_OP_USBSTS_SSS				(1 << 8)	/* remain 1 while the xHC saves its internal state */
#define XHCI_REG_OP_USBSTS_RSS				(1 << 9)	/* remain 1 while the xHC restores its internal state */
#define XHCI_REG_OP_USBSTS_SRE				(1 << 10)	/* if error occurs during a Save or Restore operation this bit shall be set to ‘1’. */
#define XHCI_REG_OP_USBSTS_CNR				(1 << 11)	/* 1: Controller Not Ready */
#define XHCI_REG_OP_USBSTS_HCE				(1 << 12)	/* 1: Internal xHC error condition */
#define XHCI_REG_OP_USBSTS_PRSRV_MASK		((1 << 1) | 0xffffe000) /* Rsvd bits */


/** @name XHCI_REG_OP_PAGESIZE Register 
 */
/* This xHC supports a page size of 2^(n+12) if bit n is Set */
#define XHCI_REG_OP_PAGESIZE_4K				(1 << 0)    /* if bit 0 is Set, the xHC supports 4k byte page sizes */

/** @name XHCI_REG_OP_CRCR Register 
 */
#define XHCI_REG_OP_CRCR_RCS				(1 << 0)	/* Ring Cycle State, value of the xHC Consumer Cycle State (CCS) flag */
#define XHCI_REG_OP_CRCR_CS					(1 << 1)	/* Command Stop, 1 */
#define XHCI_REG_OP_CRCR_CA					(1 << 2)	/* Command Abort, 1 */
#define XHCI_REG_OP_CRCR_CRR				(1 << 3)	/* Command Ring Running */
#define XHCI_REG_OP_CRCR_CR_PTR_MASK		XHCI_GENMASK_ULL(63, 6)	/* Command Ring Pointer, Dequeue Ptr of Command Ring */

/** @name XHCI_REG_OP_DCBAAP Register 
 */
#define XHCI_REG_OP_DCBAAP_MASK                     XHCI_GENMASK_ULL(63, 6)	            /* bit[31:6] Ptr of DCBAA */

/** @name XHCI_REG_OP_CONFIG Register 
 */
#define XHCI_REG_OP_CONFIG_MAX_SLOTS_EN_MASK        XHCI_GENMASK(7, 0)               /* Max Device Slots Enabled (MaxSlotsEn) – RW */
#define XHCI_REG_OP_CONFIG_MAX_SLOTS_EN_SET(x)		XHCI32_SET_BITS(x, 7, 0)	/* bit[7:0] Max Device Slots Enabled */
#define XHCI_REG_OP_CONFIG_MAX_SLOTS_EN_GET(x)      XHCI32_GET_BITS(x, 7, 0)

/** @name XHCI_REG_OP_PORTS_PORTSC Register 
 */
#define XHCI_REG_OP_PORTS_PORTSC_CCS			(1 << 0)	/* Current Connect Status (CCS) – ROS */
#define XHCI_REG_OP_PORTS_PORTSC_PED			(1 << 1)	/* Port Enabled/Disabled (PED) – RW1CS */
#define XHCI_REG_OP_PORTS_PORTSC_OCA			(1 << 3)	/* Over-current Active (OCA) – RO */
#define XHCI_REG_OP_PORTS_PORTSC_PR				(1 << 4)	/* Port Reset (PR) – RW1S */
#define XHCI_REG_OP_PORTS_PORTSC_PLS_GET(x) 	XHCI32_GET_BITS(x, 8, 5)	/* Port Link State (PLS) – RWS */
#define XHCI_REG_OP_PORTS_PORTSC_PLS_SET(x) 	XHCI32_SET_BITS(x, 8, 5)
#define XHCI_REG_OP_PORTS_PORTSC_PLS_MASK		XHCI_GENMASK(8, 5)
#define XHCI_REG_OP_PORTS_PORTSC_PLS(x)			(x << 5)
#define XHCI_REG_OP_PORTS_PORTSC_PLS_SET(x)	  	XHCI32_SET_BITS(x, 8, 5)

enum PLSStatus{
    PLS_U0              =  0,
    PLS_U1              =  1,
    PLS_U2              =  2,
    PLS_U3              =  3,
    PLS_DISABLED        =  4,
    PLS_RX_DETECT       =  5,
    PLS_INACTIVE        =  6,
    PLS_POLLING         =  7,
    PLS_RECOVERY        =  8,
    PLS_HOT_RESET       =  9,
    PLS_COMPILANCE_MODE = 10,
    PLS_TEST_MODE       = 11,
    PLS_RESUME          = 15,
}; /* Port status type */

#define XHCI_REG_OP_PORTS_PORTSC_PP (1 << 9)			/* Port Power (PP) – RWS */
#define XHCI_REG_OP_PORTS_PORTSC_PORT_SPEED_GET(x) XHCI32_GET_BITS(x, 13, 10)	/* Port Speed (Port Speed) – ROS */

/* Protocol Speed ID (PSI) */
#define XHCI_PORT_SPEED_UNKOWN		0U
#define XHCI_PORT_SPEED_FULL		1U
#define XHCI_PORT_SPEED_LOW			2U
#define XHCI_PORT_SPEED_HIGH		3U
#define XHCI_PORT_SPEED_SUPER		4U

#define XHCI_REG_OP_PORTS_PORTSC_PIC_SET(x)   XHCI32_SET_BITS(x, 15, 14)
#define XHCI_REG_OP_PORTS_PORTSC_PIC_MASK	  XHCI_GENMASK(15, 14)

#define XHCI_REG_OP_PORTS_PORTSC_LWS (1 << 16)		 /* Port Link State Write Strobe (LWS) */
#define XHCI_REG_OP_PORTS_PORTSC_CSC (1 << 17)		 /* Connect Status Change (CSC) */
#define XHCI_REG_OP_PORTS_PORTSC_PEC (1 << 18)		 /* Port Enabled/Disabled Change (PEC) 1: clear PED */
#define XHCI_REG_OP_PORTS_PORTSC_WRC (1 << 19)		 /* Warm Port Reset Change 1: Warm Reset complete */
#define XHCI_REG_OP_PORTS_PORTSC_OCC (1 << 20)		 /* Over-current Change 1: Over-current Active */
#define XHCI_REG_OP_PORTS_PORTSC_PRC (1 << 21)		 /* Port Reset Change 1: Transition of Port Reset */
#define XHCI_REG_OP_PORTS_PORTSC_PLC (1 << 22)		 /* Port Link State Change 1: PLS transition */
#define XHCI_REG_OP_PORTS_PORTSC_CEC (1 << 23)		 /* Port Config Error Change 1: Port Config Error detected */
#define XHCI_REG_OP_PORTS_PORTSC_CAS (1 << 24)		 /* Cold Attach Status  1: Far-end Receiver Terminations were detected */
#define XHCI_REG_OP_PORTS_PORTSC_WCE (1 << 25)		 /* Wake on Connect Enable 1: enable port to be sensitive to device connects */
#define XHCI_REG_OP_PORTS_PORTSC_WDE (1 << 26)		 /* Wake on Disconnect Enable 1: enable port to be sensitive to device disconnects */
#define XHCI_REG_OP_PORTS_PORTSC_WOE (1 << 27)		 /* Wake on Over-current Enable 1: enable port to be sensitive to  over-current conditions */
#define XHCI_REG_OP_PORTS_PORTSC_DR  (1 << 30)		 /* Device Removable, 0: Device is removable. 1:  Device is non-removable */
#define XHCI_REG_OP_PORTS_PORTSC_WPR (1 << 31)		 /* Warm Port Reset 1: follow Warm Reset sequence */
#define XHCI_REG_OP_PORTS_PORTSC_RW_MASK (XHCI_REG_OP_PORTS_PORTSC_PR | XHCI_REG_OP_PORTS_PORTSC_PLS_MASK | XHCI_REG_OP_PORTS_PORTSC_PP \
										| XHCI_REG_OP_PORTS_PORTSC_PIC_MASK | XHCI_REG_OP_PORTS_PORTSC_LWS | XHCI_REG_OP_PORTS_PORTSC_WCE \
										| XHCI_REG_OP_PORTS_PORTSC_WDE | XHCI_REG_OP_PORTS_PORTSC_WOE)


/***************** Host Controller Runtime Registers ***********************/

/** @name XHCI_REG_RT_IR_IMAN Register 
 */
#define XHCI_REG_RT_IR_IMAN_IP				(1 << 0)	/* Interrupt Pending, 1: an interrupt is pending for this Interrupter */
#define XHCI_REG_RT_IR_IMAN_IE				(1 << 1)	/* Interrupt Enable, 1: capable of generating an interrupt. */

/** @name XHCI_REG_RT_IR_IMOD Register 
 */
#define XHCI_REG_RT_IR_IMOD_IMODI_MASK			XHCI_GENMASK(15, 0)	/* bit[15:0] Interrupt Moderation Interval default 4000 ==> 1ms  */
#define XHCI_REG_RT_IR_IMOD_IMODC_MASK			XHCI_GENMASK(31, 16)	/* bit[31:16] Interrupt Moderation Counter(Down counter) */

/** @name XHCI_REG_RT_IR_ERSTSZ Register 
 */
#define XHCI_REG_RT_IR_ERSTSZ_MASK			    XHCI_GENMASK(15, 0)	/* bit[15:0] the number of valid Event Ring Segment Table entries */

/** @name XHCI_REG_RT_IR_ERSTBA Register 
 */
#define XHCI_REG_RT_IR_ERSTBA_MASK			    XHCI_GENMASK_ULL(63, 6) /* Event Ring Segment Table Base Address */

/** @name XHCI_REG_RT_IR_ERDP Register 
 */
#define XHCI_REG_RT_IR_ERDP_DESI_MASK			XHCI_GENMASK_ULL(2, 0)	/* bit[2:0] Dequeue ERST Segment Index */
#define XHCI_REG_RT_IR_ERDP_EHB					(1 << 3)			/* Event Handler Busy */
#define XHCI_REG_RT_IR_ERDP_MASK				XHCI_GENMASK_ULL(63, 4)	/* Event Ring Dequeue Pointer */

/***************** Doorbell Register ***********************/
#define XHCI_REG_DB_TARGET_HC_COMMAND			0   /* Host Controller Doorbell (0) Command Doorbell */
#define XHCI_REG_DB_TARGET_EP0					1   /* Device Context Doorbells Control EP 0 Enqueue Pointer Update */
#define XHCI_REG_DB_TARGET_EP1_OUT				2   /* EP 1 OUT Enqueue Pointer Update */
#define XHCI_REG_DB_TARGET_EP1_IN				3   /* EP 1 IN Enqueue Pointer Update */
#define XHCI_REG_DB_TARGET_EP15_OUT				30	/* EP 15 OUT Enqueue Pointer Update */
#define XHCI_REG_DB_TARGET_EP15_IN				31  /* EP 15 IN Enqueue Pointer Update */

/***************** xHCI Extended Capabilities Registers ***********************/
#define XHCI_REG_EXT_CAP_USBSPCF_OFFSET			     0x0
#define XHCI_REG_EXT_CAP_CAP_ID_GET(x)				 XHCI32_GET_BITS(x, 7, 0)
#define XHCI_REG_EXT_NEXT_CAP_PTR_GET(x)             XHCI32_GET_BITS(x, 15, 8)
/* refer to 'Table 138: xHCI Extended Capability Codes' for more details */
enum
{
	XHCI_EXT_CAP_ID_USB_LEGACY_SUPPORT 		= 1,
	XHCI_EXT_CAP_ID_SUPPORT_PROTOCOL 		= 2,
	XHCI_EXT_CAP_ID_EXTEND_POWER_MANAGEMENT = 3,
	XHCI_EXT_CAP_ID_IO_VIRTUALIZATION 		= 4,
	XHCI_EXT_CAP_ID_MESSAGE_INTERRUPT 		= 5,
	XHCI_EXT_CAP_ID_LOCAL_MEMORY 			= 6,
	XHCI_EXT_CAP_ID_USB_DEBUG_CAPABILITY 	= 10,
	XHCI_EXT_CAP_ID_EXT_MESSAGE_INTERRUPT 	= 17,

	XHCI_EXT_CAP_ID_VENDOR_DEFINED_MIN 		= 192,
	XHCI_EXT_CAP_ID_VENDOR_DEFINED_MAX 		= 255
};

/* xHCI Supported Protocol Capability */
#define XHCI_REG_EXT_CAP_USBSPCFDEF_OFFSET		    0x4

#define XHCI_USBSPCF_MINOR_REVERSION_GET(x)		    XHCI32_GET_BITS(x, 23, 16)
#define XHCI_USBSPCF_MAJOR_REVERSION_GET(x) 		XHCI32_GET_BITS(x, 31, 24)

#define XHCI_USBSPCFDEF_NAME_STRING_GET(x)		    XHCI32_GET_BITS(x, 31, 0)	/* four ASCII characters may be defined */
#define XHCI_USBSPCFDEF_NAME_STRING_USB		        0x20425355 /* ASCII = "USB" */

#define XHCI_REG_EXT_CAP_USBSPCFDEF2_OFFSET		    0x8
#define XHCI_USBSPCFDEF2_COMPATIBLE_PORT_OFF_GET(x) XHCI32_GET_BITS(x, 7, 0)
#define XHCI_USBSPCFDEF2_COMPATIBLE_PORT_CNT_GET(x) XHCI32_GET_BITS(x, 15, 8)
#define XHCI_USBSPCFDEF2_PROTOCOL_DEFINED_GET(x)	XHCI32_GET_BITS(x, 27, 16)

/* trb bit definitions */

/* configuration */
#define XHCI_RING_ITEMS     16U
#define XHCI_ALIGMENT       64U
#define XHCI_RING_SIZE      (XHCI_RING_ITEMS*sizeof(struct xhci_trb))

#define TRB_C               (1<<0)
#define TRB_TYPE_SHIFT      10
#define TRB_TYPE_MASK       0x3f
#define TRB_TYPE_GET(val)   XHCI32_GET_BITS(val, 15, 10)
#define TRB_TYPE_SET(t)     XHCI32_SET_BITS(t, 15, 10)

#define TRB_EV_ED           (1<<2)

#define TRB_TR_ENT          (1<<1)
#define TRB_TR_ISP          (1<<2)
#define TRB_TR_NS           (1<<3)
#define TRB_TR_CH           (1<<4)
#define TRB_TR_IOC          (1<<5)
#define TRB_TR_IDT          (1<<6)
#define TRB_TR_TBC_SHIFT    7
#define TRB_TR_TBC_MASK     0x3
#define TRB_TR_BEI          (1<<9)
#define TRB_TR_TLBPC_SHIFT  16
#define TRB_TR_TLBPC_MASK   0xf
#define TRB_TR_FRAMEID_SHIFT 20
#define TRB_TR_FRAMEID_MASK 0x7ff
#define TRB_TR_SIA          (1<<31)
#define TRB_TR_TRANS_LEN_SET(len)  XHCI32_SET_BITS(len, 23, 0)    
#define TRB_TR_TRANS_LEN_MASK      XHCI_GENMASK(23, 0)

#define TRB_TR_DIR           (1<<16)
#define TRB_TR_TYPE_SET(t)   XHCI32_SET_BITS(t, 17, 16)
#define TRB_TR_NO_DATA      0U
#define TRB_TR_OUT_DATA     2U
#define TRB_TR_IN_DATA      3U

#define TRB_CC_GET(val)          XHCI32_GET_BITS(val, 31, 24)
#define TRB_PORT_ID_GET(val)     XHCI32_GET_BITS(val, 31, 24)


#define TRB_CR_SLOTID_SHIFT 24
#define TRB_CR_SLOTID_MASK  0xff
#define TRB_CR_SLOTID_SET(id)    XHCI32_SET_BITS(id, 31, 24)
#define TRB_CR_SLOTID_GET(val)   XHCI32_GET_BITS(val, 31, 24)

#define TRB_CR_EPID_SHIFT   16
#define TRB_CR_EPID_MASK    0x1f
#define TRB_CR_EPID_SET(id)      XHCI32_SET_BITS(id, 20, 16)
#define TRB_CR_EPID_GET(field)   XHCI32_GET_BITS(field, 20, 16)

#define TRB_CR_BSR          (1<<9)
#define TRB_CR_DC           (1<<9)

#define TRB_LK_TC           (1<<1)

#define TRB_INTR_SHIFT      22
#define TRB_INTR_MASK       0x3ff
#define TRB_INTR(t)         (((t).status >> TRB_INTR_SHIFT) & TRB_INTR_MASK)

/************************** Type Definitions *********************************/
enum TRBType {
    TRB_RESERVED = 0,
    TR_NORMAL,
    TR_SETUP,
    TR_DATA,
    TR_STATUS,
    TR_ISOCH,
    TR_LINK,
    TR_EVDATA,
    TR_NOOP,
    CR_ENABLE_SLOT,
    CR_DISABLE_SLOT,
    CR_ADDRESS_DEVICE,
    CR_CONFIGURE_ENDPOINT,
    CR_EVALUATE_CONTEXT,
    CR_RESET_ENDPOINT,
    CR_STOP_ENDPOINT,
    CR_SET_TR_DEQUEUE,
    CR_RESET_DEVICE,
    CR_FORCE_EVENT,
    CR_NEGOTIATE_BW,
    CR_SET_LATENCY_TOLERANCE,
    CR_GET_PORT_BANDWIDTH,
    CR_FORCE_HEADER,
    CR_NOOP,
    ER_TRANSFER_COMPLETE = 32,
    ER_COMMAND_COMPLETE,
    ER_PORT_STATUS_CHANGE,
    ER_BANDWIDTH_REQUEST,
    ER_DOORBELL,
    ER_HOST_CONTROLLER,
    ER_DEVICE_NOTIFICATION,
    ER_MFINDEX_WRAP,
};

enum TRBCCode {
    CC_DISCONNECTED = -2,
    CC_TIMEOUT = -1,
    CC_INVALID = 0,
    CC_SUCCESS,
    CC_DATA_BUFFER_ERROR,
    CC_BABBLE_DETECTED,
    CC_USB_TRANSACTION_ERROR,
    CC_TRB_ERROR,
    CC_STALL_ERROR,
    CC_RESOURCE_ERROR,
    CC_BANDWIDTH_ERROR,
    CC_NO_SLOTS_ERROR,
    CC_INVALID_STREAM_TYPE_ERROR,
    CC_SLOT_NOT_ENABLED_ERROR,
    CC_EP_NOT_ENABLED_ERROR,
    CC_SHORT_PACKET,
    CC_RING_UNDERRUN,
    CC_RING_OVERRUN,
    CC_VF_ER_FULL,
    CC_PARAMETER_ERROR,
    CC_BANDWIDTH_OVERRUN,
    CC_CONTEXT_STATE_ERROR,
    CC_NO_PING_RESPONSE_ERROR,
    CC_EVENT_RING_FULL_ERROR,
    CC_INCOMPATIBLE_DEVICE_ERROR,
    CC_MISSED_SERVICE_ERROR,
    CC_COMMAND_RING_STOPPED,
    CC_COMMAND_ABORTED,
    CC_STOPPED,
    CC_STOPPED_LENGTH_INVALID,
    CC_MAX_EXIT_LATENCY_TOO_LARGE_ERROR = 29,
    CC_ISOCH_BUFFER_OVERRUN = 31,
    CC_EVENT_LOST_ERROR,
    CC_UNDEFINED_ERROR,
    CC_INVALID_STREAM_ID_ERROR,
    CC_SECONDARY_BANDWIDTH_ERROR,
    CC_SPLIT_TRANSACTION_ERROR
};

/***************** Macros (Inline Functions) Definitions *********************/
/*
 *  xhci_ring structs are allocated with XHCI_RING_SIZE alignment,
 *  then we can get it from a trb pointer (provided by evt ring).
 */
#define XHCI_RING(_trb)          \
    ((struct xhci_ring*)((unsigned long)(_trb) & ~(XHCI_RING_SIZE-1)))

#define BARRIER() __asm__ __volatile__("": : :"memory")

#ifdef XHCI_AARCH64
#define DSB() __asm__ __volatile__("dsb sy": : : "memory")
#else
#define DSB() __asm__ __volatile__("dsb": : : "memory")
#endif

/************************** Function Prototypes ******************************/

/*****************************************************************************/
static inline void writeq(unsigned long addr, uint64_t val) {
    BARRIER();
    *(volatile uint64_t *)addr = val;
}

static inline void writel(unsigned long addr, uint32_t val) {
    BARRIER();
    *(volatile uint32_t *)addr = val;
}

static inline void writew(unsigned long addr, uint16_t val) {
    BARRIER();
    *(volatile uint16_t *)addr = val;
}

static inline void writeb(unsigned long addr, uint8_t val) {
    BARRIER();
    *(volatile uint8_t *)addr = val;
}

static inline uint64_t readq(unsigned long addr) {
    uint64_t val = *(volatile const uint64_t *)addr;
    BARRIER();
    return val;
}

static inline uint32_t readl(unsigned long addr) {
    uint32_t val = *(volatile const uint32_t *)addr;
    BARRIER();
    return val;
}

static inline uint16_t readw(unsigned long addr) {
    uint16_t val = *(volatile const uint16_t *)addr;
    BARRIER();
    return val;
}

static inline uint8_t readb(unsigned long addr) {
    uint8_t val = *(volatile const uint8_t *)addr;
    BARRIER();
    return val;
}

#ifdef __cplusplus
}
#endif

#endif