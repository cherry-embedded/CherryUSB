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
 * @file   scpi_error.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  Error handling and storing routines
 *
 *
 */

#include <stdint.h>

#include "scpi/parser.h"
#include "scpi/ieee488.h"
#include "scpi/error.h"
#include "fifo_private.h"
#include "scpi/constants.h"

#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION
#define SCPI_ERROR_SETVAL(e, c, i) do { (e)->error_code = (c); (e)->device_dependent_info = (i); } while(0)
#else
#define SCPI_ERROR_SETVAL(e, c, i) do { (e)->error_code = (c); (void)(i);} while(0)
#endif

/**
 * Initialize error queue
 * @param context - scpi context
 */
void SCPI_ErrorInit(scpi_t * context, scpi_error_t * data, int16_t size) {
    fifo_init(&context->error_queue, data, size);
}

/**
 * Emit no error
 * @param context scpi context
 */
static void SCPI_ErrorEmitEmpty(scpi_t * context) {
    if ((SCPI_ErrorCount(context) == 0) && (SCPI_RegGet(context, SCPI_REG_STB) & STB_QMA)) {
        SCPI_RegClearBits(context, SCPI_REG_STB, STB_QMA);

        if (context->interface && context->interface->error) {
            context->interface->error(context, 0);
        }
    }
}

/**
 * Emit error
 * @param context scpi context
 * @param err Error to emit
 */
static void SCPI_ErrorEmit(scpi_t * context, int16_t err) {
    SCPI_RegSetBits(context, SCPI_REG_STB, STB_QMA);

    if (context->interface && context->interface->error) {
        context->interface->error(context, err);
    }
}

/**
 * Clear error queue
 * @param context - scpi context
 */
void SCPI_ErrorClear(scpi_t * context) {
#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION
    scpi_error_t error;
    while (fifo_remove(&context->error_queue, &error)) {
        SCPIDEFINE_free(&context->error_info_heap, error.device_dependent_info, false);
    }
#endif
    fifo_clear(&context->error_queue);

    SCPI_ErrorEmitEmpty(context);
}

/**
 * Pop error from queue
 * @param context - scpi context
 * @param error
 * @return
 */
scpi_bool_t SCPI_ErrorPop(scpi_t * context, scpi_error_t * error) {
    if (!error || !context) return FALSE;
    SCPI_ERROR_SETVAL(error, 0, NULL);
    fifo_remove(&context->error_queue, error);

    SCPI_ErrorEmitEmpty(context);

    return TRUE;
}

/**
 * Return number of errors/events in the queue
 * @param context
 * @return
 */
int32_t SCPI_ErrorCount(scpi_t * context) {
    int16_t result = 0;

    fifo_count(&context->error_queue, &result);

    return result;
}

static scpi_bool_t SCPI_ErrorAddInternal(scpi_t * context, int16_t err, char * info, size_t info_len) {
    scpi_error_t error_value;
    /* SCPIDEFINE_strndup is sometimes a dumy that does not reference it's arguments. 
       Since info_len is not referenced elsewhere caoing to void prevents unusd argument warnings */
    (void) info_len;
    char * info_ptr = NULL;
    if (info) {
        info_ptr = SCPIDEFINE_strndup(&context->error_info_heap, info, info_len);
    }
    SCPI_ERROR_SETVAL(&error_value, err, info_ptr);
    if (!fifo_add(&context->error_queue, &error_value)) {
        SCPIDEFINE_free(&context->error_info_heap, error_value.device_dependent_info, true);
        fifo_remove_last(&context->error_queue, &error_value);
        SCPIDEFINE_free(&context->error_info_heap, error_value.device_dependent_info, true);
        SCPI_ERROR_SETVAL(&error_value, SCPI_ERROR_QUEUE_OVERFLOW, NULL);
        fifo_add(&context->error_queue, &error_value);
        return FALSE;
    }
    return TRUE;
}

struct error_reg {
    int16_t from;
    int16_t to;
    scpi_reg_val_t esrBit;
};

#define ERROR_DEFS_N 9

static const struct error_reg errs[ERROR_DEFS_N] = {
    {-100, -199, ESR_CER}, /* Command error (e.g. syntax error) ch 21.8.9    */
    {-200, -299, ESR_EER}, /* Execution Error (e.g. range error) ch 21.8.10  */
    {-300, -399, ESR_DER}, /* Device specific error -300, -399 ch 21.8.11    */
    { 1, 32767, ESR_DER}, /* Device designer provided specific error 1, 32767 ch 21.8.11    */
    {-400, -499, ESR_QER}, /* Query error -400, -499 ch 21.8.12              */
    {-500, -599, ESR_PON}, /* Power on event -500, -599 ch 21.8.13           */
    {-600, -699, ESR_URQ}, /* User Request Event -600, -699 ch 21.8.14       */
    {-700, -799, ESR_REQ}, /* Request Control Event -700, -799 ch 21.8.15    */
    {-800, -899, ESR_OPC}, /* Operation Complete Event -800, -899 ch 21.8.16 */
};

/**
 * Push error to queue
 * @param context
 * @param err - error number
 * @param info - additional text information or NULL for no text
 * @param info_len - length of text or 0 for automatic length
 */
void SCPI_ErrorPushEx(scpi_t * context, int16_t err, char * info, size_t info_len) {
    int i;
    /* automatic calculation of length */
    if (info && info_len == 0) {
        info_len = SCPIDEFINE_strnlen(info, SCPI_STD_ERROR_DESC_MAX_STRING_LENGTH);
    }
    scpi_bool_t queue_overflow = !SCPI_ErrorAddInternal(context, err, info, info_len);

    for (i = 0; i < ERROR_DEFS_N; i++) {
        if ((err <= errs[i].from) && (err >= errs[i].to)) {
            SCPI_RegSetBits(context, SCPI_REG_ESR, errs[i].esrBit);
        }
    }

    SCPI_ErrorEmit(context, err);
    if (queue_overflow) {
        SCPI_ErrorEmit(context, SCPI_ERROR_QUEUE_OVERFLOW);
    }

    if (context) {
        context->cmd_error = TRUE;
    }
}

/**
 * Push error to queue
 * @param context - scpi context
 * @param err - error number
 */
void SCPI_ErrorPush(scpi_t * context, int16_t err) {
    SCPI_ErrorPushEx(context, err, NULL, 0);
    return;
}

/**
 * Translate error number to string
 * @param err - error number
 * @return Error string representation
 */
const char * SCPI_ErrorTranslate(int16_t err) {
    switch (err) {
#define X(def, val, str) case def: return str;
#if USE_FULL_ERROR_LIST
#define XE X
#else
#define XE(def, val, str)
#endif
        LIST_OF_ERRORS

#if USE_USER_ERROR_LIST
        LIST_OF_USER_ERRORS
#endif
#undef X
#undef XE
        default: return "Unknown error";
    }
}


