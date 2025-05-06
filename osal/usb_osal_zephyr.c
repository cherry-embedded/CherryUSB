/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include "usb_config.h"
#include "usb_log.h"
#include <zephyr/kernel.h>

#ifndef CONFIG_THREAD_CUSTOM_DATA
#error CONFIG_THREAD_CUSTOM_DATA must be enabled
#endif

struct k_thread_ext {
    struct k_thread thread;
    k_thread_stack_t *thread_stack;
};

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    struct k_thread_ext *new_thread;

    new_thread = k_malloc(sizeof(struct k_thread_ext));
    new_thread->thread_stack = k_malloc(stack_size);
    k_thread_create(&new_thread->thread, new_thread->thread_stack, stack_size, entry, args, NULL, NULL, prio, 0, K_NO_WAIT);
    k_thread_custom_data_set((void *)new_thread);
    k_thread_name_set(&new_thread->thread, name);

    return (usb_osal_thread_t)new_thread;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    struct k_thread_ext *new_thread;

    if (thread == NULL) {
        new_thread = (struct k_thread_ext *)k_thread_custom_data_get();
    } else {
        new_thread = (struct k_thread_ext *)thread;
    }

    k_thread_abort(&new_thread->thread);
    k_free(new_thread->thread_stack);
    k_free(new_thread);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    struct k_sem *sem;

    sem = k_malloc(sizeof(struct k_sem));
    k_sem_init(sem, initial_count, 1);

    return (usb_osal_sem_t)sem;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    while (k_sem_take((struct k_sem *)sem, K_NO_WAIT) != 0) {
        break;
    }
    k_free((struct k_sem *)sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    if (timeout == USB_OSAL_WAITING_FOREVER) {
        return (k_sem_take((struct k_sem *)sem, K_FOREVER) == 0) ? 0 : -USB_ERR_TIMEOUT;
    } else {
        return (k_sem_take((struct k_sem *)sem, K_MSEC(timeout)) == 0) ? 0 : -USB_ERR_TIMEOUT;
    }
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    k_sem_give((struct k_sem *)sem);

    return 0;
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
    k_sem_reset((struct k_sem *)sem);
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    struct k_mutex *mutex;

    mutex = k_malloc(sizeof(struct k_mutex));
    k_mutex_init(mutex);

    return (usb_osal_mutex_t)mutex;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    k_free((struct k_mutex *)mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    return (k_mutex_lock((struct k_mutex *)mutex, K_FOREVER) == 0) ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (k_mutex_unlock((struct k_mutex *)mutex) == 0) ? 0 : -USB_ERR_INVAL;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    struct k_msgq *msgq;
    char *buffer;

    msgq = k_malloc(sizeof(struct k_msgq));
    buffer = k_calloc(max_msgs, sizeof(uintptr_t));

    k_msgq_init(msgq, buffer, sizeof(uintptr_t), max_msgs);

    return (usb_osal_mq_t)msgq;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    struct k_msgq *msgq;
    uintptr_t addr;

    msgq = (struct k_msgq *)mq;

    while (k_msgq_get(msgq, &addr, K_NO_WAIT) != 0) {
        break;
    }
    k_free(msgq->buffer_start);
    k_free(msgq);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    struct k_msgq *msgq;

    msgq = (struct k_msgq *)mq;

    if (k_is_in_isr()) {
        return (k_msgq_put(msgq, &addr, K_NO_WAIT) == 0) ? 0 : -USB_ERR_TIMEOUT;
    } else {
        return (k_msgq_put(msgq, &addr, K_FOREVER) == 0) ? 0 : -USB_ERR_TIMEOUT;
    }
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    struct k_msgq *msgq;

    msgq = (struct k_msgq *)mq;

    if (k_is_in_isr()) {
        return (k_msgq_get(msgq, addr, K_NO_WAIT) == 0) ? 0 : -USB_ERR_TIMEOUT;
    } else {
        return (k_msgq_get(msgq, addr, K_FOREVER) == 0) ? 0 : -USB_ERR_TIMEOUT;
    }
}

static void zephyr_timer_wrapper(struct k_timer *ktimer)
{
    struct usb_osal_timer *timer = (struct usb_osal_timer *)ktimer->user_data;

    timer->handler(timer->argument);
}

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period)
{
    struct usb_osal_timer *timer;

    (void)name;

    timer = k_malloc(sizeof(struct usb_osal_timer));
    timer->timer = k_malloc(sizeof(struct k_timer));

    timer->handler = handler;
    timer->argument = argument;
    timer->is_period = is_period;
    timer->timeout_ms = timeout_ms;

    k_timer_init(timer->timer, zephyr_timer_wrapper, NULL);
    k_timer_user_data_set(timer->timer, timer);

    return timer;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    k_timer_stop(timer->timer);
    k_free(timer->timer);
    k_free(timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    if (timer->is_period) {
        k_timer_start(timer->timer, K_MSEC(timer->timeout_ms), K_MSEC(timer->timeout_ms));
    } else {
        k_timer_start(timer->timer, K_MSEC(timer->timeout_ms), K_NO_WAIT);
    }
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    k_timer_stop(timer->timer);
}

size_t usb_osal_enter_critical_section(void)
{
    return arch_irq_lock();
}

void usb_osal_leave_critical_section(size_t flag)
{
    arch_irq_unlock(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    k_sleep(K_MSEC(delay));
}

void *usb_osal_malloc(size_t size)
{
    return k_malloc(size);
}

void usb_osal_free(void *ptr)
{
    k_free(ptr);
}