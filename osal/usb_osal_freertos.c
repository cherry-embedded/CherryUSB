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

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    TaskHandle_t htask = NULL;
    stack_size /= sizeof(StackType_t);
    xTaskCreate(entry, name, stack_size, args, prio, &htask);
    return (usb_osal_thread_t)htask;
}

void usb_osal_thread_suspend(usb_osal_thread_t thread)
{
    vTaskSuspend(thread);
}

void usb_osal_thread_resume(usb_osal_thread_t thread)
{
    vTaskResume(thread);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    return (usb_osal_sem_t)xSemaphoreCreateCounting(1, initial_count);
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    vSemaphoreDelete((SemaphoreHandle_t)sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem)
{
    return (xSemaphoreTake((SemaphoreHandle_t)sem, portMAX_DELAY) == pdPASS) ? 0 : -1;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t intstatus = 1;
    int ret;
    /* Obtain the level of the currently executing interrupt. */
//    __asm volatile("csrr %0, mintstatus"
//                   : "=r"(intstatus)::"memory");
        /* Obtain the number of the currently executing interrupt. */
//        __asm volatile ( "mrs %0, ipsr" : "=r" ( intstatus )::"memory" );

    if (intstatus == 0) {
        ret = xSemaphoreGive((SemaphoreHandle_t)sem);
    } else {
        ret = xSemaphoreGiveFromISR((SemaphoreHandle_t)sem, &xHigherPriorityTaskWoken);
        if (ret == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }

    return (ret == pdPASS) ? 0 : -1;
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    return (usb_osal_mutex_t)xSemaphoreCreateMutex();
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    vSemaphoreDelete((SemaphoreHandle_t)mutex);
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

void usb_osal_msleep(uint32_t delay)
{
    vTaskDelay(pdMS_TO_TICKS(delay));
}

uint32_t usb_osal_get_tick(void)
{
    return xTaskGetTickCount();
}
