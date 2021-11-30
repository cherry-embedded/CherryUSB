#include "usbd_core.h"
#include "ch32f10x.h"

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 5
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

/* Endpoint Buffer */
// clang-format off
__ALIGNED(4) uint8_t EP0_Databuf[64+64+64];	//ep0(64)+ep4_out(64)+ep4_in(64)
__ALIGNED(4) uint8_t EP1_Databuf[64+64];	//ep1_out(64)+ep1_in(64)
__ALIGNED(4) uint8_t EP2_Databuf[64+64];	//ep2_out(64)+ep2_in(64)
__ALIGNED(4) uint8_t EP3_Databuf[64+64];	//ep3_out(64)+ep3_in(64)
// clang-format on

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    EXTEN->EXTEN_CTR |= EXTEN_USBHD_IO_EN;
    RCC_USBCLKConfig(RCC_USBCLKSource_PLLCLK_1Div5); //USBclk=PLLclk/1.5=48Mhz
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHD, ENABLE);

    R8_USB_CTRL = 0x00;

    R8_UEP4_1_MOD = RB_UEP4_RX_EN | RB_UEP4_TX_EN | RB_UEP1_RX_EN | RB_UEP1_TX_EN;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN | RB_UEP3_RX_EN | RB_UEP3_TX_EN;

    R8_USB_INT_FG = 0xFF;
    R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;

    R8_USB_DEV_AD = 0x00;
    usb_dc_cfg.dev_addr = 0;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_UDEV_CTRL = RB_UD_PD_DIS | RB_UD_PORT_EN;

    NVIC_EnableIRQ(USBHD_IRQn);
    return 0;
}

void usb_dc_deinit(void)
{
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        R8_USB_DEV_AD = 0x00;
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

    switch (ep_idx) {
        case 0:
            R16_UEP0_DMA = (UINT16)(UINT32)EP0_Databuf;
            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        case 1:
            R16_UEP1_DMA = (UINT16)(UINT32)EP1_Databuf;
            R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        case 2:
            R16_UEP2_DMA = (UINT16)(UINT32)EP2_Databuf;
            R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        case 3:
            R16_UEP3_DMA = (UINT16)(UINT32)EP3_Databuf;
            R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        default:
            break;
    }

    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
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
        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    if (ep_idx == 0) {
        memcpy(&EP0_Databuf[0], data, data_len);
    } else if (ep_idx == 1) {
        memcpy(&EP1_Databuf[64], data, data_len);
        R8_UEP1_T_LEN = data_len;
        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    } else if (ep_idx == 2) {
        memcpy(&EP2_Databuf[64], data, data_len);
        R8_UEP2_T_LEN = data_len;
        R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    } else if (ep_idx == 3) {
        memcpy(&EP3_Databuf[64], data, data_len);
        R8_UEP3_T_LEN = data_len;
        R8_UEP3_CTRL = (R8_UEP3_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    } else if (ep_idx == 4) {
        memcpy(&EP0_Databuf[128], data, data_len);
        R8_UEP4_T_LEN = data_len;
        R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
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

    read_count = R8_USB_RX_LEN;
    read_count = MIN(read_count, max_data_len);
    if (ep_idx == 0) {
        memcpy(data, &EP0_Databuf[0], read_count);
    } else if (ep_idx == 1) {
        memcpy(data, &EP1_Databuf[0], read_count);
    } else if (ep_idx == 2) {
        memcpy(data, &EP2_Databuf[0], read_count);
    } else if (ep_idx == 3) {
        memcpy(data, &EP3_Databuf[0], read_count);
    } else if (ep_idx == 4) {
        memcpy(data, &EP0_Databuf[64], read_count);
    }
    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

/**
  * @brief  This function handles PCD interrupt request.
  * @param  hpcd PCD handle
  * @retval HAL status
  */
void USBD_IRQHandler(void)
{
    UINT8 len, chtype;
    UINT8 intflag, errflag = 0;

    intflag = R8_USB_INT_FG;

    if (intflag & RB_UIF_TRANSFER) {
        switch (R8_USB_INT_ST & MASK_UIS_TOKEN) {
            case UIS_TOKEN_SETUP:
                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
                break;

            case UIS_TOKEN_IN:
                switch (R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) {
                    case UIS_TOKEN_IN:
                        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                        if (usb_dc_cfg.dev_addr > 0) {
                            R8_USB_DEV_AD = (R8_USB_DEV_AD & RB_UDA_GP_BIT) | usb_dc_cfg.dev_addr;
                            usb_dc_cfg.dev_addr = 0;
                        }
                        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                        break;

                    case UIS_TOKEN_IN | 1:
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(1 | 0x80));
                        R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                        break;

                    case UIS_TOKEN_IN | 2:
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(2 | 0x80));
                        R8_UEP2_CTRL ^= RB_UEP_T_TOG;
                        R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                        break;

                    case UIS_TOKEN_IN | 3:
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(3 | 0x80));
                        R8_UEP3_CTRL ^= RB_UEP_T_TOG;
                        R8_UEP3_CTRL = (R8_UEP3_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                        break;

                    case UIS_TOKEN_IN | 4:
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(4 | 0x80));
                        R8_UEP4_CTRL ^= RB_UEP_T_TOG;
                        R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                        break;

                    default:
                        break;
                }
                break;

            case UIS_TOKEN_OUT:
                switch (R8_USB_INT_ST & (MASK_UIS_TOKEN | MASK_UIS_ENDP)) {
                    case UIS_TOKEN_OUT:
                        usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                        break;

                    case UIS_TOKEN_OUT | 1:
                        if (R8_USB_INT_ST & RB_UIS_TOG_OK) {
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(1 & 0x7f));
                            R8_UEP1_CTRL ^= RB_UEP_R_TOG;
                        }
                        break;

                    case UIS_TOKEN_OUT | 2:
                        if (R8_USB_INT_ST & RB_UIS_TOG_OK) {
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(2 & 0x7f));
                            R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                        }
                        break;

                    case UIS_TOKEN_OUT | 3:
                        if (R8_USB_INT_ST & RB_UIS_TOG_OK) {
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(3 & 0x7f));
                            R8_UEP3_CTRL ^= RB_UEP_R_TOG;
                        }
                        break;

                    case UIS_TOKEN_OUT | 4:
                        if (R8_USB_INT_ST & RB_UIS_TOG_OK) {
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(4 & 0x7f));
                            R8_UEP4_CTRL ^= RB_UEP_R_TOG;
                        }
                        break;
                }

                break;

            case UIS_TOKEN_SOF:

                break;

            default:
                break;
        }
        R8_USB_INT_FG = RB_UIF_TRANSFER;
    } else if (intflag & RB_UIF_BUS_RST) {
        R8_USB_DEV_AD = 0;
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        R8_USB_INT_FG |= RB_UIF_BUS_RST;
    } else if (intflag & RB_UIF_SUSPEND) {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    } else {
        R8_USB_INT_FG = intflag;
    }
}
