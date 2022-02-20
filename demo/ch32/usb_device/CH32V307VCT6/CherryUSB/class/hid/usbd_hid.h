/**
 * @file usbd_hid.h
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
#ifndef _USBD_HID_H_
#define _USBD_HID_H_

#include "usb_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_hid_descriptor_register(uint8_t intf_num, const uint8_t *desc);
void usbd_hid_report_descriptor_register(uint8_t intf_num, const uint8_t *desc, uint32_t desc_len);
void usbd_hid_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);
void usbd_hid_reset_state(void);
void usbd_hid_send_report(uint8_t ep, uint8_t *data, uint8_t len);

void usbd_hid_set_request_callback(uint8_t intf_num,
                                   uint8_t (*get_report_callback)(uint8_t report_id, uint8_t report_type),
                                   void (*set_report_callback)(uint8_t report_id, uint8_t report_type, uint8_t *report, uint8_t report_len),
                                   uint8_t (*get_idle_callback)(uint8_t report_id),
                                   void (*set_idle_callback)(uint8_t report_id, uint8_t duration),
                                   void (*set_protocol_callback)(uint8_t protocol),
                                   uint8_t (*get_protocol_callback)(void));

#ifdef __cplusplus
}
#endif

#endif /* _USBD_HID_H_ */
