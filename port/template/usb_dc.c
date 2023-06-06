#include "usbd_core.h"

/* Endpoint state */
struct xxx_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct xxx_udc {
    struct xxx_ep_state in_ep[8];
    struct xxx_ep_state out_ep[8];
} g_xxx_udc[2];

int xxx_udc_init(struct usbd_bus *bus)
{
    struct xxx_udc *udc;

    //usbd_udc_low_level_init(bus->busid);

    udc = &g_xxx_udc[bus->busid];
    bus->udc = udc;
    memset(udc, 0, sizeof(struct xxx_udc));

    return 0;
}

int xxx_udc_deinit(struct usbd_bus *bus)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    //usbd_udc_low_level_deinit(bus->busid);

    return 0;
}

int xxx_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return 0;
}

uint8_t xxx_get_port_speed(struct usbd_bus *bus)
{
    uint8_t speed;
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return USB_SPEED_FULL;
}

int xxx_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (ep_idx > (bus->endpoints - 1)) {
        USB_LOG_ERR("Ep addr 0x%02x overflow\r\n", ep_desc->bEndpointAddress);
        return -2;
    }

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        udc->out_ep[ep_idx].ep_mps = ep_mps;
        udc->out_ep[ep_idx].ep_type = ep_type;
        udc->out_ep[ep_idx].ep_enable = true;
    } else {
        udc->in_ep[ep_idx].ep_mps = ep_mps;
        udc->in_ep[ep_idx].ep_type = ep_type;
        udc->in_ep[ep_idx].ep_enable = true;
    }

    return 0;
}

int xxx_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return 0;
}

int xxx_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return 0;
}

int xxx_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return 0;
}

int xxx_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return 0;
}

int xxx_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }
    if (!udc->in_ep[ep_idx].ep_enable) {
        return -3;
    }

    udc->in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    udc->in_ep[ep_idx].xfer_len = data_len;
    udc->in_ep[ep_idx].actual_xfer_len = 0;

    return 0;
}

int xxx_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }
    if (!udc->out_ep[ep_idx].ep_enable) {
        return -3;
    }

    udc->out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    udc->out_ep[ep_idx].xfer_len = data_len;
    udc->out_ep[ep_idx].actual_xfer_len = 0;

    return 0;
}

void xxx_udc_irq(struct usbd_bus *bus)
{
    struct xxx_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }
}

struct usbd_udc_driver xxx_udc_driver = {
    .driver_name = "xxx udc",
    .udc_init = xxx_udc_init,
    .udc_deinit = xxx_udc_deinit,
    .udc_set_address = xxx_set_address,
    .udc_get_port_speed = xxx_get_port_speed,
    .udc_ep_open = xxx_ep_open,
    .udc_ep_close = xxx_ep_close,
    .udc_ep_set_stall = xxx_ep_set_stall,
    .udc_ep_clear_stall = xxx_ep_clear_stall,
    .udc_ep_is_stalled = xxx_ep_is_stalled,
    .udc_ep_start_write = xxx_ep_start_write,
    .udc_ep_start_read = xxx_ep_start_read,
    .udc_irq = xxx_udc_irq
};