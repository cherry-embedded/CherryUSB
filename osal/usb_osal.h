/**
 * @file usb_osal.h
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
#ifndef _USB_OSAL_H
#define _USB_OSAL_H

#include <stdint.h>
#include <string.h>

typedef void *usb_osal_thread_t;
typedef void *usb_osal_sem_t;
typedef void *usb_osal_mutex_t;
typedef void *usb_osal_event_t;
typedef void (*usb_thread_entry_t)(void *argument);

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args);
void usb_osal_thread_delete(usb_osal_thread_t thread);
void usb_osal_thread_suspend(usb_osal_thread_t thread);
void usb_osal_thread_resume(usb_osal_thread_t thread);

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count);
void usb_osal_sem_delete(usb_osal_sem_t sem);
int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout);
int usb_osal_sem_give(usb_osal_sem_t sem);

usb_osal_mutex_t usb_osal_mutex_create(void);
void usb_osal_mutex_delete(usb_osal_mutex_t mutex);
int usb_osal_mutex_take(usb_osal_mutex_t mutex);
int usb_osal_mutex_give(usb_osal_mutex_t mutex);

usb_osal_event_t usb_osal_event_create(void);
void usb_osal_event_delete(usb_osal_event_t event);
int usb_osal_event_recv(usb_osal_event_t event, uint32_t set, uint32_t *recved);
int usb_osal_event_send(usb_osal_event_t event, uint32_t set);

size_t usb_osal_enter_critical_section(void);
void usb_osal_leave_critical_section(size_t flag);

void usb_osal_msleep(uint32_t delay);

#endif

