/*
 * Copyright (c) 2022, aozima
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Change Logs
 * Date           Author       Notes
 * 2022-04-17     aozima       the first version for CherryUSB.
 */

#ifndef __USB_CLASHH_AXUSBNET_H__
#define __USB_CLASHH_AXUSBNET_H__

#include "usbh_core.h"
#include "asix.h"

struct usbh_axusbnet {
    struct usbh_hubport *hport;

    uint8_t intf; /* interface number */

    usbh_pipe_t int_notify; /* Notify endpoint */
    usbh_pipe_t bulkin;  /* Bulk IN endpoint */
    usbh_pipe_t bulkout; /* Bulk OUT endpoint */

    uint32_t bulkin_buf[2048/sizeof(uint32_t)];
};

#endif /* __USB_CLASHH_AXUSBNET_H__ */
