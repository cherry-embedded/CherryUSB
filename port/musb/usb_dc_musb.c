/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usb_musb_reg.h"

#ifndef CONFIG_MUSB_PORTS
#define CONFIG_MUSB_PORTS 1
#endif

#ifndef CONFIG_MUSB_BIDIR_ENDPOINTS
#define CONFIG_MUSB_BIDIR_ENDPOINTS 4
#endif

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#if defined(CONFIG_USB_MUSB_SUNXI)

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

#define USB_BASE              (bus->reg_base)
#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

typedef enum {
    USB_EP0_STATE_SETUP = 0x0,      /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA = 0x1,    /**< IN DATA */
    USB_EP0_STATE_OUT_DATA = 0x3,   /**< OUT DATA */
    USB_EP0_STATE_IN_STATUS = 0x4,  /**< IN status */
    USB_EP0_STATE_OUT_STATUS = 0x5, /**< OUT status */
    USB_EP0_STATE_IN_ZLP = 0x6,     /**< OUT status */
    USB_EP0_STATE_STALL = 0x7,      /**< STALL status */
} ep0_state_t;

/* Endpoint state */
struct musb_ep {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct musb_udc {
    volatile uint8_t dev_addr;
    volatile uint32_t fifo_size_offset;
    volatile uint8_t usb_ep0_state;
    __attribute__((aligned(32))) struct usb_setup_packet setup;
    struct musb_ep in_ep[CONFIG_MUSB_BIDIR_ENDPOINTS];
    struct musb_ep out_ep[CONFIG_MUSB_BIDIR_ENDPOINTS];
    bool inuse;
} g_musb_udc[CONFIG_MUSB_PORTS];

/* get current active ep */
static uint8_t musb_get_active_ep(struct usbd_bus *bus)
{
    return HWREGB(USB_BASE + MUSB_EPIDX_OFFSET);
}

/* set the active ep */
static void musb_set_active_ep(struct usbd_bus *bus, uint8_t ep_index)
{
    HWREGB(USB_BASE + MUSB_EPIDX_OFFSET) = ep_index;
}

static void musb_write_packet(struct usbd_bus *bus, uint8_t ep_idx, uint8_t *buffer, uint16_t len)
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

static void musb_read_packet(struct usbd_bus *bus, uint8_t ep_idx, uint8_t *buffer, uint16_t len)
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

struct musb_udc *musb_alloc_udc(void)
{
    for (uint8_t i = 0; i < CONFIG_MUSB_PORTS; i++) {
        if (g_musb_udc[i].inuse == false) {
            g_musb_udc[i].inuse = true;
            return &g_musb_udc[i];
        }
    }
    return NULL;
}

void musb_free_udc(struct musb_udc *udc)
{
    udc->inuse = false;
}

int musb_udc_init(struct usbd_bus *bus)
{
    if (bus->busid != 0) {
        USB_LOG_ERR("musb busid must be 0\r\n");
        return -1;
    }

    struct musb_udc *udc;

    udc = musb_alloc_udc();
    if (udc == NULL) {
        return -1;
    }

    USB_LOG_INFO("========== musb udc params =========\r\n");
    USB_LOG_INFO("musb has %d endpoints, default config: %d endpoints\r\n", CONFIG_MUSB_BIDIR_ENDPOINTS, CONFIG_MUSB_BIDIR_ENDPOINTS);
    USB_LOG_INFO("=================================\r\n");

    bus->udc = udc;
    bus->endpoints = CONFIG_MUSB_BIDIR_ENDPOINTS;

    usbd_udc_low_level_init(0);

#ifdef CONFIG_USB_HS
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;
#else
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_HSENAB;
#endif

    musb_set_active_ep(bus, 0);
    HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) |= USB_DEVCTL_SESSION;

    /* Enable USB interrupts */
    HWREGB(USB_BASE + MUSB_IE_OFFSET) = USB_IE_RESET;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_SOFTCONN;
    return 0;
}

int musb_udc_deinit(struct usbd_bus *bus)
{
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) &= ~USB_DEVCTL_SESSION;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_SOFTCONN;

    musb_free_udc(udc);
    bus->udc = NULL;
    return 0;
}

int musb_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (addr == 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = 0;
    }

    udc->dev_addr = addr;
    return 0;
}

uint8_t musb_get_port_speed(struct usbd_bus *bus)
{
    uint8_t speed;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return 0;
    }

    if (HWREGB(USB_BASE + MUSB_POWER_OFFSET) & USB_POWER_HSMODE)
        speed = USB_SPEED_HIGH;
    else if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_FSDEV)
        speed = USB_SPEED_FULL;
    else if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_LSDEV)
        speed = USB_SPEED_LOW;

    return speed;
}

int musb_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint16_t used = 0;
    uint16_t fifo_size = 0;
    uint8_t old_ep_idx;
    uint32_t ui32Flags = 0;
    uint16_t ui32Register = 0;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (ep_idx > (CONFIG_MUSB_BIDIR_ENDPOINTS - 1)) {
        USB_LOG_ERR("Ep addr 0x%02x overflow\r\n", ep_desc->bEndpointAddress);
        return -2;
    }

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    if (ep_idx == 0) {
        udc->out_ep[0].ep_mps = ep_mps;
        udc->out_ep[0].ep_type = ep_type;
        udc->out_ep[0].ep_enable = true;
        udc->in_ep[0].ep_mps = ep_mps;
        udc->in_ep[0].ep_type = ep_type;
        udc->in_ep[0].ep_enable = true;
        return 0;
    }

    old_ep_idx = musb_get_active_ep(bus);
    musb_set_active_ep(bus, ep_idx);

    if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        udc->out_ep[ep_idx].ep_mps = ep_mps;
        udc->out_ep[ep_idx].ep_type = ep_type;
        udc->out_ep[ep_idx].ep_enable = true;

        HWREGH(USB_BASE + MUSB_IND_RXMAP_OFFSET) = ep_mps;

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
        if (ep_type == 0x01) {
            ui32Register |= USB_RXCSRH1_ISO;
        }

        HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = (USB_RXCSRL1_CLRDT | USB_RXCSRL1_FLUSH);
        else
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_CLRDT;

        fifo_size = musb_get_fifo_size(ep_mps, &used);

        HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = fifo_size & 0x0f;
        HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = (udc->fifo_size_offset >> 3);

        udc->fifo_size_offset += used;
    } else {
        udc->in_ep[ep_idx].ep_mps = ep_mps;
        udc->in_ep[ep_idx].ep_type = ep_type;
        udc->in_ep[ep_idx].ep_enable = true;

        HWREGH(USB_BASE + MUSB_IND_TXMAP_OFFSET) = ep_mps;

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
        if (ep_type == 0x01) {
            ui32Register |= USB_TXCSRH1_ISO;
        }

        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) = ui32Register;

        // Reset the Data toggle to zero.
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_TXCSRL1_CLRDT | USB_TXCSRL1_FLUSH);
        else
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_CLRDT;

        fifo_size = musb_get_fifo_size(ep_mps, &used);

        HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = fifo_size & 0x0f;
        HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = (udc->fifo_size_offset >> 3);

        udc->fifo_size_offset += used;
    }

    musb_set_active_ep(bus, old_ep_idx);

    return 0;
}

int musb_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    return 0;
}

int musb_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    old_ep_idx = musb_get_active_ep(bus);
    musb_set_active_ep(bus, ep_idx);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (ep_idx == 0x00) {
            udc->usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_STALL;
        }
    } else {
        if (ep_idx == 0x00) {
            udc->usb_ep0_state = USB_EP0_STATE_STALL;
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= (USB_CSRL0_STALL | USB_CSRL0_RXRDYC);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_STALL;
        }
    }

    musb_set_active_ep(bus, old_ep_idx);
    return 0;
}

int musb_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    old_ep_idx = musb_get_active_ep(bus);
    musb_set_active_ep(bus, ep_idx);

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

    musb_set_active_ep(bus, old_ep_idx);
    return 0;
}

int musb_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int musb_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }
    if (!udc->in_ep[ep_idx].ep_enable) {
        return -3;
    }

    old_ep_idx = musb_get_active_ep(bus);
    musb_set_active_ep(bus, ep_idx);

    if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
        musb_set_active_ep(bus, old_ep_idx);
        return -3;
    }

    udc->in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    udc->in_ep[ep_idx].xfer_len = data_len;
    udc->in_ep[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        if (ep_idx == 0x00) {
            if (udc->setup.wLength == 0) {
                udc->usb_ep0_state = USB_EP0_STATE_IN_STATUS;
            } else {
                udc->usb_ep0_state = USB_EP0_STATE_IN_ZLP;
            }
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
        }
        musb_set_active_ep(bus, old_ep_idx);
        return 0;
    }
    data_len = MIN(data_len, udc->in_ep[ep_idx].ep_mps);

    musb_write_packet(bus, ep_idx, (uint8_t *)data, data_len);
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);

    if (ep_idx == 0x00) {
        udc->usb_ep0_state = USB_EP0_STATE_IN_DATA;
        if (data_len < udc->in_ep[ep_idx].ep_mps) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_DATAEND);
        } else {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
        }
    } else {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
    }

    musb_set_active_ep(bus, old_ep_idx);
    return 0;
}

int musb_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t old_ep_idx;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }
    if (!udc->out_ep[ep_idx].ep_enable) {
        return -3;
    }

    old_ep_idx = musb_get_active_ep(bus);
    musb_set_active_ep(bus, ep_idx);

    udc->out_ep[ep_idx].xfer_buf = data;
    udc->out_ep[ep_idx].xfer_len = data_len;
    udc->out_ep[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        if (ep_idx == 0) {
            udc->usb_ep0_state = USB_EP0_STATE_SETUP;
        }
        musb_set_active_ep(bus, old_ep_idx);
        return 0;
    }
    if (ep_idx == 0) {
        udc->usb_ep0_state = USB_EP0_STATE_OUT_DATA;
    } else {
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
    }
    musb_set_active_ep(bus, old_ep_idx);
    return 0;
}

static void handle_ep0(struct usbd_bus *bus)
{
    uint8_t ep0_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);
    uint16_t read_count;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        udc->usb_ep0_state = USB_EP0_STATE_SETUP;
        return;
    }

    if (ep0_status & USB_CSRL0_SETEND) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_SETENDC;
    }

    if (udc->dev_addr > 0) {
        HWREGB(USB_BASE + MUSB_FADDR_OFFSET) = udc->dev_addr;
        udc->dev_addr = 0;
    }

    switch (udc->usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            if (ep0_status & USB_CSRL0_RXRDY) {
                read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                if (read_count != 8) {
                    return;
                }

                musb_read_packet(bus, 0, (uint8_t *)&udc->setup, 8);
                if (udc->setup.wLength) {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_RXRDYC;
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                }

                usbd_event_ep0_setup_complete_handler(bus->busid, (uint8_t *)&udc->setup);
            }
            break;

        case USB_EP0_STATE_IN_DATA:
            if (udc->in_ep[0].xfer_len > udc->in_ep[0].ep_mps) {
                udc->in_ep[0].actual_xfer_len += udc->in_ep[0].ep_mps;
                udc->in_ep[0].xfer_len -= udc->in_ep[0].ep_mps;
            } else {
                udc->in_ep[0].actual_xfer_len += udc->in_ep[0].xfer_len;
                udc->in_ep[0].xfer_len = 0;
            }

            usbd_event_ep_in_complete_handler(bus->busid, 0x80, udc->in_ep[0].actual_xfer_len);

            break;
        case USB_EP0_STATE_OUT_DATA:
            if (ep0_status & USB_CSRL0_RXRDY) {
                read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                musb_read_packet(0, udc->out_ep[0].xfer_buf, read_count);
                udc->out_ep[0].xfer_buf += read_count;
                udc->out_ep[0].actual_xfer_len += read_count;

                if (read_count < udc->out_ep[0].ep_mps) {
                    usbd_event_ep_out_complete_handler(bus->busid, 0x00, udc->out_ep[0].actual_xfer_len);
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_RXRDYC | USB_CSRL0_DATAEND);
                    udc->usb_ep0_state = USB_EP0_STATE_IN_STATUS;
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_RXRDYC;
                }
            }
            break;
        case USB_EP0_STATE_IN_STATUS:
        case USB_EP0_STATE_IN_ZLP:
            udc->usb_ep0_state = USB_EP0_STATE_SETUP;
            usbd_event_ep_in_complete_handler(bus->busid, 0x80, 0);
            break;
    }
}

void musb_udc_irq(struct usbd_bus *bus)
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint8_t old_ep_idx;
    uint8_t ep_idx;
    uint16_t write_count, read_count;
    struct musb_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    is = HWREGB(USB_BASE + MUSB_IS_OFFSET);
    txis = HWREGH(USB_BASE + MUSB_TXIS_OFFSET);
    rxis = HWREGH(USB_BASE + MUSB_RXIS_OFFSET);

    HWREGB(USB_BASE + MUSB_IS_OFFSET) = is;

    old_ep_idx = musb_get_active_ep(bus);

    /* Receive a reset signal from the USB bus */
    if (is & USB_IS_RESET) {
        memset(udc->in_ep, 0, sizeof(struct musb_ep) * CONFIG_MUSB_BIDIR_ENDPOINTS);
        memset(udc->out_ep, 0, sizeof(struct musb_ep) * CONFIG_MUSB_BIDIR_ENDPOINTS);
        udc->fifo_size_offset = USB_CTRL_EP_MPS;
        udc->usb_ep0_state = USB_EP0_STATE_SETUP;
        usbd_event_reset_handler(bus->busid);
        HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
        HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

        for (uint8_t i = 1; i < CONFIG_MUSB_BIDIR_ENDPOINTS; i++) {
            musb_set_active_ep(bus, i);
            HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = 0;
            HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = 0;
            HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = 0;
            HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = 0;
        }
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
        musb_set_active_ep(bus, 0);
        handle_ep0(bus);
        txis &= ~USB_TXIE_EP0;
    }

    ep_idx = 1;
    while (txis) {
        if (txis & (1 << ep_idx)) {
            musb_set_active_ep(bus, ep_idx);
            HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_UNDRN) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_UNDRN;
            }

            if (udc->in_ep[ep_idx].xfer_len > udc->in_ep[ep_idx].ep_mps) {
                udc->in_ep[ep_idx].xfer_buf += udc->in_ep[ep_idx].ep_mps;
                udc->in_ep[ep_idx].actual_xfer_len += udc->in_ep[ep_idx].ep_mps;
                udc->in_ep[ep_idx].xfer_len -= udc->in_ep[ep_idx].ep_mps;
            } else {
                udc->in_ep[ep_idx].xfer_buf += udc->in_ep[ep_idx].xfer_len;
                udc->in_ep[ep_idx].actual_xfer_len += udc->in_ep[ep_idx].xfer_len;
                udc->in_ep[ep_idx].xfer_len = 0;
            }

            if (udc->in_ep[ep_idx].xfer_len == 0) {
                HWREGH(USB_BASE + MUSB_TXIE_OFFSET) &= ~(1 << ep_idx);
                usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, udc->in_ep[ep_idx].actual_xfer_len);
            } else {
                write_count = MIN(udc->in_ep[ep_idx].xfer_len, udc->in_ep[ep_idx].ep_mps);

                musb_write_packet(bus, ep_idx, udc->in_ep[ep_idx].xfer_buf, write_count);
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
            }

            txis &= ~(1 << ep_idx);
        }
        ep_idx++;
    }

    rxis &= HWREGH(USB_BASE + MUSB_RXIE_OFFSET);
    ep_idx = 1;
    while (rxis) {
        if (rxis & (1 << ep_idx)) {
            musb_set_active_ep(bus, ep_idx);
            HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << ep_idx);
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY) {
                read_count = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);

                musb_read_packet(bus, ep_idx, udc->out_ep[ep_idx].xfer_buf, read_count);
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_RXRDY);

                udc->out_ep[ep_idx].xfer_buf += read_count;
                udc->out_ep[ep_idx].actual_xfer_len += read_count;
                udc->out_ep[ep_idx].xfer_len -= read_count;

                if ((read_count < udc->out_ep[ep_idx].ep_mps) || (udc->out_ep[ep_idx].xfer_len == 0)) {
                    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) &= ~(1 << ep_idx);
                    usbd_event_ep_out_complete_handler(bus->busid, ep_idx, udc->out_ep[ep_idx].actual_xfer_len);
                } else {
                }
            }

            rxis &= ~(1 << ep_idx);
        }
        ep_idx++;
    }

    musb_set_active_ep(bus, old_ep_idx);
}

struct usbd_udc_driver musb_udc_driver = {
    .driver_name = "musb udc",
    .udc_init = musb_udc_init,
    .udc_deinit = musb_udc_deinit,
    .udc_set_address = musb_set_address,
    .udc_get_port_speed = musb_get_port_speed,
    .udc_ep_open = musb_ep_open,
    .udc_ep_close = musb_ep_close,
    .udc_ep_set_stall = musb_ep_set_stall,
    .udc_ep_clear_stall = musb_ep_clear_stall,
    .udc_ep_is_stalled = musb_ep_is_stalled,
    .udc_ep_start_write = musb_ep_start_write,
    .udc_ep_start_read = musb_ep_start_read,
    .udc_irq = musb_udc_irq
};

/* ============= special for es32 =============== */

#undef USB_BASE
#define USB_BASE 0x40086400

uint8_t usbd_force_full_speed(const uint8_t port)
{
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~USB_POWER_HSENAB;
    return (HWREGB(USB_BASE + MUSB_POWER_OFFSET) & USB_POWER_HSENAB);
}

int usb_ep_out_data_avail(uint8_t ep_addr)
{
    uint16_t old_ep_idx, length;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);

    old_ep_idx = musb_get_active_ep(NULL);
    musb_set_active_ep(NULL, ep_idx);

    if (ep_idx == 0) {
        if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_CSRL0_RXRDY)) {
            musb_set_active_ep(NULL, old_ep_idx);
            return 0;
        }
        length = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
        musb_set_active_ep(NULL, old_ep_idx);
        return length;
    } else {
        if (!(HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)) {
            musb_set_active_ep(NULL, old_ep_idx);
            return 0;
        }
        length = HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET);
        musb_set_active_ep(NULL, old_ep_idx);
        return length;
    }
}

int usb_ep_in_data_avail(uint8_t ep_addr)
{
    uint16_t old_ep_idx, length;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);

    old_ep_idx = musb_get_active_ep(NULL);
    musb_set_active_ep(NULL, ep_idx);

    if (ep_idx == 0) {
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_CSRL0_TXRDY) {
            musb_set_active_ep(NULL, old_ep_idx);
            return 0;
        }
    } else {
        if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
            musb_set_active_ep(NULL, old_ep_idx);
            return 0;
        }
    }
    length = HWREGH(USB_BASE + MUSB_IND_TXMAP_OFFSET);
    musb_set_active_ep(NULL, old_ep_idx);
    return length;
}

int usb_ep_wait_in_data_avail(uint8_t ep_addr)
{
    uint32_t cnt;

    for (cnt = 0; cnt < 3000; cnt++) {
        if (usb_ep_in_data_avail(ep_addr))
            return cnt;
    }
    return 0;
}

int usbd_read_packet(uint8_t ep_addr, uint8_t *buffer, uint16_t len)
{
    uint16_t old_ep_idx, cnt;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);

    old_ep_idx = musb_get_active_ep(NULL);
    musb_set_active_ep(NULL, ep_idx);
    if (ep_idx == 0) {
        if (!(HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_CSRL0_RXRDY)) {
            musb_set_active_ep(NULL, old_ep_idx);
            return 0;
        }
    } else {
        if (!(HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)) {
            musb_set_active_ep(NULL, old_ep_idx);
            return 0;
        }
    }
    cnt = usb_ep_out_data_avail(ep_idx);
    if (cnt) {
        musb_read_packet(NULL, ep_idx, buffer, cnt);
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_OVER | USB_RXCSRL1_ERROR | USB_RXCSRL1_STALL | USB_RXCSRL1_STALLED);
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~(USB_RXCSRL1_RXRDY);
        musb_set_active_ep(NULL, old_ep_idx);
    }
    return cnt;
}

int usbd_write_packet(uint8_t ep_addr, uint8_t *buffer, uint16_t len)
{
    uint16_t old_ep_idx, cnt;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_addr);

    old_ep_idx = musb_get_active_ep(NULL);
    musb_set_active_ep(NULL, ep_idx);
    if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_UNDRN) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_UNDRN;
    }
    if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY) {
        musb_set_active_ep(NULL, old_ep_idx);
        return -1;
    }

    if (!buffer && len) {
        return -2;
    }

    if (!len) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_TXRDY;
        return 0;
    }

    cnt = usb_ep_in_data_avail(ep_idx);
    if (cnt) {
        cnt = MIN(cnt, len);
        musb_write_packet(NULL, ep_idx, buffer, cnt);
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_TXRDY;
        musb_set_active_ep(NULL, old_ep_idx);
    }
    return cnt;
}
