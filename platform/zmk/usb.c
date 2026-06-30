/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "usbd_core.h"
#include "usbd_hid.h"

#include <zmk/usb.h>
#include <zmk/hid.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CHERRYUSB_BUS_ID 0

/* USB status tracking, mirroring the CherryUSB event states */
enum cherryusb_status {
    CUSB_STATUS_UNKNOWN,
    CUSB_STATUS_CONNECTED,
    CUSB_STATUS_DISCONNECTED,
    CUSB_STATUS_CONFIGURED,
    CUSB_STATUS_SUSPEND,
    CUSB_STATUS_RESUME,
    CUSB_STATUS_RESET,
    CUSB_STATUS_ERROR,
};

static enum cherryusb_status usb_status = CUSB_STATUS_UNKNOWN;

static void raise_usb_status_changed_event(struct k_work *_work) {
    raise_zmk_usb_conn_state_changed(
        (struct zmk_usb_conn_state_changed){.conn_state = zmk_usb_get_conn_state()});
}

K_WORK_DEFINE(usb_status_notifier_work, raise_usb_status_changed_event);

enum zmk_usb_conn_state zmk_usb_get_conn_state(void) {
    LOG_DBG("state: %d", usb_status);
    switch (usb_status) {
    case CUSB_STATUS_CONFIGURED:
    case CUSB_STATUS_SUSPEND:
    case CUSB_STATUS_RESUME:
        return ZMK_USB_CONN_HID;

    case CUSB_STATUS_DISCONNECTED:
    case CUSB_STATUS_UNKNOWN:
        return ZMK_USB_CONN_NONE;

    default:
        return ZMK_USB_CONN_POWERED;
    }
}

bool zmk_usb_is_hid_ready(void) {
    return usb_status == CUSB_STATUS_CONFIGURED || usb_status == CUSB_STATUS_RESUME;
}

/* ---- USB Descriptors ---- */

#define USBD_VID           CONFIG_USB_DEVICE_VID
#define USBD_PID           CONFIG_USB_DEVICE_PID
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

#define HID_INT_EP          0x81
#define HID_INT_EP_SIZE     64
#define HID_INT_EP_INTERVAL 1

#define USB_HID_CONFIG_DESC_SIZ (9 + HID_KEYBOARD_DESCRIPTOR_LEN)

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00,
                               USBD_VID, USBD_PID, 0x0001, 0x01),
};

static const uint8_t config_descriptor[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_HID_CONFIG_DESC_SIZ, 0x01, 0x01,
                               USB_CONFIG_BUS_POWERED | USB_CONFIG_REMOTE_WAKEUP,
                               USBD_MAX_POWER),
    HID_KEYBOARD_DESCRIPTOR_INIT(0x00,
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
                                 0x01,  /* boot interface subclass */
#else
                                 0x00,  /* no boot */
#endif
                                 sizeof(zmk_hid_report_desc),
                                 HID_INT_EP, HID_INT_EP_SIZE, HID_INT_EP_INTERVAL),
};

static const uint8_t device_quality_descriptor[] = {
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    0x40,
    0x00, 0x00,
};

#ifndef CONFIG_USB_DEVICE_MANUFACTURER
#define CONFIG_USB_DEVICE_MANUFACTURER "ZMK Project"
#endif

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 },       /* Langid */
    CONFIG_USB_DEVICE_MANUFACTURER,      /* Manufacturer */
    CONFIG_ZMK_KEYBOARD_NAME,            /* Product */
    "ZMK000000",                         /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed) {
    (void)speed;
    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed) {
    (void)speed;
    return config_descriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed) {
    (void)speed;
    return device_quality_descriptor;
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index) {
    (void)speed;
    if (index >= ARRAY_SIZE(string_descriptors)) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor zmk_cherryusb_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

/* ---- CherryUSB event handler ---- */

static void usbd_event_handler(uint8_t busid, uint8_t event) {
    LOG_DBG("USB event: bus %d, event %d\n", busid, event);
    switch (event) {
    case USBD_EVENT_RESET:
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
        extern void zmk_usb_hid_set_protocol(uint8_t protocol);
        zmk_usb_hid_set_protocol(1); /* HID_PROTOCOL_REPORT */
#endif
        usb_status = CUSB_STATUS_RESET;
        break;
    case USBD_EVENT_CONNECTED:
        usb_status = CUSB_STATUS_CONNECTED;
        break;
    case USBD_EVENT_DISCONNECTED:
        usb_status = CUSB_STATUS_DISCONNECTED;
        break;
    case USBD_EVENT_CONFIGURED:
        usb_status = CUSB_STATUS_CONFIGURED;
        break;
    case USBD_EVENT_SUSPEND:
        usb_status = CUSB_STATUS_SUSPEND;
        break;
    case USBD_EVENT_RESUME:
        usb_status = CUSB_STATUS_RESUME;
        break;
    case USBD_EVENT_ERROR:
        usb_status = CUSB_STATUS_ERROR;
        break;
    default:
        return;
    }
    k_work_submit(&usb_status_notifier_work);
}

/* ---- HID interface and endpoint (shared with usb_hid.c) ---- */

struct usbd_interface zmk_cherryusb_hid_intf;

static void hid_in_ep_callback(uint8_t busid, uint8_t ep, uint32_t nbytes);

struct usbd_endpoint zmk_cherryusb_hid_in_ep = {
    .ep_cb = hid_in_ep_callback,
    .ep_addr = HID_INT_EP,
};

/* Semaphore for EP write completion, given from endpoint callback */
K_SEM_DEFINE(zmk_cherryusb_hid_sem, 1, 1);

static void hid_in_ep_callback(uint8_t busid, uint8_t ep, uint32_t nbytes) {
    (void)busid;
    (void)ep;
    (void)nbytes;
    k_sem_give(&zmk_cherryusb_hid_sem);
}

static int zmk_usb_init(void) {
    usbd_desc_register(CHERRYUSB_BUS_ID, &zmk_cherryusb_descriptor);

    usbd_add_interface(CHERRYUSB_BUS_ID,
                       usbd_hid_init_intf(CHERRYUSB_BUS_ID, &zmk_cherryusb_hid_intf,
                                          zmk_hid_report_desc,
                                          sizeof(zmk_hid_report_desc)));
    usbd_add_endpoint(CHERRYUSB_BUS_ID, &zmk_cherryusb_hid_in_ep);

    int ret = usbd_initialize(CHERRYUSB_BUS_ID,
                              (uintptr_t)CONFIG_CHERRYUSB_DEV_BASE_ADDR,
                              usbd_event_handler);
    if (ret != 0) {
        LOG_ERR("CherryUSB initialization failed (err %d)", ret);
        return -EINVAL;
    }

    LOG_INF("CherryUSB initialized");
    return 0;
}

SYS_INIT(zmk_usb_init, APPLICATION, CONFIG_ZMK_USB_INIT_PRIORITY);