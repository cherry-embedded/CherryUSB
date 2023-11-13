/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_CDC_ECM_H
#define USBH_CDC_ECM_H

#include "usb_cdc.h"

#include "lwip/netif.h"
#include "lwip/pbuf.h"

struct usbh_cdc_ecm {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usb_endpoint_descriptor *intin;   /* Interrupt IN endpoint */
    struct usbh_urb bulkout_urb; /* Bulk out endpoint */
    struct usbh_urb bulkin_urb; /* Bulk IN endpoint */
    struct usbh_urb intin_urb; /* Interrupt IN endpoint */

    uint8_t ctrl_intf; /* Control interface number */
    uint8_t data_intf; /* Data interface number */
    uint8_t minor;
    uint8_t mac[6];
    uint32_t max_segment_size;
    uint8_t connect_status;
    uint32_t speed[2];

    ip_addr_t ipaddr;
    ip_addr_t netmask;
    ip_addr_t gateway;
};

#ifdef __cplusplus
extern "C" {
#endif

void usbh_cdc_ecm_run(struct usbh_cdc_ecm *cdc_ecm_class);
void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class);

err_t usbh_cdc_ecm_linkoutput(struct netif *netif, struct pbuf *p);
void usbh_cdc_ecm_lwip_thread_init(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* USBH_CDC_ACM_H */
