#include "stm32f1xx_hal.h" //chanage this header for different soc
#include "usbd_core.h"

#ifndef USB_RAM_SIZE
#define USB_RAM_SIZE 512
#endif
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
#ifdef USB
    uint16_t ep_pma_buf_len; /** Previously allocated buffer size */
    uint16_t ep_pma_addr;    /**ep pmd allocated addr*/
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
};

/* Driver state */
struct usb_dc_config_priv {
    PCD_TypeDef *Instance;                                  /*!< Register base address              */
    __IO uint8_t USB_Address;                               /*!< USB Address                       */
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters             */
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters            */
#ifdef USB
    uint32_t pma_offset;
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
} usb_dc_cfg;

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));
#ifdef USB
    usb_dc_cfg.Instance = USB;
    usb_dc_cfg.pma_offset = 64;
#elif defined(USB_OTG_FS)
    usb_dc_cfg.Instance = USB_OTG_FS;
#elif defined(USB_OTG_HS)
    usb_dc_cfg.Instance = USB_OTG_HS;
#endif

    HAL_PCD_MspInit((PCD_HandleTypeDef *)&usb_dc_cfg);

#ifdef USB
    /* CNTR_FRES = 1 */
    USB->CNTR = (uint16_t)USB_CNTR_FRES;

    /* CNTR_FRES = 0 */
    USB->CNTR = 0U;

    /* Clear pending interrupts */
    USB->ISTR = 0U;

    /*Set Btable Address*/
    USB->BTABLE = BTABLE_ADDRESS;

    USB->DADDR = (uint16_t)USB_DADDR_EF;

    /* Set winterruptmask variable */
    uint32_t winterruptmask = USB_CNTR_CTRM | USB_CNTR_WKUPM |
                              USB_CNTR_SUSPM | USB_CNTR_ERRM |
                              USB_CNTR_SOFM | USB_CNTR_ESOFM |
                              USB_CNTR_RESETM;

    /* Set interrupt mask */
    USB->CNTR = (uint16_t)winterruptmask;
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    return 0;
}

void usb_dc_deinit(void)
{
    HAL_PCD_MspDeInit((PCD_HandleTypeDef *)&usb_dc_cfg);
}

int usbd_set_address(const uint8_t addr)
{
#ifdef USB
    if (addr == 0U) {
        /* set device address and enable function */
        USB->DADDR = (uint16_t)USB_DADDR_EF;
    }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    usb_dc_cfg.USB_Address = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep;
    uint8_t ep_idx;
    if (!ep_cfg) {
        return -1;
    }

    ep = ep_cfg->ep_addr;
    ep_idx = USB_EP_GET_IDX(ep);

#ifdef USB
    uint16_t wEpRegVal;

    wEpRegVal = PCD_GET_ENDPOINT(USB, ep_idx) & USB_EP_T_MASK;
    /* initialize Endpoint */
    switch (ep_cfg->ep_type) {
        case EP_TYPE_CTRL:
            wEpRegVal |= USB_EP_CONTROL;
            break;

        case EP_TYPE_BULK:
            wEpRegVal |= USB_EP_BULK;
            break;

        case EP_TYPE_INTR:
            wEpRegVal |= USB_EP_INTERRUPT;
            break;

        case EP_TYPE_ISOC:
            wEpRegVal |= USB_EP_ISOCHRONOUS;
            break;

        default:
            break;
    }
    PCD_SET_ENDPOINT(USB, ep_idx, (wEpRegVal | USB_EP_CTR_RX | USB_EP_CTR_TX));

    PCD_SET_EP_ADDRESS(USB, ep_idx, ep_idx);
    if (USB_EP_DIR_IS_OUT(ep)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        if (usb_dc_cfg.out_ep[ep_idx].ep_mps > usb_dc_cfg.out_ep[ep_idx].ep_pma_buf_len) {
            if (usb_dc_cfg.pma_offset + usb_dc_cfg.out_ep[ep_idx].ep_mps >= 512) {
                return -1;
            }
            usb_dc_cfg.out_ep[ep_idx].ep_pma_buf_len = ep_cfg->ep_mps;
            usb_dc_cfg.out_ep[ep_idx].ep_pma_addr = usb_dc_cfg.pma_offset;
            /*Set the endpoint Receive buffer address */
            PCD_SET_EP_RX_ADDRESS(USB, ep_idx, usb_dc_cfg.pma_offset);
            usb_dc_cfg.pma_offset += ep_cfg->ep_mps;
        }
        /*Set the endpoint Receive buffer counter*/
        PCD_SET_EP_RX_CNT(USB, ep_idx, ep_cfg->ep_mps);
        PCD_CLEAR_RX_DTOG(USB, ep_idx);

        /* Configure VALID status for the Endpoint*/
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        if (usb_dc_cfg.in_ep[ep_idx].ep_mps > usb_dc_cfg.in_ep[ep_idx].ep_pma_buf_len) {
            if (usb_dc_cfg.pma_offset + usb_dc_cfg.in_ep[ep_idx].ep_mps >= USB_RAM_SIZE) {
                return -1;
            }
            usb_dc_cfg.in_ep[ep_idx].ep_pma_buf_len = ep_cfg->ep_mps;
            usb_dc_cfg.in_ep[ep_idx].ep_pma_addr = usb_dc_cfg.pma_offset;
            /*Set the endpoint Transmit buffer address */
            PCD_SET_EP_TX_ADDRESS(USB, ep_idx, usb_dc_cfg.pma_offset);
            usb_dc_cfg.pma_offset += ep_cfg->ep_mps;
        }

        PCD_CLEAR_TX_DTOG(USB, ep_idx);
        if (ep_cfg->ep_type != EP_TYPE_ISOC) {
            /* Configure NAK status for the Endpoint */
            PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_NAK);
        } else {
            /* Configure TX Endpoint to disabled state */
            PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_DIS);
        }
    }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif

    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
        PCD_CLEAR_RX_DTOG(USB, ep_idx);

        /* Configure DISABLE status for the Endpoint*/
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_DIS);
    } else {
        PCD_CLEAR_TX_DTOG(USB, ep_idx);

        /* Configure DISABLE status for the Endpoint*/
        PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_DIS);
    }
    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
#ifdef USB
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_STALL);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    } else {
#ifdef USB
        PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_STALL);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    }
    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_OUT(ep)) {
#ifdef USB
        PCD_CLEAR_TX_DTOG(USB, ep_idx);

        if (usb_dc_cfg.in_ep[ep_idx].ep_type != EP_TYPE_ISOC) {
            /* Configure NAK status for the Endpoint */
            PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_NAK);
        }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    } else {
#ifdef USB
        PCD_CLEAR_RX_DTOG(USB, ep_idx);
        /* Configure VALID status for the Endpoint */
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    }
    return 0;
}
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
    } else {
    }
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (!data && data_len) {
        return -1;
    }

    if (!data_len) {
#ifdef USB
        PCD_SET_EP_TX_CNT(USB, ep_idx, (uint16_t)0);
        PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }
#ifdef USB
    USB_WritePMA(USB, (uint8_t *)data, usb_dc_cfg.in_ep[ep_idx].ep_pma_addr, (uint16_t)data_len);
    PCD_SET_EP_TX_CNT(USB, ep_idx, (uint16_t)data_len);
    PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    uint32_t read_count;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
#ifdef USB
        //PCD_SET_EP_RX_CNT(USB, ep_idx, usb_dc_cfg.out_ep[ep_idx].ep_mps);
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
        return 0;
    }
#ifdef USB
    read_count = PCD_GET_EP_RX_CNT(USB, ep_idx);
    read_count = MIN(read_count, max_data_len);
    USB_ReadPMA(USB, (uint8_t *)data,
                usb_dc_cfg.out_ep[ep_idx].ep_pma_addr, (uint16_t)read_count);
    PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
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
#ifdef USB
    uint16_t wIstr, wEPVal;
    uint8_t epindex;
    wIstr = USB->ISTR;

    uint16_t store_ep[8];
    if (wIstr & USB_ISTR_CTR) {
        while ((USB->ISTR & USB_ISTR_CTR) != 0U) {
            wIstr = USB->ISTR;

            /* extract highest priority endpoint number */
            epindex = (uint8_t)(wIstr & USB_ISTR_EP_ID);

            if (epindex == 0U) {
                /* Decode and service control endpoint interrupt */

                /* DIR bit = origin of the interrupt */
                if ((wIstr & USB_ISTR_DIR) == 0U) {
                    /* DIR = 0 */

                    /* DIR = 0 => IN  int */
                    /* DIR = 0 implies that (EP_CTR_TX = 1) always */
                    PCD_CLEAR_TX_EP_CTR(USB, PCD_ENDP0);
                    usbd_event_notify_handler(USB_EVENT_EP0_IN_NOTIFY, NULL);
                    if ((usb_dc_cfg.USB_Address > 0U) && (PCD_GET_EP_TX_CNT(USB, PCD_ENDP0) == 0U)) {
                        USB->DADDR = ((uint16_t)usb_dc_cfg.USB_Address | USB_DADDR_EF);
                        usb_dc_cfg.USB_Address = 0U;
                    }
                }
                /* DIR = 1 */

                /* DIR = 1 & CTR_RX => SETUP or OUT int */
                /* DIR = 1 & (CTR_TX | CTR_RX) => 2 int pending */

                wEPVal = PCD_GET_ENDPOINT(USB, PCD_ENDP0);

                if ((wEPVal & USB_EP_SETUP) != 0U) {
                    /* SETUP bit kept frozen while CTR_RX = 1 */
                    PCD_CLEAR_RX_EP_CTR(USB, PCD_ENDP0);

                    /* Process SETUP Packet*/
                    usbd_event_notify_handler(USB_EVENT_SETUP_NOTIFY, NULL);
                } else if ((wEPVal & USB_EP_CTR_RX) != 0U) {
                    PCD_CLEAR_RX_EP_CTR(USB, PCD_ENDP0);
                    /* Process Control Data OUT Packet */
                    usbd_event_notify_handler(USB_EVENT_EP0_OUT_NOTIFY, NULL);
                }
            } else {
                /* Decode and service non control endpoints interrupt */
                /* process related endpoint register */
                wEPVal = PCD_GET_ENDPOINT(USB, epindex);

                if ((wEPVal & USB_EP_CTR_RX) != 0U) {
                    /* clear int flag */
                    PCD_CLEAR_RX_EP_CTR(USB, epindex);
                    usbd_event_notify_handler(USB_EVENT_EP_OUT_NOTIFY, (void *)(epindex & 0x7f));
                }

                if ((wEPVal & USB_EP_CTR_TX) != 0U) {
                    /* clear int flag */
                    PCD_CLEAR_TX_EP_CTR(USB, epindex);
                    usbd_event_notify_handler(USB_EVENT_EP_IN_NOTIFY, (void *)(epindex | 0x80));
                }
            }
        }
    }
    if (wIstr & USB_ISTR_RESET) {
        struct usbd_endpoint_cfg ep0_cfg;
        /* Configure control EP */
        ep0_cfg.ep_mps = 64;
        ep0_cfg.ep_type = USB_DC_EP_CONTROL;

        ep0_cfg.ep_addr = USB_CONTROL_OUT_EP0;
        usbd_ep_open(&ep0_cfg);

        ep0_cfg.ep_addr = USB_CONTROL_IN_EP0;
        usbd_ep_open(&ep0_cfg);
        usbd_event_notify_handler(USB_EVENT_RESET, NULL);

        USB->ISTR &= (uint16_t)(~USB_ISTR_RESET);
    }
    if (wIstr & USB_ISTR_PMAOVR) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_PMAOVR);
    }
    if (wIstr & USB_ISTR_ERR) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_ERR);
    }
    if (wIstr & USB_ISTR_WKUP) {
        USB->CNTR &= (uint16_t) ~(USB_CNTR_LP_MODE);
        USB->CNTR &= (uint16_t) ~(USB_CNTR_FSUSP);

        USB->ISTR &= (uint16_t)(~USB_ISTR_WKUP);
    }
    if (wIstr & USB_ISTR_SUSP) {
        /* WA: To Clear Wakeup flag if raised with suspend signal */

        /* Store Endpoint register */
        for (uint8_t i = 0U; i < 8U; i++) {
            store_ep[i] = PCD_GET_ENDPOINT(USB, i);
        }

        /* FORCE RESET */
        USB->CNTR |= (uint16_t)(USB_CNTR_FRES);

        /* CLEAR RESET */
        USB->CNTR &= (uint16_t)(~USB_CNTR_FRES);

        /* wait for reset flag in ISTR */
        while ((USB->ISTR & USB_ISTR_RESET) == 0U) {
        }

        /* Clear Reset Flag */
        USB->ISTR &= (uint16_t)(~USB_ISTR_RESET);
        /* Restore Registre */
        for (uint8_t i = 0U; i < 8U; i++) {
            PCD_SET_ENDPOINT(USB, i, store_ep[i]);
        }

        /* Force low-power mode in the macrocell */
        USB->CNTR |= (uint16_t)USB_CNTR_FSUSP;

        /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
        USB->ISTR &= (uint16_t)(~USB_ISTR_SUSP);

        USB->CNTR |= (uint16_t)USB_CNTR_LP_MODE;
    }
    if (wIstr & USB_ISTR_SOF) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_SOF);
    }
    if (wIstr & USB_ISTR_ESOF) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_ESOF);
    }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
}