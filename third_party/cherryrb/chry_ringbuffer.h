/*
 * Copyright (c) 2022, Egahp
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CHRY_RINGBUFFER_H
#define CHRY_RINGBUFFER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t in;   /*!< Define the write pointer.               */
    uint32_t out;  /*!< Define the read pointer.                */
    uint32_t mask; /*!< Define the write and read pointer mask. */
    void *pool;    /*!< Define the memory pointer.              */
} chry_ringbuffer_t;

extern int chry_ringbuffer_init(chry_ringbuffer_t *rb, void *pool, uint32_t size);
extern void chry_ringbuffer_reset(chry_ringbuffer_t *rb);
extern void chry_ringbuffer_reset_read(chry_ringbuffer_t *rb);

extern uint32_t chry_ringbuffer_get_size(chry_ringbuffer_t *rb);
extern uint32_t chry_ringbuffer_get_used(chry_ringbuffer_t *rb);
extern uint32_t chry_ringbuffer_get_free(chry_ringbuffer_t *rb);

extern bool chry_ringbuffer_check_full(chry_ringbuffer_t *rb);
extern bool chry_ringbuffer_check_empty(chry_ringbuffer_t *rb);

extern bool chry_ringbuffer_write_byte(chry_ringbuffer_t *rb, uint8_t byte);
extern bool chry_ringbuffer_overwrite_byte(chry_ringbuffer_t *rb, uint8_t byte);
extern bool chry_ringbuffer_peek_byte(chry_ringbuffer_t *rb, uint8_t *byte);
extern bool chry_ringbuffer_read_byte(chry_ringbuffer_t *rb, uint8_t *byte);
extern bool chry_ringbuffer_drop_byte(chry_ringbuffer_t *rb);

extern uint32_t chry_ringbuffer_write(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_overwrite(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_peek(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_read(chry_ringbuffer_t *rb, void *data, uint32_t size);
extern uint32_t chry_ringbuffer_drop(chry_ringbuffer_t *rb, uint32_t size);

extern void *chry_ringbuffer_linear_write_setup(chry_ringbuffer_t *rb, uint32_t *size);
extern void *chry_ringbuffer_linear_read_setup(chry_ringbuffer_t *rb, uint32_t *size);
extern uint32_t chry_ringbuffer_linear_write_done(chry_ringbuffer_t *rb, uint32_t size);
extern uint32_t chry_ringbuffer_linear_read_done(chry_ringbuffer_t *rb, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif
