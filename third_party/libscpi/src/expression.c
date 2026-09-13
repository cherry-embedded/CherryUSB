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
 * @file   expression.c
 *
 * @brief  Expressions handling
 *
 *
 */

#include "scpi/expression.h"
#include "scpi/error.h"
#include "scpi/parser.h"

#include "lexer_private.h"

/**
 * Parse one range or single value
 * @param state lexer state
 * @param isRange return true if parsed expression is range
 * @param valueFrom return parsed value from
 * @param valueTo return parsed value to
 * @return SCPI_EXPR_OK - parsing was succesful
 *         SCPI_EXPR_ERROR - parser error
 *         SCPI_EXPR_NO_MORE - no more data
 */
static scpi_expr_result_t numericRange(lex_state_t * state, scpi_bool_t * isRange, scpi_token_t * valueFrom, scpi_token_t * valueTo) {
    if (scpiLex_DecimalNumericProgramData(state, valueFrom)) {
        if (scpiLex_Colon(state, valueTo)) {
            *isRange = TRUE;
            if (scpiLex_DecimalNumericProgramData(state, valueTo)) {
                return SCPI_EXPR_OK;
            } else {
                return SCPI_EXPR_ERROR;
            }
        } else {
            *isRange = FALSE;
            return SCPI_EXPR_OK;
        }
    }

    return SCPI_EXPR_NO_MORE;
}

/**
 * Parse entry on specified position
 * @param context scpi context
 * @param param input parameter
 * @param index index of position (start from 0)
 * @param isRange return true if expression at index was range
 * @param valueFrom return value from
 * @param valueTo return value to
 * @return SCPI_EXPR_OK - parsing was succesful
 *         SCPI_EXPR_ERROR - parser error
 *         SCPI_EXPR_NO_MORE - no more data
 * @see SCPI_ExprNumericListEntryInt, SCPI_ExprNumericListEntryDouble
 */
scpi_expr_result_t SCPI_ExprNumericListEntry(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, scpi_parameter_t * valueFrom, scpi_parameter_t * valueTo) {
    lex_state_t lex;
    int i;
    scpi_expr_result_t res = SCPI_EXPR_OK;

    if (!isRange || !valueFrom || !valueTo || !param) {
        SCPI_ErrorPush(context, SCPI_ERROR_SYSTEM_ERROR);
        return SCPI_EXPR_ERROR;
    }

    if (param->type != SCPI_TOKEN_PROGRAM_EXPRESSION) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_EXPR_ERROR;
    }

    lex.buffer = param->ptr + 1;
    lex.pos = lex.buffer;
    lex.len = param->len - 2;

    for (i = 0; i <= index; i++) {
        res = numericRange(&lex, isRange, valueFrom, valueTo);
        if (res != SCPI_EXPR_OK) {
            break;
        }
        if (i != index) {
            if (!scpiLex_Comma(&lex, valueFrom)) {
                res = scpiLex_IsEos(&lex) ? SCPI_EXPR_NO_MORE : SCPI_EXPR_ERROR;
                break;
            }
        }
    }

    if (res == SCPI_EXPR_ERROR) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXPRESSION_PARSING_ERROR);
    }
    return res;
}

/**
 * Parse entry on specified position and convert result to int32_t
 * @param context scpi context
 * @param param input parameter
 * @param index index of position (start from 0)
 * @param isRange return true if expression at index was range
 * @param valueFrom return value from
 * @param valueTo return value to
 * @return SCPI_EXPR_OK - parsing was succesful
 *         SCPI_EXPR_ERROR - parser error
 *         SCPI_EXPR_NO_MORE - no more data
 * @see SCPI_ExprNumericListEntry, SCPI_ExprNumericListEntryDouble
 */
scpi_expr_result_t SCPI_ExprNumericListEntryInt(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, int32_t * valueFrom, int32_t * valueTo) {
    scpi_expr_result_t res;
    scpi_bool_t range = FALSE;
    scpi_parameter_t paramFrom;
    scpi_parameter_t paramTo;

    res = SCPI_ExprNumericListEntry(context, param, index, &range, &paramFrom, &paramTo);
    if (res == SCPI_EXPR_OK) {
        *isRange = range;
        SCPI_ParamToInt32(context, &paramFrom, valueFrom);
        if (range) {
            SCPI_ParamToInt32(context, &paramTo, valueTo);
        }
    }

    return res;
}

/**
 * Parse entry on specified position and convert result to double
 * @param context scpi context
 * @param param input parameter
 * @param index index of position (start from 0)
 * @param isRange return true if expression at index was range
 * @param valueFrom return value from
 * @param valueTo return value to
 * @return SCPI_EXPR_OK - parsing was succesful
 *         SCPI_EXPR_ERROR - parser error
 *         SCPI_EXPR_NO_MORE - no more data
 * @see SCPI_ExprNumericListEntry, SCPI_ExprNumericListEntryInt
 */
scpi_expr_result_t SCPI_ExprNumericListEntryDouble(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, double * valueFrom, double * valueTo) {
    scpi_expr_result_t res;
    scpi_bool_t range = FALSE;
    scpi_parameter_t paramFrom;
    scpi_parameter_t paramTo;

    res = SCPI_ExprNumericListEntry(context, param, index, &range, &paramFrom, &paramTo);
    if (res == SCPI_EXPR_OK) {
        *isRange = range;
        SCPI_ParamToDouble(context, &paramFrom, valueFrom);
        if (range) {
            SCPI_ParamToDouble(context, &paramTo, valueTo);
        }
    }

    return res;
}

/**
 * Parse one channel_spec e.g. "1!5!8"
 * @param context
 * @param state lexer state
 * @param values range values
 * @param length length of values array
 * @param dimensions real number of dimensions
 */
static scpi_expr_result_t channelSpec(scpi_t * context, lex_state_t * state, int32_t * values, size_t length, size_t * dimensions) {
    scpi_parameter_t param;
    size_t i = 0;
    while (scpiLex_DecimalNumericProgramData(state, &param)) {
        if (i < length) {
            SCPI_ParamToInt32(context, &param, &values[i]);
        }

        if (scpiLex_SpecificCharacter(state, &param, '!')) {
            i++;
        } else {
            *dimensions = i + 1;
            return SCPI_EXPR_OK;
        }
    }

    if (i == 0) {
        return SCPI_EXPR_NO_MORE;
    } else {
        /* there was at least one number followed by !, but after ! was not another number */
        return SCPI_EXPR_ERROR;
    }
}

/**
 * Parse channel_range e.g. "1!2:5!6"
 * @param context
 * @param state lexer state
 * @param isRange return true if it is range
 * @param valuesFrom return array of values from
 * @param valuesTo return array of values to
 * @param length length of values arrays
 * @param dimensions real number of dimensions
 */
static scpi_expr_result_t channelRange(scpi_t * context, lex_state_t * state, scpi_bool_t * isRange, int32_t * valuesFrom, int32_t * valuesTo, size_t length, size_t * dimensions) {
    scpi_token_t token;
    scpi_expr_result_t err;
    size_t fromDimensions;
    size_t toDimensions;

    err = channelSpec(context, state, valuesFrom, length, &fromDimensions);
    if (err == SCPI_EXPR_OK) {
        if (scpiLex_Colon(state, &token)) {
            *isRange = TRUE;
            err = channelSpec(context, state, valuesTo, length, &toDimensions);
            if (err != SCPI_EXPR_OK) {
                return SCPI_EXPR_ERROR;
            }
            if (fromDimensions != toDimensions) {
                return SCPI_EXPR_ERROR;
            }
            *dimensions = fromDimensions;
        } else {
            *isRange = FALSE;
            *dimensions = fromDimensions;
            return SCPI_EXPR_OK;
        }
    } else if (err == SCPI_EXPR_NO_MORE) {
        err = SCPI_EXPR_ERROR;
    }

    return err;
}

/**
 * Parse one list entry at specific position e.g. "1!2:5!6"
 * @param context
 * @param param
 * @param index
 * @param isRange return true if it is range
 * @param valuesFrom return array of values from
 * @param valuesTo return array of values to
 * @param length length of values arrays
 * @param dimensions real number of dimensions
 */
scpi_expr_result_t SCPI_ExprChannelListEntry(scpi_t * context, scpi_parameter_t * param, int index, scpi_bool_t * isRange, int32_t * valuesFrom, int32_t * valuesTo, size_t length, size_t * dimensions) {
    lex_state_t lex;
    int i;
    scpi_expr_result_t res = SCPI_EXPR_OK;
    scpi_token_t token;

    if (!isRange || !param || !dimensions || (length && (!valuesFrom || !valuesTo))) {
        SCPI_ErrorPush(context, SCPI_ERROR_SYSTEM_ERROR);
        return SCPI_EXPR_ERROR;
    }

    if (param->type != SCPI_TOKEN_PROGRAM_EXPRESSION) {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_EXPR_ERROR;
    }

    lex.buffer = param->ptr + 1;
    lex.pos = lex.buffer;
    lex.len = param->len - 2;

    /* detect channel list expression */
    if (!scpiLex_SpecificCharacter(&lex, &token, '@')) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXPRESSION_PARSING_ERROR);
        return SCPI_EXPR_ERROR;
    }

    for (i = 0; i <= index; i++) {
        res = channelRange(context, &lex, isRange, valuesFrom, valuesTo, (i == index) ? length : 0, dimensions);
        if (res != SCPI_EXPR_OK) {
            break;
        }
        if (i != index) {
            if (!scpiLex_Comma(&lex, &token)) {
                res = scpiLex_IsEos(&lex) ? SCPI_EXPR_NO_MORE : SCPI_EXPR_ERROR;
                break;
            }
        }
    }

    if (res == SCPI_EXPR_ERROR) {
        SCPI_ErrorPush(context, SCPI_ERROR_EXPRESSION_PARSING_ERROR);
    }
    if (res == SCPI_EXPR_NO_MORE) {
        if (!scpiLex_IsEos(&lex)) {
            res = SCPI_EXPR_ERROR;
            SCPI_ErrorPush(context, SCPI_ERROR_EXPRESSION_PARSING_ERROR);
        }
    }
    return res;
}
