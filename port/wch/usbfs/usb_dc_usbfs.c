/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbd_core.h"
#include "usb_usbfs_reg.h"

#ifndef CONFIG_USBDEV_EP_NUM
#define CONFIG_USBDEV_EP_NUM 8
#endif

#define USBFSD            ((USBFSD_TypeDef *)g_usbdev_bus[busid].reg_base)
#define ENDP_TX_LEN(ep)   *((volatile uint16_t *)&(USBFSD->UEP0_TX_LEN) + (ep) * 2)
#define ENDP_TX_CTRL(ep)  *((volatile uint8_t *)&(USBFSD->UEP0_TX_CTRL) + (ep) * 4)
#define ENDP_RX_CTRL(ep)  *((volatile uint8_t *)&(USBFSD->UEP0_RX_CTRL) + (ep) * 4)

struct ch32_usbfs_ep_state {
    uint8_t ep_type;
    uint16_t ep_mps;
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

struct ch32_usbfs_udc {
    uint8_t dev_addr;
    __attribute__((aligned(4))) struct usb_setup_packet setup;
    struct ch32_usbfs_ep_state ep_in[CONFIG_USBDEV_EP_NUM];
    struct ch32_usbfs_ep_state ep_out[CONFIG_USBDEV_EP_NUM];
} g_ch32_usbfs_udc[CONFIG_USBDEV_MAX_BUS];

static const uint32_t endp_mode_reg_offset[] = {
    0,
    offsetof(USBFSD_TypeDef, UEP4_1_MOD),
    offsetof(USBFSD_TypeDef, UEP2_3_MOD),
    offsetof(USBFSD_TypeDef, UEP2_3_MOD),
    offsetof(USBFSD_TypeDef, UEP4_1_MOD),
    offsetof(USBFSD_TypeDef, UEP5_6_MOD),
    offsetof(USBFSD_TypeDef, UEP5_6_MOD),
    offsetof(USBFSD_TypeDef, UEP7_MOD),
};

static const uint8_t endp_mode_tx_en[] = {
    0,
    USBFS_UEP1_TX_EN,
    USBFS_UEP2_TX_EN,
    USBFS_UEP3_TX_EN,
    USBFS_UEP4_TX_EN,
    USBFS_UEP5_TX_EN,
    USBFS_UEP6_TX_EN,
    USBFS_UEP7_TX_EN,
};

static const uint8_t endp_mode_rx_en[] = {
    0,
    USBFS_UEP1_RX_EN,
    USBFS_UEP2_RX_EN,
    USBFS_UEP3_RX_EN,
    USBFS_UEP4_RX_EN,
    USBFS_UEP5_RX_EN,
    USBFS_UEP6_RX_EN,
    USBFS_UEP7_RX_EN,
};

static uint8_t *endp_tx_bufs[CONFIG_USBDEV_EP_NUM - 1];
static uint8_t *endp_rx_bufs[CONFIG_USBDEV_EP_NUM - 1];
static __attribute__((aligned(4))) uint8_t endp_xfer_bufs[CONFIG_USBDEV_EP_NUM - 1][64 + 64];

__WEAK void
usb_dc_low_level_init(uint8_t busid)
{
}

__WEAK void usb_dc_low_level_deinit(uint8_t busid)
{
}

int usb_dc_init(uint8_t busid)
{
    usb_dc_low_level_init(busid);

    USBFSD->BASE_CTRL = 0x00;

    USBFSD->UEP4_1_MOD = 0x00;
    USBFSD->UEP2_3_MOD = 0x00;
    USBFSD->UEP5_6_MOD = 0x00;
    USBFSD->UEP7_MOD = 0x00;

    USBFSD->UEP1_DMA = (uint32_t)endp_xfer_bufs[0];
    USBFSD->UEP2_DMA = (uint32_t)endp_xfer_bufs[1];
    USBFSD->UEP3_DMA = (uint32_t)endp_xfer_bufs[2];
    USBFSD->UEP4_DMA = (uint32_t)endp_xfer_bufs[3];
    USBFSD->UEP5_DMA = (uint32_t)endp_xfer_bufs[4];
    USBFSD->UEP6_DMA = (uint32_t)endp_xfer_bufs[5];
    USBFSD->UEP7_DMA = (uint32_t)endp_xfer_bufs[6];

    USBFSD->INT_FG = 0xFF;
    USBFSD->DEV_ADDR = 0x00;
    USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;

    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    USBFSD->BASE_CTRL = 0;
    usb_dc_low_level_deinit(busid);
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    g_ch32_usbfs_udc[busid].dev_addr = addr;
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    return -1;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    return (USBFSD->UDEV_CTRL & USBFS_UD_LOW_SPEED) ? USB_SPEED_LOW : USB_SPEED_FULL;
}

static inline void ch32_usbfs_update_ep_buf(uint8_t busid, uint8_t epid)
{
    __IO uint8_t *endp_mode_reg = (__IO uint8_t *)((uint32_t)USBFSD + endp_mode_reg_offset[epid]);
    endp_rx_bufs[epid - 1] = &endp_xfer_bufs[epid - 1][0];
    endp_tx_bufs[epid - 1] = (*endp_mode_reg & endp_mode_rx_en[epid]) ? &endp_xfer_bufs[epid - 1][64] : &endp_xfer_bufs[epid - 1][0];
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t epid = USB_EP_GET_IDX(ep->bEndpointAddress);

    if (epid >= CONFIG_USBDEV_EP_NUM) {
        USB_LOG_ERR("Ep addr %02x overflow\r\n", ep->bEndpointAddress);
        return -1;
    }

    __IO uint8_t *endp_mode_reg = (__IO uint8_t *)((uint32_t)USBFSD + endp_mode_reg_offset[epid]);

    if (USB_EP_DIR_IS_IN(ep->bEndpointAddress)) {
        g_ch32_usbfs_udc[busid].ep_in[epid].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_ch32_usbfs_udc[busid].ep_in[epid].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        if (epid) {
            *endp_mode_reg |= endp_mode_tx_en[epid];
            ch32_usbfs_update_ep_buf(busid, epid);
        }
        if (g_ch32_usbfs_udc[busid].ep_in[epid].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            ENDP_TX_CTRL(epid) = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
        } else {
            ENDP_TX_CTRL(epid) = USBFS_UEP_T_RES_NONE;
        }
    } else {
        g_ch32_usbfs_udc[busid].ep_out[epid].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_ch32_usbfs_udc[busid].ep_out[epid].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        if (epid) {
            *endp_mode_reg |= endp_mode_rx_en[epid];
            ch32_usbfs_update_ep_buf(busid, epid);
        }
        if (g_ch32_usbfs_udc[busid].ep_out[epid].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            ENDP_RX_CTRL(epid) = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_NAK;
        } else {
            ENDP_RX_CTRL(epid) = USBFS_UEP_R_RES_NONE;
        }
    }
    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t epid = USB_EP_GET_IDX(ep);

    if (epid >= CONFIG_USBDEV_EP_NUM) {
        USB_LOG_ERR("Ep addr %02x invalid\r\n", ep);
        return -1;
    }

    if (epid) {
        __IO uint8_t *endp_mode_reg = (__IO uint8_t *)((uint32_t)USBFSD + endp_mode_reg_offset[epid]);
        if (USB_EP_DIR_IS_IN(ep)) {
            *endp_mode_reg &= ~endp_mode_tx_en[epid];
        } else {
            *endp_mode_reg &= ~endp_mode_rx_en[epid];
        }
        ch32_usbfs_update_ep_buf(busid, epid);
    }
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
        ENDP_RX_CTRL(ep_idx) = (ENDP_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
    } else {
        ENDP_TX_CTRL(ep_idx) = (ENDP_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
    }
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
        ENDP_RX_CTRL(ep_idx) &= ~USBFS_UEP_R_AUTO_TOG;
        ENDP_RX_CTRL(ep_idx) = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_ACK;
        ENDP_RX_CTRL(ep_idx) |= USBFS_UEP_R_AUTO_TOG;
    } else {
        ENDP_TX_CTRL(ep_idx) &= ~USBFS_UEP_T_AUTO_TOG;
        ENDP_TX_CTRL(ep_idx) = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
        ENDP_TX_CTRL(ep_idx) |= USBFS_UEP_T_AUTO_TOG;
    }
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
        *stalled = (ENDP_RX_CTRL(USB_EP_GET_IDX(ep)) & USBFS_UEP_R_RES_MASK) == USBFS_UEP_R_RES_STALL;
    } else {
        *stalled = (ENDP_TX_CTRL(USB_EP_GET_IDX(ep)) & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_STALL;
    }
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    __IO uint8_t *endp_mode_reg = (__IO uint8_t *)((uint32_t)USBFSD + endp_mode_reg_offset[ep_idx]);
    if (ep_idx && !(*endp_mode_reg & endp_mode_tx_en[ep_idx])) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }
    if (g_ch32_usbfs_udc[busid].ep_in[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS &&
        (ENDP_TX_CTRL(ep_idx) & USBFS_UEP_T_RES_MASK) != USBFS_UEP_T_RES_NAK) {
        return -4;
    }

    g_ch32_usbfs_udc[busid].ep_in[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32_usbfs_udc[busid].ep_in[ep_idx].xfer_len = data_len;
    g_ch32_usbfs_udc[busid].ep_in[ep_idx].actual_xfer_len = 0;
    data_len = MIN(data_len, g_ch32_usbfs_udc[busid].ep_in[ep_idx].ep_mps);

    if (ep_idx == 0) {
        USBFSD->UEP0_DMA = (uint32_t)data;
    } else {
        memcpy(endp_tx_bufs[ep_idx - 1], g_ch32_usbfs_udc[busid].ep_in[ep_idx].xfer_buf, data_len);
    }

    ENDP_TX_LEN(ep_idx) = data_len;
    if (g_ch32_usbfs_udc[busid].ep_in[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        ENDP_TX_CTRL(ep_idx) = (ENDP_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
    } else {
        ENDP_TX_CTRL(ep_idx) = (ENDP_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NONE;
    }
    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    __IO uint8_t *endp_mode_reg = (__IO uint8_t *)((uint32_t)USBFSD + endp_mode_reg_offset[ep_idx]);
    if (ep_idx && !(*endp_mode_reg & endp_mode_rx_en[ep_idx])) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }

    g_ch32_usbfs_udc[busid].ep_out[ep_idx].xfer_buf = data;
    g_ch32_usbfs_udc[busid].ep_out[ep_idx].xfer_len = data_len;
    g_ch32_usbfs_udc[busid].ep_out[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        USBFSD->UEP0_DMA = (uint32_t)data;
    }

    if (g_ch32_usbfs_udc[busid].ep_out[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        ENDP_RX_CTRL(ep_idx) = (ENDP_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
    } else {
        ENDP_RX_CTRL(ep_idx) = (ENDP_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NONE;
    }
    return 0;
}

void USBD_IRQHandler(uint8_t busid)
{
    uint8_t flag = USBFSD->INT_FG;

    if (flag & USBFS_UIF_TRANSFER) {
        uint8_t status = USBFSD->INT_ST;
        uint8_t endp = status & USBFS_UIS_ENDP_MASK;
        uint8_t token = status & USBFS_UIS_TOKEN_MASK;

        switch (token) {
            case USBFS_UIS_TOKEN_SETUP:
                ENDP_TX_CTRL(0) = (ENDP_TX_CTRL(0) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK | USBFS_UEP_T_TOG;
                ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NAK | USBFS_UEP_R_TOG;
                usbd_event_ep0_setup_complete_handler(0, (uint8_t *)&g_ch32_usbfs_udc[busid].setup);
                break;

            case USBFS_UIS_TOKEN_OUT:
                if (status & USBFS_UIS_TOG_OK) {
                    if (endp == 0) {
                        USBFSD->UEP0_RX_CTRL ^= USBFS_UEP_R_TOG;
                        uint32_t read_count = USBFSD->RX_LEN;
                        read_count = MIN(read_count, g_ch32_usbfs_udc[busid].ep_out[endp].xfer_len);
                        g_ch32_usbfs_udc[busid].ep_out[0].actual_xfer_len += read_count;
                        g_ch32_usbfs_udc[busid].ep_out[0].xfer_len -= read_count;
                        usbd_event_ep_out_complete_handler(0, 0x00, g_ch32_usbfs_udc[busid].ep_out[0].actual_xfer_len);
                        if (read_count == 0) {
                            USBFSD->UEP0_DMA = (uint32_t)&g_ch32_usbfs_udc[busid].setup;
                            ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                        }
                    } else {
                        if (g_ch32_usbfs_udc[busid].ep_out[endp].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                            ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NAK;
                        }
                        uint32_t read_count = USBFSD->RX_LEN;
                        read_count = MIN(read_count, g_ch32_usbfs_udc[busid].ep_out[endp].xfer_len);
                        memcpy(g_ch32_usbfs_udc[busid].ep_out[endp].xfer_buf, endp_rx_bufs[endp - 1], read_count);
                        g_ch32_usbfs_udc[busid].ep_out[endp].xfer_buf += read_count;
                        g_ch32_usbfs_udc[busid].ep_out[endp].actual_xfer_len += read_count;
                        g_ch32_usbfs_udc[busid].ep_out[endp].xfer_len -= read_count;
                        if ((read_count < g_ch32_usbfs_udc[busid].ep_out[endp].ep_mps) || (g_ch32_usbfs_udc[busid].ep_out[endp].xfer_len == 0)) {
                            usbd_event_ep_out_complete_handler(0, endp, g_ch32_usbfs_udc[busid].ep_out[endp].actual_xfer_len);
                        } else if (g_ch32_usbfs_udc[busid].ep_out[endp].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                            ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                        } else {
                            ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NONE;
                        }
                    }
                } else {
                    // OUT transfer toggle mismatch
                    ENDP_RX_CTRL(endp) = (ENDP_RX_CTRL(endp) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                }
                break;

            case USBFS_UIS_TOKEN_IN:
                if (g_ch32_usbfs_udc[busid].ep_in[endp].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                    ENDP_TX_CTRL(endp) = (ENDP_TX_CTRL(endp) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                }
                if (endp == 0) {
                    if (g_ch32_usbfs_udc[busid].setup.bmRequestType & 0x80) {
                        if (g_ch32_usbfs_udc[busid].ep_in[0].xfer_len >= g_ch32_usbfs_udc[busid].ep_in[0].ep_mps) {
                            g_ch32_usbfs_udc[busid].ep_in[0].xfer_len -= g_ch32_usbfs_udc[busid].ep_in[0].ep_mps;
                            g_ch32_usbfs_udc[busid].ep_in[0].actual_xfer_len += g_ch32_usbfs_udc[busid].ep_in[0].ep_mps;
                        } else {
                            g_ch32_usbfs_udc[busid].ep_in[0].actual_xfer_len += g_ch32_usbfs_udc[busid].ep_in[0].xfer_len;
                            g_ch32_usbfs_udc[busid].ep_in[0].xfer_len = 0;
                        }

                        USBFSD->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_ep_in_complete_handler(0, 0x80, g_ch32_usbfs_udc[busid].ep_in[0].actual_xfer_len);
                    } else {
                        USBFSD->UEP0_DMA = (uint32_t)&g_ch32_usbfs_udc[busid].setup;
                        ENDP_RX_CTRL(0) = (ENDP_RX_CTRL(0) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
                    }
                } else if (g_ch32_usbfs_udc[busid].ep_in[endp].xfer_len > g_ch32_usbfs_udc[busid].ep_in[endp].ep_mps) {
                    g_ch32_usbfs_udc[busid].ep_in[endp].xfer_buf += g_ch32_usbfs_udc[busid].ep_in[endp].ep_mps;
                    g_ch32_usbfs_udc[busid].ep_in[endp].xfer_len -= g_ch32_usbfs_udc[busid].ep_in[endp].ep_mps;
                    g_ch32_usbfs_udc[busid].ep_in[endp].actual_xfer_len += g_ch32_usbfs_udc[busid].ep_in[endp].ep_mps;

                    uint32_t write_count = MIN(g_ch32_usbfs_udc[busid].ep_in[endp].xfer_len, g_ch32_usbfs_udc[busid].ep_in[endp].ep_mps);
                    ENDP_TX_LEN(endp) = write_count;
                    memcpy(endp_tx_bufs[endp - 1], g_ch32_usbfs_udc[busid].ep_in[endp].xfer_buf, write_count);
                    if (g_ch32_usbfs_udc[busid].ep_in[endp].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                        ENDP_TX_CTRL(endp) = (ENDP_TX_CTRL(endp) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                    } else {
                        ENDP_TX_CTRL(endp) = (ENDP_TX_CTRL(endp) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NONE;
                    }
                } else {
                    g_ch32_usbfs_udc[busid].ep_in[endp].actual_xfer_len += g_ch32_usbfs_udc[busid].ep_in[endp].xfer_len;
                    g_ch32_usbfs_udc[busid].ep_in[endp].xfer_len = 0;
                    usbd_event_ep_in_complete_handler(0, 0x80 | endp, g_ch32_usbfs_udc[busid].ep_in[endp].actual_xfer_len);
                }

                if (g_ch32_usbfs_udc[busid].dev_addr) {
                    USBFSD->DEV_ADDR = g_ch32_usbfs_udc[busid].dev_addr;
                    g_ch32_usbfs_udc[busid].dev_addr = 0;
                }
                break;

            case USBFS_UIS_TOKEN_SOF:
                break;
        }
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    } else if (flag & USBFS_UIF_BUS_RST) {
        USBFSD->DEV_ADDR = 0;
        USBFSD->UEP0_DMA = (uint32_t)&g_ch32_usbfs_udc[busid].setup;
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        usbd_event_reset_handler(busid);
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
    } else if (flag & USBFS_UIF_SUSPEND) {
        if (USBFSD->MIS_ST & USBFS_UMS_SUSPEND) {
            usbd_event_suspend_handler(busid);
        }
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
    } else {
        USBFSD->INT_FG = flag;
    }
}
