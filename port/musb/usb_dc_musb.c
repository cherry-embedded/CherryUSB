#include "usbd_core.h"
#include "usb_musb_reg.h"

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#if defined(CONFIG_USB_MUSB_SUNXI)

#ifndef USB_BASE
#define USB_BASE (0x01c13000)
#endif

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USBD_IRQHandler //use actual usb irq name instead
#endif

#define MUSB_FADDR_OFFSET 0x98
#define MUSB_POWER_OFFSET 0x40
#define MUSB_TXIS_OFFSET  0x44
#define MUSB_RXIS_OFFSET  0x46
#define MUSB_TXIE_OFFSET  0x48
#define MUSB_RXIE_OFFSET  0x4A
#define MUSB_IS_OFFSET    0x4C
#define MUSB_IE_OFFSET    0x50
#define MUSB_EPIDX_OFFSET 0x42

#define MUSB_IND_TXMAP_OFFSET   0x80
#define MUSB_IND_TXCSRL_OFFSET  0x82
#define MUSB_IND_TXCSRH_OFFSET  0x83
#define MUSB_IND_RXMAP_OFFSET   0x84
#define MUSB_IND_RXCSRL_OFFSET  0x86
#define MUSB_IND_RXCSRH_OFFSET  0x87
#define MUSB_IND_RXCOUNT_OFFSET 0x88

#define MUSB_FIFO_OFFSET 0x00

#define MUSB_DEVCTL_OFFSET 0x41

#define MUSB_TXFIFOSZ_OFFSET  0x90
#define MUSB_RXFIFOSZ_OFFSET  0x94
#define MUSB_TXFIFOADD_OFFSET 0x92
#define MUSB_RXFIFOADD_OFFSET 0x96

#elif defined(CONFIG_USB_MUSB_CUSTOM)

#else

#ifndef USBD_IRQHandler
#define USBD_IRQHandler USB_INT_Handler //use actual usb irq name instead
#endif

#ifndef USB_BASE
#define USB_BASE (0x40086400UL)
#endif

#define MUSB_FADDR_OFFSET 0x00
#define MUSB_POWER_OFFSET 0x01
#define MUSB_TXIS_OFFSET  0x02
#define MUSB_RXIS_OFFSET  0x04
#define MUSB_TXIE_OFFSET  0x06
#define MUSB_RXIE_OFFSET  0x08
#define MUSB_IS_OFFSET    0x0A
#define MUSB_IE_OFFSET    0x0B

#define MUSB_EPIDX_OFFSET 0x0E

#define MUSB_IND_TXMAP_OFFSET   0x10
#define MUSB_IND_TXCSRL_OFFSET  0x12
#define MUSB_IND_TXCSRH_OFFSET  0x13
#define MUSB_IND_RXMAP_OFFSET   0x14
#define MUSB_IND_RXCSRL_OFFSET  0x16
#define MUSB_IND_RXCSRH_OFFSET  0x17
#define MUSB_IND_RXCOUNT_OFFSET 0x18

#define MUSB_FIFO_OFFSET 0x20

#define MUSB_DEVCTL_OFFSET 0x60

#define MUSB_TXFIFOSZ_OFFSET  0x62
#define MUSB_RXFIFOSZ_OFFSET  0x63
#define MUSB_TXFIFOADD_OFFSET 0x64
#define MUSB_RXFIFOADD_OFFSET 0x66

#endif // CONFIG_USB_MUSB_SUNXI

#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
#endif

typedef enum {
    USB_EP0_STATE_SETUP = 0x0,      /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA = 0x1,    /**< IN DATA */
    USB_EP0_STATE_IN_STATUS = 0x2,  /**< IN status*/
    USB_EP0_STATE_OUT_DATA = 0x3,   /**< OUT DATA */
    USB_EP0_STATE_OUT_STATUS = 0x4, /**< OUT status */
    USB_EP0_STATE_STALL = 0x5,      /**< STALL status */
} ep0_state_t;

/* Endpoint state */
struct musb_ep_state {
    /** Endpoint max packet size */
    uint16_t ep_mps;
    /** Endpoint Transfer Type.
     * May be Bulk, Interrupt, Control or Isochronous
     */
    uint8_t ep_type;
    uint8_t ep_stalled; /** Endpoint stall flag */
};

/* Driver state */
struct musb_udc {
    volatile uint8_t dev_addr;
    volatile uint32_t fifo_size_offset;
    struct usb_setup_packet setup;
    struct musb_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct musb_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} g_musb_udc;

static volatile uint8_t usb_ep0_state = USB_EP0_STATE_SETUP;
volatile uint16_t ep0_last_size = 0;

/* get current active ep */
static uint8_t musb_get_active_ep(void)
{
    return HWREGB(USB_BASE + MUSB_EPIDX_OFFSET);
}

/* set the active ep */
static void musb_set_active_ep(uint8_t ep_index)
{
    HWREGB(USB_BASE + MUSB_EPIDX_OFFSET) = ep_index;
}

static void musb_write_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((uint32_t)buffer & 0x03) {
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

static void musb_read_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
{
    uint32_t *buf32;
    uint8_t *buf8;
    uint32_t count32;
    uint32_t count8;
    int i;

    if ((uint32_t)buffer & 0x03) {
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

static uint32_t musb_get_fifo_size(uint16_t mps, uint16_t *used)
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
    memset(&g_musb_udc, 0, sizeof(struct musb_udc));

    g_musb_udc.out_ep[0].ep_mps = USB_CTRL_EP_MPS;
    g_musb_udc.out_ep[0].ep_type = 0x00;
    g_musb_udc.in_ep[0].ep_mps = USB_CTRL_EP_MPS;
    g_musb_udc.in_ep[0].ep_type = 0x00;
    g_musb_udc.fifo_size_offset = USB_CTRL_EP_MPS;

    usb_dc_low_level_init();

#ifdef CONFIG_USB_HS
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;
#else
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_HSENAB;
#endif

    musb_set_active_ep(0);
    HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) |= USB_DEVCTL_SESSION;

    /* Enable USB interrupts */
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = USB_IE_RESET;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_SOFTCONN;
    return 0;
}

int usb_dc_deinit(void)
{
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;
    }

    g_musb_udc.dev_addr = addr;
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

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        g_musb_udc.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_musb_udc.out_ep[ep_idx].ep_type = ep_cfg->ep_type;

        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);

        HWREGH(USB_BASE + MUSB_IND_RXMAP_OFFSET) = ep_cfg->ep_mps;

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

        HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = (USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
        else
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_CLRDT;

        fifo_size = musb_get_fifo_size(ep_cfg->ep_mps, &used);

        HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = fifo_size & 0x0f;
        HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = (g_musb_udc.fifo_size_offset >> 3);

        g_musb_udc.fifo_size_offset += used;
    } else {
        g_musb_udc.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_musb_udc.in_ep[ep_idx].ep_type = ep_cfg->ep_type;

        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);

        HWREGH(USB_BASE + MUSB_IND_TXMAP_OFFSET) = ep_cfg->ep_mps;

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

        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
        else
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_CLRDT;

        fifo_size = musb_get_fifo_size(ep_cfg->ep_mps, &used);

        HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = fifo_size & 0x0f;
        HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = (g_musb_udc.fifo_size_offset >> 3);

        g_musb_udc.fifo_size_offset += used;
    }

    musb_set_active_ep(old_ep_idx);

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

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_STALL;
        }
    } else {
        if (ep_idx == 0x00) {
            usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_STALL;
        }
    }

    musb_set_active_ep(old_ep_idx);
    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an OUT endpoint.
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_STALL | USB_RXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_CLRDT;
        }
    } else {
        if (ep_idx == 0x00) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        } else {
            // Clear the stall on an IN endpoint.
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~(USB_TXCSRL1_STALL | USB_TXCSRL1_STALLED);
            // Reset the data toggle.
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_CLRDT;
        }
    }

    musb_set_active_ep(old_ep_idx);
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

    if (!data && data_len) {
        return -1;
    }

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (ep_idx != 0x00) {
        while (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
            if ((HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_ERROR) || (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_UNDRN)) {
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
        if (ep_idx == 0x00) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
        }
        goto _RET;
    }

    ep0_last_size = data_len;

    if (data_len > g_musb_udc.in_ep[ep_idx].ep_mps) {
        data_len = g_musb_udc.in_ep[ep_idx].ep_mps;
    }

    musb_write_packet(ep_idx, (uint8_t *)data, data_len);

    if (ep_idx == 0x00) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
    } else {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
    }

    if (ret_bytes) {
        *ret_bytes = data_len;
    }

_RET:
    musb_set_active_ep(old_ep_idx);
    return ret;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    int ret = 0;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t read_count = 0;
    uint8_t old_ep_idx;

    if (!data && max_data_len) {
        return -1;
    }

    old_ep_idx = musb_get_active_ep();
    musb_set_active_ep(ep_idx);

    if (!max_data_len) {
        if (ep_idx != 0x00)
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_RXRDY);
        goto _RET;
    }

    if (ep_idx == 0x00) {
        if (usb_ep0_state == USB_EP0_STATE_SETUP) {
            memcpy(data, (uint8_t *)&g_musb_udc.setup, 8);
        } else {
            read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
            read_count = MIN(read_count, max_data_len);
            musb_read_packet(0, data, read_count);
        }
    } else {
        read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
        read_count = MIN(read_count, max_data_len);
        musb_read_packet(ep_idx, data, read_count);
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

_RET:
    musb_set_active_ep(old_ep_idx);
    return ret;
}

static void handle_ep0(void)
{
    uint8_t ep0_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);

    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        usb_ep0_state = USB_EP0_STATE_SETUP;
        return;
    }

    if (ep0_status & USB_CSRL0_SETEND) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_SETENDC;
    }

    if (g_musb_udc.dev_addr > 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = g_musb_udc.dev_addr;
        g_musb_udc.dev_addr = 0;
    }

    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            if (ep0_status & USB_CSRL0_RXRDY) {
                uint32_t read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                if (read_count != 8) {
                    return;
                }

                musb_read_packet(0, (uint8_t *)&g_musb_udc.setup, 8);
                if (g_musb_udc.setup.wLength) {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_RXRDYC;
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                }

                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);

                if (usb_ep0_state != USB_EP0_STATE_STALL) {
                    if (g_musb_udc.setup.wLength) {
                        if (g_musb_udc.setup.bmRequestType & 0x80) {
                            usb_ep0_state = USB_EP0_STATE_IN_DATA;
                            if (ep0_last_size > g_musb_udc.in_ep[0].ep_mps) {
                                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
                            } else {
                                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
                                usb_ep0_state = USB_EP0_STATE_OUT_STATUS;
                            }
                        } else {
                            usb_ep0_state = USB_EP0_STATE_OUT_DATA;
                        }
                    } else {
                        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
                        usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                    }
                }
            }
            break;

        case USB_EP0_STATE_IN_DATA:
            usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
            if (ep0_last_size > g_musb_udc.in_ep[0].ep_mps) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
            } else {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
                usb_ep0_state = USB_EP0_STATE_OUT_STATUS;
            }
            break;
        case USB_EP0_STATE_IN_STATUS:
            usb_ep0_state = USB_EP0_STATE_SETUP;
            break;
        case USB_EP0_STATE_OUT_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                if (g_musb_udc.setup.wLength > g_musb_udc.out_ep[0].ep_mps) {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_RXRDYC;
                } else {
                    usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                }
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
    uint8_t old_ep_idx;

    is = HWREGB(USB_BASE + MUSB_IS_OFFSET);
    txis = HWREGH(USB_BASE + MUSB_TXIS_OFFSET);
    rxis = HWREGH(USB_BASE + MUSB_RXIS_OFFSET);

    HWREGB(USB_BASE + MUSB_IS_OFFSET) = is;

    old_ep_idx = musb_get_active_ep();

    /* Receive a reset signal from the USB bus */
    if (is & USB_IS_RESET) {
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

        for (uint8_t i = 1; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
            musb_set_active_ep(i);
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = 0;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = 0;
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = 0;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = 0;
        }
        g_musb_udc.fifo_size_offset = USB_CTRL_EP_MPS;
    }

    if (is & USB_IS_SOF) {
    }

    if (is & USB_IS_RESUME) {
    }

    if (is & USB_IS_SUSPEND) {
    }

    txis &= HWREGH(USB_BASE + MUSB_TXIE_OFFSET);
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
        musb_set_active_ep(0);
        handle_ep0();
        txis &= ~USB_TXIE_EP0;
    }

    for (uint32_t ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
        if (txis & (1 << ep_idx)) {
            musb_set_active_ep(ep_idx);
            HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_UNDRN) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_UNDRN;
            }
            usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(0x80 | ep_idx));
        }
    }

    rxis &= HWREGH(USB_BASE + MUSB_RXIE_OFFSET);
    for (uint32_t ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
        if (rxis & (1 << ep_idx)) {
            musb_set_active_ep(ep_idx);
            HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
                usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(ep_idx & 0x7f));
            }
        }
    }

    musb_set_active_ep(old_ep_idx);
}
