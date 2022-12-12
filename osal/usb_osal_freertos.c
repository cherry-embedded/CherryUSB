/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include <FreeRTOS.h>
#include "semphr.h"
#include "timers.h"
#include "event_groups.h"

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    TaskHandle_t htask = NULL;
    stack_size /= sizeof(StackType_t);
    xTaskCreate(entry, name, stack_size, args, prio, &htask);
    return (usb_osal_thread_t)htask;
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    return (usb_osal_sem_t)xSemaphoreCreateCounting(1, initial_count);
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    vSemaphoreDelete((SemaphoreHandle_t)sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    return (xSemaphoreTake((SemaphoreHandle_t)sem, pdMS_TO_TICKS(timeout)) == pdPASS) ? 0 : -ETIMEDOUT;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    ret = xSemaphoreGiveFromISR((SemaphoreHandle_t)sem, &xHigherPriorityTaskWoken);
    if (ret == pdPASS) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    return (ret == pdPASS) ? 0 : -ETIMEDOUT;
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
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdPASS) ? 0 : -ETIMEDOUT;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdPASS) ? 0 : -ETIMEDOUT;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    return (usb_osal_mq_t)xQueueCreate(max_msgs, 4);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uint32_t addr)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    ret = xQueueSendFromISR((usb_osal_mq_t)mq, &addr, &xHigherPriorityTaskWoken);
    if (ret == pdPASS) {
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

    return (ret == pdPASS) ? 0 : -ETIMEDOUT;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uint32_t *addr, uint32_t timeout)
{
    return (xQueueReceive((usb_osal_mq_t)mq, addr, timeout) == pdPASS) ? 0 : -ETIMEDOUT;
}

size_t usb_osal_enter_critical_section(void)
{
    taskDISABLE_INTERRUPTS();
    return 1;
}

void usb_osal_leave_critical_section(size_t flag)
{
    taskENABLE_INTERRUPTS();
}

void usb_osal_msleep(uint32_t delay)
{
    vTaskDelay(pdMS_TO_TICKS(delay));
}
