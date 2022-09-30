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
 * FilePath: usb_hc_xhci.c
 * Date: 2022-09-19 17:24:36
 * LastEditTime: 2022-09-19 17:24:36
 * Description:  This files is for xhci function implementation
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 */

/***************************** Include Files *********************************/
#include "usbh_core.h"
#include "usbh_hub.h"
#include "xhci_reg.h"
#include "usb_hc_xhci.h"

/************************** Constant Definitions *****************************/

/************************** Variable Definitions *****************************/
/* input is xhci speed */
static const char *speed_name[16] = {
    [ 0 ] = " - ",
    [ 1 ] = "Full",
    [ 2 ] = "Low",
    [ 3 ] = "High",
    [ 4 ] = "Super",
};

static const int speed_from_xhci[16] = {
    [ 0 ] = -1,
    [ 1 ] = USB_SPEED_FULL,
    [ 2 ] = USB_SPEED_LOW,
    [ 3 ] = USB_SPEED_HIGH,
    [ 4 ] = USB_SPEED_SUPER,
    [ 5 ... 15 ] = -1,
};

static const int speed_to_xhci[] = {
    [ USB_SPEED_FULL  ] = 1,
    [ USB_SPEED_LOW   ] = 2,
    [ USB_SPEED_HIGH  ] = 3,
    [ USB_SPEED_SUPER ] = 4,
};

static volatile struct xhci_pipe *cur_cmd_pipe = NULL; /* pass current command pipe to interrupt */
/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
void usb_hc_dcache_invalidate(void *addr, unsigned long len);

/*****************************************************************************/
static void xhci_dump_slot_ctx(const struct xhci_slotctx *const sc)
{
    USB_LOG_DBG("ctx[0]=0x%x\n", sc->ctx[0]);
    USB_LOG_DBG(" route=0x%x\n", 
                 XHCI_SLOTCTX_0_ROUTE_GET(sc->ctx[0]));
    USB_LOG_DBG("ctx[1]=0x%x\n", sc->ctx[1]);
    USB_LOG_DBG("ctx[2]=0x%x\n", sc->ctx[2]);
    USB_LOG_DBG(" tt-port=0x%x, tt-hub-slot=0x%x\n", 
                 XHCI_SLOTCTX_2_HUB_PORT_GET(sc->ctx[2]),
                 XHCI_SLOTCTX_2_HUB_SLOT_GET(sc->ctx[2]));
    USB_LOG_DBG("ctx[3]=0x%x\n", sc->ctx[3]);
}

static void xhci_dump_ep_ctx(const struct xhci_epctx *const ec)
{
    USB_LOG_DBG("ctx[0]=0x%x\n", ec->ctx[0]);
    USB_LOG_DBG("ctx[1]=0x%x\n", ec->ctx[1]);
    USB_LOG_DBG(" ep_type=%d, mps=%d\n", 
                XHCI_EPCTX_1_EPTYPE_GET(ec->ctx[1]),
                XHCI_EPCTX_1_MPS_GET(ec->ctx[1]));
    USB_LOG_DBG("deq_low=0x%x\n", ec->deq_low);
    USB_LOG_DBG("deq_high=0x%x\n", ec->deq_high);
    USB_LOG_DBG("length=0x%x\n", ec->length);
}

static void xhci_dump_input_ctx(struct xhci_s *xhci, const struct xhci_inctx *const inctx)
{
    USB_LOG_DBG("===input ctx====\n");
    USB_LOG_DBG("add=0x%x\n", inctx->add);
    USB_LOG_DBG("del=0x%x\n", inctx->del);
    if (inctx->add & (0x1)) {
        USB_LOG_DBG("===slot ctx====\n");
        const struct xhci_slotctx *const sc = (void*)&inctx[1 << xhci->context64];
        xhci_dump_slot_ctx(sc);
    }

    for (int epid = 1; epid <= 31; epid++){
        if (inctx->add & (0x1 << (epid)))
        {
            USB_LOG_DBG("===ep-%d ctx====\n", epid);
            const struct xhci_epctx *const ec = (void*)&inctx[(epid+1) << xhci->context64];
            xhci_dump_ep_ctx(ec);
        }
    }
    USB_LOG_DBG("\n");
}

static void xhci_dump_pipe(const struct xhci_pipe *const ppipe)
{
    USB_LOG_DBG("pipe@%p\n", ppipe);
    if (NULL == ppipe)
        return;

    USB_LOG_DBG("epaddr=%d\n", ppipe->epaddr);
    USB_LOG_DBG("speed=%d\n", ppipe->speed);
    USB_LOG_DBG("interval=%d\n", ppipe->interval);
    USB_LOG_DBG("maxpacket=%d\n", ppipe->maxpacket);
    USB_LOG_DBG("eptype=%d\n", ppipe->eptype);
    USB_LOG_DBG("slotid=%d\n", ppipe->slotid);
    USB_LOG_DBG("epid=%d\n", ppipe->epid);
    USB_LOG_DBG("timeout=%d\n", ppipe->timeout);
    USB_LOG_DBG("waiter=%d\n", ppipe->waiter);
    USB_LOG_DBG("waitsem=%p\n", ppipe->waitsem);
    USB_LOG_DBG("hport=%p\n", ppipe->hport);
    USB_LOG_DBG("urb=%p\n", ppipe->urb);
}

/* Wait until bit = value or timeout */
static int wait_bit(unsigned long reg_off, uint32_t mask, uint32_t value, uint32_t timeout)
{
    uint32_t delay = 0U; /* calc wait end time */

    while ((readl(reg_off) & mask) != value) {
        if (++delay > timeout) { /* check if timeout */
            USB_LOG_ERR("wait timeout !!!\n");
            return -1;
        }
        usb_osal_msleep(1U);
    }
    return 0;
}

/* Check if device attached to port */
static void xhci_print_port_state(const char *prefix, uint32_t port, uint32_t portsc)
{
    uint32_t pls = XHCI_REG_OP_PORTS_PORTSC_PLS_GET(portsc);
    uint32_t speed = XHCI_REG_OP_PORTS_PORTSC_PORT_SPEED_GET(portsc);

    USB_LOG_DBG("%s port #%d: 0x%08x,%s%s pls %d, speed %d [%s]\n",
            prefix, port + 1, portsc,
            (portsc & XHCI_REG_OP_PORTS_PORTSC_PP)  ? " powered," : "",
            (portsc & XHCI_REG_OP_PORTS_PORTSC_PED) ? " enabled," : "",
            pls, speed, speed_name[speed]);
}

static inline uint32_t xhci_readl_port(struct xhci_s *xhci, uint32_t port, uint32_t offset)
{
    /* Operational Base + (400h + (10h * (n–1))) */
    return readl(xhci->pr + port * XHCI_REG_OP_PORTS_SIZE + offset);
}

static inline void xhci_writel_port(struct xhci_s *xhci, uint32_t port, uint32_t offset, uint32_t val)
{
    /* Operational Base + (400h + (10h * (n–1))) */
    return writel(xhci->pr + port * XHCI_REG_OP_PORTS_SIZE + offset, val);
}

static void xhci_setup_mmio(struct xhci_s *xhci, unsigned long base_addr)
{
    xhci->base = base_addr;
    xhci->caps = xhci->base;

    /* add to register base to find the beginning of the Operational Register Space */
    xhci->op = xhci->base + readb(xhci->caps + XHCI_REG_CAP_CAPLENGTH);
    xhci->db = xhci->base + XHCI_REG_CAP_DBOFF_GET(readl(xhci->caps + XHCI_REG_CAP_DBOFF));
    xhci->ir = xhci->base + XHCI_REG_CAP_RTSOFF_GET(readl(xhci->caps + XHCI_REG_CAP_RTSOFF)) +
               XHCI_REG_RT_IR0;
    xhci->pr = xhci->op + XHCI_REG_OP_PORTS_BASE;

    /* cache static information of CAP_HCSPARAMS */
    xhci->hcs[0] = readl(xhci->caps + XHCI_REG_CAP_HCS1);
    xhci->hcs[1] = readl(xhci->caps + XHCI_REG_CAP_HCS2);
    xhci->hcs[2] = readl(xhci->caps + XHCI_REG_CAP_HCS3);
    xhci->hcc = readl(xhci->caps + XHCI_REG_CAP_HCC);

    /* avoid access XHCI_REG_CAP_HCIVERSION = 0x2, unaligned memory  */
    xhci->version = ((readl(xhci->caps + XHCI_REG_CAP_CAPLENGTH) >> 16) & 0xffff);

    xhci->ports = XHCI_REG_CAP_HCS1_MAX_PORTS_GET(xhci->hcs[0]); /* bit[31:24] max ports */
    xhci->slots = XHCI_REG_CAP_HCS1_MAX_SLOTS_GET(xhci->hcs[0]); /* bit[7:0] max device slots */
    
    /* For example, using the offset of Base is 1000h and the xECP value of 0068h, we can calculated 
        the following effective address of the first extended capability:
        1000h + (0068h << 2) -> 1000h + 01A0h -> 11A0h */  
    xhci->xcap = XHCI_REG_CAP_HCC_XECP_GET(xhci->hcc) << 2; /* bit[31:16] xHCI extended cap pointer */
    xhci->context64 = (XHCI_REG_CAP_HCC_CSZ & xhci->hcc) ? true : false;

    USB_LOG_INFO(" hc version: 0x%x\n", xhci->version);
    USB_LOG_INFO(" mmio base: 0x%x\n", xhci->base);
    USB_LOG_INFO(" oper base: 0x%x\n", xhci->op);
    USB_LOG_INFO(" doorbell base: 0x%x\n", xhci->db);
    USB_LOG_INFO(" runtime base: 0x%x\n", xhci->ir);
    USB_LOG_INFO(" port base: 0x%x\n", xhci->pr); 
    USB_LOG_INFO(" xcap base: 0x%x\n", xhci->xcap); 

    USB_LOG_INFO(" slot num: 0x%x\n", xhci->slots); 
    USB_LOG_INFO(" port num: 0x%x\n", xhci->ports); 
    USB_LOG_INFO(" context: %d bit\n", xhci->context64 ? 64 : 32);
}


/* Reset device on port */
static int xhci_hub_reset(struct xhci_s *xhci, uint32_t port)
{
    uint32_t portsc = xhci_readl_port(xhci, port, XHCI_REG_OP_PORTS_PORTSC);
    if (!(portsc & XHCI_REG_OP_PORTS_PORTSC_CCS)) /* double check if connected */
        /* Device no longer connected?! */
        return -1;

    switch (XHCI_REG_OP_PORTS_PORTSC_PLS_GET(portsc)) {
    case PLS_U0:
        /* A USB3 port - controller automatically performs reset */
        break;
    case PLS_POLLING:
        /* A USB2 port - perform device reset */
        xhci_print_port_state(__func__, port, portsc);
        xhci_writel_port(xhci, port, XHCI_REG_OP_PORTS_PORTSC, 
                         portsc | XHCI_REG_OP_PORTS_PORTSC_PR); /* reset port */
        break;
    default:
        return -1;
    }

    // Wait for device to complete reset and be enabled
    uint32_t end = 1000U, start = 0U;
    for (;;) {
        portsc = xhci_readl_port(xhci, port, XHCI_REG_OP_PORTS_PORTSC);
        if (!(portsc & XHCI_REG_OP_PORTS_PORTSC_CCS))
            /* Device disconnected during reset */
            return -1;
        
        if (portsc & XHCI_REG_OP_PORTS_PORTSC_PED) /* check if port enabled */
            /* Reset complete */
            break;

        if (++start > end) {
            USB_LOG_ERR("wait timeout, portsc=0x%x!!!\n", portsc);
            return -1;
        }

        usb_osal_msleep(1);
    }

    int rc = speed_from_xhci[XHCI_REG_OP_PORTS_PORTSC_PORT_SPEED_GET(portsc)];
    xhci_print_port_state("XHCI", port, portsc);
    return rc;
}

/* Add a TRB to the given ring */
static void xhci_trb_fill(struct xhci_ring *ring, void *data, uint32_t xferlen, uint32_t flags)
{
    struct xhci_trb *dst = &ring->ring[ring->nidx];
    if (flags & TRB_TR_IDT) {
        memcpy(&dst->ptr_low, data, xferlen);
    } else {
        dst->ptr_low = (uint32_t)(unsigned long)data;
#ifdef XHCI_AARCH64
        dst->ptr_high = (uint32_t)((uint64_t)data >> 32U);
#else
        dst->ptr_high = 0U;
#endif
    }
    dst->status = xferlen;
    /* cycle bit */
    dst->control = flags | (ring->cs ? TRB_C : 0);
}

/* Queue a TRB onto a ring, wrapping ring as needed */
static void xhci_trb_queue(struct xhci_ring *ring, void *data, uint32_t xferlen, uint32_t flags)
{
    if (ring->nidx >= ARRAY_SIZE(ring->ring) - 1) { /* if it is the last trb in the list */
        /* indicate the end of ring, bit[1], toggle cycle = 1, xHc shall toggle cycle bit interpretation */
        xhci_trb_fill(ring, ring->ring, 0, (TR_LINK << 10) | TRB_LK_TC); 
        ring->nidx = 0; /* adjust dequeue index to 0 */
        ring->cs ^= 1; /* toggle cycle interpretation of sw */
    }

    xhci_trb_fill(ring, data, xferlen, flags);
    ring->nidx++; /* ahead dequeue index */
}

/* Signal the hardware to process events on a TRB ring */
static void xhci_doorbell(struct xhci_s *xhci, uint32_t slotid, uint32_t value)
{
    writel(xhci->db + slotid * XHCI_REG_DB_SIZE, value); /* bit[7:0] db target, is ep_id */
}

/* Wait for a ring to empty (all TRBs processed by hardware) */
static int xhci_event_wait(struct xhci_s *xhci,
                           struct xhci_pipe *pipe,
                           struct xhci_ring *ring)
{
    int ret = CC_SUCCESS;

    if (pipe->timeout > 0){
        ret = usb_osal_sem_take(pipe->waitsem, pipe->timeout);
        if (ret < 0) {
            ret = CC_TIMEOUT;
        } else {
            ret = TRB_CC_GET(ring->evt.status); /* bit[31:24] completion code */ 
        }
    }

    return ret;
}

/* Submit a command to the xhci controller ring */
static int xhci_cmd_submit(struct xhci_s *xhci, struct xhci_inctx *inctx, 
                           struct xhci_pipe *pipe, uint32_t flags)
{
    if (inctx) {
        struct xhci_slotctx *slot = (void*)&inctx[1 << xhci->context64];
        uint32_t port = XHCI_SLOTCTX_1_ROOT_PORT_GET(slot->ctx[1]) - 1;
        uint32_t portsc = xhci_readl_port(xhci, port, XHCI_REG_OP_PORTS_PORTSC);
        if (!(portsc & XHCI_REG_OP_PORTS_PORTSC_CCS)) {
            /* Device no longer connected?! */
            xhci_print_port_state(__func__, port, portsc);
            return CC_DISCONNECTED;
        }
    }

    usb_osal_mutex_take(xhci->cmds->lock); /* handle command one by one */

    pipe->timeout = 5000;
    pipe->waiter = true;
    cur_cmd_pipe = pipe;

    xhci_trb_queue(xhci->cmds, inctx, 0, flags);

    /* pass command trb to hardware */
    DSB();

    xhci_doorbell(xhci, 0, 0); /* 0 = db host controller, 0 = db targe hc command */
    int rc = xhci_event_wait(xhci, pipe, xhci->cmds);

    usb_osal_mutex_give(xhci->cmds->lock);

    return rc;
}

static int xhci_cmd_nop(struct xhci_s *xhci, struct xhci_pipe *pipe)
{
    USB_LOG_DBG("%s\n", __func__);
    return xhci_cmd_submit(xhci, NULL, pipe, TRB_TYPE_SET(CR_NOOP));
}

static int xhci_cmd_enable_slot(struct xhci_s *xhci, struct xhci_pipe *pipe)
{
    int cc = xhci_cmd_submit(xhci, NULL, pipe, TRB_TYPE_SET(CR_ENABLE_SLOT));
    if (cc != CC_SUCCESS)
        return cc;

    struct xhci_trb *evt = &(xhci->cmds->evt);
    return TRB_CR_SLOTID_GET(evt->control); /* bit [31:24] slot id */
}

static int xhci_cmd_disable_slot(struct xhci_s *xhci, struct xhci_pipe *pipe)
{
    USB_LOG_DBG("%s: slotid %d\n", __func__, pipe->slotid);
    return xhci_cmd_submit(xhci, NULL, pipe, TRB_TYPE_SET(CR_DISABLE_SLOT) | TRB_CR_SLOTID_SET(pipe->slotid));
}

static int xhci_cmd_address_device(struct xhci_s *xhci, struct xhci_pipe *pipe, struct xhci_inctx *inctx)
{
    USB_LOG_DBG("%s: slotid %d\n", __func__, pipe->slotid);
    return xhci_cmd_submit(xhci, inctx, pipe, TRB_TYPE_SET(CR_ADDRESS_DEVICE) | TRB_CR_SLOTID_SET(pipe->slotid));
}

static int xhci_cmd_configure_endpoint(struct xhci_s *xhci, struct xhci_pipe *pipe, 
                                       struct xhci_inctx *inctx, bool defconfig)
{
    USB_LOG_DBG("%s: slotid %d\n", __func__, pipe->slotid);
    return xhci_cmd_submit(xhci, inctx, pipe, TRB_TYPE_SET(CR_CONFIGURE_ENDPOINT) | 
                                              TRB_CR_SLOTID_SET(pipe->slotid) |
                                              (defconfig ? TRB_CR_DC : 0));
}

static int xhci_cmd_evaluate_context(struct xhci_s *xhci, struct xhci_pipe *pipe, struct xhci_inctx *inctx)
{
    USB_LOG_DBG("%s: slotid %d\n", __func__, pipe->slotid);
    /* bit[15:10] TRB type, bit[31:24] Slot id */
    return xhci_cmd_submit(xhci, inctx, pipe, TRB_TYPE_SET(CR_EVALUATE_CONTEXT) | TRB_CR_SLOTID_SET(pipe->slotid));
}
static int xhci_cmd_reset_endpoint(struct xhci_s *xhci, struct xhci_pipe *pipe)
{
    USB_LOG_DBG("%s: slotid %d, epid %d\n", __func__, pipe->slotid, pipe->epid);
    /* bit[15:10] TRB type, bit[31:24] Slot id */
    return xhci_cmd_submit(xhci, NULL, pipe, TRB_TYPE_SET(CR_RESET_ENDPOINT) | TRB_CR_SLOTID_SET(pipe->slotid) | 
                                              TRB_CR_EPID_SET(pipe->epid));
}

static int xhci_controller_configure(struct xhci_s *xhci)
{
    uint32_t reg;

    /* trbs */
    xhci->devs = usb_align(XHCI_ALIGMENT, sizeof(*xhci->devs) * (xhci->slots + 1)); /* device slot */
    xhci->eseg = usb_align(XHCI_ALIGMENT, sizeof(*xhci->eseg)); /* event segment */
    xhci->cmds = usb_align(XHCI_RING_SIZE, sizeof(*xhci->cmds)); /* command ring */
    xhci->evts = usb_align(XHCI_RING_SIZE, sizeof(*xhci->evts)); /* event ring */
    
    if (!xhci->devs || !xhci->cmds || !xhci->evts || !xhci->eseg) {
        USB_LOG_ERR("allocate memory failed !!!\n");
        goto fail;
    }

    memset(xhci->devs, 0U, sizeof(*xhci->devs) * (xhci->slots + 1));
    memset(xhci->cmds, 0U, sizeof(*xhci->cmds));
    memset(xhci->evts, 0U, sizeof(*xhci->evts));
    memset(xhci->eseg, 0U, sizeof(*xhci->eseg));

    xhci->cmds->lock = usb_osal_mutex_create();
    USB_ASSERT(xhci->cmds->lock);

    reg = readl(xhci->op + XHCI_REG_OP_USBCMD);
    if (reg & XHCI_REG_OP_USBCMD_RUN_STOP) { /* if xHc running, stop it first */
        reg &= ~XHCI_REG_OP_USBCMD_RUN_STOP;
        writel(xhci->op + XHCI_REG_OP_USBCMD, reg); /* stop xHc */

        if (wait_bit(xhci->op + XHCI_REG_OP_USBSTS, 
                     XHCI_REG_OP_USBSTS_HCH, XHCI_REG_OP_USBSTS_HCH, 32) != 0) /* wait xHc halt */
            goto fail;
    }

    USB_LOG_DBG("%s: resetting\n", __func__);
    
    writel(xhci->op + XHCI_REG_OP_USBCMD, XHCI_REG_OP_USBCMD_HCRST); /* reset xHc */
    if (wait_bit(xhci->op + XHCI_REG_OP_USBCMD, XHCI_REG_OP_USBCMD_HCRST, 0, 1000) != 0) /* wait reset process done */
        goto fail;

    if (wait_bit(xhci->op + XHCI_REG_OP_USBSTS, XHCI_REG_OP_USBSTS_CNR, 0, 1000) != 0) /* wait until xHc ready */
        goto fail;

    writel(xhci->op + XHCI_REG_OP_CONFIG, xhci->slots); /* bit[7:0], set max num of device slot enabled */
    writeq(xhci->op + XHCI_REG_OP_DCBAAP, (uint64_t)xhci->devs); /* bit[63:6] device context base address array pointer */
    writeq(xhci->op + XHCI_REG_OP_CRCR, (uint64_t)xhci->cmds | 1); /* command ring pointer, cycle state = 1 */
    xhci->cmds->cs = 1; /* cycle state = 1 */

    xhci->eseg->ptr_low =  (uint32_t)((unsigned long)xhci->evts); /* event ring pointer */
#ifdef XHCI_AARCH64
    xhci->eseg->ptr_high = (uint32_t)((unsigned long)xhci->evts >> 32U);
#else
    xhci->eseg->ptr_high = 0U;
#endif

    xhci->eseg->size = XHCI_RING_ITEMS; /* items of event ring TRB */
    writel(xhci->ir + XHCI_REG_RT_IR_ERSTSZ, 1); /* bit[15:0] event ring segment table size */
    writeq(xhci->ir + XHCI_REG_RT_IR_ERDP, (uint64_t)xhci->evts); /* bit[63:4] event ring dequeue pointer */
    writeq(xhci->ir + XHCI_REG_RT_IR_ERSTBA, (uint64_t)xhci->eseg); /* bit[63:6] event ring segment table base addr */
    xhci->evts->cs = 1; /* cycle state = 1 */

    reg = xhci->hcs[1];
    uint32_t spb = XHCI_REG_CAP_HCS2_MAX_SCRATCHPAD_BUFS_GET(reg); /* bit [25:21] |  bit [31:27] max scratchpad buffers */
    if (spb) {
        /* reserve scratchpad buffers for xHc */
        USB_LOG_DBG("%s: setup %d scratch pad buffers\n", __func__, spb);
        uint64_t *spba = usb_align(XHCI_ALIGMENT, sizeof(*spba) * spb); /* base addr of scratchpad buffers */
        void *pad = usb_align(CONFIG_XHCI_PAGE_SIZE, CONFIG_XHCI_PAGE_SIZE * spb); /* the whole scratchpad buffers */
        if (!spba || !pad) {
            USB_LOG_ERR("allocate memory failed !!!\n");
            usb_free(spba);
            usb_free(pad);
            goto fail;
        }
        for (uint32_t i = 0; i < spb; i++)
            spba[i] = (uint64_t)(pad + (i * CONFIG_XHCI_PAGE_SIZE)); /* assign base addr for each pad */
        xhci->devs[0].ptr_low = (uint32_t)((unsigned long)spba); /* bit[63:6] scratchpad buffers base addr */
#ifdef XHCI_AARCH64
        xhci->devs[0].ptr_high = (uint32_t)((uint64_t)spba >> 32U);
#else
        xhci->devs[0].ptr_high = 0U;
#endif
    }

    /* enable port interrupt */
    writel(xhci->ir + XHCI_REG_RT_IR_IMOD, 500U);
    reg = readl(xhci->ir + XHCI_REG_RT_IR_IMAN);
    reg |= XHCI_REG_RT_IR_IMAN_IE;
    writel(xhci->ir + + XHCI_REG_RT_IR_IMAN, reg);

    reg = readl(xhci->op + XHCI_REG_OP_USBCMD);
    reg |= XHCI_REG_OP_USBCMD_INTE; /* enable interrupt */
    writel(xhci->op + XHCI_REG_OP_USBCMD, reg);

    USB_LOG_DBG("Start xHc ....\n");

    reg = readl(xhci->op + XHCI_REG_OP_USBCMD);
    reg |= XHCI_REG_OP_USBCMD_RUN_STOP; /* start xHc */
    writel(xhci->op + XHCI_REG_OP_USBCMD, reg);

    return 0U; /* Success */
fail:
    USB_LOG_ERR("Configure Roothub failed !!!\n");
    usb_free(xhci->eseg);
    usb_free(xhci->evts);
    usb_free(xhci->cmds);
    usb_free(xhci->devs);
    return -1; /* Failed */
}

static void xhci_check_xcap(struct xhci_s *xhci)
{
    if (xhci->xcap) { /* read Extended Capabilities */
        uint32_t off;
        unsigned long addr = xhci->base + xhci->xcap; /* start read ext-cap */
    
        do {
            unsigned long xcap = addr;
            uint32_t ports, name, cap = readl(xcap + XHCI_REG_EXT_CAP_USBSPCF_OFFSET);
            switch (XHCI_REG_EXT_CAP_CAP_ID_GET(cap)) {
                case XHCI_EXT_CAP_ID_SUPPORT_PROTOCOL: /* 0x2, supported protocol */
                    name  = readl(xcap + XHCI_REG_EXT_CAP_USBSPCFDEF_OFFSET);
                    ports = readl(xcap + XHCI_REG_EXT_CAP_USBSPCFDEF2_OFFSET);
                    uint8_t major = XHCI_USBSPCF_MAJOR_REVERSION_GET(cap);
                    uint8_t minor = XHCI_USBSPCF_MINOR_REVERSION_GET(cap);
                    uint8_t count = XHCI_USBSPCFDEF2_COMPATIBLE_PORT_CNT_GET(ports);
                    uint8_t start = XHCI_USBSPCFDEF2_COMPATIBLE_PORT_OFF_GET(ports);
                    USB_LOG_INFO("XHCI    protocol %c%c%c%c %x.%02x"
                            ", %d ports (offset %d), def %x\n"
                            , (name >>  0) & 0xff
                            , (name >>  8) & 0xff
                            , (name >> 16) & 0xff
                            , (name >> 24) & 0xff /* Print string 'USB' */
                            , major, minor
                            , count, start
                            , ports >> 16);
                    if (name == XHCI_USBSPCFDEF_NAME_STRING_USB /* ASCII "USB " */) {
                        if (major == 2) {
                            xhci->usb2.start = start; /* USB 2.0 port range  */
                            xhci->usb2.count = count;
                        }
                        if (major == 3) {
                            xhci->usb3.start = start; /* USB 3.0 port range  */
                            xhci->usb3.count = count;
                        }
                    }
                    break;
                default:
                    USB_LOG_INFO("XHCI    extcap 0x%x @ %p\n", 
                                 XHCI_REG_EXT_CAP_CAP_ID_GET(cap), addr);
                    break;
                }               
            off = XHCI_REG_EXT_NEXT_CAP_PTR_GET(cap);
            addr += off << 2; /* addr of next ext-cap */
        } while (off > 0);
    }    
        
}

static int xhci_controller_setup(struct xhci_s *xhci, unsigned long baseaddr)
{
    USB_LOG_INFO("%s@0x%x\n", __func__, baseaddr);

    /* get register offset */
    xhci_setup_mmio(xhci, baseaddr);
    if (xhci->version < 0x96 || xhci->version > 0x120) {
        USB_LOG_ERR("xHCI-0x%x not support\n", xhci->version);
        return -1;
    }

    xhci_check_xcap(xhci);

    uint32_t pagesize = readl(xhci->op + XHCI_REG_OP_PAGESIZE); /* get page-size */
    USB_LOG_INFO("XHCI    page size %d \n", (pagesize << CONFIG_XHCI_PAGE_SHIFT));
    if (CONFIG_XHCI_PAGE_SIZE != (pagesize << CONFIG_XHCI_PAGE_SHIFT)) {
        USB_LOG_ERR("XHCI driver does not support page size code %d\n"
                , pagesize << CONFIG_XHCI_PAGE_SHIFT);
        return -1;
    }

    USB_LOG_INFO("config XHCI ....\n");
    if (xhci_controller_configure(xhci)) {
        USB_LOG_ERR("init XHCI failed !!!\n");
        return -1;
    } else {
        USB_LOG_INFO("init XHCI success !!!\n");
    }

    return 0;
}

static struct xhci_inctx *xhci_alloc_inctx_config_ep(struct xhci_s *xhci, struct usbh_hubport *hport, int maxepid)
{
    USB_ASSERT(xhci && hport);

    int size = (sizeof(struct xhci_inctx) * XHCI_INCTX_ENTRY_NUM) << xhci->context64;
    struct xhci_inctx *in = usb_align(XHCI_INCTX_ALIGMENT << xhci->context64, size);
    int devport = hport->port;

    if (!in) {
        USB_LOG_ERR("allocate memory failed !!!\n");
        return NULL;
    }
    memset(in, 0, size);

    struct xhci_slotctx *slot = (void*)&in[1 << xhci->context64]; /* slot context */
    slot->ctx[0]    |= XHCI_SLOTCTX_0_MAX_EPID_SET(maxepid); /* bit[31:27] index of the last valid ep context */
    slot->ctx[0]    |= XHCI_SLOTCTX_0_SPEED_SET(speed_to_xhci[hport->speed]); /* bit[23:20] speed of this device */

    /* Set high-speed hub flags. */
    struct usbh_hub *hubdev = hport->parent;
    USB_ASSERT(hubdev);
    if (!hubdev->is_roothub) { /* if device is not under roothub */
        struct usbh_hubport *hub_port = hubdev->parent;
        if (hport->speed == USB_SPEED_LOW || hport->speed == USB_SPEED_FULL) {
            if (hub_port->speed == USB_SPEED_HIGH) {
                slot->ctx[2] |= XHCI_SLOTCTX_2_HUB_SLOT_SET(hub_port->dev_addr); /* bit[7:0] parent hub slot id */
                slot->ctx[2] |= XHCI_SLOTCTX_2_HUB_PORT_SET(hport->port); /* bit[15:8] parent port num */
            } else {
                struct xhci_slotctx *hslot = (void*)(unsigned long)xhci->devs[hub_port->dev_addr].ptr_low;
                slot->ctx[2] = hslot->ctx[2]; /* 08h, copy hub slot context */
            }
        }

        uint32_t route = 0U;
        do {
            route <<= 4;
            route |= (hport->port) & 0xf; /* record port for each tire */

            hport = hubdev->parent;
            hubdev = hport->parent;
        } while ((!hubdev) || (!hubdev->is_roothub));

        slot->ctx[0]   |= XHCI_SLOTCTX_0_ROUTE_SET(route); /* bit[19:0] route string, max 5 tires */
    }

    /* refer to spec. Ports are numbered from 1 to MaxPorts. */
    slot->ctx[1]  |= XHCI_SLOTCTX_1_ROOT_PORT_SET(hport->port); /* bit[23:16] root hub port number */    

    return in;
}

/* allocate input context to update max packet size of endpoint-0  */
static struct xhci_inctx *xhci_alloc_inctx_set_ep_mps(struct xhci_s *xhci, uint32_t slotid, uint16_t ep_mps)
{
    USB_ASSERT(xhci);

    int size = (sizeof(struct xhci_inctx) * XHCI_INCTX_ENTRY_NUM) << xhci->context64;
    struct xhci_inctx *in = usb_align(XHCI_INCTX_ALIGMENT << xhci->context64, size);
    if (!in) {
        USB_LOG_ERR("allocate memory failed !!!\n");
        return NULL;
    }
    memset(in, 0, size);

    /* copy 32 entries after inctx controller field from devctx to inctx */
#ifdef XHCI_AARCH64
    struct xhci_slotctx *devctx = (struct xhci_slotctx *)(((uint64_t)xhci->devs[slotid].ptr_high << 32U) | 
                                                            ((uint64_t)xhci->devs[slotid].ptr_low));
#else
    struct xhci_slotctx *devctx = (struct xhci_slotctx *)(unsigned long)xhci->devs[slotid].ptr_low;
#endif
    struct xhci_slotctx *input_devctx = (void*)&in[1 << xhci->context64]; 

    memcpy(input_devctx, devctx, XHCI_SLOTCTX_ENTRY_NUM * sizeof(struct xhci_slotctx));

    in->add = (1 << 1); /* update ep0 context */
    in->del = (1 << 1);

    /*
        input ctrl context
        slot
        ep-0 context, offset = 2 * xhci->context64, e.g, for 64 bit (0x40), offset = 0x80 
    */
    struct xhci_epctx *ep = (void*)&in[2 << xhci->context64]; /* ep context */
    ep->ctx[1] |= XHCI_EPCTX_1_MPS_SET(ep_mps); /* bit[31:16] update maxpacket size */

    return in;
}

// Submit a USB "setup" message request to the pipe's ring
static void xhci_xfer_setup(struct xhci_s *xhci, struct xhci_pipe *pipe, 
                            bool dir_in, void *cmd, void *data, int datalen)
{
    /* SETUP TRB ctrl,  bit[15:10] trb type
                        bit[6] Immediate Data (IDT), parameters take effect
                        bit[17:16] Transfer type, 2 = OUT Data, 3 = IN Data */
    uint32_t trans_type = (datalen > 0) ? (dir_in ? TRB_TR_IN_DATA : TRB_TR_OUT_DATA): TRB_TR_NO_DATA;
    xhci_trb_queue(&pipe->reqs, cmd, USB_SIZEOF_SETUP_PACKET,
                   TRB_TYPE_SET(TR_SETUP) | TRB_TR_IDT | TRB_TR_TYPE_SET(trans_type));

    /* DATA TRB ctrl, bit[15:10] trb type
                      bit[16] Direction, 0 = OUT, 1 = IN */
    if (datalen) {
        xhci_trb_queue(&pipe->reqs, data, datalen,
                       TRB_TYPE_SET(TR_DATA) | (dir_in ? TRB_TR_DIR : 0));
    }

    /* STATUS TRB ctrl, bit[5] Interrupt On Completion (IOC).
                        bit[16] Direction, 0 = OUT, 1 = IN */
    xhci_trb_queue(&pipe->reqs, NULL, 0, 
                    TRB_TYPE_SET(TR_STATUS) | TRB_TR_IOC | ((dir_in ? 0 : TRB_TR_DIR)));

	/* pass command trb to hardware */
    DSB();
        
    /* notfiy xHc that device slot - epid */
    xhci_doorbell(xhci, pipe->slotid, pipe->epid);
}

/* Submit a USB transfer request to the pipe's ring */
static void xhci_xfer_normal(struct xhci_s *xhci, struct xhci_pipe *pipe,
                             void *data, int datalen)
{
    /* Normal trb, used in bulk and interrupt transfer */
    xhci_trb_queue(&pipe->reqs, data, datalen, TRB_TYPE_SET(TR_NORMAL) | TRB_TR_IOC);

	/* pass command trb to hardware */
    DSB();

    xhci_doorbell(xhci, pipe->slotid, pipe->epid);
}

/* -------------------------------------------------------------- */
/* port functions */
static struct xhci_s xhci_host;

__WEAK void usb_hc_low_level_init(void)
{
}

__WEAK void *usb_hc_malloc(size_t size)
{
    return NULL;
}

__WEAK void *usb_hc_malloc_align(size_t align, size_t size)
{
    return NULL;
}

__WEAK void usb_hc_free()
{
}

int usb_hc_init(void)
{
    usb_hc_low_level_init();

    memset(&xhci_host, 0, sizeof(xhci_host));
    if (xhci_controller_setup(&xhci_host, CONFIG_XHCI_BASE_ADDR))
        return -1;
    
    return 0;
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
    uint8_t nports;
    uint8_t port;
    uint32_t portsc;
    uint32_t status;
    int ret = 0;
    struct xhci_s *xhci = &xhci_host;

    nports = CONFIG_USBHOST_MAX_RHPORTS;

    port = setup->wIndex;
    if (setup->bmRequestType & USB_REQUEST_RECIPIENT_DEVICE) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_DESCRIPTOR:
                USB_LOG_ERR("HUB_REQUEST_GET_DESCRIPTOR not implmented \n");
                break;
            case HUB_REQUEST_GET_STATUS:
                memset(buf, 0, 4);
                break;
            default:
                break;
        }
    } else if (setup->bmRequestType & USB_REQUEST_RECIPIENT_OTHER) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                if (!port || port > nports) {
                    return -EPIPE;
                }

                portsc = xhci_readl_port(xhci, port - 1, XHCI_REG_OP_PORTS_PORTSC);

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_ENABLE:
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        portsc |= XHCI_REG_OP_PORTS_PORTSC_CSC;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        portsc |= XHCI_REG_OP_PORTS_PORTSC_PEC;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
                        break;
                    default:
                        return -EPIPE;
                }

                uint32_t pclear = portsc & XHCI_REG_OP_PORTS_PORTSC_RW_MASK;
                xhci_writel_port(xhci, port - 1, XHCI_REG_OP_PORTS_PORTSC, pclear); /* clear port status */

                break;
            case HUB_REQUEST_SET_FEATURE:
                if (!port || port > nports) {
                    return -EPIPE;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        ret = xhci_hub_reset(xhci, port - 1);
                        break;

                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -EPIPE;
                }
                
                portsc = xhci_readl_port(xhci, port - 1, XHCI_REG_OP_PORTS_PORTSC);

                status = 0;
                if (portsc & XHCI_REG_OP_PORTS_PORTSC_CSC) {
                    /* Port connection status changed */
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                }

                if (portsc & XHCI_REG_OP_PORTS_PORTSC_PEC) {
                    /* Port enabled status changed */
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }

                if (portsc & XHCI_REG_OP_PORTS_PORTSC_OCC) {
                    /* Port status changed due to over-current */
                    status |= (1 << HUB_PORT_FEATURE_C_OVER_CURREN);
                }

                if (portsc & XHCI_REG_OP_PORTS_PORTSC_CCS) {
                    /* Port connected */
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                }
                
                if (portsc & XHCI_REG_OP_PORTS_PORTSC_PED) {
                    /* Port enabled */
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);

                    const int speed = speed_from_xhci[XHCI_REG_OP_PORTS_PORTSC_PORT_SPEED_GET(portsc)];
                    if (speed == USB_SPEED_LOW) {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    } else if ((speed == USB_SPEED_HIGH) || (speed == USB_SPEED_SUPER) || 
                               (speed == USB_SPEED_FULL)) {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                }

                if (portsc & XHCI_REG_OP_PORTS_PORTSC_OCA) {
                    /* Over-current condition */
                    status |= (1 << HUB_PORT_FEATURE_OVERCURRENT);
                }

                if (portsc & XHCI_REG_OP_PORTS_PORTSC_PR) {
                    /* Reset is in progress */
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                }

                if (portsc & XHCI_REG_OP_PORTS_PORTSC_PP) {
                    /* Port is not power off */
                    status |= (1 << HUB_PORT_FEATURE_POWER);
                }
                memcpy(buf, &status, 4);
                break;
            default:
                break;
        }
    }

    return ret;
}

int usbh_get_xhci_devaddr(usbh_pipe_t *pipe)
{
    struct xhci_pipe *ppipe = (struct xhci_pipe *)pipe;
    return ppipe->slotid;
}

int usbh_ep0_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    int ret = 0;
    struct xhci_s *xhci = &xhci_host;
    struct xhci_pipe *ppipe = (struct xhci_pipe *)pipe;
    int oldmaxpacket = ppipe->maxpacket; /* original max packetsz */

    if (ep_mps != oldmaxpacket) {
        /* maxpacket has changed on control endpoint - update controller. */
        USB_LOG_DBG("update ep0 mps from %d to %d\r\n", oldmaxpacket, ep_mps);
        struct xhci_inctx *in = xhci_alloc_inctx_set_ep_mps(xhci, ppipe->slotid, ep_mps); /* allocate input context */
        if (!in)
            return -1;    
    
        usb_hc_dcache_invalidate(in, sizeof(struct xhci_inctx) * XHCI_INCTX_ENTRY_NUM);
        xhci_dump_input_ctx(xhci, in);
        
        int cc = xhci_cmd_evaluate_context(xhci, ppipe, in);
        if (cc != CC_SUCCESS) {
            USB_LOG_ERR("%s: reconf ctl endpoint: failed (cc %d) (mps %d => %d)\n",
                        __func__, cc, oldmaxpacket, ep_mps);
            ret = -1;
        } else {
            ppipe->maxpacket = ep_mps; /* mps update success */ 
        }

        usb_free(in);
    }

    return ret;
}

static int usbh_get_period(int speed, int interval)
{
    if (speed != USB_SPEED_HIGH){
        /* fls - find last (most-significant) bit set  */
        return  (interval <= 4) ? 0 : fls(interval);
    }

    return (interval <= 4) ? 0 : interval - 4;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    int ret = 0;
    struct xhci_s *xhci = &xhci_host;
    struct xhci_pipe *ppipe = usb_align(XHCI_RING_SIZE, sizeof(struct xhci_pipe));
    struct usbh_hubport *hport = ep_cfg->hport;

    uint32_t epid = 0U;
    uint8_t eptype = 0U;

    if (ppipe == NULL) {
        return -ENOMEM;
    }

    memset(ppipe, 0, sizeof(struct xhci_pipe));

    ppipe->epaddr = ep_cfg->ep_addr;
    if (ep_cfg->ep_addr == 0) {
        epid = 1; /* ep0, ep_id = 1 */
    } else {
        /* refer to spec. Figure 4-4: Endpoint Context Addressing
            ep0 = 1, ep1-out = 2, ep1-in = 3, ... ep15-out = 30, ep15-in = 31 */
        epid = (ep_cfg->ep_addr & 0x0f) * 2;
        epid += (ep_cfg->ep_addr & USB_EP_DIR_IN) ? 1 : 0;
    }
    ppipe->epid = epid;
    eptype = ep_cfg->ep_type;
    ppipe->eptype = eptype;
    ppipe->maxpacket = ep_cfg->ep_mps;
    ppipe->interval = ep_cfg->ep_interval;
    ppipe->speed = ep_cfg->hport->speed;
    ppipe->hport = ep_cfg->hport;
    ppipe->waitsem = usb_osal_sem_create(0);
    ppipe->waiter = false;
    ppipe->urb = NULL;

    USB_LOG_DBG("%s epid = %d, epaddr = 0x%x eptype = %d, mps = %d, speed = %d, urb = %p\n", 
                __func__, ppipe->epid, ppipe->epaddr, ppipe->eptype, ppipe->maxpacket, 
                ppipe->speed, ppipe->urb);

    ppipe->reqs.cs = 1; /* cycle state = 1 */     

    /* Allocate input context and initialize endpoint info. */
    struct xhci_inctx *in = xhci_alloc_inctx_config_ep(xhci, hport, epid);
    if (!in){
        ret = -1;
        goto fail;
    }

    /* add slot context and ep context */
    in->add = 0x01 /* Slot Context */ | (1 << epid); /* EP Context */
    struct xhci_epctx *ep = (void*)&in[(epid + 1) << xhci->context64];    
    if (eptype == USB_ENDPOINT_TYPE_INTERRUPT)
        ep->ctx[0] = XHCI_EPCTX_0_INTERVAL_SET(usbh_get_period(ppipe->speed, ppipe->interval) + 3); /* bit[23:16] for interrupt ep, set interval to control interrupt period */
        
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
    ep->ctx[1]   |= eptype << 3; /* bit[5:3] endpoint type */
    if (ppipe->epaddr & USB_EP_DIR_IN || eptype == USB_ENDPOINT_TYPE_CONTROL)
        ep->ctx[1] |= 1 << 5; /* ep_type 4 ~ 7 */
        
    ep->ctx[1]   |= XHCI_EPCTX_1_MPS_SET(ppipe->maxpacket); /* bit[31:16] max packet size */
    if (eptype == USB_ENDPOINT_TYPE_INTERRUPT)
        ep->ctx[1] |= XHCI_EPCTX_1_CERR_SET(3);

    ep->deq_low  = (uint32_t)((unsigned long)&ppipe->reqs.ring[0]); /* bit[63:4] tr dequeue pointer */
    ep->deq_low  |= 1;         /* bit[0] dequeue cycle state */
#ifdef XHCI_AARCH64
    ep->deq_high = (uint32_t)((uint64_t)&ppipe->reqs.ring[0] >> 32U);
#else
    ep->deq_high = 0U;
#endif

    if (eptype == USB_ENDPOINT_TYPE_BULK){
        ep->length   = XHCI_EPCTX_AVE_TRB_LEN_SET(256U); /* bit[15:0] average trb length */

    } else if (eptype == USB_ENDPOINT_TYPE_INTERRUPT){
        ep->length   = XHCI_EPCTX_AVE_TRB_LEN_SET(16U) | /* bit[15:0] average trb length */
                       XHCI_EPCTX_MAX_ESIT_SET(ppipe->maxpacket); /* bit[31:16] max ESIT payload */
    }

    if (ppipe->epid == 1) { /* when allocate ep-0, allocate device first */
        struct usbh_hub *hubdev = hport->parent;
        if (!hubdev->is_roothub){
            /* make sure parent hub has been configured */
            struct usbh_hubport *hubport = hubdev->parent;
            struct xhci_pipe *hubep0 = (struct xhci_pipe *)hubport->ep0;
            struct xhci_slotctx *hdslot = (void*)(unsigned long)xhci->devs[hubep0->slotid].ptr_low;
            if (XHCI_SLOTCTX_3_SLOT_STATE_GET(hdslot->ctx[3]) != XHCI_SLOT_CONFIG) { /* bit [31:27] slot state, 3 means configured */
                USB_LOG_ERR("%s parent hub-%d not yet configured\n", __func__, hubep0->slotid);
                ret = -1;
                goto fail;
            }
        }

        /* enable slot. */
        size_t size = (sizeof(struct xhci_slotctx) * XHCI_SLOTCTX_ENTRY_NUM) << xhci->context64;
        struct xhci_slotctx *dev = usb_align(XHCI_SLOTCTX_ALIGMENT << xhci->context64, size);
        if (!dev) {
            USB_LOG_ERR("allocate memory failed !!!\n");
            ret = -1;
            goto fail;
        }

        /* send nop command to test if command ring ok */
        for (int i = 0 ; i < 3; ++i) {
            int cc = xhci_cmd_nop(xhci, ppipe);
            if (cc != CC_SUCCESS) {
                USB_LOG_ERR("%s: nop: failed\n", __func__);
                usb_free(dev);
                ret = -1; 
                goto fail;              
            }
        }

        int slotid = xhci_cmd_enable_slot(xhci, ppipe); /* get slot-id */
        if (slotid < 0) {
            USB_LOG_ERR("%s: enable slot: failed\n", __func__);
            usb_free(dev);
            ret = -1;
            goto fail;
        } 

        ppipe->slotid = slotid;
        USB_LOG_DBG("%s: enable slot: got slotid %d\n", __func__, ppipe->slotid);
        memset(dev, 0, size);
        xhci->devs[slotid].ptr_low = (uint32_t)(unsigned long)dev; /* DCBAA */
#ifdef XHCI_AARCH64
        xhci->devs[slotid].ptr_high = (uint32_t)((uint64_t)dev >> 32U);
#else
        xhci->devs[slotid].ptr_high = 0;
#endif  

        usb_hc_dcache_invalidate(in, sizeof(struct xhci_inctx) * XHCI_INCTX_ENTRY_NUM);

        /* Send set_address command. */
        int cc = xhci_cmd_address_device(xhci, ppipe, in);
        if (cc != CC_SUCCESS) {
            USB_LOG_ERR("%s: address device: failed (cc %d)\n", __func__, cc);
            cc = xhci_cmd_disable_slot(xhci, ppipe);
            if (cc != CC_SUCCESS) {
                USB_LOG_ERR("%s: disable failed (cc %d)\n", __func__, cc);
                ret = -1;
                goto fail;
            }

            xhci->devs[slotid].ptr_low = 0; /* free DCBAA */
            xhci->devs[slotid].ptr_high = 0;
            usb_free(dev);

            ret = -1;
            goto fail;
        }          
    } else { /* when allocate other ep, config ep */
        struct xhci_pipe *defpipe = (struct xhci_pipe *)hport->ep0;
        ppipe->slotid = defpipe->slotid;

        /* reset if endpoint is not running */
#ifdef XHCI_AARCH64
        struct xhci_slotctx *devctx = (struct xhci_slotctx *)(((uint64_t)xhci->devs[ppipe->slotid].ptr_high << 32U) | 
                                                            ((uint64_t)xhci->devs[ppipe->slotid].ptr_low));
#else
        struct xhci_slotctx *devctx = (struct xhci_slotctx *)(unsigned long)xhci->devs[ppipe->slotid].ptr_low;
#endif
        struct xhci_epctx *epctx = (void*)&devctx[epid << xhci->context64]; /* ep0 context */
        uint32_t epstate = XHCI_EPCTX_0_EP_STATE_GET(epctx->ctx[0]);
        int cc;

        
		/* Reset endpoint in case it is not running */
        if (epstate > 1){
            cc = xhci_cmd_reset_endpoint(xhci, ppipe);
            if (cc != CC_SUCCESS) {
                USB_LOG_ERR("%s: reset endpoint: failed (cc %d)\n", __func__, cc);
                ret = -1;
                goto fail;
            } 
        }
 
        usb_hc_dcache_invalidate(in, sizeof(struct xhci_inctx) * XHCI_INCTX_ENTRY_NUM);
        xhci_dump_input_ctx(xhci, in);

        /* Send configure command. */
        cc = xhci_cmd_configure_endpoint(xhci, ppipe, in, false);
        if (cc != CC_SUCCESS) {
            epctx = (void*)&devctx[epid << xhci->context64]; /* ep0 context */
            epstate = XHCI_EPCTX_0_EP_STATE_GET(epctx->ctx[0]);
            USB_LOG_ERR("%s: configure endpoint: failed (cc %d, epstate %d)\n", __func__, cc, epstate);
            ret = -1;
            goto fail;
        }                
    }

    *pipe = (usbh_pipe_t)ppipe;

fail:
    usb_free(in);
    return ret;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    int ret = 0;
    struct xhci_s *xhci = &xhci_host;
    struct xhci_pipe *ppipe = (struct xhci_pipe *)pipe;

    if (!ppipe) {
        return -EINVAL;
    }

    struct usbh_urb  *urb = ppipe->urb;

    if (urb) {
        usbh_kill_urb(urb);
    }

    return -1;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    int ret = 0;
    if (!urb || !urb->pipe) {
        return -EINVAL;
    }

    struct xhci_s *xhci = &xhci_host;
    struct xhci_pipe *ppipe = urb->pipe;
    struct usb_setup_packet *setup = urb->setup;   
    size_t flags;

    flags = usb_osal_enter_critical_section();

    urb->errorcode = -EBUSY;
    urb->actual_length = 0U;

    ppipe->waiter = false;
    ppipe->urb = urb;
    ppipe->timeout = urb->timeout;

    if (ppipe->timeout > 0) {
        ppipe->waiter = true;
    }

    usb_osal_leave_critical_section(flags);
    switch (ppipe->eptype){
        case USB_ENDPOINT_TYPE_CONTROL:
            USB_ASSERT(setup);
            if (setup->bRequest == USB_REQUEST_SET_ADDRESS){
                /* Set address command sent during xhci_alloc_pipe. */
                goto skip_req;
            }

            USB_LOG_DBG("%s request-%d\n", __func__, setup->bRequest);
            /* send setup in/out for command */
            xhci_xfer_setup(xhci, ppipe, setup->bmRequestType & USB_EP_DIR_IN, (void*)setup, 
                            urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:      
        case USB_ENDPOINT_TYPE_BULK:
            xhci_xfer_normal(xhci, ppipe, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        default:
            USB_ASSERT(0U);
            break;    
    }

    /* wait all ring handled by xHc */
    int cc = xhci_event_wait(xhci, ppipe, &ppipe->reqs);
    if ((cc != CC_SUCCESS) && 
        !((cc == CC_TIMEOUT) && (ppipe->eptype == USB_ENDPOINT_TYPE_INTERRUPT))) {
        /* ignore transfer timeout for interrupt type */
        USB_LOG_ERR("%s: xfer failed (cc %d)\n", __func__, cc);
        ret = -1;
        urb->errorcode = cc;
        goto errout_timeout;
    }

skip_req:
errout_timeout:
    /* Timeout will run here */
    usbh_kill_urb(urb);
    return ret;     
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    return 0;
}

void USBH_IRQHandler(void)
{
    uint32_t reg, status;
    struct xhci_s *xhci = &xhci_host;
    struct xhci_ring *evts = xhci->evts;
    struct xhci_pipe *work_pipe = NULL;    

    USB_LOG_DBG("%s\n", __func__);

    status = readl(xhci->op + XHCI_REG_OP_USBSTS);
    status |= XHCI_REG_OP_USBSTS_EINT; /* clear interrupt status */
    writel(xhci->op + XHCI_REG_OP_USBSTS, status);

    reg = readl(xhci->ir + XHCI_REG_RT_IR_IMAN);
    reg |= XHCI_REG_RT_IR_IMAN_IP; /* ack interrupt */
    writel(xhci->ir + XHCI_REG_RT_IR_IMAN, reg);

    /* check and ack event */
    for (;;) {
        uint32_t nidx = evts->nidx; /* index of dequeue trb */
        uint32_t cs = evts->cs; /* cycle state toggle by xHc */
        struct xhci_trb *etrb = evts->ring + nidx; /* current trb */
        uint32_t control = etrb->control; /* trb control field */

        if ((control & TRB_C) != (cs ? 1 : 0)) /* if cycle state not toggle, no events need to handle */
            break;

        /* process event on etrb */
        uint32_t evt_type = TRB_TYPE_GET(control);
        uint32_t evt_cc = TRB_CC_GET(etrb->status); /* bit[31:24] completion code */
    
        if (ER_PORT_STATUS_CHANGE == evt_type) {
            /* bit[31:24] port id, the port num of root hub port that generated this event */
            uint32_t port = TRB_PORT_ID_GET(etrb->ptr_low) - 1;
            uint32_t portsc = xhci_readl_port(xhci, port, XHCI_REG_OP_PORTS_PORTSC); /* Read status */

            xhci_print_port_state(__func__, port, portsc);
            if (portsc & XHCI_REG_OP_PORTS_PORTSC_CSC) {
                usbh_roothub_thread_wakeup(port + 1); /* wakeup when connection status changed */
            }
        } else if ((ER_COMMAND_COMPLETE == evt_type) || (ER_TRANSFER_COMPLETE == evt_type)) {
            struct xhci_trb  *rtrb = (void*)(unsigned long)etrb->ptr_low;
            struct xhci_ring *ring = XHCI_RING(rtrb); /* to align addr is ring base */
            struct xhci_trb  *evt = &ring->evt; /* first event trb */
            uint32_t eidx = rtrb - ring->ring + 1; /* calculate current evt trb index */

            memcpy(evt, etrb, sizeof(*etrb)); /* copy current trb to cmd/transfer ring */
            ring->eidx = eidx;

            if (ER_COMMAND_COMPLETE == evt_type) {
                work_pipe = (struct xhci_pipe *)cur_cmd_pipe;
            } else if (ER_TRANSFER_COMPLETE == evt_type) {
                /* xhci_pipe begin with reqs ring, therefore we get pipe instance from reqs ring */
                work_pipe = (struct xhci_pipe *)(void *)ring;
            }

            if (work_pipe) {
                if (work_pipe->waiter) {
                    work_pipe->waiter = false;
                    usb_osal_sem_give(work_pipe->waitsem);
                }

                if (work_pipe->urb) {
                    struct usbh_urb *cur_urb = work_pipe->urb;
                    cur_urb->errorcode = evt_cc;
                    /* bit [23:0] TRB Transfer length, residual number of bytes not transferred
                            for OUT, is the value of (len of trb) - (data bytes transmitted), '0' means successful
                            for IN, is the value of (len of trb) - (data bytes received), 
                                if cc is Short Packet, value is the diff between expected trans size and actual recv bytes
                                if cc is other error, value is the diff between expected trans size and actual recv bytes */
                    cur_urb->actual_length += cur_urb->transfer_buffer_length - 
                                            TRB_TR_TRANS_LEN_SET(evt->status); /* bit [23:0] */                    
                }
            }            
        } else {
            USB_LOG_ERR("%s: unknown event, type %d, cc %d\n",
                    __func__, evt_type, evt_cc);
        }

        /* move ring index, notify xhci */
        nidx++; /* head to next trb */
        if (nidx == XHCI_RING_ITEMS) {
            nidx = 0; /* roll-back if reach end of list */
            cs = cs ? 0 : 1;
            evts->cs = cs; /* sw toggle cycle state */
        }
        evts->nidx = nidx;
        uint64_t erdp = (uint64_t)(evts->ring + nidx);
        writeq(xhci->ir + XHCI_REG_RT_IR_ERDP, erdp | XHCI_REG_RT_IR_ERDP_EHB); /* bit[63:4] update current event ring dequeue pointer */        
    }
  
    /* handle callbacks in interrupt */
    if ((work_pipe) && (work_pipe->urb)) {
        struct usbh_urb *cur_urb = work_pipe->urb;
        if (cur_urb->complete) {
            if (cur_urb->errorcode < 0) {
                cur_urb->complete(cur_urb->arg, cur_urb->errorcode);
            } else {
                cur_urb->complete(cur_urb->arg, cur_urb->actual_length);
            }
        } 
    }

    USB_LOG_DBG("%s exit\n", __func__);
    return;
}