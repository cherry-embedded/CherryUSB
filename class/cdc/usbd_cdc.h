/**
 * @file usbd_cdc.h
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
#ifndef _USBD_CDC_H
#define _USBD_CDC_H

#include "usb_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_cdc_add_acm_interface(usbd_class_t *devclass, usbd_interface_t *intf);

void usbd_cdc_acm_set_line_coding(uint32_t baudrate, uint8_t databits, uint8_t parity, uint8_t stopbits);
void usbd_cdc_acm_set_dtr(bool dtr);
void usbd_cdc_acm_set_rts(bool rts);

#ifdef __cplusplus
}
#endif

#endif /* USBD_CDC_H_ */
