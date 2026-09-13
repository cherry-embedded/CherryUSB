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
 * @file   utils.h
 * 
 * @brief  Conversion routines and string manipulation routines
 * 
 * 
 */

#ifndef SCPI_UTILS_H
#define	SCPI_UTILS_H

#include <stdint.h>
#include "scpi/types.h"

#ifdef	__cplusplus
extern "C" {
#endif

    size_t SCPI_UInt32ToStrBase(uint32_t val, char * str, size_t len, int8_t base);
    size_t SCPI_Int32ToStr(int32_t val, char * str, size_t len);
    size_t SCPI_UInt64ToStrBase(uint64_t val, char * str, size_t len, int8_t base);
    size_t SCPI_Int64ToStr(int64_t val, char * str, size_t len);
    size_t SCPI_FloatToStr(float val, char * str, size_t len);
    size_t SCPI_DoubleToStr(double val, char * str, size_t len);

    /* deprecated finction, should be removed later */
#define SCPI_LongToStr(val, str, len, base) SCPI_Int32ToStr((val), (str), (len), (base), TRUE)

#ifdef	__cplusplus
}
#endif

#endif	/* SCPI_UTILS_H */

