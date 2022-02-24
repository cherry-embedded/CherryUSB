/**
 * @file usbh_core.h
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#ifndef _USBH_CORE_H
#define _USBH_CORE_H

#include "usb_util.h"
#include "usb_def.h"
#include "usb_hc.h"
#include "usb_osal.h"
#include "usb_workq.h"
#include "usbh_hub.h"
#include "usb_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USBH_ROOT_HUB_INDEX       1 /* roothub index*/
#define USBH_EX_HUB_INDEX         2 /* external hub index */
#define USBH_HUB_PORT_START_INDEX 1 /* first hub port index */

#ifdef CONFIG_USBHOST_HUB
#define ROOTHUB(hport) ((hport)->parent == NULL)
#else
#define ROOTHUB(hport) true
#endif

#define CLASS_CONNECT(hport, i)    ((hport)->config.intf[i].class_driver->connect(hport, i))
#define CLASS_DISCONNECT(hport, i) ((hport)->config.intf[i].class_driver->disconnect(hport, i))

enum usbh_event_type {
    USBH_EVENT_ATTACHED,
    USBH_EVENT_REMOVED,
};

struct usbh_class_info {
    uint8_t class;    /* Base device class code */
    uint8_t subclass; /* Sub-class, depends on base class. Eg. */
    uint8_t protocol; /* Protocol, depends on base class. Eg. */
    uint16_t vid;     /* Vendor ID (for vendor/product specific devices) */
    uint16_t pid;     /* Product ID (for vendor/product specific devices) */
    const struct usbh_class_driver *class_driver;
};

struct usbh_hubport;
struct usbh_class_driver {
    const char *driver_name;
    int (*connect)(struct usbh_hubport *hport, uint8_t intf);
    int (*disconnect)(struct usbh_hubport *hport, uint8_t intf);
};

typedef struct usbh_endpoint {
    struct usb_endpoint_descriptor ep_desc;
} usbh_endpoint_t;

typedef struct usbh_interface {
    struct usb_interface_descriptor intf_desc;
    struct usbh_endpoint ep[CONFIG_USBHOST_EP_NUM];
    char devname[CONFIG_USBHOST_DEV_NAMELEN];
    struct usbh_class_driver *class_driver;
    void *priv;
} usbh_interface_t;

typedef struct usbh_configuration {
    struct usb_configuration_descriptor config_desc;
    struct usbh_interface intf[CONFIG_USBHOST_INTF_NUM];
} usbh_configuration_t;

typedef struct usbh_hubport {
    bool connected;    /* True: device connected; false: disconnected */
    bool port_change;  /* True: port changed; false: port do not change */
    uint8_t port;      /* Hub port index */
    uint8_t dev_addr;  /* device address */
    uint8_t speed;     /* device speed */
    usbh_epinfo_t ep0; /* control ep info */
    struct usb_device_descriptor device_desc;
    struct usbh_configuration config;
#if 0
    uint8_t* config_desc;
#endif
    struct usb_setup_packet *setup;
    struct usbh_hub *parent; /*if NULL, is roothub*/
} usbh_hubport_t;

typedef struct usbh_hub {
    usb_slist_t list;
    uint8_t index;    /* Hub index */
    uint8_t nports;   /* Hub port number */
    uint8_t dev_addr; /* Hub device address */
    usbh_epinfo_t intin;
    uint8_t *int_buffer;
    struct hub_port_status *port_status;
    struct usb_hub_descriptor hub_desc;
    struct usbh_hubport child[CONFIG_USBHOST_EHPORTS];
    struct usbh_hubport *parent; /* Parent hub port */
    struct usb_work work;
} usbh_hub_t;

void usbh_event_notify_handler(uint8_t event, uint8_t rhport);

int usbh_initialize(void);
int usbh_lock(void);
void usbh_unlock(void);
int lsusb(int argc, char **argv);
struct usbh_hubport *usbh_find_hubport(uint8_t dev_addr);
void *usbh_find_class_instance(const char *devname);

#ifdef __cplusplus
}
#endif

#endif
