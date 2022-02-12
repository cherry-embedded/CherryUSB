/**
 * @file usbd_video.c
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
#include "usbd_video.h"

struct usbd_video_cfg_priv {
    struct video_entity_info *entity_info;
    uint8_t entity_count;
    struct video_probe_and_commit_controls *probe;
    struct video_probe_and_commit_controls *commit;
    uint8_t power_mode;
    uint8_t error_code;
    uint8_t vcintf;
    uint8_t vsintf;
} usbd_video_cfg = { .vcintf = 0xff, .vsintf = 0xff };

static int usbd_video_control_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t cs = (uint8_t)(setup->wValue >> 8); /* control_selector */

    switch (cs) {
        case VIDEO_VC_VIDEO_POWER_MODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }

            break;
        case VIDEO_VC_REQUEST_ERROR_CODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_GET_CUR:
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }

            break;
        default:
            break;
    }

    return 0;
}

static int usbd_video_control_unit_terminal_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t entity_id = (uint8_t)(setup->wIndex >> 8);
    uint8_t cs = (uint8_t)(setup->wValue >> 8); /* control_selector */

    for (uint8_t i = 0; i < usbd_video_cfg.entity_count; i++) {
        if (usbd_video_cfg.entity_info[i].bEntityId == entity_id) {
            switch (usbd_video_cfg.entity_info[i].bDescriptorSubtype) {
                case VIDEO_VC_INPUT_TERMINAL_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_OUTPUT_TERMINAL_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_PROCESSING_UNIT_DESCRIPTOR_SUBTYPE:
                    break;
                    switch (setup->bRequest) {
                        case VIDEO_REQUEST_SET_CUR:
                            if (cs == VIDEO_PU_BRIGHTNESS_CONTROL) {
                            }
                            break;
                        default:
                            USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                            return -1;
                    }
                default:
                    break;
            }
        }
    }
    return 0;
}

static int usbd_video_stream_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t cs = (uint8_t)(setup->wValue >> 8); /* control_selector */

    switch (cs) {
        case VIDEO_VS_PROBE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    memcpy((uint8_t *)usbd_video_cfg.probe, *data, setup->wLength);
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    *data = (uint8_t *)usbd_video_cfg.probe;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;

                case VIDEO_REQUEST_GET_MIN:
                case VIDEO_REQUEST_GET_MAX:
                case VIDEO_REQUEST_GET_RES:
                case VIDEO_REQUEST_GET_DEF:
                    *data = (uint8_t *)usbd_video_cfg.probe;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;
                case VIDEO_REQUEST_GET_LEN:
                    (*data)[0] = sizeof(struct video_probe_and_commit_controls);
                    *len = 1;
                    break;

                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x03;
                    *len = 1;
                    break;

                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        case VIDEO_VS_COMMIT_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    memcpy((uint8_t *)usbd_video_cfg.commit, *data, setup->wLength);
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    *data = (uint8_t *)usbd_video_cfg.commit;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;
                case VIDEO_REQUEST_GET_MIN:
                case VIDEO_REQUEST_GET_MAX:
                case VIDEO_REQUEST_GET_RES:
                case VIDEO_REQUEST_GET_DEF:
                    *data = (uint8_t *)usbd_video_cfg.commit;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;

                case VIDEO_REQUEST_GET_LEN:
                    (*data)[0] = sizeof(struct video_probe_and_commit_controls);
                    *len = 1;
                    break;

                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x03;
                    *len = 1;
                    break;

                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        case VIDEO_VS_STREAM_ERROR_CODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_GET_CUR:
                    *data = &usbd_video_cfg.error_code;
                    *len = 1;
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x01;
                    *len = 1;
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        default:
            break;
    }

    return 0;
}

static int video_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("Video Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    uint8_t intf_num = (uint8_t)setup->wIndex;
    uint8_t entity_id = (uint8_t)(setup->wIndex >> 8);

    if (usbd_video_cfg.vcintf == intf_num) { /* Video Control Interface */
        if (entity_id == 0) {
            return usbd_video_control_request_handler(setup, data, len); /* Interface Control Requests */
        } else {
            return usbd_video_control_unit_terminal_request_handler(setup, data, len); /* Unit and Terminal Requests */
        }
    } else if (usbd_video_cfg.vsintf == intf_num) {                 /* Video Stream Inteface */
        return usbd_video_stream_request_handler(setup, data, len); /* Interface Stream Requests */
    }
    return -1;
}

static void video_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            usbd_video_cfg.error_code = 0;
            usbd_video_cfg.power_mode = 0;
            break;

        case USBD_EVENT_SOF:
            usbd_video_sof_callback();
            break;

        case USBD_EVENT_SET_INTERFACE: {
            struct usb_interface_descriptor *intf = (struct usb_interface_descriptor *)arg;
            usbd_video_set_interface_callback(intf->bAlternateSetting);
        }

        break;
        default:
            break;
    }
}

void usbd_video_add_interface(usbd_class_t *class, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != class) {
        last_class = class;
        usbd_class_register(class);
    }

    intf->class_handler = video_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = video_notify_handler;
    usbd_class_add_interface(class, intf);

    if (usbd_video_cfg.vcintf == 0xff) {
        usbd_video_cfg.vcintf = intf->intf_num;
    } else if (usbd_video_cfg.vsintf == 0xff) {
        usbd_video_cfg.vsintf = intf->intf_num;
    }
}

void usbd_video_set_probe_and_commit_controls(struct video_probe_and_commit_controls *probe,
                                              struct video_probe_and_commit_controls *commit)
{
    usbd_video_cfg.probe = probe;
    usbd_video_cfg.commit = commit;
}

void usbd_video_set_entity_info(struct video_entity_info *info, uint8_t count)
{
    usbd_video_cfg.entity_info = info;
    usbd_video_cfg.entity_count = count;
}

__WEAK void usbd_video_sof_callback(void)
{
}