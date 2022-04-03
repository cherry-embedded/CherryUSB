/**
 * @file usb_osal_rtthread.c
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
#include "usb_osal.h"
#include "usb_errno.h"
#include <rtthread.h>
#include <rthw.h>

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    rt_thread_t htask;
    htask = rt_thread_create(name, entry, args, stack_size, prio, 10);
    rt_thread_startup(htask);
    return (usb_osal_thread_t)htask;
}

void usb_osal_thread_suspend(usb_osal_thread_t thread)
{
    rt_thread_suspend(thread);
}

void usb_osal_thread_resume(usb_osal_thread_t thread)
{
    rt_thread_resume(thread);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    return (usb_osal_sem_t)rt_sem_create("usbh_sem", initial_count, RT_IPC_FLAG_FIFO);
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    rt_sem_delete((rt_sem_t)sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    int ret = 0;
    rt_err_t result = RT_EOK;

    result = rt_sem_take((rt_sem_t)sem, rt_tick_from_millisecond(timeout));
    if (result == RT_ETIMEOUT) {
        ret = -ETIMEDOUT;
    } else if (result == RT_ERROR) {
        ret = -EINVAL;
    } else {
        ret = 0;
    }

    return (int)ret;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    return (int)rt_sem_release((rt_sem_t)sem);
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    return (usb_osal_mutex_t)rt_mutex_create("usbh_mutex", RT_IPC_FLAG_FIFO);
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    rt_mutex_delete((rt_mutex_t)mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    return (int)rt_mutex_take((rt_mutex_t)mutex, RT_WAITING_FOREVER);
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (int)rt_mutex_release((rt_mutex_t)mutex);
}

usb_osal_event_t usb_osal_event_create(void)
{
    return (usb_osal_event_t)rt_event_create("psc_event", RT_IPC_FLAG_FIFO);
}

void usb_osal_event_delete(usb_osal_event_t event)
{
    rt_event_delete((rt_event_t)event);
}

int usb_osal_event_recv(usb_osal_event_t event, uint32_t set, uint32_t *recved)
{
    int ret = 0;
    rt_err_t result = RT_EOK;

    result = rt_event_recv((rt_event_t)event, set, RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, RT_WAITING_FOREVER, (rt_uint32_t *)recved);
    if (result == RT_ETIMEOUT) {
        ret = -ETIMEDOUT;
    } else if (result == RT_ERROR) {
        ret = -EINVAL;
    } else {
        ret = 0;
    }

    return ret;
}

int usb_osal_event_send(usb_osal_event_t event, uint32_t set)
{
    int ret = 0;
    rt_err_t result = RT_EOK;

    result = rt_event_send((rt_event_t)event, set);
    if (result != RT_EOK) {
        ret = -EINVAL;
    }

    return ret;
}

size_t usb_osal_enter_critical_section(void)
{
    return rt_hw_interrupt_disable();
}

void usb_osal_leave_critical_section(size_t flag)
{
    rt_hw_interrupt_enable(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    rt_thread_mdelay(delay);
}
