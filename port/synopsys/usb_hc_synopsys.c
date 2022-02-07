#include "usbh_core.h"
#include "stm32f4xx_hal.h"

#if defined(CONFIG_USB_HS) || defined(CONFIG_USB_HS_IN_FULL)
HCD_HandleTypeDef hhcd_USB_OTG_HS;
#ifndef USBH_IRQHandler
#define USBH_IRQHandler OTG_HS_IRQHandler
#endif
#else
HCD_HandleTypeDef hhcd_USB_OTG_FS;
#ifndef USBH_IRQHandler
#define USBH_IRQHandler OTG_FS_IRQHandler
#endif
#endif

#define USBH_PID_SETUP 0U
#define USBH_PID_DATA  1U

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
    volatile int result;    /* The result of the transfer */
    bool inuse;             /* True: This channel is "in use" */
    bool in;                /* True: IN endpoint */
    volatile bool waiter;   /* True: Thread is waiting for a channel event */
    volatile uint16_t xfrd; /* Bytes transferred (at end of transfer) */
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

/****************************************************************************
 * Name: usb_synopsys_chan_freeall
 *
 * Description:
 *   Free all channels.
 *
 ****************************************************************************/

static inline void usb_synopsys_chan_freeall(struct usb_synopsys_priv *priv)
{
    uint8_t chidx;

    /* Free all host channels */

    for (chidx = 2; chidx < CONFIG_USBHOST_CHANNELS; chidx++) {
        usb_synopsys_chan_free(priv, chidx);
    }
}

/****************************************************************************
 * Name: usb_synopsys_chan_waitsetup
 *
 * Description:
 *   Set the request for the transfer complete event well BEFORE enabling
 *   the transfer (as soon as we are absolutely committed to the transfer).
 *   We do this to minimize race conditions.  This logic would have to be
 *   expanded if we want to have more than one packet in flight at a time!
 *
 * Assumptions:
 *   Called from a normal thread context BEFORE the transfer has been
 *   started.
 *
 ****************************************************************************/

static int usb_synopsys_chan_waitsetup(struct usb_synopsys_priv *priv,
                                       struct usb_synopsys_chan *chan)
{
    uint32_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();

    /* Is the device still connected? */

    if (priv->connected) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */

        chan->waiter = true;
#ifdef CONFIG_USBHOST_ASYNCH
        chan->callback = NULL;
        chan->arg = NULL;
#endif
        ret = 0;
    }
    usb_osal_leave_critical_section(flags);
    return ret;
}

/****************************************************************************
 * Name: usb_synopsys_chan_asynchsetup
 *
 * Description:
 *   Set the request for the transfer complete event well BEFORE enabling
 *   the transfer (as soon as we are absolutely committed to the to avoid
 *   transfer).  We do this to minimize race conditions.  This logic would
 *   have to be expanded if we want to have more than one packet in flight
 *   at a time!
 *
 * Assumptions:
 *   Might be called from the level of an interrupt handler
 *
 ****************************************************************************/

#ifdef CONFIG_USBHOST_ASYNCH
static int usb_synopsys_chan_asynchsetup(struct usb_synopsys_priv *priv,
                                         struct usb_synopsys_chan *chan,
                                         usbh_asynch_callback_t callback, void *arg)
{
    uint32_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();
    /* Is the device still connected? */

    if (priv->connected) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */

        chan->waiter = false;
        chan->callback = callback;
        chan->arg = arg;
        ret = 0;
    }

    usb_osal_leave_critical_section(flags);
    return ret;
}
#endif

/****************************************************************************
 * Name: stm32_chan_wait
 *
 * Description:
 *   Wait for a transfer on a channel to complete.
 *
 * Assumptions:
 *   Called from a normal thread context
 *
 ****************************************************************************/

static int usb_synopsys_chan_wait(struct usb_synopsys_priv *priv, struct usb_synopsys_chan *chan)
{
    int ret;

    /* Loop, testing for an end of transfer condition.  The channel 'result'
   * was set to EBUSY and 'waiter' was set to true before the transfer;
   * 'waiter' will be set to false and 'result' will be set appropriately
   * when the transfer is completed.
   */

    while (chan->waiter) {
        usb_osal_sem_take(chan->waitsem);
    }

    /* The transfer is complete re-enable interrupts and return the result */
    ret = chan->result;
    return ret;
}

/****************************************************************************
 * Name: stm32_chan_wakeup
 *
 * Description:
 *   A channel transfer has completed... wakeup any threads waiting for the
 *   transfer to complete.
 *
 * Assumptions:
 *   This function is called from the transfer complete interrupt handler for
 *   the channel.  Interrupts are disabled.
 *
 ****************************************************************************/

static void usb_synopsys_chan_wakeup(struct usb_synopsys_priv *priv, struct usb_synopsys_chan *chan)
{
    usbh_asynch_callback_t callback;
    void *arg;
    int nbytes;

    /* Is the transfer complete? */

    if (chan->result != -EBUSY) {
        /* Is there a thread waiting for this transfer to complete? */

        if (chan->waiter) {
            /* Wake'em up! */
            chan->waiter = false;
            usb_osal_sem_give(chan->waitsem);
        }
#ifdef CONFIG_USBHOST_ASYNCH
        /* No.. is an asynchronous callback expected when the transfer
       * completes?
       */
        else if (chan->callback) {
            /* Handle continuation of IN/OUT pipes */
            if (chan->in) {
                callback = chan->callback;
                arg = chan->arg;
                nbytes = chan->xfrd;
                chan->callback = NULL;
                chan->arg = NULL;
                if (chan->result < 0) {
                    nbytes = chan->result;
                }

                callback(arg, nbytes);

            } else {
                callback = chan->callback;
                arg = chan->arg;
                nbytes = chan->xfrd;
                chan->callback = NULL;
                chan->arg = NULL;
                if (chan->result < 0) {
                    nbytes = chan->result;
                }

                callback(arg, nbytes);
            }
        }
#endif
    }
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_init(void)
{
    memset(&g_usbhost, 0, sizeof(struct usb_synopsys_priv));
#if defined(CONFIG_USB_HS) || defined(CONFIG_USB_HS_IN_FULL)
    g_usbhost.handle = &hhcd_USB_OTG_HS;
    g_usbhost.handle->Instance = USB_OTG_HS;
#else
    g_usbhost.handle = &hhcd_USB_OTG_FS;
    g_usbhost.handle->Instance = USB_OTG_FS;
#endif

    g_usbhost.exclsem = usb_osal_mutex_create();

    for (uint8_t i = 0; i < CONFIG_USBHOST_CHANNELS; i++) {
        struct usb_synopsys_chan *chan = &g_usbhost.chan[i];

        /* The waitsem semaphore is used for signaling and, hence, should not
       * have priority inheritance enabled.
       */
        chan->waitsem = usb_osal_sem_create(0);
    }

    g_usbhost.handle->Init.Host_channels = CONFIG_USBHOST_CHANNELS;
    g_usbhost.handle->Init.speed = HCD_SPEED_FULL;
    g_usbhost.handle->Init.dma_enable = DISABLE;
    g_usbhost.handle->Init.phy_itface = USB_OTG_EMBEDDED_PHY;
    g_usbhost.handle->Init.Sof_enable = DISABLE;
    g_usbhost.handle->Init.low_power_enable = DISABLE;
    g_usbhost.handle->Init.vbus_sensing_enable = DISABLE;
    g_usbhost.handle->Init.use_external_vbus = DISABLE;
    if (HAL_HCD_Init(g_usbhost.handle) != HAL_OK) {
        return -1;
    }
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
    uint8_t speed;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    hport = ep_cfg->hport;

    if (hport->speed == USB_SPEED_FULL) {
        speed = HCD_SPEED_FULL;
    } else if (hport->speed == USB_SPEED_LOW) {
        speed = HCD_SPEED_LOW;
    }

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        ep0 = usb_malloc(sizeof(struct usb_synopsys_ctrlinfo));

        ep0->outndx = usb_synopsys_chan_alloc(&g_usbhost);
        ep0->inndx = usb_synopsys_chan_alloc(&g_usbhost);

        HAL_HCD_HC_Init(g_usbhost.handle, ep0->outndx, 0x00, hport->dev_addr, speed, USB_ENDPOINT_TYPE_CONTROL, ep_cfg->ep_mps);
        HAL_HCD_HC_Init(g_usbhost.handle, ep0->inndx, 0x80, hport->dev_addr, speed, USB_ENDPOINT_TYPE_CONTROL, ep_cfg->ep_mps);

        *ep = (usbh_epinfo_t)ep0;

    } else {
        chidx = usb_synopsys_chan_alloc(&g_usbhost);
        HAL_HCD_HC_Init(g_usbhost.handle, chidx, ep_cfg->ep_addr, hport->dev_addr, speed, ep_cfg->ep_type, ep_cfg->ep_mps);

        g_usbhost.handle->hc[chidx].toggle_in = 0;
        g_usbhost.handle->hc[chidx].toggle_out = 0;

        *ep = (usbh_epinfo_t)chidx;
    }
    usb_osal_mutex_give(g_usbhost.exclsem);
    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }
    if ((uintptr_t)ep < CONFIG_USBHOST_CHANNELS) {
        usb_synopsys_chan_free(&g_usbhost, (int)ep);
    } else {
        struct usb_synopsys_ctrlinfo *ep0 = (struct usb_synopsys_ctrlinfo *)ep;
        usb_synopsys_chan_free(&g_usbhost, ep0->inndx);
        usb_synopsys_chan_free(&g_usbhost, ep0->outndx);
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    int ret;
    uint32_t flags;
    struct usb_synopsys_chan *chan;
    struct usb_synopsys_priv *priv = &g_usbhost;
    struct usb_synopsys_ctrlinfo *ep0info = (struct usb_synopsys_ctrlinfo *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    flags = usb_osal_enter_critical_section();
    chan = &priv->chan[ep0info->outndx];
    usb_synopsys_chan_waitsetup(priv, chan);
    ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                   ep0info->outndx,  /* Pipe index       */
                                   0,                /* Direction : OUT  */
                                   0,                /* EP type          */
                                   USBH_PID_SETUP,   /* Type Data        */
                                   (uint8_t *)setup, /* data buffer      */
                                   8,                /* data length      */
                                   0);               /* do ping (HS Only)*/
    usb_osal_leave_critical_section(flags);

    ret = usb_synopsys_chan_wait(priv, chan);
    if (ret < 0) {
        goto errout_with_mutex;
    }
    if (setup->wLength && buffer) {
        if (setup->bmRequestType & 0x80) {
            flags = usb_osal_enter_critical_section();
            chan = &priv->chan[ep0info->inndx];
            usb_synopsys_chan_waitsetup(priv, chan);
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           ep0info->inndx, /* Pipe index       */
                                           1,              /* Direction : IN  */
                                           0,              /* EP type          */
                                           USBH_PID_DATA,  /* Type Data        */
                                           buffer,         /* data buffer      */
                                           setup->wLength, /* data length      */
                                           0);             /* do ping (HS Only)*/
            usb_osal_leave_critical_section(flags);
            ret = usb_synopsys_chan_wait(priv, chan);
            if (ret < 0) {
                goto errout_with_mutex;
            }

            flags = usb_osal_enter_critical_section();
            chan = &priv->chan[ep0info->outndx];
            usb_synopsys_chan_waitsetup(priv, chan);
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           ep0info->outndx, /* Pipe index       */
                                           0,               /* Direction : OUT  */
                                           0,               /* EP type          */
                                           USBH_PID_DATA,   /* Type Data        */
                                           NULL,            /* data buffer      */
                                           0,               /* data length      */
                                           0);              /* do ping (HS Only)*/
            usb_osal_leave_critical_section(flags);
            ret = usb_synopsys_chan_wait(priv, chan);
            if (ret < 0) {
                goto errout_with_mutex;
            }
        } else {
            flags = usb_osal_enter_critical_section();
            chan = &priv->chan[ep0info->outndx];
            usb_synopsys_chan_waitsetup(priv, chan);
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           ep0info->outndx, /* Pipe index       */
                                           0,               /* Direction : OUT  */
                                           0,               /* EP type          */
                                           USBH_PID_DATA,   /* Type Data        */
                                           buffer,          /* data buffer      */
                                           setup->wLength,  /* data length      */
                                           0);              /* do ping (HS Only)*/
            usb_osal_leave_critical_section(flags);
            ret = usb_synopsys_chan_wait(priv, chan);
            if (ret < 0) {
                goto errout_with_mutex;
            }

            flags = usb_osal_enter_critical_section();
            chan = &priv->chan[ep0info->inndx];
            usb_synopsys_chan_waitsetup(priv, chan);
            ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                           ep0info->inndx, /* Pipe index       */
                                           1,              /* Direction : IN  */
                                           0,              /* EP type          */
                                           USBH_PID_DATA,  /* Type Data        */
                                           NULL,           /* data buffer      */
                                           0,              /* data length      */
                                           0);             /* do ping (HS Only)*/
            usb_osal_leave_critical_section(flags);
            ret = usb_synopsys_chan_wait(priv, chan);
            if (ret < 0) {
                goto errout_with_mutex;
            }
        }
    } else {
        flags = usb_osal_enter_critical_section();
        chan = &priv->chan[ep0info->inndx];
        usb_synopsys_chan_waitsetup(priv, chan);
        ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                       ep0info->inndx, /* Pipe index       */
                                       1,              /* Direction : IN  */
                                       0,              /* EP type          */
                                       USBH_PID_DATA,  /* Type Data        */
                                       NULL,           /* data buffer      */
                                       0,              /* data length      */
                                       0);             /* do ping (HS Only)*/
        usb_osal_leave_critical_section(flags);
        ret = usb_synopsys_chan_wait(priv, chan);
        if (ret < 0) {
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
    int ret;
    uint32_t flags;
    struct usb_synopsys_chan *chan;
    struct usb_synopsys_priv *priv = &g_usbhost;
    uint8_t chidx = (uint8_t)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    flags = usb_osal_enter_critical_section();
    chan = &priv->chan[chidx];
    usb_synopsys_chan_waitsetup(priv, chan);
    ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                   chidx,                                        /* Pipe index       */
                                   g_usbhost.handle->hc[chidx].ep_is_in ? 1 : 0, /* Direction : IN  */
                                   2,                                            /* EP type          */
                                   USBH_PID_DATA,                                /* Type Data        */
                                   buffer,                                       /* data buffer      */
                                   buflen,                                       /* data length      */
                                   0);                                           /* do ping (HS Only)*/
    usb_osal_leave_critical_section(flags);
    ret = usb_synopsys_chan_wait(priv, chan);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return g_usbhost.handle->hc[chidx].ep_is_in ? HAL_HCD_HC_GetXferCount(g_usbhost.handle, chidx) : buflen;
errout_with_mutex:
    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen)
{
    uint32_t flags;
    int ret;
    struct usb_synopsys_chan *chan;
    struct usb_synopsys_priv *priv = &g_usbhost;
    uint8_t chidx = (uint8_t)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }
    flags = usb_osal_enter_critical_section();
    chan = &priv->chan[chidx];
    usb_synopsys_chan_waitsetup(priv, chan);
    ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                   chidx,                                        /* Pipe index       */
                                   g_usbhost.handle->hc[chidx].ep_is_in ? 1 : 0, /* Direction : IN  */
                                   3,                                            /* EP type          */
                                   USBH_PID_DATA,                                /* Type Data        */
                                   buffer,                                       /* data buffer      */
                                   buflen,                                       /* data length      */
                                   0);                                           /* do ping (HS Only)*/
    usb_osal_leave_critical_section(flags);
    ret = usb_synopsys_chan_wait(priv, chan);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return g_usbhost.handle->hc[chidx].ep_is_in ? HAL_HCD_HC_GetXferCount(g_usbhost.handle, chidx) : buflen;
errout_with_mutex:
    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;
    uint32_t flags;
    struct usb_synopsys_chan *chan;
    struct usb_synopsys_priv *priv = &g_usbhost;
    uint8_t chidx = (uint8_t)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    flags = usb_osal_enter_critical_section();
    chan = &priv->chan[chidx];
    usb_synopsys_chan_asynchsetup(priv, chan, callback, arg);
    ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                   chidx,                                        /* Pipe index       */
                                   g_usbhost.handle->hc[chidx].ep_is_in ? 1 : 0, /* Direction : IN  */
                                   2,                                            /* EP type          */
                                   USBH_PID_DATA,                                /* Type Data        */
                                   buffer,                                       /* data buffer      */
                                   buflen,                                       /* data length      */
                                   0);                                           /* do ping (HS Only)*/
    usb_osal_leave_critical_section(flags);

    usb_osal_mutex_give(g_usbhost.exclsem);

    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;
    uint32_t flags;
    struct usb_synopsys_chan *chan;
    struct usb_synopsys_priv *priv = &g_usbhost;
    uint8_t chidx = (uint8_t)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    flags = usb_osal_enter_critical_section();
    chan = &priv->chan[chidx];
    usb_synopsys_chan_asynchsetup(priv, chan, callback, arg);
    ret = HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                   chidx,                                        /* Pipe index       */
                                   g_usbhost.handle->hc[chidx].ep_is_in ? 1 : 0, /* Direction : IN  */
                                   3,                                            /* EP type          */
                                   USBH_PID_DATA,                                /* Type Data        */
                                   buffer,                                       /* data buffer      */
                                   buflen,                                       /* data length      */
                                   0);                                           /* do ping (HS Only)*/
    usb_osal_leave_critical_section(flags);

    usb_osal_mutex_give(g_usbhost.exclsem);

    return ret;
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    return 0;
}

void HAL_HCD_Connect_Callback(HCD_HandleTypeDef *hhcd)
{
    if (!g_usbhost.connected) {
        g_usbhost.connected = true;
        extern void usbh_event_notify_handler(uint8_t event, uint8_t rhport);
        usbh_event_notify_handler(USBH_EVENT_ATTACHED, 1);
    }
}

/**
  * @brief  SOF callback.
  * @param  hhcd: HCD handle
  * @retval None
  */
void HAL_HCD_Disconnect_Callback(HCD_HandleTypeDef *hhcd)
{
    if (g_usbhost.connected) {
        g_usbhost.connected = false;
        usb_synopsys_chan_freeall(&g_usbhost);
        extern void usbh_event_notify_handler(uint8_t event, uint8_t rhport);
        usbh_event_notify_handler(USBH_EVENT_REMOVED, 1);
    }
}

void HAL_HCD_HC_NotifyURBChange_Callback(HCD_HandleTypeDef *hhcd, uint8_t chnum, HCD_URBStateTypeDef urb_state)
{
    struct usb_synopsys_chan *chan;
    struct usb_synopsys_priv *priv = &g_usbhost;

    chan = &priv->chan[chnum];
    if (urb_state == URB_DONE || urb_state == URB_STALL || urb_state == URB_ERROR || urb_state == URB_NOTREADY) {
        if (urb_state != URB_NOTREADY) {
            if (urb_state == URB_ERROR) {
                chan->result = -EIO;
            } else if (urb_state == URB_STALL) {
                chan->result = -EPERM;
            } else if (urb_state == URB_DONE) {
                chan->result = 0;
            }
            chan->in = g_usbhost.handle->hc[chnum].ep_is_in;
            chan->xfrd = g_usbhost.handle->hc[chnum].ep_is_in ? HAL_HCD_HC_GetXferCount(g_usbhost.handle, chnum) : g_usbhost.handle->hc[chnum].xfer_len;
            usb_synopsys_chan_wakeup(priv, chan);
        } else {
            if (g_usbhost.handle->hc[chnum].ep_is_in == 0 && (g_usbhost.handle->hc[chnum].ep_type == USB_ENDPOINT_TYPE_CONTROL || g_usbhost.handle->hc[chnum].ep_type == USB_ENDPOINT_TYPE_BULK)) {
                HAL_HCD_HC_SubmitRequest(g_usbhost.handle,
                                         chnum,                                 /* Pipe index       */
                                         0,                                     /* Direction : OUT  */
                                         g_usbhost.handle->hc[chnum].ep_type,   /* EP type          */
                                         USBH_PID_DATA,                         /* Type Data        */
                                         g_usbhost.handle->hc[chnum].xfer_buff, /* data buffer      */
                                         g_usbhost.handle->hc[chnum].xfer_len,  /* data length      */
                                         0);                                    /* do ping (HS Only)*/
            }
        }
    }
}

void USBH_IRQHandler(void)
{
    /* USER CODE BEGIN OTG_HS_IRQn 0 */

    /* USER CODE END OTG_HS_IRQn 0 */
    HAL_HCD_IRQHandler(g_usbhost.handle);
    /* USER CODE BEGIN OTG_HS_IRQn 1 */

    /* USER CODE END OTG_HS_IRQn 1 */
}

void HAL_Delay(uint32_t Delay)
{
    usb_osal_msleep(Delay);
}
