/**
 * @file usbd_video.h
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
#ifndef _USBD_VIDEO_H_
#define _USBD_VIDEO_H_

#include "usb_video.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_video_add_interface(usbd_class_t *class, usbd_interface_t *intf);

void usbd_video_set_probe_and_commit_controls(struct video_probe_and_commit_controls *probe,
                                              struct video_probe_and_commit_controls *commit);
void usbd_video_add_entity_info(struct video_entity_info *info);
void usbd_video_sof_callback(void);
void usbd_video_set_interface_callback(uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* USBD_VIDEO_H_ */
