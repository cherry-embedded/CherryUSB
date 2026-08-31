/*
 * Copyright (c) 2026, CherryUSB contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "usbd_core.h"
#include "usb_dc_ch32x315.h"
#include "usb_ch32x315_usbhs_reg.h"

#define CH32X315_USBHS_SRAM_BASE 0x20000000UL
#define CH32X315_USBHS_SRAM_END  0x20010000UL
#define CH32X315_USBHS_DMA_MASK  0x00FFFFFFUL
#define CH32X315_EP_COUNT 8

struct ch32x315_ep_state {
    uint16_t mps;
    uint8_t type;
    bool enabled;
    bool stalled;
    uint8_t *buf;
    uint32_t remaining;
    uint32_t actual;
    uint16_t packet_len;
};

struct ch32x315_udc_state {
    USB_MEM_ALIGNX struct usb_setup_packet setup;
    struct ch32x315_ep_state in[CH32X315_EP_COUNT];
    struct ch32x315_ep_state out[CH32X315_EP_COUNT];
    uint8_t pending_address;
    bool pending_address_valid;
    bool tx_toggle[CH32X315_EP_COUNT];
    bool rx_toggle[CH32X315_EP_COUNT];
};

static struct ch32x315_udc_state g_udc;
static volatile USBHSD_TypeDef *g_usbhsd;
static uint8_t g_busid;

#define USBHSD g_usbhsd

/* Platform code must enable the USBHS peripheral clock/PHY and may provide
 * the IRQ wrapper. These hooks keep the DCD independent of a vendor SDK. */
__WEAK int usb_dc_ch32x315_low_level_init(uint8_t busid)
{
    (void)busid;
    return 0;
}

__WEAK int usb_dc_ch32x315_low_level_deinit(uint8_t busid)
{
    (void)busid;
    return 0;
}

__WEAK void usb_dc_ch32x315_bus_reset(void)
{
}

__WEAK void usb_dc_ch32x315_irq_enable(uint8_t busid)
{
    (void)busid;
}

__WEAK void usb_dc_ch32x315_irq_disable(uint8_t busid)
{
    (void)busid;
}

__WEAK void usb_dc_ch32x315_delay_ms(uint32_t ms)
{
    (void)ms;
}

static volatile uint32_t *ep_rx_dma(uint8_t ep)
{
    return (volatile uint32_t *)((uintptr_t)&USBHSD->UEP1_RX_DMA + (uintptr_t)(ep - 1U) * 4U);
}

static volatile uint32_t *ep_tx_dma(uint8_t ep)
{
    return (volatile uint32_t *)((uintptr_t)&USBHSD->UEP1_TX_DMA + (uintptr_t)(ep - 1U) * 4U);
}

static volatile uint32_t *ep_max_len(uint8_t ep)
{
    return (volatile uint32_t *)((uintptr_t)&USBHSD->UEP0_MAX_LEN + (uintptr_t)ep * 4U);
}

static volatile uint16_t *ep_tx_len(uint8_t ep)
{
    return (volatile uint16_t *)((uintptr_t)&USBHSD->UEP0_TX_LEN + (uintptr_t)ep * 4U);
}

static volatile uint8_t *ep_tx_ctrl(uint8_t ep)
{
    return (volatile uint8_t *)((uintptr_t)&USBHSD->UEP0_TX_CTRL + (uintptr_t)ep * 4U);
}

static volatile uint8_t *ep_rx_ctrl(uint8_t ep)
{
    return (volatile uint8_t *)((uintptr_t)&USBHSD->UEP0_RX_CTRL + (uintptr_t)ep * 4U);
}

static int usb_dma_addr_len(const void *ptr, uint32_t len, uint32_t *dma)
{
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t offset;
    if (!dma || (!ptr && len != 0U)) {
        return -1;
    }
    if (len == 0U && !ptr) {
        *dma = 0;
        return 0;
    }
    if ((addr & 0x03U) || addr < CH32X315_USBHS_SRAM_BASE || addr >= CH32X315_USBHS_SRAM_END ||
        (uintptr_t)len > (CH32X315_USBHS_SRAM_END - addr)) {
        return -1;
    }
    /* USBHS stores an SRAM offset in its 24-bit DMA field. The WCH reference
     * header reconstructs the CPU address by adding SRAM_BASE when reading
     * the register, so do the inverse conversion here explicitly. */
    offset = addr - CH32X315_USBHS_SRAM_BASE;
    if (offset > CH32X315_USBHS_DMA_MASK) {
        return -1;
    }
    *dma = (uint32_t)offset;
    return 0;
}

static void ep_clear_tx_done(uint8_t ep)
{
    volatile uint8_t *ctrl = ep_tx_ctrl(ep);
    *ctrl &= (uint8_t)~USBHS_UEP_T_DONE;
}

static void ep_clear_rx_done(uint8_t ep)
{
    volatile uint8_t *ctrl = ep_rx_ctrl(ep);
    *ctrl &= (uint8_t)~USBHS_UEP_R_DONE;
}

static void ep0_arm_setup(void)
{
    uint32_t dma;
    if (usb_dma_addr_len(&g_udc.setup, sizeof(g_udc.setup), &dma) != 0) {
        return;
    }
    USBHSD->UEP0_DMA = dma;
    g_udc.rx_toggle[0] = false;
    g_udc.tx_toggle[0] = false;
    g_udc.in[0].stalled = false;
    g_udc.out[0].stalled = false;
    *ep_tx_ctrl(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    *ep_rx_ctrl(0) = USBHS_UEP_R_RES_ACK | USBHS_UEP_R_TOG_DATA0;
}

static void ep_set_tx_res(uint8_t ep, uint8_t res)
{
    uint8_t ctrl = *ep_tx_ctrl(ep);
    ctrl = (uint8_t)((ctrl & (uint8_t)~USBHS_UEP_T_RES_MASK) | res);
    *ep_tx_ctrl(ep) = ctrl;
}

static void ep_set_rx_res(uint8_t ep, uint8_t res)
{
    uint8_t ctrl = *ep_rx_ctrl(ep);
    ctrl = (uint8_t)((ctrl & (uint8_t)~USBHS_UEP_R_RES_MASK) | res);
    *ep_rx_ctrl(ep) = ctrl;
}

int usb_dc_init(uint8_t busid)
{
    int ret;
    if (busid >= CONFIG_USBDEV_MAX_BUS || g_usbdev_bus[busid].reg_base == 0U) {
        return -1;
    }
    g_busid = busid;
    g_usbhsd = (volatile USBHSD_TypeDef *)g_usbdev_bus[busid].reg_base;
    memset(&g_udc, 0, sizeof(g_udc));
    ret = usb_dc_ch32x315_low_level_init(busid);
    if (ret != 0) {
        g_usbhsd = NULL;
        return ret;
    }

    USBHSD->CONTROL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
    USBHSD->INT_FG = 0xFF;
    USBHSD->INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND |
                     USBHS_UDIE_BUS_SLEEP | USBHS_UDIE_LPM_ACT |
                     USBHS_UDIE_TRANSFER | USBHS_UDIE_LINK_RDY;
#ifdef CONFIG_USB_HS
    USBHSD->BASE_MODE = USBHS_UD_SPEED_HIGH;
#else
    USBHSD->BASE_MODE = USBHS_UD_SPEED_FULL;
#endif
    USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
    USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
    g_udc.in[0].enabled = true;
    g_udc.out[0].enabled = true;
    g_udc.in[0].mps = USB_CTRL_EP_MPS;
    g_udc.out[0].mps = USB_CTRL_EP_MPS;
    *ep_max_len(0) = USB_CTRL_EP_MPS;
    *ep_tx_len(0) = 0;
    *ep_tx_ctrl(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA1;
    *ep_rx_ctrl(0) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
    USBHSD->CONTROL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN |
                      USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM;
    usb_dc_ch32x315_irq_enable(busid);
    ep0_arm_setup();
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    usb_dc_ch32x315_irq_disable(busid);
    USBHSD->CONTROL = USBHS_UD_RST_SIE | USBHS_UD_RST_LINK;
    g_usbhsd = NULL;
    return usb_dc_ch32x315_low_level_deinit(busid);
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    (void)busid;
    g_udc.pending_address = (uint8_t)(addr & USBHS_UD_DEV_ADDR);
    g_udc.pending_address_valid = true;
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    (void)busid;
    USBHSD->WAKE_CTRL |= USBHS_UD_REMOTE_WKUP;
    usb_dc_ch32x315_delay_ms(1);
    USBHSD->WAKE_CTRL &= (uint8_t)~USBHS_UD_REMOTE_WKUP;
    return 0;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    (void)busid;
    return (USBHSD->MIS_ST & USBHS_UDMS_HS_MOD) ? USB_SPEED_HIGH : USB_SPEED_FULL;
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t idx;
    uint8_t type;
    uint16_t mps;
    (void)busid;
    if (!ep) {
        return -1;
    }
    idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
    mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    if (idx >= CH32X315_EP_COUNT || mps == 0 || mps > 1024) {
        return -1;
    }
    *ep_max_len(idx) = mps;
    if (USB_EP_DIR_IS_IN(ep->bEndpointAddress)) {
        g_udc.in[idx].mps = mps;
        g_udc.in[idx].type = type;
        g_udc.in[idx].enabled = true;
        g_udc.in[idx].stalled = false;
        g_udc.tx_toggle[idx] = false;
        g_udc.in[idx].packet_len = 0;
        USBHSD->UEP_TX_EN |= (uint16_t)(1U << idx);
        *ep_tx_ctrl(idx) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    } else {
        g_udc.out[idx].mps = mps;
        g_udc.out[idx].type = type;
        g_udc.out[idx].enabled = true;
        g_udc.out[idx].stalled = false;
        g_udc.rx_toggle[idx] = false;
        g_udc.out[idx].packet_len = 0;
        USBHSD->UEP_RX_EN |= (uint16_t)(1U << idx);
        *ep_rx_ctrl(idx) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
    }
    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t idx = USB_EP_GET_IDX(ep);
    (void)busid;
    if (idx >= CH32X315_EP_COUNT) {
        return -1;
    }
    if (USB_EP_DIR_IS_IN(ep)) {
        USBHSD->UEP_TX_EN &= (uint16_t)~(1U << idx);
        g_udc.in[idx].enabled = false;
        g_udc.tx_toggle[idx] = false;
        g_udc.in[idx].packet_len = 0;
    } else {
        USBHSD->UEP_RX_EN &= (uint16_t)~(1U << idx);
        g_udc.out[idx].enabled = false;
        g_udc.rx_toggle[idx] = false;
        g_udc.out[idx].packet_len = 0;
    }
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t idx = USB_EP_GET_IDX(ep);
    (void)busid;
    if (idx >= CH32X315_EP_COUNT) {
        return -1;
    }
    if (USB_EP_DIR_IS_IN(ep)) {
        ep_set_tx_res(idx, USBHS_UEP_T_RES_STALL);
        g_udc.in[idx].stalled = true;
    } else {
        ep_set_rx_res(idx, USBHS_UEP_R_RES_STALL);
        g_udc.out[idx].stalled = true;
    }
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t idx = USB_EP_GET_IDX(ep);
    (void)busid;
    if (idx >= CH32X315_EP_COUNT) {
        return -1;
    }
    if (USB_EP_DIR_IS_IN(ep)) {
        g_udc.tx_toggle[idx] = false;
        *ep_tx_ctrl(idx) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
        g_udc.in[idx].stalled = false;
    } else {
        g_udc.rx_toggle[idx] = false;
        *ep_rx_ctrl(idx) = USBHS_UEP_R_RES_ACK | USBHS_UEP_R_TOG_DATA0;
        g_udc.out[idx].stalled = false;
    }
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t idx = USB_EP_GET_IDX(ep);
    (void)busid;
    if (!stalled || idx >= CH32X315_EP_COUNT) {
        return -1;
    }
    *stalled = USB_EP_DIR_IS_IN(ep) ? (uint8_t)g_udc.in[idx].stalled : (uint8_t)g_udc.out[idx].stalled;
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t idx = USB_EP_GET_IDX(ep);
    uint32_t dma;
    uint32_t packet_len;
    struct ch32x315_ep_state *state;
    (void)busid;
    if (!USB_EP_DIR_IS_IN(ep) || idx >= CH32X315_EP_COUNT) {
        return -1;
    }
    state = &g_udc.in[idx];
    if (!state->enabled || usb_dma_addr_len(data, data_len, &dma) != 0) {
        return -2;
    }
    state->buf = (uint8_t *)data;
    state->remaining = data_len;
    state->actual = 0;
    packet_len = data_len > state->mps ? state->mps : data_len;
    state->packet_len = (uint16_t)packet_len;
    if (packet_len) {
        if (idx == 0) {
            USBHSD->UEP0_DMA = dma;
        } else {
            *ep_tx_dma(idx) = dma;
        }
    }
    *ep_tx_len(idx) = (uint16_t)packet_len;
    *ep_tx_ctrl(idx) = (uint8_t)(USBHS_UEP_T_RES_ACK |
                                 (g_udc.tx_toggle[idx] ? USBHS_UEP_T_TOG_DATA1 : USBHS_UEP_T_TOG_DATA0));
    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t idx = USB_EP_GET_IDX(ep);
    uint32_t dma;
    struct ch32x315_ep_state *state;
    (void)busid;
    if (!USB_EP_DIR_IS_OUT(ep) || idx >= CH32X315_EP_COUNT) {
        return -1;
    }
    state = &g_udc.out[idx];
    if (idx != 0 && !state->enabled) {
        return -2;
    }
    if (usb_dma_addr_len(data, data_len, &dma) != 0) {
        return -3;
    }
    state->buf = data;
    state->remaining = data_len;
    state->actual = 0;
    if (idx == 0) {
        if (data_len) {
            USBHSD->UEP0_DMA = dma;
        }
        *ep_rx_ctrl(0) = USBHS_UEP_R_RES_ACK | (g_udc.rx_toggle[0] ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0);
    } else {
        if (data_len) {
            *ep_rx_dma(idx) = dma;
        }
        *ep_rx_ctrl(idx) = USBHS_UEP_R_RES_ACK | (g_udc.rx_toggle[idx] ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0);
    }
    return 0;
}

static void handle_in(uint8_t idx)
{
    struct ch32x315_ep_state *state = &g_udc.in[idx];
    /* Keep the programmed length in software. Some USBHS revisions may
     * clear TX_LEN as part of the DONE handshake, so reading it back here
     * would make completion accounting dependent on silicon behavior. */
    uint32_t packet_len = state->packet_len;
    ep_clear_tx_done(idx);
    ep_set_tx_res(idx, USBHS_UEP_T_RES_NAK);
    state->packet_len = 0;
    state->actual += packet_len;
    if (state->remaining >= packet_len) {
        state->remaining -= packet_len;
    } else {
        state->remaining = 0;
    }
    g_udc.tx_toggle[idx] = !g_udc.tx_toggle[idx];

    if (state->remaining) {
        uint32_t next = state->remaining > state->mps ? state->mps : state->remaining;
        uint32_t dma;
        state->buf += packet_len;
        if (usb_dma_addr_len(state->buf, next, &dma) == 0) {
            if (idx == 0) USBHSD->UEP0_DMA = dma; else *ep_tx_dma(idx) = dma;
            *ep_tx_len(idx) = (uint16_t)next;
            state->packet_len = (uint16_t)next;
            *ep_tx_ctrl(idx) = (uint8_t)(USBHS_UEP_T_RES_ACK |
                                         (g_udc.tx_toggle[idx] ? USBHS_UEP_T_TOG_DATA1 : USBHS_UEP_T_TOG_DATA0));
        } else {
            state->remaining = 0;
        }
        return;
    }

    usbd_event_ep_in_complete_handler(g_busid, (uint8_t)(idx | USB_EP_DIR_IN), state->actual);
    if (idx == 0 && usbd_get_ep0_next_state(g_busid) == USBD_EP0_STATE_SETUP) {
        ep0_arm_setup();
    }
}

static void handle_out(uint8_t idx)
{
    struct ch32x315_ep_state *state = &g_udc.out[idx];
    uint8_t rx_ctrl = *ep_rx_ctrl(idx);
    uint32_t packet_len = idx == 0 ? USBHSD->UEP0_RX_LEN : 0;
    if (idx > 0) {
        volatile uint16_t *rx_len = (volatile uint16_t *)((uintptr_t)&USBHSD->UEP1_RX_LEN + (uintptr_t)(idx - 1U) * 4U);
        packet_len = *rx_len;
    }
    /* A duplicate OUT packet can complete the endpoint after an ACK was
     * lost. Only consume packets whose DATA toggle matches the expected
     * value; re-arm the endpoint with the same toggle otherwise. */
    if (!(rx_ctrl & USBHS_UEP_R_TOG_MATCH)) {
        ep_clear_rx_done(idx);
        *ep_rx_ctrl(idx) = (uint8_t)(USBHS_UEP_R_RES_ACK |
                                     (g_udc.rx_toggle[idx] ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0));
        return;
    }
    ep_clear_rx_done(idx);
    ep_set_rx_res(idx, USBHS_UEP_R_RES_NAK);
    state->actual += packet_len;
    if (state->remaining >= packet_len) state->remaining -= packet_len; else state->remaining = 0;
    g_udc.rx_toggle[idx] = !g_udc.rx_toggle[idx];
    if (packet_len < state->mps || state->remaining == 0) {
        usbd_event_ep_out_complete_handler(g_busid, idx, state->actual);
        if (idx == 0 && packet_len == 0) {
            ep0_arm_setup();
        }
    } else {
        state->buf += packet_len;
        uint32_t dma;
        if (usb_dma_addr_len(state->buf, state->remaining, &dma) == 0) {
            *ep_rx_dma(idx) = dma;
            *ep_rx_ctrl(idx) = (uint8_t)(USBHS_UEP_R_RES_ACK |
                                         (g_udc.rx_toggle[idx] ? USBHS_UEP_R_TOG_DATA1 : USBHS_UEP_R_TOG_DATA0));
        }
    }
}

void USBD_IRQHandler(uint8_t busid)
{
    uint8_t flags = USBHSD->INT_FG;
    uint8_t status = USBHSD->INT_ST;
    uint8_t idx = status & USBHS_UDIS_EP_ID_MASK;
    (void)busid;

    if (flags & USBHS_UDIF_BUS_RST) {
        memset(&g_udc, 0, sizeof(g_udc));
        usb_dc_ch32x315_bus_reset();
        USBHSD->DEV_AD = 0;
        USBHSD->UEP_TX_EN = USBHS_UEP0_T_EN;
        USBHSD->UEP_RX_EN = USBHS_UEP0_R_EN;
        g_udc.in[0].enabled = true;
        g_udc.out[0].enabled = true;
        g_udc.in[0].mps = USB_CTRL_EP_MPS;
        g_udc.out[0].mps = USB_CTRL_EP_MPS;
        *ep_max_len(0) = USB_CTRL_EP_MPS;
        *ep_tx_ctrl(0) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA1;
        *ep_rx_ctrl(0) = USBHS_UEP_R_RES_NAK | USBHS_UEP_R_TOG_DATA0;
        USBHSD->INT_FG = USBHS_UDIF_BUS_RST;
        usbd_event_reset_handler(g_busid);
        /* The core reopens EP0 during reset handling; arm SETUP last. */
        ep0_arm_setup();
    }
    if ((flags & USBHS_UDIF_TRANSFER) && !(flags & USBHS_UDIF_BUS_RST) && idx < CH32X315_EP_COUNT) {
        USBHSD->INT_FG = USBHS_UDIF_TRANSFER;
        if (idx == 0 && (status & USBHS_UDIS_EP_DIR) == 0 && (*ep_rx_ctrl(0) & USBHS_UEP_R_SETUP_IS)) {
            ep_clear_rx_done(0);
            ep_set_rx_res(0, USBHS_UEP_R_RES_NAK);
            /* Every control transfer starts its data/status stage at DATA1. */
            g_udc.tx_toggle[0] = true;
            g_udc.rx_toggle[0] = true;
            usbd_event_ep0_setup_complete_handler(g_busid, (uint8_t *)&g_udc.setup);
        } else if (status & USBHS_UDIS_EP_DIR) {
            handle_in(idx);
        } else {
            handle_out(idx);
        }
    }
    if (flags & USBHS_UDIF_SUSPEND) {
        USBHSD->INT_FG = USBHS_UDIF_SUSPEND;
        if (USBHSD->MIS_ST & USBHS_UDMS_SUSPEND) usbd_event_suspend_handler(g_busid);
        else usbd_event_resume_handler(g_busid);
    }
    if (flags & USBHS_UDIF_BUS_SLEEP) USBHSD->INT_FG = USBHS_UDIF_BUS_SLEEP;
    if (flags & USBHS_UDIF_LPM_ACT) USBHSD->INT_FG = USBHS_UDIF_LPM_ACT;
    if (flags & USBHS_UDIF_LINK_RDY) {
        USBHSD->INT_FG = USBHS_UDIF_LINK_RDY;
        usbd_event_connect_handler(g_busid);
    }
    if (g_udc.pending_address_valid && usbd_get_ep0_next_state(g_busid) == USBD_EP0_STATE_SETUP) {
        USBHSD->DEV_AD = g_udc.pending_address;
        g_udc.pending_address_valid = false;
    }
}

void usb_dc_ch32x315_irq_handler(void)
{
    USBD_IRQHandler(g_busid);
}
