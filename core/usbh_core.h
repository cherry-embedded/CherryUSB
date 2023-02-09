/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_CORE_H
#define USBH_CORE_H

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
#include "usb_hc.h"
#include "usb_osal.h"
#include "usb_hub.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USB_CLASS_MATCH_VENDOR        0x0001
#define USB_CLASS_MATCH_PRODUCT       0x0002
#define USB_CLASS_MATCH_INTF_CLASS    0x0004
#define USB_CLASS_MATCH_INTF_SUBCLASS 0x0008
#define USB_CLASS_MATCH_INTF_PROTOCOL 0x0010

#define CLASS_CONNECT(hport, i)    ((hport)->config.intf[i].class_driver->connect(hport, i))
#define CLASS_DISCONNECT(hport, i) ((hport)->config.intf[i].class_driver->disconnect(hport, i))

#ifdef __ARMCC_VERSION /* ARM C Compiler */
#define CLASS_INFO_DEFINE __attribute__((section("usbh_class_info"))) __USED __ALIGNED(1)
#elif defined(__GNUC__)
#define CLASS_INFO_DEFINE __attribute__((section(".usbh_class_info"))) __USED __ALIGNED(1)
#elif defined(__ICCARM__) || defined(__ICCRX__)
#pragma section="usbh_class_info"
#define CLASS_INFO_DEFINE __attribute__((section("usbh_class_info"))) __USED __ALIGNED(1)
#endif

static inline void usbh_control_urb_fill(struct usbh_urb *urb,
                                         usbh_pipe_t pipe,
                                         struct usb_setup_packet *setup,
                                         uint8_t *transfer_buffer,
                                         uint32_t transfer_buffer_length,
                                         uint32_t timeout,
                                         usbh_complete_callback_t complete,
                                         void *arg)
{
    urb->pipe = pipe;
    urb->setup = setup;
    urb->transfer_buffer = transfer_buffer;
    urb->transfer_buffer_length = transfer_buffer_length;
    urb->timeout = timeout;
    urb->complete = complete;
    urb->arg = arg;
}

static inline void usbh_bulk_urb_fill(struct usbh_urb *urb,
                                      usbh_pipe_t pipe,
                                      uint8_t *transfer_buffer,
                                      uint32_t transfer_buffer_length,
                                      uint32_t timeout,
                                      usbh_complete_callback_t complete,
                                      void *arg)
{
    urb->pipe = pipe;
    urb->setup = NULL;
    urb->transfer_buffer = transfer_buffer;
    urb->transfer_buffer_length = transfer_buffer_length;
    urb->timeout = timeout;
    urb->complete = complete;
    urb->arg = arg;
}

static inline void usbh_int_urb_fill(struct usbh_urb *urb,
                                     usbh_pipe_t pipe,
                                     uint8_t *transfer_buffer,
                                     uint32_t transfer_buffer_length,
                                     uint32_t timeout,
                                     usbh_complete_callback_t complete,
                                     void *arg)
{
    urb->pipe = pipe;
    urb->setup = NULL;
    urb->transfer_buffer = transfer_buffer;
    urb->transfer_buffer_length = transfer_buffer_length;
    urb->timeout = timeout;
    urb->complete = complete;
    urb->arg = arg;
}

struct usbh_class_info {
    uint8_t match_flags; /* Used for product specific matches; range is inclusive */
    uint8_t class;       /* Base device class code */
    uint8_t subclass;    /* Sub-class, depends on base class. Eg. */
    uint8_t protocol;    /* Protocol, depends on base class. Eg. */
    uint16_t vid;        /* Vendor ID (for vendor/product specific devices) */
    uint16_t pid;        /* Product ID (for vendor/product specific devices) */
    const struct usbh_class_driver *class_driver;
};

struct usbh_hubport;
struct usbh_class_driver {
    const char *driver_name;
    int (*connect)(struct usbh_hubport *hport, uint8_t intf);
    int (*disconnect)(struct usbh_hubport *hport, uint8_t intf);
};

struct usbh_endpoint {
    struct usb_endpoint_descriptor ep_desc;
};

struct usbh_interface_altsetting {
    struct usb_interface_descriptor intf_desc;
    struct usbh_endpoint ep[CONFIG_USBHOST_MAX_ENDPOINTS];
};

struct usbh_interface {
    struct usbh_interface_altsetting altsetting[CONFIG_USBHOST_MAX_INTF_ALTSETTINGS];
    uint8_t altsetting_num;
    char devname[CONFIG_USBHOST_DEV_NAMELEN];
    struct usbh_class_driver *class_driver;
    void *priv;
};

struct usbh_configuration {
    struct usb_configuration_descriptor config_desc;
    struct usbh_interface intf[CONFIG_USBHOST_MAX_INTERFACES];
};

struct usbh_hubport {
    bool connected;   /* True: device connected; false: disconnected */
    uint8_t port;     /* Hub port index */
    uint8_t dev_addr; /* device address */
    uint8_t speed;    /* device speed */
    usbh_pipe_t ep0;  /* control ep pipe info */
    struct usb_device_descriptor device_desc;
    struct usbh_configuration config;
    const char *iManufacturer;
    const char *iProduct;
    const char *iSerialNumber;
    uint8_t* raw_config_desc;
    USB_MEM_ALIGNX struct usb_setup_packet setup;
    struct usbh_hub *parent;
};

struct usbh_hub {
    usb_slist_t list;
    bool connected;
    bool is_roothub;
    uint8_t index;
    uint8_t hub_addr;
    usbh_pipe_t intin;
    USB_MEM_ALIGNX uint8_t int_buffer[1];
    struct usbh_urb intin_urb;
    struct usb_hub_descriptor hub_desc;
    struct usbh_hubport child[CONFIG_USBHOST_MAX_EHPORTS];
    struct usbh_hubport *parent;
};

int usbh_hport_activate_epx(usbh_pipe_t *pipe, struct usbh_hubport *hport, struct usb_endpoint_descriptor *ep_desc);

/**
 * @brief Submit an control transfer to an endpoint.
 * This is a blocking method; this method will not return until the transfer has completed.
 * Default timeout is 500ms.
 *
 * @param pipe The control endpoint to send/receive the control request.
 * @param setup Setup packet to be sent.
 * @param buffer buffer used for sending the request and for returning any responses.
 * @return On success will return 0, and others indicate fail.
 */
int usbh_control_transfer(usbh_pipe_t pipe, struct usb_setup_packet *setup, uint8_t *buffer);

int usbh_initialize(void);
struct usbh_hubport *usbh_find_hubport(uint8_t dev_addr);
void *usbh_find_class_instance(const char *devname);

int lsusb(int argc, char **argv);
#ifdef __cplusplus
}
#endif

#endif /* USBH_CORE_H */
