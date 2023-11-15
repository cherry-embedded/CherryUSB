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
 * FilePath: xhci.c
 * Date: 2022-07-19 09:26:25
 * LastEditTime: 2022-07-19 09:26:25
 * Description:  This file is for xhci functions implmentation.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 * 2.0   zhugengyu  2023/3/29   support usb3.0 device attached at roothub
 */

#include <string.h>

#include "usbh_core.h"
#include "usbh_hub.h"

#include "xhci_reg.h"
#include "xhci.h"

extern struct usbh_hubport *usbh_get_roothub_port(unsigned int port);

#ifdef __aarch64__
/* find last 64bit set, binary search */
int xhci_fls(unsigned long v)
{
	int n = 64;

	if (!v) return -1;
	if (!(v & 0xFFFFFFFF00000000)) { v <<= 32; n -= 32; }
	if (!(v & 0xFFFF000000000000)) { v <<= 16; n -= 16; }
	if (!(v & 0xFF00000000000000)) { v <<=  8; n -= 8;  }
	if (!(v & 0xF000000000000000)) { v <<=  4; n -= 4;  }
	if (!(v & 0xC000000000000000)) { v <<=  2; n -= 2;  }
	if (!(v & 0x8000000000000000)) { v <<=  1; n -= 1;  }

	return n - 1;
}
#else
/* find first bit set, binary search */
int xhci_fls(unsigned int v)
{
	int n = 32;

	if (!v) return -1;
	if (!(v & 0xFFFF0000)) { v <<= 16; n -= 16; }
	if (!(v & 0xFF000000)) { v <<=  8; n -= 8;  }
	if (!(v & 0xF0000000)) { v <<=  4; n -= 4;  }
	if (!(v & 0xC0000000)) { v <<=  2; n -= 2;  }
	if (!(v & 0x80000000)) { v <<=  1; n -= 1;  }

	return n - 1;
}
#endif

/**
 * Get USB transaction translator
 *
 * @v hport		Hub port of USB device
 * @ret port    Transaction translator port, or NULL
 */
struct usbh_hubport *usbh_transaction_translator(struct usbh_hubport *hport)
{
    struct usbh_hubport *parent;

    if (hport->parent->is_roothub) {
        return NULL;
    }

    /* Navigate up to root hub.  If we find a low-speed or
	 * full-speed device with a higher-speed parent hub, then that
	 * device's port is the transaction translator.
	 */
    for (; (parent = hport->parent->parent); hport = parent) {
        if ((hport->speed <= USB_SPEED_FULL) &&
            (parent->speed > USB_SPEED_FULL)) {
            return hport;
        }
    }

    return NULL;
}

/**
 * Get USB route string
 *
 * @v hport		Hub Port of USB device
 * @ret route	USB route string
 */
unsigned int usbh_route_string(struct usbh_hubport *hport)
{
    struct usbh_hubport *parent;
    unsigned int route;

    /* Navigate up to root hub, constructing route string as we go */
    for (route = 0; (parent = hport->parent->parent); hport = parent) {
        route <<= 4;
        route |= ((hport->dev_addr > 0xf) ?
                      0xf :
                      hport->dev_addr);
    }

    return route;
}

/**
 * Get USB root hub port
 *
 * @v usb		Hub port of USB device
 * @ret port	Root hub port
 */
struct usbh_hubport *usbh_root_hub_port(struct usbh_hubport *hport)
{
    struct usbh_hubport *parent;

    /* Navigate up to root hub */
    while (parent = hport->parent->parent) {
        hport = parent;
    }

    return hport;
}

/** @file
 *
 * USB eXtensible Host Controller Interface (xHCI) driver
 *
 */

#define XHCI_DUMP  1

/** Define a XHCI speed in PSI
 *
 * @v mantissa		Mantissa
 * @v exponent		Exponent (in engineering terms: 1=k, 2=M, 3=G)
 * @ret speed		USB speed
 */
#define XCHI_PSI( mantissa, exponent ) ( (exponent << 16) | (mantissa) )

/** USB device speeds */
enum {
	/** Not connected */
	XCHI_PSI_NONE = 0,
	/** Low speed (1.5Mbps) */
	XCHI_PSI_LOW = XCHI_PSI ( 1500, 1 ),
	/** Full speed (12Mbps) */
	XCHI_PSI_FULL = XCHI_PSI ( 12, 2 ),
	/** High speed (480Mbps) */
	XCHI_PSI_HIGH = XCHI_PSI ( 480, 2 ),
	/** Super speed (5Gbps) */
	XCHI_PSI_SUPER = XCHI_PSI ( 5, 3 ),
};


/**
 * Calculate buffer alignment
 *
 * @v len		Length
 * @ret align		Buffer alignment
 *
 * Determine alignment required for a buffer which must be aligned to
 * at least XHCI_MIN_ALIGN and which must not cross a page boundary.
 */
static inline size_t xhci_align ( size_t len ) {
	size_t align;

	/* Align to own length (rounded up to a power of two) */
	align = ( 1 << xhci_fls ( len - 1 ) );

	/* Round up to XHCI_MIN_ALIGN if needed */
	if ( align < XHCI_MIN_ALIGN )
		align = XHCI_MIN_ALIGN;

	return align;
}

/**
 * Write potentially 64-bit register
 *
 * @v xhci		xHCI device
 * @v value		Value
 * @v reg		Register address
 * @ret rc		Return status code
 */
static inline int xhci_writeq ( struct xhci_host *xhci, uintptr_t value, void *reg ) {

	/* If this is a 32-bit build, then this can never fail
	 * (allowing the compiler to optimise out the error path).
	 */
	if ( sizeof ( value ) <= sizeof ( uint32_t ) ) {
		writel ( value, reg );
		writel ( 0, ( reg + sizeof ( uint32_t ) ) );
		return 0;
	}

	/* If the device does not support 64-bit addresses and this
	 * address is outside the 32-bit address space, then fail.
	 */
	if ( ( value & ~0xffffffffULL ) && ! xhci->addr64 ) {
		USB_LOG_DBG("XHCI %s cannot access address %lx\n",
		       xhci->name, value );
		return -ENOTSUP;
	}

	/* If this is a 64-bit build, then writeq() is available */
	writeq ( value, reg );
	return 0;
}

/**
 * Initialise device
 *
 * @v xhci		xHCI device
 * @v regs		MMIO registers
 */
static void xhci_init ( struct xhci_host *xhci, void *regs ) {
	uint32_t hcsparams1;
	uint32_t hcsparams2;
	uint32_t hccparams1;
	uint32_t pagesize;
	size_t caplength;
	size_t rtsoff;
	size_t dboff;

	/* Locate capability, operational, runtime, and doorbell registers */
	xhci->cap = regs;
	caplength = readb ( xhci->cap + XHCI_CAP_CAPLENGTH );
	rtsoff = readl ( xhci->cap + XHCI_CAP_RTSOFF );
	dboff = readl ( xhci->cap + XHCI_CAP_DBOFF );
	xhci->op = ( xhci->cap + caplength );
	xhci->run = ( xhci->cap + rtsoff );
	xhci->db = ( xhci->cap + dboff );

    /* avoid access XHCI_REG_CAP_HCIVERSION = 0x2, unaligned memory  */
    xhci->version = ((readl ( xhci->cap + XHCI_CAP_CAPLENGTH ) >> 16) & 0xffff);

	USB_LOG_DBG("XHCI %s version %x cap %08lx op %08lx run %08lx db %08lx\n",
		xhci->name, xhci->version, ( xhci->cap ),
		 ( xhci->op ),  ( xhci->run ),
		 ( xhci->db ) );

    if (xhci->version < 0x96 || xhci->version > 0x120) {
        USB_LOG_WRN("XHCI %s not support.\n", xhci->name);
    }

	/* Read structural parameters 1 */
	hcsparams1 = readl ( xhci->cap + XHCI_CAP_HCSPARAMS1 );
	xhci->slots = XHCI_HCSPARAMS1_SLOTS ( hcsparams1 );
	xhci->intrs = XHCI_HCSPARAMS1_INTRS ( hcsparams1 );
	xhci->ports = XHCI_HCSPARAMS1_PORTS ( hcsparams1 );
	USB_LOG_DBG("XHCI %s has %d slots %d intrs %d ports\n",
	       xhci->name, xhci->slots, xhci->intrs, xhci->ports );

	/* Read structural parameters 2 */
	hcsparams2 = readl ( xhci->cap + XHCI_CAP_HCSPARAMS2 );
	xhci->scratch.count = XHCI_HCSPARAMS2_SCRATCHPADS ( hcsparams2 );
	USB_LOG_DBG("XHCI %s needs %d scratchpads\n",
		xhci->name, xhci->scratch.count );

	/* Read capability parameters 1 */
	hccparams1 = readl ( xhci->cap + XHCI_CAP_HCCPARAMS1 );
	xhci->addr64 = XHCI_HCCPARAMS1_ADDR64 ( hccparams1 );
	xhci->csz_shift = XHCI_HCCPARAMS1_CSZ_SHIFT ( hccparams1 );
	xhci->xecp = (XHCI_HCCPARAMS1_XECP ( hccparams1 ));
	USB_LOG_DBG("XHCI %s context %d bit\n",
		xhci->name, (xhci->addr64 ? 64 : 32) );

	/* Read page size */
	pagesize = readl ( xhci->op + XHCI_OP_PAGESIZE );
	xhci->pagesize = XHCI_PAGESIZE ( pagesize );
	USB_ASSERT ( xhci->pagesize != 0 );
	USB_ASSERT ( ( ( xhci->pagesize ) & ( xhci->pagesize - 1 ) ) == 0 );
	USB_LOG_DBG("XHCI %s page size %zd bytes\n",
		xhci->name, xhci->pagesize );
}

/**
 * Find extended capability
 *
 * @v xhci		xHCI device
 * @v id		Capability ID
 * @v offset	Offset to previous extended capability instance, or zero
 * @ret offset	Offset to extended capability, or zero if not found
 */
static unsigned int xhci_extended_capability ( struct xhci_host *xhci,
					       					   unsigned int id,
					       					   unsigned int offset ) {
	uint32_t xecp;
	unsigned int next;

	/* Locate the extended capability */
	while ( 1 ) {

		/* Locate first or next capability as applicable */
		if ( offset ) {
			xecp = readl ( xhci->cap + offset );
			next = XHCI_XECP_NEXT ( xecp );
		} else {
			next = xhci->xecp;
		}
		if ( ! next )
			return 0;
		offset += next;

		/* Check if this is the requested capability */
		xecp = readl ( xhci->cap + offset );
		if ( XHCI_XECP_ID ( xecp ) == id )
			return offset;
	}
}

/**
 * Initialise USB legacy support
 *
 * @v xhci		xHCI device
 */
static void xhci_legacy_init ( struct xhci_host *xhci ) {
	unsigned int legacy;
	uint8_t bios;

	/* Locate USB legacy support capability (if present) */
	legacy = xhci_extended_capability ( xhci, XHCI_XECP_ID_LEGACY, 0 );
	if ( ! legacy ) {
		/* Not an error; capability may not be present */
		USB_LOG_DBG("XHCI %s has no USB legacy support capability\n",
		       xhci->name );
		return;
	}

	/* Check if legacy USB support is enabled */
	USB_LOG_DBG("XHCI %s bios offset 0x%x\n", xhci->name, (xhci->cap + legacy + XHCI_USBLEGSUP_BIOS));
	/* bios = readb ( xhci->cap + legacy + XHCI_USBLEGSUP_BIOS ); cannot access offset 0x2, work around */
	bios = readl ( xhci->cap + legacy );
	bios = (bios >> 16) & 0xffff;
	if ( ! ( bios & XHCI_USBLEGSUP_BIOS_OWNED ) ) {
		/* Not an error; already owned by OS */
		USB_LOG_DBG("XHCI %s USB legacy support already disabled\n",
		       xhci->name );
		return;
	}

	/* Record presence of USB legacy support capability */
	xhci->legacy = legacy;
}

/**
 * Claim ownership from BIOS
 *
 * @v xhci		xHCI device
 */
static void xhci_legacy_claim ( struct xhci_host *xhci ) {
	uint32_t ctlsts;
	uint8_t bios;
	unsigned int i;

	/* Do nothing unless legacy support capability is present */
	if ( ! xhci->legacy )
		return;

	/* Claim ownership */
	writeb ( XHCI_USBLEGSUP_OS_OWNED,
		 xhci->cap + xhci->legacy + XHCI_USBLEGSUP_OS );

	/* Wait for BIOS to release ownership */
	for ( i = 0 ; i < XHCI_USBLEGSUP_MAX_WAIT_MS ; i++ ) {

		/* Check if BIOS has released ownership */
		bios = readb ( xhci->cap + xhci->legacy + XHCI_USBLEGSUP_BIOS );
		if ( ! ( bios & XHCI_USBLEGSUP_BIOS_OWNED ) ) {
			USB_LOG_DBG("XHCI %s claimed ownership from BIOS\n",
			       xhci->name );
			ctlsts = readl ( xhci->cap + xhci->legacy +
					 XHCI_USBLEGSUP_CTLSTS );
			if ( ctlsts ) {
				USB_LOG_DBG("XHCI %s warning: BIOS retained "
				       "SMIs: %08x\n", xhci->name, ctlsts );
			}
			return;
		}

		/* Delay */
		usb_osal_msleep ( 1 );
	}

	/* BIOS did not release ownership.  Claim it forcibly by
	 * disabling all SMIs.
	 */
	USB_LOG_DBG("XHCI %s could not claim ownership from BIOS: forcibly "
	       "disabling SMIs\n", xhci->name );
	writel ( 0, xhci->cap + xhci->legacy + XHCI_USBLEGSUP_CTLSTS );
}

/** Prevent the release of ownership back to BIOS */
static int xhci_legacy_prevent_release;

/**
 * Release ownership back to BIOS
 *
 * @v xhci		xHCI device
 */
static void xhci_legacy_release ( struct xhci_host *xhci ) {

	/* Do nothing unless legacy support capability is present */
	if ( ! xhci->legacy )
		return;

	/* Do nothing if releasing ownership is prevented */
	if ( xhci_legacy_prevent_release ) {
		USB_LOG_DBG("XHCI %s not releasing ownership to BIOS\n",
		       xhci->name );
		return;
	}

	/* Release ownership */
	writeb ( 0, xhci->cap + xhci->legacy + XHCI_USBLEGSUP_OS );
	USB_LOG_DBG("XHCI %s released ownership to BIOS\n", xhci->name );
}

/**
 * Stop xHCI device
 *
 * @v xhci		xHCI device
 * @ret rc		Return status code
 */
static int xhci_stop ( struct xhci_host *xhci ) {
	uint32_t usbcmd;
	uint32_t usbsts;
	unsigned int i;

	/* Clear run/stop bit */
	usbcmd = readl ( xhci->op + XHCI_OP_USBCMD );
	usbcmd &= ~XHCI_USBCMD_RUN;
	writel ( usbcmd, xhci->op + XHCI_OP_USBCMD );

	/* Wait for device to stop */
	for ( i = 0 ; i < XHCI_STOP_MAX_WAIT_MS ; i++ ) {

		/* Check if device is stopped */
		usbsts = readl ( xhci->op + XHCI_OP_USBSTS );
		if ( usbsts & XHCI_USBSTS_HCH )
			return 0;

		/* Delay */
		usb_osal_msleep ( 1 );
	}

	USB_LOG_DBG("XHCI %s timed out waiting for stop\n", xhci->name );
	return -ETIMEDOUT;
}

/**
 * Reset xHCI device
 *
 * @v xhci		xHCI device
 * @ret rc		Return status code
 */
static int xhci_reset ( struct xhci_host *xhci ) {
	uint32_t usbcmd;
	unsigned int i;
	int rc;

	/* The xHCI specification states that resetting a running
	 * device may result in undefined behaviour, so try stopping
	 * it first.
	 */
	if ( ( rc = xhci_stop ( xhci ) ) != 0 ) {
		/* Ignore errors and attempt to reset the device anyway */
	}

	/* Reset device */
	writel ( XHCI_USBCMD_HCRST, xhci->op + XHCI_OP_USBCMD );

	/* Wait for reset to complete */
	for ( i = 0 ; i < XHCI_RESET_MAX_WAIT_MS ; i++ ) {

		/* Check if reset is complete */
		usbcmd = readl ( xhci->op + XHCI_OP_USBCMD );
		if ( ! ( usbcmd & XHCI_USBCMD_HCRST ) )
			return 0;

		/* Delay */
		usb_osal_msleep ( 1 );
	}

	USB_LOG_DBG("XHCI %s timed out waiting for reset\n", xhci->name );
	return -ETIMEDOUT;
}


/**
 * Find supported protocol extended capability for a port
 *
 * @v xhci		xHCI device
 * @v port		Port number
 * @ret supported	Offset to extended capability, or zero if not found
 */
static unsigned int xhci_supported_protocol ( struct xhci_host *xhci,
					      					  unsigned int port ) {
	unsigned int supported = 0;
	unsigned int offset;
	unsigned int count;
	uint32_t ports;

	/* Iterate over all supported protocol structures */
	while ( ( supported = xhci_extended_capability ( xhci,
							 XHCI_XECP_ID_SUPPORTED,
							 supported ) ) ) {

		/* Determine port range */
		ports = readl ( xhci->cap + supported + XHCI_SUPPORTED_PORTS );
		offset = XHCI_SUPPORTED_PORTS_OFFSET ( ports );
		count = XHCI_SUPPORTED_PORTS_COUNT ( ports );

		/* Check if port lies within this range */
		if ( ( port - offset ) < count )
			return supported;
	}

	USB_LOG_DBG("XHCI %s Port-%d has no supported protocol\n",
	       xhci->name, port );
	return 0;
}

/**
 * Transcribe port speed (for debugging)
 *
 * @v psi		Protocol speed ID
 * @ret speed		Transcribed speed
 */
static inline const char * xhci_speed_name ( uint32_t psi ) {
	static const char *exponents[4] = { "", "k", "M", "G" };
	static char buf[ 10 /* "xxxxxXbps" + NUL */ ];
	unsigned int mantissa;
	unsigned int exponent;

	/* Extract mantissa and exponent */
	mantissa = XHCI_SUPPORTED_PSI_MANTISSA ( psi );
	exponent = XHCI_SUPPORTED_PSI_EXPONENT ( psi );

	/* Transcribe speed */
	snprintf ( buf, sizeof ( buf ), "%d%sbps",
		   mantissa, exponents[exponent] );
	return buf;
}

/**
 * Find port protocol
 *
 * @v xhci		xHCI device
 * @v port		Port number
 * @ret protocol	USB protocol, or zero if not found
 */
static unsigned int xhci_port_protocol ( struct xhci_host *xhci,
					 					 unsigned int port ) {
	unsigned int supported = xhci_supported_protocol ( xhci, port );
	union {
		uint32_t raw;
		char text[5];
	} name;
	unsigned int protocol;
	unsigned int type;
	unsigned int psic;
	unsigned int psiv;
	unsigned int i;
	uint32_t revision;
	uint32_t ports;
	uint32_t slot;
	uint32_t psi;

	/* Fail if there is no supported protocol */
	if ( ! supported )
		return 0;

	/* Determine protocol version */
	revision = readl ( xhci->cap + supported + XHCI_SUPPORTED_REVISION );
	protocol = XHCI_SUPPORTED_REVISION_VER ( revision );

	/* Describe port protocol */
#if XHCI_DUMP
    {
		name.raw = CPU_TO_LE32 ( readl ( xhci->cap + supported +
						 XHCI_SUPPORTED_NAME ) );
		name.text[4] = '\0';
		slot = readl ( xhci->cap + supported + XHCI_SUPPORTED_SLOT );
		type = XHCI_SUPPORTED_SLOT_TYPE ( slot );
		USB_LOG_DBG("XHCI %s-%d %sv%04x type %d \r\n",
			xhci->name, port, name.text, protocol, type );
		ports = readl ( xhci->cap + supported + XHCI_SUPPORTED_PORTS );
		psic = XHCI_SUPPORTED_PORTS_PSIC ( ports );
		if ( psic ) {
			USB_LOG_DBG(" speeds \r\n" );
			for ( i = 0 ; i < psic ; i++ ) {
				psi = readl ( xhci->cap + supported +
					      XHCI_SUPPORTED_PSI ( i ) );
				psiv = XHCI_SUPPORTED_PSI_VALUE ( psi );
				USB_LOG_DBG(" %d:%s \r\n", psiv, xhci_speed_name ( psi ) );
			}
		}
	}
#endif

	return protocol;
}

/**
 * Probe XCHI device
 *
 * @v xhci		XCHI device
 * @v baseaddr  XHCI device register base address
 * @ret rc		Return status code
 */
int xhci_probe ( struct xhci_host *xhci, unsigned long baseaddr ) {
    USB_ASSERT(xhci);
    int error = 0;
	struct usbh_hubport *port;
	unsigned int i;

    xhci->base = (void *)baseaddr;
	xhci->name[0] = '0' + xhci->id; /* Assert there are less than 10 xhci host */
    xhci->name[1] = '\0';

	/* Initialise xHCI device */
	xhci_init ( xhci, xhci->base );

	/* Initialise USB legacy support and claim ownership */
	xhci_legacy_init ( xhci );
	xhci_legacy_claim ( xhci );

	/* Reset device */
	if ( ( error = xhci_reset ( xhci ) ) != 0 )
		goto err_reset;

	/* Set port protocols */
	for ( i = 1 ; i <= xhci->ports ; i++ ) {
		port = usbh_get_roothub_port ( i );
		port->protocol = xhci_port_protocol ( xhci, i );
	}

    return error;

err_reset:
	xhci_legacy_release ( xhci );
	return -1;
}

/*********************************************************************/

/**
 * Allocate device context base address array
 *
 * @v xhci		xHCI device
 * @ret rc		Return status code
 */
static int xhci_dcbaa_alloc ( struct xhci_host *xhci ) {
	size_t len;
	uintptr_t dcbaap;
	int rc;

	/* Allocate and initialise structure.  Must be at least
	 * 64-byte aligned and must not cross a page boundary, so
	 * align on its own size (rounded up to a power of two and
	 * with a minimum of 64 bytes).
	 */
	len = ( ( xhci->slots + 1 ) * sizeof ( xhci->dcbaa.context[0] ) );
	xhci->dcbaa.context = usb_align(xhci_align ( len ), len);
	if ( ! xhci->dcbaa.context ) {
		USB_LOG_ERR("XHCI %s could not allocate DCBAA\n", xhci->name );
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( xhci->dcbaa.context, 0, len );

	/* Program DCBAA pointer */
	dcbaap =  (uintptr_t)( xhci->dcbaa.context );
	if ( ( rc = xhci_writeq ( xhci, dcbaap,
				  xhci->op + XHCI_OP_DCBAAP ) ) != 0 )
		goto err_writeq;

	USB_LOG_DBG("XHCI %s DCBAA at [%08lx,%08lx)\n", xhci->name,
		 ( xhci->dcbaa.context ),
		(  ( xhci->dcbaa.context ) + len ) );
	return 0;

 err_writeq:
	usb_free ( xhci->dcbaa.context );
 err_alloc:
	return rc;
}


/**
 * Allocate scratchpad buffers
 *
 * @v xhci		xHCI device
 * @ret rc		Return status code
 */
static int xhci_scratchpad_alloc ( struct xhci_host *xhci ) {
	struct xhci_scratchpad *scratch = &xhci->scratch;
	size_t buffer_len;
	size_t array_len;
	uintptr_t addr;
	unsigned int i;
	int rc;

	/* Do nothing if no scratchpad buffers are used */
	if ( ! scratch->count ) {
		USB_LOG_INFO("XHCI %s no need to allocate scratchpad buffers\n",
		       xhci->name );
		return 0;
    }

	/* Allocate scratchpad buffers */
	buffer_len = ( scratch->count * xhci->pagesize );
	scratch->buffer = (uintptr_t)usb_align ( xhci->pagesize, buffer_len );
	if ( ! scratch->buffer ) {
		USB_LOG_ERR("XHCI %s could not allocate scratchpad buffers\n",
		       xhci->name );
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( (void *)scratch->buffer, 0, buffer_len );

	/* Allocate scratchpad array */
	array_len = ( scratch->count * sizeof ( scratch->array[0] ) );
	scratch->array = usb_align(xhci_align ( array_len ), array_len);
	if ( ! scratch->array ) {
		USB_LOG_ERR("XHCI %s could not allocate scratchpad buffer "
		       "array\n", xhci->name );
		rc = -ENOMEM;
		goto err_alloc_array;
	}

	/* Populate scratchpad array */
	addr = ( scratch->buffer + 0 );
	for ( i = 0 ; i < scratch->count ; i++ ) {
		scratch->array[i] = CPU_TO_LE64 ( addr );
		addr += xhci->pagesize;
	}

	/* Set scratchpad array pointer */
	USB_ASSERT ( xhci->dcbaa.context != NULL );
	xhci->dcbaa.context[0] = CPU_TO_LE64 ( ( scratch->array ) );

	USB_LOG_DBG("XHCI %s scratchpad [%08lx,%08lx) array [%08lx,%08lx)\n",
		xhci->name,  ( scratch->buffer, 0 ),
		 ( scratch->buffer, buffer_len ),
		 ( scratch->array ),
		(  ( scratch->array ) + array_len ) );
	return 0;

	usb_free ( scratch->array );
 err_alloc_array:
	usb_free ( scratch->buffer );
 err_alloc:
	return rc;
}

/**
 * Allocate command ring
 *
 * @v xhci		xHCI device
 * @ret rc		Return status code
 */
static int xhci_command_alloc ( struct xhci_host *xhci ) {
	uintptr_t crp;
	int rc;

    /* Allocate TRB ring */
    xhci->cmds = usb_align(XHCI_RING_SIZE, sizeof(*xhci->cmds)); /* command ring */
    if (! xhci->cmds)
        goto err_ring_alloc;

    memset(xhci->cmds, 0U, sizeof(*xhci->cmds));

    xhci->cmds->lock = usb_osal_mutex_create();
    USB_ASSERT(xhci->cmds->lock);

    xhci->cmds->cs = 1; /* cycle state = 1 */

	/* Program command ring control register */
	crp = (uintptr_t)( xhci->cmds );
	if ( ( rc = xhci_writeq ( xhci, ( crp | XHCI_CRCR_RCS ),
				  xhci->op + XHCI_OP_CRCR ) ) != 0 )
		goto err_writeq;

	USB_LOG_DBG("XHCI %s CRCR at [%08lx,%08lx)\n", xhci->name,
		 ( xhci->cmds->ring ),
		 ( ( xhci->cmds->ring ) + sizeof(xhci->cmds->ring) ) );
	return 0;

 err_writeq:
	usb_free(xhci->cmds);;
 err_ring_alloc:
	return rc;
}

/**
 * Allocate event ring
 *
 * @v xhci		xHCI device
 * @ret rc		Return status code
 */
static int xhci_event_alloc ( struct xhci_host *xhci ) {
	unsigned int count;
	size_t len;
	int rc;

	/* Allocate event ring */
	xhci->evts = usb_align(XHCI_RING_SIZE, sizeof(*xhci->evts)); /* event ring */
	if ( ! xhci->evts ) {
		rc = -ENOMEM;
		goto err_alloc_trb;
	}

	memset(xhci->evts, 0U, sizeof(*xhci->evts));

	/* Allocate event ring segment table */
	xhci->eseg = usb_align(XHCI_ALIGMENT, sizeof(*xhci->eseg)); /* event segment */
	if ( ! xhci->eseg ) {
		rc = -ENOMEM;
		goto err_alloc_segment;
	}
	memset(xhci->eseg, 0U, sizeof(*xhci->eseg));
    xhci->eseg->base = CPU_TO_LE64 ( ( xhci->evts ) );
    xhci->eseg->count = XHCI_RING_ITEMS; /* items of event ring TRB */

	/* Program event ring registers */
	writel ( 1, xhci->run + XHCI_RUN_ERSTSZ ( 0 ) ); /* bit[15:0] event ring segment table size */
	if ( ( rc = xhci_writeq ( xhci, (uintptr_t)( xhci->evts ),
				  xhci->run + XHCI_RUN_ERDP ( 0 ) ) ) != 0 ) /* bit[63:4] event ring dequeue pointer */
		goto err_writeq_erdp;
	if ( ( rc = xhci_writeq ( xhci, (uintptr_t)( xhci->eseg ),
				  xhci->run + XHCI_RUN_ERSTBA ( 0 ) ) ) != 0 ) /* bit[63:6] event ring segment table base addr */
		goto err_writeq_erstba;

	xhci->evts->cs = 1; /* cycle state = 1 */
	USB_LOG_DBG("XHCI %s event ring [%08lx,%08lx) table [%08lx,%08lx)\n",
		xhci->name,  ( xhci->evts->ring ),
		(  ( xhci->evts->ring ) + sizeof(xhci->evts->ring) ),
		 ( xhci->eseg ),
		(  ( xhci->eseg ) +
		  sizeof ( xhci->eseg[0] ) ) );
	return 0;

	xhci_writeq ( xhci, 0, xhci->run + XHCI_RUN_ERSTBA ( 0 ) );
 err_writeq_erstba:
	xhci_writeq ( xhci, 0, xhci->run + XHCI_RUN_ERDP ( 0 ) );
 err_writeq_erdp:
	usb_free ( xhci->eseg );
 err_alloc_segment:
	usb_free ( xhci->evts );
 err_alloc_trb:
	return rc;
}

/**
 * Start xHCI device
 *
 * @v xhci		xHCI device
 */
static void xhci_run ( struct xhci_host *xhci ) {
	uint32_t config;
	uint32_t usbcmd;
	uint32_t runtime;

	/* Configure number of device slots */
	config = readl ( xhci->op + XHCI_OP_CONFIG );
	config &= ~XHCI_CONFIG_MAX_SLOTS_EN_MASK;
	config |= XHCI_CONFIG_MAX_SLOTS_EN ( xhci->slots );
	writel ( config, xhci->op + XHCI_OP_CONFIG );

	/* Enable port interrupt */
	writel ( 500U, xhci->run + XHCI_RUN_IR_IMOD ( 0 ) );
	runtime = readl(xhci->run + XHCI_RUN_IR_IMAN ( 0 ));
	runtime |= XHCI_RUN_IR_IMAN_IE;
	writel (runtime, xhci->run + XHCI_RUN_IR_IMAN ( 0 ));

	/* Set run/stop bit and enable interrupt */
	usbcmd = readl ( xhci->op + XHCI_OP_USBCMD );
	usbcmd |= XHCI_USBCMD_RUN | XHCI_USBCMD_INTE;
	writel ( usbcmd, xhci->op + XHCI_OP_USBCMD );

	USB_LOG_DBG("XHCI %s start running\n", xhci->name );
}

/**
 * Free event ring
 *
 * @v xhci		xHCI device
 */
static void xhci_event_free ( struct xhci_host *xhci ) {

    /* Clear event ring registers */
	writel ( 0, xhci->run + XHCI_RUN_ERSTSZ ( 0 ) );
	xhci_writeq ( xhci, 0, xhci->run + XHCI_RUN_ERSTBA ( 0 ) );
	xhci_writeq ( xhci, 0, xhci->run + XHCI_RUN_ERDP ( 0 ) );

	/* Free event ring segment table */
	usb_free ( xhci->eseg );

	/* Free event ring */
	usb_free ( xhci->evts );
}

/**
 * Free command ring
 *
 * @v xhci		xHCI device
 */
static void xhci_command_free ( struct xhci_host *xhci ) {

	/* Sanity check */
	USB_ASSERT ( ( readl ( xhci->op + XHCI_OP_CRCR ) & XHCI_CRCR_CRR ) == 0 );

	/* Clear command ring control register */
	xhci_writeq ( xhci, 0, xhci->op + XHCI_OP_CRCR );

	/* Free TRB ring */
	usb_free ( xhci->cmds );
}

/**
 * Free scratchpad buffers
 *
 * @v xhci		xHCI device
 */
static void xhci_scratchpad_free ( struct xhci_host *xhci ) {
	struct xhci_scratchpad *scratch = &xhci->scratch;
	size_t array_len;
	size_t buffer_len;

	/* Do nothing if no scratchpad buffers are used */
	if ( ! scratch->count )
		return;

	/* Clear scratchpad array pointer */
	USB_ASSERT ( xhci->dcbaa.context != NULL );
	xhci->dcbaa.context[0] = 0;

	/* Free scratchpad array */
	array_len = ( scratch->count * sizeof ( scratch->array[0] ) );
	usb_free ( scratch->array );

	/* Free scratchpad buffers */
	buffer_len = ( scratch->count * xhci->pagesize );
	usb_free ( scratch->buffer );
}

/**
 * Free device context base address array
 *
 * @v xhci		xHCI device
 */
static void xhci_dcbaa_free ( struct xhci_host *xhci ) {
	size_t len;
	unsigned int i;

	/* Sanity check */
	for ( i = 0 ; i <= xhci->slots ; i++ )
		USB_ASSERT ( xhci->dcbaa.context[i] == 0 );

	/* Clear DCBAA pointer */
	xhci_writeq ( xhci, 0, xhci->op + XHCI_OP_DCBAAP );

	/* Free DCBAA */
	len = ( ( xhci->slots + 1 ) * sizeof ( xhci->dcbaa.context[0] ) );
	usb_free ( xhci->dcbaa.context );
}

/**
 * Open XHCI device
 *
 * @v xhci		XHCI device
 * @ret rc		Return status code
 */
int xhci_open ( struct xhci_host *xhci ) {
	int rc;

	/* Allocate device slot array */
	xhci->slot = usb_malloc ( ( xhci->slots + 1 ) * sizeof ( xhci->slot[0] ) );
	if ( ! xhci->slot ) {
		rc = -ENOMEM;
		goto err_slot_alloc;
	}

	/* Allocate device context base address array */
	if ( ( rc = xhci_dcbaa_alloc ( xhci ) ) != 0 )
		goto err_dcbaa_alloc;

	/* Allocate scratchpad buffers */
	if ( ( rc = xhci_scratchpad_alloc ( xhci ) ) != 0 )
		goto err_scratchpad_alloc;

	/* Allocate command ring */
	if ( ( rc = xhci_command_alloc ( xhci ) ) != 0 )
		goto err_command_alloc;

    /* Allocate event ring */
	if ( ( rc = xhci_event_alloc ( xhci ) ) != 0 )
		goto err_event_alloc;

	/* Start controller */
	xhci_run ( xhci );

	return 0;

	xhci_stop ( xhci );
	xhci_event_free ( xhci );
 err_event_alloc:
	xhci_command_free ( xhci );
 err_command_alloc:
	xhci_scratchpad_free ( xhci );
 err_scratchpad_alloc:
	xhci_dcbaa_free ( xhci );
 err_dcbaa_alloc:
	usb_free ( xhci->slot );
 err_slot_alloc:
	return rc;

}

/**
 * Close XHCI device
 *
 * @v xhci		XHCI Device
 */
void xhci_close ( struct xhci_host *xhci ) {
	unsigned int i;

	/* Sanity checks */
	USB_ASSERT ( xhci->slot != NULL );
	for ( i = 0 ; i <= xhci->slots ; i++ )
		USB_ASSERT ( xhci->slot[i] == NULL );

	xhci_stop ( xhci );
	usb_free (xhci->evts);
	usb_free (xhci->eseg);
	usb_free (xhci->cmds);
	xhci_scratchpad_free ( xhci );
	xhci_dcbaa_free ( xhci );
	usb_free ( xhci->slot );
}

/**
 * Remove XHCI device
 *
 * @v xhci		XHCI device
 */
void xhci_remove ( struct xhci_host *xhci ) {
	xhci_reset ( xhci );
	xhci_legacy_release ( xhci );
	usb_free ( xhci );

	/* If we are shutting down to boot an OS, then prevent the
	 * release of ownership back to BIOS.
	 */
	xhci_legacy_prevent_release = 0;
}

/*********************************************************************/

/**
 * Enable port
 *
 * @v xhci		XHCI device
 * @v port		Port number
 * @ret rc		Return status code
 */
int xhci_port_enable(struct xhci_host *xhci, uint32_t port) {
	uint32_t portsc = readl ( xhci->op + XHCI_OP_PORTSC ( port ) );

	/* double check if connected */
	if (!(portsc & XHCI_PORTSC_CCS)) {
        USB_LOG_ERR("device connectiong lost !!! \r\n");
        return -ENOENT;
	}

	switch ( portsc & XHCI_PORTSC_PLS_MASK )
	{
	case XHCI_PORTSC_PLS_U0:
		/* A USB3 port - controller automatically performs reset */
		break;
	case XHCI_PORTSC_PLS_POLLING:
		/* A USB2 port - perform device reset */
		xhci_dump_port_status(port, portsc);
		writel ((portsc | XHCI_PORTSC_PR), (xhci->op + XHCI_OP_PORTSC ( port )));  /* reset port */
		break;
	default:
		USB_LOG_ERR("PLS: %d \r\n", (portsc & XHCI_PORTSC_PLS_MASK));
		return -ENOENT;
	}

	/* Wait for device to complete reset and be enabled */
	uint32_t end = 1000U, start = 0U;
	for (;;) {
		portsc = readl ( xhci->op + XHCI_OP_PORTSC ( port ) );
		if (!(portsc & XHCI_PORTSC_CCS)) {
            /* USB 2.0 port would be disconnected after reset */
            USB_LOG_INFO("USB 2.0 port disconnected during reset, need rescan \r\n");
            return 0;
		}

        if (portsc & XHCI_PORTSC_PED) { /* check if port enabled */
			/* Reset complete */
            break;
        }

        if (++start > end) {
            USB_LOG_ERR("Wait timeout, portsc=0x%x !!!\n", portsc);
            return -ENXIO;
        }

        usb_osal_msleep(1);
	}

	xhci_dump_port_status(port, portsc);
	return 0;
}

/**
 * Convert USB Speed to PSI
 *
 * @v speed		USB speed
 * @ret psi		USB speed in PSI
 */
static unsigned int xhci_speed_to_psi(int speed) {
	unsigned int psi = USB_SPEED_UNKNOWN;

	switch (speed) {
		case USB_SPEED_LOW:
			psi = XCHI_PSI_LOW;
			break;
		case USB_SPEED_FULL:
			psi = XCHI_PSI_FULL;
			break;
		case USB_SPEED_HIGH:
			psi = XCHI_PSI_HIGH;
			break;
		case USB_SPEED_SUPER:
			psi = XCHI_PSI_SUPER;
			break;
	}

	return psi;
}

/**
 * Convert USB PSI to Speed
 *
 * @v psi		USB speed in PSI
 * @ret speed	USB speed
 */
static int xhci_psi_to_speed(int psi) {
	int speed = USB_SPEED_UNKNOWN;

	switch (psi) {
		case XCHI_PSI_LOW:
			speed = USB_SPEED_LOW;
			break;
		case XCHI_PSI_FULL:
			speed = USB_SPEED_FULL;
			break;
		case XCHI_PSI_HIGH:
			speed = USB_SPEED_HIGH;
			break;
		case XCHI_PSI_SUPER:
			speed = USB_SPEED_SUPER;
			break;
	}

	return speed;
}

/**
 * Find port speed
 *
 * @v xhci		xHCI device
 * @v port		Port number
 * @v psiv		Protocol speed ID value
 * @ret speed		Port speed, or negative error
 */
static int xhci_port_speed ( struct xhci_host *xhci, unsigned int port,
			     			 unsigned int psiv ) {
	unsigned int supported = xhci_supported_protocol ( xhci, port );
	unsigned int psic;
	unsigned int mantissa;
	unsigned int exponent;
	unsigned int speed;
	unsigned int i;
	uint32_t ports;
	uint32_t psi;

	/* Fail if there is no supported protocol */
	if ( ! supported )
		return -ENOTSUP;

	/* Get protocol speed ID count */
	ports = readl ( xhci->cap + supported + XHCI_SUPPORTED_PORTS );
	psic = XHCI_SUPPORTED_PORTS_PSIC ( ports );

	/* Use protocol speed ID table unless device is known to be faulty */
	/* Iterate over PSI dwords looking for a match */
	for ( i = 0 ; i < psic ; i++ ) {
		psi = readl ( xhci->cap + supported +
					XHCI_SUPPORTED_PSI ( i ) );
		if ( psiv == XHCI_SUPPORTED_PSI_VALUE ( psi ) ) {
			mantissa = XHCI_SUPPORTED_PSI_MANTISSA ( psi );
			exponent = XHCI_SUPPORTED_PSI_EXPONENT ( psi );
			return xhci_psi_to_speed(XCHI_PSI ( mantissa, exponent ));
		}
	}

	/* Record device as faulty if no match is found */
	if ( psic != 0 ) {
		USB_LOG_ERR("XHCI %s-%d spurious PSI value %d: "
				"assuming PSI table is invalid\n",
				xhci->name, port, psiv );
	}

	/* Use the default mappings */
	switch ( psiv ) {
	case XHCI_SPEED_LOW :	return USB_SPEED_LOW;
	case XHCI_SPEED_FULL :	return USB_SPEED_FULL;
	case XHCI_SPEED_HIGH :	return USB_SPEED_HIGH;
	case XHCI_SPEED_SUPER :	return USB_SPEED_SUPER;
	default:
		USB_LOG_ERR("XHCI %s-%d unrecognised PSI value %d\n",
		       xhci->name, port, psiv );
		return -ENOTSUP;
	}
}

/**
 * Find protocol speed ID value
 *
 * @v xhci		xHCI device
 * @v port		Port number
 * @v speed		USB speed
 * @ret psiv		Protocol speed ID value, or negative error
 */
static int xhci_port_psiv ( struct xhci_host *xhci, unsigned int port,
			    			unsigned int speed ) {
	unsigned int supported = xhci_supported_protocol ( xhci, port );
	unsigned int psic;
	unsigned int mantissa;
	unsigned int exponent;
	unsigned int psiv;
	unsigned int i;
	uint32_t ports;
	uint32_t psi;

	/* Fail if there is no supported protocol */
	if ( ! supported )
		return -ENOTSUP;

	/* Get protocol speed ID count */
	ports = readl ( xhci->cap + supported + XHCI_SUPPORTED_PORTS );
	psic = XHCI_SUPPORTED_PORTS_PSIC ( ports );

	/* Use the default mappings if applicable */
	if ( psic == 0  ) {
		switch ( speed ) {
		case USB_SPEED_LOW :	return XHCI_SPEED_LOW;
		case USB_SPEED_FULL :	return XHCI_SPEED_FULL;
		case USB_SPEED_HIGH :	return XHCI_SPEED_HIGH;
		case USB_SPEED_SUPER :	return XHCI_SPEED_SUPER;
		default:
			USB_LOG_DBG("XHCI %s-%d non-standard speed %d\n",
			       xhci->name, port, speed );
			return -ENOTSUP;
		}
	}

	/* Iterate over PSI dwords looking for a match */
	for ( i = 0 ; i < psic ; i++ ) {
		psi = readl ( xhci->cap + supported + XHCI_SUPPORTED_PSI ( i ));
		mantissa = XHCI_SUPPORTED_PSI_MANTISSA ( psi );
		exponent = XHCI_SUPPORTED_PSI_EXPONENT ( psi );
		if ( xhci_speed_to_psi(speed) == XCHI_PSI ( mantissa, exponent ) ) {
			psiv = XHCI_SUPPORTED_PSI_VALUE ( psi );
			return psiv;
		}
	}

	USB_LOG_DBG("XHCI %s-%d unrepresentable speed %#x\n",
	       xhci->name, port, speed );
	return -ENOENT;
}

/**
 * Update root hub port speed
 *
 * @v xhci		XHCI device
 * @v port		Port number
 * @ret rc		Return status code (speed)
 */
uint32_t xhci_root_speed ( struct xhci_host *xhci, uint8_t port ) {
	uint32_t portsc;
	unsigned int psiv;
	int ccs;
	int ped;
	int csc;
	int speed;
	unsigned int protocol = xhci_port_protocol(xhci, port);

	/* Read port status */
	portsc = readl ( xhci->op + XHCI_OP_PORTSC ( port ) );
	USB_LOG_DBG("XHCI %s-%d status is 0x%08x, protocol 0x%x\n",
		xhci->name, port, portsc, protocol );
	ccs = ( portsc & XHCI_PORTSC_CCS );
	ped = ( portsc & XHCI_PORTSC_PED );
	csc = ( portsc & XHCI_PORTSC_CSC );
	psiv = XHCI_PORTSC_PSIV ( portsc );

	USB_LOG_DBG("XHCI %s port-%d ccs: %d, ped: %d, csc: %d, psiv: 0x%x\n",
				 xhci->name, port, !!ccs, !!ped, !!csc, psiv);

	/* Port speed is not valid unless port is connected */
	if ( ! ccs ) {
		speed = USB_SPEED_UNKNOWN;
		USB_LOG_ERR("XHCI %s port-%d speed unkown\n", xhci->name, port);
		return speed;
	}

	/* For USB2 ports, the PSIV field is not valid until the port
	 * completes reset and becomes enabled.
	 */
	if ( ( protocol < USB_3_0 ) && ! ped ) {
		speed = USB_SPEED_FULL;
		return speed;
	}

	/* Get port speed and map to generic USB speed */
	speed = xhci_port_speed ( xhci, port, psiv );
	if ( speed < 0 ) {
		return speed;
	}

	return speed;
}

/*********************************************************************/
/**
 * Add a TRB to the given ring
 *
 * @v ring		XHCI TRB ring
 * @v trb		TRB content to be added
 */
static inline void xhci_trb_fill(struct xhci_ring *ring, union xhci_trb *trb) {
	union xhci_trb *dst = &ring->ring[ring->nidx];
	memcpy((void *)dst, (void *)trb, sizeof(*trb));
	dst->template.control |= (ring->cs ? XHCI_TRB_C : 0);
	xhci_dump_trbs(dst, 1);
}

/**
 * Queue a TRB onto a ring, wrapping ring as needed
 *
 * @v ring		XHCI TRB ring
 * @v trb		TRB content to be added
 */
static void xhci_trb_queue(struct xhci_ring *ring, union xhci_trb *trb) {

	if (ring->nidx >= XHCI_RING_ITEMS - 1) {
		/* if it is the last trb in the list, put a link trb in the end */
		union xhci_trb link_trb;
		link_trb.link.type = XHCI_TRB_LINK;
		link_trb.link.flags = XHCI_TRB_TC;
		link_trb.link.next = CPU_TO_LE64((ring->ring));

		xhci_trb_fill(ring, &link_trb);

		ring->nidx = 0; /* adjust dequeue index to 0 */
		ring->cs ^= 1; /* toggle cycle interpretation of sw */
	}

	xhci_trb_fill(ring, trb);
	ring->nidx++; /* ahead dequeue index */
}

/**
 * Wait for a ring to empty (all TRBs processed by hardware)
 *
 * @v xhci      XHCI Device
 * @v ep		Owner Endpoint of current TRB ring
 * @  ring		XHCI TRB ring
 */
int xhci_event_wait(struct xhci_host *xhci,
					struct xhci_endpoint *ep,
					struct xhci_ring *ring) {
    int cc = XHCI_CMPLT_SUCCESS;

    if (ep->timeout > 0)
    {
        if (usb_osal_sem_take(ep->waitsem, ep->timeout) < 0)
        {
            cc = XHCI_CMPLT_TIMEOUT;
        }
        else
        {
            cc = ring->evt.complete.code; /* bit[31:24] completion code */
        }
    }

    return cc;
}

/**
 * Ring doorbell register
 *
 * @v xhci		XHCI device
 * @v slotid	Slot id to ring
 * @v value     Value send to doorbell
 */
static inline void xhci_doorbell ( struct xhci_host *xhci, uint32_t slotid, uint32_t value ) {

	DSB();
	writel ( value, xhci->db + slotid * XHCI_REG_DB_SIZE ); /* bit[7:0] db target, is ep_id */
}

/**
 * Abort command
 *
 * @v xhci		xHCI device
 */
static void xhci_abort ( struct xhci_host *xhci ) {
	uintptr_t crp;

	/* Abort the command */
	USB_LOG_WRN("XHCI %s aborting command\n", xhci->name );
	xhci_writeq ( xhci, XHCI_CRCR_CA, xhci->op + XHCI_OP_CRCR );

	/* Allow time for command to abort */
	usb_osal_msleep ( XHCI_COMMAND_ABORT_DELAY_MS );

	/* Sanity check */
	USB_ASSERT ( ( readl ( xhci->op + XHCI_OP_CRCR ) & XHCI_CRCR_CRR ) == 0 );

	/* Consume (and ignore) any final command status */
	int cc = xhci_event_wait(xhci, xhci->cur_cmd_pipe, xhci->cmds);
	if (XHCI_CMPLT_SUCCESS != cc) {
		USB_LOG_ERR("XHCI %s abort command failed, cc %d\n", xhci->name, cc);
	}

 	/* Reset the command ring control register */
    memset(xhci->cmds->ring, 0U, XHCI_RING_ITEMS * sizeof(union xhci_trb));
	xhci_writeq ( xhci, ( (uint64_t)(uintptr_t)xhci->cmds | xhci->cmds->cs ), xhci->op + XHCI_OP_CRCR );
}

/**
 * Submit a command to the xhci controller ring
 *
 * @v xhci      XHCI Device
 * @v ep		Owner Endpoint of current TRB ring
 * @  trb		Command TRB to be sent
 */
static int xhci_cmd_submit(struct xhci_host *xhci, struct xhci_endpoint *ep, union xhci_trb *trb) {

	int rc = 0;
	usb_osal_mutex_take(xhci->cmds->lock); /* handle command one by one */

    ep->timeout = 5000;
    ep->waiter = true;
	xhci->cur_cmd_pipe = ep;

	xhci_trb_queue(xhci->cmds, trb);

    /* pass command trb to hardware */
    DSB();

    xhci_doorbell(xhci, 0, 0); /* 0 = db host controller, 0 = db targe hc command */
	int cc = xhci_event_wait(xhci, ep, xhci->cmds);
	if (XHCI_CMPLT_SUCCESS != cc) {
		USB_LOG_ERR("XHCI %s cmd failed, cc %d\n", xhci->name, cc);
		xhci_abort(xhci); /* Abort command */
		rc = -ENOTSUP;
	}

    usb_osal_mutex_give(xhci->cmds->lock);
	xhci->cur_cmd_pipe = NULL;
    return rc;
}

/**
 * Issue NOP and wait for completion
 *
 * @v xhci		xHCI device
 * @v ep		Command Endpoint
 * @ret rc		Return status code
 */
static int xhci_nop ( struct xhci_host *xhci, struct xhci_endpoint *ep ) {
	union xhci_trb trb;
	struct xhci_trb_common *nop = &trb.common;
	int rc;

	/* Construct command */
	memset ( nop, 0, sizeof ( *nop ) );
	nop->flags = XHCI_TRB_IOC;
	nop->type = XHCI_TRB_NOP_CMD;

	/* Issue command and wait for completion */
	if ( ( rc = xhci_cmd_submit(xhci, ep, &trb ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Issue Enable slot and wait for completion
 *
 * @v xhci		xHCI device
 * @v ep		Command Endpoint
 * @v type      Type of Slot to be enabled
 * @ret rc		Return status code
 */
static int xhci_enable_slot(struct xhci_host *xhci, struct xhci_endpoint *ep, unsigned int type) {
	union xhci_trb trb;
	struct xhci_trb_enable_slot *enable = &trb.enable;
	struct xhci_trb_complete *enabled;
	unsigned int slot;
	int rc;

	/* Construct command */
	memset ( enable, 0, sizeof ( *enable ) );
	enable->slot = type;
	enable->type = XHCI_TRB_ENABLE_SLOT;

	/* Issue command and Wait for completion */
	if ( ( rc = xhci_cmd_submit(xhci, ep, &trb) ) != 0 ) {
		USB_LOG_ERR("XHCI %s could not enable new slot, type %d\n",
		       xhci->name, type );
		return rc;
	}

	/* Extract slot number */
	enabled = &(xhci->cmds->evt.complete);
	slot = enabled->slot;

	USB_LOG_DBG("XHCI %s slot %d enabled\n", xhci->name, slot );
	return slot;
}

/**
 * Disable slot
 *
 * @v xhci		xHCI device
 * @v ep		Command Endpoint
 * @v slot		Device slot
 * @ret rc		Return status code
 */
static int xhci_disable_slot ( struct xhci_host *xhci, struct xhci_endpoint *ep,
				      		   unsigned int slot ) {
	union xhci_trb trb;
	struct xhci_trb_disable_slot *disable = &trb.disable;
	int rc;

	/* Construct command */
	memset ( disable, 0, sizeof ( *disable ) );
	disable->type = XHCI_TRB_DISABLE_SLOT;
	disable->slot = slot;

	/* Issue command and wait for completion */
	if ( ( rc = xhci_cmd_submit ( xhci, ep, &trb ) ) != 0 ) {
		USB_LOG_DBG("XHCI %s could not disable slot %d: %s\n",
		       xhci->name, slot, strerror ( rc ) );
		return rc;
	}

	USB_LOG_DBG("XHCI %s slot %d disabled\n", xhci->name, slot );
	return 0;
}

/**
 * Issue context-based command and wait for completion
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint	Endpoint
 * @v type		TRB type
 * @v populate	Input context populater
 * @ret rc		Return status code
 */
static int xhci_context ( struct xhci_host *xhci, struct xhci_slot *slot,
						  struct xhci_endpoint *ep, unsigned int type,
						  void ( * populate ) ( struct xhci_host *xhci,
												struct xhci_slot *slot,
												struct xhci_endpoint *ep,
												void *input ) ) {
	union xhci_trb trb;
	struct xhci_trb_context *context = &trb.context;
	size_t len;
	void *input;
	int rc;

	/* Allocate an input context */
	len = xhci_input_context_offset ( xhci, XHCI_CTX_END );
	input = usb_align(xhci_align ( len ), len);
	if ( ! input ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	memset ( input, 0, len );

	/* Populate input context */
	populate ( xhci, slot, ep, input );

	/* Construct command */
	memset ( context, 0, sizeof ( *context ) );
	context->type = type;
	context->input = CPU_TO_LE64 ( ( input ) );
	context->slot = slot->id;

	/* Issue command and wait for completion */
	if ( ( rc = xhci_cmd_submit ( xhci, ep, &trb ) ) != 0 ) {
		xhci_dump_input_ctx(xhci, ep, input);
		goto err_command;
	}

 err_command:
	usb_free ( input );
 err_alloc:
	return rc;
}

/**
 * Populate address device input context
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @v input		Input context
 */
static void xhci_address_device_input ( struct xhci_host *xhci,
										struct xhci_slot *slot,
										struct xhci_endpoint *endpoint,
										void *input ) {
	struct xhci_control_context *control_ctx;
	struct xhci_slot_context *slot_ctx;
	struct xhci_endpoint_context *ep_ctx;

	/* Sanity checks */
	USB_ASSERT ( endpoint->ctx == XHCI_CTX_EP0 );

	/* Populate control context, add slot context and ep context */
	control_ctx = input;
	control_ctx->add = CPU_TO_LE32 ( ( 1 << XHCI_CTX_SLOT ) |
					 ( 1 << XHCI_CTX_EP0 ) );

	/* Populate slot context */
	slot_ctx = ( input + xhci_input_context_offset ( xhci, XHCI_CTX_SLOT ));
	slot_ctx->info = CPU_TO_LE32 ( XHCI_SLOT_INFO ( 1, 0, slot->psiv,
							slot->route ) );
	slot_ctx->port = slot->port;
	slot_ctx->tt_id = slot->tt_id;
	slot_ctx->tt_port = slot->tt_port;

	/* Populate control endpoint context */
	ep_ctx = ( input + xhci_input_context_offset ( xhci, XHCI_CTX_EP0 ) );
	ep_ctx->type = XHCI_EP_TYPE_CONTROL; /* bit[5:3] endpoint type */
	ep_ctx->burst = endpoint->burst; /* bit[16:8] max burst size */
	ep_ctx->mtu = CPU_TO_LE16 ( endpoint->mtu ); /* bit[31:16] max packet size */

	/*
		bit[63:4] tr dequeue pointer
		bit[0] dequeue cycle state
	*/
	ep_ctx->dequeue = CPU_TO_LE64 ( (uint64_t)( &endpoint->reqs.ring[0] ) | XHCI_EP_DCS );
	ep_ctx->trb_len = CPU_TO_LE16 ( XHCI_EP0_TRB_LEN );	/* bit[15:0] average trb length */
}

/**
 * Address device
 *
 * @v xhci		xHCI device
 * @v endpoint	Endpoint
 * @v slot		Device slot
 * @ret rc		Return status code
 */
static inline int xhci_address_device ( struct xhci_host *xhci,
										struct xhci_endpoint *ep,
										struct xhci_slot *slot ) {
	struct xhci_slot_context *slot_ctx;
	int rc;

	/* Assign device address */
	if ( ( rc = xhci_context ( xhci, slot, slot->endpoint[XHCI_CTX_EP0],
				   XHCI_TRB_ADDRESS_DEVICE,
				   xhci_address_device_input ) ) != 0 )

	USB_LOG_DBG("XHCI %s slot ctx 0x%x\n", xhci->name, slot->context);

	/* Get assigned address for check */
	slot_ctx = ( slot->context +
		     xhci_device_context_offset ( xhci, XHCI_CTX_SLOT ) );
	USB_LOG_DBG("XHCI %s slot ctx 0x%x assigned address 0x%x\n",
		xhci->name, slot_ctx, slot_ctx->address );

	return rc;
}

/**
 * Reset endpoint
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @ret rc		Return status code
 */
int xhci_reset_endpoint ( struct xhci_host *xhci,
						  struct xhci_slot *slot,
						  struct xhci_endpoint *endpoint ) {
	union xhci_trb trb;
	struct xhci_trb_reset_endpoint *reset = &trb.reset;
	int rc;

	/* Construct command */
	memset ( reset, 0, sizeof ( *reset ) );
	reset->slot = slot->id;
	reset->endpoint = endpoint->ctx;
	reset->type = XHCI_TRB_RESET_ENDPOINT;

	/* Issue command and wait for completion */
	if ( ( rc = xhci_cmd_submit ( xhci, endpoint, &trb ) ) != 0 ) {
		USB_LOG_DBG("XHCI %s slot %d ctx %d could not reset endpoint "
		       "in state %d: %s\n", xhci->name, slot->id, endpoint->ctx,
		       endpoint->context->state, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Stop endpoint
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint	Endpoint
 * @ret rc		Return status code
 */
static inline int xhci_stop_endpoint ( struct xhci_host *xhci,
				       				   struct xhci_slot *slot,
				       			       struct xhci_endpoint *endpoint ) {
	union xhci_trb trb;
	struct xhci_trb_stop_endpoint *stop = &trb.stop;
	int rc;

	/* Construct command */
	memset ( stop, 0, sizeof ( *stop ) );
	stop->slot = slot->id;
	stop->endpoint = endpoint->ctx;
	stop->type = XHCI_TRB_STOP_ENDPOINT;

	/* Issue command and wait for completion */
	if ( ( rc = xhci_cmd_submit ( xhci, endpoint, &trb ) ) != 0 ) {
		USB_LOG_DBG("XHCI %s slot %d ctx %d could not stop endpoint "
		       "in state %d: %s\n", xhci->name, slot->id, endpoint->ctx,
		       endpoint->context->state, strerror ( rc ) );
		return rc;
	}

	return 0;
}

/*********************************************************************/

/**
 * Find port slot type
 *
 * @v xhci		xHCI device
 * @v port		Port number
 * @ret type	Slot type, or negative error
 */
static int xhci_port_slot_type ( struct xhci_host *xhci, unsigned int port ) {
	unsigned int supported = xhci_supported_protocol ( xhci, port );
	unsigned int type;
	uint32_t slot;

	/* Fail if there is no supported protocol */
	if ( ! supported )
		return -ENOTSUP;

	/* Get slot type */
	slot = readl ( xhci->cap + supported + XHCI_SUPPORTED_SLOT );
	type = XHCI_SUPPORTED_SLOT_TYPE ( slot );

	return type;
}

/**
 * Open device
 *
 * @v xhci		XHCI device
 * @v ep		Endpoint
 * @ret slot_id Return device slot id
 * @ret rc		Return status code
 */
int xhci_device_open ( struct xhci_host *xhci, struct xhci_endpoint *ep, int *slot_id ) {
	struct usbh_hubport *hport = ep->hport;
	struct usbh_hubport *tt = usbh_transaction_translator(hport);
	struct xhci_slot *slot;
	struct xhci_slot *tt_slot;
	int type;
	int rc;
	int id;
	size_t len;

	/* Determine applicable slot type */
	type = xhci_port_slot_type ( xhci, hport->port );
	if ( type < 0 ) {
		rc = type;
		USB_LOG_ERR("XHCI %s-%d has no slot type\n",
		       xhci->name, hport->port );
		goto err_type;
	}

	/* Allocate a device slot number */
	id = xhci_enable_slot ( xhci, ep, type );
	if ( id < 0 ) {
		rc = id;
		goto err_enable_slot;
	}

	USB_ASSERT ( ( id > 0 ) && ( ( unsigned int ) id <= xhci->slots ) );
	USB_ASSERT ( xhci->slot[id] == NULL );

	/* Allocate and initialise structure */
	slot = usb_malloc ( sizeof ( *slot ) );
	if ( ! slot ) {
		rc = -ENOMEM;
		goto err_alloc;
	}
	slot->id = id;
	xhci->slot[id] = slot;
	slot->xhci = xhci;
	if ( tt ) {
		tt_slot = xhci->slot[tt->dev_addr];
		slot->tt_id = tt_slot->id;
		slot->tt_port = tt->port;
	}

	/* Allocate a device context */
	len = xhci_device_context_offset ( xhci, XHCI_CTX_END );
	slot->context = usb_align(xhci_align ( len ), len);
	if ( ! slot->context ) {
		rc = -ENOMEM;
		goto err_alloc_context;
	}
	memset ( slot->context, 0, len );

	/* Set device context base address */
	USB_ASSERT ( xhci->dcbaa.context[id] == 0 );
	xhci->dcbaa.context[id] = CPU_TO_LE64 ( ( slot->context ) );

	USB_LOG_DBG("XHCI %s slot %d device context [%08lx,%08lx)\n",
		xhci->name, slot->id,  ( slot->context ),
		(  ( slot->context ) + len ) );
	*slot_id = id;
	return 0;

	xhci->dcbaa.context[id] = 0;
	usb_free ( slot->context );

err_alloc_context:
	xhci->slot[id] = NULL;
	usb_free ( slot );
err_alloc:
	xhci_disable_slot ( xhci, ep, id );
err_enable_slot:
err_type:
	return rc;
}

/*********************************************************************/

/**
 * Assign device address
 *
 * @v xhci		XHCI device
 * @v slot		Slot
 * @v ep		Endpoint
 * @ret rc		Return status code
 */
int xhci_device_address ( struct xhci_host *xhci, struct xhci_slot *slot, struct xhci_endpoint *ep ) {
	USB_ASSERT((slot->xhci) && (ep->hport));
	struct usbh_hubport *hport = ep->hport;
	int psiv;
	int rc;

	/* Calculate route string */
	slot->route = usbh_route_string (hport);

	/* Calculate root hub port number */
	struct usbh_hubport *root_port = usbh_root_hub_port (hport);
	slot->port = root_port->port;

	/* Calculate protocol speed ID */
	psiv = xhci_port_psiv ( xhci, slot->port, hport->speed );
	if ( psiv < 0 ) {
		rc = psiv;
		return rc;
	}
	slot->psiv = psiv;

	/* Address device */
	if ( ( rc = xhci_address_device ( xhci, ep, slot ) ) != 0 )
		return rc;

	return 0;
}


/**
 * Close device
 *
 * @v slot		Slot
 */
void xhci_device_close ( struct xhci_slot *slot ) {
	struct xhci_host *xhci = slot->xhci;
	size_t len = xhci_device_context_offset ( xhci, XHCI_CTX_END );
	unsigned int id = slot->id;
	int rc;

	/* Disable slot */
	if ( ( rc = xhci_disable_slot ( xhci, slot->endpoint[0], id ) ) != 0 ) {
		/* Slot is still enabled.  Leak the slot context,
		 * since the controller may still write to this
		 * memory, and leave the DCBAA entry intact.
		 *
		 * If the controller later reports that this same slot
		 * has been re-enabled, then some assertions will be
		 * triggered.
		 */
		USB_LOG_DBG("XHCI %s slot %d leaking context memory\n",
		       xhci->name, slot->id );
		slot->context = NULL;
	}

	/* Free slot */
	if ( slot->context ) {
		usb_free ( slot->context );
		xhci->dcbaa.context[id] = 0;
	}
	xhci->slot[id] = NULL;
	usb_free ( slot );
}


/**
 * Populate configure endpoint input context
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @v input		Input context
 */
static void xhci_configure_endpoint_input ( struct xhci_host *xhci,
					    struct xhci_slot *slot,
					    struct xhci_endpoint *endpoint,
					    void *input ) {
	struct xhci_control_context *control_ctx;
	struct xhci_slot_context *slot_ctx;
	struct xhci_endpoint_context *ep_ctx;

	xhci_dump_endpoint(endpoint);

	/* Populate control context */
	control_ctx = input;
	control_ctx->add = CPU_TO_LE32 (( 1 << XHCI_CTX_SLOT ) | ( 1 << endpoint->ctx ) );
	control_ctx->drop = CPU_TO_LE32 (( 1 << XHCI_CTX_SLOT ) | ( 1 << endpoint->ctx ) );

	/* Populate slot context */
	slot_ctx = ( input + xhci_input_context_offset ( xhci, XHCI_CTX_SLOT ));
	slot_ctx->info = CPU_TO_LE32 ( XHCI_SLOT_INFO ( ( XHCI_CTX_END - 1 ),
							( slot->ports ? 1 : 0 ),
							slot->psiv, 0 ) );
	slot_ctx->ports = slot->ports;

	/* Populate endpoint context */
	ep_ctx = ( input + xhci_input_context_offset ( xhci, endpoint->ctx ) );
	ep_ctx->interval = endpoint->interval; /* bit[23:16] for interrupt ep, set interval to control interrupt period */
	/*
        Value Endpoint Type Direction
            0 Not Valid N/A
            1 Isoch Out
            2 Bulk Out
            3 Interrupt Out
            4 Control Bidirectional
            5 Isoch In
            6 Bulk In
            7 Interrupt In
    */
	ep_ctx->type = endpoint->ctx_type;
	ep_ctx->burst = endpoint->burst;
	ep_ctx->mtu = CPU_TO_LE16 ( endpoint->mtu ); /* bit[31:16] max packet size */
	ep_ctx->dequeue = CPU_TO_LE64 ( (uint64_t)( &(endpoint->reqs.ring[0]) ) | XHCI_EP_DCS );

	/* TODO: endpoint attached on hub may need different setting here */
	if (endpoint->ep_type == USB_ENDPOINT_TYPE_BULK) {
		ep_ctx->trb_len = CPU_TO_LE16 ( 256U ); /* bit[15:0] average trb length */
	} else if (endpoint->ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
		ep_ctx->trb_len = CPU_TO_LE16 (16U);
		ep_ctx->esit_low = CPU_TO_LE16 ( endpoint->mtu ); /* bit[31:16] max ESIT payload */
	}

	xhci_dump_input_ctx(xhci, endpoint, input);
}

/**
 * Configure endpoint
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @ret rc		Return status code
 */
static inline int xhci_configure_endpoint ( struct xhci_host *xhci,
					    struct xhci_slot *slot,
					    struct xhci_endpoint *endpoint ){
	int rc;

	/* Configure endpoint */
	if ( ( rc = xhci_context ( xhci, slot, endpoint,
				   XHCI_TRB_CONFIGURE_ENDPOINT,
				   xhci_configure_endpoint_input ) ) != 0 ){
		USB_LOG_ERR("XHCI %s slot %d ctx %d configure failed, error %d !!!\n",
			xhci->name, slot->id, endpoint->ctx, rc );
		return rc;
	}

	/* Sanity check */
	if ( ( endpoint->context->state & XHCI_ENDPOINT_STATE_MASK )
		   != XHCI_ENDPOINT_RUNNING ){
		xhci_dump_slot_ctx(endpoint->slot->context);
		xhci_dump_ep_ctx(endpoint->context);
		USB_LOG_ERR("XHCI %s slot %d ctx %d configure failed !!!\n",
			xhci->name, slot->id, endpoint->ctx );
		return -1;
	}

	USB_LOG_DBG("XHCI %s slot %d ctx %d configured\n",
		xhci->name, slot->id, endpoint->ctx );
	return 0;
}

/**
 * Populate deconfigure endpoint input context
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @v input		Input context
 */
static void
xhci_deconfigure_endpoint_input ( struct xhci_host *xhci,
				  struct xhci_slot *slot,
				  struct xhci_endpoint *endpoint,
				  void *input ) {
	struct xhci_control_context *control_ctx;
	struct xhci_slot_context *slot_ctx;

	/* Populate control context */
	control_ctx = input;
	control_ctx->add = CPU_TO_LE32 ( 1 << XHCI_CTX_SLOT );
	control_ctx->drop = CPU_TO_LE32 ( 1 << endpoint->ctx );

	/* Populate slot context */
	slot_ctx = ( input + xhci_input_context_offset ( xhci, XHCI_CTX_SLOT ));
	slot_ctx->info = CPU_TO_LE32 ( XHCI_SLOT_INFO ( ( XHCI_CTX_END - 1 ),
							0, 0, 0 ) );
}

/**
 * Deconfigure endpoint
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @ret rc		Return status code
 */
static inline int xhci_deconfigure_endpoint ( struct xhci_host *xhci,
					      					  struct xhci_slot *slot,
					      					  struct xhci_endpoint *endpoint ) {
	int rc;

	/* Deconfigure endpoint */
	if ( ( rc = xhci_context ( xhci, slot, endpoint,
				   XHCI_TRB_CONFIGURE_ENDPOINT,
				   xhci_deconfigure_endpoint_input ) ) != 0 )
		return rc;

	USB_LOG_DBG("XHCI %s slot %d ctx %d deconfigured\n",
		xhci->name, slot->id, endpoint->ctx );
	return 0;
}

/*********************************************************************/

/**
 * Open control endpoint
 *
 * @v xhci		XHCI device
 * @v slot		Slot
 * @v ep		Endpoint
 * @ret rc		Return status code
 */
int xhci_ctrl_endpoint_open ( struct xhci_host *xhci, struct xhci_slot *slot, struct xhci_endpoint *ep ) {
	unsigned int ctx;

	/* Calculate context index */
	ctx = XHCI_CTX ( ep->address );
	USB_ASSERT ( slot->endpoint[ctx] == NULL );

	if (USB_ENDPOINT_TYPE_CONTROL != ep->ep_type) {
		return -EINVAL;
	}

	/* initialise structure */
	slot->endpoint[ctx] = ep;
	ep->xhci = xhci;
	ep->slot = slot;
	ep->ctx = ctx;
	ep->ctx_type = XHCI_EP_TYPE_CONTROL;
	ep->context = ( ( ( void * ) slot->context ) +
			      xhci_device_context_offset ( xhci, ctx ) );
	ep->reqs.cs = 1; /* cycle state = 1 */

	USB_LOG_DBG("XHCI %s slot %d endpoint 0x%x ep type %d xhci ep type 0x%x\n",
		xhci->name, slot->id, ep->address, ep->ep_type,
		(ep->ctx_type >> 3) );

	USB_LOG_DBG("XHCI %s slot %d ctx %d ring [%08lx,%08lx)\n",
		xhci->name, slot->id, ctx,  ( ep->reqs.ring ),
		(  ( ep->reqs.ring ) + sizeof(ep->reqs.ring) ) );

	return 0;
}

/*********************************************************************/

/**
 * Open work endpoint
 *
 * @v xhci		XHCI device
 * @v slot		Slot
 * @v ep		USB endpoint
 * @ret rc		Return status code
 */
int xhci_work_endpoint_open ( struct xhci_host *xhci, struct xhci_slot *slot, struct xhci_endpoint *ep ) {
	unsigned int ctx;
	unsigned int interval;
	unsigned int ctx_type;
	int rc;

	/* Calculate context index */
	ctx = XHCI_CTX ( ep->address );
	USB_ASSERT ( slot->endpoint[ctx] == NULL );

	if (USB_ENDPOINT_TYPE_CONTROL == ep->ep_type) {
		return -EINVAL;
	}

	/* Calculate endpoint type */
    /*
        Value Endpoint Type Direction, bit[5:3]
            0 Not Valid N/A
            1 Isoch Out
            2 Bulk Out
            3 Interrupt Out
            4 Control Bidirectional
            5 Isoch In
            6 Bulk In
            7 Interrupt In
    */
	ctx_type = XHCI_EP_TYPE ( ep->ep_type );
    if ( ep->address & USB_EP_DIR_IN )
		ctx_type |= XHCI_EP_TYPE_IN;

	/* initialise structure */
	slot->endpoint[ctx] = ep;
	ep->xhci = xhci;
	ep->slot = slot;
	ep->ctx = ctx;

	/* Calculate interval */
	if ( ctx_type & XHCI_EP_TYPE_PERIODIC ) {
		ep->interval = ( xhci_fls ( ep->interval ) - 1 );
	}

	ep->ctx_type = ctx_type;
	ep->context = ( ( ( void * ) slot->context ) +
			      xhci_device_context_offset ( xhci, ctx ) );
	ep->reqs.cs = 1; /* cycle state = 1 */

	USB_LOG_DBG("XHCI %s slot %d endpoint 0x%x ep type %d xhci ep type 0x%x\n",
		xhci->name, slot->id, ep->address, ep->ep_type,
		(ep->ctx_type >> 3) );

	/* Configure endpoint */
	if (( rc = xhci_configure_endpoint ( xhci, slot, ep ) ) != 0) {
		goto err_configure_endpoint;
	}

	USB_LOG_DBG("XHCI %s slot %d ctx %d ring [%08lx,%08lx)\n",
		xhci->name, slot->id, ctx,  ( ep->reqs.ring ),
		(  ( ep->reqs.ring ) + sizeof(ep->reqs.ring) ) );

	return 0;
 err_configure_endpoint:
 	(void)xhci_deconfigure_endpoint ( xhci, slot, ep );
 	slot->endpoint[ctx] = NULL;
	return rc;
}

/**
 * Close endpoint
 *
 * @v ep		USB endpoint
 */
void xhci_endpoint_close ( struct xhci_endpoint *ep ) {
	struct xhci_slot *slot = ep->slot;
	struct xhci_host *xhci = slot->xhci;
	unsigned int ctx = ep->ctx;

	/* Deconfigure endpoint, if applicable */
	if ( ctx != XHCI_CTX_EP0 )
		(void)xhci_deconfigure_endpoint ( xhci, slot, ep );

	slot->endpoint[ctx] = NULL;
	usb_free(ep);
}

/*********************************************************************/

/**
 * Enqueue message transfer
 *
 * @v ep		USB endpoint
 * @v packet	Setup packet buffer
 * @v data_buff	Data buffer
 * @v datalen	Data length
 * @ret rc		Return status code
 */
void xhci_endpoint_message ( struct xhci_endpoint *ep,
							 struct usb_setup_packet *packet,
				   			 void *data_buff,
							 int datalen ) {
	struct xhci_host *xhci = ep->xhci;
	struct xhci_slot *slot = ep->slot;
	union xhci_trb trb;
	struct xhci_trb_setup *setup;
	struct xhci_trb_data *data;
	struct xhci_trb_status *status;
	unsigned int input;

	/* Construct setup stage TRB */
	setup = &(trb.setup);
	memset ( setup, 0, sizeof ( *setup ) );

	memcpy ( &setup->packet, packet, sizeof ( setup->packet ) );
	setup->len = CPU_TO_LE32 ( sizeof ( *packet ) ); /* bit[16:0] trb transfer length, always 8 */
	setup->flags = XHCI_TRB_IDT; /* bit[6] Immediate Data (IDT), parameters take effect */
	setup->type = XHCI_TRB_SETUP; /* bit[15:10] trb type */
	input = ( packet->bmRequestType & CPU_TO_LE16 ( USB_REQUEST_DIR_IN ) );
	if (datalen > 0) {
		/* bit[17:16] Transfer type, 2 = OUT Data, 3 = IN Data */
		setup->direction = ( input ? XHCI_SETUP_IN : XHCI_SETUP_OUT );
	}

	xhci_trb_queue(&(ep->reqs), &trb);

	/* Construct data stage TRB, if applicable */
	if (datalen > 0) {
		data = &(trb.data);
		memset ( data, 0, sizeof ( *data ) );

		data->data = CPU_TO_LE64 ( data_buff );
		data->len = CPU_TO_LE32 ( datalen );
		data->type = XHCI_TRB_DATA; /* bit[15:10] trb type */
		data->direction = ( input ? XHCI_DATA_IN : XHCI_DATA_OUT ); /* bit[16] Direction, 0 = OUT, 1 = IN */

		xhci_trb_queue(&(ep->reqs), &trb);
	}

	status = &(trb.status);
	memset ( status, 0, sizeof ( *status ) );
	status->flags = XHCI_TRB_IOC;
	status->type = XHCI_TRB_STATUS;
	status->direction =
		( ( datalen && input ) ? XHCI_STATUS_OUT : XHCI_STATUS_IN );

	xhci_trb_queue(&(ep->reqs), &trb);

    /* pass command trb to hardware */
    DSB();

    USB_LOG_DBG("ring doorbell slot-%d ep-%d \r\n", slot->id, ep->ctx);
    xhci_doorbell(xhci, slot->id, ep->ctx); /* 0 = db host controller, 0 = db targe hc command */

	return;
}

/*********************************************************************/

/**
 * Enqueue stream transfer
 *
 * @v ep		USB endpoint
 * @v data_buff	Data buffer
 * @v datalen	Data length
 * @ret rc		Return status code
 */
void xhci_endpoint_stream ( struct xhci_endpoint *ep,
							void *data_buff,
							int datalen ) {
	struct xhci_host *xhci = ep->xhci;
	struct xhci_slot *slot = ep->slot;
	union xhci_trb trbs;
	union xhci_trb *trb = &trbs;
	struct xhci_trb_normal *normal;
	int trb_len;

	/* Calculate TRB length */
	trb_len = XHCI_MTU;
	if ( trb_len > datalen ) {
		trb_len = datalen;
	} else {
		USB_LOG_ERR("transfer length %d exceed MTU %d \r\n", datalen, trb_len);
		goto err_enqueue;
	}

	/* Construct normal TRBs */
	normal = &trb->normal;
	memset ( normal, 0, sizeof ( *normal ) );
	normal->data = CPU_TO_LE64 ( (uintptr_t)data_buff );
	normal->len = CPU_TO_LE32 ( trb_len );
	normal->type = XHCI_TRB_NORMAL;
	normal->flags = XHCI_TRB_IOC;

	xhci_trb_queue(&(ep->reqs), trb);

    /* pass command trb to hardware */
    DSB();

	xhci_doorbell(xhci, slot->id, ep->ctx);

err_enqueue:
	return;
}

/*********************************************************************/

/**
 * Populate evaluate context input context
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @v input		Input context
 */
static void xhci_evaluate_context_input ( struct xhci_host *xhci,
					  					  struct xhci_slot *slot,
					  					  struct xhci_endpoint *endpoint,
					  					  void *input ) {
	struct xhci_control_context *control_ctx;
	struct xhci_slot_context *slot_ctx;
	struct xhci_endpoint_context *ep_ctx;

	/* Populate control context */
	control_ctx = input;
	control_ctx->add = CPU_TO_LE32 ( ( 1 << XHCI_CTX_SLOT ) /*|
					 ( 1 << endpoint->ctx )*/ );
	control_ctx->drop = CPU_TO_LE32 ( ( 1 << XHCI_CTX_SLOT ) /* |
					 ( 1 << endpoint->ctx )*/ );

	/* Populate slot context */
	slot_ctx = ( input + xhci_input_context_offset ( xhci, XHCI_CTX_SLOT ));
	slot_ctx->info = CPU_TO_LE32 ( XHCI_SLOT_INFO ( ( XHCI_CTX_END - 1 ),
							0, 0, 0 ) );

	/* Populate endpoint context */
	ep_ctx = ( input + xhci_input_context_offset ( xhci, endpoint->ctx ) );
	ep_ctx->mtu = CPU_TO_LE16 ( endpoint->mtu );
}

/**
 * Evaluate context
 *
 * @v xhci		xHCI device
 * @v slot		Device slot
 * @v endpoint		Endpoint
 * @ret rc		Return status code
 */
static inline int xhci_evaluate_context ( struct xhci_host *xhci,
					  					  struct xhci_slot *slot,
					  					  struct xhci_endpoint *endpoint ) {
	int rc;

	/* Configure endpoint */
	if ( ( rc = xhci_context ( xhci, slot, endpoint,
				   XHCI_TRB_EVALUATE_CONTEXT,
				   xhci_evaluate_context_input ) ) != 0 )
		return rc;

	USB_LOG_DBG("XHCI %s slot %d ctx %d (re-)evaluated\n",
		xhci->name, slot->id, endpoint->ctx );
	return 0;
}

/**
 * Update MTU
 *
 * @v ep		USB endpoint
 * @ret rc		Return status code
 */
int xhci_endpoint_mtu ( struct xhci_endpoint *ep ) {
	struct xhci_endpoint *endpoint = ( ep );
	struct xhci_slot *slot = endpoint->slot;
	struct xhci_host *xhci = slot->xhci;
	int rc;

	/* Evalulate context */
	if ( ( rc = xhci_evaluate_context ( xhci, slot, endpoint ) ) != 0 )
		return rc;

	return 0;
}

/*********************************************************************/

/**
 * Handle port status event
 *
 * @v xhci		xHCI device
 * @v trb		Port status event
 */
static void xhci_port_status ( struct xhci_host *xhci,
			       			   struct xhci_trb_port_status *trb ) {
    uint32_t portsc;

	/* Sanity check */
	USB_ASSERT ( ( trb->port > 0 ) && ( trb->port <= xhci->ports ) );

	/* Record disconnections, changes flag will be cleared later */
	portsc = readl ( xhci->op + XHCI_OP_PORTSC ( trb->port ) );
	xhci_dump_port_status(trb->port, portsc);

	if (portsc & XHCI_PORTSC_CSC) {
		/* Report port status change */
		usbh_roothub_thread_wakeup ( trb->port );
	}
}

/**
 * Handle transfer event
 *
 * @v xhci		xHCI device
 * @v trb		Transfer event TRB
 */
static void xhci_transfer ( struct xhci_host *xhci,
			    			struct xhci_trb_transfer *trb ) {
	struct xhci_slot *slot;
	struct xhci_endpoint *endpoint;
	union xhci_trb *trans_trb = (void *)(uintptr_t)(trb->transfer);
    struct xhci_ring *trans_ring = XHCI_RING(trans_trb); /* to align addr is ring base */
	union xhci_trb *pending = &trans_ring->evt; /* preserve event trb pending to handle */
    uint32_t eidx = trans_trb - trans_ring->ring + 1; /* calculate current evt trb index */
	int rc;

	/* Identify slot */
	if ( ( trb->slot > xhci->slots ) ||
	     ( ( slot = xhci->slot[trb->slot] ) == NULL ) ) {
		USB_LOG_DBG("XHCI %s transfer event invalid slot %d:\n",
		       xhci->name, trb->slot );
		return;
	}

	/* Identify endpoint */
	if ( ( trb->endpoint >= XHCI_CTX_END ) ||
	     ( ( endpoint = slot->endpoint[trb->endpoint] ) == NULL ) ) {
		USB_LOG_DBG("XHCI %s slot %d transfer event invalid epid "
		       "%d:\n", xhci->name, slot->id, trb->endpoint );
		return;
	}

	/* Record completion */
	memcpy(pending, trb, sizeof(*trb)); /* copy current trb to cmd/transfer ring */
	trans_ring->eidx = eidx;

	/* Check for errors */
	if ( ! ( ( trb->code == XHCI_CMPLT_SUCCESS ) ||
		 ( trb->code == XHCI_CMPLT_SHORT ) ) ) {
		USB_LOG_ERR("XHCI %s slot %d ctx %d failed (code %d)\n",
		       xhci->name, slot->id, endpoint->ctx, trb->code);

		/* Sanity check */
		USB_ASSERT ( ( endpoint->context->state & XHCI_ENDPOINT_STATE_MASK )
			 != XHCI_ENDPOINT_RUNNING );

		xhci_dump_ep_ctx(endpoint->context);
		return;
	}

	if (endpoint->waiter) {
		endpoint->waiter = false;
		usb_osal_sem_give(endpoint->waitsem);
	}

	if (endpoint->urb) {
		struct usbh_urb *cur_urb = endpoint->urb;
		cur_urb->errorcode = trb->code;
		/* bit [23:0] TRB Transfer length, residual number of bytes not transferred
				for OUT, is the value of (len of trb) - (data bytes transmitted), '0' means successful
				for IN, is the value of (len of trb) - (data bytes received),
					if cc is Short Packet, value is the diff between expected trans size and actual recv bytes
					if cc is other error, value is the diff between expected trans size and actual recv bytes */
		cur_urb->actual_length += cur_urb->transfer_buffer_length - trb->residual; /* bit [23:0] */

        if (cur_urb->complete) {
            if (cur_urb->errorcode < 0) {
                cur_urb->complete(cur_urb->arg, cur_urb->errorcode);
            } else {
                cur_urb->complete(cur_urb->arg, cur_urb->actual_length);
            }
        }
	}

	return;
}

/**
 * Handle command completion event
 *
 * @v xhci		xHCI device
 * @v trb		Command completion event
 */
static void xhci_complete ( struct xhci_host *xhci,
			    			struct xhci_trb_complete *trb ) {
	int rc;
	union xhci_trb *cmd_trb = (void *)(uintptr_t)(trb->command);
    struct xhci_ring *cmd_ring = XHCI_RING(cmd_trb); /* to align addr is ring base */
	union xhci_trb *pending = &cmd_ring->evt; /* preserve event trb pending to handle */
    uint32_t eidx = cmd_trb - cmd_ring->ring + 1; /* calculate current evt trb index */
	struct xhci_endpoint *work_pipe = xhci->cur_cmd_pipe;

	/* Ignore "command ring stopped" notifications */
	if ( trb->code == XHCI_CMPLT_CMD_STOPPED ) {
		USB_LOG_DBG("XHCI %s command ring stopped\n", xhci->name );
		return;
	}

	/* Record completion */
	USB_LOG_DBG("command-0x%x completed !!! \r\n", pending);
	memcpy(pending, trb, sizeof(*trb)); /* copy current trb to cmd/transfer ring */
	cmd_ring->eidx = eidx;

	USB_ASSERT(work_pipe);
	if (work_pipe->waiter)
	{
		work_pipe->waiter = false;
		usb_osal_sem_give(work_pipe->waitsem);
	}
}

/**
 * Handle host controller event
 *
 * @v xhci		xHCI device
 * @v trb		Host controller event
 */
static void xhci_host_controller ( struct xhci_host *xhci,
				   				   struct xhci_trb_host_controller *trb ) {
	int rc;

	/* Construct error */
	rc = -( trb->code );
	USB_LOG_ERR("XHCI %s host controller event (code %d)\n",
	       xhci->name, trb->code );
}

/**
 * Process event ring in interrupt
 *
 * @v xhci		xHCI device
 * @r workpip   current work endpoint
 */
struct xhci_endpoint *xhci_event_process(struct xhci_host *xhci) {
    struct xhci_endpoint *work_pipe = NULL;
    struct xhci_ring *evts = xhci->evts;
    unsigned int evt_type;
    unsigned int evt_cc;

    /* check and ack event */
    for (;;) {
		/* Stop if we reach an empty TRB */
		DSB();

        uint32_t nidx = evts->nidx; /* index of dequeue trb */
        uint32_t cs = evts->cs; /* cycle state toggle by xHc */
        union xhci_trb *trb = evts->ring + nidx; /* current trb */
        uint32_t control = trb->common.flags; /* trb control field */

        if ((control & XHCI_TRB_C) != (cs ? 1 : 0)) { /* if cycle state not toggle, no events need to handle */
            break;
        }

        /* Handle TRB */
		evt_type = ( trb->common.type & XHCI_TRB_TYPE_MASK );
		switch ( evt_type ) {

		case XHCI_TRB_TRANSFER :
			evt_cc = trb->transfer.code;
			xhci_transfer ( xhci, &trb->transfer );
			break;

		case XHCI_TRB_COMPLETE :
			evt_cc = trb->complete.code;
			xhci_complete ( xhci, &trb->complete );
			break;

		case XHCI_TRB_PORT_STATUS:
			evt_cc = trb->port.code;
			xhci_port_status ( xhci, &trb->port );
			break;

		case XHCI_TRB_HOST_CONTROLLER:
			evt_cc = trb->host.code;
			xhci_host_controller ( xhci, &trb->host );
			break;

		default:
			USB_LOG_DBG("XHCI %s unrecognised event type %d, cc %d\n:",
			       xhci->name, ( evt_type ) );
			break;
        }

        /* move ring index, notify xhci */
        nidx++; /* head to next trb */
        if (nidx == XHCI_RING_ITEMS)
        {
            nidx = 0; /* roll-back if reach end of list */
            cs = cs ? 0 : 1;
            evts->cs = cs; /* sw toggle cycle state */
        }
        evts->nidx = nidx;

        /* Update dequeue pointer if applicable */
        uint64_t erdp = (uint64_t)(unsigned long)(evts->ring + nidx);
		xhci_writeq ( xhci, (uintptr_t)( erdp ) | XHCI_RUN_ERDP_EHB,
				xhci->run + XHCI_RUN_ERDP ( 0 ) );
   }

   return work_pipe;
}