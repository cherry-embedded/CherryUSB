/**
 * @file usb_osal_freertos.c
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
#include <FreeRTOS.h>
#include "semphr.h"

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry)
{
    TaskHandle_t htask = NULL;
    stack_size /= sizeof(StackType_t);
    xTaskCreate(entry, name, stack_size, NULL, prio, &htask);
    return (usb_osal_thread_t)htask;
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    return (usb_osal_sem_t)xSemaphoreCreateCounting(1, initial_count);
}

int usb_osal_sem_take(usb_osal_sem_t sem)
{
    return (xSemaphoreTake((SemaphoreHandle_t)sem, portMAX_DELAY) == pdPASS) ? 0 : -1;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    return (xSemaphoreGive((SemaphoreHandle_t)sem) == pdPASS) ? 0 : -1;

    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // int ret = xSemaphoreGiveFromISR((SemaphoreHandle_t)sem, &xHigherPriorityTaskWoken);

    // if (ret == pdPASS) {
    //     portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    // }
    // return (ret == pdPASS) ? 0 : -1;
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    return (usb_osal_mutex_t)xSemaphoreCreateMutex();
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdPASS) ? 0 : -1;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdPASS) ? 0 : -1;
}

uint32_t usb_osal_enter_critical_section(void)
{
    taskENTER_CRITICAL();
    return 1;
}

void usb_osal_leave_critical_section(uint32_t flag)
{
    taskEXIT_CRITICAL();
}

void usb_osal_delay_ms(uint32_t delay)
{
    vTaskDelay(delay);
}