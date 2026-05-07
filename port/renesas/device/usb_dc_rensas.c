/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "r_usb_device.h"

static void udc_renesas_ra_event_handler(usbd_callback_arg_t *p_args);

usbd_instance_ctrl_t g_usbd_ctrl[CONFIG_USBDEV_MAX_BUS];
usbd_cfg_t g_usbd_cfg[CONFIG_USBDEV_MAX_BUS] = {
    {
        .module_number = 0,
        .usb_speed = USBD_SPEED_FS,
#if defined(VECTOR_NUMBER_USBFS_INT)
        .irq = VECTOR_NUMBER_USBFS_INT,
#else
        .irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_RESUME)
        .irq_r = VECTOR_NUMBER_USBFS_RESUME,
#else
        .irq_r = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_FIFO_0)
        .irq_d0 = VECTOR_NUMBER_USBFS_FIFO_0,
#else
        .irq_d0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_FIFO_1)
        .irq_d1 = VECTOR_NUMBER_USBFS_FIFO_1,
#else
        .irq_d1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_USB_INT_RESUME)
        .hs_irq = VECTOR_NUMBER_USBHS_USB_INT_RESUME,
#else
        .hs_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_FIFO_0)
        .hsirq_d0 = VECTOR_NUMBER_USBHS_FIFO_0,
#else
        .hsirq_d0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_FIFO_1)
        .hsirq_d1 = VECTOR_NUMBER_USBHS_FIFO_1,
#else
        .hsirq_d1 = FSP_INVALID_VECTOR,
#endif
        .ipl = (12),
        .ipl_r = (12),
        .ipl_d0 = (12),
        .ipl_d1 = (12),
        .hsipl = (12),
        .hsipl_d0 = (12),
        .hsipl_d1 = (12),
        .p_callback = udc_renesas_ra_event_handler,
        .p_context = NULL,
    },
#if CONFIG_USBDEV_MAX_BUS > 1
    {
        .module_number = 1,
        .usb_speed = USBD_SPEED_HS,
#if defined(VECTOR_NUMBER_USBFS_INT)
        .irq = VECTOR_NUMBER_USBFS_INT,
#else
        .irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_RESUME)
        .irq_r = VECTOR_NUMBER_USBFS_RESUME,
#else
        .irq_r = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_FIFO_0)
        .irq_d0 = VECTOR_NUMBER_USBFS_FIFO_0,
#else
        .irq_d0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBFS_FIFO_1)
        .irq_d1 = VECTOR_NUMBER_USBFS_FIFO_1,
#else
        .irq_d1 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_USB_INT_RESUME)
        .hs_irq = VECTOR_NUMBER_USBHS_USB_INT_RESUME,
#else
        .hs_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_FIFO_0)
        .hsirq_d0 = VECTOR_NUMBER_USBHS_FIFO_0,
#else
        .hsirq_d0 = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_USBHS_FIFO_1)
        .hsirq_d1 = VECTOR_NUMBER_USBHS_FIFO_1,
#else
        .hsirq_d1 = FSP_INVALID_VECTOR,
#endif
        .ipl = (12),
        .ipl_r = (12),
        .ipl_d0 = (12),
        .ipl_d1 = (12),
        .hsipl = (12),
        .hsipl_d0 = (12),
        .hsipl_d1 = (12),
        .p_callback = udc_renesas_ra_event_handler,
        .p_context = NULL,
    },
#endif

};

uint8_t g_usbd_speed[CONFIG_USBDEV_MAX_BUS];
uint8_t g_usbd_is_stalled[CONFIG_USBDEV_MAX_BUS][2][16];

int usb_dc_init(uint8_t busid)
{
    g_usbd_speed[busid] = USB_SPEED_UNKNOWN;
    R_USBD_Open(&g_usbd_ctrl[busid], &g_usbd_cfg[busid]);
    R_USBD_Connect(&g_usbd_ctrl[busid]);
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    R_USBD_Disconnect(&g_usbd_ctrl[busid]);
    R_USBD_Close(&g_usbd_ctrl[busid]);
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    R_USBD_RemoteWakeup(&g_usbd_ctrl[busid]);
    return 0;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    return g_usbd_speed[busid];
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    if ((ep->bEndpointAddress & 0x0f) == 0) {
        return 0;
    }

    return R_USBD_EdptOpen(&g_usbd_ctrl[busid], (usbd_desc_endpoint_t *)ep) == FSP_SUCCESS ? 0 : -1;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    R_USBD_EdptClose(&g_usbd_ctrl[busid], ep);
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        g_usbd_is_stalled[busid][0][ep_idx] = 1;
    } else {
        g_usbd_is_stalled[busid][1][ep_idx] = 1;
    }
    R_USBD_EdptStall(&g_usbd_ctrl[busid], ep);
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        g_usbd_is_stalled[busid][0][ep_idx] = 0;
    } else {
        g_usbd_is_stalled[busid][1][ep_idx] = 0;
    }
    R_USBD_EdptClearStall(&g_usbd_ctrl[busid], ep);
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        *stalled = g_usbd_is_stalled[busid][0][ep_idx];
    } else {
        *stalled = g_usbd_is_stalled[busid][1][ep_idx];
    }
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data,
                        uint32_t data_len)
{
    R_USBD_XferStart(&g_usbd_ctrl[busid], ep, (uint8_t *)data, data_len);
    if (data_len == 0) {
        usbd_event_ep_in_complete_handler(busid, ep, 0);
        return 0;
    }
    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data,
                       uint32_t data_len)
{
    if (data_len == 0) {
        usbd_event_ep_out_complete_handler(busid, ep, 0);
        return 0;
    }

    R_USBD_XferStart(&g_usbd_ctrl[busid], ep, data, data_len);
    return 0;
}

void udc_renesas_ra_event_handler(usbd_callback_arg_t *p_args)
{
    switch (p_args->event.event_id) {
        case R_USBD_EVENT_BUS_RESET:
            g_usbd_speed[p_args->module_number] = p_args->event.bus_reset.speed + 1;
            usbd_event_reset_handler(p_args->module_number);
            break;
        case R_USBD_EVENT_VBUS_RDY:
            break;
        case R_USBD_EVENT_VBUS_REMOVED:
            break;
        case R_USBD_EVENT_SOF:
            break;
        case R_USBD_EVENT_SUSPEND:
            usbd_event_suspend_handler(p_args->module_number);
            break;
        case R_USBD_EVENT_RESUME:
            usbd_event_resume_handler(p_args->module_number);
            break;
        case R_USBD_EVENT_SETUP_RECEIVED:
            usbd_event_ep0_setup_complete_handler(p_args->module_number, (uint8_t *)&p_args->event.setup_received);
            break;
        case R_USBD_EVENT_XFER_COMPLETE:
            if (p_args->event.xfer_complete.result == USBD_XFER_RESULT_SUCCESS) {
                if (USB_EP_DIR_IS_IN(p_args->event.xfer_complete.ep_addr)) {
                    usbd_event_ep_in_complete_handler(p_args->module_number, p_args->event.xfer_complete.ep_addr, p_args->event.xfer_complete.len);
                } else {
                    usbd_event_ep_out_complete_handler(p_args->module_number, p_args->event.xfer_complete.ep_addr, p_args->event.xfer_complete.len);
                }
            }
            break;
        default:
            break;
    }
}

extern void usb_device_isr(void);

void usbfs_interrupt_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);
    usb_device_isr();

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

void usbfs_resume_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

void usbfs_d0fifo_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

void usbfs_d1fifo_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

void usbhs_interrupt_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);
    usb_device_isr();
    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

void usbhs_d0fifo_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}

void usbhs_d1fifo_handler(void)
{
    /* Save context if RTOS is used */
    FSP_CONTEXT_SAVE

    IRQn_Type irq = R_FSP_CurrentIrqGet();
    R_BSP_IrqStatusClear(irq);

    /* Restore context if RTOS is used */
    FSP_CONTEXT_RESTORE
}
