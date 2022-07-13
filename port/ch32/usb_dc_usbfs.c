#include "usbd_core.h"
#include "usb_ch32_usbfs_reg.h"

#ifdef CONFIG_USB_HS
#error "usb fs do not support hs"
#endif

#ifndef USBD_IRQHandler
#define USBD_IRQHandler OTG_FS_IRQHandler //use actual usb irq name instead
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
#endif

#define USB_SET_TX_LEN(ep_idx, len)  (*(volatile uint16_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_LEN) + 4 * ep_idx) = len)
#define USB_GET_TX_LEN(ep_idx)       (*(volatile uint16_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_LEN) + 4 * ep_idx))
#define USB_SET_TX_CTRL(ep_idx, val) (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_CTRL) + 4 * ep_idx) = val)
#define USB_GET_TX_CTRL(ep_idx)      (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_TX_CTRL) + 4 * ep_idx))
#define USB_SET_RX_CTRL(ep_idx, val) (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_RX_CTRL) + 4 * ep_idx) = val)
#define USB_GET_RX_CTRL(ep_idx)      (*(volatile uint8_t *)((uint32_t)(&USBFS_DEVICE->UEP0_RX_CTRL) + 4 * ep_idx))

/* Endpoint state */
struct ch32_usbfs_ep_state {
    /** Endpoint max packet size */
    uint16_t ep_mps;
    /** Endpoint Transfer Type.
     * May be Bulk, Interrupt, Control or Isochronous
     */
    uint8_t ep_type;
    uint8_t ep_stalled; /** Endpoint stall flag */
};

/* Driver state */
struct ch32_usbfs_udc {
    volatile uint8_t dev_addr;
    struct ch32_usbfs_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];                            /*!< IN endpoint parameters*/
    struct ch32_usbfs_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS];                           /*!< OUT endpoint parameters */
    __attribute__((aligned(4))) uint8_t ep_databuf[USB_NUM_BIDIR_ENDPOINTS - 1][64 + 64]; //epx_out(64)+epx_in(64)
} g_ch32_usbfs_udc;

/* Endpoint0 Buffer */
__attribute__((aligned(4))) uint8_t EP0_DatabufHD[64]; //ep0(64)

void USBD_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

volatile uint8_t mps_over_flag = 0;

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    memset(&g_ch32_usbfs_udc, 0, sizeof(struct ch32_usbfs_udc));

    usb_dc_low_level_init();

    USBFS_DEVICE->BASE_CTRL = 0x00;

    USBFS_DEVICE->UEP4_1_MOD = USBFS_UEP4_RX_EN | USBFS_UEP4_TX_EN | USBFS_UEP1_RX_EN | USBFS_UEP1_TX_EN;
    USBFS_DEVICE->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP2_TX_EN | USBFS_UEP3_RX_EN | USBFS_UEP3_TX_EN;
    USBFS_DEVICE->UEP5_6_MOD = USBFS_UEP5_RX_EN | USBFS_UEP5_TX_EN | USBFS_UEP6_RX_EN | USBFS_UEP6_TX_EN;
    USBFS_DEVICE->UEP7_MOD = USBFS_UEP7_RX_EN | USBFS_UEP7_TX_EN;

    USBFS_DEVICE->UEP0_DMA = (uint32_t)EP0_DatabufHD;
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

int usb_dc_deinit(void)
{
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        USBFS_DEVICE->DEV_ADDR = (USBFS_DEVICE->DEV_ADDR & USBFS_UDA_GP_BIT) | 0;
    }
    g_ch32_usbfs_udc.dev_addr = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        g_ch32_usbfs_udc.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_ch32_usbfs_udc.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_ACK | USBFS_UEP_AUTO_TOG);
    } else {
        g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_ch32_usbfs_udc.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USB_SET_TX_CTRL(ep_idx, USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG);
    }
    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
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

    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
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
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }

    while (((USB_GET_TX_CTRL(ep_idx) & USBFS_UEP_T_RES_MASK) == USBFS_UEP_T_RES_ACK) && (ep_idx != 0)) {
    }

    if (!data_len) {
        if (ep_idx == 0) {
            USB_SET_TX_LEN(ep_idx, 0);
        } else {
            USB_SET_TX_LEN(ep_idx, 0);
            USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK);
        }
        return 0;
    }

    if (data_len >= g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps) {
        data_len = g_ch32_usbfs_udc.in_ep[ep_idx].ep_mps;

        if (ep_idx == 0) {
            mps_over_flag = 1;
        }
    }

    if (ep_idx == 0) {
        memcpy(&EP0_DatabufHD[0], data, data_len);
        USB_SET_TX_LEN(ep_idx, data_len);
    } else {
        memcpy(&g_ch32_usbfs_udc.ep_databuf[ep_idx - 1][64], data, data_len);
        USB_SET_TX_LEN(ep_idx, data_len);
        USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK);
    }
    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t read_count;

    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
        if (ep_idx) {
            USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK);
        }
        return 0;
    }

    read_count = USBFS_DEVICE->RX_LEN;
    read_count = MIN(read_count, max_data_len);

    if (ep_idx == 0x00) {
        if ((max_data_len == 8) && !read_bytes) {
            read_count = 8;
            memcpy(data, &EP0_DatabufHD[0], 8);
        } else {
            memcpy(data, &EP0_DatabufHD[0], read_count);
        }
    } else {
        memcpy(data, &g_ch32_usbfs_udc.ep_databuf[ep_idx - 1][0], read_count);
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

void USBD_IRQHandler(void)
{
    uint32_t ep_idx, token;
    uint8_t intflag = 0;

    intflag = USBFS_DEVICE->INT_FG;

    if (intflag & USBFS_UIF_TRANSFER) {
        token = USBFS_DEVICE->INT_ST & USBFS_UIS_TOKEN_MASK;
        ep_idx = USBFS_DEVICE->INT_ST & USBFS_UIS_ENDP_MASK;
        switch (token) {
            case USBFS_UIS_TOKEN_SETUP:
                USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;

                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);

                USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                break;

            case USBFS_UIS_TOKEN_IN:
                if (ep_idx == 0x00) {
                    usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                    if (g_ch32_usbfs_udc.dev_addr > 0) {
                        USBFS_DEVICE->DEV_ADDR = (USBFS_DEVICE->DEV_ADDR & USBFS_UDA_GP_BIT) | g_ch32_usbfs_udc.dev_addr;
                        g_ch32_usbfs_udc.dev_addr = 0;
                    }

                    if (mps_over_flag) {
                        mps_over_flag = 0;
                        USBFS_DEVICE->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                    } else {
                        USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
                    }
                } else {
                    USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK);
                    usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(ep_idx | 0x80));
                }
                break;
            case USBFS_UIS_TOKEN_OUT:
                if (ep_idx == 0x00) {
                    usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                } else {
                    if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                        USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_NAK);
                        usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(ep_idx));
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
        USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;

        for (uint8_t ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
            USB_SET_TX_LEN(ep_idx, 0);
            USB_SET_TX_CTRL(ep_idx, USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG);
            USB_SET_RX_CTRL(ep_idx, USBFS_UEP_R_RES_NAK | USBFS_UEP_AUTO_TOG);
        }

        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);

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
