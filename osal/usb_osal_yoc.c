/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include <aos/kernel.h>
#include <csi_core.h>

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    aos_task_t task_handle = NULL;

    aos_task_new_ext(&task_handle, name, entry, args, stack_size, prio + AOS_DEFAULT_APP_PRI - 4);

    return task_handle;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    aos_task_exit(0);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    aos_sem_t sem = NULL;

    aos_sem_new(&sem, initial_count);

    return sem;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    aos_sem_free((aos_sem_t *)&sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    if (timeout == USB_OSAL_WAITING_FOREVER) {
        return aos_sem_wait((aos_sem_t *)&sem, AOS_WAIT_FOREVER);
    } else {
        return aos_sem_wait((aos_sem_t *)&sem, timeout);
    }
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    aos_sem_signal((aos_sem_t *)&sem);
    return 0;
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
    
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    aos_mutex_t mutex = NULL;

    aos_mutex_new(&mutex);

    return (usb_osal_mutex_t)mutex;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    aos_mutex_free(((aos_mutex_t *)&mutex));
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    return aos_mutex_lock(((aos_mutex_t *)&mutex), AOS_WAIT_FOREVER);
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return aos_mutex_unlock(((aos_mutex_t *)&mutex));
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    aos_queue_t queue = NULL;

    aos_queue_create(&queue, sizeof(uintptr_t), max_msgs, 0);

    return (usb_osal_mq_t)queue;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    aos_queue_free((aos_queue_t)mq);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    return aos_queue_send((aos_queue_t *)&mq, &addr, sizeof(uintptr_t));
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    size_t recv_size;
    if (timeout == USB_OSAL_WAITING_FOREVER) {
        return aos_queue_recv((aos_queue_t *)&mq, AOS_WAIT_FOREVER, addr, &recv_size);
    } else {
        return aos_queue_recv((aos_queue_t *)&mq, timeout, addr, &recv_size);
    }
}

size_t usb_osal_enter_critical_section(void)
{
    return csi_irq_save();
}

void usb_osal_leave_critical_section(size_t flag)
{
    csi_irq_restore(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    aos_msleep(delay);
}
