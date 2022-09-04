/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_PRINTER_H
#define USBH_PRINTER_H

#include "usb_printer.h"

struct usbh_printer {
    struct usbh_hubport *hport;

    uint8_t intf;          /* interface number */
    usbh_pipe_t bulkin;  /* BULK IN endpoint */
    usbh_pipe_t bulkout; /* BULK OUT endpoint */
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* USBH_PRINTER_H */
