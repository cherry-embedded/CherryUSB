#include "usbd_core.h"
#include "usb_musb_reg.h"

#ifdef USB_MUSB_SUNXI
#define SUNXI_SRAMC_BASE 0x01c00000
#define SUNXI_USB0_BASE  0x01c13000

#define USBC_REG_o_PHYCTL 0x0404

#ifndef USB_BASE
#define USB_BASE (SUNXI_USB0_BASE)
#endif

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USB_INT_Handler //use actual usb irq name instead

void USBD_IRQHandler(int, void *);
#endif

#define MUSB_IND_TXMAP_OFFSET   0x80
#define MUSB_IND_TXCSRL_OFFSET  0x82
#define MUSB_IND_TXCSRH_OFFSET  0x83
#define MUSB_IND_RXMAP_OFFSET   0x84
#define MUSB_IND_RXCSRL_OFFSET  0x86
#define MUSB_IND_RXCSRH_OFFSET  0x87
#define MUSB_IND_RXCOUNT_OFFSET 0x88
#define MUSB_FIFO_OFFSET        0x00

#else

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USB_INT_Handler //use actual usb irq name instead
#endif

#ifndef USB_BASE
#define USB_BASE (0x40086400UL)
#endif

#define MUSB_IND_TXMAP_OFFSET   0x10
#define MUSB_IND_TXCSRL_OFFSET  0x12
#define MUSB_IND_TXCSRH_OFFSET  0x13
#define MUSB_IND_RXMAP_OFFSET   0x14
#define MUSB_IND_RXCSRL_OFFSET  0x16
#define MUSB_IND_RXCSRH_OFFSET  0x17
#define MUSB_IND_RXCOUNT_OFFSET 0x18
#define MUSB_FIFO_OFFSET        0x20

#endif // USB_MUSB_SUNXI

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
#endif

#define USB_TXMAPx_BASE       (USB_BASE + MUSB_IND_TXMAP_OFFSET)
#define USB_RXMAPx_BASE       (USB_BASE + MUSB_IND_RXMAP_OFFSET)
#define USB_TXCSRLx_BASE      (USB_BASE + MUSB_IND_TXCSRL_OFFSET)
#define USB_RXCSRLx_BASE      (USB_BASE + MUSB_IND_RXCSRL_OFFSET)
#define USB_TXCSRHx_BASE      (USB_BASE + MUSB_IND_TXCSRH_OFFSET)
#define USB_RXCSRHx_BASE      (USB_BASE + MUSB_IND_RXCSRH_OFFSET)
#define USB_RXCOUNTx_BASE     (USB_BASE + MUSB_IND_RXCOUNT_OFFSET)
#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

#define USB ((volatile USB0_Type *)USB_BASE)

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

typedef enum {
    USB_EP0_STATE_SETUP = 0x0,      /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA = 0x1,    /**< IN DATA */
    USB_EP0_STATE_IN_STATUS = 0x2,  /**< IN status*/
    USB_EP0_STATE_OUT_DATA = 0x3,   /**< OUT DATA */
    USB_EP0_STATE_OUT_STATUS = 0x4, /**< OUT status */
} ep0_state_t;

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
    volatile uint32_t fifo_size_offset;
    struct usb_setup_packet setup;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

volatile uint8_t usb_ep0_state = USB_EP0_STATE_SETUP;
volatile uint16_t ep0_last_size = 0;

/* get current active ep */
static uint8_t USBC_GetActiveEp(void)
{
    return USB->EPIDX;
}

/* set the active ep */
static void USBC_SelectActiveEp(uint8_t ep_index)
{
    USB->EPIDX = ep_index;
}

static void usb_musb_write_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((size_t)buffer & 0x03) {
        buf8 = buffer;
        for (i = 0; i < len; i++) {
            HWREGB(USB_FIFO_BASE(ep_idx)) = *buf8++;
        }
    } else {
        count32 = len >> 2;
        count8 = len & 0x03;

        buf32 = (uint32_t *)buffer;

        while (count32--) {
            HWREG(USB_FIFO_BASE(ep_idx)) = *buf32++;
        }

        buf8 = (uint8_t *)buf32;

        while (count8--) {
            HWREGB(USB_FIFO_BASE(ep_idx)) = *buf8++;
        }
    }
}

static void usb_musb_read_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((size_t)buffer & 0x03) {
        buf8 = buffer;
        for (i = 0; i < len; i++) {
            *buf8++ = HWREGB(USB_FIFO_BASE(ep_idx));
        }
    } else {
        count32 = len >> 2;
        count8 = len & 0x03;

        buf32 = (uint32_t *)buffer;

        while (count32--) {
            *buf32++ = HWREG(USB_FIFO_BASE(ep_idx));
        }

        buf8 = (uint8_t *)buf32;

        while (count8--) {
            *buf8++ = HWREGB(USB_FIFO_BASE(ep_idx));
        }
    }
}

static uint32_t usb_musb_get_fifo_size(uint16_t mps, uint16_t *used)
{
    uint32_t size;

    for (uint8_t i = USB_TXFIFOSZ_SIZE_8; i <= USB_TXFIFOSZ_SIZE_2048; i++) {
        size = (8 << i);
        if (mps <= size) {
            *used = size;
            return i;
        }
    }

    *used = 0;
    return USB_TXFIFOSZ_SIZE_8;
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

    usb_dc_cfg.out_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.out_ep[0].ep_type = 0x00;
    usb_dc_cfg.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.in_ep[0].ep_type = 0x00;
    usb_dc_cfg.fifo_size_offset = USB_CTRL_EP_MPS;

    usb_dc_low_level_init();

#ifdef CONFIG_USB_HS
    USB->POWER |= USB_POWER_HSENAB;
#else
    USB->POWER &= ~USB_POWER_HSENAB;
#endif

    USB->EPIDX = 0;
    USB->FADDR = 0;

    USB->DEVCTL |= USB_DEVCTL_SESSION;

    /* Enable USB interrupts */
    USB->IE = USB_IE_RESET;
    USB->TXIE = USB_TXIE_EP0;
    USB->RXIE = 0;

    USB->POWER |= USB_POWER_SOFTCONN;
    return 0;
}

int usb_dc_deinit(void)
{
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        USB->FADDR = 0;
    }

    usb_dc_cfg.dev_addr = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint16_t used = 0;
    uint16_t fifo_size = 0;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);
    uint8_t old_ep_idx;
    uint32_t ui32Flags = 0;
    uint16_t ui32Register = 0;

    if (ep_idx == 0) {
        return 0;
    }

    old_ep_idx = USBC_GetActiveEp();
    USBC_SelectActiveEp(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USB->RXIE |= (1 << ep_idx);

        HWREGH(USB_RXMAPx_BASE) = ep_cfg->ep_mps;

        //
        // Allow auto clearing of RxPktRdy when packet of size max packet
        // has been unloaded from the FIFO.
        //
        if (ui32Flags & USB_EP_AUTO_CLEAR) {
            ui32Register = USB_RXCSRH1_AUTOCL;
        }
        //
        // Configure the DMA mode.
        //
        if (ui32Flags & USB_EP_DMA_MODE_1) {
            ui32Register |= USB_RXCSRH1_DMAEN | USB_RXCSRH1_DMAMOD;
        } else if (ui32Flags & USB_EP_DMA_MODE_0) {
            ui32Register |= USB_RXCSRH1_DMAEN;
        }
        //
        // If requested, disable NYET responses for high-speed bulk and
        // interrupt endpoints.
        //
        if (ui32Flags & USB_EP_DIS_NYET) {
            ui32Register |= USB_RXCSRH1_DISNYET;
        }

        //
        // Enable isochronous mode if requested.
        //
        if (ep_cfg->ep_type == 0x01) {
            ui32Register |= USB_RXCSRH1_ISO;
        }

        HWREGB(USB_RXCSRHx_BASE) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_RXCSRLx_BASE) & USB_RXCSRL1_RXRDY)
            HWREGB(USB_RXCSRLx_BASE) = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
        else
            HWREGB(USB_RXCSRLx_BASE) = USB_RXCSRL1_CLRDT;

        fifo_size = usb_musb_get_fifo_size(ep_cfg->ep_mps, &used);

        USB->RXFIFOSZ = fifo_size & 0x0f;
        USB->RXFIFOADD = (usb_dc_cfg.fifo_size_offset >> 3);

        usb_dc_cfg.fifo_size_offset += used;
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USB->TXIE |= (1 << ep_idx);

        HWREGH(USB_TXMAPx_BASE) = ep_cfg->ep_mps;

        //
        // Allow auto setting of TxPktRdy when max packet size has been loaded
        // into the FIFO.
        //
        if (ui32Flags & USB_EP_AUTO_SET) {
            ui32Register |= USB_TXCSRH1_AUTOSET;
        }

        //
        // Configure the DMA mode.
        //
        if (ui32Flags & USB_EP_DMA_MODE_1) {
            ui32Register |= USB_TXCSRH1_DMAEN | USB_TXCSRH1_DMAMOD;
        } else if (ui32Flags & USB_EP_DMA_MODE_0) {
            ui32Register |= USB_TXCSRH1_DMAEN;
        }

        //
        // Enable isochronous mode if requested.
        //
        if (ep_cfg->ep_type == 0x01) {
            ui32Register |= USB_TXCSRH1_ISO;
        }

        HWREGB(USB_TXCSRHx_BASE) = ui32Register;
        // Reset the Data toggle to zero.
        if (HWREGB(USB_TXCSRLx_BASE) & USB_TXCSRL1_TXRDY)
            HWREGB(USB_TXCSRLx_BASE) = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
        else
            HWREGB(USB_TXCSRLx_BASE) = USB_TXCSRL1_CLRDT;

        fifo_size = usb_musb_get_fifo_size(ep_cfg->ep_mps, &used);

        USB->TXFIFOSZ = fifo_size & 0x0f;
        USB->TXFIFOADD = (usb_dc_cfg.fifo_size_offset >> 3);

        usb_dc_cfg.fifo_size_offset += used;
    }

    USBC_SelectActiveEp(old_ep_idx);

    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    return 0;
}

int usbd_ep_set_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    old_ep_idx = USBC_GetActiveEp();
    USBC_SelectActiveEp(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            HWREGB(USB_TXCSRLx_BASE) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_RXCSRLx_BASE) |= USB_RXCSRL1_STALL;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_TXCSRLx_BASE) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_TXCSRLx_BASE) |= USB_TXCSRL1_STALL;
        }
    }

    USBC_SelectActiveEp(old_ep_idx);
    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    old_ep_idx = USBC_GetActiveEp();
    USBC_SelectActiveEp(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            HWREGB(USB_TXCSRLx_BASE) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an OUT endpoint.
            HWREGB(USB_RXCSRLx_BASE) &= ~(USB_RXCSRL1_STALL | USB_RXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_RXCSRLx_BASE) |= USB_RXCSRL1_CLRDT;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_TXCSRLx_BASE) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an IN endpoint.
            HWREGB(USB_TXCSRLx_BASE) &= ~(USB_TXCSRL1_STALL | USB_TXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_TXCSRLx_BASE) |= USB_TXCSRL1_CLRDT;
        }
    }

    USBC_SelectActiveEp(old_ep_idx);
    return 0;
}

int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    int ret = 0;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t timeout = 0xffffff;
    uint8_t old_ep_idx;

    old_ep_idx = USBC_GetActiveEp();
    USBC_SelectActiveEp(ep_idx);
    if (!data && data_len) {
        ret = -1;
        goto _RET;
    }

    if (ep_idx == 0x00) {
        while (HWREGB(USB_TXCSRLx_BASE) & USB_CSRL0_TXRDY) {
            if (HWREGB(USB_TXCSRLx_BASE) & USB_CSRL0_ERROR) {
                ret = -2;
                goto _RET;
            }
            if (!(timeout--)) {
                ret = -3;
                goto _RET;
            }
        }
    } else {
        while (HWREGB(USB_TXCSRLx_BASE) & USB_TXCSRL1_TXRDY) {
            if ((HWREGB(USB_TXCSRLx_BASE) & USB_TXCSRL1_ERROR) || (HWREGB(USB_TXCSRLx_BASE) & USB_TXCSRL1_UNDRN)) {
                ret = -2;
                goto _RET;
            }
            if (!(timeout--)) {
                ret = -3;
                goto _RET;
            }
        }
    }

    if (!data_len) {
        ret = 0;
        goto _RET;
    }

    ep0_last_size = data_len;

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    usb_musb_write_packet(ep_idx, (uint8_t *)data, data_len);

    if (ep_idx != 0) {
        HWREGB(USB_TXCSRLx_BASE) = USB_TXCSRL1_TXRDY;
    }
    if (ret_bytes) {
        *ret_bytes = data_len;
    }

_RET:
    USBC_SelectActiveEp(old_ep_idx);
    return ret;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    int ret = 0;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t read_count;
    uint8_t old_ep_idx;

    old_ep_idx = USBC_GetActiveEp();
    USBC_SelectActiveEp(ep_idx);
    if (!data && max_data_len) {
        ret = -1;
        goto _RET;
    }

    if (!max_data_len) {
        if (ep_idx != 0x00) {
            HWREGB(USB_RXCSRLx_BASE) &= ~(USB_RXCSRL1_RXRDY);
        }
        ret = 0;
        goto _RET;
    }

    if (ep_idx == 0x00) {
        if (usb_ep0_state == USB_EP0_STATE_SETUP) {
            memcpy(data, (uint8_t *)&usb_dc_cfg.setup, 8);
        } else {
            read_count = HWREGH(USB_RXCOUNTx_BASE);
            read_count = MIN(read_count, max_data_len);
            usb_musb_read_packet(0, data, read_count);
        }
    } else {
        read_count = HWREGH(USB_RXCOUNTx_BASE);
        read_count = MIN(read_count, max_data_len);
        usb_musb_read_packet(ep_idx, data, read_count);
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

_RET:
    USBC_SelectActiveEp(old_ep_idx);
    return ret;
}

static void handle_ep0(void)
{
    uint8_t ep0_status = USB->CSRL0;

    if (ep0_status & USB_CSRL0_STALLED) {
        USB->CSRL0 &= ~USB_CSRL0_STALLED;
        return;
    }

    if (ep0_status & USB_CSRL0_SETEND) {
        USB->CSRL0 = USB_CSRL0_SETENDC;
    }

    if (usb_dc_cfg.dev_addr > 0) {
        USB->FADDR = usb_dc_cfg.dev_addr;
        usb_dc_cfg.dev_addr = 0;
    }

    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            if (ep0_status & USB_CSRL0_RXRDY) {
                uint32_t read_count = HWREGH(USB_RXCOUNTx_BASE);

                if (read_count != 8) {
                    return;
                }

                usb_musb_read_packet(0, (uint8_t *)&usb_dc_cfg.setup, 8);
                if (usb_dc_cfg.setup.wLength) {
                    HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_RXRDYC;
                } else {
                    HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND;
                }

                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);

                if (usb_dc_cfg.setup.wLength) {
                    if (usb_dc_cfg.setup.bmRequestType & 0x80) {
                        usb_ep0_state = USB_EP0_STATE_IN_DATA;
                        if (ep0_last_size > usb_dc_cfg.in_ep[0].ep_mps) {
                            HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_TXRDY;
                        } else {
                            HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_TXRDY | USB_CSRL0_DATAEND;
                            usb_ep0_state = USB_EP0_STATE_OUT_STATUS;
                        }
                    } else {
                        usb_ep0_state = USB_EP0_STATE_OUT_DATA;
                    }
                } else {
                    HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_TXRDY | USB_CSRL0_DATAEND;
                    usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                }
            }
            break;

        case USB_EP0_STATE_IN_DATA:
            usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
            if (ep0_last_size > usb_dc_cfg.in_ep[0].ep_mps) {
                HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_TXRDY;
            } else {
                HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_TXRDY | USB_CSRL0_DATAEND;
                usb_ep0_state = USB_EP0_STATE_OUT_STATUS;
            }
            break;
        case USB_EP0_STATE_IN_STATUS:
            usb_ep0_state = USB_EP0_STATE_SETUP;
            break;
        case USB_EP0_STATE_OUT_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                if (usb_dc_cfg.setup.wLength > usb_dc_cfg.out_ep[0].ep_mps) {
                    HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_RXRDYC;
                } else {
                    usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                    HWREGB(USB_TXCSRLx_BASE) = USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND;
                }
            }
            break;
        case USB_EP0_STATE_OUT_STATUS:
            usb_ep0_state = USB_EP0_STATE_SETUP;
            break;
    }
}

#ifdef USB_MUSB_SUNXI
void USBD_IRQHandler(int irq, void *args)
#else
void USBD_IRQHandler(void)
#endif
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint8_t old_ep_idx;

    is = USB->IS;
    txis = USB->TXIS;
    rxis = USB->RXIS;

    old_ep_idx = USBC_GetActiveEp();
    /* Receive a reset signal from the USB bus */
    if (is & USB_IS_RESET) {
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        USB->TXIE = USB_TXIE_EP0;
        USB->RXIE = 0;

        for (uint8_t i = 1; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
            USB->EPIDX = i;
            USB->TXFIFOSZ = 0;
            USB->TXFIFOADD = 0;
            USB->RXFIFOSZ = 0;
            USB->RXFIFOADD = 0;
        }
        usb_dc_cfg.fifo_size_offset = USB_CTRL_EP_MPS;
    }

    if (is & USB_IS_SOF) {
    }

    if (is & USB_IS_RESUME) {
    }

    if (is & USB_IS_SUSPEND) {
    }

    USB->IS = is; // clear isr flag
    txis &= USB->TXIE;
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        USBC_SelectActiveEp(0);
        handle_ep0();
        txis &= ~USB_TXIE_EP0;
        USB->TXIS = USB_TXIE_EP0; // clear isr flag
    }

    while (txis) {
        uint8_t ep_idx = __builtin_ctz(txis);
        USBC_SelectActiveEp(ep_idx);
        if (HWREGB(USB_TXCSRLx_BASE) & USB_TXCSRL1_UNDRN) {
            HWREGB(USB_TXCSRLx_BASE) &= ~USB_TXCSRL1_UNDRN;
        }
        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(0x80 | ep_idx));
        txis &= ~(1 << ep_idx);
        USB->TXIS = (1 << ep_idx); // clear isr flag
    }

    rxis &= USB->RXIE;
    while (rxis) {
        uint8_t ep_idx = __builtin_ctz(rxis);
        USBC_SelectActiveEp(ep_idx);
        if (HWREGB(USB_RXCSRLx_BASE) & USB_RXCSRL1_RXRDY)
            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(ep_idx & 0x7f));
        rxis &= ~(1 << ep_idx);
        USB->RXIS = (1 << ep_idx); // clear isr flag
    }

    USBC_SelectActiveEp(old_ep_idx);
}
