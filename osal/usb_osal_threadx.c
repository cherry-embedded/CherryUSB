/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include "usb_config.h"
#include "usb_log.h"
#include "tx_api.h"

/* create bytepool in tx_application_define
 *
 * tx_byte_pool_create(&usb_byte_pool, "usb byte pool", memory_area, 65536);
 */

extern TX_BYTE_POOL usb_byte_pool;

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    CHAR *pointer = TX_NULL;
    TX_THREAD *thread_ptr = TX_NULL;

    tx_byte_allocate(&usb_byte_pool, (VOID **)&thread_ptr, sizeof(TX_THREAD), TX_NO_WAIT);

    if (thread_ptr == TX_NULL) {
        USB_LOG_ERR("Create thread %s failed\r\n", name);
        while (1) {
        }
    }

    tx_byte_allocate(&usb_byte_pool, (VOID **)&pointer, stack_size, TX_NO_WAIT);
    if (pointer == TX_NULL) {
        USB_LOG_ERR("Create thread %s failed\r\n", name);
        while (1) {
        }
    }

    tx_thread_create(thread_ptr, (CHAR *)name, (VOID(*)(ULONG))entry, (uintptr_t)args,
                     pointer, stack_size,
                     prio, prio, TX_NO_TIME_SLICE, TX_AUTO_START);

    return (usb_osal_thread_t)thread_ptr;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    TX_THREAD *thread_ptr = NULL;

    if (thread == NULL) {
        /* Call the tx_thread_identify to get the control block pointer of the
        currently executing thread. */
        thread_ptr = tx_thread_identify();

        /* Check if the current running thread pointer is not NULL */
        if (thread_ptr != NULL) {
            /* Call the tx_thread_terminate to terminates the specified application
            thread regardless of whether the thread is suspended or not. A thread
            may call this service to terminate itself. */
            tx_thread_terminate(thread_ptr);
            tx_byte_release(thread_ptr->tx_thread_stack_start);
            tx_byte_release(thread_ptr);
        }
        return;
    }

    tx_thread_terminate(thread);
    tx_byte_release(thread_ptr->tx_thread_stack_start);
    tx_byte_release(thread);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    TX_SEMAPHORE *sem_ptr = TX_NULL;

    tx_byte_allocate(&usb_byte_pool, (VOID **)&sem_ptr, sizeof(TX_SEMAPHORE), TX_NO_WAIT);

    if (sem_ptr == TX_NULL) {
        USB_LOG_ERR("Create semaphore failed\r\n");
        while (1) {
        }
    }

    tx_semaphore_create(sem_ptr, "usbh_sem", initial_count);
    return (usb_osal_sem_t)sem_ptr;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    tx_semaphore_delete((TX_SEMAPHORE *)sem);
    tx_byte_release(sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    int ret = 0;

    ret = tx_semaphore_get((TX_SEMAPHORE *)sem, timeout);
    if (ret == TX_SUCCESS) {
        ret = 0;
    } else if ((ret == TX_WAIT_ABORTED) || (ret == TX_NO_INSTANCE)) {
        ret = -USB_ERR_TIMEOUT;
    } else {
        ret = -USB_ERR_INVAL;
    }

    return (int)ret;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    return (int)tx_semaphore_put((TX_SEMAPHORE *)sem);
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    TX_MUTEX *mutex_ptr = TX_NULL;

    tx_byte_allocate(&usb_byte_pool, (VOID **)&mutex_ptr, sizeof(TX_MUTEX), TX_NO_WAIT);

    if (mutex_ptr == TX_NULL) {
        USB_LOG_ERR("Create mutex failed\r\n");
        while (1) {
        }
    }

    tx_mutex_create(mutex_ptr, "usbh_mutx", TX_INHERIT);
    return (usb_osal_mutex_t)mutex_ptr;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    tx_mutex_delete((TX_MUTEX *)mutex);
    tx_byte_release(mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    int ret = 0;

    ret = tx_mutex_get((TX_MUTEX *)mutex, TX_WAIT_FOREVER);
    if (ret == TX_SUCCESS) {
        ret = 0;
    } else if ((ret == TX_WAIT_ABORTED) || (ret == TX_NO_INSTANCE)) {
        ret = -USB_ERR_TIMEOUT;
    } else {
        ret = -USB_ERR_INVAL;
    }

    return (int)ret;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (int)(tx_mutex_put((TX_MUTEX *)mutex) == TX_SUCCESS) ? 0 : -USB_ERR_INVAL;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    CHAR *pointer = TX_NULL;
    TX_QUEUE *queue_ptr = TX_NULL;

    tx_byte_allocate(&usb_byte_pool, (VOID **)&queue_ptr, sizeof(TX_QUEUE), TX_NO_WAIT);

    if (queue_ptr == TX_NULL) {
        USB_LOG_ERR("Create TX_QUEUE failed\r\n");
        while (1) {
        }
    }

    tx_byte_allocate(&usb_byte_pool, (VOID **)&pointer, sizeof(uintptr_t) * max_msgs, TX_NO_WAIT);

    if (pointer == TX_NULL) {
        USB_LOG_ERR("Create mq failed\r\n");
        while (1) {
        }
    }

    tx_queue_create(queue_ptr, "usbh_mq", sizeof(uintptr_t) / 4, pointer, sizeof(uintptr_t) * max_msgs);
    return (usb_osal_mq_t)queue_ptr;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    tx_queue_delete((TX_QUEUE *)mq);
    tx_byte_release(((TX_QUEUE *)mq)->tx_queue_start);
    tx_byte_release(mq);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    return (tx_queue_send((TX_QUEUE *)mq, &addr, TX_NO_WAIT) == TX_SUCCESS) ? 0 : -USB_ERR_INVAL;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    int ret = 0;

    ret = tx_queue_receive((TX_QUEUE *)mq, addr, timeout);
    if (ret == TX_SUCCESS) {
        ret = 0;
    } else if (ret == TX_QUEUE_EMPTY) {
        ret = -USB_ERR_TIMEOUT;
    } else {
        ret = -USB_ERR_INVAL;
    }

    return (int)ret;
}

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period)
{
    TX_TIMER *timer_ptr = TX_NULL;
    struct usb_osal_timer *timer;

    tx_byte_allocate(&usb_byte_pool, (VOID **)&timer, sizeof(struct usb_osal_timer), TX_NO_WAIT);

    if (timer == TX_NULL) {
        USB_LOG_ERR("Create usb_osal_timer failed\r\n");
        while (1) {
        }
    }
    memset(timer, 0, sizeof(struct usb_osal_timer));

    tx_byte_allocate(&usb_byte_pool, (VOID **)&timer_ptr, sizeof(TX_TIMER), TX_NO_WAIT);

    if (timer_ptr == TX_NULL) {
        USB_LOG_ERR("Create TX_TIMER failed\r\n");
        while (1) {
        }
    }

    timer->timer = timer_ptr;
    timer->ticks = timeout_ms;
    timer->is_period = is_period;
    if (tx_timer_create(timer_ptr, (CHAR *)name, (void (*)(ULONG))handler, (uintptr_t)argument, 1, is_period ? 1 : 0,
                        TX_NO_ACTIVATE) != TX_SUCCESS) {
        return NULL;
    }
    return timer;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    tx_timer_deactivate((TX_TIMER *)timer->timer);
    tx_timer_delete((TX_TIMER *)timer->timer);
    tx_byte_release(timer->timer);
    tx_byte_release(timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    if (tx_timer_change((TX_TIMER *)timer->timer, timer->ticks, timer->is_period ? timer->ticks : 0) == TX_SUCCESS) {
        /* Call the tx_timer_activate to activates the specified application
             timer. The expiration routines of timers that expire at the same
             time are executed in the order they were activated. */
        if (tx_timer_activate((TX_TIMER *)timer->timer) == TX_SUCCESS) {
            /* Return osOK for success */
        } else {
            /* Return osErrorResource in case of error */
        }
    } else {
    }
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    tx_timer_deactivate((TX_TIMER *)timer->timer);
}

size_t usb_osal_enter_critical_section(void)
{
    TX_INTERRUPT_SAVE_AREA

    TX_DISABLE

    return interrupt_save;
}

void usb_osal_leave_critical_section(size_t flag)
{
    int interrupt_save;

    interrupt_save = flag;
    TX_RESTORE
}

void usb_osal_msleep(uint32_t delay)
{
#if TX_TIMER_TICKS_PER_SECOND != 1000
#error "TX_TIMER_TICKS_PER_SECOND must be 1000"
#endif
    tx_thread_sleep(delay);
}

void *usb_osal_malloc(size_t size)
{
    CHAR *pointer = TX_NULL;

    tx_byte_allocate(&usb_byte_pool, (VOID **)&pointer, size, TX_WAIT_FOREVER);

    return pointer;
}

void usb_osal_free(void *ptr)
{
    tx_byte_release(ptr);
}