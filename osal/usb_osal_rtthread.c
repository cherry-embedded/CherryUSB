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
#include <rtthread.h>

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry)
{
    rt_thread_t htask;
    htask = rt_thread_create(name, entry, NULL, stack_size, prio, 10);
    rt_thread_startup(htask);
    return (usb_osal_thread_t)htask;
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    static uint16_t index = 0;
    char str[16];

    sprintf(str, "usbh_sem%u", index);
    index++;
    return (usb_osal_sem_t)rt_sem_create(str, initial_count, RT_IPC_FLAG_FIFO);
}

int usb_osal_sem_take(usb_osal_sem_t sem)
{
    return (int)rt_sem_take(sem, RT_WAITING_FOREVER);
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    return (int)rt_sem_release(sem);
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    static uint16_t index = 0;
    char str[16];

    sprintf(str, "usbh_mutex%u", index);
    index++;
    return (usb_osal_mutex_t)rt_mutex_create(str, RT_IPC_FLAG_FIFO);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    return (int)rt_mutex_take(mutex, RT_WAITING_FOREVER);
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (int)rt_mutex_release(mutex);
}

uint32_t usb_osal_enter_critical_section(void)
{
    rt_enter_critical();
    return 1;
}

void usb_osal_leave_critical_section(uint32_t flag)
{
    rt_exit_critical();
}

void usb_osal_delay_ms(uint32_t delay)
{
    rt_thread_mdelay(delay);
}