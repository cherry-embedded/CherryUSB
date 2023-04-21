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
 * Description:  This file is for xhci register definition.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 * 2.0   zhugengyu  2023/3/29   support usb3.0 device attached at roothub
 */

#ifndef XHCI_REG_H
#define XHCI_REG_H

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

/** Capability register length */
#define XHCI_CAP_CAPLENGTH 0x00

/** Host controller interface version number */
#define XHCI_CAP_HCIVERSION 0x02

/** Structural parameters 1 */
#define XHCI_CAP_HCSPARAMS1 0x04

/** Number of device slots */
#define XHCI_HCSPARAMS1_SLOTS(params) ( ( (params) >> 0 ) & 0xff )

/** Number of interrupters */
#define XHCI_HCSPARAMS1_INTRS(params) ( ( (params) >> 8 ) & 0x3ff )

/** Number of ports */
#define XHCI_HCSPARAMS1_PORTS(params) ( ( (params) >> 24 ) & 0xff )

/** Structural parameters 2 */
#define XHCI_CAP_HCSPARAMS2 0x08

/** Number of page-sized scratchpad buffers */
#define XHCI_HCSPARAMS2_SCRATCHPADS(params) \
	( ( ( (params) >> 16 ) & 0x3e0 ) | ( ( (params) >> 27 ) & 0x1f ) )

/** Capability parameters */
#define XHCI_CAP_HCCPARAMS1 0x10

/** 64-bit addressing capability */
#define XHCI_HCCPARAMS1_ADDR64(params) ( ( (params) >> 0 ) & 0x1 )

/** Context size shift */
#define XHCI_HCCPARAMS1_CSZ_SHIFT(params) ( 5 + ( ( (params) >> 2 ) & 0x1 ) )

/** xHCI extended capabilities pointer */
#define XHCI_HCCPARAMS1_XECP(params) ( ( ( (params) >> 16 ) & 0xffff ) << 2 )

/** Doorbell offset */
#define XHCI_CAP_DBOFF 0x14

/** Runtime register space offset */
#define XHCI_CAP_RTSOFF 0x18

/** xHCI extended capability ID */
#define XHCI_XECP_ID(xecp) ( ( (xecp) >> 0 ) & 0xff )

/** Next xHCI extended capability pointer */
#define XHCI_XECP_NEXT(xecp) ( ( ( (xecp) >> 8 ) & 0xff ) << 2 )

/** USB legacy support extended capability */
#define XHCI_XECP_ID_LEGACY 1

/** USB legacy support BIOS owned semaphore */
#define XHCI_USBLEGSUP_BIOS 0x02

/** USB legacy support BIOS ownership flag */
#define XHCI_USBLEGSUP_BIOS_OWNED 0x01

/** USB legacy support OS owned semaphore */
#define XHCI_USBLEGSUP_OS 0x03

/** USB legacy support OS ownership flag */
#define XHCI_USBLEGSUP_OS_OWNED 0x01

/** USB legacy support control/status */
#define XHCI_USBLEGSUP_CTLSTS 0x04

/** Supported protocol extended capability */
#define XHCI_XECP_ID_SUPPORTED 2

/** Supported protocol revision */
#define XHCI_SUPPORTED_REVISION 0x00

/** Supported protocol minor revision */
#define XHCI_SUPPORTED_REVISION_VER(revision) ( ( (revision) >> 16 ) & 0xffff )

/** Supported protocol name */
#define XHCI_SUPPORTED_NAME 0x04

/** Supported protocol ports */
#define XHCI_SUPPORTED_PORTS 0x08

/** Supported protocol port offset */
#define XHCI_SUPPORTED_PORTS_OFFSET(ports) ( ( (ports) >> 0 ) & 0xff )

/** Supported protocol port count */
#define XHCI_SUPPORTED_PORTS_COUNT(ports) ( ( (ports) >> 8 ) & 0xff )

/** Supported protocol PSI count */
#define XHCI_SUPPORTED_PORTS_PSIC(ports) ( ( (ports) >> 28 ) & 0x0f )

/** Supported protocol slot */
#define XHCI_SUPPORTED_SLOT 0x0c

/** Supported protocol slot type */
#define XHCI_SUPPORTED_SLOT_TYPE(slot) ( ( (slot) >> 0 ) & 0x1f )

/** Supported protocol PSI */
#define XHCI_SUPPORTED_PSI(index) ( 0x10 + ( (index) * 4 ) )

/** Supported protocol PSI value */
#define XHCI_SUPPORTED_PSI_VALUE(psi) ( ( (psi) >> 0 ) & 0x0f )

/** Supported protocol PSI mantissa */
#define XHCI_SUPPORTED_PSI_MANTISSA(psi) ( ( (psi) >> 16 ) & 0xffff )

/** Supported protocol PSI exponent */
#define XHCI_SUPPORTED_PSI_EXPONENT(psi) ( ( (psi) >> 4 ) & 0x03 )

/** Default PSI values */
enum {
	/** Full speed (12Mbps) */
	XHCI_SPEED_FULL = 1,
	/** Low speed (1.5Mbps) */
	XHCI_SPEED_LOW = 2,
	/** High speed (480Mbps) */
	XHCI_SPEED_HIGH = 3,
	/** Super speed */
	XHCI_SPEED_SUPER = 4,
};

/** USB command register */
#define XHCI_OP_USBCMD 0x00

/** Run/stop */
#define XHCI_USBCMD_RUN 0x00000001UL

/* Interrupter Enable (INTE) 1: enabled - RW */
#define XHCI_USBCMD_INTE  (1UL << 2)

/** Host controller reset */
#define XHCI_USBCMD_HCRST 0x00000002UL

/** USB status register */
#define XHCI_OP_USBSTS 0x04

/** Host controller halted */
#define XHCI_USBSTS_HCH 0x00000001UL

/** Interrupt Pending (IP) */
#define XHCI_USBSTS_EINT (1UL << 3)

/** Page size register */
#define XHCI_OP_PAGESIZE 0x08

/** Page size */
#define XHCI_PAGESIZE(pagesize) ( (pagesize) << 12 )

/** Device notifcation control register */
#define XHCI_OP_DNCTRL 0x14

/** Command ring control register */
#define XHCI_OP_CRCR 0x18

/** Command ring cycle state */
#define XHCI_CRCR_RCS 0x00000001UL

/** Command abort */
#define XHCI_CRCR_CA 0x00000004UL

/** Command ring running */
#define XHCI_CRCR_CRR 0x00000008UL

/** Device context base address array pointer */
#define XHCI_OP_DCBAAP 0x30

/** Configure register */
#define XHCI_OP_CONFIG 0x38

/** Maximum device slots enabled */
#define XHCI_CONFIG_MAX_SLOTS_EN(slots) ( (slots) << 0 )

/** Maximum device slots enabled mask */
#define XHCI_CONFIG_MAX_SLOTS_EN_MASK \
	XHCI_CONFIG_MAX_SLOTS_EN ( 0xff )

/** Port status and control register */
#define XHCI_OP_PORTSC(port) ( 0x400 - 0x10 + ( (port) << 4 ) )

/** Current connect status */
#define XHCI_PORTSC_CCS 0x00000001UL

/** Port enabled */
#define XHCI_PORTSC_PED 0x00000002UL

#define XHCI_PORTSC_OCA (1 << 3)

/** Port reset */
#define XHCI_PORTSC_PR 0x00000010UL

/** Port link state */
#define XHCI_PORTSC_PLS(pls) ( (pls) << 5 )

/** U0 state */
#define XHCI_PORTSC_PLS_U0 XHCI_PORTSC_PLS ( 0 )

/** Disabled port link state */
#define XHCI_PORTSC_PLS_DISABLED XHCI_PORTSC_PLS ( 4 )

/** RxDetect port link state */
#define XHCI_PORTSC_PLS_RXDETECT XHCI_PORTSC_PLS ( 5 )

/** Polling state */
#define XHCI_PORTSC_PLS_POLLING XHCI_PORTSC_PLS ( 7 )

/** Port link state mask */
#define XHCI_PORTSC_PLS_MASK XHCI_PORTSC_PLS ( 0xf )

/** Port power */
#define XHCI_PORTSC_PP 0x00000200UL

/** Time to delay after enabling power to a port */
#define XHCI_PORT_POWER_DELAY_MS 20

/** Port speed ID value */
#define XHCI_PORTSC_PSIV(portsc) ( ( (portsc) >> 10 ) & 0xf )

/** Port indicator control */
#define XHCI_PORTSC_PIC(indicators) ( (indicators) << 14 )

/** Port indicator control mask */
#define XHCI_PORTSC_PIC_MASK XHCI_PORTSC_PIC ( 3 )

/** Port link state write strobe */
#define XHCI_PORTSC_LWS 0x00010000UL

/** Time to delay after writing the port link state */
#define XHCI_LINK_STATE_DELAY_MS 100

/** Connect status change */
#define XHCI_PORTSC_CSC (1 << 17)

/** Port enabled/disabled change */
#define XHCI_PORTSC_PEC (1 << 18)

/** Warm port reset change */
#define XHCI_PORTSC_WRC (1 << 19) 

/** Over-current change */
#define XHCI_PORTSC_OCC (1 << 20)

/** Port reset change */
#define XHCI_PORTSC_PRC (1 << 21)

/** Port link state change */
#define XHCI_PORTSC_PLC (1 << 22)

/** Port config error change */
#define XHCI_PORTSC_CEC (1 << 23)

/* Cold Attach Status  1: Far-end Receiver Terminations were detected */
#define XHCI_PORTSC_CAS (1 << 24)       

/* Wake on Connect Enable 1: enable port to be sensitive to device connects */
#define XHCI_PORTSC_WCE (1 << 25)       

/* Wake on Disconnect Enable 1: enable port to be sensitive to device disconnects */
#define XHCI_PORTSC_WDE (1 << 26)       

/* Wake on Over-current Enable 1: enable port to be sensitive to  over-current conditions */
#define XHCI_PORTSC_WOE (1 << 27)       

/* Device Removable, 0: Device is removable. 1:  Device is non-removable */
#define XHCI_PORTSC_DR  (1 << 30)       

/* Warm Port Reset 1: follow Warm Reset sequence */
#define XHCI_PORTSC_WPR (1 << 31)   

/** Port status change mask */
#define XHCI_PORTSC_CHANGE					\
	( XHCI_PORTSC_CSC | XHCI_PORTSC_PEC | XHCI_PORTSC_WRC |	\
	  XHCI_PORTSC_OCC | XHCI_PORTSC_PRC | XHCI_PORTSC_PLC |	\
	  XHCI_PORTSC_CEC )

#define XHCI_PORTSC_RW_MASK (XHCI_PORTSC_PR | XHCI_PORTSC_PLS_MASK | XHCI_PORTSC_PP \
							| XHCI_PORTSC_PIC_MASK | XHCI_PORTSC_LWS | XHCI_PORTSC_WCE \
							| XHCI_PORTSC_WDE | XHCI_PORTSC_WOE)

/** Port status and control bits which should be preserved
 *
 * The port status and control register is a horrendous mix of
 * differing semantics.  Some bits are written to only when a separate
 * write strobe bit is set.  Some bits should be preserved when
 * modifying other bits.  Some bits will be cleared if written back as
 * a one.  Most excitingly, the "port enabled" bit has the semantics
 * that 1=enabled, 0=disabled, yet writing a 1 will disable the port.
 */
#define XHCI_PORTSC_PRESERVE ( XHCI_PORTSC_PP | XHCI_PORTSC_PIC_MASK )

/** Port power management status and control register */
#define XHCI_OP_PORTPMSC(port) ( 0x404 - 0x10 + ( (port) << 4 ) )

/** Port link info register */
#define XHCI_OP_PORTLI(port) ( 0x408 - 0x10 + ( (port) << 4 ) )

/** Port hardware link power management control register */
#define XHCI_OP_PORTHLPMC(port) ( 0x40c - 0x10 + ( (port) << 4 ) )

/* Doorbell registers are 32 bits in length */
#define XHCI_REG_DB_SIZE                4       

/** Interrupter Management Register */
#define XHCI_RUN_IR_IMAN(intr) ( 0x20 + ( (intr) << 5 ) )

/* Interrupt Pending, 1: an interrupt is pending for this Interrupter */
#define XHCI_RUN_IR_IMAN_IP              (1 << 0)

/* Interrupt Enable, 1: capable of generating an interrupt. */
#define XHCI_RUN_IR_IMAN_IE              (1 << 1)

/** Interrupter Moderation Register */
#define XHCI_RUN_IR_IMOD(intr) ( 0x24 + ( (intr) << 5 ) )

/** Event ring segment table size register */
#define XHCI_RUN_ERSTSZ(intr) ( 0x28 + ( (intr) << 5 ) )

/** Event ring segment table base address register */
#define XHCI_RUN_ERSTBA(intr) ( 0x30 + ( (intr) << 5 ) )

/** Event ring dequeue pointer register */
#define XHCI_RUN_ERDP(intr) ( 0x38 + ( (intr) << 5 ) )

/** Event Handler Busy */
#define XHCI_RUN_ERDP_EHB	(1 << 3)

/** Minimum alignment required for data structures
 *
 * With the exception of the scratchpad buffer pages (which are
 * page-aligned), data structures used by xHCI generally require from
 * 16 to 64 byte alignment and must not cross an (xHCI) page boundary.
 * We simplify this requirement by aligning each structure on its own
 * size, with a minimum of a 64 byte alignment.
 */
#define XHCI_MIN_ALIGN 64

/** Maximum transfer size */
#define XHCI_MTU 65536

/** Read/Write Data Barrier for ARM */
#define BARRIER() __asm__ __volatile__("": : :"memory")

#ifdef XHCI_AARCH64
#define DSB() __asm__ __volatile__("dsb sy": : : "memory")
#else
#define DSB() __asm__ __volatile__("dsb": : : "memory")
#endif

/**
 * Read byte from memory-mapped device
 *
 * @v io_addr		I/O address
 * @ret data		Value read
 */
static inline uint8_t readb(void *io_addr ) {
    uint8_t val = *(volatile const uint8_t *)io_addr;
    BARRIER();
    return val;
}

/**
 * Read 16-bit word from memory-mapped device
 *
 * @v io_addr		I/O address
 * @ret data		Value read
 */
static inline uint16_t readw(void * io_addr ) {
    uint16_t val = *(volatile const uint16_t *)io_addr;
    BARRIER();
    return val;
}

/**
 * Read 32-bit dword from memory-mapped device
 *
 * @v io_addr		I/O address
 * @ret data		Value read
 */
static inline uint32_t readl(void * io_addr ) {
    uint32_t val = *(volatile const uint32_t *)io_addr;
    BARRIER();
    return val;
}

/**
 * Read 64-bit qword from memory-mapped device
 *
 * @v io_addr		I/O address
 * @ret data		Value read
 */
static inline uint64_t readq(void * io_addr ) {
    uint64_t val = *(volatile const uint64_t *)io_addr;
    BARRIER();
    return val;
}

/**
 * Write byte to memory-mapped device
 *
 * @v data		Value to write
 * @v io_addr		I/O address
 */
static inline void writeb(uint8_t data, void * io_addr ) {
    BARRIER();
    *(volatile uint8_t *)io_addr = data;
}

/**
 * Write 16-bit word to memory-mapped device
 *
 * @v data		Value to write
 * @v io_addr	I/O address
 */
static inline void writew(uint16_t data, void * io_addr ) {
    BARRIER();
    *(volatile uint16_t *)io_addr = data;
}

/**
 * Write 32-bit dword to memory-mapped device
 *
 * @v data		Value to writed
 * @v io_addr		I/O address
 */
static inline void writel(uint32_t data, void * io_addr ) {
    BARRIER();
    *(volatile uint32_t *)io_addr = data;
}

/**
 * Write 64-bit qword to memory-mapped device
 *
 * @v data		Value to write
 * @v io_addr		I/O address
 */
static inline void writeq(uint64_t data, void * io_addr ) {
    BARRIER();
    *(volatile uint64_t *)io_addr = data;
}

/**
 *  Byte-order converter for ARM-Little-End 
 */
#define CPU_TO_LE64(x) ((uint64_t)(x))
#define LE64_to_CPU(x) ((uint64_t)(x))
#define CPU_TO_LE32(x) ((uint32_t)(x))
#define LE32_TO_CPU(x) ((uint32_t)(x))
#define CPU_TO_LE16(x) ((uint16_t)(x))
#define LE16_TO_CPU(x) ((uint16_t)(x))


#endif /* XHCI_REG_H */