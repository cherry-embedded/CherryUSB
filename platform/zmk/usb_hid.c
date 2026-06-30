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
#include <zmk/keymap.h>

#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
#include <zmk/pointing/resolution_multipliers.h>
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
#include <zmk/hid_indicators.h>
#endif

#include <zmk/event_manager.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define CHERRYUSB_BUS_ID 0
#define HID_INT_EP       0x81

/* Defined in cherryusb/usb.c */
extern struct k_sem zmk_cherryusb_hid_sem;

/* Write buffer must be USB-aligned and in non-cached memory */
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t hid_write_buffer[128];

#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
static uint8_t hid_protocol = 1; /* HID_PROTOCOL_REPORT */

void zmk_usb_hid_set_protocol(uint8_t protocol) { hid_protocol = protocol; }
#endif

static uint8_t *get_keyboard_report(size_t *len) {
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
    if (hid_protocol != 1) { /* boot protocol */
        zmk_hid_boot_report_t *boot_report = zmk_hid_get_boot_report();
        *len = sizeof(*boot_report);
        return (uint8_t *)boot_report;
    }
#endif
    struct zmk_hid_keyboard_report *report = zmk_hid_get_keyboard_report();
    *len = sizeof(*report);
    return (uint8_t *)report;
}

/* ---- CherryUSB HID class callbacks (weak overrides) ---- */

void usbd_hid_get_report(uint8_t busid, uint8_t intf, uint8_t report_id,
                         uint8_t report_type, uint8_t **data, uint32_t *len) {
    (void)busid;
    (void)intf;

    switch (report_type) {
    case 3: /* Feature */
        switch (report_id) {
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
        case ZMK_HID_REPORT_ID_MOUSE: {
            static struct zmk_hid_mouse_resolution_feature_report res_feature_report;
            struct zmk_endpoint_instance endpoint = {
                .transport = ZMK_TRANSPORT_USB,
            };
            *len = sizeof(struct zmk_hid_mouse_resolution_feature_report);
            struct zmk_pointing_resolution_multipliers mult =
                zmk_pointing_resolution_multipliers_get_profile(endpoint);
            res_feature_report.body.wheel_res = mult.wheel;
            res_feature_report.body.hwheel_res = mult.hor_wheel;
            *data = (uint8_t *)&res_feature_report;
            break;
        }
#endif
        default:
            *len = 0;
            break;
        }
        break;

    case 1: /* Input */
        switch (report_id) {
        case ZMK_HID_REPORT_ID_KEYBOARD: {
            size_t size;
            *data = get_keyboard_report(&size);
            *len = (uint32_t)size;
            break;
        }
        case ZMK_HID_REPORT_ID_CONSUMER: {
            struct zmk_hid_consumer_report *report = zmk_hid_get_consumer_report();
            *data = (uint8_t *)report;
            *len = sizeof(*report);
            break;
        }
        default:
            LOG_ERR("Invalid report ID %d requested", report_id);
            *len = 0;
            break;
        }
        break;

    default:
        LOG_ERR("Unsupported report type %d requested", report_type);
        *len = 0;
        break;
    }
}

void usbd_hid_set_report(uint8_t busid, uint8_t intf, uint8_t report_id,
                         uint8_t report_type, uint8_t *report, uint32_t report_len) {
    (void)busid;
    (void)intf;

    switch (report_type) {
    case 3: /* Feature */
        switch (report_id) {
#if IS_ENABLED(CONFIG_ZMK_POINTING_SMOOTH_SCROLLING)
        case ZMK_HID_REPORT_ID_MOUSE:
            if (report_len != sizeof(struct zmk_hid_mouse_resolution_feature_report)) {
                return;
            }

            struct zmk_hid_mouse_resolution_feature_report *feat =
                (struct zmk_hid_mouse_resolution_feature_report *)report;
            struct zmk_endpoint_instance endpoint = {
                .transport = ZMK_TRANSPORT_USB,
            };
            zmk_pointing_resolution_multipliers_process_report(&feat->body, endpoint);
            break;
#endif
        default:
            break;
        }
        break;

    case 2: /* Output */
        switch (report_id) {
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
        case ZMK_HID_REPORT_ID_LEDS:
            if (report_len != sizeof(struct zmk_hid_led_report)) {
                LOG_ERR("LED set report is malformed: length=%d", report_len);
                return;
            }
            struct zmk_hid_led_report *led_report = (struct zmk_hid_led_report *)report;
            struct zmk_endpoint_instance ep = {
                .transport = ZMK_TRANSPORT_USB,
            };
            zmk_hid_indicators_process_report(&led_report->body, ep);
            break;
#endif
        default:
            LOG_ERR("Invalid report ID %d for output", report_id);
            break;
        }
        break;

    default:
        LOG_ERR("Unsupported report type %d", report_type);
        break;
    }
}

void usbd_hid_set_idle(uint8_t busid, uint8_t intf, uint8_t report_id, uint8_t duration) {
    (void)busid;
    (void)intf;
    (void)report_id;
    (void)duration;
}

uint8_t usbd_hid_get_idle(uint8_t busid, uint8_t intf, uint8_t report_id) {
    (void)busid;
    (void)intf;
    (void)report_id;
    return 0;
}

uint8_t usbd_hid_get_protocol(uint8_t busid, uint8_t intf) {
    (void)busid;
    (void)intf;
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
    return hid_protocol;
#else
    return 1; /* Report protocol */
#endif
}

void usbd_hid_set_protocol(uint8_t busid, uint8_t intf, uint8_t protocol) {
    (void)busid;
    (void)intf;
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
    hid_protocol = protocol;
#endif
}

/* ---- Report sending ---- */

static int zmk_usb_hid_send_report(const uint8_t *report, size_t len) {
    if (!zmk_usb_is_hid_ready()) {
        if (zmk_usb_get_conn_state() == ZMK_USB_CONN_HID) {
            /* Suspended - try remote wakeup */
            return usbd_send_remote_wakeup(CHERRYUSB_BUS_ID);
        }
        return -ENODEV;
    }

    if (len > sizeof(hid_write_buffer)) {
        return -EINVAL;
    }

    k_sem_take(&zmk_cherryusb_hid_sem, K_MSEC(30));
    memcpy(hid_write_buffer, report, len);
    int err = usbd_ep_start_write(CHERRYUSB_BUS_ID, HID_INT_EP, hid_write_buffer, len);
    if (err) {
        k_sem_give(&zmk_cherryusb_hid_sem);
    }
    return err;
}

int zmk_usb_hid_send_keyboard_report(void) {
    size_t len;
    uint8_t *report = get_keyboard_report(&len);
    return zmk_usb_hid_send_report(report, len);
}

int zmk_usb_hid_send_consumer_report(void) {
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
    if (hid_protocol != 1) {
        return -ENOTSUP;
    }
#endif
    struct zmk_hid_consumer_report *report = zmk_hid_get_consumer_report();
    return zmk_usb_hid_send_report((uint8_t *)report, sizeof(*report));
}

#if IS_ENABLED(CONFIG_ZMK_POINTING)
int zmk_usb_hid_send_mouse_report(void) {
#if IS_ENABLED(CONFIG_ZMK_USB_BOOT)
    if (hid_protocol != 1) {
        return -ENOTSUP;
    }
#endif
    struct zmk_hid_mouse_report *report = zmk_hid_get_mouse_report();
    return zmk_usb_hid_send_report((uint8_t *)report, sizeof(*report));
}
#endif /* CONFIG_ZMK_POINTING */