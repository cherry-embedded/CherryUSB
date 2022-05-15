#include "usbd_core.h"
#include "usb_mm32_reg.h"

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USB_HP_CAN1_TX_IRQHandler //use actual usb irq name instead
#endif

#define USB_BASE ((uint32_t)0x40005C00)
#define USB      ((USB_TypeDef *)USB_BASE)

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 5
#endif

#define USB_GET_EPX_INT_STATE(ep_idx)      (*(volatile uint32_t *)(&USB->rEP1_INT_STATE + (ep_idx - 1)))
#define USB_SET_EPX_INT_STATE(ep_idx, val) (*(volatile uint32_t *)(&USB->rEP1_INT_STATE + (ep_idx - 1)) = val)
#define USB_SET_EP_INT(ep_idx, val)        (*(volatile uint32_t *)(&USB->rEP1_INT_EN + (ep_idx - 1)) = val)
#define USB_GET_EP_RX_CNT(ep_idx)          (*(volatile uint32_t *)(&USB->rEP0_AVIL + ep_idx))
#define USB_GET_EP_CTRL(ep_idx)            (*(volatile uint32_t *)(&USB->rEP0_CTRL + ep_idx))
#define USB_SET_EP_CTRL(ep_idx, val)       (*(volatile uint32_t *)(&USB->rEP0_CTRL + ep_idx) = val)
#define USB_GET_EP_FIFO(ep_idx)            (*(volatile uint32_t *)(&USB->rEP0_FIFO + ep_idx))
#define USB_SET_EP_FIFO(ep_idx, val)       (*(volatile uint32_t *)(&USB->rEP0_FIFO + ep_idx) = val)

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
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));

    usb_dc_cfg.out_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.out_ep[0].ep_type = 0x00;
    usb_dc_cfg.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.in_ep[0].ep_type = 0x00;

    usb_dc_low_level_init();

    USB->rTOP = USB_TOP_RESET; //reset usb
    USB->rTOP &= ~USB_TOP_RESET;
    USB->rTOP &= ~USB_TOP_CONNECT; //usb disconnect

    USB->rINT_STATE |= 0;
    USB->rEP_INT_STATE |= 0;
    USB->rEP0_INT_STATE |= 0;
    USB->rEP1_INT_STATE |= 0;
    USB->rEP2_INT_STATE |= 0;
    USB->rEP3_INT_STATE |= 0;
    USB->rEP4_INT_STATE |= 0;

    USB->rEP0_CTRL = 0;
    USB->rEP1_CTRL = 0;
    USB->rEP2_CTRL = 0;
    USB->rEP3_CTRL = 0;
    USB->rEP4_CTRL = 0;

    USB->rINT_EN = USB_INT_EN_RSTIE | USB_INT_EN_SUSPENDIE | USB_INT_EN_RESUMIE | USB_INT_EN_EPINTIE;                                //enable rst、suppend、resume、ep global irq
    USB->rEP0_INT_EN = EPn_INT_EN_SETUPIE | EPn_INT_EN_OUTACKIE | EPn_INT_EN_INACKIE | EPn_INT_EN_OUTSTALLIE | EPn_INT_EN_INSTALLIE; //enable ep0  setup、inack、outack irq

    USB->rEP_INT_EN = EP_INT_EN_EP0IE; //enable ep0 irq
    USB->rEP_EN = EP_EN_EP0EN;         //enable ep0
    USB->rADDR = 0;                    //set addr 0

    USB->rTOP = USB_TOP_CONNECT | ((~USB_TOP_SPEED) & 0x01); //connect usb
    USB->rPOWER = USB_POWER_SUSPEN | USB_POWER_SUSP;
    return 0;
}

int usb_dc_deinit(void)
{
    usb_dc_low_level_deinit();
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (ep_idx == 0) {
        return 0;
    }

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USB_SET_EP_INT(ep_idx, EPn_INT_EN_OUTACKIE);
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USB_SET_EP_INT(ep_idx, EPn_INT_EN_INACKIE);
    }
    USB->rEP_INT_EN |= (1 << ep_idx);
    USB->rEP_EN |= (1 << ep_idx);
    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    return 0;
}

int usbd_ep_set_stall(const uint8_t ep)
{
    USB->rEP_HALT |= (1 << (ep & 0x7f));
    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
    USB->rEP_HALT &= ~(1 << (ep & 0x7f));
    return 0;
}
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t *pdata = (uint8_t *)data;
    if (!data && data_len) {
        return -1;
    }

    if (!data_len) {
        while (USB_GET_EP_CTRL(ep_idx) & 0x80) {
        }
        USB_SET_EP_CTRL(ep_idx, 0x80);
        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    while (USB_GET_EP_CTRL(ep_idx) & 0x80) {
    }

    for (uint8_t i = 0; i < data_len; i++) {
        USB_SET_EP_FIFO(ep_idx, *pdata);
        pdata++;
    }
    USB_SET_EP_CTRL(ep_idx, 0x80 | data_len);

    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t read_count;
    uint8_t *pdata = (uint8_t *)data;
    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
        return 0;
    }
    if ((ep_idx == 0) && (max_data_len == 8) && !read_bytes) {
        for (uint8_t i = 0; i < 8; i++) {
            *(pdata + i) = USB->rSETUP[i];
        }

    } else {
        read_count = USB_GET_EP_RX_CNT(ep_idx);
        read_count = MIN(read_count, max_data_len);

        for (uint8_t i = 0; i < read_count; i++) {
            *(pdata + i) = USB_GET_EP_FIFO(ep_idx);
        }
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

void USBD_IRQHandler(void)
{
    uint32_t int_status;
    uint32_t epindex;
    uint32_t ep_int_status;
    int_status = USB->rINT_STATE;
    USB->rINT_STATE = int_status;
    if (int_status & USB_INT_STATE_EPINTF) {
        epindex = USB->rEP_INT_STATE; //read all ep interrupt status
        USB->rEP_INT_STATE = epindex; //clear interrupt status

        for (uint32_t i = 0; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
            if (epindex & (EP_INT_STATE_EP0F << i)) //read epx interrupt status
            {
                if (i == 0) {
                    ep_int_status = USB->rEP0_INT_STATE;
                    USB->rEP0_INT_STATE = ep_int_status;

                    if (ep_int_status & EPn_INT_STATE_SETUP) //setup interrupt status
                    {
                        usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                    }
                    if (ep_int_status & EPn_INT_STATE_OUTACK) //outack interrupt status
                    {
                        usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                    }
                    if (ep_int_status & EPn_INT_STATE_INACK) //inack interrupt status
                    {
                        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                    }
                } else {
                    ep_int_status = USB_GET_EPX_INT_STATE(i);
                    USB_SET_EPX_INT_STATE(i, ep_int_status);
                    if (ep_int_status & EPn_INT_STATE_OUTACK) {
                        usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(i & 0x7f));
                    }
                    if (ep_int_status & EPn_INT_STATE_INACK) {
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(i | 0x80));
                    }
                }
                if (ep_int_status & EPn_INT_STATE_INSTALL) {
                }
                if (ep_int_status & EPn_INT_STATE_OUTSTALL) {
                }
            }
        }
    } else if (int_status & USB_INT_STATE_RSTF) {
        USB->rTOP |= USB_TOP_RESET;
        USB->rTOP &= ~USB_TOP_RESET;
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
    } else if (int_status & USB_INT_STATE_SUSPENDF) {
    } else if (int_status & USB_INT_STATE_RESUMF) {
    }
}
