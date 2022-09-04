/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_MTP_H
#define USBH_MTP_H

#include "usb_mtp.h"

struct usbh_mtp {
    struct usbh_hubport *hport;

    uint8_t intf;          /* interface number */
    usbh_pipe_t bulkin;  /* BULK IN endpoint */
    usbh_pipe_t bulkout; /* BULK OUT endpoint */
#ifdef CONFIG_USBHOST_MTP_NOTIFY
    usbh_pipe_t intin; /* Interrupt IN endpoint (optional) */
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* USBH_MTP_H */
