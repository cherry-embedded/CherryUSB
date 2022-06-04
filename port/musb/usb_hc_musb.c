#include "usbh_core.h"
#include "usb_musb_reg.h"

#define HWREG(x) \
    (*((volatile uint32_t *)(x)))
#define HWREGH(x) \
    (*((volatile uint16_t *)(x)))
#define HWREGB(x) \
    (*((volatile uint8_t *)(x)))

#ifdef CONFIG_USB_MUSB_SUNXI

#ifndef USB_BASE
#define USB_BASE (0x01c13000)
#endif

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBH_IRQHandler //use actual usb irq name instead
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

#define MUSB_FIFO_OFFSET 0x00

#define MUSB_DEVCTL_OFFSET 0x41

#define MUSB_TXFIFOSZ_OFFSET  0x90
#define MUSB_RXFIFOSZ_OFFSET  0x94
#define MUSB_TXFIFOADD_OFFSET 0x92
#define MUSB_RXFIFOADD_OFFSET 0x96

#define MUSB_TXFUNCADDR0_OFFSET 0x98
#define MUSB_TXHUBADDR0_OFFSET  0x9A
#define MUSB_TXHUBPORT0_OFFSET  0x9B
#define MUSB_TXFUNCADDRx_OFFSET 0x98
#define MUSB_TXHUBADDRx_OFFSET  0x9A
#define MUSB_TXHUBPORTx_OFFSET  0x9B
#define MUSB_RXFUNCADDRx_OFFSET 0x9C
#define MUSB_RXHUBADDRx_OFFSET  0x9E
#define MUSB_RXHUBPORTx_OFFSET  0x9F

#define USB_TXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDRx_OFFSET)
#define USB_TXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXHUBADDRx_OFFSET)
#define USB_TXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXHUBPORTx_OFFSET)
#define USB_RXADDR_BASE(ep_idx)    (USB_BASE + MUSB_RXFUNCADDRx_OFFSET)
#define USB_RXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_RXHUBADDRx_OFFSET)
#define USB_RXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_RXHUBPORTx_OFFSET)

#elif defined(CONFIG_USB_MUSB_CUSTOM)

#else
#ifndef USBH_IRQHandler
#define USBH_IRQHandler USB_INT_Handler
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

#define MUSB_FIFO_OFFSET 0x20

#define MUSB_DEVCTL_OFFSET 0x60

#define MUSB_TXFIFOSZ_OFFSET  0x62
#define MUSB_RXFIFOSZ_OFFSET  0x63
#define MUSB_TXFIFOADD_OFFSET 0x64
#define MUSB_RXFIFOADD_OFFSET 0x66

#define MUSB_TXFUNCADDR0_OFFSET 0x80
#define MUSB_TXHUBADDR0_OFFSET  0x82
#define MUSB_TXHUBPORT0_OFFSET  0x83
#define MUSB_TXFUNCADDRx_OFFSET 0x88
#define MUSB_TXHUBADDRx_OFFSET  0x8A
#define MUSB_TXHUBPORTx_OFFSET  0x8B
#define MUSB_RXFUNCADDRx_OFFSET 0x8C
#define MUSB_RXHUBADDRx_OFFSET  0x8E
#define MUSB_RXHUBPORTx_OFFSET  0x8F

#define USB_TXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx)
#define USB_TXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 2)
#define USB_TXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 3)
#define USB_RXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 4)
#define USB_RXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 6)
#define USB_RXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 7)
#endif

#define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)

#ifndef CONIFG_USB_MUSB_EP_NUM
#define CONIFG_USB_MUSB_EP_NUM 5
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
    uint8_t ep_idx;
    bool inuse; /* True: This channel is "in use" */
    bool in;    /* True: IN endpoint */
    uint16_t mps;
    uint8_t interval; /* Polling interval */
    uint8_t speed;
    uint8_t *buffer;
    volatile uint32_t buflen;
    volatile uint16_t xfrd; /* Bytes transferred (at end of transfer) */
    volatile int result;    /* The result of the transfer */
    struct usbh_hubport *hport;
    volatile bool waiter;   /* True: Thread is waiting for a channel event */
    usb_osal_sem_t waitsem; /* Channel wait semaphore */
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback; /* Transfer complete callback */
    void *arg;                       /* Argument that accompanies the callback */
#endif
};

struct usb_musb_priv {
    struct usb_musb_chan chan[CONIFG_USB_MUSB_EP_NUM][CONFIG_USBHOST_PIPE_NUM];
    volatile struct usb_musb_chan *active_chan[CONIFG_USB_MUSB_EP_NUM];
    usb_osal_mutex_t exclsem[CONIFG_USB_MUSB_EP_NUM]; /* Support mutually exclusive access */
} g_usbhost;

static volatile uint8_t usb_ep0_state = USB_EP0_STATE_SETUP;
volatile uint8_t ep0_outlen = 0;

/* get current active ep */
static uint8_t USBC_GetActiveEp(void)
{
    return HWREGB(USB_BASE + MUSB_EPIDX_OFFSET);
}

/* set the active ep */
static void USBC_SelectActiveEp(uint8_t ep_index)
{
    HWREGB(USB_BASE + MUSB_EPIDX_OFFSET) = ep_index;
}

static void usb_musb_fifo_flush(uint8_t ep)
{
    uint8_t ep_idx = ep & 0x7f;
    if (ep_idx == 0) {
        if ((HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & (USB_CSRL0_RXRDY | USB_CSRL0_TXRDY)) != 0)
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_CSRH0_FLUSH;
    } else {
        if (ep & 0x80) {
            if (HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) & USB_TXCSRL1_TXRDY)
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) |= USB_TXCSRL1_FLUSH;
        } else {
            if (HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) & USB_RXCSRL1_RXRDY)
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) |= USB_RXCSRL1_FLUSH;
        }
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

/****************************************************************************
 * Name: usb_synopsys_chan_alloc
 *
 * Description:
 *   Allocate a channel.
 *
 ****************************************************************************/
static int usb_musb_chan_alloc(uint8_t ep_idx)
{
    int chidx;

    /* Search the table of channels */

    for (chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        /* Is this channel available? */
        if (!g_usbhost.chan[ep_idx][chidx].inuse) {
            /* Yes... make it "in use" and return the index */

            g_usbhost.chan[ep_idx][chidx].inuse = true;
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
    size_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();

    /* Is the device still connected? */

    if (usbh_get_port_connect_status(0)) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */
        g_usbhost.active_chan[chan->ep_idx] = chan;
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
    size_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();
    /* Is the device still connected? */

    if (usbh_get_port_connect_status(0)) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */
        g_usbhost.active_chan[chan->ep_idx] = chan;
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

    /* The transfer is complete and return the result */
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

    g_usbhost.active_chan[chan->ep_idx] = NULL;

    /* Is the transfer complete? */
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
#endif
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_init(void)
{
    uint8_t regval;
    uint32_t fifo_offset = 0;

    usb_hc_low_level_init();

    USBC_SelectActiveEp(0);
    HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = 0;
    HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = USB_TXFIFOSZ_SIZE_64;
    HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = 0;
    HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = USB_TXFIFOSZ_SIZE_64;
    HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = 0;
    fifo_offset += 64;

    for (uint8_t i = 1; i < CONIFG_USB_MUSB_EP_NUM; i++) {
        USBC_SelectActiveEp(i);
        HWREGB(USB_BASE + MUSB_TXFIFOSZ_OFFSET) = USB_TXFIFOSZ_SIZE_512;
        HWREGH(USB_BASE + MUSB_TXFIFOADD_OFFSET) = fifo_offset;
        HWREGB(USB_BASE + MUSB_RXFIFOSZ_OFFSET) = USB_TXFIFOSZ_SIZE_512;
        HWREGH(USB_BASE + MUSB_RXFIFOADD_OFFSET) = fifo_offset;
        fifo_offset += 512;
    }

    for (uint8_t i = 0; i < CONIFG_USB_MUSB_EP_NUM; i++) {
        g_usbhost.exclsem[i] = usb_osal_mutex_create();
    }

    /* Enable USB interrupts */
    regval = USB_IE_RESET | USB_IE_CONN | USB_IE_DISCON |
             USB_IE_RESUME | USB_IE_SUSPND |
             USB_IE_BABBLE | USB_IE_SESREQ | USB_IE_VBUSERR;

    HWREGB(USB_BASE + MUSB_IE_OFFSET) = regval;
    HWREGH(USB_BASE + MUSB_TXIE_OFFSET) = USB_TXIE_EP0;
    HWREGH(USB_BASE + MUSB_RXIE_OFFSET) = 0;

    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_HSENAB;

    HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) |= USB_DEVCTL_SESSION;

#ifdef CONFIG_USB_MUSB_SUNXI
    USBC_SelectActiveEp(0);
    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
#endif
    return 0;
}

bool usbh_get_port_connect_status(const uint8_t port)
{
    if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_FSDEV)
        return true;
    if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_LSDEV)
        return true;

    return false;
}

int usbh_reset_port(const uint8_t port)
{
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) |= USB_POWER_RESET;
    usb_osal_msleep(20);
    HWREGB(USB_BASE + MUSB_POWER_OFFSET) &= ~(USB_POWER_RESET);
    usb_osal_msleep(20);
    return 0;
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    uint8_t speed;

    if (HWREGB(USB_BASE + MUSB_POWER_OFFSET) & USB_POWER_HSMODE)
        speed = USB_SPEED_HIGH;
    else if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_FSDEV)
        speed = USB_SPEED_FULL;
    else if (HWREGB(USB_BASE + MUSB_DEVCTL_OFFSET) & USB_DEVCTL_LSDEV)
        speed = USB_SPEED_LOW;

    return speed;
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    int ret;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem[0]);
    if (ret < 0) {
        return ret;
    }

    chan->mps = ep_mps;
    chan->hport->dev_addr = dev_addr;

    if (speed == USB_SPEED_HIGH) {
        chan->speed = USB_TYPE0_SPEED_HIGH;
    } else if (speed == USB_SPEED_FULL) {
        chan->speed = USB_TYPE0_SPEED_FULL;
    } else if (speed == USB_SPEED_LOW) {
        chan->speed = USB_TYPE0_SPEED_LOW;
    }

    usb_osal_mutex_give(g_usbhost.exclsem[0]);
    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct usbh_hubport *hport;
    struct usb_musb_chan *chan;
    uint32_t chidx;
    uint8_t ep_idx = 0;
    uint8_t old_ep_index;

    hport = ep_cfg->hport;

    ep_idx = ep_cfg->ep_addr & 0x7f;
    old_ep_index = USBC_GetActiveEp();
    USBC_SelectActiveEp(ep_idx);

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        chidx = usb_musb_chan_alloc(0);
        chan = &g_usbhost.chan[0][chidx];
        memset(chan, 0, sizeof(struct usb_musb_chan));
        chan->inuse = true;
        chan->ep_idx = 0;
        chan->interval = 0;
        chan->mps = ep_cfg->ep_mps;
        chan->hport = hport;

        chan->waitsem = usb_osal_sem_create(0);

        *ep = (usbh_epinfo_t)chan;
    } else {
        chidx = usb_musb_chan_alloc(ep_idx);

        chan = &g_usbhost.chan[ep_idx][chidx];
        memset(chan, 0, sizeof(struct usb_musb_chan));
        chan->inuse = true;
        chan->in = ep_cfg->ep_addr & 0x80 ? 1 : 0;
        chan->ep_idx = ep_idx;
        chan->interval = ep_cfg->ep_interval;
        chan->mps = ep_cfg->ep_mps;

        if (hport->speed == USB_SPEED_HIGH) {
            chan->speed = USB_TXTYPE1_SPEED_HIGH;
        } else if (hport->speed == USB_SPEED_FULL) {
            chan->speed = USB_TXTYPE1_SPEED_FULL;
        } else if (hport->speed == USB_SPEED_LOW) {
            chan->speed = USB_TXTYPE1_SPEED_LOW;
        }

        chan->hport = hport;

        chan->waitsem = usb_osal_sem_create(0);

        if (chan->in) {
            HWREGH(USB_BASE + MUSB_RXIE_OFFSET) |= (1 << ep_idx);
        } else {
            HWREGH(USB_BASE + MUSB_TXIE_OFFSET) |= (1 << ep_idx);
        }

        *ep = (usbh_epinfo_t)chan;
    }

    USBC_SelectActiveEp(old_ep_index);
    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;
    usb_musb_chan_free(chan);
    usb_osal_sem_delete(chan->waitsem);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    int ret;
    uint32_t old_ep_index;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem[0]);
    if (ret < 0) {
        return ret;
    }

    ret = usb_musb_chan_waitsetup(chan);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    old_ep_index = USBC_GetActiveEp();
    USBC_SelectActiveEp(0);

    HWREGB(USB_TXADDR_BASE(0)) = chan->hport->dev_addr;
    HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = chan->speed;
    if (chan->hport->parent == NULL) {
        HWREGB(USB_TXHUBADDR_BASE(0)) = 0;
        HWREGB(USB_TXHUBPORT_BASE(0)) = 0;
    } else {
        HWREGB(USB_TXHUBADDR_BASE(0)) = chan->hport->parent->dev_addr;
        HWREGB(USB_TXHUBPORT_BASE(0)) = chan->hport->parent->index - 1;
    }

    usb_musb_write_packet(0, (uint8_t *)setup, 8);
    ep0_outlen = 8;
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

    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY | USB_CSRL0_SETUP;
    USBC_SelectActiveEp(old_ep_index);

    ret = usb_musb_chan_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(g_usbhost.exclsem[0]);
    return ret;
errout_with_mutex:
    chan->waiter = false;
    usb_osal_mutex_give(g_usbhost.exclsem[0]);
    return ret;
}

int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    uint32_t old_ep_index;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem[chan->ep_idx]);
    if (ret < 0) {
        return ret;
    }

    ret = usb_musb_chan_waitsetup(chan);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    old_ep_index = USBC_GetActiveEp();
    USBC_SelectActiveEp(chan->ep_idx);

    if (chan->in) {
        HWREGB(USB_RXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_BULK;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
    } else {
        HWREGB(USB_TXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_BULK;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        if (buflen > chan->mps) {
            buflen = chan->mps;
        }

        usb_musb_write_packet(chan->ep_idx, chan->buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
    }
    USBC_SelectActiveEp(old_ep_index);

    ret = usb_musb_chan_wait(chan, timeout);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
errout_with_mutex:
    chan->waiter = false;
    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    uint32_t old_ep_index;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;

    ret = usb_osal_mutex_take(g_usbhost.exclsem[chan->ep_idx]);
    if (ret < 0) {
        return ret;
    }

    ret = usb_musb_chan_waitsetup(chan);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    old_ep_index = USBC_GetActiveEp();
    USBC_SelectActiveEp(chan->ep_idx);

    if (chan->in) {
        HWREGB(USB_RXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_INT;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
    } else {
        HWREGB(USB_TXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_INT;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        if (buflen > chan->mps) {
            buflen = chan->mps;
        }

        usb_musb_write_packet(chan->ep_idx, chan->buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
    }
    USBC_SelectActiveEp(old_ep_index);

    ret = usb_musb_chan_wait(chan, timeout);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
errout_with_mutex:
    chan->waiter = false;
    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;
    uint32_t old_ep_index;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;

    if (g_usbhost.active_chan[chan->ep_idx]) {
        return -EINVAL;
    }

    ret = usb_osal_mutex_take(g_usbhost.exclsem[chan->ep_idx]);
    if (ret < 0) {
        return ret;
    }

    ret = usb_musb_chan_asynchsetup(chan, callback, arg);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    old_ep_index = USBC_GetActiveEp();
    USBC_SelectActiveEp(chan->ep_idx);

    if (chan->in) {
        HWREGB(USB_RXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_BULK;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
    } else {
        HWREGB(USB_TXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_BULK;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        if (buflen > chan->mps) {
            buflen = chan->mps;
        }

        usb_musb_write_packet(chan->ep_idx, chan->buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
    }
    USBC_SelectActiveEp(old_ep_index);

    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
errout_with_mutex:
    g_usbhost.active_chan[chan->ep_idx] = NULL;
    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;
    uint32_t old_ep_index;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;

    if (g_usbhost.active_chan[chan->ep_idx]) {
        return -EINVAL;
    }

    ret = usb_osal_mutex_take(g_usbhost.exclsem[chan->ep_idx]);
    if (ret < 0) {
        return ret;
    }

    ret = usb_musb_chan_asynchsetup(chan, callback, arg);
    if (ret < 0) {
        goto errout_with_mutex;
    }

    old_ep_index = USBC_GetActiveEp();
    USBC_SelectActiveEp(chan->ep_idx);

    if (chan->in) {
        HWREGB(USB_RXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_RXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_INT;
        HWREGB(USB_BASE + MUSB_IND_RXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_RXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_RXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
    } else {
        HWREGB(USB_TXADDR_BASE(chan->ep_idx)) = chan->hport->dev_addr;
        HWREGB(USB_BASE + MUSB_IND_TXTYPE_OFFSET) = chan->ep_idx | chan->speed | USB_TXTYPE1_PROTO_INT;
        HWREGB(USB_BASE + MUSB_IND_TXINTERVAL_OFFSET) = chan->interval;
        if (chan->hport->parent == NULL) {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = 0;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = 0;
        } else {
            HWREGB(USB_TXHUBADDR_BASE(chan->ep_idx)) = chan->hport->parent->dev_addr;
            HWREGB(USB_TXHUBPORT_BASE(chan->ep_idx)) = chan->hport->parent->index - 1;
        }

        chan->buffer = buffer;
        chan->buflen = buflen;
        if (buflen > chan->mps) {
            buflen = chan->mps;
        }

        usb_musb_write_packet(chan->ep_idx, chan->buffer, buflen);
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) &= ~USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRH_OFFSET) |= USB_TXCSRH1_MODE;
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
    }
    USBC_SelectActiveEp(old_ep_index);

    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
errout_with_mutex:
    g_usbhost.active_chan[chan->ep_idx] = NULL;
    usb_osal_mutex_give(g_usbhost.exclsem[chan->ep_idx]);
    return ret;
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    size_t flags;
    struct usb_musb_chan *chan = (struct usb_musb_chan *)ep;
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback;
    void *arg;
#endif

    flags = usb_osal_enter_critical_section();

    chan->result = -ESHUTDOWN;
#ifdef CONFIG_USBHOST_ASYNCH
    /* Extract the callback information */
    callback = chan->callback;
    arg = chan->arg;
    chan->callback = NULL;
    chan->arg = NULL;
    chan->xfrd = 0;
#endif

    g_usbhost.active_chan[chan->ep_idx] = NULL;
    usb_osal_leave_critical_section(flags);
    /* Is there a thread waiting for this transfer to complete? */

    if (chan->waiter) {
        /* Wake'em up! */
        chan->waiter = false;
        usb_osal_sem_give(chan->waitsem);
    }
#ifdef CONFIG_USBHOST_ASYNCH
    /* No.. is an asynchronous callback expected when the transfer completes? */
    else if (callback) {
        /* Then perform the callback */
        callback(arg, -ESHUTDOWN);
    }
#endif
    return 0;
}

void handle_ep0(void)
{
    uint8_t ep0_status;
    struct usb_musb_chan *chan;
    int result = 0;

    chan = (struct usb_musb_chan *)g_usbhost.active_chan[0];

    USBC_SelectActiveEp(0);
    ep0_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);

    if (ep0_status & USB_CSRL0_STALLED) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALLED;
        usb_ep0_state = USB_EP0_STATE_SETUP;
        result = -EPERM;
        goto chan_wait;
    }

    if (ep0_status & USB_CSRL0_ERROR) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_ERROR;
        usb_musb_fifo_flush(0);
        usb_ep0_state = USB_EP0_STATE_SETUP;
        result = -EIO;
        goto chan_wait;
    }

    if (ep0_status & USB_CSRL0_STALL) {
        HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_STALL;
        usb_ep0_state = USB_EP0_STATE_SETUP;
        result = -EPERM;
        goto chan_wait;
    }

    switch (usb_ep0_state) {
        case USB_EP0_STATE_SETUP:
            break;
        case USB_EP0_STATE_IN_DATA:
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
            usb_ep0_state = USB_EP0_STATE_IN_DATA_C;
            chan->xfrd += 8;
            break;
        case USB_EP0_STATE_IN_DATA_C:
            if (ep0_status & USB_CSRL0_RXRDY) {
                uint32_t size = chan->buflen;
                if (size > chan->mps) {
                    size = chan->mps;
                }

                size = MIN(size, HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET));

                usb_musb_read_packet(0, chan->buffer, size);

                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_CSRL0_RXRDY;

                chan->buffer += size;
                chan->buflen -= size;
                chan->xfrd += size;
                if ((size < chan->mps) || (chan->buflen == 0)) {
                    usb_ep0_state = USB_EP0_STATE_OUT_STATUS;
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_TXRDY | USB_CSRL0_STATUS);
                } else {
                    HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
                }
            }
            break;
        case USB_EP0_STATE_OUT_STATUS:
            usb_ep0_state = USB_EP0_STATE_SETUP;
            result = 0;
            goto chan_wait;
        case USB_EP0_STATE_IN_STATUS_C:
            if (ep0_status & (USB_CSRL0_RXRDY | USB_CSRL0_STATUS)) {
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~(USB_CSRL0_RXRDY | USB_CSRL0_STATUS);
            }

            usb_ep0_state = USB_EP0_STATE_SETUP;
            result = 0;
            goto chan_wait;

            break;
        case USB_EP0_STATE_IN_STATUS:
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = (USB_CSRL0_REQPKT | USB_CSRL0_STATUS);
            usb_ep0_state = USB_EP0_STATE_IN_STATUS_C;
            chan->xfrd += 8;
            break;

        case USB_EP0_STATE_OUT_DATA: {
            uint32_t size = chan->buflen;
            if (size > chan->mps) {
                size = chan->mps;
            }

            chan->xfrd += ep0_outlen;

            usb_musb_write_packet(0, chan->buffer, size);

            chan->buffer += size;
            chan->buflen -= size;
            ep0_outlen = size;
            if (size == chan->mps) {
            } else {
                usb_ep0_state = USB_EP0_STATE_IN_STATUS;
            }
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_CSRL0_TXRDY;
        }

        break;
    }
    return;
chan_wait:
    if (chan) {
        chan->result = result;
        usb_musb_chan_wakeup(chan);
    }
}

void USBH_IRQHandler(void)
{
    uint32_t is;
    uint32_t txis;
    uint32_t rxis;
    uint8_t ep_csrl_status;
    // uint8_t ep_csrh_status;
    struct usb_musb_chan *chan;
    uint8_t ep_idx;
    uint8_t old_ep_idx;
    int result = 0;

    is = HWREGB(USB_BASE + MUSB_IS_OFFSET);
    txis = HWREGH(USB_BASE + MUSB_TXIS_OFFSET);
    rxis = HWREGH(USB_BASE + MUSB_RXIS_OFFSET);

    HWREGB(USB_BASE + MUSB_IS_OFFSET) = is;

    old_ep_idx = USBC_GetActiveEp();

    if (is & USB_IS_CONN) {
        if (usbh_get_port_connect_status(0)) {
            usbh_event_notify_handler(USBH_EVENT_CONNECTED, 1);
        }
    }

    if (is & USB_IS_DISCON) {
        if (usbh_get_port_connect_status(0) == false) {
            for (uint8_t ep_index = 0; ep_index < CONIFG_USB_MUSB_EP_NUM; ep_index++) {
                if (g_usbhost.active_chan[ep_index]) {
                    chan = (struct usb_musb_chan *)g_usbhost.active_chan[ep_index];
                    chan->result = -ENXIO;
                    usb_musb_chan_wakeup(chan);
                }
            }

            usbh_event_notify_handler(USBH_EVENT_DISCONNECTED, 1);
        }
    }

    if (is & USB_IS_SOF) {
    }

    if (is & USB_IS_RESUME) {
    }

    if (is & USB_IS_SUSPEND) {
    }

    if (is & USB_IS_VBUSERR) {
    }

    if (is & USB_IS_SESREQ) {
    }

    if (is & USB_IS_BABBLE) {
    }

    txis &= HWREGH(USB_BASE + MUSB_TXIE_OFFSET);
    /* Handle EP0 interrupt */
    if (txis & USB_TXIE_EP0) {
        txis &= ~USB_TXIE_EP0;
        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = USB_TXIE_EP0;
        handle_ep0();
    }

    while (txis) {
        ep_idx = __builtin_ctz(txis);
        txis &= ~(1 << ep_idx);
        HWREGH(USB_BASE + MUSB_TXIS_OFFSET) = (1 << ep_idx);

        chan = (struct usb_musb_chan *)g_usbhost.active_chan[ep_idx];

        USBC_SelectActiveEp(ep_idx);

        ep_csrl_status = HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET);

        if (ep_csrl_status & USB_TXCSRL1_ERROR) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_ERROR;
            result = -EIO;
            goto chan_wait;
        } else if (ep_csrl_status & USB_TXCSRL1_NAKTO) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_NAKTO;
            result = -EBUSY;
            goto chan_wait;
        } else if (ep_csrl_status & USB_TXCSRL1_STALL) {
            HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) &= ~USB_TXCSRL1_STALL;
            result = -EPERM;
            goto chan_wait;
        } else {
            uint32_t size = chan->buflen;

            if (size > chan->mps) {
                size = chan->mps;
            }

            chan->buffer += size;
            chan->buflen -= size;
            chan->xfrd += size;

            if (chan->buflen == 0) {
                result = 0;
                goto chan_wait;
            } else {
                usb_musb_write_packet(ep_idx, chan->buffer, size);
                HWREGB(USB_BASE + MUSB_IND_TXCSRL_OFFSET) = USB_TXCSRL1_TXRDY;
            }
        }
    }

    rxis &= HWREGH(USB_BASE + MUSB_RXIE_OFFSET);
    while (rxis) {
        ep_idx = __builtin_ctz(rxis);
        rxis &= ~(1 << ep_idx);
        HWREGH(USB_BASE + MUSB_RXIS_OFFSET) = (1 << ep_idx); // clear isr flag

        chan = (struct usb_musb_chan *)g_usbhost.active_chan[ep_idx];

        USBC_SelectActiveEp(ep_idx);

        ep_csrl_status = HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET);
        //ep_csrh_status = HWREGB(USB_BASE + MUSB_IND_RXCSRH_OFFSET); // todo:for iso transfer

        if (ep_csrl_status & USB_RXCSRL1_ERROR) {
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_ERROR;
            result = -EIO;
            goto chan_wait;
        } else if (ep_csrl_status & USB_RXCSRL1_NAKTO) {
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_NAKTO;
            result = -EBUSY;
            goto chan_wait;
        } else if (ep_csrl_status & USB_RXCSRL1_STALL) {
            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_STALL;
            result = -EPERM;
            goto chan_wait;
        } else if (ep_csrl_status & USB_RXCSRL1_RXRDY) {
            uint32_t size = chan->buflen;
            if (size > chan->mps) {
                size = chan->mps;
            }
            size = MIN(size, HWREGH(USB_BASE + MUSB_IND_RXCOUNT_OFFSET));

            usb_musb_read_packet(ep_idx, chan->buffer, size);

            HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) &= ~USB_RXCSRL1_RXRDY;

            chan->buffer += size;
            chan->buflen -= size;
            chan->xfrd += size;
            if ((size < chan->mps) || (chan->buflen == 0)) {
                result = 0;
                goto chan_wait;
            } else {
                HWREGB(USB_BASE + MUSB_IND_RXCSRL_OFFSET) = USB_RXCSRL1_REQPKT;
            }
        }
    }
    USBC_SelectActiveEp(old_ep_idx);
    return;
chan_wait:
    USBC_SelectActiveEp(old_ep_idx);
    if (chan) {
        chan->result = result;
        usb_musb_chan_wakeup(chan);
    }
}