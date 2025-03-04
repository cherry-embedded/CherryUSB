/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "chry_mempool.h"

int chry_mempool_create(struct chry_mempool *pool, void *block, uint32_t block_size, uint32_t block_count)
{
    uintptr_t *item;

    if (block_count > CONFIG_CHRY_MEMPOOL_MAX_BLOCK_COUNT) {
        return -1;
    }

    if (pool->block_size % 4) {
        return -1;
    }

    if (chry_ringbuffer_init(&pool->in, pool->in_buf, sizeof(uintptr_t) * block_count) == -1) {
        return -1;
    }

    if (chry_ringbuffer_init(&pool->out, pool->out_buf, sizeof(uintptr_t) * block_count) == -1) {
        return -1;
    }

    pool->out_sem = chry_mempool_osal_sem_create(block_count);
    if (pool->out_sem == NULL) {
        return -1;
    }

    pool->block = block;
    pool->block_size = block_size;
    pool->block_count = block_count;

    for (uint32_t i = 0; i < pool->block_count; i++) {
        item = (uintptr_t *)((uint8_t *)pool->block + i * pool->block_size);
        chry_mempool_free(pool, item);
    }

    return 0;
}

void chry_mempool_delete(struct chry_mempool *pool)
{
    chry_mempool_osal_sem_delete(pool->out_sem);
    chry_ringbuffer_reset(&pool->in);
    chry_ringbuffer_reset(&pool->out);
}

uintptr_t *chry_mempool_alloc(struct chry_mempool *pool)
{
    uintptr_t *addr;
    uint32_t len;

    len = chry_ringbuffer_read(&pool->in, (uintptr_t *)&addr, sizeof(uintptr_t));
    if (len == 0) {
        return NULL;
    } else {
        return addr;
    }
}

int chry_mempool_free(struct chry_mempool *pool, uintptr_t *item)
{
    uintptr_t addr;

    addr = (uintptr_t)item;
    return chry_ringbuffer_write(&pool->in, &addr, sizeof(uintptr_t));
}

int chry_mempool_send(struct chry_mempool *pool, uintptr_t *item)
{
    uintptr_t addr;

    addr = (uintptr_t)item;
    chry_ringbuffer_write(&pool->out, &addr, sizeof(uintptr_t));
    return chry_mempool_osal_sem_give(pool->out_sem);
}

int chry_mempool_recv(struct chry_mempool *pool, uintptr_t **item, uint32_t timeout)
{
    uint32_t len;
    int ret;

    ret = chry_mempool_osal_sem_take(pool->out_sem, timeout);
    if (ret < 0) {
        return -1;
    }

    len = chry_ringbuffer_read(&pool->out, (uintptr_t *)item, sizeof(uintptr_t));
    if (len == 0) {
        return -1;
    } else {
        return 0;
    }
}

void chry_mempool_reset(struct chry_mempool *pool)
{
    uintptr_t *item;

    chry_ringbuffer_reset(&pool->in);
    chry_ringbuffer_reset(&pool->out);

    for (uint32_t i = 0; i < pool->block_count; i++) {
        item = (uintptr_t *)((uint8_t *)pool->block + i * pool->block_size);
        chry_mempool_free(pool, item);
    }
}