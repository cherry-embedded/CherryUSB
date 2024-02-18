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
 * FilePath: usb_dc_pusb2.c
 * Date: 2021-08-25 14:53:42
 * LastEditTime: 2021-08-26 09:01:26
 * Description:  This file is for implementation of PUSB2 port to cherryusb for host mode
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2023/7/19 first commit
 */

#include "usbd_core.h"
#include "fpusb2.h"

/* Endpoint state */
struct pusb2_dc_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    const struct usb_endpoint_descriptor *desc;
    FPUsb2DcEp *priv_ep;
};

/* Data IN/OUT request */
struct pusb2_dc_request {
    struct pusb2_dc_ep_state *ep;
    FPUsb2DcReq *priv_req;
    int status;
};

/* Driver state */
struct pusb2_udc {
    FPUsb2 pusb2;
    int speed;
    FPUsb2Config config;
    volatile uint8_t dev_addr;
    int ep0_init_finish;
    struct pusb2_dc_ep_state in_ep[FPUSB2_DC_EP_NUM];  /*!< IN endpoint parameters*/
    struct pusb2_dc_ep_state out_ep[FPUSB2_DC_EP_NUM]; /*!< OUT endpoint parameters */
} g_pusb2_udc;

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

static void pusb2_dc_init_ep_state(struct pusb2_dc_ep_state *ep_state, 
                                   FPUsb2DcEp *priv_ep)
{
    /* reset ep state and attach priv ep */
    ep_state->ep_mps = 0U;
    ep_state->ep_type = 0U;
    ep_state->ep_stalled = 0U;
    ep_state->desc = NULL;
    ep_state->priv_ep = priv_ep;
}

static void pusb2_dc_connect_handler(FPUsb2DcController *instance)
{
    FPUsb2DcDev *dc_dev = NULL;
    extern void FPUsb2DcNoReset(FPUsb2DcController *instance);

    FPUsb2DcGetDevInstance(&g_pusb2_udc.pusb2.device_ctrl, &dc_dev);
    USB_ASSERT(dc_dev);

    USB_LOG_DBG("%s \n", __func__);

    usbd_event_reset_handler(0);

    /* update speed and max packet size when connect */
    g_pusb2_udc.speed = dc_dev->speed;
    if (g_pusb2_udc.speed > USB_SPEED_HIGH) {
        g_pusb2_udc.in_ep[0].ep_mps = 9;
        g_pusb2_udc.out_ep[0].ep_mps = 9;
    } else {
        g_pusb2_udc.in_ep[0].ep_mps = dc_dev->ep0->max_packet;
        g_pusb2_udc.out_ep[0].ep_mps = dc_dev->ep0->max_packet;
    }

    FPUsb2DcNoReset(instance);
}

static void pusb2_dc_disconnect_handler(FPUsb2DcController *instance)
{
    USB_LOG_DBG("%s \n", __func__);
}

static void pusb2_dc_resume_handler(FPUsb2DcController *instance)
{
    USB_LOG_DBG("%s \n", __func__);
}

static uint32_t pusb2_dc_receive_steup_handler(FPUsb2DcController *instance, FUsbSetup *setup)
{
    USB_LOG_DBG("%s 0x%x:0x%x:0x%x:0x%x:0x%x\n", 
                __func__,
                setup->bmRequestType, 
                setup->bRequest,
                setup->wIndex,
                setup->wLength,
                setup->wValue);

    usbd_event_ep0_setup_complete_handler(0, (u8 *)setup);

    return 0;
}

static void pusb2_dc_suspend_handler(FPUsb2DcController *instance)
{
    USB_LOG_DBG("%s \n", __func__);
}

static void* pusb2_dc_allocate_request_handler(FPUsb2DcController *instance, uint32_t size)
{
    FPUsb2DcReq * cusbd_req = usb_malloc(size);
    if (!cusbd_req) {
        return NULL;
    }

    memset(cusbd_req, 0, sizeof(*cusbd_req));

    return cusbd_req;
}

static void pusb2_dc_free_request_handler(FPUsb2DcController *instance, void *usb_request)
{
	if (!usb_request)
		return;

    usb_free(usb_request);
}

static void pusb2_dc_pre_start_handler(FPUsb2DcController *instance)
{
    FPUsb2DcEp *priv_epx = NULL;
    FPUsb2DcDev *dc_dev = NULL;
    FDListHead  *list;
    int ep_num;

    FPUsb2DcGetDevInstance(&g_pusb2_udc.pusb2.device_ctrl, &dc_dev);
    USB_ASSERT(dc_dev);

    g_pusb2_udc.speed = dc_dev->max_speed;

    pusb2_dc_init_ep_state(&g_pusb2_udc.in_ep[0], dc_dev->ep0);
    pusb2_dc_init_ep_state(&g_pusb2_udc.out_ep[0], dc_dev->ep0);

    for(list = dc_dev->ep_list.next; 
        list != &dc_dev->ep_list; 
        list = list->next) {
        priv_epx = (FPUsb2DcEp*)list;
        ep_num = USB_EP_GET_IDX(priv_epx->address);

        if (USB_EP_DIR_IS_IN(priv_epx->address)) {
            pusb2_dc_init_ep_state(&g_pusb2_udc.in_ep[ep_num], priv_epx);
        } else {
            pusb2_dc_init_ep_state(&g_pusb2_udc.out_ep[ep_num], priv_epx);
        }
    }
}

static void pusb2_dc_prepare_ctrl_config(FPUsb2Config *config)
{
    *config = *FPUsb2LookupConfig(CONFIG_USBDEV_PUSB2_CTRL_ID);

    config->mode = FPUSB2_MODE_PERIPHERAL;
    
    /* allocate DMA buffer for TRB transfer */
    config->trb_mem_addr = usb_align(64U, config->trb_mem_size);
    USB_ASSERT(config->trb_mem_addr);

    /* hook up device callbacks */
    config->host_cb.givback_request = NULL;
    config->host_cb.otg_state_change = NULL;
    config->host_cb.port_status_change = NULL;
    config->host_cb.set_ep_toggle = NULL;
    config->host_cb.get_ep_toggle = NULL;
    config->host_cb.pre_start = NULL;

    config->device_cb.connect = pusb2_dc_connect_handler;
    config->device_cb.disconnect= pusb2_dc_disconnect_handler;
    config->device_cb.resume = pusb2_dc_resume_handler;
    config->device_cb.setup = pusb2_dc_receive_steup_handler;
    config->device_cb.suspend = pusb2_dc_suspend_handler;
    config->device_cb.usb_request_mem_alloc = pusb2_dc_allocate_request_handler;
    config->device_cb.usb_request_mem_free = pusb2_dc_free_request_handler;
    config->device_cb.pre_start = pusb2_dc_pre_start_handler;

    return;
}

int usb_dc_init(uint8_t busid)
{
    memset(&g_pusb2_udc, 0, sizeof(struct pusb2_udc));

    usb_dc_low_level_init();

    pusb2_dc_prepare_ctrl_config(&g_pusb2_udc.config);
    if (FPUSB2_SUCCESS != FPUsb2CfgInitialize(&g_pusb2_udc.pusb2,
                                              &g_pusb2_udc.config)) {
        USB_LOG_ERR("init pusb2 failed \n");
        return -1;
    }

    USB_LOG_INFO("init pusb2 successed \n");
    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    usb_dc_low_level_deinit();

    FPUsb2DeInitialize(&g_pusb2_udc.pusb2);
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    g_pusb2_udc.dev_addr = addr;
    return 0;
}

static struct usb_endpoint_descriptor *usbd_get_ep0_desc(const struct usb_endpoint_descriptor *ep)
{
    static struct usb_endpoint_descriptor ep0_desc;

    /* Config EP0 mps from speed */
    ep0_desc.bEndpointAddress = ep->bEndpointAddress;
    ep0_desc.bDescriptorType = USB_DESCRIPTOR_TYPE_ENDPOINT;
    ep0_desc.bmAttributes = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
    ep0_desc.wMaxPacketSize = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    ep0_desc.bInterval = 0;
    ep0_desc.bLength = 7;

    return &ep0_desc;    
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    struct pusb2_dc_ep_state *ep_state;
    uint32_t error;

    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        ep_state = &g_pusb2_udc.out_ep[ep_idx];
    } else {
        ep_state = &g_pusb2_udc.in_ep[ep_idx];
    }

    ep_state->ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    ep_state->ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
    ep_state->desc = usbd_get_ep0_desc(ep);

    USB_ASSERT(ep_state->priv_ep != NULL);
    USB_LOG_DBG("try to enable ep@0x%x 0x%x:0x%x\n", ep->bEndpointAddress,
                ep_state->priv_ep, ep_state->desc );
    error = FPUsb2DcEpEnable(&g_pusb2_udc.pusb2.device_ctrl,
                            ep_state->priv_ep,
                            (const FUsbEndpointDescriptor *)ep_state->desc);
    if (FPUSB2_SUCCESS != error){
        USB_LOG_ERR("enable ep-%d failed, error = 0x%x\n", ep->bEndpointAddress, error);
        return -1;
    }        

    g_pusb2_udc.ep0_init_finish = 1;

    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct pusb2_dc_ep_state *ep_state;

    if (USB_EP_DIR_IS_OUT(ep)) {
        ep_state = &g_pusb2_udc.out_ep[ep_idx];
    } else {
        ep_state = &g_pusb2_udc.in_ep[ep_idx];
    }

    ep_state->desc = NULL;
    if (FPUSB2_SUCCESS != FPUsb2DcEpDisable(&g_pusb2_udc.pusb2.device_ctrl,
                                           ep_state->priv_ep)){
        USB_LOG_ERR("disable ep@0x%x failed\n", ep);
        return -1;
    }

    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct pusb2_dc_ep_state *ep_state;

    if (USB_EP_DIR_IS_OUT(ep)) {
        ep_state = &g_pusb2_udc.out_ep[ep_idx];
    } else {
        ep_state = &g_pusb2_udc.in_ep[ep_idx];
    }

    if (FPUSB2_SUCCESS != FPUsb2DcEpSetHalt(&g_pusb2_udc.pusb2.device_ctrl,
                                            ep_state->priv_ep, 1)){
        USB_LOG_ERR("stall ep@0x%x failed\n", ep);
        return -1;
    }

    ep_state->ep_stalled = 1;
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct pusb2_dc_ep_state *ep_state;

    if (USB_EP_DIR_IS_OUT(ep)) {
        ep_state = &g_pusb2_udc.out_ep[ep_idx];
    } else {
        ep_state = &g_pusb2_udc.in_ep[ep_idx];
    }

    if (FPUSB2_SUCCESS != FPUsb2DcEpSetHalt(&g_pusb2_udc.pusb2.device_ctrl,
                                            ep_state->priv_ep, 0)){
        USB_LOG_ERR("clear ep@0x%x stall status failed\n", ep);
        return -1;
    }

    ep_state->ep_stalled = 0;
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct pusb2_dc_ep_state *ep_state;

    if (USB_EP_DIR_IS_OUT(ep)) {
        ep_state = &g_pusb2_udc.out_ep[ep_idx];
    } else {
        ep_state = &g_pusb2_udc.in_ep[ep_idx];
    }

    if (stalled) {
        *stalled = ep_state->ep_stalled;
    }

    return 0;
}

static struct pusb2_dc_request *pusb2_dc_allocate_request(struct pusb2_dc_ep_state *ep_state)
{
    struct pusb2_dc_request *request = usb_malloc(sizeof(*request));
    if (!request) {
        return NULL;
    }

    memset(request, 0, sizeof(*request));

    request->ep = ep_state;
    request->priv_req = NULL;

    if (FPUSB2_SUCCESS != FPUsb2DcReqAlloc(&g_pusb2_udc.pusb2.device_ctrl,
                                            ep_state->priv_ep, 
                                            &request->priv_req )){
        USB_LOG_ERR("allocate request failed\n");
        usb_free(request);
        return NULL;
    }

    return request;
}

static void pusb2_dc_free_request(struct pusb2_dc_request *request)
{
    USB_ASSERT(request);
    struct pusb2_dc_ep_state *ep_state = request->ep;
    FPUsb2DcReqFree(&g_pusb2_udc.pusb2.device_ctrl, 
                    ep_state->priv_ep,
                    request->priv_req);

    usb_free(request);
}

void pusb2_dc_callback_complete(FPUsb2DcEp *priv_ep, FPUsb2DcReq *priv_request)
{
    USB_ASSERT(priv_ep && priv_request);
    struct pusb2_dc_request *request;

    request = priv_request->context;

    if (USB_EP_DIR_IS_OUT(priv_ep->address)) {
        usbd_event_ep_out_complete_handler(0, priv_ep->address, priv_request->actual);
    } else {
        usbd_event_ep_in_complete_handler(0, priv_ep->address, priv_request->actual);
    }

    request->status = priv_request->status;
    if (request->status != 0) {
        USB_LOG_ERR("Request failed, status = %d\n", request->status);
    }

    pusb2_dc_free_request(request);
    priv_request->context = NULL;
}

int pusb2_dc_ep_read_write(const uint8_t ep, uintptr data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct pusb2_dc_ep_state *ep_state;
    struct pusb2_dc_request *request;
    uint32_t error;

    if (USB_EP_DIR_IS_OUT(ep)) {
        ep_state = &g_pusb2_udc.out_ep[ep_idx];
    } else {
        ep_state = &g_pusb2_udc.in_ep[ep_idx];
    }

    request = pusb2_dc_allocate_request(ep_state);
    if (!request) {
        USB_LOG_ERR("failed to allocate request !!!\n");
        return -1;
    }    

    request->priv_req->dma = data;
    request->priv_req->buf = (void *)data;
    request->priv_req->length = data_len;

    request->priv_req->complete = pusb2_dc_callback_complete;
    request->priv_req->context = request;
    request->priv_req->status = 0;

    error = FPUsb2DcReqQueue(&g_pusb2_udc.pusb2.device_ctrl,
                              ep_state->priv_ep, 
                              request->priv_req);
    if (FPUSB2_SUCCESS != error){
        USB_LOG_ERR("send req to ep@0x%x failed, error = 0x%x\n", ep, error);
        return -1;
    }

    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    return pusb2_dc_ep_read_write(ep, (uintptr)data, data_len);
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    return pusb2_dc_ep_read_write(ep, (uintptr)data, data_len);
}

void USBD_IRQHandler(uint8_t busid)
{
    FPUsb2InterruptHandler(&g_pusb2_udc.pusb2);
}
