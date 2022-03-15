/**
 * @file usbd_rndis.h
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
#ifndef _USBD_RNDIS_H_
#define _USBD_RNDIS_H_

#include "usb_cdc.h"

#define ETH_HEADER_SIZE 14
#define RNDIS_MTU            1500 /* MTU value */

#ifdef CONFIG_USB_HS
#define RNDIS_LINK_SPEED     12000000 /* Link baudrate (12Mbit/s for USB-FS) */
#else
#define RNDIS_LINK_SPEED     480000000 /* Link baudrate (480Mbit/s for USB-HS) */
#endif

#define CONFIG_RNDIS_RESP_BUFFER_SIZE 128

#ifdef __cplusplus
extern "C" {
#endif

void usbd_rndis_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif