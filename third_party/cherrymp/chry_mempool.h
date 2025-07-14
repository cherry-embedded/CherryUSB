/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHRY_MEMPOOL_H
#define CHRY_MEMPOOL_H

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "chry_ringbuffer.h"

typedef void *chry_mempool_osal_sem_t;

#ifndef CONFIG_CHRY_MEMPOOL_MAX_BLOCK_COUNT
#define CONFIG_CHRY_MEMPOOL_MAX_BLOCK_COUNT 128
#endif

struct chry_mempool {
    chry_ringbuffer_t in;
    chry_ringbuffer_t out;
    chry_mempool_osal_sem_t out_sem;

    void *block;
    uint32_t block_size;
    uint32_t block_count;
    uint8_t in_buf[sizeof(uintptr_t) * CONFIG_CHRY_MEMPOOL_MAX_BLOCK_COUNT];
    uint8_t out_buf[sizeof(uintptr_t) * CONFIG_CHRY_MEMPOOL_MAX_BLOCK_COUNT];
};

#ifdef __cplusplus
extern "C" {
#endif

chry_mempool_osal_sem_t chry_mempool_osal_sem_create(uint32_t max_count);
void chry_mempool_osal_sem_delete(chry_mempool_osal_sem_t sem);
int chry_mempool_osal_sem_take(chry_mempool_osal_sem_t sem, uint32_t timeout);
int chry_mempool_osal_sem_give(chry_mempool_osal_sem_t sem);

int chry_mempool_create(struct chry_mempool *pool, void *block, uint32_t block_size, uint32_t block_count);
uintptr_t *chry_mempool_alloc(struct chry_mempool *pool);
int chry_mempool_free(struct chry_mempool *pool, uintptr_t *item);
int chry_mempool_send(struct chry_mempool *pool, uintptr_t *item);
int chry_mempool_recv(struct chry_mempool *pool, uintptr_t **item, uint32_t timeout);
void chry_mempool_reset(struct chry_mempool *pool);

#ifdef __cplusplus
}
#endif

#endif