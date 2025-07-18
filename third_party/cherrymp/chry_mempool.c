/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "chry_mempool.h"

/*****************************************************************************
* @brief        init ringbuffer
*
* @param[in]    rb          ringbuffer instance
* @param[in]    pool        memory pool address
* @param[in]    size        memory size in byte,
*                           must be power of 2 !!!
*
* @retval int               0:Success -1:Error
*****************************************************************************/
static int __chry_ringbuffer_init(chry_mempool_ringbuffer_t *rb, void *pool, uint32_t size)
{
    if (NULL == rb) {
        return -1;
    }

    if (NULL == pool) {
        return -1;
    }

    if ((size < 2) || (size & (size - 1))) {
        return -1;
    }

    rb->in = 0;
    rb->out = 0;
    rb->mask = size - 1;
    rb->pool = pool;

    return 0;
}

/*****************************************************************************
* @brief        reset ringbuffer, clean all data,
*               should be add lock in multithread
*
* @param[in]    rb          ringbuffer instance
*
*****************************************************************************/
static void __chry_ringbuffer_reset(chry_mempool_ringbuffer_t *rb)
{
    rb->in = 0;
    rb->out = 0;
}

/*****************************************************************************
* @brief        write data to ringbuffer,
*               should be add lock in multithread,
*               in single write thread not need lock
*
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
*
* @retval uint32_t          actual write size in byte
*****************************************************************************/
static uint32_t __chry_ringbuffer_write(chry_mempool_ringbuffer_t *rb, void *data, uint32_t size)
{
    uint32_t unused;
    uint32_t offset;
    uint32_t remain;

    unused = (rb->mask + 1) - (rb->in - rb->out);

    if (size > unused) {
        size = unused;
    }

    offset = rb->in & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > size ? size : remain;

    memcpy(((uint8_t *)(rb->pool)) + offset, data, remain);
    memcpy(rb->pool, (uint8_t *)data + remain, size - remain);

    rb->in += size;

    return size;
}

/*****************************************************************************
* @brief        peek data from ringbuffer
*               should be add lock in multithread,
*               in single read thread not need lock
*
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
*
* @retval uint32_t          actual peek size in byte
*****************************************************************************/
static uint32_t __chry_ringbuffer_peek(chry_mempool_ringbuffer_t *rb, void *data, uint32_t size)
{
    uint32_t used;
    uint32_t offset;
    uint32_t remain;

    used = rb->in - rb->out;
    if (size > used) {
        size = used;
    }

    offset = rb->out & rb->mask;

    remain = rb->mask + 1 - offset;
    remain = remain > size ? size : remain;

    memcpy(data, ((uint8_t *)(rb->pool)) + offset, remain);
    memcpy((uint8_t *)data + remain, rb->pool, size - remain);

    return size;
}

/*****************************************************************************
* @brief        read data from ringbuffer
*               should be add lock in multithread,
*               in single read thread not need lock
*
* @param[in]    rb          ringbuffer instance
* @param[in]    data        data pointer
* @param[in]    size        size in byte
*
* @retval uint32_t          actual read size in byte
*****************************************************************************/
static uint32_t __chry_ringbuffer_read(chry_mempool_ringbuffer_t *rb, void *data, uint32_t size)
{
    size = __chry_ringbuffer_peek(rb, data, size);
    rb->out += size;
    return size;
}

int chry_mempool_create(struct chry_mempool *pool, void *block, uint32_t block_size, uint32_t block_count)
{
    uintptr_t *item;

    if (block_count > CONFIG_CHRY_MEMPOOL_MAX_BLOCK_COUNT) {
        return -1;
    }

    if (pool->block_size % 4) {
        return -1;
    }

    if (__chry_ringbuffer_init(&pool->in, pool->in_buf, sizeof(uintptr_t) * block_count) == -1) {
        return -1;
    }

    if (__chry_ringbuffer_init(&pool->out, pool->out_buf, sizeof(uintptr_t) * block_count) == -1) {
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
    __chry_ringbuffer_reset(&pool->in);
    __chry_ringbuffer_reset(&pool->out);
}

uintptr_t *chry_mempool_alloc(struct chry_mempool *pool)
{
    uintptr_t *addr;
    uint32_t len;

    len = __chry_ringbuffer_read(&pool->in, (uintptr_t *)&addr, sizeof(uintptr_t));
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
    return __chry_ringbuffer_write(&pool->in, &addr, sizeof(uintptr_t));
}

int chry_mempool_send(struct chry_mempool *pool, uintptr_t *item)
{
    uintptr_t addr;

    addr = (uintptr_t)item;
    __chry_ringbuffer_write(&pool->out, &addr, sizeof(uintptr_t));
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

    len = __chry_ringbuffer_read(&pool->out, (uintptr_t *)item, sizeof(uintptr_t));
    if (len == 0) {
        return -1;
    } else {
        return 0;
    }
}

void chry_mempool_reset(struct chry_mempool *pool)
{
    uintptr_t *item;

    __chry_ringbuffer_reset(&pool->in);
    __chry_ringbuffer_reset(&pool->out);

    for (uint32_t i = 0; i < pool->block_count; i++) {
        item = (uintptr_t *)((uint8_t *)pool->block + i * pool->block_size);
        chry_mempool_free(pool, item);
    }
}