/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "chry_mempool.h"
#include "stdlib.h"

chry_mempool_osal_sem_t chry_mempool_osal_sem_create(uint32_t max_count)
{
    return (chry_mempool_osal_sem_t)1;
}

void chry_mempool_osal_sem_delete(chry_mempool_osal_sem_t sem)
{
}

int chry_mempool_osal_sem_take(chry_mempool_osal_sem_t sem, uint32_t timeout)
{
    return 0;
}

int chry_mempool_osal_sem_give(chry_mempool_osal_sem_t sem)
{
    return 0;
}

void *chry_mempool_osal_malloc(size_t size)
{
    return malloc(size);
}

void chry_mempool_osal_free(void *ptr)
{
    free(ptr);
}