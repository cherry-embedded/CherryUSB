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
 * FilePath: usb_hc_xhci.h
 * Date: 2022-07-19 09:26:25
 * LastEditTime: 2022-07-19 09:26:25
 * Description:  This files is for xhci data structure definition
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 */
#ifndef  USB_HC_XHCI_H
#define  USB_HC_XHCI_H

/***************************** Include Files *********************************/
#include "usbh_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

/************************** Constant Definitions *****************************/

/************************** Type Definitions     *****************************/
/* slot context */
struct xhci_slotctx {
    uint32_t ctx[4];
#define XHCI_SLOTCTX_0_MAX_EPID_SET(maxid)  XHCI32_SET_BITS(maxid, 31, 27)
#define XHCI_SLOTCTX_0_SPEED_SET(speed)     XHCI32_SET_BITS(speed, 23, 20)
#define XHCI_SLOTCTX_1_ROOT_PORT_GET(val)   XHCI32_GET_BITS(val, 23, 16)
#define XHCI_SLOTCTX_1_ROOT_PORT_SET(port)  XHCI32_SET_BITS(port, 23, 16)
#define XHCI_SLOTCTX_0_ROUTE_SET(route)     XHCI32_SET_BITS(route, 19, 0)
#define XHCI_SLOTCTX_0_ROUTE_GET(route)     XHCI32_GET_BITS(route, 19, 0)
#define XHCI_SLOTCTX_2_HUB_SLOT_SET(slot)   XHCI32_SET_BITS(slot, 7, 0)
#define XHCI_SLOTCTX_2_HUB_SLOT_GET(slot)   XHCI32_GET_BITS(slot, 7, 0)
#define XHCI_SLOTCTX_2_HUB_PORT_SET(port)   XHCI32_SET_BITS(port, 15, 8)
#define XHCI_SLOTCTX_2_HUB_PORT_GET(port)   XHCI32_GET_BITS(port, 15, 8)
#define XHCI_SLOTCTX_3_SLOT_STATE_GET(ctx)  XHCI32_GET_BITS(ctx, 31, 27)
    uint32_t reserved_01[4];
#define XHCI_SLOTCTX_ENTRY_NUM     32U
#define XHCI_SLOTCTX_ALIGMENT      1024U
} __PACKED;

enum xhci_slot_state {
    XHCI_SLOT_DEFAULT  = 1,
    XHCI_SLOT_ADDRESS  = 2,
    XHCI_SLOT_CONFIG   = 3
};

/* endpoint context */
struct xhci_epctx {
    uint32_t ctx[2];
#define XHCI_EPCTX_0_EP_STATE_GET(ctx)          XHCI32_GET_BITS(ctx, 2, 0)
#define XHCI_EPCTX_0_INTERVAL_SET(interval)     XHCI32_SET_BITS(interval, 23, 16)
#define XHCI_EPCTX_1_MPS_SET(mps)               XHCI32_SET_BITS(mps, 31, 16)    
#define XHCI_EPCTX_1_MPS_GET(ctx)               XHCI32_GET_BITS(ctx, 31, 16)
#define XHCI_EPCTX_1_EPTYPE_GET(ctx)            XHCI32_GET_BITS(ctx, 5, 3)
#define XHCI_EPCTX_1_CERR_SET(cerr)             XHCI32_SET_BITS(cerr, 2, 1)      
    uint32_t deq_low;
    uint32_t deq_high;
    uint32_t length;
#define XHCI_EPCTX_AVE_TRB_LEN_SET(len)         XHCI32_SET_BITS(len, 15, 0)  
#define XHCI_EPCTX_MAX_ESIT_SET(esit)           XHCI32_SET_BITS(esit, 31, 16)
    uint32_t reserved_01[3];
} __PACKED;

/* device context array element */
struct xhci_devlist {
    uint32_t ptr_low;
    uint32_t ptr_high;
} __PACKED;

/* input context */
struct xhci_inctx {
    uint32_t del;
    uint32_t add;
    uint32_t reserved_01[6];
/* refer to spec. The Input Context is an array of up to 33 context data structure entries */
#define XHCI_INCTX_ENTRY_NUM     33U
#define XHCI_INCTX_ALIGMENT      2048
} __PACKED;

/* transfer block (ring element) */
struct xhci_trb {
    uint32_t ptr_low;
    uint32_t ptr_high;
    uint32_t status;
    uint32_t control;
} __PACKED;

/* event ring segment */
struct xhci_er_seg {
    uint32_t ptr_low;
    uint32_t ptr_high;
    uint32_t size;
    uint32_t reserved_01;
} __PACKED;

struct xhci_portmap {
    uint8_t             start;
    uint8_t             count;
};

struct xhci_ring {
    struct xhci_trb      ring[XHCI_RING_ITEMS];
    struct xhci_trb      evt;
    uint32_t             eidx;
    uint32_t             nidx;
    uint32_t             cs;
    usb_osal_mutex_t     lock;
};

struct xhci_pipe {
    struct xhci_ring     reqs; /* DO NOT MOVE reqs from structure beg */
    uint8_t              epaddr;
    uint8_t              speed;
    uint8_t              interval;
    uint8_t              eptype;
    uint16_t             maxpacket;
    uint32_t             slotid;
    uint32_t             epid;

    /* command or transfer waiter */
    int                  timeout; /* = 0 no need to wait */
    bool                 waiter;
    usb_osal_sem_t       waitsem;

    /* handle urb */
    struct usbh_hubport *hport;
    struct usbh_urb     *urb; /* NULL if no active URB */
};

struct xhci_s {
    /* devinfo */
    uint32_t             ports;
    uint32_t             slots;
    bool                 context64;
    struct xhci_portmap  usb2;
    struct xhci_portmap  usb3;

    /* xhci registers base addr */
    unsigned long        base; /* register base */
    unsigned long        caps; /* capability register base */
    unsigned long        op;   /* operational register base */
    unsigned long        pr;   /* port register base */
    unsigned long        ir;   /* interrupt runtime register base */
    unsigned long        db;   /* doorbell register base */
    unsigned long        xcap; /* extended capability */
    uint32_t             hcs[3]; /* capability cache */
    uint32_t             hcc;
    uint16_t             version; /* xhci version */

    /* xhci data structures */
    struct xhci_devlist  *devs;
    struct xhci_ring     *cmds;
    struct xhci_ring     *evts;
    struct xhci_er_seg   *eseg;
};

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif