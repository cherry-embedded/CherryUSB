/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_config.h"
#include "usb_util.h"
#include "usb_osal.h"
#include "usb_errno.h"
#include "usb_log.h"
/*
 *********************************************************************************************************
 *                                         STANDARD LIBRARIES
 *********************************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

/*
 *********************************************************************************************************
 *                                                 OS
 *********************************************************************************************************
 */

#include <os.h>

/*
 *********************************************************************************************************
 *                                              LIBRARIES
 *********************************************************************************************************
 */

#include <cpu.h>
#include <lib_def.h>
#include <lib_ascii.h>
#include <lib_math.h>
#include <lib_mem.h>
#include <lib_str.h>

/*
 *********************************************************************************************************
 *                                              APP / BSP
 *********************************************************************************************************
 */

#include <app_cfg.h>

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    OS_ERR err;
    OS_TCB *tcb;

    tcb = (OS_TCB *)usb_osal_malloc(USB_ALIGN_UP(sizeof(OS_TCB), 4) + stack_size);
    if (tcb == NULL) {
        USB_LOG_ERR("Create thread %s failed\r\n", name);
        while (1) {
        }
    }

    memset(tcb, 0, USB_ALIGN_UP(sizeof(OS_TCB), 4) + stack_size);
    OSTaskCreate((OS_TCB *)tcb,
                 (CPU_CHAR *)name,
                 (OS_TASK_PTR)entry,
                 (void *)args,
                 (OS_PRIO)prio,
                 (CPU_STK *)((uint8_t *)tcb + USB_ALIGN_UP(sizeof(OS_TCB), 4)),
                 (CPU_STK_SIZE)(stack_size / 4) / 10,
                 (CPU_STK_SIZE)(stack_size / 4),
                 (OS_MSG_QTY)0,
                 (OS_TICK)0,
                 (void *)0,
                 (OS_OPT)OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
                 (OS_ERR *)&err);

    return (usb_osal_thread_t)tcb;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    OS_ERR err;
    OS_TCB *tcb;

    if (thread == NULL) {
        tcb = OSTCBCurPtr;
    } else {
        tcb = (OS_TCB *)thread;
    }

    OSTaskDel(tcb, &err);
    usb_osal_free(tcb);
}

void usb_osal_thread_schedule_other(void)
{
    OS_ERR err;

    OS_TCB *current_tcb = OSTCBCurPtr;

    const OS_PRIO old_priority = OSPrioCur;

    OSTaskChangePrio(current_tcb, OS_CFG_PRIO_MAX - 1, &err);

    OSSchedRoundRobinYield(&err);

    OSTaskChangePrio(current_tcb, old_priority, &err);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    OS_ERR err;
    OS_SEM *sem;

    sem = (OS_SEM *)usb_osal_malloc(sizeof(OS_SEM));
    if (sem == NULL) {
        USB_LOG_ERR("Create semaphore failed\r\n");
        while (1) {
        }
    }

    memset(sem, 0, sizeof(OS_SEM));
    OSSemCreate(sem, "usbh_sem", initial_count, &err);

    return sem;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    OS_ERR err;

    OSSemDel((OS_SEM *)sem, OS_OPT_DEL_ALWAYS, &err);
    usb_osal_free(sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    OS_ERR err;
    CPU_TS ts;

    if (timeout == USB_OSAL_WAITING_FOREVER) {
        OSSemPend((OS_SEM *)sem, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
    } else {
        if (timeout == 0) {
            OSSemPend((OS_SEM *)sem, 0, OS_OPT_PEND_NON_BLOCKING, &ts, &err);
        } else {
            OSSemPend((OS_SEM *)sem, timeout, OS_OPT_PEND_BLOCKING, &ts, &err);
        }
    }

    if (err == OS_ERR_TIMEOUT) {
        return -USB_ERR_TIMEOUT;
    }

    if (err != OS_ERR_NONE) {
        return -USB_ERR_INVAL;
    }

    return 0;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    OS_ERR err;

    return (OSSemPost((OS_SEM *)sem, OS_OPT_POST_1, &err) == OS_ERR_NONE) ? 0 : -USB_ERR_TIMEOUT;
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
    OS_ERR err;

    OSSemSet((OS_SEM *)sem, 0, &err);
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    OS_ERR err;
    OS_MUTEX *mutex;

    mutex = (OS_MUTEX *)usb_osal_malloc(sizeof(OS_MUTEX));
    if (mutex == NULL) {
        USB_LOG_ERR("Create mutex failed\r\n");
        while (1) {
        }
    }

    memset(mutex, 0, sizeof(OS_MUTEX));
    OSMutexCreate(mutex, "usbh_mutex", &err);

    return mutex;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    OS_ERR err;

    OSMutexDel((OS_MUTEX *)mutex, OS_OPT_DEL_ALWAYS, &err);
    usb_osal_free(mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    OS_ERR err;

    CPU_TS ts;

    OSMutexPend((OS_MUTEX *)mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);

    if (err == OS_ERR_TIMEOUT) {
        return -USB_ERR_TIMEOUT;
    }

    if (err != OS_ERR_NONE) {
        return -USB_ERR_INVAL;
    }

    return 0;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    OS_ERR err;

    OSMutexPost((OS_MUTEX *)mutex, OS_OPT_POST_NONE, &err);

    return (err == OS_ERR_NONE) ? 0 : -USB_ERR_TIMEOUT;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    OS_ERR err;
    OS_Q *mq;

    mq = (OS_Q *)usb_osal_malloc(sizeof(OS_Q));
    if (mq == NULL) {
        USB_LOG_ERR("Create message queue failed\r\n");
        while (1) {
        }
    }

    memset(mq, 0, sizeof(OS_Q));
    OSQCreate((OS_Q *)mq, "usbh_mq", (OS_MSG_QTY)max_msgs, &err);

    return mq;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    OS_ERR err;

    OSQDel((OS_Q *)mq, OS_OPT_DEL_ALWAYS, &err);
    usb_osal_free(mq);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    OS_ERR err;

    OSQPost((OS_Q *)mq, &addr, sizeof(uintptr_t), OS_OPT_POST_FIFO, &err);

    if (err == OS_ERR_TIMEOUT) {
        return -USB_ERR_TIMEOUT;
    }

    if (err != OS_ERR_NONE) {
        return -USB_ERR_INVAL;
    }

    return 0;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    OS_ERR err;
    void *msg = NULL;
    OS_MSG_SIZE msg_size;

    if (timeout == USB_OSAL_WAITING_FOREVER) {
        msg = OSQPend((OS_Q *)mq, 0, OS_OPT_PEND_BLOCKING, &msg_size, NULL, &err);
    } else {
        if (timeout == 0) {
            msg = OSQPend((OS_Q *)mq, 0, OS_OPT_PEND_NON_BLOCKING, &msg_size, NULL, &err);
        } else {
            msg = OSQPend((OS_Q *)mq, timeout, OS_OPT_PEND_BLOCKING, &msg_size, NULL, &err);
        }
    }

    if (err == OS_ERR_TIMEOUT) {
        return -USB_ERR_TIMEOUT;
    }

    if (err != OS_ERR_NONE) {
        return -USB_ERR_INVAL;
    }

    *addr = *(uintptr_t *)msg;

    return 0;
}

static void __usb_timeout(void *p_tmr, void *p_arg)
{
    struct usb_osal_timer *timer = (struct usb_osal_timer *)p_arg;

    timer->handler(timer->argument);
}

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period)
{
    struct usb_osal_timer *timer;
    OS_ERR err;
    OS_TMR *tmr;
    (void)name;

    timer = (struct usb_osal_timer *)usb_osal_malloc(sizeof(struct usb_osal_timer));
    if (timer == NULL) {
        USB_LOG_ERR("Create timer %s failed\r\n", name);
        while (1) {
        }
    }

    memset(timer, 0, sizeof(struct usb_osal_timer));
    tmr = (OS_TMR *)usb_osal_malloc(sizeof(OS_TMR));
    if (tmr == NULL) {
        USB_LOG_ERR("Create timer %s failed\r\n", name);
        while (1) {
        }
    }

    memset(tmr, 0, sizeof(OS_TMR));

    timer->timeout_ms = timeout_ms;
    timer->handler = handler;
    timer->argument = argument;
    timer->is_period = is_period;
    timer->timer = tmr;

    OSTmrCreate((OS_TMR *)tmr,
                (CPU_CHAR *)name,
                (OS_TICK)is_period ? 0 : timeout_ms,
                (OS_TICK)is_period ? timeout_ms : 0,
                (OS_OPT)is_period ? OS_OPT_TMR_PERIODIC : OS_OPT_TMR_ONE_SHOT,
                (OS_TMR_CALLBACK_PTR)__usb_timeout,
                (void *)timer,
                (OS_ERR *)&err);

    return timer;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    OS_ERR err;

    OSTmrStop((OS_TMR *)timer->timer, OS_OPT_TMR_NONE, NULL, &err);
    OSTmrDel((OS_TMR *)timer->timer, &err);
    usb_osal_free(timer->timer);
    usb_osal_free(timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    OS_ERR err;

    OSTmrStart((OS_TMR *)timer->timer, &err);
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    OS_ERR err;

    OSTmrStop((OS_TMR *)timer->timer, OS_OPT_TMR_NONE, NULL, &err);
}

size_t usb_osal_enter_critical_section(void)
{
    return CPU_SR_Save();
}

void usb_osal_leave_critical_section(size_t flag)
{
    CPU_SR_Restore(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    OS_ERR err;

    OSTimeDlyHMSM(0, 0, 0, delay, OS_OPT_TIME_HMSM_STRICT, &err);
}

void *usb_osal_malloc(size_t size)
{
    void *buf = malloc(size);
    if (!buf) {
        USB_LOG_ERR("usb_osal_malloc failed\r\n");
        while (1) {
        }
    }
    return buf;
}

void usb_osal_free(void *ptr)
{
    free(ptr);
}