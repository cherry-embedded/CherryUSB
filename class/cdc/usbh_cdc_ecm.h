/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_CDC_ECM_H
#define USBH_CDC_ECM_H

#include "usb_cdc.h"

struct usbh_cdc_ecm {
    struct usbh_hubport *hport;

    uint8_t ctrl_intf; /* Control interface number */
    uint8_t data_intf; /* Data interface number */
    uint8_t minor;
    uint8_t mac[6];
    usbh_pipe_t bulkin;  /* Bulk IN endpoint */
    usbh_pipe_t bulkout; /* Bulk OUT endpoint */
    usbh_pipe_t intin;   /* Interrupt IN endpoint */
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_cdc_ecm_set_eth_packet_filter(struct usbh_cdc_ecm *cdc_ecm_class, uint16_t filter_value);

void usbh_cdc_ecm_run(struct usbh_cdc_ecm *cdc_ecm_class);
void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_CDC_ACM_H */
