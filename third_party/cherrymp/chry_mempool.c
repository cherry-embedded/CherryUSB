/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "chry_mempool.h"

int chry_mempool_create(struct chry_mempool *pool, void *block, uint32_t block_size, uint32_t block_count)
{
    uintptr_t addr;
    uint8_t *ringbuf;

    ringbuf = usb_osal_malloc(sizeof(uintptr_t) * block_count);
    memset(ringbuf, 0, sizeof(uintptr_t) * block_count);

    if (chry_ringbuffer_init(&pool->rb, ringbuf, sizeof(uintptr_t) * block_count) == -1) {
        usb_osal_free(ringbuf);
        return -1;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        addr = ((uintptr_t)block + i * block_size);
        chry_ringbuffer_write(&pool->rb, &addr, sizeof(uintptr_t));
    }
    pool->mq = usb_osal_mq_create(block_count);
    if (pool->mq == NULL) {
        return -1;
    }
    return 0;
}

void chry_mempool_delete(struct chry_mempool *pool)
{
    usb_osal_mq_delete(pool->mq);
    chry_ringbuffer_reset(&pool->rb);
    usb_osal_free(pool->rb.pool);
}

uintptr_t *chry_mempool_alloc(struct chry_mempool *pool)
{
    uintptr_t *addr;
    bool ret;

    ret = chry_ringbuffer_read(&pool->rb, (uintptr_t *)&addr, sizeof(uintptr_t));
    if (ret == false) {
        return NULL;
    } else {
        return addr;
    }
}

int chry_mempool_free(struct chry_mempool *pool, uintptr_t *item)
{
    uintptr_t addr;

    addr = (uintptr_t)item;
    return chry_ringbuffer_write(&pool->rb, &addr, sizeof(uintptr_t));
}

int chry_mempool_send(struct chry_mempool *pool, uintptr_t *item)
{
    return usb_osal_mq_send(pool->mq, (uintptr_t)item);
}

int chry_mempool_recv(struct chry_mempool *pool, uintptr_t **item, uint32_t timeout)
{
    return usb_osal_mq_recv(pool->mq, (uintptr_t *)item, timeout);
}