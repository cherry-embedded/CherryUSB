/**
 * @file usbh_mtp.h
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#ifndef _USBH_MTP_H
#define _USBH_MTP_H

#include "usb_mtp.h"

struct usbh_mtp {
    struct usbh_hubport *hport;

    uint8_t intf;          /* interface number */
    usbh_epinfo_t bulkin;  /* BULK IN endpoint */
    usbh_epinfo_t bulkout; /* BULK OUT endpoint */
#ifdef CONFIG_USBHOST_MTP_NOTIFY
    usbh_epinfo_t intin; /* Interrupt IN endpoint (optional) */
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif