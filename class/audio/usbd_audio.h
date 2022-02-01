/**
 * @file usbd_audio.h
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
#ifndef _USBD_AUDIO_H_
#define _USBD_AUDIO_H_

#include "usb_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_audio_control_info {
    uint16_t vol_min;
    uint16_t vol_max;
    uint16_t vol_res;
    uint16_t vol_current;
    uint8_t mute;
};

void usbd_audio_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);
void usbd_audio_set_interface_callback(uint8_t value);
void usbd_audio_set_volume(uint8_t vol);
#ifdef __cplusplus
}
#endif

#endif /* _USB_AUDIO_H_ */
