#include "usbd_core.h"
#include "usb_ch32_usbhs_reg.h"

#ifndef CONFIG_CH32HS_BIDIR_ENDPOINTS
#define CONFIG_CH32HS_BIDIR_ENDPOINTS 16
#endif

#define USB_SET_RX_DMA(ep_idx, addr) (*(volatile uint32_t *)((uint32_t)(&USBHS_DEVICE->UEP1_RX_DMA) + 4 * (ep_idx - 1)) = addr)
#define USB_SET_TX_DMA(ep_idx, addr) (*(volatile uint32_t *)((uint32_t)(&USBHS_DEVICE->UEP1_TX_DMA) + 4 * (ep_idx - 1)) = addr)
#define USB_SET_MAX_LEN(ep_idx, len) (*(volatile uint16_t *)((uint32_t)(&USBHS_DEVICE->UEP0_MAX_LEN) + 4 * ep_idx) = len)
#define USB_SET_TX_LEN(ep_idx, len)  (*(volatile uint16_t *)((uint32_t)(&USBHS_DEVICE->UEP0_TX_LEN) + 4 * ep_idx) = len)
#define USB_GET_TX_LEN(ep_idx)       (*(volatile uint16_t *)((uint32_t)(&USBHS_DEVICE->UEP0_TX_LEN) + 4 * ep_idx))
#define USB_SET_TX_CTRL(ep_idx, val) (*(volatile uint8_t *)((uint32_t)(&USBHS_DEVICE->UEP0_TX_CTRL) + 4 * ep_idx) = val)
#define USB_GET_TX_CTRL(ep_idx)      (*(volatile uint8_t *)((uint32_t)(&USBHS_DEVICE->UEP0_TX_CTRL) + 4 * ep_idx))
#define USB_SET_RX_CTRL(ep_idx, val) (*(volatile uint8_t *)((uint32_t)(&USBHS_DEVICE->UEP0_RX_CTRL) + 4 * ep_idx) = val)
#define USB_GET_RX_CTRL(ep_idx)      (*(volatile uint8_t *)((uint32_t)(&USBHS_DEVICE->UEP0_RX_CTRL) + 4 * ep_idx))

/* Endpoint state */
struct ch32_usbhs_ep {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct ch32_usbhs_udc {
    __attribute__((aligned(4))) struct usb_setup_packet setup;
    volatile uint8_t dev_addr;
    struct ch32_usbhs_ep in_ep[CONFIG_CH32HS_BIDIR_ENDPOINTS];
    struct ch32_usbhs_ep out_ep[CONFIG_CH32HS_BIDIR_ENDPOINTS];
} g_ch32_usbhs_udc;

volatile uint8_t mps_over_flag = 0;
volatile bool ep0_rx_data_toggle;
volatile bool ep0_tx_data_toggle;
volatile bool epx_tx_data_toggle[CONFIG_CH32HS_BIDIR_ENDPOINTS - 1];

int ch32hs_udc_init(struct usbd_bus *bus)
{
    if (bus->busid != 1) {
        USB_LOG_ERR("ch32hs busid must be 1\r\n");
        return -1;
    }

    USB_LOG_INFO("========== ch32hs udc params =========\r\n");
    USB_LOG_INFO("ch32hs has %d endpoints\r\n", CONFIG_CH32HS_BIDIR_ENDPOINTS);
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

    /* ENABLE RCC_AHBPeriph_USBHS */
    RCC[5] |= 0x00000800;

    /* Enable Interrupt USBHS_IRQn */
    IENR[((uint32_t)(85) >> 5)] = (1 << ((uint32_t)(85) & 0x1F));
    /* ===================== low level init end ===================== */

    USBHS_DEVICE->HOST_CTRL = 0x00;
    USBHS_DEVICE->HOST_CTRL = USBHS_PHY_SUSPENDM;

    USBHS_DEVICE->CONTROL = 0;
#ifdef CONFIG_USB_HS
    USBHS_DEVICE->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_HIGH_SPEED;
#else
    USBHS_DEVICE->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_FULL_SPEED;
#endif

    USBHS_DEVICE->INT_FG = 0xff;
    USBHS_DEVICE->INT_EN = 0;
    USBHS_DEVICE->INT_EN = USBHS_SETUP_ACT_EN | USBHS_TRANSFER_EN | USBHS_DETECT_EN;

    /* ALL endpoint enable */
    USBHS_DEVICE->ENDP_CONFIG = 0xffffffff;

    USBHS_DEVICE->ENDP_TYPE = 0x00;
    USBHS_DEVICE->BUF_MODE = 0x00;

    USBHS_DEVICE->CONTROL |= USBHS_DEV_PU_EN;

    return 0;
}

int ch32hs_udc_deinit(struct usbd_bus *bus)
{
    return 0;
}

int ch32hs_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    if (addr == 0) {
        USBHS_DEVICE->DEV_AD = addr & 0xff;
    }
    g_ch32_usbhs_udc.dev_addr = addr;
    return 0;
}

uint8_t ch32hs_get_port_speed(struct usbd_bus *bus)
{
    return USB_SPEED_HIGH;
}

int ch32hs_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;

    if (ep_idx > (CONFIG_CH32HS_BIDIR_ENDPOINTS - 1)) {
        USB_LOG_ERR("Ep addr 0x%02x overflow\r\n", ep_desc->bEndpointAddress);
        return -1;
    }

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        g_ch32_usbhs_udc.out_ep[ep_idx].ep_mps = ep_mps;
        g_ch32_usbhs_udc.out_ep[ep_idx].ep_type = ep_type;
        g_ch32_usbhs_udc.out_ep[ep_idx].ep_enable = true;
        USBHS_DEVICE->ENDP_CONFIG |= (1 << (ep_idx + 16));
        USB_SET_RX_CTRL(ep_idx, USBHS_EP_R_RES_NAK | USBHS_EP_R_TOG_0 | USBHS_EP_R_AUTOTOG);
    } else {
        g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps = ep_mps;
        g_ch32_usbhs_udc.in_ep[ep_idx].ep_type = ep_type;
        g_ch32_usbhs_udc.in_ep[ep_idx].ep_enable = true;
        USBHS_DEVICE->ENDP_CONFIG |= (1 << (ep_idx));
        USB_SET_TX_CTRL(ep_idx, USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0 | USBHS_EP_T_AUTOTOG);
    }
    USB_SET_MAX_LEN(ep_idx, ep_mps);
    return 0;
}

int ch32hs_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    return 0;
}

int ch32hs_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0) {
            USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_STALL;
        } else {
            USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_STALL);
        }
    } else {
        if (ep_idx == 0) {
            USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_STALL;
        } else {
            USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBHS_EP_T_RES_MASK) | USBHS_EP_T_RES_STALL);
        }
    }

    return 0;
}

int ch32hs_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_SET_RX_CTRL(ep_idx, USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0);
    } else {
        USB_SET_TX_CTRL(ep_idx, USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0);
    }
    return 0;
}

int ch32hs_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int ch32hs_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t tmp;

    if (!data && data_len) {
        return -1;
    }
    if (!g_ch32_usbhs_udc.in_ep[ep_idx].ep_enable) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }

    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len = data_len;
    g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        if (data_len == 0) {
            USB_SET_TX_LEN(ep_idx, 0);
        } else {
            data_len = MIN(data_len, g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps);
            USB_SET_TX_LEN(ep_idx, data_len);
            USBHS_DEVICE->UEP0_DMA = (uint32_t)data;
        }
        tmp = ep0_tx_data_toggle ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0;
        USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_ACK | tmp;
    } else {
        if (data_len == 0) {
            USB_SET_TX_LEN(ep_idx, 0);
        } else {
            data_len = MIN(data_len, g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps);
            USB_SET_TX_LEN(ep_idx, data_len);
            USB_SET_TX_DMA(ep_idx, (uint32_t)data);
        }
        tmp = USB_GET_TX_CTRL(ep_idx);
        tmp &= ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK);
        tmp |= USBHS_EP_T_RES_ACK;
        tmp |= (epx_tx_data_toggle[ep_idx - 1] ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
        USB_SET_TX_CTRL(ep_idx, tmp);
    }
    return 0;
}

int ch32hs_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_ch32_usbhs_udc.out_ep[ep_idx].ep_enable) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }

    g_ch32_usbhs_udc.out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_ch32_usbhs_udc.out_ep[ep_idx].xfer_len = data_len;
    g_ch32_usbhs_udc.out_ep[ep_idx].actual_xfer_len = 0;

    if (ep_idx == 0) {
        if (data_len == 0) {
            USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_1;
        } else {
            USBHS_DEVICE->UEP0_DMA = (uint32_t)data;
            USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK | (ep0_rx_data_toggle ? USBHS_EP_R_TOG_1 : USBHS_EP_R_TOG_0);
        }
        return 0;
    } else {
        USB_SET_RX_DMA(ep_idx, (uint32_t)data);
        USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_ACK);
    }

    return 0;
}

void ch32hs_udc_irq(struct usbd_bus *bus)
{
    uint32_t ep_idx, token, write_count, read_count;
    uint8_t intflag = 0;

    intflag = USBHS_DEVICE->INT_FG;

    if (intflag & USBHS_TRANSFER_FLAG) {
        ep_idx = (USBHS_DEVICE->INT_ST) & MASK_UIS_ENDP;
        token = (((USBHS_DEVICE->INT_ST) & MASK_UIS_TOKEN) >> 4) & 0x03;

        if (token == PID_IN) {
            if (ep_idx == 0x00) {
                if (g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len > g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps) {
                    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len -= g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps;
                    g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps;
                    ep0_tx_data_toggle ^= 1;
                } else {
                    g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len;
                    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len = 0;
                    ep0_tx_data_toggle = true;
                }

                usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len);

                if (g_ch32_usbhs_udc.dev_addr > 0) {
                    USBHS_DEVICE->DEV_AD = g_ch32_usbhs_udc.dev_addr & 0xff;
                    g_ch32_usbhs_udc.dev_addr = 0;
                }

                if (g_ch32_usbhs_udc.setup.wLength && ((g_ch32_usbhs_udc.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                    /* In status, start reading setup */
                    USBHS_DEVICE->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc.setup;
                    USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK;
                    ep0_tx_data_toggle = true;

                } else if (g_ch32_usbhs_udc.setup.wLength == 0) {
                    /* In status, start reading setup */
                    USBHS_DEVICE->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc.setup;
                    USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK;
                    ep0_tx_data_toggle = true;
                }
            } else {
                USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK)) | USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0);

                if (g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len > g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps) {
                    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_buf += g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps;
                    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len -= g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps;
                    g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps;
                    epx_tx_data_toggle[ep_idx - 1] ^= 1;

                    write_count = MIN(g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len, g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps);
                    USB_SET_TX_LEN(ep_idx, write_count);
                    USB_SET_TX_DMA(ep_idx, (uint32_t)g_ch32_usbhs_udc.in_ep[ep_idx].xfer_buf);

                    uint32_t tmp = USB_GET_TX_CTRL(ep_idx);
                    tmp &= ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK);
                    tmp |= USBHS_EP_T_RES_ACK;
                    tmp |= (epx_tx_data_toggle[ep_idx - 1] ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
                    USB_SET_TX_CTRL(ep_idx, tmp);
                } else {
                    g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len += g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len;
                    g_ch32_usbhs_udc.in_ep[ep_idx].xfer_len = 0;
                    epx_tx_data_toggle[ep_idx - 1] ^= 1;
                    usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, g_ch32_usbhs_udc.in_ep[ep_idx].actual_xfer_len);
                }
            }
        } else if (token == PID_OUT) {
            if (ep_idx == 0x00) {
                read_count = USBHS_DEVICE->RX_LEN;

                g_ch32_usbhs_udc.out_ep[ep_idx].actual_xfer_len += read_count;
                g_ch32_usbhs_udc.out_ep[ep_idx].xfer_len -= read_count;

                usbd_event_ep_out_complete_handler(bus->busid, 0x00, g_ch32_usbhs_udc.out_ep[ep_idx].actual_xfer_len);

                if (read_count == 0) {
                    /* Out status, start reading setup */
                    USBHS_DEVICE->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc.setup;
                    USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK;
                    ep0_rx_data_toggle = true;
                    ep0_tx_data_toggle = true;
                } else {
                    ep0_rx_data_toggle ^= 1;
                }
            } else {
                if (USBHS_DEVICE->INT_ST & USBHS_DEV_UIS_TOG_OK) {
                    USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_NAK);
                    read_count = USBHS_DEVICE->RX_LEN;

                    g_ch32_usbhs_udc.out_ep[ep_idx].xfer_buf += read_count;
                    g_ch32_usbhs_udc.out_ep[ep_idx].actual_xfer_len += read_count;
                    g_ch32_usbhs_udc.out_ep[ep_idx].xfer_len -= read_count;

                    if ((read_count < g_ch32_usbhs_udc.out_ep[ep_idx].ep_mps) || (g_ch32_usbhs_udc.out_ep[ep_idx].xfer_len == 0)) {
                        usbd_event_ep_out_complete_handler(bus->busid, ep_idx, g_ch32_usbhs_udc.out_ep[ep_idx].actual_xfer_len);
                    } else {
                        USB_SET_RX_DMA(ep_idx, (uint32_t)g_ch32_usbhs_udc.out_ep[ep_idx].xfer_buf);
                        USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_ACK);
                    }
                }
            }
        }
        USBHS_DEVICE->INT_FG = USBHS_TRANSFER_FLAG;
    } else if (intflag & USBHS_SETUP_FLAG) {
        usbd_event_ep0_setup_complete_handler(bus->busid, (uint8_t *)&g_ch32_usbhs_udc.setup);
        USBHS_DEVICE->INT_FG = USBHS_SETUP_FLAG;
    } else if (intflag & USBHS_DETECT_FLAG) {
        USBHS_DEVICE->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN;

        USBHS_DEVICE->UEP0_TX_LEN = 0;
        USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_NAK;

        ep0_tx_data_toggle = true;
        ep0_rx_data_toggle = true;

        for (uint8_t ep_idx = 1; ep_idx < CONFIG_CH32HS_BIDIR_ENDPOINTS; ep_idx++) {
            USB_SET_TX_LEN(ep_idx, 0);
            USB_SET_TX_CTRL(ep_idx, USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK); // autotog does not work
            USB_SET_RX_CTRL(ep_idx, USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_NAK);
            epx_tx_data_toggle[ep_idx - 1] = false;
        }

        memset(&g_ch32_usbhs_udc, 0, sizeof(struct ch32_usbhs_udc));
        usbd_event_reset_handler(bus->busid);
        USBHS_DEVICE->UEP0_DMA = (uint32_t)&g_ch32_usbhs_udc.setup;
        USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK;
        USBHS_DEVICE->INT_FG = USBHS_DETECT_FLAG;
    }
}

struct usbd_udc_driver ch32hs_udc_driver = {
    .driver_name = "ch32hs udc",
    .udc_init = ch32hs_udc_init,
    .udc_deinit = ch32hs_udc_deinit,
    .udc_set_address = ch32hs_set_address,
    .udc_get_port_speed = ch32hs_get_port_speed,
    .udc_ep_open = ch32hs_ep_open,
    .udc_ep_close = ch32hs_ep_close,
    .udc_ep_set_stall = ch32hs_ep_set_stall,
    .udc_ep_clear_stall = ch32hs_ep_clear_stall,
    .udc_ep_is_stalled = ch32hs_ep_is_stalled,
    .udc_ep_start_write = ch32hs_ep_start_write,
    .udc_ep_start_read = ch32hs_ep_start_read,
    .udc_irq = ch32hs_udc_irq
};

void USBHS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void USBHS_IRQHandler(void)
{
    usbd_irq(1);
}