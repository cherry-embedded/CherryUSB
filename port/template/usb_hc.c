#include "usbh_core.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBH_IRQHandler
#endif

struct xxx_pipe {
    volatile int result;      /* The result of the transfer */
    volatile uint32_t xfrd;   /* Bytes transferred (at end of transfer) */
    volatile bool waiter;     /* True: Thread is waiting for a channel event */
    usb_osal_sem_t waitsem;   /* Channel wait semaphore */
    usb_osal_mutex_t exclsem; /* Support mutually exclusive access */
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback; /* Transfer complete callback */
    void *arg;                       /* Argument that accompanies the callback */
#endif
};

struct xxx_hcd {
    struct xxx_pipe chan[5];
} g_xxx_hcd;

static int xxx_pipe_alloc(void)
{
    int chidx;

    for (chidx = 0; chidx < HCD_MAX_ENDPOINT; chidx++) {
        if (!g_xxx_hcd.chan[chidx].inuse) {
            g_xxx_hcd.chan[chidx].inuse = true;
            return chidx;
        }
    }

    return -EBUSY;
}

static void xxx_pipe_free(struct xxx_pipe *chan)
{
    chan->inuse = false;
}

static int xxx_pipe_waitsetup(struct xxx_pipe *chan)
{
    size_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();

    if (usbh_get_port_connect_status(1)) {

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

#ifdef CONFIG_USBHOST_ASYNCH
static int xxx_pipe_asynchsetup(struct xxx_pipe *chan, usbh_asynch_callback_t callback, void *arg)
{
    size_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();

    if (usbh_get_port_connect_status(1)) {

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

static int xxx_pipe_wait(struct xxx_pipe *chan, uint32_t timeout)
{
    int ret;

    if (chan->waiter) {
        ret = usb_osal_sem_take(chan->waitsem, timeout);
        if (ret < 0) {
            return ret;
        }
    }

    ret = chan->result;

    if (ret < 0) {
        return ret;
    }
    return chan->xfrd;
}

static void xxx_pipe_wakeup(struct xxx_pipe *chan)
{
    usbh_asynch_callback_t callback;
    void *arg;
    int nbytes;

    if (chan->waiter) {
        chan->waiter = false;
        usb_osal_sem_give(chan->waitsem);
    }
#ifdef CONFIG_USBHOST_ASYNCH
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

int usb_hc_sw_init(void)
{
    memset(&g_xxx_hcd, 0, sizeof(struct xxx_hcd));

    return 0;
}

int usb_hc_hw_init(void)
{
    usb_hc_low_level_init();

    return 0;
}

int usbh_reset_port(const uint8_t port)
{
    return 0;
}

bool usbh_get_port_connect_status(const uint8_t port)
{
    return false;
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    return USB_SPEED_FULL;
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct xxx_pipe *chan;
    struct usbh_hubport *hport;
    int chidx;
    uint8_t speed;
    usb_osal_sem_t waitsem;
    usb_osal_mutex_t exclsem;

    chidx = xxx_pipe_alloc();
    chan = &g_xxx_hcd.chan[chidx];

    waitsem = chan->waitsem;
    exclsem = chan->exclsem;

    memset(chan, 0, sizeof(struct xxx_pipe));

    /* restore variable */
    chan->waitsem = waitsem;
    chan->exclsem = exclsem;

    *ep = (usbh_epinfo_t)chan;
    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = xxx_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
error_out:
    chan->waiter = false;
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = xxx_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
error_out:
    chan->waiter = false;
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);

    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    struct xxx_pipe *chan;
    int ret;

    chan = (struct xxx_pipe *)ep;

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

void USBH_IRQHandler(void)
{
}