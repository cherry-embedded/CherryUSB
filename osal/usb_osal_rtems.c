/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef __rtems__

#include "usb_osal.h"
#include "usb_errno.h"
#include <rtems.h>

#define SYS_USB_MBOX_SIZE (sizeof(void *))

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    rtems_id id = 0;
    rtems_status_code res;

    res = rtems_task_create(
        rtems_build_name(name[0], name[1], name[2], name[3]),
        prio,
        stack_size,
        RTEMS_PREEMPT,
        0,
        &id);

    if (res != RTEMS_SUCCESSFUL) {
        return NULL;
    }

    res = rtems_task_start(id, (rtems_task_entry)entry, (rtems_task_argument)args);

    if (res != RTEMS_SUCCESSFUL) {
        rtems_task_delete(id);
        return NULL;
    }

    return (usb_osal_thread_t)id;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    rtems_task_delete(thread);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    rtems_id semaphore = 0;
    rtems_status_code ret = rtems_semaphore_create(
        rtems_build_name('U', 'S', 'B', 's'),
        initial_count,
        RTEMS_COUNTING_SEMAPHORE,
        0,
        &semaphore);

    return semaphore;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    rtems_semaphore_delete(sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    rtems_status_code status;
    status = rtems_semaphore_obtain(sem, RTEMS_WAIT, timeout);
    return status == RTEMS_SUCCESSFUL ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    rtems_status_code status = rtems_semaphore_release(sem);

    return (status == RTEMS_SUCCESSFUL) ? 0 : -USB_ERR_TIMEOUT;
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    rtems_id mutex;
    rtems_status_code ret = rtems_semaphore_create(
        rtems_build_name('U', 'S', 'B', 'm'),
        1,
        RTEMS_PRIORITY | RTEMS_BINARY_SEMAPHORE | RTEMS_INHERIT_PRIORITY | RTEMS_LOCAL,
        0,
        &mutex);

    return mutex;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    rtems_semaphore_delete(mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    return (rtems_semaphore_obtain(mutex, RTEMS_WAIT, RTEMS_NO_TIMEOUT) == RTEMS_SUCCESSFUL) ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (rtems_semaphore_release(mutex) == RTEMS_SUCCESSFUL) ? 0 : -USB_ERR_TIMEOUT;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    rtems_status_code ret;
    rtems_id mailbox = 0;
    ret = rtems_message_queue_create(
        rtems_build_name('U', 'S', 'B', 'q'),
        max_msgs,
        SYS_USB_MBOX_SIZE,
        RTEMS_DEFAULT_ATTRIBUTES,
        &mailbox);
    return mailbox;
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    rtems_status_code ret;
    ret = rtems_message_queue_send(mq, &addr, SYS_USB_MBOX_SIZE);
    return (ret == RTEMS_SUCCESSFUL) ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    size_t size;
    rtems_status_code sc;
    sc = rtems_message_queue_receive(
        mq,
        addr,
        &size,
        RTEMS_WAIT,
        timeout);
    return (sc == RTEMS_SUCCESSFUL) ? 0 : -USB_ERR_TIMEOUT;
}

uint32_t usb_osal_enter_critical_section(void)
{
    rtems_interrupt_level pval;

#if RTEMS_SMP
    rtems_recursive_mutex_lock(&sys_arch_lock);
#else
    rtems_interrupt_disable(pval);
#endif
    return pval;
}

void usb_osal_leave_critical_section(size_t flag)
{
#if RTEMS_SMP
    rtems_recursive_mutex_unlock(&sys_arch_lock);
#else
    rtems_interrupt_enable(flag);
#endif
}

void usb_osal_msleep(uint32_t delay)
{
    rtems_task_wake_after(RTEMS_MILLISECONDS_TO_TICKS(delay));
}

#endif