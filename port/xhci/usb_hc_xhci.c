/*
 * Copyright : (C) 2022 Phytium Information Technology, Inc.
 * All Rights Reserved.
 *
 * This program is OPEN SOURCE software: you can redistribute it and/or modify it
 * under the terms of the Phytium Public License as published by the Phytium Technology Co.,Ltd,
 * either version 1.0 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the Phytium Public License for more details.
 *
 *
 * FilePath: usb_hc_xhci.c
 * Date: 2022-09-19 17:24:36
 * LastEditTime: 2022-09-19 17:24:36
 * Description:  This file is for xhci function implementation.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 * 1.1   zhugengyu  2022/10/9   add dumpers and fix command abort
 * 2.0   zhugengyu  2023/3/29   support usb3.0 device attached at roothub
 */

/***************************** Include Files *********************************/
#include "usbh_hub.h"

#include "xhci.h"

/************************** Constant Definitions *****************************/

/************************** Variable Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/*****************************************************************************/
__WEAK void usb_hc_low_level_init(void)
{
}

__WEAK void *usb_hc_malloc(size_t size)
{
    return NULL;
}

__WEAK void *usb_hc_malloc_align(size_t align, size_t size)
{
    return NULL;
}

__WEAK void usb_hc_free()
{
}

/* one may get xhci register base address by PCIe bus emuration */
__WEAK unsigned long usb_hc_get_register_base(void)
{
    return 0U;
}

/**
 * Get USB root hub port
 *
 * @v hport		Hub port of USB device
 * @ret port	Root hub port
 */
extern struct usbh_hubport *usbh_root_hub_port(struct usbh_hubport *hport);

static struct xhci_host xhci_host;

/* xhci hardware init */
int usb_hc_init()
{
    int rc = 0;
    struct xhci_host *xhci = &(xhci_host);

    size_t flag = usb_osal_enter_critical_section(); /* no interrupt when init hc */

    usb_hc_low_level_init(); /* set gic and memp */

    memset(xhci, 0, sizeof(*xhci));
    xhci->id = CONFIG_USBHOST_XHCI_ID;
    if (rc = xhci_probe(xhci, usb_hc_get_register_base()) != 0) {
        goto err_open; 
    }

    if (rc = xhci_open(xhci) != 0 ) {
        goto err_open;
    }

 err_open:
    usb_osal_leave_critical_section(flag);
	return rc;
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
    uint8_t nports;
    uint8_t port;
    uint32_t portsc;
    uint32_t status;
    int ret = 0;
    struct xhci_host *xhci = &xhci_host;
    nports = CONFIG_USBHOST_MAX_RHPORTS;

    port = setup->wIndex;

	/*  
		bmRequestType bit[4:0], define whether the request is directed to the device (0000b), 
					  specific interface (00001b), endpoint (00010b), or other element (00011b)
		bRequest, identifies the request
		wValue, pass the request-specific info to the device
	*/
    if (setup->bmRequestType & USB_REQUEST_RECIPIENT_DEVICE) /* request is directed to device */
    {
        switch (setup->bRequest)
        {
            case HUB_REQUEST_CLEAR_FEATURE: /* disable the feature */
                switch (setup->wValue)
                {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                		USB_LOG_ERR("HUB_FEATURE_HUB_C_LOCALPOWER not implmented.\n");
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                		USB_LOG_ERR("HUB_FEATURE_HUB_C_OVERCURRENT not implmented.\n");
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_SET_FEATURE: /* set a value reported in the hub status */
                switch (setup->wValue)
                {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                		USB_LOG_ERR("HUB_FEATURE_HUB_C_LOCALPOWER not implmented.\n");
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                		USB_LOG_ERR("HUB_FEATURE_HUB_C_OVERCURRENT not implmented.\n");
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_DESCRIPTOR:
                USB_LOG_ERR("HUB_REQUEST_GET_DESCRIPTOR not implmented.\n");
                break;
            case HUB_REQUEST_GET_STATUS:
				USB_ASSERT(buf);
                memset(buf, 0, 4);
                break;
            default:
                break;
        }
    }
    else if (setup->bmRequestType & USB_REQUEST_RECIPIENT_OTHER)
	{
       switch (setup->bRequest)
        {
            case HUB_REQUEST_CLEAR_FEATURE:
                if (!port || port > nports)
                {
                    return -EPIPE;
                }

                portsc = readl ( xhci->op + XHCI_OP_PORTSC ( port ) );
                switch (setup->wValue)
                {
                    case HUB_PORT_FEATURE_ENABLE:
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
						portsc |= XHCI_PORTSC_CSC;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
						portsc |= XHCI_PORTSC_PEC;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
                        break;
                    default:
                        return -EPIPE;
                }

                uint32_t pclear = portsc & XHCI_PORTSC_RW_MASK;
				/* clear port status */
				writel(pclear, xhci->op + XHCI_OP_PORTSC ( port ));

                break;
            case HUB_REQUEST_SET_FEATURE:
                if (!port || port > nports)
                {
                    return -EPIPE;
                }

                switch (setup->wValue)
                {
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        ret = xhci_port_enable(xhci, port);
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports)
                {
                    return -EPIPE;
                }

                portsc = readl ( xhci->op + XHCI_OP_PORTSC ( port ) );
                status = 0;

                if (portsc & XHCI_PORTSC_CSC)
                {
                    /* Port connection status changed */
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);

                    /* always clear all the status change bits */
                    uint32_t temp = portsc & ( XHCI_PORTSC_PRESERVE | XHCI_PORTSC_CHANGE );
                    writel ( temp, xhci->op + XHCI_OP_PORTSC ( port ) );
                }

                if (portsc & XHCI_PORTSC_PEC)
                {
                    /* Port enabled status changed */
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }

                if (portsc & XHCI_PORTSC_OCC)
                {
                    /* Port status changed due to over-current */
                    status |= (1 << HUB_PORT_FEATURE_C_OVER_CURREN);
                }

                if (portsc & XHCI_PORTSC_CCS)
                {
                    /* Port connected */
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                }

                if (portsc & XHCI_PORTSC_PED)
                {
                    /* Port enabled */
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);

                    const unsigned int speed = xhci_root_speed(xhci, port);
                    USB_LOG_DBG("Port-%d speed = %d \r\n", port, speed);
                    if (speed == USB_SPEED_LOW)
                    {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    }
                    else if (speed == USB_SPEED_HIGH)
                    {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                    /* else is full-speed */
                }

                if (portsc & XHCI_PORTSC_OCA)
                {
                    /* Over-current condition */
                    status |= (1 << HUB_PORT_FEATURE_OVERCURRENT);
                }

                if (portsc & XHCI_PORTSC_PR)
                {
                    /* Reset is in progress */
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                }

                if (portsc & XHCI_PORTSC_PP)
                {
                    /* Port is not power off */
                    status |= (1 << HUB_PORT_FEATURE_POWER);
                }
                memcpy(buf, &status, 4);        
                break;
            default:
                break;
        }
	}

    return 0;
}

uint8_t usbh_get_port_speed(struct usbh_hub *hub, const uint8_t port)
{
    USB_ASSERT(hub);
    struct xhci_host *xhci = &(xhci_host);

    if (hub->is_roothub) {
        return xhci_root_speed(xhci, port);
    } else {
        struct usbh_hubport *roothub_port = usbh_root_hub_port(&(hub->child[port]));
        /* TODO, hanlde TT for low-speed device on high-speed hub, but it doesn't matter, since
         we haven't well handle hub yet */
        USB_ASSERT(roothub_port);
        return xhci_root_speed(xhci, roothub_port->port);
    }
}

int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t mtu, uint8_t speed)
{
    struct xhci_endpoint *ppipe = (struct xhci_endpoint *)pipe;
    size_t old_mtu = ppipe->mtu;
    int rc = 0;

    if (mtu != old_mtu) {
        ppipe->mtu = mtu;
        rc = xhci_endpoint_mtu(ppipe);
    }

    return rc;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    int rc = 0;
    int slot_id = 0;
    struct xhci_host *xhci = &xhci_host;
    struct usbh_hubport *hport = ep_cfg->hport;
    struct xhci_endpoint *ppipe = usb_align(XHCI_RING_SIZE, sizeof(struct xhci_endpoint));
    struct xhci_slot *slot;

    if (NULL == ppipe) {
        return -ENOMEM;
    }

    memset(ppipe, 0, sizeof(struct xhci_endpoint));

    ppipe->waitsem = usb_osal_sem_create(0);
    ppipe->waiter = false;
    ppipe->urb = NULL;
    ppipe->hport = hport;

    ppipe->address = ep_cfg->ep_addr;
    ppipe->mtu = ep_cfg->ep_mps;
    ppipe->interval = ep_cfg->ep_interval;
    ppipe->ep_type = ep_cfg->ep_type;
    ppipe->burst = 0U;

    if (ppipe->address == 0) { /* if try to allocate ctrl ep, open device first */
        USB_LOG_DBG("allocate device for port-%d \r\n", hport->port);
        rc = xhci_device_open(xhci, ppipe, &slot_id);
        if (rc) {
            goto failed;
        }

        slot = xhci->slot[slot_id];
        USB_ASSERT(slot);
        rc = xhci_ctrl_endpoint_open(xhci, slot, ppipe);
        if (rc) {
            goto failed;
        }

        rc = xhci_device_address(xhci, slot, ppipe);
        if (rc) {
            goto failed;
        }
    } else {
        slot_id = hport->dev_addr;
        slot = xhci->slot[slot_id];

        rc = xhci_work_endpoint_open(xhci, slot, ppipe);
        if (rc) {
            goto failed;
        }
    }

    *pipe = (usbh_pipe_t)ppipe;
    return rc;
failed:
    usb_free(ppipe);
    *pipe = NULL;
    return rc;
}

int usbh_get_xhci_devaddr(usbh_pipe_t *pipe)
{
    struct xhci_endpoint *ppipe = (struct xhci_endpoint *)pipe;
    USB_ASSERT(ppipe && (ppipe->slot));
    return ppipe->slot->id;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    int ret = 0;
    struct xhci_endpoint *ppipe = (struct xhci_endpoint *)pipe;

    if (!ppipe) {
        return -EINVAL;
    }

    struct usbh_urb  *urb = ppipe->urb;
    if (urb) {
        usbh_kill_urb(urb);
    }

    size_t flags = usb_osal_enter_critical_section();
    xhci_endpoint_close(ppipe);
    usb_osal_leave_critical_section(flags);
    return 0;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    int ret = 0;
    if (!urb || !urb->pipe) {
        return -EINVAL;
    }

    struct xhci_endpoint *ppipe = (struct xhci_endpoint *)urb->pipe;
    struct xhci_host *xhci = ppipe->xhci;
    struct usb_setup_packet *setup = urb->setup;
    size_t flags = usb_osal_enter_critical_section();

    urb->errorcode = -EBUSY;
    urb->actual_length = 0U;

    ppipe->urb = urb;
    ppipe->timeout = urb->timeout;
    if (ppipe->timeout > 0) {
        ppipe->waiter = true;
    } else {
        ppipe->waiter = false;
    }

    usb_osal_leave_critical_section(flags);
    switch (ppipe->ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
            USB_ASSERT(setup);
            if (setup->bRequest == USB_REQUEST_SET_ADDRESS) {
                /* Set address command sent during xhci_alloc_pipe. */
                goto skip_req;
            }

            USB_LOG_DBG("%s request-%d.\n", __func__, setup->bRequest);
            xhci_endpoint_message(ppipe, setup, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
        case USB_ENDPOINT_TYPE_BULK:
            xhci_endpoint_stream(ppipe, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        default:
            USB_ASSERT(0U);
            break;
    }

    /* wait all ring handled by xHc */
    int cc = xhci_event_wait(xhci, ppipe, &ppipe->reqs);
    if ((cc != XHCI_CMPLT_SUCCESS) &&
        !((cc == XHCI_CMPLT_TIMEOUT) && (ppipe->ep_type == USB_ENDPOINT_TYPE_INTERRUPT)))
    {
        /* ignore transfer timeout for interrupt type */
        USB_LOG_ERR("%s: xfer failed (cc %d).\n", __func__, cc);
        ret = -1;
        urb->errorcode = cc;
        goto errout_timeout;
    }

skip_req:
errout_timeout:
    /* Timeout will run here */
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    return 0;
}

void USBH_IRQHandler(void *param)
{
	struct xhci_host *xhci = &xhci_host;
    struct xhci_endpoint *work_pipe = NULL;
    USB_ASSERT(xhci);
	uint32_t usbsts;
	uint32_t runtime;

	/* clear interrupt status */
	usbsts = readl ( xhci->op + XHCI_OP_USBSTS );
	usbsts |= XHCI_USBSTS_EINT;
	writel(usbsts, xhci->op + XHCI_OP_USBSTS);

	/* ack interrupt */
	runtime = readl(xhci->run + XHCI_RUN_IR_IMAN ( 0 ));
	runtime |= XHCI_RUN_IR_IMAN_IP;
	writel (runtime, xhci->run + XHCI_RUN_IR_IMAN ( 0 ));

    work_pipe = xhci_event_process(xhci);
}