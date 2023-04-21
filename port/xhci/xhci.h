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
 * FilePath: xhci.h
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
#ifndef XHCI_H
#define XHCI_H

#include "xhci_reg.h"

#include "usbh_core.h"

/** @file
 *
 * USB eXtensible Host Controller Interface (xHCI) driver
 *
 */

#define XHCI_RING_ITEMS     16U
#define XHCI_ALIGMENT       64U
#define XHCI_RING_SIZE      (XHCI_RING_ITEMS * sizeof(union xhci_trb))

/*
 *  xhci_ring structs are allocated with XHCI_RING_SIZE alignment,
 *  then we can get it from a trb pointer (provided by evt ring).
 */
#define XHCI_RING(_trb)          \
    ((struct xhci_ring*)((unsigned long)(_trb) & ~(XHCI_RING_SIZE-1)))

/** Device context base address array */
struct xhci_dcbaa {
	/** Context base addresses */
	uint64_t *context;
};

/** Scratchpad buffer */
struct xhci_scratchpad {
	/** Number of page-sized scratchpad buffers */
	unsigned int count;
	/** Scratchpad buffer area */
	uintptr_t buffer;
	/** Scratchpad array */
	uint64_t *array;
};

/** An input control context */
struct xhci_control_context {
	/** Drop context flags */
	uint32_t drop;
	/** Add context flags */
	uint32_t add;
	/** Reserved */
	uint32_t reserved_a[5];
	/** Configuration value */
	uint8_t config;
	/** Interface number */
	uint8_t intf;
	/** Alternate setting */
	uint8_t alt;
	/** Reserved */
	uint8_t reserved_b;
} __attribute__ (( packed ));

/** A slot context */
struct xhci_slot_context {
	/** Device info 03-00h */
	uint32_t info;
	/** Maximum exit latency */
	uint16_t latency;
	/** Root hub port number */
	uint8_t port;
	/** Number of downstream ports 07-04h */
	uint8_t ports;
	/** TT hub slot ID */
	uint8_t tt_id;
	/** TT port number */
	uint8_t tt_port;
	/** Interrupter target 0b-08h */
	uint16_t intr;
	/** USB address */
	uint8_t address;
	/** Reserved */
	uint16_t reserved_a;
	/** Slot state 0f-0ch */
	uint8_t state;
	/** Reserved */
	uint32_t reserved_b[4];
} __attribute__ (( packed ));

/** Construct slot context device info */
#define XHCI_SLOT_INFO( entries, hub, speed, route ) \
	( ( (entries) << 27 ) | ( (hub) << 26 ) | ( (speed) << 20 ) | (route) )

/** An endpoint context */
struct xhci_endpoint_context {
	/** Endpoint state  03-00h*/
	uint8_t state;
	/** Stream configuration */
	uint8_t stream;
#define XHCI_EPCTX_MULT_GET(stream)		XHCI32_GET_BITS(stream, 1, 0)
#define XHCI_EPCTX_STREAM_GET(stream)	XHCI32_GET_BITS(stream, 6, 2)
#define XHCI_EPCTX_LSA					BIT(7)
	/** Polling interval */
	uint8_t interval;
	/** Max ESIT payload high */
	uint8_t esit_high;
	/** Endpoint type 04-04h */
	uint8_t type;
#define XHCI_EPCTX_CERR_GET(type)		XHCI32_GET_BITS(type, 2, 1)
#define XHCI_EPCTX_TYPE_GET(type)		XHCI32_GET_BITS(type, 5, 3)
#define XHCI_EPCTX_HID					BIT(7)
	/** Maximum burst size */
	uint8_t burst;
	/** Maximum packet size */
	uint16_t mtu;
	/** Transfer ring dequeue pointer 0f-08h */
	uint64_t dequeue;
#define XHCI_EPCTX_DCS				   BIT(0)
	/** Average TRB length 13-10h */
	uint16_t trb_len;
	/** Max ESIT payload low */
	uint16_t esit_low;
	/** Reserved */
	uint32_t reserved[3];
} __attribute__ (( packed ));

/** Endpoint states */
enum {
	/** Endpoint is disabled */
	XHCI_ENDPOINT_DISABLED = 0,
	/** Endpoint is running */
	XHCI_ENDPOINT_RUNNING = 1,
	/** Endpoint is halted due to a USB Halt condition */
	XHCI_ENDPOINT_HALTED = 2,
	/** Endpoint is stopped */
	XHCI_ENDPOINT_STOPPED = 3,
	/** Endpoint is halted due to a TRB error */
	XHCI_ENDPOINT_ERROR = 4,
};

/** Endpoint state mask */
#define XHCI_ENDPOINT_STATE_MASK 0x07

/** Endpoint type */
#define XHCI_EP_TYPE(type) ( (type) << 3 )

/** Control endpoint type */
#define XHCI_EP_TYPE_CONTROL XHCI_EP_TYPE ( 4 )

/** Input endpoint type */
#define XHCI_EP_TYPE_IN XHCI_EP_TYPE ( 4 )

/** Periodic endpoint type */
#define XHCI_EP_TYPE_PERIODIC XHCI_EP_TYPE ( 1 )

/** Endpoint dequeue cycle state */
#define XHCI_EP_DCS 0x00000001UL

/** Control endpoint average TRB length */
#define XHCI_EP0_TRB_LEN 8

/**
 * Calculate doorbell register value
 *
 * @v target		Doorbell target
 * @v stream		Doorbell stream ID
 * @ret dbval		Doorbell register value
 */
#define XHCI_DBVAL( target, stream ) ( (target) | ( (stream) << 16 ) )


/** Slot context index */
#define XHCI_CTX_SLOT 0

/** Calculate context index from USB endpoint address */
/* refer to spec. Figure 4-4: Endpoint Context Addressing
		ep0 = 1, ep1-out = 2, ep1-in = 3, ... ep15-out = 30, ep15-in = 31 */
#define XHCI_CTX(address)						\
	( (address) ? ( ( ( (address) & 0x0f ) << 1 ) |			\
			( ( (address) & 0x80 ) >> 7 ) ) : 1 )

/** Endpoint zero context index */
#define XHCI_CTX_EP0 XHCI_CTX ( 0x00 )

/** End of contexts */
#define XHCI_CTX_END 32

/** Device context index */
#define XHCI_DCI(ctx) ( (ctx) + 0 )

/** Input context index */
#define XHCI_ICI(ctx) ( (ctx) + 1 )

/** Number of TRBs (excluding Link TRB) in the command ring
 *
 * This is a policy decision.
 */
#define XHCI_CMD_TRBS_LOG2 2

/** Number of TRBs in the event ring
 *
 * This is a policy decision.
 */
#define XHCI_EVENT_TRBS_LOG2 6

/** Number of TRBs in a transfer ring
 *
 * This is a policy decision.
 */
#define XHCI_TRANSFER_TRBS_LOG2 6

/** Maximum time to wait for BIOS to release ownership
 *
 * This is a policy decision.
 */
#define XHCI_USBLEGSUP_MAX_WAIT_MS 100

/** Maximum time to wait for host controller to stop
 *
 * This is a policy decision.
 */
#define XHCI_STOP_MAX_WAIT_MS 100

/** Maximum time to wait for reset to complete
 *
 * This is a policy decision.
 */
#define XHCI_RESET_MAX_WAIT_MS 500

/** Maximum time to wait for a command to complete
 *
 * The "address device" command involves waiting for a response to a
 * USB control transaction, and so we must wait for up to the 5000ms
 * that USB allows for devices to respond to control transactions.
 */
#define XHCI_COMMAND_MAX_WAIT_MS 5000

/** Time to delay after aborting a command
 *
 * This is a policy decision
 */
#define XHCI_COMMAND_ABORT_DELAY_MS 500

/** Maximum time to wait for a port reset to complete
 *
 * This is a policy decision.
 */
#define XHCI_PORT_RESET_MAX_WAIT_MS 500

/** A transfer request block template */
struct xhci_trb_template {
	/** Parameter */
	uint64_t parameter;
	/** Status */
	uint32_t status;
	/** Control */
	uint32_t control;
};

/** A transfer request block */
struct xhci_trb_common {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint32_t reserved_b;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint16_t reserved_c;
} __attribute__ (( packed ));

/** Transfer request block cycle bit flag */
#define XHCI_TRB_C 0x01

/** Transfer request block toggle cycle bit flag */
#define XHCI_TRB_TC 0x02

/** Transfer request block chain flag */
#define XHCI_TRB_CH 0x10

/** Transfer request block interrupt on completion flag */
#define XHCI_TRB_IOC 0x20

/** Transfer request block immediate data flag */
#define XHCI_TRB_IDT 0x40

/** Transfer request block type */
#define XHCI_TRB_TYPE(type) ( (type) << 2 )

/** Transfer request block type mask */
#define XHCI_TRB_TYPE_MASK XHCI_TRB_TYPE ( 0x3f )

/** A normal transfer request block */
struct xhci_trb_normal {
	/** Data buffer */
	uint64_t data;
	/** Length */
	uint32_t len;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint16_t reserved;
} __attribute__ (( packed ));

/** A normal transfer request block */
#define XHCI_TRB_NORMAL XHCI_TRB_TYPE ( 1 )

/** Construct TD size field */
#define XHCI_TD_SIZE(remaining) \
	( ( ( (remaining) <= 0xf ) ? remaining : 0xf ) << 17 )

/** A setup stage transfer request block */
struct xhci_trb_setup {
	/** Setup packet, 04-00h sw will copy request content to this field */
	struct usb_setup_packet packet;
	/** Length 08h */
	uint32_t len;
	/** Flags 0ch */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Transfer direction */
	uint8_t direction;
	/** Reserved */
	uint8_t reserved;
} __attribute__ (( packed ));

/** A setup stage transfer request block */
#define XHCI_TRB_SETUP XHCI_TRB_TYPE ( 2 )

/** Setup stage input data direction */
#define XHCI_SETUP_IN 3

/** Setup stage output data direction */
#define XHCI_SETUP_OUT 2

/** A data stage transfer request block */
struct xhci_trb_data {
	/** Data buffer */
	uint64_t data;
	/** Length */
	uint32_t len;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Transfer direction */
	uint8_t direction;
	/** Reserved */
	uint8_t reserved;
} __attribute__ (( packed ));

/** A data stage transfer request block */
#define XHCI_TRB_DATA XHCI_TRB_TYPE ( 3 )

/** Input data direction */
#define XHCI_DATA_IN 0x01

/** Output data direction */
#define XHCI_DATA_OUT 0x00

/** A status stage transfer request block */
struct xhci_trb_status {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint32_t reserved_b;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Direction */
	uint8_t direction;
	/** Reserved */
	uint8_t reserved_c;
} __attribute__ (( packed ));

/** A status stage transfer request block */
#define XHCI_TRB_STATUS XHCI_TRB_TYPE ( 4 )

/** Input status direction */
#define XHCI_STATUS_IN 0x01

/** Output status direction */
#define XHCI_STATUS_OUT 0x00

/** A link transfer request block */
struct xhci_trb_link {
	/** Next ring segment */
	uint64_t next;
	/** Reserved */
	uint32_t reserved_a;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint16_t reserved_c;
} __attribute__ (( packed ));

/** A link transfer request block */
#define XHCI_TRB_LINK XHCI_TRB_TYPE ( 6 )

/** A no-op transfer request block */
#define XHCI_TRB_NOP XHCI_TRB_TYPE ( 8 )

/** An enable slot transfer request block */
struct xhci_trb_enable_slot {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint32_t reserved_b;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Slot type */
	uint8_t slot;
	/** Reserved */
	uint8_t reserved_c;
} __attribute__ (( packed ));

/** An enable slot transfer request block */
#define XHCI_TRB_ENABLE_SLOT XHCI_TRB_TYPE ( 9 )

/** A disable slot transfer request block */
struct xhci_trb_disable_slot {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint32_t reserved_b;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint8_t reserved_c;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** A disable slot transfer request block */
#define XHCI_TRB_DISABLE_SLOT XHCI_TRB_TYPE ( 10 )

/** A context transfer request block */
struct xhci_trb_context {
	/** Input context */
	uint64_t input;
	/** Reserved */
	uint32_t reserved_a;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint8_t reserved_b;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** An address device transfer request block */
#define XHCI_TRB_ADDRESS_DEVICE XHCI_TRB_TYPE ( 11 )

/** A configure endpoint transfer request block */
#define XHCI_TRB_CONFIGURE_ENDPOINT XHCI_TRB_TYPE ( 12 )

/** An evaluate context transfer request block */
#define XHCI_TRB_EVALUATE_CONTEXT XHCI_TRB_TYPE ( 13 )

/** A reset endpoint transfer request block */
struct xhci_trb_reset_endpoint {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint32_t reserved_b;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Endpoint ID */
	uint8_t endpoint;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** A reset endpoint transfer request block */
#define XHCI_TRB_RESET_ENDPOINT XHCI_TRB_TYPE ( 14 )

/** A stop endpoint transfer request block */
struct xhci_trb_stop_endpoint {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint32_t reserved_b;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Endpoint ID */
	uint8_t endpoint;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** A stop endpoint transfer request block */
#define XHCI_TRB_STOP_ENDPOINT XHCI_TRB_TYPE ( 15 )

/** A set transfer ring dequeue pointer transfer request block */
struct xhci_trb_set_tr_dequeue_pointer {
	/** Dequeue pointer */
	uint64_t dequeue;
	/** Reserved */
	uint32_t reserved;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Endpoint ID */
	uint8_t endpoint;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** A set transfer ring dequeue pointer transfer request block */
#define XHCI_TRB_SET_TR_DEQUEUE_POINTER XHCI_TRB_TYPE ( 16 )

/** A no-op command transfer request block */
#define XHCI_TRB_NOP_CMD XHCI_TRB_TYPE ( 23 )

/** A transfer event transfer request block */
struct xhci_trb_transfer {
	/** Transfer TRB pointer */
	uint64_t transfer;
	/** Residual transfer length */
	uint16_t residual;
	/** Reserved */
	uint8_t reserved;
	/** Completion code */
	uint8_t code;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Endpoint ID */
	uint8_t endpoint;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** A transfer event transfer request block */
#define XHCI_TRB_TRANSFER XHCI_TRB_TYPE ( 32 )

/** A command completion event transfer request block */
struct xhci_trb_complete {
	/** Command TRB pointer */
	uint64_t command;
	/** Parameter */
	uint8_t parameter[3];
	/** Completion code */
	uint8_t code;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Virtual function ID */
	uint8_t vf;
	/** Slot ID */
	uint8_t slot;
} __attribute__ (( packed ));

/** A command completion event transfer request block */
#define XHCI_TRB_COMPLETE XHCI_TRB_TYPE ( 33 )

/** xHCI completion codes */
enum xhci_completion_code {
	/** Timeout */
	XHCI_CMPLT_TIMEOUT = -1,
	/** Success */
	XHCI_CMPLT_SUCCESS = 1,
	/** Stall Error */
	XHCI_CMPLT_STALL = 6,
	/** Bandwidth Error */
	XHCI_CMPLT_BANDWIDTH = 8,
	/** Endpoint Not Enabled Error */
	XHCI_CMPLT_ENDPOINT_NOT_ENABLED = 12,
	/** Short packet */
	XHCI_CMPLT_SHORT = 13,
	/** Parameter Error */
	XHCI_CMPLT_PARAMETER = 17,
	/** Command ring stopped */
	XHCI_CMPLT_CMD_STOPPED = 24,
};

/** A port status change transfer request block */
struct xhci_trb_port_status {
	/** Reserved */
	uint8_t reserved_a[3];
	/** Port ID */
	uint8_t port;
	/** Reserved */
	uint8_t reserved_b[7];
	/** Completion code */
	uint8_t code;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint16_t reserved_c;
} __attribute__ (( packed ));

/** A port status change transfer request block */
#define XHCI_TRB_PORT_STATUS XHCI_TRB_TYPE ( 34 )

/** A port status change transfer request block */
struct xhci_trb_host_controller {
	/** Reserved */
	uint64_t reserved_a;
	/** Reserved */
	uint8_t reserved_b[3];
	/** Completion code */
	uint8_t code;
	/** Flags */
	uint8_t flags;
	/** Type */
	uint8_t type;
	/** Reserved */
	uint16_t reserved_c;
} __attribute__ (( packed ));

/** A port status change transfer request block */
#define XHCI_TRB_HOST_CONTROLLER XHCI_TRB_TYPE ( 37 )

/** A transfer request block */
union xhci_trb {
	/** Template */
	struct xhci_trb_template template;
	/** Common fields */
	struct xhci_trb_common common;
	/** Normal TRB */
	struct xhci_trb_normal normal;
	/** Setup stage TRB */
	struct xhci_trb_setup setup;
	/** Data stage TRB */
	struct xhci_trb_data data;
	/** Status stage TRB */
	struct xhci_trb_status status;
	/** Link TRB */
	struct xhci_trb_link link;
	/** Enable slot TRB */
	struct xhci_trb_enable_slot enable;
	/** Disable slot TRB */
	struct xhci_trb_disable_slot disable;
	/** Input context TRB */
	struct xhci_trb_context context;
	/** Reset endpoint TRB */
	struct xhci_trb_reset_endpoint reset;
	/** Stop endpoint TRB */
	struct xhci_trb_stop_endpoint stop;
	/** Set transfer ring dequeue pointer TRB */
	struct xhci_trb_set_tr_dequeue_pointer dequeue;
	/** Transfer event */
	struct xhci_trb_transfer transfer;
	/** Command completion event */
	struct xhci_trb_complete complete;
	/** Port status changed event */
	struct xhci_trb_port_status port;
	/** Host controller event */
	struct xhci_trb_host_controller host;
} __attribute__ (( packed ));

struct xhci_ring {
    union  xhci_trb      ring[XHCI_RING_ITEMS];
    union  xhci_trb      evt;
    uint32_t             eidx;
    uint32_t             nidx;
    uint32_t             cs;
    usb_osal_mutex_t     lock;
};

/** An event ring segment */
struct xhci_er_seg {
	/** Base address */
	uint64_t base;
	/** Number of TRBs */
	uint32_t count;
	/** Reserved */
	uint32_t reserved;
} __attribute__ (( packed ));

/** An xHCI endpoint */
struct xhci_endpoint {
    struct xhci_ring     reqs; /* DO NOT MOVE reqs from structure beg */
	/** xHCI device */
	struct xhci_host   *xhci;
	/** xHCI slot */
	struct xhci_slot     *slot;
	/** Endpoint address */
	unsigned int address;
	/** Context index */
	unsigned int ctx;
	/** Endpoint type in USB definition */
	unsigned int ep_type;
	/** Endpoint type in XHCI Endpoint context definition */
	unsigned int ctx_type;

	/** Maximum transfer size (Maximum packet size) */
	unsigned int mtu;
	/** Maximum burst size */
	unsigned int burst;    
	/** Endpoint interval */
	unsigned int interval;

	/** Endpoint context */
	struct xhci_endpoint_context *context;

    /* command or transfer waiter */
    int                  timeout; /* = 0 no need to wait */
    bool                 waiter;
    usb_osal_sem_t       waitsem;

    /* handle urb */
    struct usbh_hubport *hport;
    struct usbh_urb     *urb; /* NULL if no active URB */
};

/** An xHCI device slot */
struct xhci_slot {
	/** xHCI device */
	struct xhci_host *xhci;
	/** Slot ID */
	unsigned int id;
	/** Slot context */
	struct xhci_slot_context *context;

	/** Route string */
	unsigned int route;
	/** Root hub port number */
	unsigned int port;
	/** Protocol speed ID */
	unsigned int psiv;
	/** Number of ports (if this device is a hub) */
	unsigned int ports;
	/** Transaction translator slot ID */
	unsigned int tt_id;
	/** Transaction translator port */
	unsigned int tt_port;

	/** Endpoints, indexed by context ID */
	struct xhci_endpoint *endpoint[XHCI_CTX_END];
};

/** An xHCI device */
struct xhci_host {
	/** ID */
	uint32_t			 id;
	/** Name */
	char           		 name[4];

    /* xhci registers base addr */
    /** Registers base */
    void                 *base;
	/** Capability registers */
	void                 *cap;
	/** Operational registers */
	void                 *op;
	/** Runtime registers */
	void                 *run;
	/** Doorbell registers */
	void                 *db;
    /** extended capability */
    unsigned int         xecp;
    /** capability cache */
    uint32_t             hcs[3];
    uint32_t             hcc;
    /** xhci version */
    uint16_t             version;

	/** Number of device slots */
	unsigned int         slots;
	/** Number of interrupters */
	unsigned int         intrs;
	/** Number of ports */
	unsigned int         ports;

	/** 64-bit addressing capability */
	int                  addr64;
	/** Context size shift */
	unsigned int         csz_shift;
	/** Page size */
	size_t               pagesize;

	/** USB legacy support capability (if present and enabled) */
	unsigned int         legacy;

	/** Device context base address array */
	struct xhci_dcbaa    dcbaa;

	/** Scratchpad buffer */
	struct xhci_scratchpad scratch;

	/** Device slots, indexed by slot ID */
	struct xhci_slot       **slot;

	/** Command ring */
	struct xhci_ring       *cmds;
	/** Event ring */
	struct xhci_ring       *evts;
    struct xhci_er_seg     *eseg;

	struct xhci_endpoint   *cur_cmd_pipe;
};

/**
 * Calculate input context offset
 *
 * @v xhci		xHCI device
 * @v ctx		Context index
 */
static inline size_t xhci_input_context_offset ( struct xhci_host *xhci,
						 unsigned int ctx ) {

	return ( XHCI_ICI ( ctx ) << xhci->csz_shift );
}

/**
 * Calculate device context offset
 *
 * @v xhci		xHCI device
 * @v ctx		Context index
 */
static inline size_t xhci_device_context_offset ( struct xhci_host *xhci,
						  unsigned int ctx ) {

	return ( XHCI_DCI ( ctx ) << xhci->csz_shift );
}

/* Probe XCHI device */
int xhci_probe ( struct xhci_host *xhci, unsigned long baseaddr );

/* Open XHCI device and start running */
int xhci_open ( struct xhci_host *xhci );

/* Close XHCI device and stop running */
void xhci_close ( struct xhci_host *xhci );

/* Remove XHCI device and free allocated memory */
void xhci_remove ( struct xhci_host *xhci );

/* Enable port */
int xhci_port_enable (struct xhci_host *xhci, uint32_t port);

/* Get port speed */
uint32_t xhci_root_speed ( struct xhci_host *xhci, uint8_t port );

/* Open and enable device */
int xhci_device_open ( struct xhci_host *xhci, struct xhci_endpoint *pipe, int *slot_id );

/* Assign device address */
int xhci_device_address ( struct xhci_host *xhci, struct xhci_slot *slot, struct xhci_endpoint *pipe );

/* Close device and free allocated memory */
void xhci_device_close ( struct xhci_slot *slot );

/* Open control endpoint for slot */
int xhci_ctrl_endpoint_open ( struct xhci_host *xhci, struct xhci_slot *slot, struct xhci_endpoint *pipe );

/* Open work endpoint for slot */
int xhci_work_endpoint_open ( struct xhci_host *xhci, struct xhci_slot *slot, struct xhci_endpoint *pipe );

/* Close endpoint and free allocated memory */
void xhci_endpoint_close ( struct xhci_endpoint *ep );

/* Update MTU (Max packet size) of endpoint */
int xhci_endpoint_mtu ( struct xhci_endpoint *ep );

/* Enqueue message transfer, usually for control transfer */
void xhci_endpoint_message ( struct xhci_endpoint *ep, struct usb_setup_packet *packet,
				   			void *data_buff, int datalen );

/* Enqueue stream transfer, usually for bulk/interrupt transfer */
void xhci_endpoint_stream ( struct xhci_endpoint *ep, void *data_buff, int datalen );

/* Process event ring in interrupt */
struct xhci_endpoint *xhci_event_process(struct xhci_host *xhci);

/* Wait for a ring to empty (all TRBs processed by hardware) */
int xhci_event_wait(struct xhci_host *xhci,
					struct xhci_endpoint *pipe,
					struct xhci_ring *ring);

/* Dump host controller registers */
void xhci_dump(struct xhci_host *xhci);

/* Dump port registers */
void xhci_dump_port ( struct xhci_host *xhci,
				      unsigned int port );

/* Dump Port status */
void xhci_dump_port_status(uint32_t port, uint32_t portsc);

/* Dump input context */
void xhci_dump_input_ctx( struct xhci_host *xhci, const struct xhci_endpoint *endpoint, const void *input);

/* Dump Endpoint */
void xhci_dump_endpoint(const struct xhci_endpoint *ep);

/* Dump endpoint context */
void xhci_dump_ep_ctx(const struct xhci_endpoint_context *ep);

/* Dump TRB */
void xhci_dump_trbs(const union xhci_trb *trbs, unsigned int count);

/* Dump slot context */
void xhci_dump_slot_ctx(const struct xhci_slot_context *const sc);

#endif /* XHCI_H */