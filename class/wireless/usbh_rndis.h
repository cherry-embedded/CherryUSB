/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_RNDIS_H
#define USBH_RNDIS_H

#include "usb_cdc.h"

struct usbh_rndis {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usb_endpoint_descriptor *intin;   /* INTR endpoint */
    struct usbh_urb bulkin_urb;              /* Bulk IN urb */
    struct usbh_urb bulkout_urb;             /* Bulk OUT urb */
    struct usbh_urb intin_urb;               /* INTR IN urb */

    uint8_t ctrl_intf; /* Control interface number */
    uint8_t data_intf; /* Data interface number */
    uint8_t minor;

    uint32_t request_id;

    uint32_t link_speed;
    bool link_status;
    uint8_t mac[6];
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_rndis_bulk_out_transfer(struct usbh_rndis *rndis_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
int usbh_rndis_bulk_in_transfer(struct usbh_rndis *rndis_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

int usbh_rndis_keepalive(struct usbh_rndis *rndis_class);

void usbh_rndis_run(struct usbh_rndis *rndis_class);
void usbh_rndis_stop(struct usbh_rndis *rndis_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_RNDIS_H */
