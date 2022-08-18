/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_VIDEO_H
#define USBD_VIDEO_H

#include "usb_video.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_video_add_interface(usbd_class_t *devclass, usbd_interface_t *intf);

void usbd_video_open(uint8_t intf);
void usbd_video_close(uint8_t intf);
void usbd_video_probe_and_commit_controls_init(uint32_t dwFrameInterval, uint32_t dwMaxVideoFrameSize, uint32_t dwMaxPayloadTransferSize);
uint32_t usbd_video_mjpeg_payload_fill(uint8_t *input, uint32_t input_len, uint8_t *output, uint32_t *out_len);
void usbd_video_mjpeg_payload_header_toggle(uint8_t *output, uint32_t packets);

#ifdef __cplusplus
}
#endif

#endif /* USBD_VIDEO_H */
