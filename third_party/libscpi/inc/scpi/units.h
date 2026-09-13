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
 * @file   scpi_units.h
 * @date   Thu Nov 15 10:58:45 UTC 2012
 * 
 * @brief  SCPI Units
 * 
 * 
 */

#ifndef SCPI_UNITS_H
#define	SCPI_UNITS_H

#include "scpi/types.h"

#ifdef	__cplusplus
extern "C" {
#endif

    extern const scpi_unit_def_t scpi_units_def[];
    extern const scpi_choice_def_t scpi_special_numbers_def[];

    scpi_bool_t SCPI_ParamNumber(scpi_t * context, const scpi_choice_def_t * special, scpi_number_t * value, scpi_bool_t mandatory);

    scpi_bool_t SCPI_ParamTranslateNumberVal(scpi_t * context, scpi_parameter_t * parameter);
    size_t SCPI_NumberToStr(scpi_t * context, const scpi_choice_def_t * special, scpi_number_t * value, char * str, size_t len);

#ifdef	__cplusplus
}
#endif

#endif	/* SCPI_UNITS_H */

