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

void usbd_audio_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

void usbd_audio_open(uint8_t intf);
void usbd_audio_close(uint8_t intf);
void usbd_audio_add_entity(uint8_t entity_id, uint16_t bDescriptorSubtype);
void usbd_audio_set_volume(uint8_t entity_id, uint8_t ch, float dB);
void usbd_audio_set_mute(uint8_t entity_id, uint8_t ch, uint8_t enable);
void usbd_audio_set_sampling_freq(uint8_t entity_id, uint8_t ep_ch, uint32_t sampling_freq);
void usbd_audio_get_sampling_freq_table(uint8_t entity_id, uint8_t **sampling_freq_table);
void usbd_audio_set_pitch(uint8_t ep, bool enable);
void usbd_audio_sof_callback(void);

#ifdef __cplusplus
}
#endif

#endif /* _USB_AUDIO_H_ */
