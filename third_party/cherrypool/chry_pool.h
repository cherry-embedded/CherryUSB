#ifndef CHRY_POOL_H
#define CHRY_POOL_H

#include "usb_config.h"
#include "usb_osal.h"
#include "chry_ringbuffer.h"

struct chry_pool {
    chry_ringbuffer_t rb;
    usb_osal_mq_t mq;
};

int chry_pool_create(struct chry_pool *pool, uint32_t *ringbuf, void *item, uint32_t item_size, uint32_t item_count);
uintptr_t *chry_pool_item_alloc(struct chry_pool *pool);
int chry_pool_item_free(struct chry_pool *pool, uintptr_t *item);
int chry_pool_item_send(struct chry_pool *pool, uintptr_t *frame);
int chry_pool_item_recv(struct chry_pool *pool, uintptr_t **frame, uint32_t timeout);

#endif