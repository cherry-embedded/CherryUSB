/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_usbhs_reg.h"

#define USBHSH ((USBHSH_TypeDef *)bus->hcd.reg_base)

#define SPLIT_MAX_RETRY             6
#define HIGH_SPLIT_ISO_OUT_MAX_SIZE 188

typedef enum {
    USB_EP0_STATE_SETUP,
    USB_EP0_STATE_DATA,
    USB_EP0_STATE_STATUS,
} ep0_state_t;

typedef enum {
    XFER_STATE_IDLE,
    XFER_STATE_BUSY,
    XFER_STATE_COMP,
} xfer_state_t;

typedef enum {
    ENDP_TOG_DATA0,
    ENDP_TOG_DATA1,
    ENDP_TOG_DATA2,
    ENDP_TOG_MDATA,
} endp_tog_t;

struct usbhs_pipe;

typedef struct
{
    union {
        uint32_t split_data;

        struct
        {
            uint32_t hub_addr : 7;
            uint32_t sc       : 1;
            uint32_t port     : 7;
            uint32_t s        : 1;
            uint32_t e        : 1;
            uint32_t et       : 2;
            uint32_t reserved : 13;
        };
    };

    uint16_t iso_out_length;
    uint16_t iso_out_offset;
} split_data_t;

typedef struct usbhs_xfer {
    uint8_t pre;
    uint8_t endp;
    uint8_t token;
    uint8_t toggle;
    uint8_t dev_addr;
    uint8_t xfer_state;
    uint8_t split_retry;
    uint8_t split_mframe;
    uint16_t length;
    uint8_t *buffer;
    split_data_t split_data;
    struct usbhs_pipe *pipe;
    struct usbhs_xfer *next;
} usbhs_xfer_t;

struct usbhs_pipe {
    bool used;
    bool killed;
    bool ping;
    bool ping_en;
    uint8_t type;
    uint8_t ep0_state;
    uint32_t tick;
    uint32_t interval;
    usb_osal_sem_t waitsem;
    usbhs_xfer_t xfer;
    struct usbh_urb *urb;
    struct usbhs_pipe *prev;
    struct usbhs_pipe *next;
};

struct usbhs_hcd {
    bool sof_act;
    bool port_csc;
    bool port_pec;
    bool port_ssc;
    bool port_rsc;
    uint8_t speed;
    uint32_t tick;
    usbhs_xfer_t *curr_xfer;
    struct usbhs_pipe *pipe_list[4];
    struct usbhs_pipe pipe_pool[8];
} g_usbhs_hcd[CONFIG_USBHOST_MAX_BUS];

static struct usbhs_pipe *usbhs_pipe_alloc(struct usbh_bus *bus, struct usbh_urb *urb, struct usb_endpoint_descriptor *ep)
{
    for (size_t chidx = 0; chidx < sizeof(g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool) / sizeof(struct usbhs_pipe); chidx++) {
        if (!g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].used) {
            uint8_t type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].used = true;
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].killed = false;
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].ping = false;
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].type = type;
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].ep0_state = USB_EP0_STATE_SETUP;
            if (g_usbhs_hcd[bus->hcd.hcd_id].speed == USB_SPEED_HIGH &&
                (type == USB_ENDPOINT_TYPE_CONTROL || type == USB_ENDPOINT_TYPE_BULK)) {
                g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].ping_en = true;
            } else {
                g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].ping_en = false;
            }
            if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS || type == USB_ENDPOINT_TYPE_INTERRUPT) {
                uint8_t interval = ep->bInterval;
                if (g_usbhs_hcd[bus->hcd.hcd_id].speed == USB_SPEED_HIGH) {
                    if (urb->hport->speed == USB_SPEED_HIGH) {
                        g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = 1 << (interval - 1);
                    } else if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                        g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = (1 << (interval - 1)) << 3;
                    } else {
                        g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = interval << 3;
                    }
                } else if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                    g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = 1 << (interval - 1);
                } else {
                    g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = interval;
                }
            } else {
                g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = 0;
            }

            memset(&g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].xfer, 0, sizeof(usbhs_xfer_t));
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].urb = urb;
            if (g_usbhs_hcd[bus->hcd.hcd_id].pipe_list[type] == NULL) {
                g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].prev = NULL;
                g_usbhs_hcd[bus->hcd.hcd_id].pipe_list[type] = &g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
            } else {
                struct usbhs_pipe *ppipe = g_usbhs_hcd[bus->hcd.hcd_id].pipe_list[type];
                while (ppipe->next) {
                    ppipe = ppipe->next;
                }
                g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].prev = ppipe;
                ppipe->next = &g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
            }
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].next = NULL;
            return &g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
        }
    }

    return NULL;
}

static void usbhs_pipe_free(struct usbh_bus *bus, struct usbhs_pipe *pipe, uint8_t type)
{
    usbhs_xfer_t *xfer = &pipe->xfer;

    if (xfer->xfer_state != XFER_STATE_BUSY) {
        if (pipe->prev) {
            pipe->prev->next = pipe->next;
        } else {
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_list[type] = pipe->next;
        }

        if (pipe->next) {
            pipe->next->prev = pipe->prev;
        }

        pipe->used = false;
    } else {
        pipe->killed = true;
    }
}

static usbhs_xfer_t **usbhs_xfer_process(struct usbh_bus *bus, usbhs_xfer_t **last_xfer, uint8_t type)
{
    struct usbhs_pipe *pipe = g_usbhs_hcd[bus->hcd.hcd_id].pipe_list[type];

    while (pipe) {
        usbhs_xfer_t *xfer = &pipe->xfer;
        if (xfer->xfer_state != XFER_STATE_COMP) {
            struct usbh_urb *urb = pipe->urb;
            if (urb->hport->speed != g_usbhs_hcd[bus->hcd.hcd_id].speed) {
                if (g_usbhs_hcd[bus->hcd.hcd_id].speed == USB_SPEED_FULL) {
                    xfer->pre = 1;
                } else {
                    split_data_t *split = &xfer->split_data;
                    uint8_t mframe = (USBHSH->FRAME & USBHS_UH_MFRAME_NO) >> 16;

                    if (split->sc == 1 || split->iso_out_offset) {
                        if (mframe != xfer->split_mframe) {
                            xfer->split_mframe = mframe;
                            xfer->xfer_state = XFER_STATE_BUSY;
                            *last_xfer = xfer;
                            last_xfer = &xfer->next;
                        }

                        pipe = pipe->next;
                        continue;
                    }

                    static const uint8_t valid_microframe[] = { 6, 1, 6, 2 };
                    if (mframe >= valid_microframe[type]) {
                        pipe = pipe->next;
                        continue;
                    }

                    struct usbh_hubport *port = urb->hport;
                    while (port->parent && port->parent->speed != USB_SPEED_HIGH) {
                        port = port->parent->parent;
                    }

                    split->hub_addr = port->parent->hub_addr;
                    split->sc = 0;
                    split->port = port->port;
                    split->s = urb->hport->speed == USB_SPEED_LOW ? 1 : 0;
                    split->e = 0;
                    split->et = type;

                    if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS && xfer->token == USB_PID_OUT) {
                        split->iso_out_length = MIN(urb->transfer_buffer_length - urb->actual_length, 1023);
                        split->iso_out_offset = 0;
                        split->s = 1;
                        split->e = split->iso_out_length <= HIGH_SPLIT_ISO_OUT_MAX_SIZE ? 1 : 0;
                    }
                }
            }

            if (pipe->ping) {
                xfer->token = USB_PID_PING;
                xfer->xfer_state = XFER_STATE_BUSY;
                *last_xfer = xfer;
                last_xfer = &xfer->next;
                pipe = pipe->next;
                continue;
            }

            xfer->dev_addr = urb->hport->dev_addr;
            xfer->pipe = pipe;

            if (type == USB_ENDPOINT_TYPE_CONTROL) {
                xfer->endp = 0;
                switch (pipe->ep0_state) {
                    case USB_EP0_STATE_SETUP:
                        xfer->token = USB_PID_SETUP;
                        xfer->toggle = ENDP_TOG_DATA0;
                        xfer->length = sizeof(struct usb_setup_packet);
                        xfer->buffer = (uint8_t *)(urb->setup);
                        break;

                    case USB_EP0_STATE_DATA:
                        xfer->token = urb->setup->bmRequestType & 0x80 ? USB_PID_IN : USB_PID_OUT;
                        xfer->toggle = urb->data_toggle;
                        xfer->length = MIN(urb->transfer_buffer_length - (urb->actual_length - sizeof(struct usb_setup_packet)),
                                           USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize));
                        xfer->buffer = urb->transfer_buffer + urb->actual_length - sizeof(struct usb_setup_packet);
                        break;

                    case USB_EP0_STATE_STATUS:
                        xfer->token = urb->setup->bmRequestType & 0x80 ? USB_PID_OUT : USB_PID_IN;
                        xfer->toggle = ENDP_TOG_DATA1;
                        xfer->length = 0;
                        xfer->buffer = NULL;
                        break;
                }

                xfer->xfer_state = XFER_STATE_BUSY;
                *last_xfer = xfer;
                last_xfer = &xfer->next;
            } else if (pipe->interval == 0 || g_usbhs_hcd[bus->hcd.hcd_id].tick - pipe->tick >= pipe->interval) {
                pipe->tick = g_usbhs_hcd[bus->hcd.hcd_id].tick;
                xfer->endp = USB_EP_GET_IDX(urb->ep->bEndpointAddress);
                xfer->token = USB_EP_GET_DIR(urb->ep->bEndpointAddress) ? USB_PID_IN : USB_PID_OUT;
                xfer->toggle = urb->data_toggle;
                xfer->length = MIN(urb->transfer_buffer_length - urb->actual_length, USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize));
                xfer->buffer = urb->transfer_buffer + urb->actual_length;

                xfer->xfer_state = XFER_STATE_BUSY;
                *last_xfer = xfer;
                last_xfer = &xfer->next;
            }
        }

        pipe = pipe->next;
    }

    return last_xfer;
}

static void usbhs_transfer_start(struct usbh_bus *bus)
{
    if ((USBHSH->CONTROL & USBHS_UH_HOST_ACTION) || (USBHSH->INT_FLAG & USBHS_UHIF_TRANSFER) ||
        !(USBHSH->PORT_STATUS & USBHS_UHIS_PORT_CONNECT)) {
        return;
    }

    if (g_usbhs_hcd[bus->hcd.hcd_id].sof_act || !g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer) {
        g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer = NULL;
        usbhs_xfer_t **last_xfer = &g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer;

        if (g_usbhs_hcd[bus->hcd.hcd_id].sof_act) {
            g_usbhs_hcd[bus->hcd.hcd_id].sof_act = 0;
            last_xfer = usbhs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_ISOCHRONOUS);
            last_xfer = usbhs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_INTERRUPT);
        }

        last_xfer = usbhs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_CONTROL);
        last_xfer = usbhs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_BULK);
        *last_xfer = NULL;
    }

    if (g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer) {
        usbhs_xfer_t *xfer = g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer;
        uint32_t supplement = 0;
        if (xfer->split_data.split_data) {
            split_data_t *split = &xfer->split_data;
            USBHSH->SPLIT = split->split_data;

            if (split->sc == 0) {
                if (split->et == USB_ENDPOINT_TYPE_ISOCHRONOUS || split->et == USB_ENDPOINT_TYPE_INTERRUPT) {
                    supplement = USBHS_UH_SPLIT_VALID | USBHS_UH_TX_NO_RES | USBHS_UH_RX_NO_DATA;
                } else {
                    supplement = USBHS_UH_SPLIT_VALID | USBHS_UH_RX_NO_DATA;
                }
            } else {
                supplement = USBHS_UH_SPLIT_VALID | USBHS_UH_RX_NO_RES | USBHS_UH_TX_NO_DATA;
            }
        } else if (xfer->pre) {
            supplement = USBHS_UH_PRE_PID_EN;
        }

        if (xfer->pipe->type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            supplement |= USBHS_UH_RX_NO_RES | USBHS_UH_TX_NO_RES;
        }

        USBHSH->DEV_ADDR = xfer->dev_addr;
        if (xfer->token == USB_PID_IN) {
            USBHSH->RX_MAX_LEN = xfer->length;
            USBHSH->RX_DMA = (uint32_t)xfer->buffer;
            USBHSH->CONTROL = USBHS_UH_HOST_ACTION | xfer->token | (xfer->endp << 4) | supplement;
        } else if (xfer->token == USB_PID_OUT || xfer->token == USB_PID_SETUP) {
            USBHSH->TX_LEN = xfer->length;
            USBHSH->TX_DMA = (uint32_t)xfer->buffer;
            USBHSH->CONTROL = USBHS_UH_HOST_ACTION | xfer->token | (xfer->endp << 4) | (xfer->toggle << 8) | supplement;
        } else if (xfer->token == USB_PID_PING) {
            USBHSH->TX_LEN = 0;
            USBHSH->TX_DMA = 0;
            USBHSH->CONTROL = USBHS_UH_HOST_ACTION | xfer->token | (xfer->endp << 4);
        }
    }
}

static bool usbhs_split_transfer_complete(struct usbh_bus *bus, usbhs_xfer_t *xfer, uint8_t recv_pid)
{
    split_data_t *split = &xfer->split_data;

    if (split->sc == 0) {
        if (split->iso_out_length) {
            split->iso_out_offset += xfer->length;

            if (split->iso_out_offset >= split->iso_out_length) {
                xfer->length = split->iso_out_offset;
                split->iso_out_offset = 0;
                return true;
            }

            xfer->buffer += xfer->length;

            uint16_t remaining_length = split->iso_out_length - split->iso_out_offset;
            if (remaining_length <= HIGH_SPLIT_ISO_OUT_MAX_SIZE) {
                split->s = 0;
                split->e = 1;
                xfer->length = remaining_length;
            } else {
                split->s = 0;
                split->e = 0;
                xfer->length = HIGH_SPLIT_ISO_OUT_MAX_SIZE;
            }

            return false;
        }

        if (split->et == USB_ENDPOINT_TYPE_INTERRUPT || split->et == USB_ENDPOINT_TYPE_ISOCHRONOUS || recv_pid == USB_PID_ACK) {
            split->sc = 1;
            xfer->split_retry = 0;
            xfer->split_mframe = (USBHSH->FRAME & USBHS_UH_MFRAME_NO) >> 16;
            return false;
        }
    } else if (recv_pid == USB_PID_NYET) {
        xfer->split_retry++;
        if (xfer->split_retry >= SPLIT_MAX_RETRY) {
            split->sc = 0;
        }
        return false;
    } else if (recv_pid == USB_PID_MDATA) {
        xfer->buffer += xfer->length;
        return false;
    }

    split->sc = 0;
    return true;
}

static void usbhs_transfer_complete(struct usbh_bus *bus, uint8_t recv_pid, size_t recv_len)
{
    static const uint8_t tog_pid[] = { USB_PID_DATA0, USB_PID_DATA1, USB_PID_DATA2, USB_PID_MDATA };

    usbhs_xfer_t *xfer = g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer;
    struct usbhs_pipe *pipe = xfer->pipe;

    xfer->xfer_state = XFER_STATE_IDLE;
    if (pipe->killed) {
        usbhs_pipe_free(bus, pipe, pipe->type);
        return;
    }

    if (xfer->split_data.split_data && !usbhs_split_transfer_complete(bus, xfer, recv_pid)) {
        return;
    }

    struct usbh_urb *urb = pipe->urb;
    if (recv_pid == USB_PID_ACK || recv_pid == USB_PID_NYET || recv_pid == tog_pid[xfer->toggle]) {
        if (xfer->token == USB_PID_PING) {
            pipe->ping = false;
            return;
        }

        if (recv_pid == USB_PID_NYET) {
            pipe->ping = true;
        }

        size_t len = xfer->token == USB_PID_IN ? recv_len : xfer->length;
        urb->actual_length += len;

        if (xfer->endp != 0) {
            urb->data_toggle ^= ENDP_TOG_DATA1;
            if (len < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize) || urb->actual_length >= urb->transfer_buffer_length) {
                urb->errorcode = 0;
                goto end;
            }
        } else if (pipe->ep0_state == USB_EP0_STATE_SETUP) {
            urb->data_toggle = ENDP_TOG_DATA1;
            pipe->ep0_state = urb->transfer_buffer_length ? USB_EP0_STATE_DATA : USB_EP0_STATE_STATUS;
        } else if (pipe->ep0_state == USB_EP0_STATE_DATA) {
            if (len < USB_GET_MAXPACKETSIZE(urb->ep->wMaxPacketSize) ||
                (urb->actual_length - sizeof(struct usb_setup_packet)) >= urb->transfer_buffer_length) {
                urb->data_toggle = ENDP_TOG_DATA1;
                pipe->ep0_state = USB_EP0_STATE_STATUS;
            } else {
                urb->data_toggle ^= ENDP_TOG_DATA1;
            }
        } else {
            urb->errorcode = 0;
            goto end;
        }
    } else if (recv_pid == USB_PID_STALL) {
        urb->errorcode = -USB_ERR_STALL;
        goto end;
    } else if (recv_pid == USB_PID_NAK) {
        if (pipe->ping_en && xfer->token == USB_PID_OUT) {
            pipe->ping = true;
        }
    } else {
        urb->errorcode = -USB_ERR_IO;
        goto end;
    }
    return;

end:
    xfer->xfer_state = XFER_STATE_COMP;
    if (urb->timeout) {
        usb_osal_sem_give(pipe->waitsem);
    } else {
        usbhs_pipe_free(bus, pipe, pipe->type);
    }

    if (urb->complete) {
        if (urb->errorcode < 0) {
            urb->complete(urb->arg, urb->errorcode);
        } else {
            urb->complete(urb->arg, urb->actual_length);
        }
    }
}

__WEAK void usb_hc_low_level_init(struct usbh_bus *bus)
{
    (void)bus;
}

__WEAK void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    (void)bus;
}

int usb_hc_init(struct usbh_bus *bus)
{
    memset(&g_usbhs_hcd[bus->hcd.hcd_id], 0, sizeof(struct usbhs_hcd));
    usb_hc_low_level_init(bus);

    for (uint8_t i = 0; i < sizeof(g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool) / sizeof(struct usbhs_pipe); i++) {
        g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem = usb_osal_sem_create(0);
        USB_ASSERT(g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem != NULL);
    }

    USBHSH->CFG = USBHS_RST_LINK | USBHS_UH_PHY_SUSPENDM;
    USBHSH->PORT_CFG = USBHS_UH_PD_EN | USBHS_UH_HOST_EN;
    USBHSH->FRAME |= USBHS_UH_SOF_CNT_EN;
    USBHSH->CFG = USBHS_UH_SOF_EN | USBHS_UH_DMA_EN | USBHS_UH_PHY_SUSPENDM;
    USBHSH->INT_EN = USBHS_UHIE_SOF_ACT | USBHS_UHIE_TRANSFER;
    USBHSH->PORT_INT_EN = USBHS_UHIE_PORT_RESET | USBHS_UHIE_PORT_SUSP | USBHS_UHIE_PORT_EN | USBHS_UHIE_PORT_CONNECT;
    return 0;
}

int usb_hc_deinit(struct usbh_bus *bus)
{
    USBHSH->CFG = USBHS_RST_LINK | USBHS_RST_SIE | USBHS_UH_CLR_ALL;

    for (uint8_t i = 0; i < sizeof(g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool) / sizeof(struct usbhs_pipe); i++) {
        if (g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem) {
            usb_osal_sem_delete(g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem);
            g_usbhs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem = NULL;
        }
    }

    usb_hc_low_level_deinit(bus);
    return 0;
}

uint16_t usbh_get_frame_number(struct usbh_bus *bus)
{
    return USBHSH->FRAME & USBHS_UH_FRAME_NO;
}

int usbh_roothub_control(struct usbh_bus *bus, struct usb_setup_packet *setup, uint8_t *buf)
{
    uint8_t nports;
    uint8_t port;
    uint32_t status;

    nports = CONFIG_USBHOST_MAX_RHPORTS;
    port = setup->wIndex;
    if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_DEVICE) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -USB_ERR_NOTSUPP;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -USB_ERR_NOTSUPP;
                }
                break;
            case HUB_REQUEST_GET_DESCRIPTOR:
                break;
            case HUB_REQUEST_GET_STATUS:
                memset(buf, 0, 4);
                break;
            default:
                break;
        }
    } else if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_OTHER) {
        uint16_t port_status;
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_ENABLE:
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        g_usbhs_hcd[bus->hcd.hcd_id].port_ssc = 0;
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        g_usbhs_hcd[bus->hcd.hcd_id].port_csc = 0;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        g_usbhs_hcd[bus->hcd.hcd_id].port_pec = 0;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
                        g_usbhs_hcd[bus->hcd.hcd_id].port_rsc = 0;
                        break;
                    default:
                        return -USB_ERR_NOTSUPP;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        USBHSH->PORT_CTRL |= USBHS_UH_SET_PORT_RESET;
                        break;

                    default:
                        return -USB_ERR_NOTSUPP;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
                }
                status = 0;

                if (g_usbhs_hcd[bus->hcd.hcd_id].port_csc)
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                if (g_usbhs_hcd[bus->hcd.hcd_id].port_pec)
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                if (g_usbhs_hcd[bus->hcd.hcd_id].port_ssc)
                    status |= (1 << HUB_PORT_FEATURE_C_SUSPEND);
                if (g_usbhs_hcd[bus->hcd.hcd_id].port_rsc)
                    status |= (1 << HUB_PORT_FEATURE_C_RESET);

                port_status = USBHSH->PORT_STATUS;
                status |= (1 << HUB_PORT_FEATURE_POWER);
                if (port_status & USBHS_UHIS_PORT_CONNECT)
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                if (port_status & USBHS_UHIS_PORT_EN)
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);
                if (port_status & USBHS_UHIS_PORT_SUSP)
                    status |= (1 << HUB_PORT_FEATURE_SUSPEND);
                if (port_status & USBHS_UHIS_PORT_RST)
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                if (port_status & USBHS_UHIS_PORT_LS)
                    status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                if (port_status & USBHS_UHIS_PORT_HS)
                    status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                if (port_status & USBHS_UHIS_PORT_TEST)
                    status |= (1 << HUB_PORT_FEATURE_TEST);

                if (port_status & USBHS_UHIS_PORT_HS)
                    g_usbhs_hcd[bus->hcd.hcd_id].speed = USB_SPEED_HIGH;
                else if (port_status & USBHS_UHIS_PORT_LS)
                    g_usbhs_hcd[bus->hcd.hcd_id].speed = USB_SPEED_LOW;
                else
                    g_usbhs_hcd[bus->hcd.hcd_id].speed = USB_SPEED_FULL;

                memcpy(buf, &status, 4);
                break;
            default:
                break;
        }
    }
    return 0;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    if (!urb || !urb->hport || !urb->ep || !urb->hport->bus) {
        return -USB_ERR_INVAL;
    }

    int ret = 0;
    struct usbh_bus *bus = urb->hport->bus;

    /* dma addr must be aligned 4 bytes */
    USB_ASSERT_MSG(!((uintptr_t)urb->setup % 4) && !((uintptr_t)urb->transfer_buffer % 4),
                   "urb->setup or urb->transfer_buffer is not aligned 4 bytes");

    if (!urb->hport->connected) {
        return -USB_ERR_NOTCONN;
    }

    if (urb->errorcode == -USB_ERR_BUSY) {
        return -USB_ERR_BUSY;
    }

    size_t flags = usb_osal_enter_critical_section();
    struct usbhs_pipe *pipe = usbhs_pipe_alloc(bus, urb, urb->ep);
    if (!pipe) {
        usb_osal_leave_critical_section(flags);
        return -USB_ERR_NOMEM;
    }

    urb->hcpriv = pipe;
    urb->errorcode = -USB_ERR_BUSY;
    urb->actual_length = 0;
    usbhs_transfer_start(bus);
    usb_osal_leave_critical_section(flags);

    if (urb->timeout > 0) {
        /* wait until timeout or sem give */
        ret = usb_osal_sem_take(pipe->waitsem, urb->timeout);
        if (ret < 0) {
            goto errout_timeout;
        }
        urb->timeout = 0;
        ret = urb->errorcode;

        /* we can free pipe when waitsem is done */
        usbhs_pipe_free(bus, pipe, pipe->type);
    }
    return ret;

errout_timeout:
    urb->timeout = 0;
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    if (!urb || !urb->hcpriv || !urb->hport->bus) {
        return -USB_ERR_INVAL;
    }

    size_t flags = usb_osal_enter_critical_section();

    struct usbhs_pipe *pipe = urb->hcpriv;

    urb->errorcode = -USB_ERR_SHUTDOWN;

    if (urb->timeout) {
        usb_osal_sem_give(pipe->waitsem);
    } else {
        usbhs_pipe_free(urb->hport->bus, pipe, USB_GET_ENDPOINT_TYPE(urb->ep->bmAttributes));
    }

    if (urb->complete) {
        urb->complete(urb->arg, urb->errorcode);
    }

    usb_osal_leave_critical_section(flags);
    return 0;
}

void USBH_IRQHandler(uint8_t busid)
{
    struct usbh_bus *bus = &g_usbhost_bus[busid];

    /* Get the interrupt flag */
    uint8_t int_flag = USBHSH->INT_FLAG;

    if (int_flag & USBHS_UHIF_SOF_ACT) {
        g_usbhs_hcd[bus->hcd.hcd_id].tick++;
        g_usbhs_hcd[bus->hcd.hcd_id].sof_act = true;
        usbhs_transfer_start(bus);
        USBHSH->INT_FLAG = USBHS_UHIF_SOF_ACT;
    } else if (int_flag & USBHS_UHIF_TRANSFER) {
        usbhs_transfer_complete(bus, USBHSH->INT_ST & 0x0F, USBHSH->RX_LEN);
        g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer = g_usbhs_hcd[bus->hcd.hcd_id].curr_xfer->next;
        USBHSH->INT_FLAG = USBHS_UHIF_TRANSFER;
        usbhs_transfer_start(bus);
    }

    /* Get the port change status */
    uint8_t port_change = USBHSH->PORT_STATUS_CHG;

    if (port_change) {
        bus->hcd.roothub.int_buffer[0] = (1 << 1);
        usbh_hub_thread_wakeup(&bus->hcd.roothub);

        if (port_change & USBHS_UHIF_PORT_CONNECT) {
            g_usbhs_hcd[bus->hcd.hcd_id].port_csc = 1;
        }

        if (port_change & USBHS_UHIF_PORT_EN) {
            g_usbhs_hcd[bus->hcd.hcd_id].port_pec = 1;
        }

        if (port_change & USBHS_UHIF_PORT_SUSP) {
            g_usbhs_hcd[bus->hcd.hcd_id].port_ssc = 1;
        }

        if (port_change & USBHS_UHIF_PORT_RESET) {
            g_usbhs_hcd[bus->hcd.hcd_id].port_rsc = 1;
        }

        USBHSH->PORT_STATUS_CHG = port_change;
    }
}
