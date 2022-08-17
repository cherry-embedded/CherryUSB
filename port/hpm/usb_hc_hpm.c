#include "usbh_core.h"
#include "hpm_usb_host.h"
#include "hpm_common.h"
#include "hpm_soc.h"
#include "hpm_l1c_drv.h"
#include "board.h"

#define HCD_MAX_ENDPOINT 16

struct hpm_ehci_pipe {
    uint8_t ep_addr;
    uint8_t dev_addr;
    uint8_t *buffer;          /* for dcache invalidate */
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

struct hpm_ehci_hcd {
    struct hpm_ehci_pipe chan[HCD_MAX_ENDPOINT][2];
} g_hpm_ehci_hcd;

static inline uint32_t hpm_ehci_align32(uint32_t value)
{
    return (value & 0xFFFFFFE0UL);
}

/*---------------------------------------------------------------------*
 * Enum Declaration
 *---------------------------------------------------------------------*/
typedef enum {
    hcd_int_mask_usb = HPM_BITSMASK(1, 0),
    hcd_int_mask_error = HPM_BITSMASK(1, 1),
    hcd_int_mask_port_change = HPM_BITSMASK(1, 2),

    hcd_int_mask_framelist_rollover = HPM_BITSMASK(1, 3),
    hcd_int_mask_pci_host_system_error = HPM_BITSMASK(1, 4),
    hcd_int_mask_async_advance = HPM_BITSMASK(1, 5),
    hcd_int_mask_sof = HPM_BITSMASK(1, 7),

    hcd_int_mask_async = HPM_BITSMASK(1, 18),
    hcd_int_mask_periodic = HPM_BITSMASK(1, 19),

    hcd_int_mask_all = hcd_int_mask_usb | hcd_int_mask_error | hcd_int_mask_port_change |
                       hcd_int_mask_framelist_rollover | hcd_int_mask_pci_host_system_error |
                       hcd_int_mask_async_advance | hcd_int_mask_sof |
                       hcd_int_mask_async | hcd_int_mask_periodic
} usb_interrupt_mask_t;
typedef struct
{
    USB_Type *regs;        /* register base */
    const uint32_t irqnum; /* IRQ number */
} hcd_controller_t;

static const hcd_controller_t _hcd_controller[] = {
    { .regs = (USB_Type *)HPM_USB0_BASE, .irqnum = IRQn_USB0 },
#ifdef HPM_USB1_BASE
    { .regs = (USB_Type *)HPM_USB1_BASE, .irqnum = IRQn_USB1 }
#endif
};

/*---------------------------------------------------------------------*
 * Variable Definitions
 *---------------------------------------------------------------------*/
ATTR_PLACE_AT_NONCACHEABLE static usb_host_handle_t usb_host_handle;
ATTR_PLACE_AT_NONCACHEABLE static bool hcd_int_sta;
// clang-format off
ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(USB_SOC_HCD_DATA_RAM_ADDRESS_ALIGNMENT) static hcd_data_t _hcd_data;
// clang-format on

bool hcd_init(uint8_t rhport)
{
    uint32_t int_mask;

    if (rhport > USB_SOC_MAX_COUNT) {
        return false;
    }

    usb_host_handle.rhport = rhport;
    usb_host_handle.regs = _hcd_controller[rhport].regs;
    usb_host_handle.hcd_data = &_hcd_data;
    usb_host_handle.hcd_vbus_ctrl_cb = board_usb_vbus_ctrl;

    int_mask = hcd_int_mask_error | hcd_int_mask_port_change | hcd_int_mask_async_advance |
               hcd_int_mask_periodic | hcd_int_mask_async | hcd_int_mask_framelist_rollover;

    usb_host_init(&usb_host_handle, int_mask, USB_HOST_FRAMELIST_SIZE);

    intc_m_enable_irq(_hcd_controller[rhport].irqnum);
    return true;
}

static int hpm_ehci_pipe_waitsetup(struct hpm_ehci_pipe *chan)
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
static int hpm_ehci_pipe_asynchsetup(struct hpm_ehci_pipe *chan, usbh_asynch_callback_t callback, void *arg)
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

static int hpm_ehci_pipe_wait(struct hpm_ehci_pipe *chan, uint32_t timeout)
{
    int ret;

    /* wait until timeout or sem give */
    if (chan->waiter) {
        ret = usb_osal_sem_take(chan->waitsem, timeout);
        if (ret < 0) {
            return ret;
        }
    }

    /* Sem give, check if giving from error isr */
    ret = chan->result;

    if (ret < 0) {
        return ret;
    }
    return chan->xfrd;
}

static void hpm_ehci_pipe_wakeup(struct hpm_ehci_pipe *chan)
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
#ifdef CONFIG_USB_DCACHE_ENABLE
        if (((chan->ep_addr & 0x80) == 0x80) && (nbytes > 0)) {
            l1c_dc_invalidate((uint32_t)chan->buffer, nbytes);
        }
#endif
        callback(arg, nbytes);
    }
#endif
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_sw_init(void)
{
    memset(&g_hpm_ehci_hcd, 0, sizeof(struct hpm_ehci_hcd));

    for (uint8_t chidx = 0; chidx < HCD_MAX_ENDPOINT; chidx++) {
        g_hpm_ehci_hcd.chan[chidx][0].exclsem = usb_osal_mutex_create();
        if (g_hpm_ehci_hcd.chan[chidx][0].exclsem == NULL) {
            printf("no memory to alloc mutex\r\n");
            return -ENOMEM;
        }
        g_hpm_ehci_hcd.chan[chidx][0].waitsem = usb_osal_sem_create(0);
        if (g_hpm_ehci_hcd.chan[chidx][0].exclsem == NULL) {
            printf("no memory to alloc sem\r\n");
            return -ENOMEM;
        }
        g_hpm_ehci_hcd.chan[chidx][1].exclsem = usb_osal_mutex_create();
        if (g_hpm_ehci_hcd.chan[chidx][0].exclsem == NULL) {
            printf("no memory to alloc mutex\r\n");
            return -ENOMEM;
        }
        g_hpm_ehci_hcd.chan[chidx][1].waitsem = usb_osal_sem_create(0);
        if (g_hpm_ehci_hcd.chan[chidx][0].exclsem == NULL) {
            printf("no memory to alloc sem\r\n");
            return -ENOMEM;
        }
    }

    return 0;
}

int usb_hc_hw_init(void)
{
    usb_hc_low_level_init();
    hcd_init(0);
    return 0;
}

int usbh_reset_port(const uint8_t port)
{
    usb_host_port_reset(&usb_host_handle);
    return 0;
}

bool usbh_get_port_connect_status(const uint8_t port)
{
    return usb_host_get_port_ccs(&usb_host_handle);
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    uint8_t speed = usb_host_get_port_speed(&usb_host_handle);
    if (speed == 0) {
        speed = USB_SPEED_FULL;
    } else if (speed == 1) {
        speed = USB_SPEED_LOW;
    } else if (speed == 2) {
        speed = USB_SPEED_HIGH;
    }
    return speed;
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    struct hpm_ehci_pipe *chan;
    int ret;
    usb_desc_endpoint_t ep_desc;

    chan = (struct hpm_ehci_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    if (speed == 1) {
        usb_host_handle.ep_speed = 1;
    } else if (speed == 2) {
        usb_host_handle.ep_speed = 0;
    } else if (speed == 3) {
        usb_host_handle.ep_speed = 2;
    }

    usb_host_handle.hub_addr = 0;
    usb_host_handle.hub_port = 0;

    ep_desc.bEndpointAddress = 0x00;
    ep_desc.bmAttributes.xfer = 0x00;
    ep_desc.wMaxPacketSize.size = ep_mps;

    chan->dev_addr = dev_addr;
    usb_host_edpt_open(&usb_host_handle, dev_addr, &ep_desc);

    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct hpm_ehci_pipe *chan;
    struct usbh_hubport *hport;
    uint8_t speed;
    usb_osal_sem_t waitsem;
    usb_osal_mutex_t exclsem;

    usb_desc_endpoint_t ep_desc;

    hport = ep_cfg->hport;

    if (ep_desc.bmAttributes.xfer == 0) {
        chan = &g_hpm_ehci_hcd.chan[0][0];
    } else {
        if (ep_cfg->ep_addr & 0x80) {
            chan = &g_hpm_ehci_hcd.chan[ep_cfg->ep_addr & 0x7f][1];
        } else {
            chan = &g_hpm_ehci_hcd.chan[ep_cfg->ep_addr & 0x7f][0];
        }
    }

    /* store variables */
    waitsem = chan->waitsem;
    exclsem = chan->exclsem;

    memset(chan, 0, sizeof(struct hpm_ehci_pipe));

    if (hport->speed == 1) {
        usb_host_handle.ep_speed = 1;
    } else if (hport->speed == 2) {
        usb_host_handle.ep_speed = 0;
    } else if (hport->speed == 3) {
        usb_host_handle.ep_speed = 2;
    }

    usb_host_handle.hub_addr = 0;
    usb_host_handle.hub_port = 0;

    ep_desc.bEndpointAddress = ep_cfg->ep_addr;
    ep_desc.bmAttributes.xfer = ep_cfg->ep_type;
    ep_desc.wMaxPacketSize.size = ep_cfg->ep_mps;

    chan->ep_addr = ep_cfg->ep_addr;
    chan->dev_addr = hport->dev_addr;

    usb_host_edpt_open(&usb_host_handle, hport->dev_addr, &ep_desc);

    /* restore variables */
    chan->waitsem = waitsem;
    chan->exclsem = exclsem;

    *ep = (usbh_epinfo_t)chan;
    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    struct hpm_ehci_pipe *chan;
    int ret;

    chan = (struct hpm_ehci_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(chan->exclsem);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    struct hpm_ehci_pipe *chan;
    int ret;

    chan = (struct hpm_ehci_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = hpm_ehci_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    l1c_dc_writeback((uint32_t)setup, 8);
#endif
    usb_host_setup_send(&usb_host_handle, chan->dev_addr, (uint8_t *)setup);
    ret = hpm_ehci_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
    if (ret < 0) {
        goto error_out;
    }

    if (setup->wLength && buffer) {
        if (setup->bmRequestType & 0x80) {
            chan->waiter = true;
            chan->result = -EBUSY;
            usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, 0x80, buffer, setup->wLength);
            ret = hpm_ehci_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
#ifdef CONFIG_USB_DCACHE_ENABLE
            l1c_dc_invalidate((uint32_t)buffer, setup->wLength);
#endif
            chan->waiter = true;
            chan->result = -EBUSY;
            usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, 0x00, NULL, 0);
            ret = hpm_ehci_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
        } else {
            chan->waiter = true;
            chan->result = -EBUSY;
            l1c_dc_writeback((uint32_t)buffer, setup->wLength);
            usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, 0x00, buffer, setup->wLength);
            ret = hpm_ehci_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }

            chan->waiter = true;
            chan->result = -EBUSY;
            usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, 0x00, NULL, 0);
            ret = hpm_ehci_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
        }
    } else {
        chan->waiter = true;
        chan->result = -EBUSY;
        usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, 0x80, NULL, 0);
        ret = hpm_ehci_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
        if (ret < 0) {
            goto error_out;
        }
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
    struct hpm_ehci_pipe *chan;
    int ret;

    chan = (struct hpm_ehci_pipe *)ep;

    if (buffer && (((uint32_t)buffer) & 0x1f)) {
        return -EINVAL;
    }

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = hpm_ehci_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x00) {
        l1c_dc_writeback((uint32_t)buffer, buflen);
    }
#endif
    chan->buffer = buffer;
    usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, chan->ep_addr, buffer, buflen);
    ret = hpm_ehci_pipe_wait(chan, timeout);
    if (ret < 0) {
        goto error_out;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x80) {
        l1c_dc_invalidate((uint32_t)buffer, buflen);
    }
#endif
    usb_osal_mutex_give(chan->exclsem);
    return ret;
error_out:
    chan->waiter = false;
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    return usbh_ep_bulk_transfer(ep, buffer, buflen, timeout);
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    struct hpm_ehci_pipe *chan;
    int ret;

    chan = (struct hpm_ehci_pipe *)ep;

    if (buffer && (((uint32_t)buffer) & 0x1f)) {
        return -EINVAL;
    }

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = hpm_ehci_pipe_asynchsetup(chan, callback, arg);
    if (ret < 0) {
        goto error_out;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x00) {
        l1c_dc_writeback((uint32_t)buffer, buflen);
    }
#endif
    usb_host_edpt_xfer(&usb_host_handle, chan->dev_addr, chan->ep_addr, buffer, buflen);

error_out:
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    return usbh_ep_bulk_async_transfer(ep, buffer, buflen, callback, arg);
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    struct hpm_ehci_pipe *chan;
    int ret;
    size_t flags;
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback;
    void *arg;
#endif

    chan = (struct hpm_ehci_pipe *)ep;

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

/*---------------------------------------------------------------------*
 * HCD Interrupt Handler
 *---------------------------------------------------------------------*/
/* async_advance is handshake between usb stack & ehci controller.
 * This isr mean it is safe to modify previously removed queue head from async list.
 * In tinyusb, queue head is only removed when device is unplugged.
 */
static void async_advance_isr(usb_host_handle_t *handle)
{
    hcd_qhd_t *qhd_pool = handle->hcd_data->qhd_pool;

    for (uint32_t i = 0; i < HCD_MAX_ENDPOINT; i++) {
        if (qhd_pool[i].removing) {
            qhd_pool[i].removing = 0;
            qhd_pool[i].used = 0;
        }
    }
}

static void port_connect_status_change_isr(usb_host_handle_t *handle)
{
    struct hpm_ehci_pipe *chan;
    /* NOTE There is an sequence plug->unplug->…..-> plug if device is powering with pre-plugged device */
    if (usb_host_get_port_ccs(handle)) {
        usbh_event_notify_handler(USBH_EVENT_CONNECTED, 1);
    } else { /* device unplugged */
        for (uint8_t chidx = 0; chidx < HCD_MAX_ENDPOINT; chidx++) {
            for (uint8_t j = 0; j < 2; j++) {
                chan = &g_hpm_ehci_hcd.chan[chidx][j];
                if (chan->waiter) {
                    chan->result = -ENXIO;
                    hpm_ehci_pipe_wakeup(chan);
                }
            }
        }
        usb_host_device_close(&usb_host_handle, 1);
        usbh_event_notify_handler(USBH_EVENT_DISCONNECTED, 1);
    }
}

static void qhd_xfer_complete_isr(hcd_qhd_t *p_qhd)
{
    struct hpm_ehci_pipe *chan;
    bool is_ioc;

    while (p_qhd->p_qtd_list_head != NULL && !p_qhd->p_qtd_list_head->active) {
        /* TD need to be freed and removed from qhd, before invoking callback */
        is_ioc = (p_qhd->p_qtd_list_head->int_on_complete != 0);
        p_qhd->total_xferred_bytes += p_qhd->p_qtd_list_head->expected_bytes - p_qhd->p_qtd_list_head->total_bytes;

        p_qhd->p_qtd_list_head->used = 0; /* free QTD */
        usb_host_qtd_remove_1st_from_qhd(p_qhd);

        if (is_ioc) {
            if (p_qhd->ep_number == 0) {
                chan = &g_hpm_ehci_hcd.chan[0][0];
            } else {
                if (p_qhd->qtd_overlay.pid == usb_pid_in) {
                    chan = &g_hpm_ehci_hcd.chan[p_qhd->ep_number][1];
                } else {
                    chan = &g_hpm_ehci_hcd.chan[p_qhd->ep_number][0];
                }
            }

            chan->xfrd += p_qhd->total_xferred_bytes;
            chan->result = 0;

            p_qhd->total_xferred_bytes = 0;
            hpm_ehci_pipe_wakeup(chan);
        }
    }
}

static void async_list_xfer_complete_isr(hcd_qhd_t *const async_head)
{
    hcd_qhd_t *p_qhd = async_head;

    do {
        if (!p_qhd->qtd_overlay.halted) { /* halted or error is processed in error isr */
            qhd_xfer_complete_isr(p_qhd);
        }

        p_qhd = usb_host_qhd_next(p_qhd);
        p_qhd = (hcd_qhd_t *)sys_address_to_core_local_mem(USB_HOST_MCU_CORE, (uint32_t)p_qhd);

    } while (p_qhd != async_head); /* async list traversal, stop if loop around */
}

static void period_list_xfer_complete_isr(usb_host_handle_t *handle, uint8_t interval_ms)
{
    uint16_t max_loop = 0;
    uint32_t const period_1ms_addr = (uint32_t)usb_host_get_period_head(handle, 1);
    hcd_link_t next_item = *usb_host_get_period_head(handle, interval_ms);
    hcd_qhd_t *p_qhd_int;

    /* TODO abstract max loop guard for period */
    while (!next_item.terminate &&
           !(interval_ms > 1 && period_1ms_addr == hpm_ehci_align32(next_item.address)) &&
           max_loop < (HCD_MAX_ENDPOINT + usb_max_itd + usb_max_sitd) * 1) {
        switch (next_item.type) {
            case usb_qtype_qhd:
                p_qhd_int = (hcd_qhd_t *)hpm_ehci_align32(next_item.address);
                if (!p_qhd_int->qtd_overlay.halted) {
                    qhd_xfer_complete_isr(p_qhd_int);
                }

                break;

            case usb_qtype_itd:
            case usb_qtype_sitd:
            case usb_qtype_fstn:

            default:
                break;
        }

        next_item = *usb_host_list_next(&next_item);
        max_loop++;
    }
}

static void qhd_xfer_error_isr(usb_host_handle_t *handle, hcd_qhd_t *p_qhd)
{
    struct hpm_ehci_pipe *chan;
    hcd_qtd_t *p_setup;

    if ((p_qhd->dev_addr != 0 && p_qhd->qtd_overlay.halted) || /* addr0 cannot be protocol STALL */
        usb_host_qhd_has_xact_error(p_qhd)) {
        /* no error bits are set, endpoint is halted due to STALL */
        p_qhd->total_xferred_bytes += p_qhd->p_qtd_list_head->expected_bytes - p_qhd->p_qtd_list_head->total_bytes;

        if (p_qhd->ep_number == 0) {
            chan = &g_hpm_ehci_hcd.chan[0][0];
        } else {
            if (p_qhd->qtd_overlay.pid == usb_pid_in) {
                chan = &g_hpm_ehci_hcd.chan[p_qhd->ep_number][1];
            } else {
                chan = &g_hpm_ehci_hcd.chan[p_qhd->ep_number][0];
            }
        }

        if (p_qhd->qtd_overlay.babble_err) {
            chan->result = -EPERM;
        }
        if (p_qhd->qtd_overlay.xact_err) {
            chan->result = -EIO;
        }
        if (p_qhd->qtd_overlay.buffer_err) {
            chan->result = -EIO;
        }

        /* TODO skip unplugged device */

        p_qhd->p_qtd_list_head->used = 0; /* free QTD */
        usb_host_qtd_remove_1st_from_qhd(p_qhd);

        if (0 == p_qhd->ep_number) {
            /* control cannot be halted --> clear all qtd list */
            p_qhd->p_qtd_list_head = NULL;
            p_qhd->p_qtd_list_tail = NULL;

            p_qhd->qtd_overlay.next.terminate = 1;
            p_qhd->qtd_overlay.alternate.terminate = 1;
            p_qhd->qtd_overlay.halted = 0;

            p_setup = usb_host_qtd_control(handle, p_qhd->dev_addr);
            p_setup->used = 0;
        }

        p_qhd->total_xferred_bytes = 0;
        hpm_ehci_pipe_wakeup(chan);
    }
}

static void xfer_error_isr(usb_host_handle_t *handle)
{
    hcd_qhd_t *const async_head = usb_host_qhd_async_head(handle);
    hcd_qhd_t *p_qhd = async_head;
    hcd_qhd_t *p_qhd_int;
    hcd_link_t next_item, *p;

    /*------------- async list -------------*/
    do {
        qhd_xfer_error_isr(handle, p_qhd);
        p_qhd = usb_host_qhd_next(p_qhd);
        p_qhd = (hcd_qhd_t *)sys_address_to_core_local_mem(USB_HOST_MCU_CORE, (uint32_t)p_qhd);
    } while (p_qhd != async_head); /* async list traversal, stop if loop around */

    /*------------- TODO refractor period list -------------*/
    uint32_t const period_1ms_addr = (uint32_t)usb_host_get_period_head(handle, 1);
    for (uint8_t interval_ms = 1; interval_ms <= USB_HOST_FRAMELIST_SIZE; interval_ms *= 2) {
        next_item = *usb_host_get_period_head(handle, interval_ms);

        /* TODO abstract max loop guard for period */
        while (!next_item.terminate &&
               !(interval_ms > 1 && period_1ms_addr == hpm_ehci_align32(next_item.address))) {
            switch (next_item.type) {
                case usb_qtype_qhd:
                    p_qhd_int = (hcd_qhd_t *)hpm_ehci_align32(next_item.address);
                    qhd_xfer_error_isr(handle, p_qhd_int);
                    break;

                case usb_qtype_itd:
                case usb_qtype_sitd:
                case usb_qtype_fstn:

                default:
                    break;
            }

            p = usb_host_list_next(&next_item);
            p = (hcd_link_t *)sys_address_to_core_local_mem(USB_HOST_MCU_CORE, (uint32_t)p);
            next_item = *p;
        }
    }
}

/*------------- Host Controller Driver's Interrupt Handler -------------*/
void hcd_int_handler(uint8_t rhport)
{
    uint32_t status;
    usb_host_handle_t *handle = &usb_host_handle;

    /* Acknowledge handled interrupt */
    status = usb_host_status_flags(handle);
    status &= usb_host_interrupts(handle);
    usb_host_clear_status_flags(handle, status);

    if (status == 0) {
        return;
    }

    if (status & hcd_int_mask_framelist_rollover) {
        handle->hcd_data->uframe_number += (USB_HOST_FRAMELIST_SIZE << 3);
    }

    if (status & hcd_int_mask_port_change) {
        if (usb_host_port_csc(handle)) {
            port_connect_status_change_isr(handle);
        }
    }

    if (status & hcd_int_mask_error) {
        xfer_error_isr(handle);
    }

    /*------------- some QTD/SITD/ITD with IOC set is completed -------------*/
    if (status & hcd_int_mask_async) {
        async_list_xfer_complete_isr(usb_host_qhd_async_head(handle));
    }

    if (status & hcd_int_mask_periodic) {
        for (uint8_t i = 1; i <= USB_HOST_FRAMELIST_SIZE; i *= 2) {
            period_list_xfer_complete_isr(handle, i);
        }
    }

    /*------------- There is some removed async previously -------------*/
    if (status & hcd_int_mask_async_advance) { /* need to place after EHCI_INT_MASK_ASYNC */
        async_advance_isr(handle);
    }
}

void isr_usb0(void)
{
    hcd_int_handler(0);
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB0, isr_usb0)

#ifdef HPM_USB1_BASE
void isr_usb1(void)
{
    hcd_int_handler(1);
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB1, isr_usb1)
#endif