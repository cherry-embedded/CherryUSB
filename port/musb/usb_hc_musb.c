#include "usbh_core.h"
#include "usb_musb_reg.h"

#ifdef USB_MUSB_SUNXI

#define SUNXI_SRAMC_BASE 0x01c00000
#define SUNXI_USB0_BASE  0x01c13000

#define USBC_REG_o_PHYCTL 0x0404

#ifndef USB_BASE
#define USB_BASE (SUNXI_USB0_BASE)
#endif

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USB_INT_Handler //use actual usb irq name instead
void USBH_IRQHandler(int, void *);
#endif

#define MUSB_IND_TXMAP_OFFSET      0x80
#define MUSB_IND_TXCSRL_OFFSET     0x82
#define MUSB_IND_TXCSRH_OFFSET     0x83
#define MUSB_IND_RXMAP_OFFSET      0x84
#define MUSB_IND_RXCSRL_OFFSET     0x86
#define MUSB_IND_RXCSRH_OFFSET     0x87
#define MUSB_IND_RXCOUNT_OFFSET    0x88
#define MUSB_IND_TXTYPE_OFFSET     0x8C
#define MUSB_IND_TXINTERVAL_OFFSET 0x8D
#define MUSB_IND_RXTYPE_OFFSET     0x8E
#define MUSB_IND_RXINTERVAL_OFFSET 0x8F
#define MUSB_FIFO_OFFSET           0x00
#else
#ifndef USBH_IRQHandler
#define USBH_IRQHandler USB_INT_Handler
#endif

#ifndef USB_BASE
#define USB_BASE (0x40086400UL)
#endif

#define MUSB_IND_TXMAP_OFFSET      0x10
#define MUSB_IND_TXCSRL_OFFSET     0x12
#define MUSB_IND_TXCSRH_OFFSET     0x13
#define MUSB_IND_RXMAP_OFFSET      0x14
#define MUSB_IND_RXCSRL_OFFSET     0x16
#define MUSB_IND_RXCSRH_OFFSET     0x17
#define MUSB_IND_RXCOUNT_OFFSET    0x18
#define MUSB_IND_TXTYPE_OFFSET     0x1A
#define MUSB_IND_TXINTERVAL_OFFSET 0x1B
#define MUSB_IND_RXTYPE_OFFSET     0x1C
#define MUSB_IND_RXINTERVAL_OFFSET 0x1D
#define MUSB_FIFO_OFFSET           0x20
#endif

#define USB ((USB0_Type *)USB_BASE)

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#define USB_TXMAPx_BASE       (USB_BASE + MUSB_IND_TXMAP_OFFSET)
#define USB_RXMAPx_BASE       (USB_BASE + MUSB_IND_RXMAP_OFFSET)
#define USB_TXCSRLx_BASE      (USB_BASE + MUSB_IND_TXCSRL_OFFSET)
#define USB_RXCSRLx_BASE      (USB_BASE + MUSB_IND_RXCSRL_OFFSET)
#define USB_TXCSRHx_BASE      (USB_BASE + MUSB_IND_TXCSRH_OFFSET)
#define USB_RXCSRHx_BASE      (USB_BASE + MUSB_IND_RXCSRH_OFFSET)
#define USB_RXCOUNTx_BASE     (USB_BASE + MUSB_IND_RXCOUNT_OFFSET)
#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)
#define USB_TXTYPEx_BASE      (USB_BASE + MUSB_IND_TXTYPE_OFFSET)
#define USB_RXTYPEx_BASE      (USB_BASE + MUSB_IND_RXTYPE_OFFSET)
#define USB_TXINTERVALx_BASE  (USB_BASE + MUSB_IND_TXINTERVAL_OFFSET)
#define USB_RXINTERVALx_BASE  (USB_BASE + MUSB_IND_RXINTERVAL_OFFSET)

#ifdef USB_MUSB_SUNXI
#define USB_TXADDR_BASE(ep_idx)    (&USB->TXFUNCADDR0)
#define USB_TXHUBADDR_BASE(ep_idx) (&USB->TXHUBADDR0)
#define USB_TXHUBPORT_BASE(ep_idx) (&USB->TXHUBPORT0)
#define USB_RXADDR_BASE(ep_idx)    (&USB->RXFUNCADDR0)
#define USB_RXHUBADDR_BASE(ep_idx) (&USB->RXHUBADDR0)
#define USB_RXHUBPORT_BASE(ep_idx) (&USB->RXHUBPORT0)
#else
#define USB_TXADDR_BASE(ep_idx)    (&USB->TXFUNCADDR0 + 0x8 * ep_idx)
#define USB_TXHUBADDR_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 2)
#define USB_TXHUBPORT_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 3)
#define USB_RXADDR_BASE(ep_idx)    (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 4)
#define USB_RXHUBADDR_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 6)
#define USB_RXHUBPORT_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 7)
#endif

typedef enum {
    USB_EP0_STATE_SETUP = 0x0, /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA,     /**< IN DATA */
    USB_EP0_STATE_IN_STATUS,   /**< IN status*/
    USB_EP0_STATE_OUT_DATA,    /**< OUT DATA */
    USB_EP0_STATE_OUT_STATUS,  /**< OUT status */
    USB_EP0_STATE_IN_DATA_C,   /**< IN status*/
    USB_EP0_STATE_IN_STATUS_C, /**< IN DATA */
} ep0_state_t;

struct usb_musb_chan {
    bool inuse; /* True: This channel is "in use" */
    bool in;    /* True: IN endpoint */
    uint16_t mps;
    uint8_t interval; /* Polling interval */
    uint8_t *buffer;
    volatile uint32_t buflen;
    volatile uint32_t once_outlen;
    volatile uint16_t xfrd;   /* Bytes transferred (at end of transfer) */
    volatile int result;      /* The result of the transfer */
    volatile bool waiter;     /* True: Thread is waiting for a channel event */
    usb_osal_sem_t waitsem;   /* Channel wait semaphore */
    usb_osal_mutex_t exclsem; /* Support mutually exclusive access */
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback; /* Transfer complete callback */
    void *arg;                       /* Argument that accompanies the callback */
#endif
};

struct usb_musb_priv {
    volatile bool connected; /* Connected to device */
    volatile uint32_t fifo_size_offset;
    struct usb_musb_chan chan[CONFIG_USBHOST_PIPE_NUM];
} g_usbhost;

volatile uint8_t usb_ep0_state = USB_EP0_STATE_SETUP;

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

static void usb_musb_fifo_flush(uint8_t ep)
{
    uint8_t ep_idx = ep & 0x7f;
    if (ep_idx == 0) {
        if ((HWREGB(USB_TXCSRLx_BASE) & (USB_CSRL0_RXRDY | USB_CSRL0_TXRDY)) != 0)
            HWREGB(USB_RXCSRLx_BASE) |= USB_CSRH0_FLUSH;
    } else {
        if (ep & 0x80) {
            if (HWREGB(USB_TXCSRLx_BASE) & USB_TXCSRL1_TXRDY)
                HWREGB(USB_TXCSRLx_BASE) |= USB_TXCSRL1_FLUSH;
        } else {
            if (HWREGB(USB_RXCSRLx_BASE) & USB_RXCSRL1_RXRDY)
                HWREGB(USB_RXCSRLx_BASE) |= USB_RXCSRL1_FLUSH;
        }
    }
}

void usb_musb_ep_status_clear(uint32_t ep_idx, uint32_t flags)
{
    if (ep_idx == USB_EP_0) {
        HWREGB(USB_TXCSRLx_BASE) &= ~flags;
    } else {
        HWREGB(USB_TXCSRLx_BASE) &= ~flags;
        HWREGB(USB_RXCSRLx_BASE) &= ~flags;
    }
}

static void usb_musb_write_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
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

static void usb_musb_read_packet(uint8_t ep_idx, uint8_t *buffer, uint16_t len)
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

/****************************************************************************
 * Name: usb_synopsys_chan_alloc
 *
 * Description:
 *   Allocate a channel.
 *
 ****************************************************************************/
static int usb_musb_chan_alloc(void)
{
    int chidx;

    /* Search the table of channels */

    for (chidx = 2; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        /* Is this channel available? */
        if (!g_usbhost.chan[chidx].inuse) {
            /* Yes... make it "in use" and return the index */

            g_usbhost.chan[chidx].inuse = true;
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
static void usb_musb_chan_free(struct usb_musb_chan *chan)
{
    /* Mark the channel available */
    chan->inuse = false;
}

/****************************************************************************
 * Name: usb_musb_chan_waitsetup
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

static int usb_musb_chan_waitsetup(struct usb_musb_chan *chan)
{
    uint32_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();

    /* Is the device still connected? */

    if (g_usbhost.connected) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */

        chan->waiter = true;
        chan->result = -EBUSY;
        chan->xfrd = 0;
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
 * Name: usb_musb_chan_asynchsetup
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
static int usb_musb_chan_asynchsetup(struct usb_musb_chan *chan, usbh_asynch_callback_t callback, void *arg)
{
    uint32_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();
    /* Is the device still connected? */

    if (g_usbhost.connected) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */

        chan->waiter = false;
        chan->result = -EBUSY;
        chan->xfrd = 0;
        chan->callback = callback;
        chan->arg = arg;
        ret = 0;
    }

    usb_osal_leave_critical_section(flags);
    return ret;
}
#endif

/****************************************************************************
 * Name: usb_musb_chan_wait
 *
 * Description:
 *   Wait for a transfer on a channel to complete.
 *
 * Assumptions:
 *   Called from a normal thread context
 *
 ****************************************************************************/

static int usb_musb_chan_wait(struct usb_musb_chan *chan, uint32_t timeout)
{
    int ret;

    /* Loop, testing for an end of transfer condition.  The channel 'result'
   * was set to EBUSY and 'waiter' was set to true before the transfer;
   * 'waiter' will be set to false and 'result' will be set appropriately
   * when the transfer is completed.
   */

    if (chan->waiter) {
        ret = usb_osal_sem_take(chan->waitsem, timeout);
        if (ret < 0) {
            return ret;
        }
    }

    /* The transfer is complete re-enable interrupts and return the result */
    ret = chan->result;

    if (ret < 0) {
        return ret;
    }

    return chan->xfrd;
}

/****************************************************************************
 * Name: usb_musb_chan_wakeup
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

static void usb_musb_chan_wakeup(struct usb_musb_chan *chan)
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
    g_usbhost.connected = 0;
    g_usbhost.fifo_size_offset = 0;

    usb_hc_low_level_init();

    USB->IE = USB_IE_RESET | USB_IE_CONN | USB_IE_DISCON |
              USB_IE_RESUME | USB_IE_SUSPND |
              USB_IE_BABBLE | USB_IE_SESREQ | USB_IE_VBUSERR;

    USB->TXIE = USB_TXIE_EP0;
    USB->RXIE = 0;

    USB->DEVCTL |= USB_DEVCTL_SESSION;

#ifdef USB_MUSB_SUNXI
    USB->CSRL0 = USB_CSRL0_TXRDY;
#endif
    return 0;
}

bool usbh_get_port_connect_status(const uint8_t port)
{
    return g_usbhost.connected;
}

int usbh_reset_port(const uint8_t port)
{
    USB->POWER |= USB_POWER_RESET;
    usb_osal_msleep(20);
    USB->POWER &= ~(USB_POWER_RESET);
    usb_osal_msleep(20);
    return 0;
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    uint8_t speed;

    if (USB->POWER & USB_POWER_HSMODE)
        speed = USB_SPEED_HIGH;
    else if (USB->DEVCTL & USB_DEVCTL_FSDEV)
        speed = USB_SPEED_FULL;
    else if (USB->DEVCTL & USB_DEVCTL_LSDEV)
        speed = USB_SPEED_LOW;

    return speed;
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    int ret;
    uint8_t ep0 = (uint8_t)ep;
    struct usb_musb_chan *chan;

    chan = &g_usbhost.chan[0];
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    USBC_SelectActiveEp(ep0);
    HWREGB(USB_TXADDR_BASE(ep0)) = dev_addr;

    if (speed == USB_SPEED_HIGH) {
        USB->TYPE0 = USB_TYPE0_SPEED_HIGH;
    } else if (speed == USB_SPEED_FULL) {
        USB->TYPE0 = USB_TYPE0_SPEED_FULL;
    } else if (speed == USB_SPEED_LOW) {
        USB->TYPE0 = USB_TYPE0_SPEED_LOW;
    }

    chan = &g_usbhost.chan[ep0];
    chan->mps = ep_mps;

    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct usbh_hubport *hport;
    struct usb_musb_chan *chan;
    uint32_t chidx;
    uint16_t used = 0;
    uint8_t fifo_size;

    hport = ep_cfg->hport;

    uint32_t old_index = USBC_GetActiveEp();

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        if (hport->parent == NULL) {
            chan = &g_usbhost.chan[0];
            memset(chan, 0, sizeof(struct usb_musb_chan));

            chan->waitsem = usb_osal_sem_create(0);
            chan->exclsem = usb_osal_mutex_create();
            USBC_SelectActiveEp(0);

            HWREGB(USB_TXINTERVALx_BASE) = 0;
            HWREGB(USB_TXHUBADDR_BASE(0)) = 0;
            HWREGB(USB_TXHUBPORT_BASE(0)) = 0;
            USB->NAKLMT = 0;
            *ep = (usbh_epinfo_t)0;
        } else {
        }
    } else {
        chidx = usb_musb_chan_alloc();

        chan = &g_usbhost.chan[chidx];
        memset(chan, 0, sizeof(struct usb_musb_chan));
        chan->inuse = true;
        chan->interval = ep_cfg->ep_interval;
        chan->mps = ep_cfg->ep_mps;

        chan->waitsem = usb_osal_sem_create(0);
        chan->exclsem = usb_osal_mutex_create();

        USBC_SelectActiveEp(chidx);

        uint32_t temp = ep_cfg->ep_addr & 0x7f;

        if (hport->speed == USB_SPEED_HIGH) {
            temp |= USB_TXTYPE1_SPEED_HIGH;
        } else if (hport->speed == USB_SPEED_FULL) {
            temp |= USB_TXTYPE1_SPEED_FULL;
        } else if (hport->speed == USB_SPEED_LOW) {
            temp |= USB_TXTYPE1_SPEED_LOW;
        }

        switch (ep_cfg->ep_type) {
            case 0x00:
                temp |= USB_TXTYPE1_PROTO_CTRL;
                break;
            case 0x01:
                temp |= USB_TXTYPE1_PROTO_ISOC;
                break;
            case 0x02:
                temp |= USB_TXTYPE1_PROTO_BULK;
                break;
            case 0x03:
                temp |= USB_TXTYPE1_PROTO_INT;
                break;
        }

        if (ep_cfg->ep_addr & 0x80) {
            chan->in = 1;

            HWREGB(USB_RXADDR_BASE(chidx)) = hport->dev_addr;
            HWREGB(USB_RXHUBADDR_BASE(chidx)) = 0;
            HWREGB(USB_RXHUBPORT_BASE(chidx)) = 0;

            HWREGB(USB_RXTYPEx_BASE) = temp;
            HWREGH(USB_RXMAPx_BASE) = ep_cfg->ep_mps;
            HWREGB(USB_RXINTERVALx_BASE) = ep_cfg->ep_interval;

            HWREGB(USB_RXCSRHx_BASE) = 0;
            HWREGB(USB_RXCSRLx_BASE) |= USB_RXCSRL1_CLRDT;

            usb_musb_ep_status_clear(chidx, USB_HOST_IN_STATUS);

            fifo_size = usb_musb_get_fifo_size(ep_cfg->ep_mps, &used);

            USB->RXFIFOADD = g_usbhost.fifo_size_offset >> 3;
            USB->RXFIFOSZ = fifo_size & 0x0f;

            g_usbhost.fifo_size_offset += used;

            USB->RXIE |= (1 << chidx);
        } else {
            chan->in = 0;
            HWREGB(USB_TXADDR_BASE(chidx)) = hport->dev_addr;
            HWREGB(USB_TXHUBADDR_BASE(chidx)) = 0;
            HWREGB(USB_TXHUBPORT_BASE(chidx)) = 0;

            HWREGB(USB_TXTYPEx_BASE) = temp;
            HWREGH(USB_TXMAPx_BASE) = ep_cfg->ep_mps;
            HWREGB(USB_TXINTERVALx_BASE) = ep_cfg->ep_interval;

            HWREGB(USB_TXCSRHx_BASE) = 0;
            usb_musb_ep_status_clear(chidx, USB_HOST_OUT_STATUS);
            HWREGB(USB_TXCSRLx_BASE) |= USB_TXCSRL1_CLRDT;

            fifo_size = usb_musb_get_fifo_size(ep_cfg->ep_mps, &used);

            USB->TXFIFOADD = g_usbhost.fifo_size_offset >> 3;
            USB->TXFIFOSZ = fifo_size & 0x0f;

            g_usbhost.fifo_size_offset += used;

            USB->TXIE |= (1 << chidx);
        }

        *ep = (usbh_epinfo_t)chidx;
    }

    USBC_SelectActiveEp(old_index);
    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    usb_musb_chan_free(&g_usbhost.chan[(int)ep]);
    usb_osal_sem_delete(g_usbhost.chan[(int)ep].waitsem);
    usb_osal_mutex_delete(g_usbhost.chan[(int)ep].exclsem);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    int ret;
    struct usb_musb_chan *chan;
    uint8_t ep0 = (uint8_t)ep;

    chan = &g_usbhost.chan[0];
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_musb_chan_waitsetup(chan);

    usb_musb_write_packet(0, (uint8_t *)setup, 8);
    chan->once_outlen = 8;
    if (setup->wLength && buffer) {
        if (setup->bmRequestType & 0x80) {
            usb_ep0_state = USB_EP0_STATE_IN_DATA;
        } else {
            usb_ep0_state = USB_EP0_STATE_OUT_DATA;
        }
        chan->buffer = buffer;
        chan->buflen = setup->wLength;
    } else {
        usb_ep0_state = USB_EP0_STATE_IN_STATUS;
    }

    USBC_SelectActiveEp(0);
    USB->CSRL0 = USB_CSRL0_TXRDY | USB_CSRL0_SETUP;

    ret = usb_musb_chan_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
errout_with_mutex:
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usb_musb_chan *chan;
    uint8_t chidx = (uint8_t)ep;

    chan = &g_usbhost.chan[chidx];
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_musb_chan_waitsetup(chan);

    if (chan->in) {
        chan->buffer = buffer;
        chan->buflen = buflen;
        USBC_SelectActiveEp(chidx);
        HWREGB(USB_RXCSRLx_BASE) = USB_RXCSRL1_REQPKT;
    } else {
        chan->buffer = buffer;
        chan->buflen = buflen;
        if (buflen > chan->mps) {
            buflen = chan->mps;
        }

        usb_musb_write_packet(chidx, (uint8_t *)buffer, buflen);
        USBC_SelectActiveEp(chidx);
        HWREGB(USB_TXCSRLx_BASE) = USB_TXCSRL1_TXRDY;
        chan->buffer += buflen;
        chan->buflen -= buflen;
        chan->xfrd += buflen;
    }
    ret = usb_musb_chan_wait(chan, timeout);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
errout_with_mutex:
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usb_musb_chan *chan;
    uint8_t chidx = (uint8_t)ep;

    chan = &g_usbhost.chan[chidx];
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_musb_chan_waitsetup(chan);

    if (chan->in) {
        chan->buffer = buffer;
        chan->buflen = buflen;
        USBC_SelectActiveEp(chidx);
        HWREGB(USB_RXCSRLx_BASE) = USB_RXCSRL1_REQPKT;
    } else {
        chan->buffer = buffer;
        chan->buflen = buflen;
        if (buflen > chan->mps) {
            buflen = chan->mps;
        }

        usb_musb_write_packet(chidx, (uint8_t *)buffer, buflen);
        USBC_SelectActiveEp(chidx);
        HWREGB(USB_TXCSRLx_BASE) = USB_TXCSRL1_TXRDY;
        chan->buffer += buflen;
        chan->buflen -= buflen;
        chan->xfrd += buflen;
    }
    ret = usb_musb_chan_wait(chan, timeout);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
errout_with_mutex:
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;
    struct usb_musb_chan *chan;
    uint8_t chidx = (uint8_t)ep;

    chan = &g_usbhost.chan[chidx];
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;
    struct usb_musb_chan *chan;
    uint8_t chidx = (uint8_t)ep;

    chan = &g_usbhost.chan[chidx];
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    return 0;
}

void handle_ep0(void)
{
    uint8_t ep0_status = USB->CSRL0;

    if (ep0_status & USB_HOST_EP0_ERROR) {
        usb_musb_ep_status_clear(0, USB_HOST_EP0_ERROR);
        usb_musb_fifo_flush(0);
        usb_ep0_state = USB_EP0_STATE_SETUP;
        g_usbhost.chan[0].result = -EIO;
        goto chan_wait;
    }

    if (ep0_status & USB_HOST_EP0_RX_STALL) {
        usb_musb_ep_status_clear(0, ep0_status & USB_HOST_IN_STATUS);
        usb_ep0_state = USB_EP0_STATE_SETUP;
        g_usbhost.chan[0].result = -EPERM;
        goto chan_wait;
    }
    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            break;
        case USB_EP0_STATE_IN_DATA:
            USB->CSRL0 = USB_RXCSRL1_REQPKT;
            usb_ep0_state = USB_EP0_STATE_IN_DATA_C;
            g_usbhost.chan[0].xfrd += g_usbhost.chan[0].once_outlen;
            break;
        case USB_EP0_STATE_IN_DATA_C:
            if (USB->CSRL0 & USB_CSRL0_RXRDY) {
                uint32_t size = g_usbhost.chan[0].buflen;
                if (size > g_usbhost.chan[0].mps) {
                    size = g_usbhost.chan[0].mps;
                }

                size = MIN(size, USB->COUNT0);

                usb_musb_read_packet(0, g_usbhost.chan[0].buffer, size);
                USB->CSRL0 &= ~USB_CSRL0_RXRDY;

                g_usbhost.chan[0].buffer += size;
                g_usbhost.chan[0].buflen -= size;
                g_usbhost.chan[0].xfrd += size;
                if ((size < g_usbhost.chan[0].mps) || (g_usbhost.chan[0].buflen == 0)) {
                    usb_ep0_state = USB_EP0_STATE_IN_STATUS_C;
                    USB->CSRL0 = USB_CSRL0_TXRDY | USB_CSRL0_STATUS;
                } else {
                    USB->CSRL0 = USB_RXCSRL1_REQPKT;
                }
            }
            break;
        case USB_EP0_STATE_IN_STATUS_C:
            if (ep0_status & (USB_HOST_EP0_RXPKTRDY | USB_HOST_EP0_STATUS)) {
                usb_musb_ep_status_clear(0, (USB_HOST_EP0_RXPKTRDY | USB_HOST_EP0_STATUS));
            }

            usb_ep0_state = USB_EP0_STATE_SETUP;
            g_usbhost.chan[0].result = 0;
            goto chan_wait;

            break;
        case USB_EP0_STATE_IN_STATUS:
            USB->CSRL0 = USB_CSRL0_REQPKT | USB_CSRL0_STATUS;
            usb_ep0_state = USB_EP0_STATE_IN_STATUS_C;
            g_usbhost.chan[0].xfrd += g_usbhost.chan[0].once_outlen;
            break;

        case USB_EP0_STATE_OUT_DATA: {
            g_usbhost.chan[0].xfrd += g_usbhost.chan[0].once_outlen;

            uint32_t size = g_usbhost.chan[0].buflen;
            if (size > g_usbhost.chan[0].mps) {
                size = g_usbhost.chan[0].mps;
            }

            usb_musb_write_packet(0, g_usbhost.chan[0].buffer, size);

            g_usbhost.chan[0].buffer += size;
            g_usbhost.chan[0].buflen -= size;
            g_usbhost.chan[0].once_outlen = size;
            if (size == g_usbhost.chan[0].mps) {
                USB->CSRL0 = USB_CSRL0_TXRDY;
            } else {
                USB->CSRL0 = USB_CSRL0_TXRDY;
                usb_ep0_state = USB_EP0_STATE_IN_STATUS;
            }
        }

        break;
    }
    return;
chan_wait:
    usb_musb_chan_wakeup(&g_usbhost.chan[0]);
}
void USBH_IRQHandler(void)
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint32_t ep_status;
    struct usb_musb_chan *chan;
    uint8_t chidx;
    uint8_t old_ep_idx;

    is = USB->IS;
    txis = USB->TXIS;
    rxis = USB->RXIS;

    old_ep_idx = USBC_GetActiveEp();

    if (is & USB_IS_CONN) {
        if (!g_usbhost.connected) {
            g_usbhost.connected = true;
            usbh_event_notify_handler(USBH_EVENT_CONNECTED, 1);
        }
        USB->IS = USB_IS_CONN;
    }

    if (is & USB_IS_DISCON) {
        if (g_usbhost.connected) {
            g_usbhost.connected = false;

            for (uint8_t chnum = 0; chnum < CONFIG_USBHOST_PIPE_NUM; chnum++) {
                if (g_usbhost.chan[chnum].waiter) {
                    /* Wake'em up! */
                    g_usbhost.chan[chnum].waiter = false;
                    usb_osal_sem_give(g_usbhost.chan[chnum].waitsem);
                }
            }

            usbh_event_notify_handler(USBH_EVENT_DISCONNECTED, 1);
        }
        USB->IS = USB_IS_DISCON;
    }
    if (is & USB_IS_SOF) {
        USB->IS = USB_IS_SOF;
    }

    txis &= USB->TXIE;
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        txis &= ~USB_TXIE_EP0;
        USB->TXIS = USB_TXIE_EP0; // clear isr flag
        USBC_SelectActiveEp(0);
        handle_ep0();
    }

    while (txis) {
        chidx = __builtin_ctz(txis);
        txis &= ~(1 << chidx);
        USB->TXIS = (1 << chidx); // clear isr flag

        chan = &g_usbhost.chan[chidx];

        USBC_SelectActiveEp(chidx);

        ep_status = HWREGB(USB_TXCSRLx_BASE);
        usb_musb_ep_status_clear(chidx, ep_status | USB_HOST_OUT_STATUS);

        if (ep_status & USB_HOST_OUT_STALL) {
            chan->result = -EPERM;
            usb_musb_ep_status_clear(chidx, USB_HOST_OUT_STALL);
            usb_musb_chan_wakeup(chan);
        } else if (ep_status & USB_HOST_OUT_ERROR) {
            chan->result = -EIO;
            usb_musb_ep_status_clear(chidx, USB_HOST_OUT_ERROR);
            usb_musb_chan_wakeup(chan);
        } else {
            uint32_t size = chan->buflen;

            if (size) {
                if (size > chan->mps) {
                    size = chan->mps;
                }
                usb_musb_write_packet(chidx, chan->buffer, size);
                HWREGB(USB_TXCSRLx_BASE) = USB_TXCSRL1_TXRDY;
                chan->buffer += size;
                chan->buflen -= size;
                chan->xfrd += size;
            } else {
                chan->result = 0;
                usb_musb_chan_wakeup(chan);
            }
        }
    }

    rxis &= USB->RXIE;
    while (rxis) {
        chidx = __builtin_ctz(rxis);
        rxis &= ~(1 << chidx);
        USB->RXIS = (1 << chidx); // clear isr flag
        chan = &g_usbhost.chan[chidx];

        USBC_SelectActiveEp(chidx);

        ep_status = HWREGB(USB_RXCSRLx_BASE);

        if (ep_status & USB_HOST_IN_STALL) {
            chan->result = -EPERM;
            usb_musb_ep_status_clear(chidx, USB_HOST_IN_STALL);
            usb_musb_chan_wakeup(chan);
        } else if (ep_status & USB_HOST_IN_ERROR) {
            chan->result = -EIO;
            usb_musb_ep_status_clear(chidx, USB_HOST_IN_ERROR);
            usb_musb_chan_wakeup(chan);
        } else if (ep_status & USB_RXCSRL1_RXRDY) {
            uint32_t size = chan->buflen;
            if (size > chan->mps) {
                size = chan->mps;
            }
            size = MIN(size, HWREG(USB_RXCOUNTx_BASE));

            usb_musb_read_packet(chidx, chan->buffer, size);
            HWREGB(USB_RXCSRLx_BASE) &= ~USB_RXCSRL1_RXRDY;
            chan->buffer += size;
            chan->buflen -= size;
            chan->xfrd += size;
            if ((size < chan->mps) || (chan->buflen == 0)) {
                chan->result = 0;
                usb_musb_chan_wakeup(chan);
            } else {
                HWREGB(USB_RXCSRLx_BASE) = USB_RXCSRL1_REQPKT;
            }
        }
    }
    USBC_SelectActiveEp(old_ep_idx);
}