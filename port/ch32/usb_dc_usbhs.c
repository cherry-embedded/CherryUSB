#include "usbd_core.h"
#include "usb_ch32_usbhs_reg.h"

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
#endif

/* Endpoint state */
struct usb_dc_ep_state {
    /** Endpoint max packet size */
    uint16_t ep_mps;
    /** Endpoint Transfer Type.
     * May be Bulk, Interrupt, Control or Isochronous
     */
    uint8_t ep_type;
    uint8_t ep_stalled; /** Endpoint stall flag */
};

/* Driver state */
struct usb_dc_config_priv {
    uint8_t dev_addr;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

// clang-format off
/* Endpoint Buffer */
__attribute__ ((aligned(4))) uint8_t EP0_DatabufHD[64]; //ep0(64)
__attribute__ ((aligned(4))) uint8_t EP1_DatabufHD[512+512];  //ep1_out(64)+ep1_in(64)
__attribute__ ((aligned(4))) uint8_t EP2_DatabufHD[512+512];  //ep2_out(64)+ep2_in(64)
// clang-format on

void USBHS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

volatile uint8_t mps_over_flag = 0;
volatile uint8_t USBHS_Dev_Endp0_Tog = 0x01; /* USB2.0高速设备端点0同步标志 */

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));

    usb_dc_low_level_init();

    USBHS_DEVICE->HOST_CTRL = 0x00;
    USBHS_DEVICE->HOST_CTRL = USBHS_SUSPEND_EN;

    USBHS_DEVICE->CONTROL = 0;
#if 1
    USBHS_DEVICE->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_HIGH_SPEED;
#else
    USBHS_DEVICE->CONTROL = USBHS_DMA_EN | USBHS_INT_BUSY_EN | USBHS_FULL_SPEED;
#endif

    USBHS_DEVICE->INT_EN = 0;
    USBHS_DEVICE->INT_EN = USBHS_SETUP_ACT_EN | USBHS_TRANSFER_EN | USBHS_DETECT_EN | USBHS_SUSPEND_EN;

    /* ALL endpoint enable */
    USBHS_DEVICE->ENDP_CONFIG = 0xffffffff;

    USBHS_DEVICE->ENDP_CONFIG = USBHS_EP0_T_EN | USBHS_EP0_R_EN | USBHS_EP1_T_EN | USBHS_EP2_T_EN | USBHS_EP1_R_EN | USBHS_EP2_R_EN;
    USBHS_DEVICE->ENDP_TYPE = 0x00;
    USBHS_DEVICE->BUF_MODE = 0x00;

    USBHS_DEVICE->UEP0_MAX_LEN = 64;
    USBHS_DEVICE->UEP1_MAX_LEN = 512;
    USBHS_DEVICE->UEP2_MAX_LEN = 512;
    USBHS_DEVICE->UEP3_MAX_LEN = 512;
    USBHS_DEVICE->UEP4_MAX_LEN = 512;
    USBHS_DEVICE->UEP5_MAX_LEN = 512;
    USBHS_DEVICE->UEP6_MAX_LEN = 512;
    USBHS_DEVICE->UEP7_MAX_LEN = 512;
    USBHS_DEVICE->UEP8_MAX_LEN = 512;
    USBHS_DEVICE->UEP9_MAX_LEN = 512;
    USBHS_DEVICE->UEP10_MAX_LEN = 512;
    USBHS_DEVICE->UEP11_MAX_LEN = 512;
    USBHS_DEVICE->UEP12_MAX_LEN = 512;
    USBHS_DEVICE->UEP13_MAX_LEN = 512;
    USBHS_DEVICE->UEP14_MAX_LEN = 512;
    USBHS_DEVICE->UEP15_MAX_LEN = 512;

    USBHS_DEVICE->UEP0_DMA = (uint32_t)EP0_DatabufHD;
    USBHS_DEVICE->UEP1_TX_DMA = (uint32_t)&EP1_DatabufHD[512];
    USBHS_DEVICE->UEP1_RX_DMA = (uint32_t)&EP1_DatabufHD[0];
    USBHS_DEVICE->UEP2_TX_DMA = (uint32_t)&EP2_DatabufHD[512];
    USBHS_DEVICE->UEP2_RX_DMA = (uint32_t)&EP2_DatabufHD[0];

    USBHS_DEVICE->UEP0_TX_LEN = 0;
    USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP1_TX_LEN = 0;
    USBHS_DEVICE->UEP1_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP1_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP2_TX_LEN = 0;
    USBHS_DEVICE->UEP2_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP2_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP3_TX_LEN = 0;
    USBHS_DEVICE->UEP3_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP3_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP4_TX_LEN = 0;
    USBHS_DEVICE->UEP4_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP4_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP5_TX_LEN = 0;
    USBHS_DEVICE->UEP5_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP5_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP6_TX_LEN = 0;
    USBHS_DEVICE->UEP6_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP6_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP7_TX_LEN = 0;
    USBHS_DEVICE->UEP7_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP7_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP8_TX_LEN = 0;
    USBHS_DEVICE->UEP8_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP8_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP9_TX_LEN = 0;
    USBHS_DEVICE->UEP9_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP9_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP10_TX_LEN = 0;
    USBHS_DEVICE->UEP10_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP10_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP11_TX_LEN = 0;
    USBHS_DEVICE->UEP11_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP11_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP12_TX_LEN = 0;
    USBHS_DEVICE->UEP12_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP12_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP13_TX_LEN = 0;
    USBHS_DEVICE->UEP13_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP13_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP14_TX_LEN = 0;
    USBHS_DEVICE->UEP14_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP14_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->UEP15_TX_LEN = 0;
    USBHS_DEVICE->UEP15_TX_CTRL = USBHS_EP_T_AUTOTOG | USBHS_EP_T_RES_NAK;
    USBHS_DEVICE->UEP15_RX_CTRL = USBHS_EP_R_AUTOTOG | USBHS_EP_R_RES_ACK;

    USBHS_DEVICE->CONTROL |= USBHS_DEV_PU_EN;

    return 0;
}

void usb_dc_deinit(void)
{
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        USBHS_DEVICE->DEV_AD = addr & 0xff;
    }
    usb_dc_cfg.dev_addr = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
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
        switch (ep_idx) {
            case 0:
                USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_STALL;
                break;
            case 1:
                USBHS_DEVICE->UEP1_RX_CTRL = (USBHS_DEVICE->UEP1_RX_CTRL & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_STALL;
                break;
            case 2:
                USBHS_DEVICE->UEP2_RX_CTRL = (USBHS_DEVICE->UEP2_RX_CTRL & ~USBHS_EP_R_RES_MASK) | USBHS_EP_R_RES_STALL;
                break;
            default:
                break;
        }

    } else {
        switch (ep_idx) {
            case 0:
                USBHS_DEVICE->UEP0_TX_LEN = 0;
                USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_STALL;
                break;
            case 1:
                USBHS_DEVICE->UEP1_TX_CTRL = (USBHS_DEVICE->UEP1_TX_CTRL & ~USBHS_EP_T_RES_MASK) | USBHS_EP_T_RES_STALL;
                break;
            case 2:
                USBHS_DEVICE->UEP2_TX_CTRL = (USBHS_DEVICE->UEP2_TX_CTRL & ~USBHS_EP_T_RES_MASK) | USBHS_EP_T_RES_STALL;
                break;
            default:
                break;
        }
    }

    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        switch (ep_idx) {
            case 0:

                break;
            case 1:
                /* SET Endp1 Rx to USBHS_EP_R_RES_NAK;USBHS_EP_R_TOG_0 */
                USBHS_DEVICE->UEP1_RX_CTRL = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0;
                break;
            case 2:
                /* SET Endp2 Rx to USBHS_EP_R_RES_ACK;USBHS_EP_R_TOG_0 */
                USBHS_DEVICE->UEP2_TX_CTRL = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_0;
                break;
            default:
                break;
        }

    } else {
        switch (ep_idx) {
            case 0:

                break;
            case 1:
                /* SET Endp1 Tx to USBHS_EP_T_RES_NAK;USBHS_EP_T_TOG_0;len = 0 */
                USBHS_DEVICE->UEP1_TX_LEN = 0;
                USBHS_DEVICE->UEP1_TX_CTRL = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
                break;
            case 2:
                /* SET Endp2 Tx to USBHS_EP_T_RES_NAK;USBHS_EP_T_TOG_0;len = 0 */
                USBHS_DEVICE->UEP2_TX_LEN = 0;
                USBHS_DEVICE->UEP2_TX_CTRL = USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
                break;
            default:
                break;
        }
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

    if (!data_len) {
        switch (ep_idx) {
            case 0:
                USBHS_DEVICE->UEP0_TX_LEN = 0;
                USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_ACK | USBHS_EP_T_TOG_1;
                break;
            case 1:
                USBHS_DEVICE->UEP1_TX_LEN = 0;
                break;
            case 2:
                USBHS_DEVICE->UEP2_TX_LEN = 0;
                break;
            default:
                break;
        }

        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    switch (ep_idx) {
        case 0:
            memcpy(&EP0_DatabufHD[0], data, data_len);
            USBHS_DEVICE->UEP0_TX_LEN = data_len;
            USBHS_DEVICE->UEP0_TX_CTRL = USBHS_EP_T_RES_ACK | (USBHS_Dev_Endp0_Tog ? USBHS_EP_T_TOG_0 : USBHS_EP_T_TOG_1);
            USBHS_Dev_Endp0_Tog ^= 1;
            break;
        case 1:
            memcpy(&EP1_DatabufHD[512], data, data_len);
            USBHS_DEVICE->UEP1_TX_LEN = data_len;
            USBHS_DEVICE->UEP1_TX_CTRL = (USBHS_DEVICE->UEP1_TX_CTRL & ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_LEN_MASK | USBHS_EP_T_TOG_MASK)) | USBHS_EP_T_RES_ACK;
            //USBHS_DEVICE->UEP1_TX_CTRL |= ( USBHS_Endp1_T_Tog ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0 );
            break;
        case 2:
            memcpy(&EP2_DatabufHD[512], data, data_len);
            USBHS_DEVICE->UEP2_TX_LEN = data_len;
            USBHS_DEVICE->UEP2_TX_CTRL = (USBHS_DEVICE->UEP2_TX_CTRL & ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_LEN_MASK | USBHS_EP_T_TOG_MASK)) | USBHS_EP_T_RES_ACK;
            //USBHS_DEVICE->UEP2_TX_CTRL |= ( USBHS_Endp2_T_Tog ? USBHS_EP_T_TOG_1 : USBHS_EP_T_TOG_0 );
            break;
        default:
            break;
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
        return 0;
    }

    read_count = USBHS_DEVICE->RX_LEN;
    read_count = MIN(read_count, max_data_len);

    switch (ep_idx) {
        case 0:
            if ((max_data_len == 8) && !read_bytes) {
                read_count = 8;
                memcpy(data, &EP0_DatabufHD[0], 8);
            } else {
                memcpy(data, &EP0_DatabufHD[0], read_count);
            }
            break;
        case 1:
            memcpy(data, &EP1_DatabufHD[0], read_count);
            break;
        case 2:
            memcpy(data, &EP2_DatabufHD[0], read_count);
            break;
        default:
            break;
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

/*********************************************************************
 * @fn      USBHS_IRQHandler
 *
 * @brief   This function handles OTG_FS exception.
 *
 * @return  none
 */
void USBHS_IRQHandler(void)
{
    uint32_t end_num, rx_token;
    uint8_t intflag = 0;

    intflag = USBHS_DEVICE->INT_FG;

    if (intflag & USBHS_TRANSFER_FLAG) {
        end_num = (USBHS_DEVICE->INT_ST) & MASK_UIS_ENDP;
        rx_token = (((USBHS_DEVICE->INT_ST) & MASK_UIS_TOKEN) >> 4) & 0x03;
        if (end_num == 0) {
            if (rx_token == PID_IN) {
                usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                if (usb_dc_cfg.dev_addr > 0) {
                    USBHS_DEVICE->DEV_AD = usb_dc_cfg.dev_addr & 0xff;
                    usb_dc_cfg.dev_addr = 0;
                }
            } else if (rx_token == PID_OUT) {
                usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                USBHS_DEVICE->UEP0_RX_CTRL = USBHS_EP_R_RES_ACK | USBHS_EP_R_TOG_1;
            }
        } else if (end_num == 1) {
            if (rx_token == PID_IN) {
                //USBHS_Endp1_Up_Flag = 0x00;
                /* 默认回NAK */
                USBHS_DEVICE->UEP1_TX_CTRL = (USBHS_DEVICE->UEP1_TX_CTRL & ~(USBHS_EP_T_RES_MASK | USBHS_EP_T_TOG_MASK)) | USBHS_EP_T_RES_NAK | USBHS_EP_T_TOG_0;
            } else if (rx_token == PID_OUT) {
            }
        } else if (end_num == 2) {
            if (rx_token == PID_IN) {
            } else if (rx_token == PID_OUT) {
            }
        }
        USBHS_DEVICE->INT_FG = USBHS_TRANSFER_FLAG;
    } else if (intflag & USBHS_SETUP_FLAG) {
        usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
        USBHS_DEVICE->INT_FG = USBHS_SETUP_FLAG;
    } else if (intflag & USBHS_DETECT_FLAG) {
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        USBHS_DEVICE->INT_FG = USBHS_DETECT_FLAG;
    }
    printf("reset\r\n");
}
