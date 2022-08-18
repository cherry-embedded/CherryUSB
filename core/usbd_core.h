/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_CORE_H
#define USBD_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "usb_config.h"
#include "usb_util.h"
#include "usb_errno.h"
#include "usb_def.h"
#include "usb_list.h"
#include "usb_mem.h"
#include "usb_log.h"
#include "usb_dc.h"

enum usbd_event_type {
    /* USB DCD IRQ */
    USBD_EVENT_ERROR,        /** USB error reported by the controller */
    USBD_EVENT_RESET,        /** USB reset */
    USBD_EVENT_SOF,          /** Start of Frame received */
    USBD_EVENT_CONNECTED,    /** USB connected*/
    USBD_EVENT_DISCONNECTED, /** USB disconnected */
    USBD_EVENT_SUSPEND,      /** USB connection suspended by the HOST */
    USBD_EVENT_RESUME,       /** USB connection resumed by the HOST */

    /* USB DEVICE STATUS */
    USBD_EVENT_CONFIGURED,    /** USB configuration done */
    USBD_EVENT_SET_INTERFACE, /** USB interface selected */
    USBD_EVENT_UNKNOWN
};

typedef int (*usbd_request_handler)(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len);
typedef void (*usbd_endpoint_callback)(uint8_t ep, uint32_t nbytes);
typedef void (*usbd_notify_handler)(uint8_t event, void *arg);

typedef struct usbd_endpoint {
    usb_slist_t list;
    uint8_t ep_addr;
    usbd_endpoint_callback ep_cb;
} usbd_endpoint_t;

typedef struct usbd_interface {
    usb_slist_t list;
    /** Handler for USB Class specific commands*/
    usbd_request_handler class_handler;
    /** Handler for USB Vendor specific commands */
    usbd_request_handler vendor_handler;
    /** Handler for USB custom specific commands */
    usbd_request_handler custom_handler;
    /** Handler for USB event notify commands */
    usbd_notify_handler notify_handler;
    uint8_t intf_num;
    usb_slist_t ep_list;
} usbd_interface_t;

typedef struct usbd_class {
    usb_slist_t list;
    const char *name;
    usb_slist_t intf_list;
} usbd_class_t;

void usbd_desc_register(const uint8_t *desc);
void usbd_msosv1_desc_register(struct usb_msosv1_descriptor *desc);
void usbd_msosv2_desc_register(struct usb_msosv2_descriptor *desc);
void usbd_bos_desc_register(struct usb_bos_descriptor *desc);
void usbd_class_register(usbd_class_t *devclass);
void usbd_class_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);
void usbd_interface_add_endpoint(usbd_interface_t *intf, usbd_endpoint_t *ep);
bool usb_device_is_configured(void);
void usbd_configure_done_callback(void);
int usbd_initialize(void);

#ifdef __cplusplus
}
#endif

#endif /* USBD_CORE_H */
