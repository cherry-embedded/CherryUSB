/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_CDC_ACM_H
#define USBH_CDC_ACM_H

#include "usb_cdc.h"

struct usbh_cdc_acm {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usb_endpoint_descriptor *intin; /* INTR IN endpoint (optional) */
    struct usbh_urb bulkout_urb;
    struct usbh_urb bulkin_urb;
    struct usbh_urb intin_urb;

    struct cdc_line_coding linecoding;
    uint8_t ctrl_intf; /* Control interface number */
    uint8_t data_intf; /* Data interface number */
    bool dtr;
    bool rts;
    uint8_t minor;
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_cdc_acm_set_line_coding(struct usbh_cdc_acm *cdc_acm_class, struct cdc_line_coding *line_coding);
int usbh_cdc_acm_get_line_coding(struct usbh_cdc_acm *cdc_acm_class, struct cdc_line_coding *line_coding);
int usbh_cdc_acm_set_line_state(struct usbh_cdc_acm *cdc_acm_class, bool dtr, bool rts);

int usbh_cdc_acm_bulk_in_transfer(struct usbh_cdc_acm *cdc_acm_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
int usbh_cdc_acm_bulk_out_transfer(struct usbh_cdc_acm *cdc_acm_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

void usbh_cdc_acm_run(struct usbh_cdc_acm *cdc_acm_class);
void usbh_cdc_acm_stop(struct usbh_cdc_acm *cdc_acm_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_CDC_ACM_H */
