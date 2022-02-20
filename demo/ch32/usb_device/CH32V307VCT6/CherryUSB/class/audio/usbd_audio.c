/**
 * @file usbd_audio.c
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
#include "usbd_core.h"
#include "usbd_audio.h"

struct usbd_audio_control_info {
    uint16_t vol_min;
    uint16_t vol_max;
    uint16_t vol_res;
    uint16_t vol_current;
    uint8_t mute;
} audio_control_info = { 0xdb00, 0x0000, 0x0100, 0xf600, 0 };

static int audio_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("AUDIO Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    switch (setup->bRequest) {
        case AUDIO_REQUEST_SET_CUR:

            if (LO_BYTE(setup->wValue) == 0x01) {
                if (HI_BYTE(setup->wValue) == AUDIO_FU_CONTROL_MUTE) {
                    memcpy(&audio_control_info.mute, *data, *len);
                    usbd_audio_set_mute(audio_control_info.mute);
                } else if (HI_BYTE(setup->wValue) == AUDIO_FU_CONTROL_VOLUME) {
                    memcpy(&audio_control_info.vol_current, *data, *len);
                    int vol;
                    if (audio_control_info.vol_current == 0) {
                        vol = 100;
                    } else {
                        vol = (audio_control_info.vol_current - 0xDB00 + 1) * 100 / (0xFFFF - 0xDB00);
                    }
                    usbd_audio_set_volume(vol);
                    USB_LOG_INFO("current audio volume:%d\r\n", vol);
                }
            }

            break;

        case AUDIO_REQUEST_GET_CUR:
            if (HI_BYTE(setup->wValue) == AUDIO_FU_CONTROL_MUTE) {
                *data = (uint8_t *)&audio_control_info.mute;
                *len = 1;
            } else if (HI_BYTE(setup->wValue) == AUDIO_FU_CONTROL_VOLUME) {
                *data = (uint8_t *)&audio_control_info.vol_current;
                *len = 2;
            }

            break;

        case AUDIO_REQUEST_SET_RES:
            break;

        case AUDIO_REQUEST_SET_MEM:
            break;

        case AUDIO_REQUEST_GET_MIN:
            *data = (uint8_t *)&audio_control_info.vol_min;
            *len = 2;
            break;

        case AUDIO_REQUEST_GET_MAX:
            *data = (uint8_t *)&audio_control_info.vol_max;
            *len = 2;
            break;

        case AUDIO_REQUEST_GET_RES:
            *data = (uint8_t *)&audio_control_info.vol_res;
            *len = 2;
            break;
        case AUDIO_REQUEST_GET_MEM:
            *data[0] = 0;
            *len = 1;
            break;

        default:
            USB_LOG_WRN("Unhandled Audio Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void audio_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:

            break;

        case USBD_EVENT_SOF:
            usbd_audio_sof_callback();
            break;

        case USBD_EVENT_SET_INTERFACE:
            struct usb_interface_descriptor *intf = (struct usb_interface_descriptor *)arg;
            usbd_audio_set_interface_callback(intf->bAlternateSetting);
            break;

        default:
            break;
    }
}

void usbd_audio_add_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = audio_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = audio_notify_handler;
    usbd_class_add_interface(devclass, intf);
}

__WEAK void usbd_audio_set_volume(uint8_t vol)
{
}

__WEAK void usbd_audio_set_mute(uint8_t mute)
{
}

__WEAK void usbd_audio_sof_callback(void)
{
}