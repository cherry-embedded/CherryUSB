#include "usbd_core.h"
#include "hpm_usb_device.h"
#include "board.h"

/* USBSTS, USBINTR */
enum {
    intr_usb = HPM_BITSMASK(1, 0),
    intr_error = HPM_BITSMASK(1, 1),
    intr_port_change = HPM_BITSMASK(1, 2),
    intr_reset = HPM_BITSMASK(1, 6),
    intr_sof = HPM_BITSMASK(1, 7),
    intr_suspend = HPM_BITSMASK(1, 8),
    intr_nak = HPM_BITSMASK(1, 16)
};

/* Endpoint state */
struct hpm_ep {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct hpm_udc {
    usb_device_handle_t *handle;
    struct hpm_ep in_ep[USB_SOC_DCD_MAX_ENDPOINT_COUNT];
    struct hpm_ep out_ep[USB_SOC_DCD_MAX_ENDPOINT_COUNT];
} g_hpm_udc[USB_SOC_MAX_COUNT];

static ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(USB_SOC_DCD_DATA_RAM_ADDRESS_ALIGNMENT) dcd_data_t _dcd_data0;
#if defined(HPM_USB1_BASE)
static ATTR_PLACE_AT_NONCACHEABLE_WITH_ALIGNMENT(USB_SOC_DCD_DATA_RAM_ADDRESS_ALIGNMENT) dcd_data_t _dcd_data1;
#endif
static ATTR_PLACE_AT_NONCACHEABLE usb_device_handle_t usb_device_handle[USB_SOC_MAX_COUNT];

#if defined(HPM_USB0_BASE) && defined(HPM_USB1_BASE)
const uint8_t hpm_irq_num[] = { IRQn_USB0, IRQn_USB1 };
#else
const uint8_t hpm_irq_num[] = { IRQn_USB0 };
#endif
/* Index to bit position in register */
static inline uint8_t ep_idx2bit(uint8_t ep_idx)
{
    return ep_idx / 2 + ((ep_idx % 2) ? 16 : 0);
}

int hpm_udc_init(struct usbd_bus *bus)
{
    struct hpm_udc *udc;

    if (bus->busid > (USB_SOC_MAX_COUNT - 1)) {
        return -1;
    }
    //usbd_udc_low_level_init(bus->busid);

    USB_LOG_INFO("========== hpm udc params =========\r\n");
    USB_LOG_INFO("hpm has %d endpoints\r\n", USB_SOC_DCD_MAX_ENDPOINT_COUNT);
    USB_LOG_INFO("=================================\r\n");

    udc = &g_hpm_udc[bus->busid];
    bus->udc = udc;
    bus->endpoints = USB_SOC_DCD_MAX_ENDPOINT_COUNT;
    memset(udc, 0, sizeof(struct hpm_udc));

    udc->handle = &usb_device_handle[bus->busid];
    udc->handle->regs = (USB_Type *)bus->reg_base;
    if (bus->busid == 0) {
        udc->handle->dcd_data = &_dcd_data0;
    } else {
        udc->handle->dcd_data = &_dcd_data1;
    }

    uint32_t int_mask;
    int_mask = (USB_USBINTR_UE_MASK | USB_USBINTR_UEE_MASK |
                USB_USBINTR_PCE_MASK | USB_USBINTR_URE_MASK);

    usb_device_init(udc->handle, int_mask);

    intc_m_enable_irq(hpm_irq_num[bus->busid]);
    return 0;
}

int hpm_udc_deinit(struct usbd_bus *bus)
{
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    //usbd_udc_low_level_deinit(bus->busid);

    return 0;
}

int hpm_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    usb_dcd_set_address(udc->handle->regs, addr);
    return 0;
}

uint8_t hpm_get_port_speed(struct usbd_bus *bus)
{
    uint8_t speed;
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    speed = usb_get_port_speed(udc->handle->regs);

    if (speed == 0x00) {
        return USB_SPEED_FULL;
    }
    if (speed == 0x01) {
        return USB_SPEED_LOW;
    }
    if (speed == 0x02) {
        return USB_SPEED_HIGH;
    }

    return 0;
}

int hpm_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    usb_endpoint_config_t tmp_ep_cfg;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (ep_idx > (USB_SOC_DCD_MAX_ENDPOINT_COUNT - 1)) {
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

    tmp_ep_cfg.xfer = ep_type;
    tmp_ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    tmp_ep_cfg.max_packet_size = ep_mps;

    usb_device_edpt_open(udc->handle, &tmp_ep_cfg);
    return 0;
}

int hpm_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    usb_device_edpt_close(udc->handle, ep);
    return 0;
}

int hpm_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    usb_device_edpt_stall(udc->handle, ep);
    return 0;
}

int hpm_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    usb_device_edpt_clear_stall(udc->handle, ep);
    return 0;
}

int hpm_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    return 0;
}

int hpm_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct hpm_udc *udc = bus->udc;

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

    usb_device_edpt_xfer(udc->handle, ep, (uint8_t *)data, data_len);

    return 0;
}

int hpm_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct hpm_udc *udc = bus->udc;

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

    usb_device_edpt_xfer(udc->handle, ep, data, data_len);

    return 0;
}

void hpm_udc_irq(struct usbd_bus *bus)
{
    uint32_t int_status;
    usb_device_handle_t *handle;
    uint32_t transfer_len;
    struct hpm_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    handle = udc->handle;
    /* Acknowledge handled interrupt */
    int_status = usb_device_status_flags(handle);
    int_status &= usb_device_interrupts(handle);
    usb_device_clear_status_flags(handle, int_status);

    /* disabled interrupt sources */
    if (int_status == 0) {
        return;
    }

    if (int_status & intr_reset) {
        memset(g_hpm_udc[bus->busid].in_ep, 0, sizeof(struct hpm_ep) * USB_SOC_DCD_MAX_ENDPOINT_COUNT);
        memset(g_hpm_udc[bus->busid].out_ep, 0, sizeof(struct hpm_ep) * USB_SOC_DCD_MAX_ENDPOINT_COUNT);
        usbd_event_reset_handler(bus->busid);
        usb_device_bus_reset(handle, 64);
    }

    if (int_status & intr_suspend) {
        if (usb_device_get_suspend_status(handle)) {
            /* Note: Host may delay more than 3 ms before and/or after bus reset
       * before doing enumeration. */
            if (usb_device_get_address(handle)) {
            }
        }
    }

    if (int_status & intr_port_change) {
        if (!usb_device_get_port_ccs(handle)) {
        } else {
            if (usb_device_get_port_reset_status(handle) == 0) {
            }
        }
    }

    if (int_status & intr_usb) {
        uint32_t const edpt_complete = usb_device_get_edpt_complete_status(handle);
        usb_device_clear_edpt_complete_status(handle, edpt_complete);
        uint32_t edpt_setup_status = usb_device_get_setup_status(handle);

        if (edpt_setup_status) {
            /*------------- Set up Received -------------*/
            usb_device_clear_setup_status(handle, edpt_setup_status);
            dcd_qhd_t *qhd0 = usb_device_qhd_get(handle, 0);
            usbd_event_ep0_setup_complete_handler(bus->busid, (uint8_t *)&qhd0->setup_request);
        }

        if (edpt_complete) {
            for (uint8_t ep_idx = 0; ep_idx < USB_SOS_DCD_MAX_QHD_COUNT; ep_idx++) {
                if (edpt_complete & (1 << ep_idx2bit(ep_idx))) {
                    /* Failed QTD also get ENDPTCOMPLETE set */
                    dcd_qtd_t *p_qtd = usb_device_qtd_get(handle, ep_idx);

                    if (p_qtd->halted || p_qtd->xact_err || p_qtd->buffer_err) {
                    } else {
                        /* only number of bytes in the IOC qtd */
                        uint8_t const ep_addr = (ep_idx / 2) | ((ep_idx & 0x01) ? 0x80 : 0);

                        transfer_len = p_qtd->expected_bytes - p_qtd->total_bytes;

                        if (ep_addr & 0x80) {
                            usbd_event_ep_in_complete_handler(bus->busid, ep_addr, transfer_len);
                        } else {
                            usbd_event_ep_out_complete_handler(bus->busid, ep_addr, transfer_len);
                        }
                    }
                }
            }
        }
    }
}

struct usbd_udc_driver hpm_udc_driver = {
    .driver_name = "hpm udc",
    .udc_init = hpm_udc_init,
    .udc_deinit = hpm_udc_deinit,
    .udc_set_address = hpm_set_address,
    .udc_get_port_speed = hpm_get_port_speed,
    .udc_ep_open = hpm_ep_open,
    .udc_ep_close = hpm_ep_close,
    .udc_ep_set_stall = hpm_ep_set_stall,
    .udc_ep_clear_stall = hpm_ep_clear_stall,
    .udc_ep_is_stalled = hpm_ep_is_stalled,
    .udc_ep_start_write = hpm_ep_start_write,
    .udc_ep_start_read = hpm_ep_start_read,
    .udc_irq = hpm_udc_irq
};

#ifdef HPM_USB0_BASE
void isr_usb0(void)
{
    usbd_irq(0);
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB0, isr_usb0)
#endif

#ifdef HPM_USB1_BASE
void isr_usb1(void)
{
    usbd_irq(1);
}
SDK_DECLARE_EXT_ISR_M(IRQn_USB1, isr_usb1)
#endif