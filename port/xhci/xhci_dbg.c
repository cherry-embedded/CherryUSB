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
 * FilePath: xhci.c
 * Date: 2022-07-19 09:26:25
 * LastEditTime: 2022-07-19 09:26:25
 * Description:  This file is for xhci register access functions.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 * 2.0   zhugengyu  2023/3/29   support usb3.0 device attached at roothub
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#include "usbh_core.h"

#include "xhci.h"

#define XHCI_DUMP_TRB
#define XHCI_DUMP_SLOT
#define XHCI_DUMP_EP_CTX
#define XHCI_DUMP_INPUT_CTX
#define XHCI_DUMP_ENDPOINT
#define XHCI_DUMP_PORT_STATUS

void xhci_dump_slot_ctx(const struct xhci_slot_context *const sc) {
	const uint8_t *ctx = (uint8_t *)sc;

#ifdef XHCI_DUMP_SLOT
	USB_LOG_DBG("===== slot ctx ===== \r\n");
	USB_LOG_DBG("ctx[0]=0x%x\n", *((uint32_t*)ctx));
	USB_LOG_DBG("	info: 0x%x \r\n", sc->info);
	USB_LOG_DBG("ctx[1]=0x%x\n", *((uint32_t*)ctx + 1));
	USB_LOG_DBG("	latency: 0x%x \r\n", sc->latency);
	USB_LOG_DBG("	port: 0x%x \r\n", sc->port);
	USB_LOG_DBG("	ports: 0x%x \r\n", sc->ports);
	USB_LOG_DBG("ctx[2]=0x%x\n", *((uint32_t*)ctx + 2));
	USB_LOG_DBG("	tt_id: 0x%x \r\n", sc->tt_id);
	USB_LOG_DBG("	tt_port: 0x%x \r\n", sc->tt_port);
	USB_LOG_DBG("ctx[3]=0x%x\n", *((uint32_t*)ctx + 3));
	USB_LOG_DBG("	intr: 0x%x \r\n", sc->intr);
	USB_LOG_DBG("	address: 0x%x \r\n", sc->address);
	USB_LOG_DBG("	state: 0x%x \r\n", sc->state);
	USB_LOG_DBG("=====+++++++++===== \r\n");
#endif
}

void xhci_dump_ep_ctx(const struct xhci_endpoint_context *ep) {
	const uint8_t *ctx = (uint8_t *)ep;

#ifdef XHCI_DUMP_EP_CTX
	USB_LOG_DBG("===== ep ctx ===== \r\n");
	USB_LOG_DBG("ctx[0]=0x%x\n", *((uint32_t*)ctx));
	USB_LOG_DBG("	ep_state: 0x%x \r\n", ep->state);
	USB_LOG_DBG("	mult: 0x%x \r\n", XHCI_EPCTX_MULT_GET(ep->stream));
	USB_LOG_DBG("	stream: 0x%x \r\n", XHCI_EPCTX_STREAM_GET(ep->stream));
	USB_LOG_DBG("	lsa: 0x%x \r\n", !!(XHCI_EPCTX_LSA & (ep->stream)));
	USB_LOG_DBG("	interval: 0x%x \r\n", ep->interval);
	USB_LOG_DBG("	esit_high: 0x%x \r\n", ep->esit_high);
	USB_LOG_DBG("ctx[1]=0x%x\n", *((uint32_t*)ctx + 1));
	USB_LOG_DBG("	cerr: 0x%x \r\n", XHCI_EPCTX_CERR_GET(ep->type));
	USB_LOG_DBG("	type: 0x%x \r\n", XHCI_EPCTX_TYPE_GET(ep->type));
	USB_LOG_DBG("	hid: 0x%x \r\n", !!(XHCI_EPCTX_HID & (ep->type)));
	USB_LOG_DBG("	burst: 0x%x \r\n", ep->burst);
	USB_LOG_DBG("	mtu: 0x%x \r\n", ep->mtu);
	USB_LOG_DBG("ctx[2]=0x%x\n", *((uint32_t*)ctx + 2));
	USB_LOG_DBG("ctx[3]=0x%x\n", *((uint32_t*)ctx + 3));
	USB_LOG_DBG("	dequeue: 0x%lx \r\n", ep->dequeue);
	USB_LOG_DBG("	dcs: 0x%lx \r\n", !!(XHCI_EPCTX_DCS & ep->dequeue));
	USB_LOG_DBG("ctx[4]=0x%x\n", *((uint32_t*)ctx + 4));
	USB_LOG_DBG("	trb_len: 0x%x \r\n", ep->trb_len);
	USB_LOG_DBG("	esit_low: 0x%x \r\n", ep->esit_low);		
	USB_LOG_DBG("=====+++++++++===== \r\n");
#endif
}

void xhci_dump_input_ctx( struct xhci_device *xhci, const struct xhci_endpoint *endpoint, const void *input) {
	const struct xhci_control_context *control_ctx;
	const struct xhci_slot_context *slot_ctx;
	const struct xhci_endpoint_context *ep_ctx;

	control_ctx = input;
	slot_ctx = ( input + xhci_input_context_offset ( xhci, XHCI_CTX_SLOT ));
	ep_ctx = ( input + xhci_input_context_offset ( xhci, endpoint->ctx ) );

#ifdef XHCI_DUMP_INPUT_CTX
	USB_LOG_DBG("===input ctx====\n");
	USB_LOG_DBG("ctrl@%p=0x%x\n", control_ctx, *control_ctx);
    USB_LOG_DBG("add=0x%x\n", control_ctx->add);
    USB_LOG_DBG("del=0x%x\n", control_ctx->drop);

	USB_LOG_DBG("slot@%p\n", slot_ctx);	
	xhci_dump_slot_ctx(slot_ctx);

	USB_LOG_DBG("ep-%d@%p\n", endpoint->ctx, ep_ctx);
	xhci_dump_ep_ctx(ep_ctx);

	USB_LOG_DBG("=====+++++++++===== \r\n");
#endif
}

static const union xhci_trb * xhci_get_next_trb(const union xhci_trb *trbs) {
	const struct xhci_trb_link *link = &(trbs->link);
	if (link->type == XHCI_TRB_LINK) {
		return (link->next) ? (const union xhci_trb *)(link->next) : NULL;
	} else {
		return trbs + 1;
	}
}

void xhci_dump_trbs(const union xhci_trb *trbs, unsigned int count) {
	const union xhci_trb *cur;
	const struct xhci_trb_common *comm;
	const uint8_t *dword;
	
#ifdef XHCI_DUMP_TRB
	USB_LOG_DBG("===trbs    ====\n");
	for (unsigned int i = 0; i < count; i++) {
		cur = &(trbs[i]);
		dword = (const uint8_t *)cur;

		comm = &(cur->common);
		USB_LOG_DBG("[0x%x-0x%x-0x%x-0x%x]\n", 
					*((uint32_t *)dword), *((uint32_t *)dword + 1), 
					*((uint32_t *)dword + 2), *((uint32_t *)dword + 3));
		if (XHCI_TRB_SETUP == comm->type) {
			const struct xhci_trb_setup *setup = (const struct xhci_trb_setup *)comm;

			USB_LOG_DBG("setup trb\n");
			USB_LOG_DBG("	packet=0x%x\n", setup->packet);
			USB_LOG_DBG("		bmRequestType type=0x%x\n", setup->packet.bmRequestType);
			USB_LOG_DBG("		bRequest=0x%x\n", setup->packet.bRequest);
			USB_LOG_DBG("		wValue=0x%x\n", setup->packet.wValue);
			USB_LOG_DBG("		wIndex=0x%x\n", setup->packet.wIndex);
			USB_LOG_DBG("		wLength=0x%x\n", setup->packet.wLength);
			USB_LOG_DBG("	trb_trans_len=%d\n", setup->len);
			USB_LOG_DBG("	flags=0x%x\n", setup->flags);
			USB_LOG_DBG("	direction=%d\n", setup->direction);
		} else if (XHCI_TRB_DATA == comm->type) {
			const struct xhci_trb_data *data = (const struct xhci_trb_data *)comm;

			USB_LOG_DBG("data trb......\n");
			USB_LOG_DBG("	data=0x%x\n", data->data);
			USB_LOG_DBG("	len=%d\n", data->len);
			USB_LOG_DBG("	direction=%d\n", data->direction);
		} else if (XHCI_TRB_STATUS == comm->type) {
			const struct xhci_trb_status *status = (const struct xhci_trb_status *)comm;

			USB_LOG_DBG("status trb......\n");
			USB_LOG_DBG("	direction=%d\n", status->direction);
		}
	}
	USB_LOG_DBG("=====+++++++++===== \r\n");
#endif
}

static const char *xhci_endpoint_xhci_type (unsigned int ep_xhci_type) {
	static const char *ep_names[] = {"not_valid", "isoc_out", "bulk_out", 
									 "intr_out", "ctrl", "isoc_in",
									 "bulk_in", "intr_in"};
	unsigned int index = ep_xhci_type >> 3;
	if (index < sizeof(ep_names)/sizeof(ep_names[0])) {
		return ep_names[index];
	}

	return "unkown";
}

static const char *xhci_endpoint_type (unsigned int ep_type) {
	const char *ep_name = "unkown";

	switch (ep_type)
	{
	case USB_ENDPOINT_TYPE_CONTROL:
		ep_name = "ctrl-ep";
		break;
	case USB_ENDPOINT_TYPE_ISOCHRONOUS:
		ep_name = "isoc-ep";
		break;
	case USB_ENDPOINT_TYPE_BULK:
		ep_name = "bulk-ep";
		break;
	case USB_ENDPOINT_TYPE_INTERRUPT:
		ep_name = "intr-ep";
		break;
	default:
		break;
	}

	return ep_name;
}

void xhci_dump_endpoint(const struct xhci_endpoint *ep) {
#ifdef XHCI_DUMP_ENDPOINT
	USB_LOG_DBG("===endpoint====\n");
	USB_LOG_DBG("xhci=0x%x\n", ep->xhci);
	USB_LOG_DBG("slot=0x%x\n", ep->slot);
	USB_LOG_DBG("address=0x%x\n", ep->address);
	USB_LOG_DBG("mtu=0x%x\n", ep->mtu);
	USB_LOG_DBG("burst=0x%x\n", ep->burst);
	USB_LOG_DBG("ctx=0x%x\n", ep->ctx);
	USB_LOG_DBG("ep_type=%s\n", xhci_endpoint_type(ep->ep_type));
	USB_LOG_DBG("xhci_type=%s\n", xhci_endpoint_xhci_type(ep->ctx_type));
	USB_LOG_DBG("interval=0x%x\n", ep->interval);
	USB_LOG_DBG("=====+++++++++===== \r\n");
#endif
}

void xhci_dump_port_status(uint32_t port, uint32_t portsc) {
#ifdef XHCI_DUMP_PORT_STATUS
	USB_LOG_DBG("====port-%d====\n");
	USB_LOG_DBG("connect=%d \n", !!(XHCI_PORTSC_CCS & port));
	USB_LOG_DBG("enabled=%d \n", !!(XHCI_PORTSC_PED & port));
	USB_LOG_DBG("powered=%d \n", !!(XHCI_PORTSC_PP & port));
	USB_LOG_DBG("=====+++++++++===== \r\n");
#endif
}