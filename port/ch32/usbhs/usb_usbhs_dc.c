/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbd_core.h"
#include "usb_usbhs_reg.h"

#ifndef CONFIG_USBDEV_EP_NUM
#define CONFIG_USBDEV_EP_NUM 8
#endif

#define USBHSD           ((USBHSD_TypeDef *)g_usbdev_bus[busid].reg_base)
#define ENDP_MAX_LEN(ep) *((__IO uint32_t *)&(USBHSD->UEP0_MAX_LEN) + (ep))
#define ENDP_TX_LEN(ep)  *((__IO uint16_t *)&(USBHSD->UEP0_TX_LEN) + (ep) * 2)
#define ENDP_RX_LEN(ep)  *((__IO uint16_t *)&(USBHSD->UEP0_RX_LEN) + (ep) * 2)
#define ENDP_RX_SIZE(ep) *((__IO uint16_t *)&(USBHSD->UEP1_RX_SIZE) + (ep - 1) * 2)
#define ENDP_TX_CTRL(ep) *((__IO uint8_t *)&(USBHSD->UEP0_TX_CTRL) + (ep) * 4)
#define ENDP_RX_CTRL(ep) *((__IO uint8_t *)&(USBHSD->UEP0_RX_CTRL) + (ep) * 4)
#define ENDP_TX_DMA(ep)  *((__IO uint32_t *)&(USBHSD->UEP1_TX_DMA) + (ep - 1))
#define ENDP_RX_DMA(ep)  *((__IO uint32_t *)&(USBHSD->UEP1_RX_DMA) + (ep - 1))

struct ch32_usbhs_ep_state {
    uint16_t ep_mps;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

struct ch32_usbhs_udc {
    struct ch32_usbhs_ep_state ep_in[CONFIG_USBDEV_EP_NUM];
    struct ch32_usbhs_ep_state ep_out[CONFIG_USBDEV_EP_NUM];
    struct usb_setup_packet setup;
} g_ch32_usbhs_udc[CONFIG_USBDEV_MAX_BUS];

__WEAK void usb_dc_low_level_init(uint8_t busid)
{
}

__WEAK void usb_dc_low_level_deinit(uint8_t busid)
{
}

int usb_dc_init(uint8_t busid)
{
    usb_dc_low_level_init(busid);

    USBHSD->CONTROL = USBHS_UD_RST_LINK | USBHS_UD_PHY_SUSPENDM;
    USBHSD->INT_EN = USBHS_UDIE_BUS_RST | USBHS_UDIE_SUSPEND | USBHS_UDIE_BUS_SLEEP | USBHS_UDIE_LPM_ACT |
                     USBHS_UDIE_TRANSFER | USBHS_UDIE_LINK_RDY;

    USBHSD->UEP_TX_EN = 0;
    USBHSD->UEP_RX_EN = 0;
    USBHSD->UEP_TX_TOG_AUTO = 0;
    USBHSD->UEP_RX_TOG_AUTO = 0;
    USBHSD->UEP_TX_ISO = 0;
    USBHSD->UEP_RX_ISO = 0;
    USBHSD->BASE_MODE = USBHS_UD_SPEED_HIGH;
    USBHSD->CONTROL = USBHS_UD_DEV_EN | USBHS_UD_DMA_EN | USBHS_UD_LPM_EN | USBHS_UD_PHY_SUSPENDM;
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    USBHSD->CONTROL = USBHS_UD_RST_SIE | USBHS_UD_RST_LINK;
    usb_dc_low_level_deinit(busid);
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    USBHSD->DEV_AD = addr;
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    USBHSD->WAKE_CTRL |= USBHS_UD_REMOTE_WKUP;
    return 0;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    if (USBHSD->MIS_ST & USBHS_UDMS_HS_MOD) {
        return USB_SPEED_HIGH;
    } else if ((USBHSD->BASE_MODE & USBHS_UD_SPEED_TYPE) == USBHS_UD_SPEED_LOW) {
        return USB_SPEED_LOW;
    } else {
        return USB_SPEED_FULL;
    }
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t epid = USB_EP_GET_IDX(ep->bEndpointAddress);

    if (epid >= CONFIG_USBDEV_EP_NUM) {
        USB_LOG_ERR("Ep addr %02x overflow\r\n", ep->bEndpointAddress);
        return -1;
    }

    uint8_t ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
    uint16_t ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    uint32_t bit = 1 << epid;

    if (USB_EP_DIR_IS_IN(ep->bEndpointAddress)) {
        g_ch32_usbhs_udc[busid].ep_in[epid].ep_mps = ep_mps;
        USBHSD->UEP_TX_EN |= bit;
        USBHSD->UEP_TX_TOG_AUTO |= bit;
        USBHSD->UEP_TX_ISO = ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS ? USBHSD->UEP_TX_ISO | bit : USBHSD->UEP_TX_ISO & ~bit;
        if (ep_type == USB_ENDPOINT_TYPE_BULK) {
            ENDP_MAX_LEN(epid) = ep_mps;
            // No send ZLP.
            USBHSD->UEP_TX_BURST_MODE |= bit;
            USBHSD->UEP_TX_BURST |= bit;
        } else {
            USBHSD->UEP_TX_BURST &= ~bit;
        }
        ENDP_TX_CTRL(epid) = USBHS_UEP_T_RES_NAK;
    } else {
        g_ch32_usbhs_udc[busid].ep_out[epid].ep_mps = ep_mps;
        ENDP_MAX_LEN(epid) = ep_mps;
        USBHSD->UEP_RX_EN |= bit;
        USBHSD->UEP_RX_TOG_AUTO |= bit;
        USBHSD->UEP_RX_ISO = ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS ? USBHSD->UEP_RX_ISO | bit : USBHSD->UEP_RX_ISO & ~bit;
        if (ep_type == USB_ENDPOINT_TYPE_BULK) {
            ENDP_MAX_LEN(epid) = ep_mps;
            // Response NYTE.
            USBHSD->UEP_RX_RES_MODE |= bit;
            USBHSD->UEP_RX_BURST |= bit;
        } else {
            USBHSD->UEP_RX_BURST &= ~bit;
        }
        ENDP_RX_CTRL(epid) = USBHS_UEP_R_RES_NAK;
    }

    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t epid = USB_EP_GET_IDX(ep);
    uint32_t bit = 1 << epid;
    if (USB_EP_DIR_IS_IN(ep)) {
        USBHSD->UEP_TX_EN &= ~bit;
        USBHSD->UEP_TX_TOG_AUTO &= ~bit;
    } else {
        USBHSD->UEP_RX_EN &= ~bit;
        USBHSD->UEP_RX_TOG_AUTO &= ~bit;
    }
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
        ENDP_RX_CTRL(ep_idx) = (ENDP_RX_CTRL(ep_idx) & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_STALL;
    } else {
        ENDP_TX_CTRL(ep_idx) = (ENDP_TX_CTRL(ep_idx) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_STALL;
    }
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
        ENDP_RX_CTRL(ep_idx) = USBHS_UEP_R_RES_ACK | USBHS_UEP_R_TOG_DATA0;
    } else {
        ENDP_TX_CTRL(ep_idx) = USBHS_UEP_T_RES_NAK | USBHS_UEP_T_TOG_DATA0;
    }
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
        *stalled = (ENDP_RX_CTRL(USB_EP_GET_IDX(ep)) & USBHS_UEP_R_RES_MASK) == USBHS_UEP_R_RES_STALL;
    } else {
        *stalled = (ENDP_TX_CTRL(USB_EP_GET_IDX(ep)) & USBHS_UEP_T_RES_MASK) == USBHS_UEP_T_RES_STALL;
    }
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!(USBHSD->UEP_TX_EN & (1 << ep_idx))) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }
    if ((ENDP_TX_CTRL(ep_idx) & USBHS_UEP_T_RES_MASK) != USBHS_UEP_T_RES_NAK) {
        return -4;
    }

    g_ch32_usbhs_udc[busid].ep_in[ep_idx].xfer_len = data_len;
    g_ch32_usbhs_udc[busid].ep_in[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        USBHSD->UEP0_DMA = (uint32_t)data;
    } else {
        ENDP_TX_DMA(ep_idx) = (uint32_t)data;
    }

    if (USBHSD->UEP_TX_BURST & (1 << ep_idx)) {
        ENDP_TX_LEN(ep_idx) = data_len;
    } else {
        ENDP_TX_LEN(ep_idx) = MIN(data_len, g_ch32_usbhs_udc[busid].ep_in[ep_idx].ep_mps);
    }
    ENDP_TX_CTRL(ep_idx) = (ENDP_TX_CTRL(ep_idx) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!(USBHSD->UEP_RX_EN & (1 << ep_idx))) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }

    g_ch32_usbhs_udc[busid].ep_out[ep_idx].xfer_len = data_len;
    g_ch32_usbhs_udc[busid].ep_out[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        USBHSD->UEP0_DMA = (uint32_t)data;
    } else {
        ENDP_RX_DMA(ep_idx) = (uint32_t)data;
    }

    if (USBHSD->UEP_RX_BURST & (1 << ep_idx)) {
        ENDP_RX_SIZE(ep_idx) = data_len;
    } else {
        ENDP_RX_SIZE(ep_idx) = MIN(data_len, g_ch32_usbhs_udc[busid].ep_out[ep_idx].ep_mps);
    }
    ENDP_RX_CTRL(ep_idx) = (ENDP_RX_CTRL(ep_idx) & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
    return 0;
}

static inline void handle_ep0_in(uint8_t busid)
{
    if (g_ch32_usbhs_udc[busid].setup.bmRequestType & 0x80) {
        USBHSD->UEP0_TX_CTRL ^= USBHS_UEP_T_TOG_DATA1;
        ENDP_TX_CTRL(0) = (ENDP_TX_CTRL(0) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;

        if (g_ch32_usbhs_udc[busid].ep_in[0].xfer_len > g_ch32_usbhs_udc[busid].ep_in[0].ep_mps) {
            g_ch32_usbhs_udc[busid].ep_in[0].xfer_len -= g_ch32_usbhs_udc[busid].ep_in[0].ep_mps;
            g_ch32_usbhs_udc[busid].ep_in[0].actual_xfer_len += g_ch32_usbhs_udc[busid].ep_in[0].ep_mps;
            usbd_event_ep_in_complete_handler(0, 0x80, g_ch32_usbhs_udc[busid].ep_in[0].actual_xfer_len);
        } else {
            g_ch32_usbhs_udc[busid].ep_in[0].actual_xfer_len += g_ch32_usbhs_udc[busid].ep_in[0].xfer_len;
            g_ch32_usbhs_udc[busid].ep_in[0].xfer_len = 0;
            usbd_event_ep_in_complete_handler(0, 0x80, g_ch32_usbhs_udc[busid].ep_in[0].actual_xfer_len);
        }
    } else {
        USBHSD->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc[busid].setup;
        ENDP_TX_CTRL(0) = (ENDP_TX_CTRL(0) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;
        ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
    }
}

static inline void handle_non_ep0_in(uint8_t busid, uint8_t epid)
{
    ENDP_TX_CTRL(epid) = (ENDP_TX_CTRL(epid) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_NAK;

    if (USBHSD->UEP_TX_BURST & (1 << epid)) {
        usbd_event_ep_in_complete_handler(0, 0x80 | epid, g_ch32_usbhs_udc[busid].ep_in[epid].xfer_len);
    } else if (g_ch32_usbhs_udc[busid].ep_in[epid].xfer_len > g_ch32_usbhs_udc[busid].ep_in[epid].ep_mps) {
        g_ch32_usbhs_udc[busid].ep_in[epid].xfer_len -= g_ch32_usbhs_udc[busid].ep_in[epid].ep_mps;
        g_ch32_usbhs_udc[busid].ep_in[epid].actual_xfer_len += g_ch32_usbhs_udc[busid].ep_in[epid].ep_mps;

        uint32_t write_count = MIN(g_ch32_usbhs_udc[busid].ep_in[epid].xfer_len, g_ch32_usbhs_udc[busid].ep_in[epid].ep_mps);
        ENDP_TX_LEN(epid) = write_count;
        ENDP_TX_DMA(epid) += g_ch32_usbhs_udc[busid].ep_in[epid].ep_mps;
        ENDP_TX_CTRL(epid) = (ENDP_TX_CTRL(epid) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
    } else {
        g_ch32_usbhs_udc[busid].ep_in[epid].actual_xfer_len += g_ch32_usbhs_udc[busid].ep_in[epid].xfer_len;
        g_ch32_usbhs_udc[busid].ep_in[epid].xfer_len = 0;
        usbd_event_ep_in_complete_handler(0, 0x80 | epid, g_ch32_usbhs_udc[busid].ep_in[epid].actual_xfer_len);
    }
}

static inline void handle_ep0_out(uint8_t busid)
{
    USBHSD->UEP0_RX_CTRL ^= USBHS_UEP_R_TOG_DATA1;

    uint32_t read_count = ENDP_RX_LEN(0);
    g_ch32_usbhs_udc[busid].ep_out[0].actual_xfer_len += read_count;
    g_ch32_usbhs_udc[busid].ep_out[0].xfer_len -= read_count;
    usbd_event_ep_out_complete_handler(0, 0x00, g_ch32_usbhs_udc[busid].ep_out[0].actual_xfer_len);
    if (read_count == 0) {
        USBHSD->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc[busid].setup;
        ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
    }
}

static inline void handle_non_ep0_out(uint8_t busid, uint8_t epid)
{
    uint32_t read_count = ENDP_RX_LEN(epid);

    if (USBHSD->UEP_RX_BURST & (1 << epid)) {
        usbd_event_ep_out_complete_handler(0, epid, read_count);
    } else {
        g_ch32_usbhs_udc[busid].ep_out[epid].actual_xfer_len += read_count;
        g_ch32_usbhs_udc[busid].ep_out[epid].xfer_len -= read_count;
        if ((read_count < g_ch32_usbhs_udc[busid].ep_out[epid].ep_mps) || (g_ch32_usbhs_udc[busid].ep_out[epid].xfer_len == 0)) {
            usbd_event_ep_out_complete_handler(0, epid, g_ch32_usbhs_udc[busid].ep_out[epid].actual_xfer_len);
        } else {
            ENDP_RX_DMA(epid) += g_ch32_usbhs_udc[busid].ep_out[epid].ep_mps;
            ENDP_RX_SIZE(epid) = MIN(g_ch32_usbhs_udc[busid].ep_out[epid].xfer_len, g_ch32_usbhs_udc[busid].ep_out[epid].ep_mps);
            ENDP_RX_CTRL(epid) = (ENDP_RX_CTRL(epid) & ~USBHS_UEP_R_RES_MASK) | USBHS_UEP_R_RES_ACK;
        }
    }
}

void USBD_IRQHandler(uint8_t busid)
{
    uint8_t flag = USBHSD->INT_FG;

    if (flag & USBHS_UDIF_TRANSFER) {
        uint8_t status = USBHSD->INT_ST;
        uint8_t endp = status & USBHS_UDIS_EP_ID_MASK;
        uint8_t dir = status & USBHS_UDIS_EP_DIR;

        // SETUP packet received
        if (endp == 0x00 && !dir && (ENDP_RX_CTRL(0) & USBHS_UEP_R_SETUP_IS)) {
            ENDP_TX_CTRL(0) = (ENDP_TX_CTRL(0) & ~USBHS_UEP_T_TOG_MASK) | USBHS_UEP_T_TOG_DATA1;
            ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBHS_UEP_R_TOG_MASK) | USBHS_UEP_R_TOG_DATA1;
            if (!(g_ch32_usbhs_udc[busid].setup.bmRequestType & 0x80)) {
                ENDP_TX_LEN(0) = 0;
                ENDP_TX_CTRL(0) = (ENDP_TX_CTRL(0) & ~USBHS_UEP_T_RES_MASK) | USBHS_UEP_T_RES_ACK;
            }
            usbd_event_ep0_setup_complete_handler(0, (uint8_t *)&g_ch32_usbhs_udc[busid].setup);
            ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBHS_UEP_R_DONE);
        }
        // IN transfer
        else if (dir) {
            if (endp == 0) {
                handle_ep0_in(busid);
            } else {
                handle_non_ep0_in(busid, endp);
            }
            ENDP_TX_CTRL(endp) = (ENDP_TX_CTRL(endp) & ~USBHS_UEP_T_DONE);
        }
        // OUT transfer
        else if (ENDP_RX_CTRL(endp) & USBHS_UEP_R_TOG_MATCH) {
            if (endp == 0) {
                handle_ep0_out(busid);
            } else {
                handle_non_ep0_out(busid, endp);
            }
            ENDP_RX_LEN(endp) = 0;
            ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBHS_UEP_R_DONE);
        }
        // OUT transfer toggle mismatch
        else {
            ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~(USBHS_UEP_R_DONE | USBHS_UEP_R_RES_MASK)) | USBHS_UEP_R_RES_ACK;
        }
    } else if (flag & USBHS_UDIF_BUS_RST) {
        USBHSD->DEV_AD = 0;
        USBHSD->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc[busid].setup;
        USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
        usbd_event_reset_handler(busid);
        USBHSD->INT_FG = USBHS_UDIF_BUS_RST;
    } else if (flag & USBHS_UDIF_SUSPEND) {
        if (USBHSD->MIS_ST & USBHS_UDMS_SUSPEND) {
            usbd_event_suspend_handler(busid);
        }
        USBHSD->INT_FG = USBHS_UDIF_SUSPEND;
    } else {
        USBHSD->INT_FG = flag;
    }
}
