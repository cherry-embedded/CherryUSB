/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_osal.h"
#include "usb_errno.h"

#include <nuttx/config.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <debug.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <nuttx/kmalloc.h>
#include <nuttx/mqueue.h>
#include <nuttx/spinlock.h>
#include <nuttx/irq.h>
#include <nuttx/kthread.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>
#include <nuttx/semaphore.h>
#include <nuttx/sched.h>
#include <nuttx/signal.h>

#if 1
#error please modfiy all thread param (void *argument) with (int argc, char **argv), and argument = ((uintptr_t)strtoul(argv[1], NULL, 16));
#endif

struct mq_adpt {
    struct file mq;   /* Message queue handle */
    uint32_t msgsize; /* Message size */
    char name[16];    /* Message queue name */
};

struct timer_adpt {
    struct usb_osal_timer timer;
    struct wdog_s wdog;
};

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    int pid;
    char *argv[2];
    char arg1[32];

    snprintf(arg1, 16, "%p", args);
    argv[0] = arg1;
    argv[1] = NULL;

    pid = kthread_create(name, prio, stack_size, (void *)entry,
                         argv);
    if (pid > 0) {
        return (usb_osal_thread_t)pid;
    } else {
        return NULL;
    }
}

void usb_osal_thread_delete(usb_osal_thread_t thread)
{
    pid_t pid = (pid_t)((uintptr_t)thread);

    kthread_delete(pid);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    int ret;
    sem_t *sem;
    int tmp;

    tmp = sizeof(sem_t);
    sem = kmm_malloc(tmp);
    if (!sem) {
        //printf("ERROR: Failed to alloc %d memory\n", tmp);
        return NULL;
    }

    ret = nxsem_init(sem, 0, initial_count);
    if (ret) {
        //printf("ERROR: Failed to initialize sem error=%d\n", ret);
        kmm_free(sem);
        return NULL;
    }
    return (usb_osal_sem_t)sem;
}

void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    sem_t *__sem = (sem_t *)sem;

    nxsem_destroy(__sem);
    kmm_free(__sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    int ret;
    sem_t *__sem = (sem_t *)sem;

    if (timeout == 0xffffffff) {
        ret = nxsem_wait(__sem);
    } else {
        ret = nxsem_tickwait(__sem, MSEC2TICK(timeout));
    }

    if (ret) {
        return -USB_ERR_TIMEOUT;
    } else {
        return 0;
    }
}

int usb_osal_sem_give(usb_osal_sem_t sem)
{
    int ret;
    sem_t *__sem = (sem_t *)sem;

    ret = nxsem_post(__sem);
    if (ret) {
        return -USB_ERR_INVAL;
    } else {
        return 0;
    }
}

void usb_osal_sem_reset(usb_osal_sem_t sem)
{
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    int ret;
    mutex_t *mutex;
    int tmp;

    tmp = sizeof(mutex_t);
    mutex = kmm_malloc(tmp);
    if (!mutex) {
        //printf("ERROR: Failed to alloc %d memory\n", tmp);
        return NULL;
    }

    ret = nxmutex_init(mutex);
    if (ret) {
        //printf("ERROR: Failed to initialize mutex error=%d\n", ret);
        kmm_free(mutex);
        return NULL;
    }

    return (usb_osal_mutex_t)mutex;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    mutex_t *__mutex = (mutex_t *)mutex;

    nxmutex_destroy(__mutex);
    kmm_free(__mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    int ret;
    mutex_t *__mutex = (mutex_t *)mutex;

    ret = nxmutex_lock(__mutex);
    if (ret) {
        return -USB_ERR_INVAL;
    } else {
        return 0;
    }
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    int ret;
    mutex_t *__mutex = (mutex_t *)mutex;

    ret = nxmutex_unlock(__mutex);
    if (ret) {
        return -USB_ERR_INVAL;
    } else {
        return 0;
    }
}

usb_osal_mq_t usb_osal_mq_create(uint32_t max_msgs)
{
    struct mq_attr attr;
    struct mq_adpt *mq_adpt;
    int ret;

    mq_adpt = (struct mq_adpt *)kmm_malloc(sizeof(struct mq_adpt));

    if (!mq_adpt) {
        //printf("ERROR: Failed to kmm_malloc\n");
        return NULL;
    }

    snprintf(mq_adpt->name, sizeof(mq_adpt->name),
             "/tmp/%p", mq_adpt);

    attr.mq_maxmsg = max_msgs;
    attr.mq_msgsize = sizeof(uintptr_t);
    attr.mq_curmsgs = 0;
    attr.mq_flags = 0;

    ret = file_mq_open(&mq_adpt->mq, mq_adpt->name,
                       O_RDWR | O_CREAT, 0644, &attr);

    if (ret < 0) {
        //printf("ERROR: Failed to create mqueue\n");
        kmm_free(mq_adpt);
        return NULL;
    }

    mq_adpt->msgsize = sizeof(uintptr_t);
    return (usb_osal_mq_t)mq_adpt;
}

void usb_osal_mq_delete(usb_osal_mq_t mq)
{
    struct mq_adpt *mq_adpt = (struct mq_adpt *)mq;

    file_mq_close(&mq_adpt->mq);
    file_mq_unlink(mq_adpt->name);
    kmm_free(mq_adpt);
}

int usb_osal_mq_send(usb_osal_mq_t mq, uintptr_t addr)
{
    struct mq_adpt *mq_adpt = (struct mq_adpt *)mq;
    int ret;

    /* send mq from isr, do not use timeout*/
    ret = file_mq_send(&mq_adpt->mq, (const char *)&addr, mq_adpt->msgsize, 0);
    if (ret < 0) {
        return -USB_ERR_INVAL;
    } else {
        return 0;
    }
}

static void msec2spec(struct timespec *timespec, uint32_t ticks)
{
    uint32_t tmp;

    tmp = TICK2SEC(ticks);
    timespec->tv_sec += tmp;

    ticks -= SEC2TICK(tmp);
    tmp = TICK2NSEC(ticks);

    timespec->tv_nsec += tmp;
}

int usb_osal_mq_recv(usb_osal_mq_t mq, uintptr_t *addr, uint32_t timeout)
{
    struct mq_adpt *mq_adpt = (struct mq_adpt *)mq;
    struct timespec __timeout;
    int ret;

    if (timeout == 0xffffffff)
        return file_mq_receive(&mq_adpt->mq, (char *)addr, mq_adpt->msgsize, 0);
    else {
        ret = clock_gettime(CLOCK_REALTIME, &__timeout);

        if (ret < 0) {
            //printf("ERROR: Failed to get time\n");
            return -USB_ERR_INVAL;
        }

        if (timeout) {
            msec2spec(&__timeout, timeout);
        }

        return file_mq_timedreceive(&mq_adpt->mq,
                                    (char *)addr,
                                    mq_adpt->msgsize,
                                    0,
                                    &__timeout);
    }
}

static void os_timer_callback(wdparm_t arg)
{
    struct timer_adpt *timer;

    timer = (struct timer_adpt *)arg;

    if (timer->timer.handler) {
        timer->timer.handler(timer->timer.argument);
    }

    if (timer->timer.is_period) {
        wd_start(&timer->wdog, timer->timer.ticks, os_timer_callback, arg);
    }
}

struct usb_osal_timer *usb_osal_timer_create(const char *name, uint32_t timeout_ms, usb_timer_handler_t handler, void *argument, bool is_period)
{
    struct timer_adpt *timer = kmm_malloc(sizeof(struct timer_adpt));

    (void)name;

    if (!timer) {
        return NULL;
    }

    memset((void *)timer, 0, sizeof(struct timer_adpt));

    timer->timer.handler = handler;
    timer->timer.argument = argument;
    timer->timer.ticks = MSEC2TICK(timeout_ms);
    timer->timer.is_period = is_period;

    return (struct usb_osal_timer *)timer;
}

void usb_osal_timer_delete(struct usb_osal_timer *timer)
{
    struct timer_adpt *__timer = (struct timer_adpt *)timer;

    wd_cancel(&__timer->wdog);

    kmm_free(__timer);
}

void usb_osal_timer_start(struct usb_osal_timer *timer)
{
    struct timer_adpt *__timer = (struct timer_adpt *)timer;

    wd_start(&__timer->wdog, __timer->timer.ticks, os_timer_callback, (wdparm_t)__timer);
}

void usb_osal_timer_stop(struct usb_osal_timer *timer)
{
    struct timer_adpt *__timer = (struct timer_adpt *)timer;

    wd_cancel(&__timer->wdog);
}

size_t usb_osal_enter_critical_section(void)
{
    irqstate_t flags;

    flags = enter_critical_section();

    return (size_t)flags;
}

void usb_osal_leave_critical_section(size_t flag)
{
    leave_critical_section(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    useconds_t usec = delay * 1000;

    nxsig_usleep(usec);
}

void *usb_osal_malloc(size_t size)
{
    return kmm_malloc(size);
}

void usb_osal_free(void *ptr)
{
    kmm_free(ptr);
}