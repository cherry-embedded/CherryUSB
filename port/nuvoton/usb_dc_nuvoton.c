#include "usbd_core.h"
#include "usb_nuvoton_reg.h"

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USBD_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE (0x40019000UL)
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 13
#endif

#define USBD ((USBD_T *)USB_BASE)

#define USBD_ENABLE_USB()              ((uint32_t)(USBD->PHYCTL |= (USBD_PHYCTL_PHYEN_Msk | USBD_PHYCTL_DPPUEN_Msk)))                     /*!<Enable USB  \hideinitializer */
#define USBD_DISABLE_USB()             ((uint32_t)(USBD->PHYCTL &= ~USBD_PHYCTL_DPPUEN_Msk))                                              /*!<Disable USB  \hideinitializer */
#define USBD_ENABLE_PHY()              ((uint32_t)(USBD->PHYCTL |= USBD_PHYCTL_PHYEN_Msk))                                                /*!<Enable PHY  \hideinitializer */
#define USBD_DISABLE_PHY()             ((uint32_t)(USBD->PHYCTL &= ~USBD_PHYCTL_PHYEN_Msk))                                               /*!<Disable PHY  \hideinitializer */
#define USBD_SET_SE0()                 ((uint32_t)(USBD->PHYCTL &= ~USBD_PHYCTL_DPPUEN_Msk))                                              /*!<Enable SE0, Force USB PHY Transceiver to Drive SE0  \hideinitializer */
#define USBD_CLR_SE0()                 ((uint32_t)(USBD->PHYCTL |= USBD_PHYCTL_DPPUEN_Msk))                                               /*!<Disable SE0  \hideinitializer */
#define USBD_SET_ADDR(addr)            (USBD->FADDR = (addr))                                                                             /*!<Set USB address  \hideinitializer */
#define USBD_GET_ADDR()                ((uint32_t)(USBD->FADDR))                                                                          /*!<Get USB address  \hideinitializer */
#define USBD_ENABLE_USB_INT(intr)      (USBD->GINTEN = (intr))                                                                            /*!<Enable USB Interrupt  \hideinitializer */
#define USBD_ENABLE_BUS_INT(intr)      (USBD->BUSINTEN = (intr))                                                                          /*!<Enable BUS Interrupt  \hideinitializer */
#define USBD_GET_BUS_INT_FLAG()        (USBD->BUSINTSTS)                                                                                  /*!<Clear Bus interrupt flag  \hideinitializer */
#define USBD_CLR_BUS_INT_FLAG(flag)    (USBD->BUSINTSTS = flag)                                                                           /*!<Clear Bus interrupt flag  \hideinitializer */
#define USBD_ENABLE_CEP_INT(intr)      (USBD->CEPINTEN = (intr))                                                                          /*!<Enable CEP Interrupt  \hideinitializer */
#define USBD_CLR_CEP_INT_FLAG(flag)    (USBD->CEPINTSTS = flag)                                                                           /*!<Clear CEP interrupt flag  \hideinitializer */
#define USBD_SET_CEP_STATE(flag)       (USBD->CEPCTL = flag)                                                                              /*!<Set CEP state  \hideinitializer */
#define USBD_START_CEP_IN(size)        (USBD->CEPTXCNT = size)                                                                            /*!<Start CEP IN Transfer  \hideinitializer */
#define USBD_SET_MAX_PAYLOAD(ep, size) (USBD->EP[ep].EPMPS = (size))                                                                      /*!<Set EPx Maximum Packet Size  \hideinitializer */
#define USBD_ENABLE_EP_INT(ep, intr)   (USBD->EP[ep].EPINTEN = (intr))                                                                    /*!<Enable EPx Interrupt  \hideinitializer */
#define USBD_GET_EP_INT_FLAG(ep)       (USBD->EP[ep].EPINTSTS)                                                                            /*!<Get EPx interrupt flag  \hideinitializer */
#define USBD_CLR_EP_INT_FLAG(ep, flag) (USBD->EP[ep].EPINTSTS = (flag))                                                                   /*!<Clear EPx interrupt flag  \hideinitializer */
#define USBD_SET_DMA_LEN(len)          (USBD->DMACNT = len)                                                                               /*!<Set DMA transfer length  \hideinitializer */
#define USBD_SET_DMA_ADDR(addr)        (USBD->DMAADDR = addr)                                                                             /*!<Set DMA transfer address  \hideinitializer */
#define USBD_SET_DMA_READ(epnum)       (USBD->DMACTL = (USBD->DMACTL & ~USBD_DMACTL_EPNUM_Msk) | USBD_DMACTL_DMARD_Msk | epnum | 0x100)   /*!<Set DMA transfer type to read \hideinitializer */
#define USBD_SET_DMA_WRITE(epnum)      (USBD->DMACTL = (USBD->DMACTL & ~(USBD_DMACTL_EPNUM_Msk | USBD_DMACTL_DMARD_Msk | 0x100)) | epnum) /*!<Set DMA transfer type to write \hideinitializer */
#define USBD_ENABLE_DMA()              (USBD->DMACTL |= USBD_DMACTL_DMAEN_Msk)                                                            /*!<Enable DMA transfer  \hideinitializer */
#define USBD_IS_ATTACHED()             ((uint32_t)(USBD->PHYCTL & USBD_PHYCTL_VBUSDET_Msk))                                               /*!<Check cable connect state  \hideinitializer */

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
    volatile uint8_t dev_addr;
    volatile uint32_t bufaddr;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

static void usb_fifo_write(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((uint32_t)buffer & 0x03) {
        buf8 = buffer;
        if (ep_idx == 0) {
            for (i = 0; i < len; i++) {
                USBD->cep.CEPDAT_BYTE = *buf8;
                buf8++;
            }

        } else {
            for (i = 0; i < len; i++) {
                USBD->EP[ep_idx - 1].ep.EPDAT_BYTE = *buf8;
                buf8++;
            }
        }
    } else {
        count32 = len >> 2;
        count8 = len & 0x03;

        buf32 = (uint32_t *)buffer;

        if (ep_idx == 0) {
            while (count32--) {
                USBD->cep.CEPDAT = *buf32;
                buf32++;
            }

            buf8 = (uint8_t *)buf32;

            while (count8--) {
                USBD->cep.CEPDAT_BYTE = *buf8;
                buf8++;
            }
        } else {
            while (count32--) {
                USBD->EP[ep_idx - 1].ep.EPDAT = *buf32;
                buf32++;
            }
            buf8 = (uint8_t *)buf32;
            while (count8--) {
                USBD->EP[ep_idx - 1].ep.EPDAT_BYTE = *buf8;
                buf8++;
            }
        }
    }
}

static void usb_fifo_read(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((uint32_t)buffer & 0x03) {
        buf8 = buffer;
        if (ep_idx == 0) {
            for (i = 0; i < len; i++) {
                *buf8 = USBD->cep.CEPDAT_BYTE;
                buf8++;
            }
        } else {
            for (i = 0; i < len; i++) {
                *buf8 = USBD->EP[ep_idx - 1].ep.EPDAT_BYTE;
                buf8++;
            }
        }
    } else {
        count32 = len >> 2;
        count8 = len & 0x03;

        buf32 = (uint32_t *)buffer;

        if (ep_idx == 0) {
            while (count32--) {
                *buf32 = USBD->cep.CEPDAT;
                buf32++;
            }

            buf8 = (uint8_t *)buf32;

            while (count8--) {
                *buf8 = USBD->cep.CEPDAT_BYTE;
                buf8++;
            }

        } else {
            while (count32--) {
                *buf32 = USBD->EP[ep_idx - 1].ep.EPDAT;
                buf32++;
            }
            buf8 = (uint8_t *)buf32;
            while (count8--) {
                *buf8 = USBD->EP[ep_idx - 1].ep.EPDAT_BYTE;
                buf8++;
            }
        }
    }
}

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

    /* Enable PHY */
    USBD_ENABLE_PHY();
    /* wait PHY clock ready */
    while (1) {
        USBD->EP[0].EPMPS = 0x20;
        if (USBD->EP[0].EPMPS == 0x20)
            break;
    }
#ifdef CONFIG_USB_HS
    USBD->OPER |= USBD_OPER_HISPDEN_Msk; /* high-speed */
#else
    USBD->OPER &= ~USBD_OPER_HISPDEN_Msk; /* full-speed */
#endif
    /* Reset Address to 0 */
    USBD_SET_ADDR(0);

    USBD->CEPINTEN = USBD_CEPINTEN_SETUPPKIEN_Msk | USBD_CEPINTEN_TXPKIEN_Msk | USBD_CEPINTEN_RXPKIEN_Msk | USBD_CEPINTEN_STSDONEIEN_Msk;
    /* Enable BUS interrupt */
    USBD->BUSINTEN = USBD_BUSINTEN_RSTIEN_Msk | USBD_BUSINTEN_VBUSDETIEN_Msk;
    /* Enable USB BUS, CEP and EPA global interrupt */
    USBD->GINTEN = USBD_GINTEN_USBIE_Msk | USBD_GINTEN_CEPIE_Msk;
    return 0;
}

int usb_dc_deinit(void)
{
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0x00) {
    }
    usb_dc_cfg.dev_addr = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);
    uint8_t ep_type;
    uint8_t ep_dir;
    uint32_t intr;

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        ep_dir = USB_EP_CFG_DIR_OUT;
        intr = USBD_EPINTEN_RXPKIEN_Msk;
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
    } else {
        ep_dir = USB_EP_CFG_DIR_IN;
        intr = USBD_EPINTEN_TXPKIEN_Msk;
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
    }

    if (ep_idx == 0) {
        /* Control endpoint */
        USBD->CEPBUFSTART = usb_dc_cfg.bufaddr;
        USBD->CEPBUFEND = usb_dc_cfg.bufaddr + 64 - 1;
        usb_dc_cfg.bufaddr += 64;
        return 0;
    }

    switch (ep_cfg->ep_type) {
        case 0x01:
            ep_type = USB_EP_CFG_TYPE_ISO;
            USBD->EP[ep_idx - 1].EPRSPCTL = (USB_EP_RSPCTL_FLUSH | USB_EP_RSPCTL_MODE_FLY);
            break;
        case 0x02:
            ep_type = USB_EP_CFG_TYPE_BULK;
            USBD->EP[ep_idx - 1].EPRSPCTL = (USB_EP_RSPCTL_FLUSH | USB_EP_RSPCTL_MODE_AUTO);
            break;
        case 0x03:
            ep_type = USB_EP_CFG_TYPE_INT;
            USBD->EP[ep_idx - 1].EPRSPCTL = (USB_EP_RSPCTL_FLUSH | USB_EP_RSPCTL_MODE_MANUAL);
            break;
    }
    USBD->EP[ep_idx - 1].EPBUFSTART = usb_dc_cfg.bufaddr;
    USBD->EP[ep_idx - 1].EPBUFEND = usb_dc_cfg.bufaddr + ep_cfg->ep_mps - 1;
    USBD->EP[ep_idx - 1].EPMPS = ep_cfg->ep_mps;
    USBD->EP[ep_idx - 1].EPCFG = (ep_type | ep_dir | USB_EP_CFG_VALID | (ep_idx << 4));
    USBD->EP[ep_idx - 1].EPINTEN = intr;
    USBD->GINTEN |= (1 << (USBD_GINTEN_CEPIE_Pos + ep_idx));
    usb_dc_cfg.bufaddr += ep_cfg->ep_mps;
    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    return 0;
}

int usbd_ep_set_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (ep_idx == 0x00) {
        USBD_SET_CEP_STATE(USB_CEPCTL_STALL);
    } else {
        USBD->EP[ep_idx - 1].EPRSPCTL = (USBD->EP[ep_idx - 1].EPRSPCTL & 0xf7) | USB_EP_RSPCTL_HALT;
    }
    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (ep_idx == 0x00) {
        return 0;
    }
    USBD->EP[ep_idx - 1].EPRSPCTL = USB_EP_RSPCTL_TOGGLE;

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
        if (ep_idx == 0x00) {
            USBD_SET_CEP_STATE(USB_CEPCTL_NAKCLR);
        } else {
            USBD->EP[ep_idx - 1].EPRSPCTL = USB_EP_RSPCTL_ZEROLEN;
        }
        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    if (ep_idx == 0x00) {
        usb_fifo_write(0, (uint8_t *)data, data_len);
        USBD->CEPTXCNT = data_len;
        USBD->CEPCTL = USB_CEPCTL_NAKCLR;
    } else {
        while (USBD->EP[ep_idx - 1].EPDATCNT != 0) {
        }
        usb_fifo_write(ep_idx, (uint8_t *)data, data_len);
        USBD->EP[ep_idx - 1].EPTXCNT = data_len;
        USBD->EP[ep_idx - 1].EPRSPCTL = USB_EP_RSPCTL_SHORTTXEN;
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

    if (ep_idx == 0x00) {
        if (max_data_len == 0x08 && !read_bytes) {
            *((uint16_t *)(data + 0)) = (uint16_t)(USBD->SETUP1_0 & 0xFFFFUL);
            *((uint16_t *)(data + 2)) = (uint16_t)(USBD->SETUP3_2 & 0xFFFFUL);
            *((uint16_t *)(data + 4)) = (uint16_t)(USBD->SETUP5_4 & 0xFFFFUL);
            *((uint16_t *)(data + 6)) = (uint16_t)(USBD->SETUP7_6 & 0xFFFFUL);
        } else {
            read_count = USBD->CEPRXCNT & 0xFFFFUL;
            read_count = MIN(read_count, max_data_len);

            usb_fifo_read(0, data, read_count);
        }
    } else {
        read_count = USBD->EP[ep_idx - 1].EPDATCNT & 0xFFFFUL;
        read_count = MIN(read_count, max_data_len);

        usb_fifo_read(ep_idx, data, read_count);
    }
    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

void USBD_IRQHandler(void)
{
    volatile uint32_t IrqStL, IrqSt;
    IrqStL = USBD->GINTSTS & USBD->GINTEN; /* get interrupt status */
    if (!IrqStL)
        return;

    /* USB interrupt */
    if (IrqStL & USBD_GINTSTS_USBIF_Msk) {
        IrqSt = USBD->BUSINTSTS & USBD->BUSINTEN;
        if (IrqSt & USBD_BUSINTSTS_SOFIF_Msk) {
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_SOFIF_Msk);
        }
        if (IrqSt & USBD_BUSINTSTS_RSTIF_Msk) {
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_RSTIF_Msk);

            USBD->DMACNT = 0;
            USBD->DMACTL = 0x80;
            USBD->DMACTL = 0x00;
            for (uint8_t i = 1; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
                USBD->EP[i - 1].EPRSPCTL = USBD_EPRSPCTL_FLUSH_Msk;
            }
            USBD_SET_ADDR(0);
            USBD_CLR_CEP_INT_FLAG(0x1ffc);
            usb_dc_cfg.bufaddr = 0;
            usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        }

        if (IrqSt & USBD_BUSINTSTS_RESUMEIF_Msk) {
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_RESUMEIF_Msk);
        }

        if (IrqSt & USBD_BUSINTSTS_SUSPENDIF_Msk) {
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_SUSPENDIF_Msk);
        }

        if (IrqSt & USBD_BUSINTSTS_HISPDIF_Msk) {
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_HISPDIF_Msk);
        }
        if (IrqSt & USBD_BUSINTSTS_DMADONEIF_Msk) {
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_DMADONEIF_Msk);
            if (USBD->DMACTL & USBD_DMACTL_DMARD_Msk) {
            }
        }

        if (IrqSt & USBD_BUSINTSTS_PHYCLKVLDIF_Msk)
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_PHYCLKVLDIF_Msk);

        if (IrqSt & USBD_BUSINTSTS_VBUSDETIF_Msk) {
            if (USBD_IS_ATTACHED()) {
                /* USB Plug In */
                USBD_ENABLE_USB();
            } else {
                /* USB Un-plug */
                USBD_DISABLE_USB();
            }
            USBD_CLR_BUS_INT_FLAG(USBD_BUSINTSTS_VBUSDETIF_Msk);
        }
    }
    if (IrqStL & USBD_GINTSTS_CEPIF_Msk) {
        IrqSt = USBD->CEPINTSTS & USBD->CEPINTEN;

        if (IrqSt & USBD_CEPINTSTS_SETUPTKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_SETUPTKIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_OUTTKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_OUTTKIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_INTKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_INTKIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_PINGIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_PINGIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_SETUPPKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_SETUPPKIF_Msk);
            usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_TXPKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_TXPKIF_Msk);
            usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_RXPKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_RXPKIF_Msk);
            usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_NAKIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_NAKIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_STALLIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_STALLIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_ERRIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_ERRIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_STSDONEIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_STSDONEIF_Msk);
            if (usb_dc_cfg.dev_addr > 0) {
                USBD_SET_ADDR(usb_dc_cfg.dev_addr);
                usb_dc_cfg.dev_addr = 0;
            }
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_BUFFULLIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_BUFFULLIF_Msk);
            return;
        }

        if (IrqSt & USBD_CEPINTSTS_BUFEMPTYIF_Msk) {
            USBD_CLR_CEP_INT_FLAG(USBD_CEPINTSTS_BUFEMPTYIF_Msk);
            return;
        }
    }

    for (uint8_t ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
        if (IrqStL & (0x1UL << (USBD_GINTSTS_CEPIF_Pos + ep_idx))) {
            IrqSt = USBD->EP[ep_idx - 1].EPINTSTS & USBD->EP[ep_idx - 1].EPINTEN;
            USBD_CLR_EP_INT_FLAG(ep_idx - 1, IrqSt);
            if (usb_dc_cfg.in_ep[ep_idx].ep_mps) {
                usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(0x80 | ep_idx));
            } else if (usb_dc_cfg.out_ep[ep_idx].ep_mps) {
                usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(ep_idx & 0x7f));
            }
        }
    }
}
