#include "usbh_core.h"
#include "stm32f4xx_hal.h"

#define USBH_PID_SETUP 0U
#define USBH_PID_DATA  1U

#define USB_SNOPSYS_RETRY_COUNT 5

#ifndef CONFIG_USBHOST_CHANNELS
#define CONFIG_USBHOST_CHANNELS 12 /* Number of host channels */
#endif

/* This structure retains the state of one host channel.  NOTE: Since there
 * is only one channel operation active at a time, some of the fields in
 * in the structure could be moved in struct stm32_ubhost_s to achieve
 * some memory savings.
 */

struct usb_synopsys_chan {
    usb_osal_sem_t waitsem; /* Channel wait semaphore */
    uint8_t chidx;          /* Channel index */
    bool inuse;             /* True: This channel is "in use" */
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback; /* Transfer complete callback */
    void *arg;                       /* Argument that accompanies the callback */
#endif
};

/* A channel represents on uni-directional endpoint.  So, in the case of the
 * bi-directional, control endpoint, there must be two channels to represent
 * the endpoint.
 */

struct usb_synopsys_ctrlinfo {
    uint8_t inndx;  /* EP0 IN control channel index */
    uint8_t outndx; /* EP0 OUT control channel index */
};

struct usb_synopsys_priv {
    HCD_HandleTypeDef *handle;
    volatile bool connected; /* Connected to device */
    volatile bool pscwait;   /* True: Thread is waiting for a port event */
    usb_osal_sem_t exclsem;  /* Support mutually exclusive access */
    struct usb_synopsys_chan chan[CONFIG_USBHOST_CHANNELS];
} g_usbhost;

/****************************************************************************
 * Name: usb_synopsys_chan_alloc
 *
 * Description:
 *   Allocate a channel.
 *
 ****************************************************************************/

static int usb_synopsys_chan_alloc(struct usb_synopsys_priv *priv)
{
    int chidx;

    /* Search the table of channels */

    for (chidx = 0; chidx < CONFIG_USBHOST_CHANNELS; chidx++) {
        /* Is this channel available? */
        if (!priv->chan[chidx].inuse) {
            /* Yes... make it "in use" and return the index */

            priv->chan[chidx].inuse = true;
            return chidx;
        }
    }

    /* All of the channels are "in-use" */

    return -EBUSY;
}

/****************************************************************************
 * Name: usb_synopsys_chan_free
 *
 * Description:
 *   Free a previoiusly allocated channel.
 *
 ****************************************************************************/

static void usb_synopsys_chan_free(struct usb_synopsys_priv *priv, int chidx)
{
    /* Halt the channel */
    HAL_HCD_HC_Halt(priv->handle, chidx);
    /* Mark the channel available */

    priv->chan[chidx].inuse = false;
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_init(void)
{
    memset(&g_usbhost, 0, sizeof(struct usb_synopsys_priv));
#ifdef CONFIG_USB_HS
    extern HCD_HandleTypeDef hhcd_USB_OTG_HS;
    g_usbhost.handle = &hhcd_USB_OTG_HS;
#else
    extern HCD_HandleTypeDef hhcd_USB_OTG_FS;
    g_usbhost.handle = &hhcd_USB_OTG_FS;
#endif

    g_usbhost.exclsem = usb_osal_mutex_create();

    for (uint8_t i = 0; i < CONFIG_USBHOST_CHANNELS; i++) {
        struct usb_synopsys_chan *chan = &g_usbhost.chan[i];

        chan->chidx = i;

        /* The waitsem semaphore is used for signaling and, hence, should not
       * have priority inheritance enabled.
       */
        chan->waitsem = usb_osal_sem_create(0);
    }
    usb_hc_low_level_init();
    HAL_HCD_Start(g_usbhost.handle);
    return 0;
}

int usbh_reset_port(const uint8_t port)
{
    HAL_HCD_ResetPort(g_usbhost.handle);
    return 0;
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    if (HAL_HCD_GetCurrentSpeed(g_usbhost.handle) == 1) {
        return USB_SPEED_FULL;
    } else if (HAL_HCD_GetCurrentSpeed(g_usbhost.handle) == 2)
        return USB_SPEED_LOW;
    else
        return USB_SPEED_HIGH;
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    struct usb_synopsys_ctrlinfo *ep0info;
    int ret;

    ep0info = (struct usb_synopsys_ctrlinfo *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    if (speed == USB_SPEED_FULL) {
        speed = HCD_SPEED_FULL;
    } else if (speed == USB_SPEED_LOW) {
        speed = HCD_SPEED_LOW;
    }

    ret = HAL_HCD_HC_Init(g_usbhost.handle, ep0info->outndx, 0x00, dev_addr, speed, USB_ENDPOINT_TYPE_CONTROL, ep_mps);
    ret = HAL_HCD_HC_Init(g_usbhost.handle, ep0info->inndx, 0x80, dev_addr, speed, USB_ENDPOINT_TYPE_CONTROL, ep_mps);
    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct usb_synopsys_ctrlinfo *ep0;
    struct usbh_hubport *hport;
    int chidx;
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    hport = ep_cfg->hport;

    if ((ep_cfg->ep_type & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL) {
        ep0 = usb_malloc(sizeof(struct usb_synopsys_ctrlinfo));

        ep0->outndx = usb_synopsys_chan_alloc(&g_usbhost);
        ep0->inndx = usb_synopsys_chan_alloc(&g_usbhost);

        HAL_HCD_HC_Init(g_usbhost.handle, ep0->outndx, 0x00, hport->dev_addr, hport->speed, USB_ENDPOINT_TYPE_CONTROL, ep_cfg->ep_mps);
        HAL_HCD_HC_Init(g_usbhost.handle, ep0->inndx, 0x80, hport->dev_addr, hport->speed, USB_ENDPOINT_TYPE_CONTROL, ep_cfg->ep_mps);

        *ep = (usbh_epinfo_t)ep0;

    } else {
        chidx = usb_synopsys_chan_alloc(&g_usbhost);
        HAL_HCD_HC_Init(g_usbhost.handle, chidx, ep_cfg->ep_addr, hport->dev_addr, hport->speed, ep_cfg->ep_type, ep_cfg->ep_mps);
        if ((ep_cfg->ep_type & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK) {
            if (g_usbhost.handle->hc[chidx].ep_is_in) {
                g_usbhost.handle->hc[chidx].toggle_in = 0;
            } else {
                g_usbhost.handle->hc[chidx].toggle_out = 0;
            }
        }
        *ep = (usbh_epinfo_t)chidx;
    }
    usb_osal_mutex_give(g_usbhost.exclsem);
    return 0;
}
int usbh_ep_free(usbh_epinfo_t ep)
{
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    uint8_t retries;
    int ret;
    struct usb_synopsys_ctrlinfo *ep0info = (struct usb_synopsys_ctrlinfo *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }
    for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
        ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                       ep0info->outndx,  /* Pipe index       */
                                       0,                /* Direction : OUT  */
                                       0,                /* EP type          */
                                       USBH_PID_SETUP,   /* Type Data        */
                                       (uint8_t *)setup, /* data buffer      */
                                       8,                /* data length      */
                                       0);               /* do ping (HS Only)*/

        ret = usb_osal_sem_take(g_usbhost.chan[ep0info->outndx].waitsem);
        if (ret < 0) {
            goto errout_with_mutex;
        }
        usb_osal_msleep(5);
        if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_NOTREADY) {
            ret = -EAGAIN;
            continue;
        } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_STALL) {
            ret = -EPERM;
            goto errout_with_mutex;
        } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_ERROR) {
            ret = -EIO;
            goto errout_with_mutex;
        } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_DONE) {
            break;
        }
    }
    if (retries >= USB_SNOPSYS_RETRY_COUNT) {
        ret = -ETIMEDOUT;
        goto errout_with_mutex;
    }

    if (setup->wLength && buffer) {
        if (setup->bmRequestType & 0x80) {
            for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
                ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                               ep0info->inndx, /* Pipe index       */
                                               1,              /* Direction : IN  */
                                               0,              /* EP type          */
                                               USBH_PID_DATA,  /* Type Data        */
                                               buffer,         /* data buffer      */
                                               setup->wLength, /* data length      */
                                               0);             /* do ping (HS Only)*/
                usb_osal_sem_take(g_usbhost.chan[ep0info->inndx].waitsem);
                if (ret < 0) {
                    goto errout_with_mutex;
                }
                usb_osal_msleep(5);
                if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_NOTREADY) {
                    ret = -EAGAIN;
                    continue;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_STALL) {
                    ret = -EPERM;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_ERROR) {
                    ret = -EIO;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_DONE) {
                    break;
                }
            }
            if (retries >= USB_SNOPSYS_RETRY_COUNT) {
                ret = -ETIMEDOUT;
                goto errout_with_mutex;
            }
            for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
                ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                               ep0info->outndx, /* Pipe index       */
                                               0,               /* Direction : OUT  */
                                               0,               /* EP type          */
                                               USBH_PID_DATA,   /* Type Data        */
                                               NULL,            /* data buffer      */
                                               0,               /* data length      */
                                               0);              /* do ping (HS Only)*/
                usb_osal_sem_take(g_usbhost.chan[ep0info->outndx].waitsem);
                if (ret < 0) {
                    goto errout_with_mutex;
                }
                usb_osal_msleep(5);
                if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_NOTREADY) {
                    ret = -EAGAIN;
                    continue;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_STALL) {
                    ret = -EPERM;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_ERROR) {
                    ret = -EIO;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_DONE) {
                    break;
                }
            }
            if (retries >= USB_SNOPSYS_RETRY_COUNT) {
                ret = -ETIMEDOUT;
                goto errout_with_mutex;
            }
        } else {
            for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
                ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                               ep0info->outndx, /* Pipe index       */
                                               0,               /* Direction : OUT  */
                                               0,               /* EP type          */
                                               USBH_PID_DATA,   /* Type Data        */
                                               buffer,          /* data buffer      */
                                               setup->wLength,  /* data length      */
                                               0);              /* do ping (HS Only)*/

                usb_osal_sem_take(g_usbhost.chan[ep0info->outndx].waitsem);
                if (ret < 0) {
                    goto errout_with_mutex;
                }
                usb_osal_msleep(5);
                if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_NOTREADY) {
                    ret = -EAGAIN;
                    continue;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_STALL) {
                    ret = -EPERM;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_ERROR) {
                    ret = -EIO;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->outndx) == URB_DONE) {
                    break;
                }
            }

            if (retries >= USB_SNOPSYS_RETRY_COUNT) {
                ret = -ETIMEDOUT;
                goto errout_with_mutex;
            }
            for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
                ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                               ep0info->inndx, /* Pipe index       */
                                               1,              /* Direction : IN  */
                                               0,              /* EP type          */
                                               USBH_PID_DATA,  /* Type Data        */
                                               NULL,           /* data buffer      */
                                               0,              /* data length      */
                                               0);             /* do ping (HS Only)*/
                usb_osal_sem_take(g_usbhost.chan[ep0info->inndx].waitsem);
                if (ret < 0) {
                    goto errout_with_mutex;
                }
                usb_osal_msleep(5);
                if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_NOTREADY) {
                    ret = -EAGAIN;
                    continue;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_STALL) {
                    ret = -EPERM;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_ERROR) {
                    ret = -EIO;
                    goto errout_with_mutex;
                } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_DONE) {
                    break;
                }
            }

            if (retries >= USB_SNOPSYS_RETRY_COUNT) {
                ret = -ETIMEDOUT;
                goto errout_with_mutex;
            }
        }
    } else {
        for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           ep0info->inndx, /* Pipe index       */
                                           1,              /* Direction : IN  */
                                           0,              /* EP type          */
                                           USBH_PID_DATA,  /* Type Data        */
                                           NULL,           /* data buffer      */
                                           0,              /* data length      */
                                           0);             /* do ping (HS Only)*/
            usb_osal_sem_take(g_usbhost.chan[ep0info->inndx].waitsem);
            if (ret < 0) {
                goto errout_with_mutex;
            }
            usb_osal_msleep(10);
            if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_NOTREADY) {
                ret = -EAGAIN;
                continue;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_STALL) {
                ret = -EPERM;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_ERROR) {
                ret = -EIO;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, ep0info->inndx) == URB_DONE) {
                break;
            }
        }
        if (retries >= USB_SNOPSYS_RETRY_COUNT) {
            ret = -ETIMEDOUT;
            goto errout_with_mutex;
        }
    }
    usb_osal_mutex_give(g_usbhost.exclsem);
    return 0;
errout_with_mutex:
    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}
int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen)
{
    uint8_t retries;
    int ret;
    uint8_t chidx = (uint8_t)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }
    if (g_usbhost.handle->hc[chidx].ep_is_in) {
        for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           chidx,         /* Pipe index       */
                                           1,             /* Direction : IN  */
                                           2,             /* EP type          */
                                           USBH_PID_DATA, /* Type Data        */
                                           buffer,        /* data buffer      */
                                           buflen,        /* data length      */
                                           0);            /* do ping (HS Only)*/

            usb_osal_sem_take(g_usbhost.chan[chidx].waitsem);
            if (ret < 0) {
                goto errout_with_mutex;
            }
            usb_osal_msleep(10);
            if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_NOTREADY) {
                ret = -EAGAIN;
                continue;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_STALL) {
                ret = -EPERM;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_ERROR) {
                ret = -EIO;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_DONE) {
                break;
            }
        }
        if (retries >= USB_SNOPSYS_RETRY_COUNT) {
            ret = -ETIMEDOUT;
            goto errout_with_mutex;
        }
    } else {
        for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           chidx,         /* Pipe index       */
                                           0,             /* Direction : OUT  */
                                           2,             /* EP type          */
                                           USBH_PID_DATA, /* Type Data        */
                                           buffer,        /* data buffer      */
                                           buflen,        /* data length      */
                                           1);            /* do ping (HS Only)*/
            usb_osal_sem_take(g_usbhost.chan[chidx].waitsem);
            if (ret < 0) {
                goto errout_with_mutex;
            }
            usb_osal_msleep(10);
            if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_NOTREADY) {
                ret = -EAGAIN;
                continue;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_STALL) {
                ret = -EPERM;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_ERROR) {
                ret = -EIO;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_DONE) {
                break;
            }
        }
        if (retries >= USB_SNOPSYS_RETRY_COUNT) {
            ret = -ETIMEDOUT;
            goto errout_with_mutex;
        }
    }
    usb_osal_mutex_give(g_usbhost.exclsem);
    return HAL_HCD_HC_GetXferCount(g_usbhost.handle, chidx);
errout_with_mutex:
    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen)
{
    uint8_t retries;
    int ret;
    uint8_t chidx = (uint8_t)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }
    if (g_usbhost.handle->hc[chidx].ep_is_in) {
        for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           chidx,         /* Pipe index       */
                                           1,             /* Direction : IN  */
                                           3,             /* EP type          */
                                           USBH_PID_DATA, /* Type Data        */
                                           buffer,        /* data buffer      */
                                           buflen,        /* data length      */
                                           0);            /* do ping (HS Only)*/

            usb_osal_sem_take(g_usbhost.chan[chidx].waitsem);
            if (ret < 0) {
                goto errout_with_mutex;
            }
            usb_osal_msleep(10);
            if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_NOTREADY) {
                ret = -EAGAIN;
                continue;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_STALL) {
                ret = -EPERM;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_ERROR) {
                ret = -EIO;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_DONE) {
                break;
            }
        }
        if (retries >= USB_SNOPSYS_RETRY_COUNT) {
            ret = -ETIMEDOUT;
            goto errout_with_mutex;
        }
    } else {
        for (retries = 0; retries < USB_SNOPSYS_RETRY_COUNT; retries++) {
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           chidx,         /* Pipe index       */
                                           0,             /* Direction : OUT  */
                                           3,             /* EP type          */
                                           USBH_PID_DATA, /* Type Data        */
                                           buffer,        /* data buffer      */
                                           buflen,        /* data length      */
                                           1);            /* do ping (HS Only)*/
            usb_osal_sem_take(g_usbhost.chan[chidx].waitsem);
            if (ret < 0) {
                goto errout_with_mutex;
            }
            usb_osal_msleep(10);
            if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_NOTREADY) {
                ret = -EAGAIN;
                continue;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_STALL) {
                ret = -EPERM;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_ERROR) {
                ret = -EIO;
                goto errout_with_mutex;
            } else if (HAL_HCD_HC_GetURBState(g_usbhost.handle, chidx) == URB_DONE) {
                break;
            }
        }
        if (retries >= USB_SNOPSYS_RETRY_COUNT) {
            ret = -ETIMEDOUT;
            goto errout_with_mutex;
        }
    }
    usb_osal_mutex_give(g_usbhost.exclsem);
    return HAL_HCD_HC_GetXferCount(g_usbhost.handle, chidx);
errout_with_mutex:
    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    return 0;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    return 0;
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    return 0;
}

void HAL_Delay(uint32_t Delay)
{
    usb_osal_msleep(Delay);
}

void HAL_HCD_Connect_Callback(HCD_HandleTypeDef *hhcd)
{
    g_usbhost.connected = true;
    extern void usbh_event_notify_handler(uint8_t event, uint8_t rhport);
    usbh_event_notify_handler(USBH_EVENT_ATTACHED, 1);
}

/**
  * @brief  SOF callback.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_Disconnect_Callback(HCD_HandleTypeDef *hhcd)
{
    g_usbhost.connected = false;
    extern void usbh_event_notify_handler(uint8_t event, uint8_t rhport);
    usbh_event_notify_handler(USBH_EVENT_REMOVED, 1);
}

void HAL_HCD_HC_NotifyURBChange_Callback(HCD_HandleTypeDef *hhcd, uint8_t chnum, HCD_URBStateTypeDef urb_state)
{
    usb_osal_sem_give(g_usbhost.chan[chnum].waitsem);
}