#include "usbd_core.h"
#include "MM32F103.h"

#define USB_GET_EP_INT_STATE(ep_idx)      (*(volatile uint32_t *)(USB_BASE + 0x20 + 4 * ep_idx))
#define USB_CLR_EP_INT_STATE(ep_idx, clr) (*(volatile uint32_t *)(USB_BASE + 0x20 + 4 * ep_idx) = clr)
#define USB_SET_EP_INT(ep_idx, int)       (*(volatile uint32_t *)(USB_BASE + 0x40 + 4 * ep_idx) = int)
#define USB_GET_EP_RX_CNT(ep_idx)         (*(volatile uint32_t *)(USB_BASE + 0x100 + 4 * ep_idx))
#define USB_GET_EP_CTRL(ep_idx)           (*(volatile uint32_t *)(USB_BASE + 0x140 + 4 * ep_idx))
#define USB_SET_EP_CTRL(ep_idx, ctrl)     (*(volatile uint32_t *)(USB_BASE + 0x140 + 4 * ep_idx) = ctrl)
#define USB_GET_EP_FIFO(ep_idx)           (*(volatile uint32_t *)(USB_BASE + 0x160 + 4 * ep_idx))
#define USB_SET_EP_FIFO(ep_idx, data)     (*(volatile uint32_t *)(USB_BASE + 0x160 + 4 * ep_idx) = data)

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
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

extern void SetUSBSysClockTo48M(void);

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));
    //SetUSBSysClockTo48M();//set usb clock 48M
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;    //enable usb clock
    RCC->APB2ENR |= RCC_APB2RSTR_IOPARST; //enable gpio a clock
    GPIOA->CRH &= 0XFFF00FFF;
    //MY_NVIC_Init(1, 1, USB_HP_CAN1_TX_IRQn, 2); //enable usb irq

    usb_dc_cfg.out_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.out_ep[0].ep_type = USBD_EP_TYPE_CTRL;
    usb_dc_cfg.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.in_ep[0].ep_type = USBD_EP_TYPE_CTRL;

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

    USB->rINT_EN = USB_INT_EN_RSTIE | USB_INT_EN_SUSPENDIE | USB_INT_EN_RESUMIE | USB_INT_EN_EPINTIE; //enable rst、suppend、resume、ep int
    USB->rEP0_INT_EN = EPn_INT_EN_SETUPIE | EPn_INT_EN_OUTACKIE | EPn_INT_EN_INACKIE;                 //enable ep0 setup、inack、outack int
    USB->rEP_INT_EN = EP_INT_EN_EP0IE;                                                                //enable ep0 int
    USB->rEP_EN = EP_EN_EP0EN;                                                                        //enable ep0
    USB->rADDR = 0;                                                                                   //set addr 0

    USB->rTOP = USB_TOP_CONNECT | ((~USB_TOP_SPEED) & 0x01); //connect usb
    USB->rPOWER = USB_POWER_SUSPEN | USB_POWER_SUSP;
    return 0;
}

void usb_dc_deinit(void)
{
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
    USB->rEP_INT_EN |= (EP_INT_EN_EP0IE << ep_idx);
    USB->rEP_EN |= (EP_EN_EP0EN << ep_idx);
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
    if (ep_idx == 0) {
        for (uint8_t i = 0; i < read_count; i++) {
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

/**
  * @brief  This function handles PCD interrupt request.
  * @param  hpcd PCD handle
  * @retval HAL status
  */
void USB_HP_CAN1_TX_IRQHandler(void)
{
    uint32_t int_status;
    uint32_t epindex;
    uint32_t ep_int_status;
    int_status = USB->rINT_STATE;          //读取USB中断状态
    USB->rINT_STATE = int_status;          //清USB中断状态,写1清零
    if (int_status & USB_INT_STATE_EPINTF) //端点中断
    {
        epindex = USB->rEP_INT_STATE; //读取端点中断号 0x1,0x2,0x4,0x8,0x10分别对应0,1,2,3,4端点中断
        USB->rEP_INT_STATE = epindex; //清端点号中断

        for (uint32_t i = 0; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
            if (epindex & (EP_INT_STATE_EP0F << i)) //端点x中断
            {
                if (i == 0) {
                    ep_int_status = USB_GET_EP_INT_STATE(i);
                    USB_CLR_EP_INT_STATE(i, ep_int_status);
                    if (ep_int_status & EPn_INT_STATE_SETUP) //SETUP中断
                    {
                        usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                    }
                    if (ep_int_status & EPn_INT_STATE_OUTACK) //端点0 OUT包应答中断，收到数据
                    {
                        usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                    }
                    if (ep_int_status & EPn_INT_STATE_INACK) //IN包应答中断，准备写入数据
                    {
                        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                    }
                } else {
                    ep_int_status = USB_GET_EP_INT_STATE(i);
                    USB_CLR_EP_INT_STATE(i, ep_int_status);
                    if (ep_int_status & EPn_INT_STATE_OUTACK) //端点x OUT包应答中断，收到数据
                    {
                        usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(i & 0x7f));
                    }
                    if (ep_int_status & EPn_INT_STATE_INACK) //IN包应答中断，准备写入数据
                    {
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(i | 0x80));
                    }
                }
            }
        }
    } else if (int_status & USB_INT_STATE_RSTF) //USB复位中断
    {
        USB->rTOP |= USB_TOP_RESET; //reset usb
        USB->rTOP &= ~USB_TOP_RESET;
    } else if (int_status & USB_INT_STATE_SUSPENDF) //USB挂起中断
    {
    } else if (int_status & USB_INT_STATE_RESUMF) //USB恢复中断
    {
    }
}
