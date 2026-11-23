/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbd_core.h"
#include "usb_usbss_reg.h"

#ifndef CONFIG_USBDEV_EP_NUM
#define CONFIG_USBDEV_EP_NUM 8
#endif

#define USBSSD                     ((USBSSD_TypeDef *)g_usbdev_bus[busid].reg_base)
#define ENDP_TX_CFG(ep)            *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CFG) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CFG(ep)            *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CFG) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_CR(ep)             *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CR) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CR(ep)             *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CR) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_SEQ(ep)            *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_SEQ) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_SEQ(ep)            *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_SEQ) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_ST(ep)             *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_ST) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_ST(ep)             *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_ST) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_CHAIN_CR(ep)       *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CHAIN_CR) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CHAIN_CR(ep)       *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CHAIN_CR) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_CHAIN_ST(ep)       *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CHAIN_ST) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CHAIN_ST(ep)       *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CHAIN_ST) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_CHAIN_LEN(ep)      *((__IO uint16_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CHAIN_LEN) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CHAIN_LEN(ep)      *((__IO uint16_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CHAIN_LEN) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_CHAIN_EXP_NUMP(ep) *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CHAIN_EXP_NUMP) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CHAIN_MAX_NUMP(ep) *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CHAIN_MAX_NUMP) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_CHAIN_NUMP(ep)     *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_CHAIN_NUMP) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_CHAIN_NUMP(ep)     *((__IO uint8_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_CHAIN_NUMP) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_DMA_OFS(ep)        *((__IO uint16_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_DMA_OFS) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_DMA_OFS(ep)        *((__IO uint16_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_DMA_OFS) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))
#define ENDP_TX_DMA(ep)            *((__IO uint32_t *)(((uint32_t)&USBSSD->EP1_TX.UEP_TX_DMA) + (sizeof(USBSS_EP_TX_TypeDef) * 2) * (ep - 1)))
#define ENDP_RX_DMA(ep)            *((__IO uint32_t *)(((uint32_t)&USBSSD->EP1_RX.UEP_RX_DMA) + (sizeof(USBSS_EP_RX_TypeDef) * 2) * (ep - 1)))

extern void usb_dc_low_level_init(uint8_t busid);
extern void usb_dc_low_level_deinit(uint8_t busid);

struct ch32_usbhs_udc {
    uint8_t dev_addr;
    uint8_t set_isoch_delay;
    uint8_t *ep0_xfer_buf;
    uint32_t ep0_xfer_len;
    uint32_t ep0_actual_xfer_len;
    struct usb_setup_packet setup;
    __attribute__((aligned(4))) uint8_t ep0_dma_buf[512];
} g_ch32_usbss_udc[CONFIG_USBDEV_MAX_BUS];

int usb_dc_init(uint8_t busid)
{
    usb_dc_low_level_init(busid);
    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;
    USBSSD->USB_CONTROL |= USBSS_USB_CLR_ALL;
    USBSSD->USB_CONTROL &= ~USBSS_USB_CLR_ALL;
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
    USBSSD->UEP0_TX_DMA = (uint32_t)g_ch32_usbss_udc[busid].ep0_dma_buf;
    USBSSD->UEP0_RX_DMA = (uint32_t)g_ch32_usbss_udc[busid].ep0_dma_buf;
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    USBSSD->USB_CONTROL = USBSS_FORCE_RST;
    USBSSD->USB_CONTROL &= ~USBSS_FORCE_RST;
    usb_dc_low_level_deinit(busid);
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    g_ch32_usbss_udc[busid].dev_addr = addr;
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    return -1;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    return USB_SPEED_SUPER;
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    if (ep_idx >= CONFIG_USBDEV_EP_NUM) {
        USB_LOG_ERR("Ep addr %02x overflow\r\n", ep->bEndpointAddress);
        return -1;
    }
    uint8_t ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
    if (USB_EP_DIR_IS_IN(ep->bEndpointAddress)) {
        USBSSD->UEP_TX_EN |= 1 << ep_idx;
        if (ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            ENDP_TX_CFG(ep_idx) = (ENDP_TX_CFG(ep_idx) & ~USBSS_EP_TX_ERDY_AUTO) | USBSS_EP_TX_ISO_MODE;
        } else {
            ENDP_TX_CFG(ep_idx) = (ENDP_TX_CFG(ep_idx) & ~USBSS_EP_TX_ISO_MODE) | USBSS_EP_TX_ERDY_AUTO;
        }
    } else {
        USBSSD->UEP_RX_EN |= 1 << ep_idx;
        if (ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            ENDP_RX_CFG(ep_idx) = (ENDP_RX_CFG(ep_idx) & ~USBSS_EP_RX_ERDY_AUTO) | USBSS_EP_RX_ISO_MODE;
        } else {
            ENDP_RX_CFG(ep_idx) = (ENDP_RX_CFG(ep_idx) & ~USBSS_EP_RX_ISO_MODE) | USBSS_EP_RX_ERDY_AUTO;
        }
    }
    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (ep_idx >= CONFIG_USBDEV_EP_NUM)
        return -1;
    if (USB_EP_DIR_IS_IN(ep)) {
        USBSSD->UEP_TX_EN &= ~(1 << ep_idx);
    } else {
        USBSSD->UEP_RX_EN &= ~(1 << ep_idx);
    }
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (ep_idx >= CONFIG_USBDEV_EP_NUM) {
        return -1;
    }
    if (ep_idx == 0) {
        if (USB_EP_DIR_IS_OUT(ep)) {
            USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_STALL;
        } else {
            USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_ERDY | USBSS_EP0_TX_STALL;
        }
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        ENDP_RX_CR(ep_idx) |= USBSS_EP_RX_HALT;
    } else {
        ENDP_TX_CR(ep_idx) |= USBSS_EP_TX_HALT;
    }

    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (ep_idx >= CONFIG_USBDEV_EP_NUM) {
        return -1;
    }
    if (ep_idx == 0) {
        if (USB_EP_DIR_IS_OUT(ep)) {
            USBSSD->UEP0_RX_CTRL = 0;
        } else {
            USBSSD->UEP0_TX_CTRL = 0;
        }
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        ENDP_RX_CR(ep_idx) = USBSS_EP_RX_CLR | USBSS_EP_RX_CHAIN_CLR;
    } else {
        ENDP_TX_CR(ep_idx) = USBSS_EP_TX_CLR | USBSS_EP_TX_CHAIN_CLR;
    }
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (ep_idx >= CONFIG_USBDEV_EP_NUM) {
        return -1;
    }
    if (ep_idx == 0) {
        if (USB_EP_DIR_IS_OUT(ep)) {
            *stalled = USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_STALL ? 1 : 0;
        } else {
            *stalled = USBSSD->UEP0_TX_CTRL & USBSS_EP0_TX_STALL ? 1 : 0;
        }
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        *stalled = ENDP_RX_CR(ep_idx) & USBSS_EP_RX_HALT ? 1 : 0;
    } else {
        *stalled = ENDP_TX_CR(ep_idx) & USBSS_EP_TX_HALT ? 1 : 0;
    }
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (ep_idx >= CONFIG_USBDEV_EP_NUM) {
        return -2;
    }
    if (ep_idx && !(USBSSD->UEP_TX_EN & (1 << ep_idx))) {
        return -3;
    }
    if ((uint32_t)data & 0x03) {
        return -4;
    }
    if (ep_idx && (ENDP_TX_ST(ep_idx) & USBSS_EP_TX_CHAIN_EN_MASK) >= USBSS_EP_TX_CHAIN_EN_MASK) {
        return -5;
    }
    if (data_len > 16 * 1024) {
        return -6;
    }

    if (ep_idx == 0) {
        /* Control transfer status stage */
        if (data_len == 0) {
            USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
            return 0;
        }
        g_ch32_usbss_udc[busid].ep0_xfer_buf = (uint8_t *)data;
        g_ch32_usbss_udc[busid].ep0_xfer_len = data_len;
        g_ch32_usbss_udc[busid].ep0_actual_xfer_len = 0;
        uint32_t xfer_len = (data_len > 512 ? 512 : data_len);
        memcpy(g_ch32_usbss_udc[busid].ep0_dma_buf, data, xfer_len);
        USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | USBSS_EP0_TX_ERDY | xfer_len;
    } else {
        uint32_t temp = data_len ? (data_len - 1) : 0;
        ENDP_TX_DMA(ep_idx) = (uint32_t)data;
        ENDP_TX_CHAIN_LEN(ep_idx) = data_len ? (temp & 0x03FF) + 1 : 0;
        ENDP_TX_CHAIN_EXP_NUMP(ep_idx) = (temp >> 10) + 1;
    }

    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (ep_idx >= CONFIG_USBDEV_EP_NUM) {
        return -2;
    }
    if (ep_idx && !(USBSSD->UEP_RX_EN & (1 << ep_idx))) {
        return -3;
    }
    if ((uint32_t)data & 0x03) {
        return -4;
    }
    if (ep_idx && (ENDP_RX_ST(ep_idx) & USBSS_EP_RX_CHAIN_EN_MASK) >= USBSS_EP_RX_CHAIN_EN_MASK) {
        return -5;
    }
    if (data_len > 16 * 1024) {
        return -6;
    }

    if (ep_idx == 0) {
        g_ch32_usbss_udc[busid].ep0_xfer_buf = (uint8_t *)data;
        g_ch32_usbss_udc[busid].ep0_xfer_len = data_len;
        g_ch32_usbss_udc[busid].ep0_actual_xfer_len = 0;
        USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
    } else {
        ENDP_RX_DMA(ep_idx) = (uint32_t)data;
        ENDP_RX_CHAIN_MAX_NUMP(ep_idx) = data_len ? (((data_len - 1) >> 10) + 1) : 1;
    }

    return 0;
}

void usbss_link_reset(uint8_t busid)
{
    USBSSD->USB_CONTROL |= USBSS_FORCE_RST;
    USBSSD->USB_STATUS = USBSS_UIF_TRANSFER;
    USBSSD->USB_CONTROL = USBSS_UIE_TRANSFER | USBSS_UDIE_SETUP | USBSS_UDIE_STATUS | USBSS_DMA_EN | USBSS_SETUP_FLOW;
    USBSSD->USB_CONTROL |= USBSS_USB_CLR_ALL;
    USBSSD->USB_CONTROL &= ~USBSS_USB_CLR_ALL;
    USBSSD->UEP_TX_EN = 0;
    USBSSD->UEP_RX_EN = 0;
    USBSSD->UEP0_TX_CTRL = 0;
    USBSSD->UEP0_RX_CTRL = 0;
    USBSSD->UEP0_TX_DMA = (uint32_t)g_ch32_usbss_udc[busid].ep0_dma_buf;
    USBSSD->UEP0_RX_DMA = (uint32_t)g_ch32_usbss_udc[busid].ep0_dma_buf;
    usbd_event_reset_handler(busid);
}

void USBD_IRQHandler(uint8_t busid)
{
    uint32_t status = USBSSD->USB_STATUS;
    if ((status & USBSS_UDIF_SETUP) && !(status & USBSS_UDIF_STATUS)) {
        memcpy(&g_ch32_usbss_udc[busid].setup, g_ch32_usbss_udc[busid].ep0_dma_buf, sizeof(struct usb_setup_packet));
        usbd_event_ep0_setup_complete_handler(busid, g_ch32_usbss_udc[busid].ep0_dma_buf);
        USBSSD->USB_STATUS = USBSS_UDIF_SETUP;
    } else if (status & USBSS_UDIF_STATUS) {
        USBSSD->USB_STATUS = USBSS_UDIF_STATUS;
        if (g_ch32_usbss_udc[busid].dev_addr) {
            USBSSD->USB_CONTROL = (USBSSD->USB_CONTROL & ~0xFF000000) | (g_ch32_usbss_udc[busid].dev_addr << 24);
            g_ch32_usbss_udc[busid].dev_addr = 0;
        } else if (g_ch32_usbss_udc[busid].set_isoch_delay) {
            USBSSD->LINK_ISO_DLY = g_ch32_usbss_udc[busid].set_isoch_delay;
            g_ch32_usbss_udc[busid].set_isoch_delay = 0;
        }
        if (g_ch32_usbss_udc[busid].setup.bmRequestType & 0x80) {
            usbd_event_ep_out_complete_handler(busid, 0x00, 0);
        } else {
            usbd_event_ep_in_complete_handler(busid, 0x80, 0);
        }
    } else if (status & USBSS_UIF_TRANSFER) {
        uint8_t ep_idx = (status & USBSS_EP_ID_MASK) >> 8;
        if (status & USBSS_EP_DIR_MASK) {
            if (ep_idx == 0) {
                uint32_t xfer_len = USBSSD->UEP0_TX_CTRL & USBSS_EP0_TX_LEN_MASK;
                g_ch32_usbss_udc[busid].ep0_xfer_buf += xfer_len;
                g_ch32_usbss_udc[busid].ep0_actual_xfer_len += xfer_len;
                if (g_ch32_usbss_udc[busid].ep0_actual_xfer_len >= g_ch32_usbss_udc[busid].ep0_xfer_len) {
                    USBSSD->UEP0_TX_CTRL = 0;
                    usbd_event_ep_in_complete_handler(busid, 0x80, g_ch32_usbss_udc[busid].ep0_actual_xfer_len);
                } else {
                    uint32_t remaining = g_ch32_usbss_udc[busid].ep0_xfer_len - g_ch32_usbss_udc[busid].ep0_actual_xfer_len;
                    xfer_len = (remaining > 512 ? 512 : remaining);
                    memcpy(g_ch32_usbss_udc[busid].ep0_dma_buf, g_ch32_usbss_udc[busid].ep0_xfer_buf, xfer_len);
                    uint32_t curr_seq = USBSSD->UEP0_TX_CTRL & (0x1F << 16);
                    USBSSD->UEP0_TX_CTRL = USBSS_EP0_TX_DPH | USBSS_EP0_TX_ERDY | xfer_len | curr_seq + (1 << 16);
                }
            } else {
                uint32_t xfer_len = (ENDP_TX_CHAIN_NUMP(ep_idx) - 1) * 1024 + ENDP_TX_CHAIN_LEN(ep_idx);
                ENDP_TX_CHAIN_ST(ep_idx) |= USBSS_EP_TX_CHAIN_IF;
                usbd_event_ep_in_complete_handler(busid, 0x80 | ep_idx, xfer_len);
            }
        } else {
            if (ep_idx == 0) {
                uint32_t xfer_len = USBSSD->UEP0_RX_CTRL & USBSS_EP0_RX_LEN_MASK;
                memcpy(g_ch32_usbss_udc[busid].ep0_xfer_buf, g_ch32_usbss_udc[busid].ep0_dma_buf, xfer_len);
                g_ch32_usbss_udc[busid].ep0_xfer_buf += xfer_len;
                g_ch32_usbss_udc[busid].ep0_actual_xfer_len += xfer_len;
                if (g_ch32_usbss_udc[busid].ep0_actual_xfer_len >= g_ch32_usbss_udc[busid].ep0_xfer_len) {
                    USBSSD->UEP0_RX_CTRL = 0;
                    usbd_event_ep_out_complete_handler(busid, 0x00, g_ch32_usbss_udc[busid].ep0_actual_xfer_len);
                } else {
                    USBSSD->UEP0_RX_CTRL = USBSS_EP0_RX_ERDY | USBSS_EP0_RX_ACK;
                }
            } else {
                uint32_t xfer_len = (ENDP_RX_CHAIN_NUMP(ep_idx) - 1) * 1024 + ENDP_RX_CHAIN_LEN(ep_idx);
                ENDP_RX_CHAIN_ST(ep_idx) |= USBSS_EP_RX_CHAIN_IF;
                usbd_event_ep_out_complete_handler(busid, 0x00 | ep_idx, xfer_len);
            }
        }
    }
}
