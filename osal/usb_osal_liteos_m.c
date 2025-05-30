/*
 * Copyright (c) 2025, HPMicro
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"
#include "usb_config.h"
#include "usb_log.h"

#include "los_interrupt.h"
#include "los_task.h"
#include "los_queue.h"
#include "los_sem.h"
#include "los_mux.h"
#include "los_memory.h"
#include "los_swtmr.h"

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    UINT32 task_id;
    UINT32 ret;

    TSK_INIT_PARAM_S task_init_param = {
        .usTaskPrio = prio,
        .pfnTaskEntry = (TSK_ENTRY_FUNC)entry,
        .uwStackSize = stack_size,
        .uwArg = (UINT32)args,
        .pcName = name
    };

    ret = LOS_TaskCreate(&task_id, &task_init_param);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Create task[%s] failed code[%u]\r\n", name, ret);
        while (1) {
        }
    }

    return (usb_osal_thread_t)task_id;
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    UINT32 task_id = (UINT32)thread;
    UINT32 ret;

    if (thread == NULL) {
        task_id = LOS_CurTaskIDGet();
    }

    ret = LOS_TaskDelete(task_id);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Delete task id[%u] failed code[%u]\r\n", task_id, ret);
        while (1) {
        }
    }
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    UINT32 sem_handle;
    UINT32 ret;

    ret = LOS_SemCreate(initial_count, &sem_handle);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Create Sem failed code[%u]\r\n", ret);
        while (1) {
        }
    }
    return (usb_osal_sem_t)sem_handle;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    UINT32 sem_handle = (UINT32)sem;
    UINT32 ret;

    ret = LOS_SemDelete(sem_handle);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Delete sem handle[%u] failed code[%u]\r\n",
                    sem_handle, ret);
        while (1) {
        }
    }
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    UINT32 sem_handle = (UINT32)sem;
    UINT32 ret;

    ret = LOS_SemPend(sem_handle,
                      timeout == USB_OSAL_WAITING_FOREVER ?
                          LOS_WAIT_FOREVER :
                          timeout);

    return ret == LOS_OK ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    UINT32 sem_handle = (UINT32)sem;
    UINT32 ret;

    ret = LOS_SemPost(sem_handle);

    return ret == LOS_OK ? 0 : -USB_ERR_TIMEOUT;
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    UINT32 mux_handle;
    UINT32 ret;

    ret = LOS_MuxCreate(&mux_handle);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Create mux failed code[%u]\r\n", ret);
        while (1) {
        }
    }
    return (usb_osal_mutex_t)mux_handle;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    UINT32 mux_handle = (UINT32)mutex;
    UINT32 ret;

    ret = LOS_MuxDelete(mux_handle);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Delete mux handle[%u] failed code[%u]\r\n",
                    mux_handle, ret);
        while (1) {
        }
    }
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    UINT32 mux_handle = (UINT32)mutex;
    UINT32 ret;

    ret = LOS_MuxPend(mux_handle, LOS_WAIT_FOREVER);

    return ret == LOS_OK ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    UINT32 mux_handle = (UINT32)mutex;
    UINT32 ret;

    ret = LOS_MuxPost(mux_handle);

    return ret == LOS_OK ? 0 : -USB_ERR_TIMEOUT;
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    UINT32 queue_id;
    UINT32 ret;

    ret = LOS_QueueCreate(NULL, max_msgs, &queue_id, 0, sizeof(uintptr_t));
    if (ret != LOS_OK) {
        USB_LOG_ERR("Create queue failed code[%u]\r\n", ret);
        while (1) {
        }
    }
    return (usb_osal_mq_t)queue_id;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    UINT32 queue_id = (UINT32)mq;
    UINT32 ret;

    ret = LOS_QueueDelete(queue_id);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Delete queue id[%u] failed code[%u]\r\n",
                    queue_id, ret);
        while (1) {
        }
    }
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    UINT32 queue_id = (UINT32)mq;
    UINT32 ret;
    UINT32 timeout;

    if (OS_INT_ACTIVE)
        timeout = LOS_NO_WAIT;
    else
        timeout = LOS_WAIT_FOREVER;

    ret = LOS_QueueWrite(queue_id, addr, sizeof(uintptr_t), timeout);

    return ret == LOS_OK ? 0 : -USB_ERR_TIMEOUT;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    UINT32 queue_id = (UINT32)mq;
    UINT32 ret;
    UINT32 _timeout;

    if (OS_INT_ACTIVE)
        _timeout = LOS_NO_WAIT;
    else
        _timeout = timeout == USB_OSAL_WAITING_FOREVER ? LOS_WAIT_FOREVER : timeout;

    ret = LOS_QueueRead(queue_id, addr, sizeof(uintptr_t), _timeout);
    return ret == LOS_OK ? 0 : -USB_ERR_TIMEOUT;
}

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period)
{
    UINT32 timer_id;
    UINT32 ret;
    struct usb_osal_timer *timer_handle;

    timer_handle = usb_osal_malloc(sizeof(struct usb_osal_timer));
    if (timer_handle == NULL) {
        USB_LOG_ERR("Malloc usb_osal_timer failed\r\n");
        while (1) {
        }
    }
    memset(timer_handle, 0x00, sizeof(struct usb_osal_timer));

    ret = LOS_SwtmrCreate(timeout_ms,
                          is_period ? LOS_SWTMR_MODE_PERIOD : LOS_SWTMR_MODE_NO_SELFDELETE,
                          (SWTMR_PROC_FUNC)handler, &timer_id, (UINT32)argument
#if (LOSCFG_BASE_CORE_SWTMR_ALIGN == 1)
                          ,
                          OS_SWTMR_ROUSES_IGNORE, OS_SWTMR_ALIGN_INSENSITIVE
#endif
    );

    if (ret != LOS_OK) {
        USB_LOG_ERR("Create software timer failed code[%u]\r\n", ret);
        while (1) {
        }
    }

    timer_handle->handler = handler;
    timer_handle->argument = argument;
    timer_handle->is_period = is_period;
    timer_handle->timeout_ms = timeout_ms;
    timer_handle->timer = (void *)timer_id;

    return timer_handle;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    UINT32 timer_id = (UINT32)timer->timer;
    UINT32 ret;

    ret = LOS_SwtmrDelete(timer_id);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Delete software timer id[%u] failed code[%u]\r\n",
                    timer_id, ret);
        while (1) {
        }
    }
    usb_osal_free(timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    UINT32 timer_id = (UINT32)timer->timer;
    UINT32 ret;

    ret = LOS_SwtmrStart(timer_id);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Start software timer id[%u] failed code[%u]\r\n",
                    timer_id, ret);
        while (1) {
        }
    }
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    UINT32 timer_id = (UINT32)timer->timer;
    UINT32 ret;

    ret = LOS_SwtmrStop(timer_id);
    if (ret != LOS_OK) {
        USB_LOG_ERR("Stop software timer id[%u] failed code[%u]\r\n",
                    timer_id, ret);
        while (1) {
        }
    }
}

size_t usb_osal_enter_critical_section(void)
{
    return LOS_IntLock();
}

void usb_osal_leave_critical_section(size_t flag)
{
    LOS_IntRestore(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    LOS_Msleep(delay);
}

void *usb_osal_malloc(size_t size)
{
    return LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, size);
}

void usb_osal_free(void *ptr)
{
    if (ptr != NULL) {
        LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, ptr);
    }
}