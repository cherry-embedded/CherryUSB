/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHRY_MEMPOOL_H
#define CHRY_MEMPOOL_H

#include "usb_osal.h"
#include "chry_ringbuffer.h"

struct chry_mempool {
    chry_ringbuffer_t rb;
    usb_osal_mq_t mq;
};

#ifdef __cplusplus
extern "C" {
#endif

int chry_mempool_create(struct chry_mempool *pool, void *block, uint32_t block_size, uint32_t block_count);
uintptr_t *chry_mempool_alloc(struct chry_mempool *pool);
int chry_mempool_free(struct chry_mempool *pool, uintptr_t *item);
int chry_mempool_send(struct chry_mempool *pool, uintptr_t *item);
int chry_mempool_recv(struct chry_mempool *pool, uintptr_t **item, uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif