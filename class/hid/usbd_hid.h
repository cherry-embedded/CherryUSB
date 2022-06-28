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
uint8_t usbh_hid_get_report(uint8_t intf, uint8_t report_id, uint8_t report_type);
uint8_t usbh_hid_get_idle(uint8_t intf, uint8_t report_id);
uint8_t usbh_hid_get_protocol(uint8_t intf);
void usbh_hid_set_report(uint8_t intf, uint8_t report_id, uint8_t report_type, uint8_t *report, uint8_t report_len);
void usbh_hid_set_idle(uint8_t intf, uint8_t report_id, uint8_t duration);
void usbh_hid_set_protocol(uint8_t intf, uint8_t protocol);

#ifdef __cplusplus
}
#endif

#endif /* _USBD_HID_H_ */
