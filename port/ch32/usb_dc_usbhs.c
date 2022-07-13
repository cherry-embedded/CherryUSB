#include "usbd_core.h"
#include "usb_ch32_usbhs_reg.h"

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USBHS_IRQHandler //use actual usb irq name instead
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 16
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
struct ch32_usbhs_ep_state {
    /** Endpoint max packet size */
    uint16_t ep_mps;
    /** Endpoint Transfer Type.
     * May be Bulk, Interrupt, Control or Isochronous
     */
    uint8_t ep_type;
    uint8_t ep_stalled; /** Endpoint stall flag */
};

/* Driver state */
struct ch32_usbhs_udc {
    volatile uint8_t dev_addr;
    struct ch32_usbhs_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];                              /*!< IN endpoint parameters*/
    struct ch32_usbhs_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS];                             /*!< OUT endpoint parameters */
    __attribute__((aligned(4))) uint8_t ep_databuf[USB_NUM_BIDIR_ENDPOINTS - 1][512 + 512]; //epx_out(512)+epx_in(512)
} g_ch32_usbhs_udc;

// clang-format off
/* Endpoint Buffer */
__attribute__ ((aligned(4))) uint8_t EP0_DatabufHD[64]; //ep0(64)
__attribute__ ((aligned(4))) uint8_t EP1_DatabufHD[512+512];  //ep1_out(64)+ep1_in(64)
__attribute__ ((aligned(4))) uint8_t EP2_DatabufHD[512+512];  //ep2_out(64)+ep2_in(64)
// clang-format on

void USBHS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

volatile bool ep0_data_toggle = 0x01;
volatile uint8_t mps_over_flag = 0;
volatile bool epx_data_toggle[USB_NUM_BIDIR_ENDPOINTS - 1];

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    memset(&g_ch32_usbhs_udc, 0, sizeof(struct ch32_usbhs_udc));

    usb_dc_low_level_init();

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

    USBHS_DEVICE->UEP0_DMA = (uint32_t)EP0_DatabufHD;

    for (uint8_t ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
        USB_SET_RX_DMA(ep_idx, (uint32_t)&g_ch32_usbhs_udc.ep_databuf[ep_idx - 1][0]);
        USB_SET_TX_DMA(ep_idx, (uint32_t)&g_ch32_usbhs_udc.ep_databuf[ep_idx - 1][512]);
    }

    USBHS_DEVICE->CONTROL |= USBHS_DEV_PU_EN;

    return 0;
}

int usb_dc_deinit(void)
{
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        USBHS_DEVICE->DEV_AD = addr & 0xff;
    }
    g_ch32_usbhs_udc.dev_addr = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        g_ch32_usbhs_udc.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_ch32_usbhs_udc.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USBHS_DEVICE->ENDP_CONFIG |= (1 << (ep_idx + 16));
        USB_SET_RX_CTRL(ep_idx, USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0 | USBHS_EP_R_AUTOTOG);
    } else {
        g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_ch32_usbhs_udc.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USBHS_DEVICE->ENDP_CONFIG |= (1 << (ep_idx));
        USB_SET_TX_CTRL(ep_idx, USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0 | USBHS_EP_T_AUTOTOG);
    }
    USB_SET_MAX_LEN(ep_idx, ep_cfg->ep_mps);
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

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_SET_RX_CTRL(ep_idx, USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0);
    } else {
        USB_SET_TX_CTRL(ep_idx, USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0);
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
    uint32_t tmp;

    if (!data && data_len) {
        return -1;
    }

    while (((USB_GET_TX_CTRL(ep_idx) & USBHS_EP_T_RES_MASK) == USBHS_EP_T_RES_ACK) && (ep_idx != 0)) {
    }

    if (!data_len) {
        if (ep_idx == 0) {
            USB_SET_TX_LEN(ep_idx, 0);
        } else {
            USB_SET_TX_LEN(ep_idx, 0);
            tmp = USB_GET_TX_CTRL(ep_idx);
            tmp &= ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK);
            tmp |= USBHS_EP_T_RES_ACK;
            tmp |= (epx_data_toggle[ep_idx - 1] ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
            USB_SET_TX_CTRL(ep_idx, tmp);
            epx_data_toggle[ep_idx - 1] ^= 1;
        }
        return 0;
    }

    if (data_len >= g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps) {
        data_len = g_ch32_usbhs_udc.in_ep[ep_idx].ep_mps;
        if (ep_idx == 0) {
            mps_over_flag = 1;
        }
    }

    if (ep_idx == 0) {
        memcpy(&EP0_DatabufHD[0], data, data_len);
        USB_SET_TX_LEN(ep_idx, data_len);
    } else {
        USB_SET_TX_LEN(ep_idx, data_len);
        memcpy(&g_ch32_usbhs_udc.ep_databuf[ep_idx - 1][512], data, data_len);

        tmp = USB_GET_TX_CTRL(ep_idx);
        tmp &= ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK);
        tmp |= USBHS_EP_T_RES_ACK;
        tmp |= (epx_data_toggle[ep_idx - 1] ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0);
        USB_SET_TX_CTRL(ep_idx, tmp);
        epx_data_toggle[ep_idx - 1] ^= 1;
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
            USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_ACK);
        }
        return 0;
    }

    read_count = USBHS_DEVICE->RX_LEN;
    read_count = MIN(read_count, max_data_len);

    if (ep_idx == 0x00) {
        if ((max_data_len == 8) && !read_bytes) {
            read_count = 8;
            memcpy(data, &EP0_DatabufHD[0], 8);
        } else {
            memcpy(data, &EP0_DatabufHD[0], read_count);
        }
    } else {
        memcpy(data, &g_ch32_usbhs_udc.ep_databuf[ep_idx - 1][0], read_count);
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

    intflag = USBHS_DEVICE->INT_FG;

    if (intflag & USBHS_TRANSFER_FLAG) {
        ep_idx = (USBHS_DEVICE->INT_ST) & MASK_UIS_ENDP;
        token = (((USBHS_DEVICE->INT_ST) & MASK_UIS_TOKEN) >> 4) & 0x03;

        if (token == PID_IN) {
            if (ep_idx == 0x00) {
                usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                if (g_ch32_usbhs_udc.dev_addr > 0) {
                    USBHS_DEVICE->DEV_AD = g_ch32_usbhs_udc.dev_addr & 0xff;
                    g_ch32_usbhs_udc.dev_addr = 0;
                }
                if (mps_over_flag) {
                    mps_over_flag = 0;
                    ep0_data_toggle ^= 1;
                    USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_ACK | (ep0_data_toggle ? USBHS_EP_T_TOG_0 : USBHS_EP_T_TOG_1);
                } else {
                    USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_1;
                }
            } else {
                USB_SET_TX_CTRL(ep_idx, (USB_GET_TX_CTRL(ep_idx) & ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK)) | USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0);
                usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(ep_idx | 0x80));
            }
        } else if (token == PID_OUT) {
            if (ep_idx == 0x00) {
                usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_ACK | USBHS_EP_T_TOG_1;
            } else {
                if (USBHS_DEVICE->INT_ST & USBHS_DEV_UIS_TOG_OK) {
                    USB_SET_RX_CTRL(ep_idx, (USB_GET_RX_CTRL(ep_idx) & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_NAK);
                    usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(ep_idx & 0x7f));
                }
            }
        }
        USBHS_DEVICE->INT_FG = USBHS_TRANSFER_FLAG;
    } else if (intflag & USBHS_SETUP_FLAG) {
        usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
        USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_ACK | USBHS_EP_T_TOG_1;
        USBHS_DEVICE->INT_FG = USBHS_SETUP_FLAG;
    } else if (intflag & USBHS_DETECT_FLAG) {
        USBHS_DEVICE->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN;

        USBHS_DEVICE->UEP0_TX_LEN = 0;
        USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_NAK;
        USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK;

        for (uint8_t ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
            USB_SET_TX_LEN(ep_idx, 0);
            USB_SET_TX_CTRL(ep_idx, USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK); // autotog does not work
            USB_SET_RX_CTRL(ep_idx, USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_NAK);
            epx_data_toggle[ep_idx - 1] = false;
        }

        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        USBHS_DEVICE->INT_FG = USBHS_DETECT_FLAG;
    }
}
