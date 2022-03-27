/**
 * @file usb_workq.h
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
#ifndef _USB_WORKQUEUE_H
#define _USB_WORKQUEUE_H

/* Defines the work callback */
typedef void (*usb_worker_t)(void *arg);

struct usb_work
{
    usb_dlist_t list;
    usb_worker_t worker;
    void *arg;
};

struct usb_workqueue
{
    usb_dlist_t work_list;
    usb_dlist_t delay_work_list;
    usb_osal_sem_t sem;
    usb_osal_thread_t thread;
};

extern struct usb_workqueue g_hpworkq;
extern struct usb_workqueue g_lpworkq;

void usb_workqueue_submit(struct usb_workqueue *queue, struct usb_work *work, usb_worker_t worker, void *arg, uint32_t ticks);
int usbh_workq_initialize();
#endif
