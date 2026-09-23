/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_usbfs_reg.h"

#define USBFSH ((USBFSH_TypeDef *)bus->hcd.reg_base)

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

struct usbfs_pipe;

typedef struct usbfs_xfer {
    uint8_t pre;
    uint8_t endp;
    uint8_t token;
    uint8_t toggle;
    uint8_t dev_addr;
    uint8_t xfer_state;
    uint16_t length;
    uint8_t *buffer;
    struct usbfs_pipe *pipe;
    struct usbfs_xfer *next;
} usbfs_xfer_t;

struct usbfs_pipe {
    bool used;
    bool killed;
    uint8_t type;
    uint8_t ep0_state;
    uint32_t tick;
    uint32_t interval;
    usb_osal_sem_t waitsem;
    usbfs_xfer_t xfer;
    struct usbh_urb *urb;
    struct usbfs_pipe *prev;
    struct usbfs_pipe *next;
};

struct usbfs_hcd {
    bool sof_act;
    bool port_csc;
    uint8_t speed;
    uint32_t tick;
    usbfs_xfer_t *curr_xfer;
    struct usbfs_pipe *pipe_list[4];
    struct usbfs_pipe pipe_pool[8];
} g_usbfs_hcd[CONFIG_USBHOST_MAX_BUS];

static struct usbfs_pipe *usbfs_pipe_alloc(struct usbh_bus *bus, struct usbh_urb *urb, struct usb_endpoint_descriptor *ep)
{
    for (size_t chidx = 0; chidx < sizeof(g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool) / sizeof(struct usbfs_pipe); chidx++) {
        if (!g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].used) {
            uint8_t type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].used = true;
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].killed = false;
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].type = type;
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].ep0_state = USB_EP0_STATE_SETUP;
            if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS || type == USB_ENDPOINT_TYPE_INTERRUPT) {
                uint8_t interval = ep->bInterval;
                if (type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                    g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = 1 << (interval - 1);
                } else {
                    g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = interval;
                }
            } else {
                g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].interval = 0;
            }

            memset(&g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].xfer, 0, sizeof(usbfs_xfer_t));
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].urb = urb;
            if (g_usbfs_hcd[bus->hcd.hcd_id].pipe_list[type] == NULL) {
                g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].prev = NULL;
                g_usbfs_hcd[bus->hcd.hcd_id].pipe_list[type] = &g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
            } else {
                struct usbfs_pipe *ppipe = g_usbfs_hcd[bus->hcd.hcd_id].pipe_list[type];
                while (ppipe->next) {
                    ppipe = ppipe->next;
                }
                g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].prev = ppipe;
                ppipe->next = &g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
            }
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx].next = NULL;
            return &g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[chidx];
        }
    }

    return NULL;
}

static void usbfs_pipe_free(struct usbh_bus *bus, struct usbfs_pipe *pipe, uint8_t type)
{
    usbfs_xfer_t *xfer = &pipe->xfer;

    size_t flags = usb_osal_enter_critical_section();

    if (xfer->xfer_state != XFER_STATE_BUSY) {
        if (pipe->prev) {
            pipe->prev->next = pipe->next;
        } else {
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_list[type] = pipe->next;
        }

        if (pipe->next) {
            pipe->next->prev = pipe->prev;
        }

        pipe->used = false;
    } else {
        pipe->killed = true;
    }

    usb_osal_leave_critical_section(flags);
}

static usbfs_xfer_t **usbfs_xfer_process(struct usbh_bus *bus, usbfs_xfer_t **last_xfer, uint8_t type)
{
    struct usbfs_pipe *pipe = g_usbfs_hcd[bus->hcd.hcd_id].pipe_list[type];

    while (pipe) {
        usbfs_xfer_t *xfer = &pipe->xfer;
        if (xfer->xfer_state != XFER_STATE_COMP) {
            struct usbh_urb *urb = pipe->urb;
            if (urb->hport->speed != g_usbfs_hcd[bus->hcd.hcd_id].speed) {
                xfer->pre = 1;
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
            } else if (pipe->interval == 0 || g_usbfs_hcd[bus->hcd.hcd_id].tick - pipe->tick >= pipe->interval) {
                pipe->tick = g_usbfs_hcd[bus->hcd.hcd_id].tick;
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

static void usbfs_transfer_start(struct usbh_bus *bus)
{
    if ((USBFSH->HOST_EP_PID != 0) || (USBFSH->INT_FG & USBFS_UIF_TRANSFER) || !(USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH)) {
        return;
    }

    if (g_usbfs_hcd[bus->hcd.hcd_id].sof_act || !g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer) {
        g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer = NULL;
        usbfs_xfer_t **last_xfer = &g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer;

        if (g_usbfs_hcd[bus->hcd.hcd_id].sof_act) {
            g_usbfs_hcd[bus->hcd.hcd_id].sof_act = 0;
            last_xfer = usbfs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_ISOCHRONOUS);
            last_xfer = usbfs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_INTERRUPT);
        }

        last_xfer = usbfs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_CONTROL);
        last_xfer = usbfs_xfer_process(bus, last_xfer, USB_ENDPOINT_TYPE_BULK);
        *last_xfer = NULL;
    }

    if (g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer) {
        usbfs_xfer_t *xfer = g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer;
        uint8_t type = USB_GET_ENDPOINT_TYPE(xfer->pipe->type);

        USBFSH->DEV_ADDR = xfer->dev_addr;
        if (xfer->pre || g_usbfs_hcd[bus->hcd.hcd_id].speed == USB_SPEED_LOW) {
            USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
            USBFSH->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
        } else {
            USBFSH->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
            USBFSH->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
        }

        if (xfer->token == USB_PID_IN) {
            USBFSH->HOST_RX_DMA = (uint32_t)xfer->buffer;
            USBFSH->HOST_RX_CTRL = (xfer->toggle == ENDP_TOG_DATA1 ? USBFS_UH_R_TOG : 0) |
                                   (type == USB_ENDPOINT_TYPE_ISOCHRONOUS ? USBFS_UH_R_RES : 0);
            USBFSH->HOST_EP_PID = (xfer->token << 4) | xfer->endp;
        } else {
            USBFSH->HOST_TX_LEN = xfer->length;
            USBFSH->HOST_TX_DMA = (uint32_t)xfer->buffer;
            USBFSH->HOST_TX_CTRL = (xfer->toggle == ENDP_TOG_DATA1 ? USBFS_UH_T_TOG : 0) |
                                   (type == USB_ENDPOINT_TYPE_ISOCHRONOUS ? USBFS_UH_T_RES : 0);
            USBFSH->HOST_EP_PID = (xfer->token << 4) | xfer->endp;
        }
    }
}

static void usbfs_transfer_complete(struct usbh_bus *bus, uint8_t recv_pid, size_t recv_len)
{
    static const uint8_t tog_pid[] = { USB_PID_DATA0, USB_PID_DATA1, USB_PID_DATA2, USB_PID_MDATA };

    usbfs_xfer_t *xfer = g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer;
    struct usbfs_pipe *pipe = xfer->pipe;

    xfer->xfer_state = XFER_STATE_IDLE;
    if (pipe->killed) {
        usbfs_pipe_free(bus, pipe, pipe->type);
        return;
    }

    struct usbh_urb *urb = pipe->urb;
    if (recv_pid == USB_PID_ACK || recv_pid == USB_PID_NYET || recv_pid == tog_pid[xfer->toggle]) {
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
        if (xfer->endp != 0) {
            urb->errorcode = -USB_ERR_NAK;
            goto end;
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
        usbfs_pipe_free(bus, pipe, pipe->type);
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
    memset(&g_usbfs_hcd[bus->hcd.hcd_id], 0, sizeof(struct usbfs_hcd));
    usb_hc_low_level_init(bus);

    for (uint8_t i = 0; i < sizeof(g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool) / sizeof(struct usbfs_pipe); i++) {
        g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem = usb_osal_sem_create(0);
        USB_ASSERT(g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem != NULL);
    }

    USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE;
    while (!(USBFSH->BASE_CTRL & USBFS_UC_HOST_MODE))
        ;
    USBFSH->HOST_CTRL = 0;
    USBFSH->DEV_ADDR = 0;
    USBFSH->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;
    USBFSH->HOST_SETUP = USBFS_UH_SOF_EN;

    USBFSH->HOST_RX_CTRL = 0;
    USBFSH->HOST_TX_CTRL = 0;
    USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;

    USBFSH->INT_FG = 0xFF;
    USBFSH->INT_EN = USBFS_UIE_HST_SOF | USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    return 0;
}

int usb_hc_deinit(struct usbh_bus *bus)
{
    USBFSH->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    usb_osal_msleep(1);
    USBFSH->BASE_CTRL = 0;

    for (uint8_t i = 0; i < sizeof(g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool) / sizeof(struct usbfs_pipe); i++) {
        if (g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem) {
            usb_osal_sem_delete(g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem);
            g_usbfs_hcd[bus->hcd.hcd_id].pipe_pool[i].waitsem = NULL;
        }
    }

    usb_hc_low_level_deinit(bus);
    return 0;
}

uint16_t usbh_get_frame_number(struct usbh_bus *bus)
{
    return 0;
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
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        g_usbfs_hcd[bus->hcd.hcd_id].port_csc = 0;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
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
                        USBFSH->INT_EN = 0;
                        USBFSH->HOST_CTRL = USBFS_UH_BUS_RESET;
                        usb_osal_msleep(15);
                        USBFSH->HOST_CTRL = 0;
                        usb_osal_msleep(2);
                        if (USBFSH->MIS_ST & USBFS_UMS_DM_LEVEL) {
                            USBFSH->HOST_CTRL = USBFS_UH_LOW_SPEED;
                        }
                        USBFSH->INT_FG = 0xFF;
                        USBFSH->INT_EN = USBFS_UIE_HST_SOF | USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
                        USBFSH->HOST_CTRL |= USBFS_UH_PORT_EN;
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
                if (g_usbfs_hcd[bus->hcd.hcd_id].port_csc)
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);

                port_status = USBFSH->MIS_ST;
                status |= (1 << HUB_PORT_FEATURE_POWER);

                if (port_status & USBFS_UMS_DEV_ATTACH)
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                if (USBFSH->HOST_CTRL & USBFS_UH_PORT_EN)
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);
                if (port_status & USBFS_UMS_SUSPEND)
                    status |= (1 << HUB_PORT_FEATURE_SUSPEND);
                if (port_status & USBFS_UMS_BUS_RESET)
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                if (port_status & USBFS_UMS_DM_LEVEL) {
                    status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    g_usbfs_hcd[bus->hcd.hcd_id].speed = USB_SPEED_LOW;
                } else {
                    g_usbfs_hcd[bus->hcd.hcd_id].speed = USB_SPEED_FULL;
                }

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
    struct usbfs_pipe *pipe = usbfs_pipe_alloc(bus, urb, urb->ep);
    if (!pipe) {
        usb_osal_leave_critical_section(flags);
        return -USB_ERR_NOMEM;
    }

    urb->hcpriv = pipe;
    urb->errorcode = -USB_ERR_BUSY;
    urb->actual_length = 0;
    usbfs_transfer_start(bus);
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
        usbfs_pipe_free(bus, pipe, pipe->type);
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

    struct usbfs_pipe *pipe = urb->hcpriv;
    if (!pipe) {
        usb_osal_leave_critical_section(flags);
        return -USB_ERR_INVAL;
    }

    urb->errorcode = -USB_ERR_SHUTDOWN;

    usb_osal_leave_critical_section(flags);

    if (urb->timeout) {
        usb_osal_sem_give(pipe->waitsem);
    } else {
        usbfs_pipe_free(urb->hport->bus, pipe, USB_GET_ENDPOINT_TYPE(urb->ep->bmAttributes));
    }

    if (urb->complete) {
        urb->complete(urb->arg, urb->errorcode);
    }

    return 0;
}

void USBH_IRQHandler(uint8_t busid)
{
    struct usbh_bus *bus = &g_usbhost_bus[busid];

    /* Get the interrupt flag */
    uint8_t int_flag = USBFSH->INT_FG;

    if (int_flag & USBFS_UIF_HST_SOF) {
        g_usbfs_hcd[bus->hcd.hcd_id].tick++;
        g_usbfs_hcd[bus->hcd.hcd_id].sof_act = true;
        usbfs_transfer_start(bus);
        USBFSH->INT_FG = USBFS_UIF_HST_SOF;
    } else if (int_flag & USBFS_UIF_TRANSFER) {
        USBFSH->HOST_EP_PID = 0x00;
        usbfs_transfer_complete(bus, USBFSH->INT_ST & 0x0F, USBFSH->RX_LEN);
        g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer = g_usbfs_hcd[bus->hcd.hcd_id].curr_xfer->next;
        USBFSH->INT_FG = USBFS_UIF_TRANSFER;
        usbfs_transfer_start(bus);
    } else if (int_flag & USBFS_UIF_DETECT) {
        g_usbfs_hcd[bus->hcd.hcd_id].port_csc = 1;
        bus->hcd.roothub.int_buffer[0] = (1 << 1);
        usbh_hub_thread_wakeup(&bus->hcd.roothub);
        USBFSH->INT_FG = USBFS_UIF_DETECT;
    } else {
        USBFSH->INT_FG = int_flag;
    }
}
