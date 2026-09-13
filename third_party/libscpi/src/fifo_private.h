/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   scpi_fifo.h
 * @date   Thu Nov 15 10:58:45 UTC 2012
 * 
 * @brief  basic FIFO implementation
 * 
 * 
 */

#ifndef SCPI_FIFO_H
#define	SCPI_FIFO_H

#include "scpi/types.h"
#include "utils_private.h"

#ifdef	__cplusplus
extern "C" {
#endif

    void fifo_init(scpi_fifo_t * fifo, scpi_error_t * data, int16_t size) LOCAL;
    void fifo_clear(scpi_fifo_t * fifo) LOCAL;
    scpi_bool_t fifo_is_empty(scpi_fifo_t * fifo) LOCAL;
    scpi_bool_t fifo_is_full(scpi_fifo_t * fifo) LOCAL;
    scpi_bool_t fifo_add(scpi_fifo_t * fifo, const scpi_error_t * value) LOCAL;
    scpi_bool_t fifo_remove(scpi_fifo_t * fifo, scpi_error_t * value) LOCAL;
    scpi_bool_t fifo_remove_last(scpi_fifo_t * fifo, scpi_error_t * value) LOCAL;
    scpi_bool_t fifo_count(scpi_fifo_t * fifo, int16_t * value) LOCAL;

#ifdef	__cplusplus
}
#endif

#endif	/* SCPI_FIFO_H */
