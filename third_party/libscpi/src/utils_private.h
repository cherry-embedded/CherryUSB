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
 * @file   scpi_utils.h
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  Conversion routines and string manipulation routines
 *
 *
 */

#ifndef SCPI_UTILS_PRIVATE_H
#define	SCPI_UTILS_PRIVATE_H

#include <stdint.h>
#include "scpi/config.h"
#include "scpi/types.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define LOCAL __attribute__((visibility ("hidden")))
#else
#define LOCAL
#endif

    char * strnpbrk(const char *str, size_t size, const char *set) LOCAL;
    scpi_bool_t compareStr(const char * str1, size_t len1, const char * str2, size_t len2) LOCAL;
    scpi_bool_t compareStrAndNum(const char * str1, size_t len1, const char * str2, size_t len2, int32_t * num) LOCAL;
    size_t UInt32ToStrBaseSign(uint32_t val, char * str, size_t len, int8_t base, scpi_bool_t sign) LOCAL;
    size_t UInt64ToStrBaseSign(uint64_t val, char * str, size_t len, int8_t base, scpi_bool_t sign) LOCAL;
    size_t strBaseToInt32(const char * str, int32_t * val, int8_t base) LOCAL;
    size_t strBaseToUInt32(const char * str, uint32_t * val, int8_t base) LOCAL;
    size_t strBaseToInt64(const char * str, int64_t * val, int8_t base) LOCAL;
    size_t strBaseToUInt64(const char * str, uint64_t * val, int8_t base) LOCAL;
    size_t strToFloat(const char * str, float * val) LOCAL;
    size_t strToDouble(const char * str, double * val) LOCAL;
    scpi_bool_t locateText(const char * str1, size_t len1, const char ** str2, size_t * len2) LOCAL;
    scpi_bool_t locateStr(const char * str1, size_t len1, const char ** str2, size_t * len2) LOCAL;
    size_t skipWhitespace(const char * cmd, size_t len) LOCAL;
    scpi_bool_t matchPattern(const char * pattern, size_t pattern_len, const char * str, size_t str_len, int32_t * num) LOCAL;
    scpi_bool_t matchCommand(const char * pattern, const char * cmd, size_t len, int32_t *numbers, size_t numbers_len, int32_t default_value) LOCAL;
    scpi_bool_t composeCompoundCommand(const scpi_token_t * prev, scpi_token_t * current) LOCAL;

#define SCPI_DTOSTRE_UPPERCASE   1
#define SCPI_DTOSTRE_ALWAYS_SIGN 2
#define SCPI_DTOSTRE_PLUS_SIGN   4
    char * SCPI_dtostre(double __val, char * __s, size_t __ssize, unsigned char __prec, unsigned char __flags);

    scpi_array_format_t SCPI_GetNativeFormat(void);
    uint16_t SCPI_Swap16(uint16_t val);
    uint32_t SCPI_Swap32(uint32_t val);
    uint64_t SCPI_Swap64(uint64_t val);

#if !HAVE_STRNLEN
    size_t BSD_strnlen(const char *s, size_t maxlen) LOCAL;
#endif

#if !HAVE_STRNCASECMP && !HAVE_STRNICMP
    int OUR_strncasecmp(const char *s1, const char *s2, size_t n) LOCAL;
#endif

#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION && !USE_MEMORY_ALLOCATION_FREE
    void scpiheap_init(scpi_error_info_heap_t * heap, char * error_info_heap, size_t error_info_heap_length);
    char * scpiheap_strndup(scpi_error_info_heap_t * heap, const char *s, size_t n) LOCAL;
    void scpiheap_free(scpi_error_info_heap_t * heap, char *s, scpi_bool_t rollback) LOCAL;
    scpi_bool_t scpiheap_get_parts(scpi_error_info_heap_t * heap, const char *s1, size_t * len1, const char ** s2, size_t * len2) LOCAL;
#endif

#if !HAVE_STRNDUP
    char *OUR_strndup(const char *s, size_t n);
#endif

#ifndef min
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#if 0
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* SCPI_UTILS_PRIVATE_H */

