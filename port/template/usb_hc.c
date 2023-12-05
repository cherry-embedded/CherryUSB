/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_xxx_reg.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler OTG_HS_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE (0x40040000UL)
#endif

struct dwc2_pipe {
    bool inuse;
    uint32_t xfrd;
    volatile bool waiter;
    usb_osal_sem_t waitsem;
    struct usbh_hubport *hport;
    struct usbh_urb *urb;
};

struct dwc2_hcd {
    struct dwc2_pipe pipe_pool[CONFIG_USBHOST_PIPE_NUM];
} g_dwc2_hcd;

static int dwc2_chan_alloc(void)
{
    int chidx;

    for (chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        if (!g_dwc2_hcd.pipe_pool[chidx].inuse) {
            g_dwc2_hcd.pipe_pool[chidx].inuse = true;
            return chidx;
        }
    }

    return -1;
}

static void dwc2_chan_free(struct dwc2_pipe *pipe)
{
    pipe->inuse = false;
}

static int usbh_reset_port(const uint8_t port)
{
    return 0;
}

static uint8_t usbh_get_port_speed(const uint8_t port)
{
    return USB_SPEED_UNKNOWN;
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_init(void)
{
    int ret;

    memset(&g_dwc2_hcd, 0, sizeof(struct dwc2_hcd));

    for (uint8_t chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        g_dwc2_hcd.pipe_pool[chidx].waitsem = usb_osal_sem_create(0);
    }

    usb_hc_low_level_init();

    return 0;
}

uint16_t usbh_get_frame_number(void)
{
    return 0;
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
    uint8_t nports;
    uint8_t port;
    uint32_t status;

    nports = CONFIG_USBHOST_MAX_RHPORTS;
    port = setup->wIndex;
    if (setup->bmRequestType & USB_REQUEST_RECIPIENT_DEVICE) {
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
    } else if (setup->bmRequestType & USB_REQUEST_RECIPIENT_OTHER) {
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
                        usbh_reset_port(port);
                        break;

                    default:
                        return -USB_ERR_NOTSUPP;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -USB_ERR_INVAL;
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
    struct dwc2_chan *chan;
    size_t flags;
    int ret = 0;
    int chidx;

    if (!urb || !urb->hport || !urb->ep) {
        return -USB_ERR_INVAL;
    }

    if (!urb->hport->connected) {
        return -USB_ERR_NOTCONN;
    }

    if (urb->errorcode == -USB_ERR_BUSY) {
        return -USB_ERR_BUSY;
    }

    flags = usb_osal_enter_critical_section();

    chidx = dwc2_chan_alloc();
    if (chidx == -1) {
        usb_osal_leave_critical_section(flags);
        return -USB_ERR_NOMEM;
    }

    chan = &g_dwc2_hcd.chan_pool[chidx];
    chan->chidx = chidx;
    chan->urb = urb;

    urb->hcpriv = chan;
    urb->errorcode = -USB_ERR_BUSY;
    urb->actual_length = 0;

    usb_osal_leave_critical_section(flags);

    switch (USB_GET_ENDPOINT_TYPE(urb->ep->bmAttributes)) {
        case USB_ENDPOINT_TYPE_CONTROL:
            break;
        case USB_ENDPOINT_TYPE_BULK:
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            break;
        default:
            break;
    }

    if (urb->timeout > 0) {
        /* wait until timeout or sem give */
        ret = usb_osal_sem_take(chan->waitsem, urb->timeout);
        if (ret < 0) {
            goto errout_timeout;
        }
        urb->timeout = 0;
        ret = urb->errorcode;
        /* we can free chan when waitsem is done */
        dwc2_chan_free(chan);
    }
    return ret;
errout_timeout:
    urb->timeout = 0;
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    struct dwc2_chan *chan;
    size_t flags;

    if (!urb || !urb->hcpriv) {
        return -USB_ERR_INVAL;
    }

    flags = usb_osal_enter_critical_section();

    chan = (struct dwc2_chan *)urb->hcpriv;

    chan->urb = NULL;
    urb->hcpriv = NULL;

    if (urb->timeout) {
        urb->timeout = 0;
        urb->errorcode = -USB_ERR_SHUTDOWN;
        usb_osal_sem_give(chan->waitsem);
    } else {
        dwc2_chan_free(chan);
    }

    usb_osal_leave_critical_section(flags);

    return 0;
}

static inline void dwc2_urb_waitup(struct usbh_urb *urb)
{
    struct dwc2_chan *chan;

    chan = (struct dwc2_chan *)urb->hcpriv;
    chan->urb = NULL;
    urb->hcpriv = NULL;

    if (urb->timeout) {
        urb->timeout = 0;
        usb_osal_sem_give(chan->waitsem);
    } else {
        dwc2_chan_free(chan);
    }

    if (urb->complete) {
        if (urb->errorcode < 0) {
            urb->complete(urb->arg, urb->errorcode);
        } else {
            urb->complete(urb->arg, urb->actual_length);
        }
    }
}

void USBH_IRQHandler(void)
{
}