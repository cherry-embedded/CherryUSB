#include "usbd_core.h"
#include "usb_musb_reg.h"

#ifndef USB_BASE
#define USB_BASE (0x40080000UL + 0x6400)
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
#endif

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USB_INT_Handler //use actual usb irq name instead
#endif

#define USB ((USB0_Type *)USB_BASE)

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#define USB_CSRL0_BASE            (&USB->CSRL0)
#define USB_TXCSRLx_BASE(ep_idx)  (&USB->TXCSRL1 + 0x10 * (ep_idx - 1))
#define USB_RXCSRLx_BASE(ep_idx)  (&USB->RXCSRL1 + 0x10 * (ep_idx - 1))
#define USB_TXCSRHx_BASE(ep_idx)  (&USB->TXCSRH1 + 0x10 * (ep_idx - 1))
#define USB_RXCSRHx_BASE(ep_idx)  (&USB->RXCSRH1 + 0x10 * (ep_idx - 1))
#define USB_TXMAPx_BASE(ep_idx)   ((uint8_t *)&USB->TXMAXP1 + 0x10 * (ep_idx - 1))
#define USB_RXMAPx_BASE(ep_idx)   ((uint8_t *)&USB->RXMAXP1 + 0x10 * (ep_idx - 1))
#define USB_RXCOUNTx_BASE(ep_idx) ((uint8_t *)&USB->RXCOUNT1 + 0x10 * (ep_idx - 1))
#define USB_FIFO_BASE(ep_idx)     (&USB->FIFO0_BYTE + 0x4 * ep_idx)

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

static void usb_musb_data_ack(uint8_t ep_idx, bool bIsLastPacket)
{
    if (ep_idx == 0) {
        // Clear RxPktRdy, and optionally DataEnd, on endpoint zero.
        HWREGB(USB_CSRL0_BASE) = USB_CSRL0_RXRDYC | (bIsLastPacket ? USB_CSRL0_DATAEND : 0);
    } else {
        // Clear RxPktRdy on all other endpoints.
        HWREGB(USB_RXCSRLx_BASE(ep_idx)) &= ~(USB_RXCSRL1_RXRDY);
    }
}

static void usb_musb_data_send(uint8_t ep_idx, bool bIsLastPacket)
{
    if (ep_idx == 0) {
        HWREGB(USB_CSRL0_BASE) = USB_CSRL0_TXRDY | (bIsLastPacket ? USB_CSRL0_DATAEND : 0);
    } else {
        HWREGB(USB_TXCSRLx_BASE(ep_idx)) = USB_TXCSRL1_TXRDY;
    }
}

static int usb_musb_write_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;

    buf32 = (uint32_t *)buffer;

    count32 = len >> 2;
    count8 = len & 0x03;

    while (count32--) {
        HWREG(USB_FIFO_BASE(ep_idx)) = *buf32++;
    }

    buf8 = (uint8_t *)buf32;

    while (count8--) {
        HWREGB(USB_FIFO_BASE(ep_idx)) = *buf8++;
    }

    return 0;
}

static void usb_musb_read_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;

    buf32 = (uint32_t *)buffer;

    count32 = len >> 2;
    count8 = len & 0x03;

    while (count32--) {
        *buf32++ = HWREG(USB_FIFO_BASE(ep_idx));
    }

    buf8 = (uint8_t *)buf32;

    while (count8--) {
        *buf8++ = HWREGB(USB_FIFO_BASE(ep_idx));
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
    usb_dc_cfg.out_ep[0].ep_type = USBD_EP_TYPE_CTRL;
    usb_dc_cfg.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
    usb_dc_cfg.in_ep[0].ep_type = USBD_EP_TYPE_CTRL;
    usb_dc_cfg.fifo_size_offset = USB_CTRL_EP_MPS;

    usb_dc_low_level_init();

#ifdef CONFIG_USB_HS
    USB->POWER |= USB_POWER_HSENAB;
#else
    USB->POWER &= ~USB_POWER_HSENAB;
#endif

    USB->DEVCTL |= USB_DEVCTL_SESSION;

    /* Enable USB interrupts */
    USB->IE = USB_IE_RESET | USB_IE_SUSPND;
    USB->TXIE = USB_TXIE_EP0;
    USB->RXIE = 0;

    USB->POWER |= USB_POWER_SOFTCONN;
    return 0;
}

void usb_dc_deinit(void)
{
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
    uint32_t ui32Flags = 0;
    uint16_t ui32Register = 0;

    if (ep_idx == 0) {
        return 0;
    }
    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USB->RXIE |= (1 << ep_idx);

        HWREGH(USB_RXMAPx_BASE(ep_idx)) = ep_cfg->ep_mps;

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

        HWREGB(USB_RXCSRHx_BASE(ep_idx)) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_RXCSRLx_BASE(ep_idx)) & USB_RXCSRL1_RXRDY)
            HWREGB(USB_RXCSRLx_BASE(ep_idx)) = USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH;
        else
            HWREGB(USB_RXCSRLx_BASE(ep_idx)) = USB_RXCSRL1_CLRDT;

        fifo_size = usb_musb_get_fifo_size(ep_cfg->ep_mps, &used);

        uint32_t index = USB->EPIDX;

        USB->EPIDX = ep_idx;
        USB->RXFIFOSZ = fifo_size & 0x0f;
        USB->RXFIFOADD = (usb_dc_cfg.fifo_size_offset >> 3);

        USB->EPIDX = index;

        usb_dc_cfg.fifo_size_offset += used;
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USB->TXIE |= (1 << ep_idx);

        HWREGH(USB_TXMAPx_BASE(ep_idx)) = ep_cfg->ep_mps;

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

        HWREGB(USB_TXCSRHx_BASE(ep_idx)) = ui32Register;
        // Reset the Data toggle to zero.
        if (HWREGB(USB_TXCSRLx_BASE(ep_idx)) & USB_TXCSRL1_TXRDY)
            HWREGB(USB_TXCSRLx_BASE(ep_idx)) = USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH;
        else
            HWREGB(USB_TXCSRLx_BASE(ep_idx)) = USB_TXCSRL1_CLRDT;

        fifo_size = usb_musb_get_fifo_size(ep_cfg->ep_mps, &used);

        uint32_t index = USB->EPIDX;

        USB->EPIDX = ep_idx;
        USB->TXFIFOSZ = fifo_size & 0x0f;
        USB->TXFIFOADD = (usb_dc_cfg.fifo_size_offset >> 3);

        USB->EPIDX = index;

        usb_dc_cfg.fifo_size_offset += used;
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
        if (ep_idx == 0x00) {
            HWREGB(USB_CSRL0_BASE) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_RXCSRLx_BASE(ep_idx)) |= USB_RXCSRL1_STALL;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_CSRL0_BASE) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_TXCSRLx_BASE(ep_idx)) |= USB_TXCSRL1_STALL;
        }
    }

    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            HWREGB(USB_CSRL0_BASE) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an OUT endpoint.
            HWREGB(USB_RXCSRLx_BASE(ep_idx)) &= ~(USB_RXCSRL1_STALL | USB_RXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_RXCSRLx_BASE(ep_idx)) |= USB_RXCSRL1_CLRDT;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_CSRL0_BASE) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an IN endpoint.
            HWREGB(USB_TXCSRLx_BASE(ep_idx)) &= ~(USB_TXCSRL1_STALL | USB_TXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_TXCSRLx_BASE(ep_idx)) |= USB_TXCSRL1_CLRDT;
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
    int ret = 0;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t timeout = 0xffffff;

    if (!data && data_len) {
        return -1;
    }

    if (ep_idx == 0x00) {
        while (HWREGB(USB_CSRL0_BASE) & USB_CSRL0_TXRDY) {
            if (HWREGB(USB_CSRL0_BASE) & USB_CSRL0_ERROR) {
                return -2;
            }
            if (!(timeout--)) {
                return -3;
            }
        }
    } else {
        while (HWREGB(USB_TXCSRLx_BASE(ep_idx)) & USB_TXCSRL1_TXRDY) {
            if ((HWREGB(USB_TXCSRLx_BASE(ep_idx)) & USB_TXCSRL1_ERROR) || (HWREGB(USB_TXCSRLx_BASE(ep_idx)) & USB_TXCSRL1_UNDRN)) {
                return -2;
            }
            if (!(timeout--)) {
                return -3;
            }
        }
    }

    if (!data_len) {
        if (ep_idx == 0x00) {
            usb_ep0_state = USB_EP0_STATE_IN_STATUS;
            usb_musb_data_ack(ep_idx, true);
        } else {
            usb_musb_data_send(ep_idx, true);
        }
        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    ret = usb_musb_write_packet(ep_idx, (uint8_t *)data, data_len);

    if (data_len == usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        if (ep_idx == 0) {
            usb_ep0_state = USB_EP0_STATE_IN_DATA;
        }
        usb_musb_data_send(ep_idx, false);
    } else {
        if (ep_idx == 0) {
            usb_ep0_state = USB_EP0_STATE_OUT_STATUS;
        }
        usb_musb_data_send(ep_idx, true);
    }
    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return ret;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t read_count;
    uint8_t *buf8 = data;

    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
        if (ep_idx != 0x00) {
            usb_musb_data_ack(ep_idx, false);
        }
        return 0;
    }

    if (ep_idx == 0x00) {
        read_count = USB->COUNT0;
        usb_musb_read_packet(0, data, read_count);
        if (usb_ep0_state == USB_EP0_STATE_SETUP) {
            memcpy((uint8_t *)&usb_dc_cfg.setup, data, 8);
            usb_musb_data_ack(0, false);
        }
    } else {
        read_count = HWREGH(USB_RXCOUNTx_BASE(ep_idx));
        read_count = MIN(read_count, max_data_len);
        usb_musb_read_packet(ep_idx, data, read_count);
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

static void handle_ep0(void)
{
    uint8_t ep0_status = HWREGB(USB_CSRL0_BASE);
    uint8_t req = usb_dc_cfg.setup.bmRequestType;

    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_CSRL0_BASE) &= ~USB_CSRL0_STALLED;
        return;
    }

    if (ep0_status & USB_CSRL0_SETEND) {
        HWREGB(USB_CSRL0_BASE) = USB_CSRL0_SETENDC;
    }

    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP: {
            if (ep0_status & USB_CSRL0_RXRDY) {
                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                if (usb_dc_cfg.setup.wLength && !(usb_dc_cfg.setup.bmRequestType & 0x80)) {
                    usb_ep0_state = USB_EP0_STATE_OUT_DATA;
                }
            }
        } break;

        case USB_EP0_STATE_IN_DATA:
            usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
            break;
        case USB_EP0_STATE_IN_STATUS:
            if (usb_dc_cfg.dev_addr > 0) {
                USB->FADDR = usb_dc_cfg.dev_addr;
                usb_dc_cfg.dev_addr = 0;
            }
            usb_ep0_state = USB_EP0_STATE_SETUP;
            break;
        case USB_EP0_STATE_OUT_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                usb_musb_data_ack(0, true);
            }
            break;
        case USB_EP0_STATE_OUT_STATUS:
            usb_ep0_state = USB_EP0_STATE_SETUP;
            break;
    }
}

void USBD_IRQHandler(void)
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;

    is = USB->IS;
    txis = USB->TXIS;
    rxis = USB->RXIS;

    /* Receive a reset signal from the USB bus */
    if (is & USB_IS_RESET) {
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        USB->TXIE = 1;
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

    if (is & USB_IS_DISCON) {
    }
    if (is & USB_IS_SOF) {
    }

    if (is & USB_IS_RESUME) {
    }

    if (is & USB_IS_SUSPEND) {
    }

    txis &= USB->TXIE;
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        handle_ep0();
        txis &= ~USB_TXIE_EP0;
    }

    while (txis) {
        uint8_t ep_idx = __builtin_ctz(txis);
        if (HWREGB(USB_TXCSRLx_BASE(ep_idx)) == 0)
            usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(0x80 | ep_idx));
        txis &= ~(1 << ep_idx);
    }

    rxis &= USB->RXIE;
    while (rxis) {
        uint8_t ep_idx = __builtin_ctz(rxis);
        if (HWREGB(USB_RXCSRLx_BASE(ep_idx)) & USB_RXCSRL1_RXRDY)
            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(ep_idx & 0x7f));
        rxis &= ~(1 << ep_idx);
    }
}
