/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_AIC8800_H
#define USBH_AIC8800_H

#include "usbh_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define USBH_AIC8800_VID    0xa69cU
#define USBH_AIC8800_VID_V2 0x368bU

#define USBH_AIC8800_PID_8800       0x8800U
#define USBH_AIC8800_PID_8801       0x8801U
#define USBH_AIC8800_PID_DC         0x88dcU
#define USBH_AIC8800_PID_DW         0x88ddU
#define USBH_AIC8800_PID_D80_BOOT   0x8d80U
#define USBH_AIC8800_PID_D81        0x8d81U
#define USBH_AIC8800_PID_D83        0x8d83U
#define USBH_AIC8800_PID_D84        0x8d84U
#define USBH_AIC8800_PID_D85        0x8d85U
#define USBH_AIC8800_PID_D86        0x8d86U
#define USBH_AIC8800_PID_D88        0x8d88U
#define USBH_AIC8800_PID_D40_BOOT   0x8d40U
#define USBH_AIC8800_PID_D41        0x8d41U
#define USBH_AIC8800_PID_D80X2_BOOT 0x8d90U
#define USBH_AIC8800_PID_D81X2      0x8d91U
#define USBH_AIC8800_PID_D89X2      0x8d99U

enum usbh_aic8800_family {
    USBH_AIC8800_FAMILY_UNKNOWN = 0,
    USBH_AIC8800_FAMILY_8800,
    USBH_AIC8800_FAMILY_D80,
    USBH_AIC8800_FAMILY_D80X2,
    USBH_AIC8800_FAMILY_DC
};

struct usbh_aic8800 {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *data_in;
    struct usb_endpoint_descriptor *data_out;
    struct usb_endpoint_descriptor *message_in;
    struct usb_endpoint_descriptor *message_out;
    struct usbh_urb tx_urb;
    struct usbh_urb rx_urb;
    enum usbh_aic8800_family family;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t intf;
    bool runtime;
    volatile bool online;
};

struct usbh_aic8800 *usbh_aic8800_get_device(void);

/*
 * Register the 1111:1111 ZeroCD SCSI message. Call once before
 * usbh_initialize(). Requires CONFIG_USBHOST_AIC8800_ZEROCD and the
 * MSC modeswitch macros in usb_config.h.
 */
void usbh_aic8800_enable_zero_cd_modeswitch(void);

int usbh_aic8800_bulk_send(struct usbh_aic8800 *aic8800_class,
                           const void *data, uint32_t length,
                           uint32_t timeout_ms, bool message_pipe);
int usbh_aic8800_bulk_receive(struct usbh_aic8800 *aic8800_class,
                              void *data, uint32_t capacity,
                              uint32_t timeout_ms, bool message_pipe);
int usbh_aic8800_bulk_receive_async(struct usbh_aic8800 *aic8800_class,
                                    void *data, uint32_t capacity,
                                    usbh_complete_callback_t complete,
                                    void *arg, bool message_pipe);
void usbh_aic8800_abort_receive(struct usbh_aic8800 *aic8800_class);

void usbh_aic8800_run(struct usbh_aic8800 *aic8800_class);
void usbh_aic8800_stop(struct usbh_aic8800 *aic8800_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_AIC8800_H */
