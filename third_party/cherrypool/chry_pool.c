#include "chry_pool.h"

int chry_pool_create(struct chry_pool *pool, uint32_t *ringbuf, void *item, uint32_t item_size, uint32_t item_count)
{
    uintptr_t addr;

    chry_ringbuffer_init(&pool->rb, ringbuf, sizeof(uintptr_t) * item_count);

    for (uint32_t i = 0; i < item_count; i++) {
        addr = ((uintptr_t)item + i * item_size);
        chry_ringbuffer_write(&pool->rb, &addr, sizeof(uintptr_t));
    }
    pool->mq = usb_osal_mq_create(item_count);
    if (pool->mq == NULL) {
        return -1;
    }
    return 0;
}

uintptr_t *chry_pool_item_alloc(struct chry_pool *pool)
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

int chry_pool_item_free(struct chry_pool *pool, uintptr_t *item)
{
    uintptr_t addr;

    addr = (uintptr_t)item;
    return chry_ringbuffer_write(&pool->rb, &addr, sizeof(uintptr_t));
}

int chry_pool_item_send(struct chry_pool *pool, uintptr_t *frame)
{
    return usb_osal_mq_send(pool->mq, (uintptr_t)frame);
}

int chry_pool_item_recv(struct chry_pool *pool, uintptr_t **frame, uint32_t timeout)
{
    return usb_osal_mq_recv(pool->mq, (uintptr_t *)frame, timeout);
}