#include "usbh_core.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler OTG_FS_IRQHandler
#endif

struct xxx_hcd {
    volatile bool connected; /* Connected to device */
    volatile bool pscwait;   /* True: Thread is waiting for a port event */
    usb_osal_sem_t exclsem;  /* Support mutually exclusive access */
} g_xxx_hcd;

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_sw_init(void)
{
    memset(&g_usbhost, 0, sizeof(struct xxx_hcd));

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

uint8_t usbh_get_port_speed(const uint8_t port)
{
    return USB_SPEED_FULL;
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    int ret;
    struct usbh_hubport *hport;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    hport = ep_cfg->hport;

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
    } else {
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

    usb_osal_mutex_give(g_usbhost.exclsem);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);

    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;

    ret = usb_osal_mutex_take(g_usbhost.exclsem);
    if (ret < 0) {
        return ret;
    }

    usb_osal_mutex_give(g_usbhost.exclsem);

    return ret;
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    return 0;
}

void USBH_IRQHandler(void)
{
}