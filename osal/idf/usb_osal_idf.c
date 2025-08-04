/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"

static portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    TaskHandle_t htask = NULL;
    stack_size /= sizeof(StackType_t);
    xTaskCreatePinnedToCore(entry, name, stack_size, args, configMAX_PRIORITIES - 1 - prio, &htask, CONFIG_ESP_TIMER_TASK_AFFINITY);
    return (usb_osal_thread_t)htask;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    vTaskDelete(thread);
}

void usb_osal_thread_schedule_other(void)
{
    TaskHandle_t xCurrentTask = xTaskGetCurrentTaskHandle();
    const UBaseType_t old_priority = uxTaskPriorityGet(xCurrentTask);

    vTaskPrioritySet(xCurrentTask, tskIDLE_PRIORITY);

    taskYIELD();

    vTaskPrioritySet(xCurrentTask, old_priority);
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
    if (timeout == USB_OSAL_WAITING_FOREVER) {
        return (xSemaphoreTake((SemaphoreHandle_t)sem, portMAX_DELAY) == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
    } else {
        return (xSemaphoreTake((SemaphoreHandle_t)sem, pdMS_TO_TICKS(timeout)) == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
    }
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    if (xPortInIsrContext()) {
        ret = xSemaphoreGiveFromISR((SemaphoreHandle_t)sem, &xHigherPriorityTaskWoken);
        if (ret == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else {
        ret = xSemaphoreGive((SemaphoreHandle_t)sem);
    }

    return (ret == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
    xQueueReset((QueueHandle_t)sem);
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
    return (xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (xSemaphoreGive((SemaphoreHandle_t)mutex) == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    return (usb_osal_mq_t)xQueueCreate(max_msgs, sizeof(uintptr_t));
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    vQueueDelete((QueueHandle_t)mq);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    if (xPortInIsrContext()) {
        ret = xQueueSendFromISR((usb_osal_mq_t)mq, &addr, &xHigherPriorityTaskWoken);
        if (ret == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else {
        ret = xQueueSend((usb_osal_mq_t)mq, &addr, 0xffffffff);
    }

    return (ret == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    if (xPortInIsrContext()) {
        ret = xQueueReceiveFromISR((usb_osal_mq_t)mq, addr, &xHigherPriorityTaskWoken);
        if (ret == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        return (ret == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
    } else {
        if (timeout == USB_OSAL_WAITING_FOREVER) {
            return (xQueueReceive((usb_osal_mq_t)mq, addr, portMAX_DELAY) == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
        } else {
            return (xQueueReceive((usb_osal_mq_t)mq, addr, pdMS_TO_TICKS(timeout)) == pdPASS) ? 0 : -USB_ERR_TIMEOUT;
        }
    }
}

static void usb_timeout(void *arg)
{
    struct usb_osal_timer *timer = (struct usb_osal_timer *)arg;

    timer->handler(timer->argument);
}

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period)
{
    struct usb_osal_timer *timer;

    timer = pvPortMalloc(sizeof(struct usb_osal_timer));

    if (timer == NULL) {
        return NULL;
    }

    esp_timer_create_args_t timer_args = {
        .callback = usb_timeout,
        // argument specified here will be passed to timer callback function
        .arg = (void *)timer,
        .dispatch_method = ESP_TIMER_TASK,
        .name = name,
        .skip_unhandled_events = true
    };

    timer->handler = handler;
    timer->argument = argument;
    timer->is_period = is_period;
    timer->timeout_ms = timeout_ms;

    if (esp_timer_create(&timer_args, (esp_timer_handle_t *)&timer->timer) != ESP_OK) {
        vPortFree(timer);
        return NULL;
    }

    return timer;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    esp_timer_stop((esp_timer_handle_t)timer->timer);
    esp_timer_delete((esp_timer_handle_t)timer->timer);
    vPortFree(timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    esp_timer_stop((esp_timer_handle_t)timer->timer);

    if (timer->is_period) {
        esp_timer_start_periodic((esp_timer_handle_t)timer->timer, ((uint64_t)timer->timeout_ms) * 1000);
    } else {
        esp_timer_start_once((esp_timer_handle_t)timer->timer, ((uint64_t)timer->timeout_ms) * 1000);
    }
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    esp_timer_stop((esp_timer_handle_t)timer->timer);
}

size_t usb_osal_enter_critical_section(void)
{
    portENTER_CRITICAL_SAFE(&spinlock);
    return 0;
}

void usb_osal_leave_critical_section(size_t flag)
{
    portEXIT_CRITICAL_SAFE(&spinlock);
}

void usb_osal_msleep(uint32_t delay)
{
    vTaskDelay(pdMS_TO_TICKS(delay));
}

void *usb_osal_malloc(size_t size)
{
    return malloc(size);
}

void usb_osal_free(void *ptr)
{
    free(ptr);
}