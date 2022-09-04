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

    uint8_t ctrl_intf; /* Control interface number */
    uint8_t data_intf; /* Data interface number */

    usbh_pipe_t bulkin;  /* Bulk IN endpoint */
    usbh_pipe_t bulkout; /* Bulk OUT endpoint */
    usbh_pipe_t intin;   /* Notify endpoint */

    uint32_t request_id;
    uint8_t mac[6];
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_rndis_keepalive(struct usbh_rndis *rndis_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_RNDIS_H */
