#include "usbd_core.h"
#include "usbd_udc.h"

struct usbd_bus g_usbd_bus[CONFIG_USBDEV_MAX_BUS] = { 0 };

void usbd_bus_add_udc(uint8_t busid, uint32_t reg_base, struct usbd_udc_driver *driver, void *param)
{
    if (busid > CONFIG_USBDEV_MAX_BUS) {
        USB_LOG_ERR("Bus overflow\r\n");
        while (1) {
        }
    }

    memset(&g_usbd_bus[busid], 0, sizeof(struct usbd_bus));
    g_usbd_bus[busid].busid = busid;
    g_usbd_bus[busid].reg_base = reg_base;
    g_usbd_bus[busid].driver = driver;
    g_usbd_bus[busid].udc_param = param;
}

int usbd_udc_init(uint8_t busid)
{
    if (g_usbd_bus[busid].driver == NULL) {
        USB_LOG_ERR("No driver on bus %d, please call usbd_bus_add_udc first\r\n", busid);
        while (1) {
        }
    }
    return g_usbd_bus[busid].driver->udc_init(&g_usbd_bus[busid]);
}

int usbd_udc_deinit(uint8_t busid)
{
    return g_usbd_bus[busid].driver->udc_deinit(&g_usbd_bus[busid]);
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    return g_usbd_bus[busid].driver->udc_set_address(&g_usbd_bus[busid], addr);
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    return g_usbd_bus[busid].driver->udc_get_port_speed(&g_usbd_bus[busid]);
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep_desc)
{
    return g_usbd_bus[busid].driver->udc_ep_open(&g_usbd_bus[busid], ep_desc);
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    return g_usbd_bus[busid].driver->udc_ep_close(&g_usbd_bus[busid], ep);
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    return g_usbd_bus[busid].driver->udc_ep_set_stall(&g_usbd_bus[busid], ep);
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    return g_usbd_bus[busid].driver->udc_ep_clear_stall(&g_usbd_bus[busid], ep);
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    return g_usbd_bus[busid].driver->udc_ep_is_stalled(&g_usbd_bus[busid], ep, stalled);
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    return g_usbd_bus[busid].driver->udc_ep_start_write(&g_usbd_bus[busid], ep, data, data_len);
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    return g_usbd_bus[busid].driver->udc_ep_start_read(&g_usbd_bus[busid], ep, data, data_len);
}

void usbd_irq(uint8_t busid)
{
    g_usbd_bus[busid].driver->udc_irq(&g_usbd_bus[busid]);
}