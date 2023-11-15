/*
 * Copyright : (C) 2023 Phytium Information Technology, Inc.
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
 * FilePath: usb_hc_pusb2.c
 * Date: 2021-08-25 14:53:42
 * LastEditTime: 2021-08-26 09:01:26
 * Description:  This file is for implementation of PUSB2 port to cherryusb for host mode
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2023/7/19    first commit
 * 1.1   zhugengyu  2023/11/14   sync with 0.11.1 port interface
 */

#include <assert.h>

#include "usbh_core.h"
#include "usbh_hub.h"
#include "fpusb2.h"

struct pusb2_pipe;
struct pusb2_dev;
struct pusb2_hcd;

struct pusb2_hcd {
    FPUsb2 pusb2;
    FPUsb2Config config;
};

struct pusb2_dev {
    FPUsb2HcEp          ep0;
    FPUsb2HcEp          *epx_in[FPUSB2_HC_EP_NUM];
    FPUsb2HcEp          *epx_out[FPUSB2_HC_EP_NUM];
    FPUsb2HcDevice      udev;

    /*one bit for each endpoint, with ([0] = IN, [1] = OUT) endpoints*/
    unsigned int        toggle[2];
#define PUSB2_GET_TOGGLE(dev, ep, out) (((dev)->toggle[out] >> (ep)) & 1)
#define	PUSB2_DO_TOGGLE(dev, ep, out)  ((dev)->toggle[out] ^= (1 << (ep)))
#define PUSB2_SET_TOGGLE(dev, ep, out, bit) \
		((dev)->toggle[out] = ((dev)->toggle[out] & ~(1 << (ep))) | \
		 ((bit) << (ep)))
};

struct pusb2_pipe {
    struct pusb2_hcd *hcd;
    struct pusb2_dev *dev;

    uint8_t speed;
    uint8_t dev_addr;
    uint8_t ep_addr;
    uint8_t ep_type;
    uint8_t ep_num;
    uint8_t ep_is_in;
    uint8_t ep_interval;
    uint16_t ep_mps;

    bool inuse;
    volatile bool waiter;
    usb_osal_sem_t waitsem;
    struct usbh_hubport *hport;
    struct usbh_urb *urb;
    const struct usb_endpoint_descriptor *desc;
};

static int usb_id = CONFIG_USBDEV_PUSB2_CTRL_ID;
static struct pusb2_hcd g_pusb2_hcd[CONFIG_USBDEV_PUSB2_CTRL_NUM];

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
__WEAK unsigned long usb_hc_get_register_base(uint32_t id)
{
    return 0U;
}

static inline struct pusb2_dev *pusb2_hc_pipe_to_dev(struct pusb2_pipe *ppipe)
{
    USB_ASSERT(ppipe && ppipe->dev);
    return ppipe->dev;
}

static inline struct pusb2_hcd *pusb2_hc_get_hcd(void)
{
    return &g_pusb2_hcd[usb_id];
}

static void pusb2_pipe_waitup(struct pusb2_pipe *pipe)
{
    struct usbh_urb *urb;

    urb = pipe->urb;
    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        usb_osal_sem_give(pipe->waitsem);
    }

    if (urb->complete) {
        if (urb->errorcode < 0) {
            urb->complete(urb->arg, urb->errorcode);
        } else {
            urb->complete(urb->arg, urb->actual_length);
        }
    }
}

static void pusb2_hc_request_giveback(FPUsb2HcController *instance, FPUsb2HcReq *req, u32 status)
{
    struct usbh_urb *urb;
    struct pusb2_pipe *pipe;
    int error = 0;

    urb = req->user_ext;
    pipe = urb->pipe;

    switch(status) {
        case FPUSB2_HC_ESTALL:
            error = -EPIPE;
            break;
        case FPUSB2_HC_EUNHANDLED:
            error = -EPROTO;
            break;
        case FPUSB2_HC_ESHUTDOWN:
            error = -ESHUTDOWN;
            break;
    }

    urb->errorcode = error;
    urb->actual_length = req->actual_length;

    pusb2_pipe_waitup(pipe);
    return;
}

static void pusb2_hc_otg_state_change(FPUsb2HcController *instance, int otg_state)
{
    return;
}


static int pusb2_hub_status_data(FPUsb2HcController *instance, char *buf)
{
    int retval = 0;

    retval = FPUsb2VHubStatusChangeData(instance, (u8 *)buf);
    if(retval != 0) {
        return 0;
    }

    if(*buf == 0x02) {
        return 1;
    } else {
        return 0;
    }
}

static void pusb2_hc_rh_port_status_change(FPUsb2HcController *instance)
{
    u32 status_hub = 0U;
    u16 *status = (u16*)&status_hub;
    FUsbSetup setup;
    u32 retval = 0;

    pusb2_hub_status_data(instance, (char*)status);

    setup.bRequest = FUSB_REQ_GET_STATUS;
    setup.bmRequestType = FUSB_REQ_TYPE_CLASS | FUSB_REQ_RECIPIENT_OTHER | FUSB_DIR_DEVICE_TO_HOST;
    setup.wIndex = cpu_to_le16(1);  /* port number */
    setup.wLength = cpu_to_le16(4);
    setup.wValue = 0;

    retval = FPUsb2VHubControl(instance, &setup, (u8*)status);
    if(retval) {
        return;
    }

    if(status[1] & FUSB_PSC_CONNECTION) {
        if(status[0] & FUSB_PS_CONNECTION) {
            USB_LOG_DBG("resume roothub \n");
            /* Report port status change */
            usbh_roothub_thread_wakeup ( 1U );
        }
    }
}

static u8 pusb2_hc_get_ep_toggle(void *instance, struct FPUsb2HcDevice *udev, u8 ep_num, u8 is_in)
{
    struct pusb2_dev *dev;
    u8 toggle = 0;

    dev = (struct pusb2_dev*) udev->user_ext;
    toggle = PUSB2_GET_TOGGLE(dev, ep_num, !is_in);
    return toggle;
}

static void pusb2_hc_set_ep_toggle(void *instance, struct FPUsb2HcDevice *udev, u8 ep_num, u8 is_in, u8 toggle)
{
    struct pusb2_dev *dev;

    dev = (struct pusb2_dev*) udev->user_ext;
    PUSB2_SET_TOGGLE(dev, ep_num, !is_in, toggle);
}

static void pusb2_hc_prepare_ctrl_config(uint32_t id, FPUsb2Config *config)
{
    *config = *FPUsb2LookupConfig(id);

    config->mode = FPUSB2_MODE_HOST;

    /* allocate DMA buffer for TRB transfer */
    config->trb_mem_addr = usb_align(64U, config->trb_mem_size);
    USB_ASSERT(config->trb_mem_addr);

    /* hook up host callbacks */
    config->host_cb.givback_request = pusb2_hc_request_giveback;
    config->host_cb.otg_state_change = pusb2_hc_otg_state_change;
    config->host_cb.port_status_change = pusb2_hc_rh_port_status_change;
    config->host_cb.set_ep_toggle = pusb2_hc_set_ep_toggle;
    config->host_cb.get_ep_toggle = pusb2_hc_get_ep_toggle;
    config->host_cb.pre_start = NULL;
    config->host_cb.usb_dev_callbacks = &config->device_cb;

    config->device_cb.connect = NULL;
    config->device_cb.disconnect= NULL;
    config->device_cb.resume = NULL;
    config->device_cb.setup = NULL;
    config->device_cb.suspend = NULL;
    config->device_cb.usb_request_mem_alloc = NULL;
    config->device_cb.usb_request_mem_free = NULL;
    config->device_cb.pre_start = NULL;

    return;
}

int usb_hc_init(void)
{
    int rc;
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();

    size_t flag = usb_osal_enter_critical_section(); /* no interrupt when init hc */
    usb_hc_low_level_init(); /* set gic and memp */
    
    memset(hcd, 0, sizeof(*hcd));

    pusb2_hc_prepare_ctrl_config(usb_id, &hcd->config);

    if (FPUSB2_SUCCESS != FPUsb2CfgInitialize(&hcd->pusb2,
                                              &hcd->config)) {
        USB_LOG_ERR("init pusb2 failed \n");
        rc = -1;
    } else {
        USB_LOG_INFO("init pusb2 successed \n");
    }

    usb_osal_leave_critical_section(flag);
	return rc; 
}

uint16_t usbh_get_frame_number(void)
{
    return 0;
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();
    int retval = 0;

    retval = FPUsb2VHubControl(&(hcd->pusb2.host_ctrl), (FUsbSetup *)setup, buf);
    if(retval != 0) {
        USB_LOG_ERR("%s failed, retval = %d \r\n", __func__, retval);
    }

    return retval; 
}

static void pusb2_hc_update_device(struct pusb2_dev *dev, int dev_addr, int speed, int mps)
{
    dev->udev.speed = (FUsbSpeed)speed;
    dev->udev.devnum = dev_addr;
    dev->ep0.ep_desc.max_packet_size = mps;
}

int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t mtu, uint8_t speed)
{
    struct pusb2_pipe *ppipe = pipe;
    struct usbh_hubport *hport = ppipe->hport;
    struct pusb2_dev *dev = pusb2_hc_pipe_to_dev(ppipe);

    pusb2_hc_update_device(dev, dev_addr, hport->speed, mtu);

    return 0;
}

static struct pusb2_dev *pusb2_hc_allocate_dev(void)
{
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();
    struct pusb2_dev *dev;

    dev = usb_malloc((sizeof *dev)+ FPUsb2HcGetPrivateDataSize(&(hcd->pusb2.host_ctrl)));
    if (dev == NULL)
        return NULL;

    dev->ep0.hc_priv = &((u8*)dev)[sizeof *dev]; /* ep private data */
    dev->udev.user_ext =  (void*)dev;

    dev->ep0.ep_desc.bLength = FUSB_DS_ENDPOINT;
    dev->ep0.ep_desc.bDescriptorType = FUSB_DT_ENDPOINT;
    FDLIST_INIT_HEAD(&dev->ep0.reqList);
    dev->epx_in[0] = &dev->ep0;
    dev->epx_out[0] = &dev->ep0;

    return dev;
}

static void pusb2_hc_free_ep(struct pusb2_pipe *ppipe)
{
    USB_ASSERT(ppipe && ppipe->hcd);
    struct usbh_hubport *hport = ppipe->hport;
    struct pusb2_hcd *hcd = ppipe->hcd;
    struct pusb2_dev *dev = pusb2_hc_pipe_to_dev(ppipe);
    int ep_num = USB_EP_GET_IDX(ppipe->ep_addr);

    if (USB_EP_DIR_IS_IN(ppipe->ep_addr)) {
        dev->epx_in[ep_num]->user_ext = NULL;
        usb_free(dev->epx_in[ep_num]);
        dev->epx_in[ep_num] = NULL;
    } else {
        dev->epx_out[ep_num]->user_ext = NULL;
        usb_free(dev->epx_out[ep_num]);
        dev->epx_out[ep_num] = NULL;
    }

    return;
}

static void pusb2_hc_free_dev(struct pusb2_pipe *ppipe)
{
    USB_ASSERT(ppipe && ppipe->hcd);
    struct usbh_hubport *hport = ppipe->hport;
    struct pusb2_hcd *hcd = ppipe->hcd;
    struct pusb2_dev *dev = pusb2_hc_pipe_to_dev(ppipe);

    dev->epx_in[0] = NULL;
    dev->epx_out[0] = NULL;

    for (int i = 1; i < FPUSB2_HC_EP_NUM; i++) {
        if (dev->epx_in[i]) {
            dev->epx_in[i]->user_ext = NULL;
            usb_free(dev->epx_in[i]);
            dev->epx_in[i] = NULL;
        }

        if (dev->epx_out[i]) {
            dev->epx_out[i]->user_ext = NULL;
            usb_free(dev->epx_out[i]);
            dev->epx_out[i] = NULL;
        }
    }

    usb_free(dev);
    return;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct usbh_hubport *hport = ep_cfg->hport;
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();
    struct pusb2_pipe *ppipe = usb_malloc(sizeof(struct pusb2_pipe));
    struct pusb2_dev *dev;

    if (NULL == ppipe) {
        return -ENOMEM;
    }

    memset(ppipe, 0, sizeof(struct pusb2_pipe));

    ppipe->waitsem = usb_osal_sem_create(0);
    ppipe->waiter = false;
    ppipe->urb = NULL;
    ppipe->hport = hport;

    ppipe->ep_addr = ep_cfg->ep_addr;
    ppipe->ep_type = ep_cfg->ep_type;
    ppipe->ep_num = USB_EP_GET_IDX(ep_cfg->ep_addr);
    ppipe->ep_is_in = USB_EP_DIR_IS_IN(ep_cfg->ep_addr);
    ppipe->ep_mps = ep_cfg->ep_mps;
    ppipe->ep_interval = ep_cfg->ep_interval;
    ppipe->hcd = hcd;

    USB_LOG_DBG("allocate ep-%d\n", ppipe->ep_num);
    if (ppipe->ep_addr == 0) { /* if try to allocate ctrl ep, open device first */
        dev = pusb2_hc_allocate_dev();
        if (NULL == dev) {
            usb_free(ppipe);
            return -ENOMEM;
        }

        ppipe->desc = (const struct usb_endpoint_descriptor *)&(dev->ep0.ep_desc);
        ppipe->dev = dev;
    } else {
        dev = pusb2_hc_pipe_to_dev((struct pusb2_pipe *)hport->ep0);
        struct pusb2_pipe *ppipe_ctrl = hport->ep0;

        ppipe->desc = ppipe_ctrl->desc;
        ppipe->dev = dev;
    }

    *pipe = (usbh_pipe_t)ppipe;
    return 0;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    USB_ASSERT(pipe);
    struct pusb2_pipe *ppipe = (struct pusb2_pipe *)pipe;
    struct usbh_urb *urb = ppipe->urb;
    size_t flags;
    
    /* free any un-finished urb */
    if (ppipe->urb) {
        usbh_kill_urb(urb);
    }
    
    flags = usb_osal_enter_critical_section();
    if (USB_EP_GET_IDX(ppipe->ep_addr) == 0) {
        /* free control ep means free device */
        pusb2_hc_free_dev(ppipe);
    } else {
        /* free work ep */
        pusb2_hc_free_ep(ppipe);
    }
    usb_osal_leave_critical_section(flags);

    return 0;
}

static void pusb2_hc_update_endpoint(struct pusb2_hcd *hcd, struct pusb2_dev *dev, struct pusb2_pipe *pipe)
{
    USB_ASSERT(hcd && dev && pipe);
    FPUsb2HcEp * priv_ep = NULL;
    int epnum = pipe->ep_num;
    int is_out = !pipe->ep_is_in;

    if (is_out) {
        if (dev->epx_out[epnum] == NULL) {
            priv_ep = usb_malloc(sizeof(FPUsb2HcEp) + FPUsb2HcGetPrivateDataSize(&(hcd->pusb2.host_ctrl)));
            USB_ASSERT(priv_ep);
            dev->epx_out[epnum] = priv_ep;
        } else {
            priv_ep = dev->epx_out[epnum];
        }
    } else {
        if (dev->epx_in[epnum] == NULL) {
            priv_ep = usb_malloc(sizeof(FPUsb2HcEp) + FPUsb2HcGetPrivateDataSize(&(hcd->pusb2.host_ctrl)));
            USB_ASSERT(priv_ep);
            dev->epx_in[epnum] = priv_ep;
        } else {
            priv_ep = dev->epx_in[epnum];
        }
    }

    priv_ep->ep_desc = *((FUsbEndpointDescriptor *)pipe->desc);
    priv_ep->user_ext = (void *)pipe;
    FDLIST_INIT_HEAD(&priv_ep->reqList);
    priv_ep->hc_priv = &((u8*)priv_ep)[sizeof *priv_ep];    
}

static int pusb2_hc_enqueue_urb(struct usbh_urb *urb)
{
    struct pusb2_pipe *pipe = urb->pipe;
    struct usbh_hubport *hport = pipe->hport;
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();
    struct pusb2_dev *dev;

    u32 iso_frame_size;
    FPUsb2HcReq *priv_req;
    int ret;

    if(!FPUsb2HcIsHostMode(&(hcd->pusb2.host_ctrl))) {
        return -ENODEV;
    }

    dev = pusb2_hc_pipe_to_dev(pipe);
    if(!dev)
        return -ENODEV;

    if (pipe->ep_is_in) {
        if (!dev->epx_in[pipe->ep_num]) {
            pusb2_hc_update_endpoint(hcd, dev, pipe);
        }
    } else {
        if (!dev->epx_out[pipe->ep_num]) {
            pusb2_hc_update_endpoint(hcd, dev, pipe);
        }
    }

    iso_frame_size = urb->num_of_iso_packets * sizeof(FPUsb2HcIsoFrameDesc);
    priv_req = (FPUsb2HcReq*)usb_malloc((sizeof *priv_req) + iso_frame_size);
    if (!priv_req)
        return -ENOMEM;

    priv_req->iso_frames_desc = NULL;
    priv_req->iso_frames_number = urb->num_of_iso_packets;

    FDLIST_INIT_HEAD(&priv_req->list);
    priv_req->user_ext = (void*) urb; 

    priv_req->actual_length = urb->actual_length;
    priv_req->buf_address = urb->transfer_buffer;
    priv_req->buf_dma = (uintptr_t)urb->transfer_buffer;
    priv_req->buf_length = urb->transfer_buffer_length;
    priv_req->ep_is_in = pipe->ep_is_in;
    priv_req->ep_num = pipe->ep_num;
    priv_req->ep_type = pipe->ep_type;
    priv_req->faddress =  dev->udev.devnum;
    priv_req->interval = pipe->ep_interval;
    priv_req->req_unlinked = 0;
    priv_req->setup = (FUsbSetup*)urb->setup;
    priv_req->setup_dma = (uintptr_t)urb->setup;
    priv_req->status = FPUSB2_ERR_INPROGRESS;
    priv_req->usb_dev = &dev->udev;
    priv_req->usb_ep = priv_req->ep_is_in ? dev->epx_in[priv_req->ep_num]:
                      dev->epx_out[priv_req->ep_num];

    if (priv_req->ep_num == 0) {
        dev->ep0.ep_desc.max_packet_size = pipe->ep_mps;
    }

    urb->hcpriv = priv_req;

    ret = FPUsb2HcReqQueue(&(hcd->pusb2.host_ctrl), priv_req);
    if(ret) {
        usb_free(priv_req);
        return ret;
    }

    return ret;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    struct pusb2_pipe *pipe = (struct pusb2_pipe *)urb->pipe;
    size_t flags;
    int ret = 0;

    if (!urb) {
        return -EINVAL;
    }

    if (!pipe->hport->connected) {
        return -ENODEV;
    }

    if (pipe->urb) {
        return -EBUSY;
    }

    if (urb->timeout > 0) {
        flags = usb_osal_enter_critical_section();
    }

    pipe->waiter = false;
    pipe->urb = urb;
    urb->errorcode = -EBUSY;
    urb->actual_length = 0;

    if (urb->timeout > 0) {
        pipe->waiter = true;
    }

    if (urb->timeout > 0) {
        usb_osal_leave_critical_section(flags);
    }

    switch (pipe->ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
        case USB_ENDPOINT_TYPE_BULK:
        case USB_ENDPOINT_TYPE_INTERRUPT:
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            ret = pusb2_hc_enqueue_urb(urb);
            break;
        default:
            break;
    }

    if (urb->timeout > 0) {
        /* wait until timeout or sem give */
        ret = usb_osal_sem_take(pipe->waitsem, urb->timeout);
        if (ret < 0) {
            USB_LOG_ERR("wait request timeout, ret = %d \n", ret);
            goto errout_timeout;
        }

        ret = urb->errorcode;
    }

    return ret;
errout_timeout:
    pipe->waiter = false;
    usbh_kill_urb(urb);
    return ret;    
}

static void pusb2_hc_dequeue_urb(struct usbh_urb *urb)
{
    USB_ASSERT(urb);
    struct pusb2_pipe *pipe = urb->pipe;
    struct usbh_hubport *hport = pipe->hport;
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();
    struct pusb2_dev *dev;
    FPUsb2HcReq *priv_req = urb->hcpriv;

    USB_ASSERT(priv_req);
    if (FPUSB2_SUCCESS != FPUsb2HcReqDequeue(&(hcd->pusb2.host_ctrl), priv_req, 0)) {
        USB_LOG_ERR("failed to dequeue urb \n");
    }

    usb_free(priv_req);
    urb->hcpriv = NULL;
    return;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    size_t flags;
    if (!urb) {
        return -EINVAL;
    }

    struct pusb2_pipe *pipe = urb->pipe;

    flags = usb_osal_enter_critical_section();

    pusb2_hc_dequeue_urb(urb);
    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        urb->errorcode = -ESHUTDOWN;
        usb_osal_sem_give(pipe->waitsem);
    }

    usb_osal_sem_delete(pipe->waitsem);
    usb_osal_leave_critical_section(flags);

    return 0;
}

void USBH_IRQHandler(void *param)
{
    struct pusb2_hcd *hcd = pusb2_hc_get_hcd();
    FPUsb2InterruptHandler(&hcd->pusb2);
    return;  
}