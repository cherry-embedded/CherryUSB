#include "usbd_core.h"
#include "usb_ch32_usbfs_reg.h"

#ifndef CONFIG_CH32FS_BIDIR_ENDPOINTS
#define CONFIG_CH32FS_BIDIR_ENDPOINTS 8
#endif

#define USB_SET_DMA(ep_idx, addr)    (*(volatile uint32_t *)((uint32_t)(&USBFS_DEVICE->UEP0_DMA) + 4 * ep_idx) = addr)
#define USB_SET_TX_LEN(ep_idx, len)  (*(volatile uint16_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_LEN) + 4 * ep_idx) = len)
#define USB_GET_TX_LEN(ep_idx)       (*(volatile uint16_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_LEN) + 4 * ep_idx))
#define USB_SET_TX_CTRL(ep_idx, val) (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_CTRL) + 4 * ep_idx) = val)
#define USB_GET_TX_CTRL(ep_idx)      (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_CTRL) + 4 * ep_idx))
#define USB_SET_RX_CTRL(ep_idx, val) (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_RX_CTRL) + 4 * ep_idx) = val)
#define USB_GET_RX_CTRL(ep_idx)      (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_RX_CTRL) + 4 * ep_idx))

/* Endpoint state */
struct ch32_usbfs_ep {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct ch32_usbfs_udc {
    __attribute__((aligned(4))) struct usb_setup_packet setup;
    volatile uint8_t dev_addr;
    struct ch32_usbfs_ep in_ep[CONFIG_CH32FS_BIDIR_ENDPOINTS];
    struct ch32_usbfs_ep out_ep[CONFIG_CH32FS_BIDIR_ENDPOINTS];
    __attribute__((aligned(4))) uint8_t ep_databuf[CONFIG_CH32FS_BIDIR_ENDPOINTS - 1][64 + 64]; /* epx_out(64)+epx_in(64) */
} g_ch32_usbfs_udc;

volatile bool ep0_rx_data_toggle;
volatile bool ep0_tx_data_toggle;

int ch32fs_udc_init(struct usbd_bus *bus)
{
    if (bus->busid != 0) {
        USB_LOG_ERR("ch32fs busid must be 0\r\n");
        return -1;
    }

    USB_LOG_INFO("========== ch32fs udc params =========\r\n");
    USB_LOG_INFO("ch32fs has %d endpoints\r\n", CONFIG_CH32FS_BIDIR_ENDPOINTS);
    USB_LOG_INFO("=================================\r\n");

    /* ===================== low level init start ===================== */
    uint32_t *RCC = (uint32_t *)(0x40000000 + 0x20000 + 0x1000);
    uint32_t *IENR = (uint32_t *)(0xE000E100);

    /* RCC_USBCLK48MCLKSource_USBPHY */
    RCC[11] &= ~(1 << 31);
    RCC[11] |= 0x00000001 << 31;

    /* RCC_HSBHSPLLCLKSource_HSE */
    RCC[11] &= ~(1 << 27);
    RCC[11] |= 0x00000000 << 27;

    /* RCC_USBPLL_Div2 */
    RCC[11] &= ~(7 << 24);
    RCC[11] |= 0x00000001 << 24;

    /* RCC_USBHSPLLCKREFCLK_4M */
    RCC[11] &= ~(3 << 28);
    RCC[11] |= 0x00000001 << 28;

    /* Enable USBHS PHY control. */
    RCC[11] |= 0x00000001 << 30;

    /* ENABLE RCC_AHBPeriph_OTG_FS */
    RCC[5] |= 0x00001000;

    /* Enable Interrupt OTG_FS_IRQn */
    IENR[((uint32_t)(83) >> 5)] = (1 << ((uint32_t)(83) & 0x1F));
    /* ===================== low level init end ===================== */

    USBFS_DEVICE->BASE_CTRL = 0x00;

    USBFS_DEVICE->UEP4_1_MOD = USBFS_UEP4_RX_EN | USBFS_UEP4_TX_EN | USBFS_UEP1_RX_EN | USBFS_UEP1_TX_EN;
    USBFS_DEVICE->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP2_TX_EN | USBFS_UEP3_RX_EN | USBFS_UEP3_TX_EN;
    USBFS_DEVICE->UEP5_6_MOD = USBFS_UEP5_RX_EN | USBFS_UEP5_TX_EN | USBFS_UEP6_RX_EN | USBFS_UEP6_TX_EN;
    USBFS_DEVICE->UEP7_MOD = USBFS_UEP7_RX_EN | USBFS_UEP7_TX_EN;

    USBFS_DEVICE->UEP1_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[0];
    USBFS_DEVICE->UEP2_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[1];
    USBFS_DEVICE->UEP3_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[2];
    USBFS_DEVICE->UEP4_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[3];
    USBFS_DEVICE->UEP5_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[4];
    USBFS_DEVICE->UEP6_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[5];
    USBFS_DEVICE->UEP7_DMA = (uint32_t)g_ch32_usbfs_udc.ep_databuf[6];

    USBFS_DEVICE->INT_FG = 0xFF;
    USBFS_DEVICE->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
    USBFS_DEVICE->DEV_ADDR = 0x00;

    USBFS_DEVICE->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBFS_DEVICE->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
    return 0;
}

int ch32fs_udc_deinit(struct usbd_bus *bus)
{
    return 0;
}

int ch32fs_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    if (addr == 0) {
        USBFS_DEVICE->DEV_ADDR = (USBFS_DEVICE->DEV_ADDR & USBFS_UDA_GP_BIT) | 0;
    }
    g_ch32_usbfs_udc.dev_addr = addr;
    return 0;
}

uint8_t ch32fs_get_port_speed(struct usbd_bus *bus)
{
    return USB_SPEED_FULL;
}

int ch32fs_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;

    if (ep_idx > (CONFIG_CH32FS_BIDIR_ENDPOINTS - 1)) {
        USB_LOG_ERR("Ep addr 0x%02x overflow\r\n", ep_desc->bEndpointAddress);
        return -1;
    }

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        g_ch32_usbfs_udc.out_ep[ep_idx].ep_mps = ep_mps;
        g_ch32_usbfs_udc.out_ep[ep_idx].ep_type = ep_type;
        g_ch32_usbfs_udc.out_ep[ep_idx].ep_enable = true;
        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_NAK | USBFS_UEP_AUTO_TOG);
    } else {
        g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps = ep_mps;
        g_ch32_usbfs_udc.in_ep[ep_idx].ep_type = ep_type;
        g_ch32_usbfs_udc.in_ep[ep_idx].ep_enable = true;
        USB_SET_TX_CTRL(ep_idx, USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG);
    }
    return 0;
}
int ch32fs_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    return 0;
}
int ch32fs_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0) {
            USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_STALL;
        } else {
            USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL);
        }
    } else {
        if (ep_idx == 0) {
            USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_STALL;
        } else {
            USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL);
        }
    }

    if (ep_idx == 0) {
        USB_SET_DMA(ep_idx, (uint32_t)&g_ch32_usbfs_udc.setup);
        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK);
    }
    return 0;
}

int ch32fs_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (ep_idx == 0) {
        return 0;
    }

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK);
    } else {
        USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK);
    }
    return 0;
}

int ch32fs_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int ch32fs_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_ch32_usbfs_udc.in_ep[ep_idx].ep_enable) {
        return -2;
    }

    if ((uint32_t)data & 0x03) {
        printf("data do not align4\r\n");
        return -3;
    }

    g_ch32_usbfs_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len = data_len;
    g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        if (data_len == 0) {
            USB_SET_TX_LEN(ep_idx, 0);
        } else {
            data_len = MIN(data_len, g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps);
            USB_SET_TX_LEN(ep_idx, data_len);
            USB_SET_DMA(ep_idx, (uint32_t)data);
        }
        if (ep0_tx_data_toggle) {
            USB_SET_TX_CTRL(ep_idx, USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK);
        } else {
            USB_SET_TX_CTRL(ep_idx, USBFS_UEP_T_RES_ACK);
        }

    } else {
        if (data_len == 0) {
            USB_SET_TX_LEN(ep_idx, 0);
        } else {
            data_len = MIN(data_len, g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps);
            USB_SET_TX_LEN(ep_idx, data_len);
            memcpy(&g_ch32_usbfs_udc.ep_databuf[ep_idx - 1][64], data, data_len);
        }
        USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK);
    }
    return 0;
}

int ch32fs_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_ch32_usbfs_udc.out_ep[ep_idx].ep_enable) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        printf("data do not align4\r\n");
        return -3;
    }

    g_ch32_usbfs_udc.out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32_usbfs_udc.out_ep[ep_idx].xfer_len = data_len;
    g_ch32_usbfs_udc.out_ep[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        if (data_len == 0) {
        } else {
            USB_SET_DMA(ep_idx, (uint32_t)data);
        }
        if (ep0_rx_data_toggle) {
            USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK);
        } else {
            USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK);
        }
        return 0;
    } else {
        USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK);
    }

    return 0;
}

void ch32fs_udc_irq(struct usbd_bus *bus)
{
    uint32_t ep_idx = 0, token, write_count, read_count;
    uint8_t intflag = 0;

    intflag = USBFS_DEVICE->INT_FG;

    if (intflag & USBFS_UIF_TRANSFER) {
        token = USBFS_DEVICE->INT_ST & USBFS_UIS_TOKEN_MASK;
        ep_idx = USBFS_DEVICE->INT_ST & USBFS_UIS_ENDP_MASK;
        switch (token) {
            case USBFS_UIS_TOKEN_SETUP:
                USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_NAK);
                usbd_event_ep0_setup_complete_handler(bus->busid, (uint8_t *)&g_ch32_usbfs_udc.setup);
                break;

            case USBFS_UIS_TOKEN_IN:
                if (ep_idx == 0x00) {
                    if (g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len > g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps) {
                        g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len -= g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps;
                        g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps;
                        ep0_tx_data_toggle ^= 1;
                    } else {
                        g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len;
                        g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len = 0;
                        ep0_tx_data_toggle = true;
                    }

                    usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len);

                    if (g_ch32_usbfs_udc.dev_addr > 0) {
                        USBFS_DEVICE->DEV_ADDR = (USBFS_DEVICE->DEV_ADDR & USBFS_UDA_GP_BIT) | g_ch32_usbfs_udc.dev_addr;
                        g_ch32_usbfs_udc.dev_addr = 0;
                    }

                    if (g_ch32_usbfs_udc.setup.wLength && ((g_ch32_usbfs_udc.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                        /* In status, start reading setup */
                        USB_SET_DMA(ep_idx, (uint32_t)&g_ch32_usbfs_udc.setup);
                        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK);
                        ep0_tx_data_toggle = true;

                    } else if (g_ch32_usbfs_udc.setup.wLength == 0) {
                        /* In status, start reading setup */
                        USB_SET_DMA(ep_idx, (uint32_t)&g_ch32_usbfs_udc.setup);
                        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK);
                        ep0_tx_data_toggle = true;
                    }
                } else {
                    USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK);

                    if (g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len > g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps) {
                        g_ch32_usbfs_udc.in_ep[ep_idx].xfer_buf += g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps;
                        g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len -= g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps;
                        g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps;

                        write_count = MIN(g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len, g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps);
                        USB_SET_TX_LEN(ep_idx, write_count);
                        memcpy(&g_ch32_usbfs_udc.ep_databuf[ep_idx - 1][64], g_ch32_usbfs_udc.in_ep[ep_idx].xfer_buf, write_count);

                        USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK);
                    } else {
                        g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len;
                        g_ch32_usbfs_udc.in_ep[ep_idx].xfer_len = 0;
                        usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, g_ch32_usbfs_udc.in_ep[ep_idx].actual_xfer_len);
                    }
                }
                break;
            case USBFS_UIS_TOKEN_OUT:
                if (ep_idx == 0x00) {
                    USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_NAK);

                    read_count = USBFS_DEVICE->RX_LEN;

                    g_ch32_usbfs_udc.out_ep[ep_idx].actual_xfer_len += read_count;
                    g_ch32_usbfs_udc.out_ep[ep_idx].xfer_len -= read_count;

                    usbd_event_ep_out_complete_handler(bus->busid, 0x00, g_ch32_usbfs_udc.out_ep[ep_idx].actual_xfer_len);

                    if (read_count == 0) {
                        /* Out status, start reading setup */
                        USB_SET_DMA(ep_idx, (uint32_t)&g_ch32_usbfs_udc.setup);
                        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK);
                        ep0_rx_data_toggle = true;
                        ep0_tx_data_toggle = true;
                    } else {
                        ep0_rx_data_toggle ^= 1;
                    }
                } else {
                    if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                        USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NAK);
                        read_count = USBFS_DEVICE->RX_LEN;

                        memcpy(g_ch32_usbfs_udc.out_ep[ep_idx].xfer_buf, &g_ch32_usbfs_udc.ep_databuf[ep_idx - 1][0], read_count);

                        g_ch32_usbfs_udc.out_ep[ep_idx].xfer_buf += read_count;
                        g_ch32_usbfs_udc.out_ep[ep_idx].actual_xfer_len += read_count;
                        g_ch32_usbfs_udc.out_ep[ep_idx].xfer_len -= read_count;

                        if ((read_count < g_ch32_usbfs_udc.out_ep[ep_idx].ep_mps) || (g_ch32_usbfs_udc.out_ep[ep_idx].xfer_len == 0)) {
                            usbd_event_ep_out_complete_handler(bus->busid, ep_idx, g_ch32_usbfs_udc.out_ep[ep_idx].actual_xfer_len);
                        } else {
                            USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK);
                        }
                    }
                }
                break;

            case USBFS_UIS_TOKEN_SOF:

                break;

            default:
                break;
        }

        USBFS_DEVICE->INT_FG = USBFS_UIF_TRANSFER;
    } else if (intflag & USBFS_UIF_BUS_RST) {
        USBFS_DEVICE->UEP0_TX_LEN = 0;
        USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_RES_NAK;

        for (uint8_t ep_idx = 1; ep_idx < CONFIG_CH32FS_BIDIR_ENDPOINTS; ep_idx++) {
            USB_SET_TX_LEN(ep_idx, 0);
            USB_SET_TX_CTRL(ep_idx, USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG);
            USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_NAK | USBFS_UEP_AUTO_TOG);
        }

        ep0_tx_data_toggle = true;
        ep0_rx_data_toggle = true;

        memset(&g_ch32_usbfs_udc, 0, sizeof(struct ch32_usbfs_udc));
        usbd_event_reset_handler(bus->busid);
        USB_SET_DMA(ep_idx, (uint32_t)&g_ch32_usbfs_udc.setup);
        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK);

        USBFS_DEVICE->INT_FG |= USBFS_UIF_BUS_RST;
    } else if (intflag & USBFS_UIF_SUSPEND) {
        if (USBFS_DEVICE->MIS_ST & USBFS_UMS_SUSPEND) {
        } else {
        }
        USBFS_DEVICE->INT_FG = USBFS_UIF_SUSPEND;
    } else {
        USBFS_DEVICE->INT_FG = intflag;
    }
}

struct usbd_udc_driver ch32fs_udc_driver = {
    .driver_name = "ch32fs udc",
    .udc_init = ch32fs_udc_init,
    .udc_deinit = ch32fs_udc_deinit,
    .udc_set_address = ch32fs_set_address,
    .udc_get_port_speed = ch32fs_get_port_speed,
    .udc_ep_open = ch32fs_ep_open,
    .udc_ep_close = ch32fs_ep_close,
    .udc_ep_set_stall = ch32fs_ep_set_stall,
    .udc_ep_clear_stall = ch32fs_ep_clear_stall,
    .udc_ep_is_stalled = ch32fs_ep_is_stalled,
    .udc_ep_start_write = ch32fs_ep_start_write,
    .udc_ep_start_read = ch32fs_ep_start_read,
    .udc_irq = ch32fs_udc_irq
};

void OTG_FS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void OTG_FS_IRQHandler(void)
{
    usbd_irq(0);
}