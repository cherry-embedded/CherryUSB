/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "chry_mempool.h"
#include <rtthread.h>
#include <rthw.h>

chry_mempool_osal_sem_t chry_mempool_osal_sem_create(uint32_t max_count)
{
    return (chry_mempool_osal_sem_t)rt_sem_create("chry_mempoolh_sem", max_count, RT_IPC_FLAG_FIFO);
}

void chry_mempool_osal_sem_delete(chry_mempool_osal_sem_t sem)
{
    rt_sem_delete((rt_sem_t)sem);
}

int chry_mempool_osal_sem_take(chry_mempool_osal_sem_t sem, uint32_t timeout)
{
    int ret = 0;
    rt_err_t result = RT_EOK;

    if (timeout == 0xfffffff) {
        result = rt_sem_take((rt_sem_t)sem, RT_WAITING_FOREVER);
    } else {
        result = rt_sem_take((rt_sem_t)sem, rt_tick_from_millisecond(timeout));
    }
    if (result == -RT_ETIMEOUT) {
        ret = -1;
    } else if (result == -RT_ERROR) {
        ret = -1;
    } else {
        ret = 0;
    }

    return (int)ret;
}

int chry_mempool_osal_sem_give(chry_mempool_osal_sem_t sem)
{
    return (int)rt_sem_release((rt_sem_t)sem);
}

void *chry_mempool_osal_malloc(size_t size)
{
    return rt_malloc(size);
}

void chry_mempool_osal_free(void *ptr)
{
    rt_free(ptr);
}