/**
 * @file usb_workq.c
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
#include "usb_list.h"
#include "usb_osal.h"
#include "usb_workq.h"
#include "usb_config.h"

void usb_workqueue_submit(struct usb_workqueue *queue, struct usb_work *work, usb_worker_t worker, void *arg, uint32_t ticks)
{
    size_t flags;
    flags = usb_osal_enter_critical_section();
    usb_dlist_remove(&work->list);
    work->worker = worker;
    work->arg = arg;

    if (ticks == 0) {
        usb_dlist_insert_after(&queue->work_list, &work->list);
        usb_osal_sem_give(queue->sem);
    }

    usb_osal_leave_critical_section(flags);
}

struct usb_workqueue g_hpworkq = { NULL };
struct usb_workqueue g_lpworkq = { NULL };

static void usbh_hpwork_thread(void *argument)
{
    struct usb_work *work;
    size_t flags;
    int ret;
    struct usb_workqueue *queue = (struct usb_workqueue *)argument;
    while (1) {
        ret = usb_osal_sem_take(queue->sem, 0xffffffff);
        if (ret < 0) {
            continue;
        }
        flags = usb_osal_enter_critical_section();
        if (usb_dlist_isempty(&queue->work_list)) {
            usb_osal_leave_critical_section(flags);
            continue;
        }
        work = usb_dlist_first_entry(&queue->work_list, struct usb_work, list);
        usb_dlist_remove(&work->list);
        usb_osal_leave_critical_section(flags);
        work->worker(work->arg);
    }
}

static void usbh_lpwork_thread(void *argument)
{
    struct usb_work *work;
    size_t flags;
    int ret;
    struct usb_workqueue *queue = (struct usb_workqueue *)argument;
    while (1) {
        ret = usb_osal_sem_take(queue->sem, 0xffffffff);
        if (ret < 0) {
            continue;
        }
        flags = usb_osal_enter_critical_section();
        if (usb_dlist_isempty(&queue->work_list)) {
            usb_osal_leave_critical_section(flags);
            continue;
        }
        work = usb_dlist_first_entry(&queue->work_list, struct usb_work, list);
        usb_dlist_remove(&work->list);
        usb_osal_leave_critical_section(flags);
        work->worker(work->arg);
    }
}

int usbh_workq_initialize(void)
{
#ifdef CONFIG_USBHOST_HIGH_WORKQ
    g_hpworkq.sem = usb_osal_sem_create(0);
    if (g_hpworkq.sem == NULL) {
        return -1;
    }
    g_hpworkq.thread = usb_osal_thread_create("usbh_hpworkq", CONFIG_USBHOST_HPWORKQ_STACKSIZE, CONFIG_USBHOST_HPWORKQ_PRIO, usbh_hpwork_thread, &g_hpworkq);
    if (g_hpworkq.thread == NULL) {
        return -1;
    }
#endif
#ifdef CONFIG_USBHOST_LOW_WORKQ
    g_lpworkq.sem = usb_osal_sem_create(0);
    if (g_lpworkq.sem == NULL) {
        return -1;
    }

    g_lpworkq.thread = usb_osal_thread_create("usbh_lpworkq", CONFIG_USBHOST_LPWORKQ_STACKSIZE, CONFIG_USBHOST_LPWORKQ_PRIO, usbh_lpwork_thread, &g_lpworkq);
    if (g_lpworkq.thread == NULL) {
        return -1;
    }
#endif
    return 0;
}
