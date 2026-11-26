/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_CDC_ECM_H
#define USBH_CDC_ECM_H

#include "usb_cdc.h"

#define USBH_CDC_ECM_LINK_STATE_DOWN         0
#define USBH_CDC_ECM_LINK_STATE_UP           1
#define USBH_CDC_ECM_LINK_STATE_PENDING_UP   2
#define USBH_CDC_ECM_LINK_STATE_PENDING_DOWN 3

struct usbh_cdc_ecm {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usb_endpoint_descriptor *intin;   /* Interrupt IN endpoint */
    struct usbh_urb bulkout_urb;             /* Bulk out endpoint */
    struct usbh_urb bulkin_urb;              /* Bulk IN endpoint */
    struct usbh_urb intin_urb;               /* Interrupt IN endpoint */

    uint8_t ctrl_intf; /* Control interface number */
    uint8_t data_intf; /* Data interface number */
    uint8_t minor;

    uint16_t max_segment_size;

    uint8_t mac[6];
    uint8_t link_state;
    uint32_t link_speed;
    usb_osal_sem_t link_sem;
    struct usb_osal_timer *link_timer;
    usb_osal_sem_t rx_sem;
    usb_osal_thread_t thread;

    void *user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

void usbh_cdc_ecm_run(struct usbh_cdc_ecm *cdc_ecm_class);
void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class);

uint8_t *usbh_cdc_ecm_get_eth_txbuf(void);
int usbh_cdc_ecm_eth_output(uint32_t buflen);
void usbh_cdc_ecm_eth_input(uint8_t *buf, uint32_t buflen);
void usbh_cdc_ecm_link_up(bool up);

#ifdef __cplusplus
}
#endif

#endif /* USBH_CDC_ECM_H */
