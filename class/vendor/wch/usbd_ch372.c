/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_ch372.h"

struct usbd_interface *usbd_ch372_init_intf(uint8_t busid, struct usbd_interface *intf)
{
    (void)busid;

    intf->class_interface_handler = NULL;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = NULL;

    return intf;
}
