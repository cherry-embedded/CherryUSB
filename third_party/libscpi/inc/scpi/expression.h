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
 * @file   expression.h
 *
 * @brief  Expressions handling
 *
 *
 */
#ifndef SCPI_EXPRESSION_H
#define SCPI_EXPRESSION_H

#include "scpi/config.h"
#include "scpi/types.h"

#ifdef __cplusplus
extern "C" {
#endif

    enum _scpi_expr_result_t {
        SCPI_EXPR_OK = 0,
        SCPI_EXPR_ERROR,
        SCPI_EXPR_NO_MORE,
    };
    typedef enum _scpi_expr_result_t scpi_expr_result_t;

    scpi_expr_result_t SCPI_ExprNumericListEntry(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, scpi_parameter_t * valueFrom, scpi_parameter_t * valueTo);
    scpi_expr_result_t SCPI_ExprNumericListEntryInt(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, int32_t * valueFrom, int32_t * valueTo);
    scpi_expr_result_t SCPI_ExprNumericListEntryDouble(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, double * valueFrom, double * valueTo);
    scpi_expr_result_t SCPI_ExprChannelListEntry(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, int32_t * valuesFrom, int32_t * valuesTo, size_t length, size_t * dimensions);

#ifdef __cplusplus
}
#endif

#endif /* SCPI_EXPRESSION_H */
