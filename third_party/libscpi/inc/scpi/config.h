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
 * @file   config.h
 * @date   Wed Mar 20 12:21:26 UTC 2013
 *
 * @brief  SCPI Configuration
 *
 *
 */

#ifndef __SCPI_CONFIG_H_
#define __SCPI_CONFIG_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include "cc.h"

#ifdef SCPI_USER_CONFIG
#include "scpi_user_config.h"
#endif

/* set the termination character(s)   */
#define LINE_ENDING_CR          "\r"    /*   use a <CR> carriage return as termination charcter */
#define LINE_ENDING_LF          "\n"    /*   use a <LF> line feed as termination charcter */
#define LINE_ENDING_CRLF        "\r\n"  /*   use <CR><LF> carriage return + line feed as termination charcters */

#ifndef SCPI_LINE_ENDING
#define SCPI_LINE_ENDING        LINE_ENDING_CRLF
#endif

#ifndef USE_CUSTOM_REGISTERS
#define USE_CUSTOM_REGISTERS 0
#endif

/**
 * Detect, if it has limited resources or it is running on a full blown operating system.
 * All values can be overiden by scpi_user_config.h
 */
#define SYSTEM_BARE_METAL 0
#define SYSTEM_FULL_BLOWN 1

/* This should cover all windows compilers (msvc, mingw, cvi) and all Linux/OSX/BSD and other UNIX compatible systems (gcc, clang) */
#if defined(_WIN32) || defined(_WIN64) || defined(__unix) || defined(__unix__) || defined(__APPLE__)
#define SYSTEM_TYPE SYSTEM_FULL_BLOWN
#else
#define SYSTEM_TYPE SYSTEM_BARE_METAL
#endif

/**
 * Enable full error list
 * 0 = Minimal set of errors
 * 1 = Full set of errors
 *
 * For small systems, full set of errors will occupy large ammount of data
 * It is enabled by default on full blown systems and disabled on limited bare metal systems
 */
#ifndef USE_FULL_ERROR_LIST
#define USE_FULL_ERROR_LIST SYSTEM_TYPE
#endif

/**
 * Enable also LIST_OF_USER_ERRORS to be included
 * 0 = Use only library defined errors
 * 1 = Use also LIST_OF_USER_ERRORS
 */
#ifndef USE_USER_ERROR_LIST
#define USE_USER_ERROR_LIST 0
#endif

#ifndef USE_DEVICE_DEPENDENT_ERROR_INFORMATION
#define USE_DEVICE_DEPENDENT_ERROR_INFORMATION SYSTEM_TYPE
#endif

#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION
#ifndef USE_MEMORY_ALLOCATION_FREE
#define USE_MEMORY_ALLOCATION_FREE 1
#endif
#else
#ifndef USE_MEMORY_ALLOCATION_FREE
#define USE_MEMORY_ALLOCATION_FREE 0
#endif
#endif

#ifndef USE_COMMAND_TAGS
#define USE_COMMAND_TAGS 1
#endif

#ifndef USE_DEPRECATED_FUNCTIONS
#define USE_DEPRECATED_FUNCTIONS 1
#endif

#ifndef USE_CUSTOM_DTOSTRE
#define USE_CUSTOM_DTOSTRE 0
#endif

#ifndef USE_UNITS_IMPERIAL
#define USE_UNITS_IMPERIAL 0
#endif

#ifndef USE_UNITS_ANGLE
#define USE_UNITS_ANGLE SYSTEM_TYPE
#endif

#ifndef USE_UNITS_PARTICLES
#define USE_UNITS_PARTICLES SYSTEM_TYPE
#endif

#ifndef USE_UNITS_DISTANCE
#define USE_UNITS_DISTANCE SYSTEM_TYPE
#endif

#ifndef USE_UNITS_MAGNETIC
#define USE_UNITS_MAGNETIC SYSTEM_TYPE
#endif

#ifndef USE_UNITS_LIGHT
#define USE_UNITS_LIGHT SYSTEM_TYPE
#endif

#ifndef USE_UNITS_ENERGY_FORCE_MASS
#define USE_UNITS_ENERGY_FORCE_MASS SYSTEM_TYPE
#endif

#ifndef USE_UNITS_TIME
#define USE_UNITS_TIME SYSTEM_TYPE
#endif

#ifndef USE_UNITS_TEMPERATURE
#define USE_UNITS_TEMPERATURE SYSTEM_TYPE
#endif

#ifndef USE_UNITS_RATIO
#define USE_UNITS_RATIO SYSTEM_TYPE
#endif

#ifndef USE_UNITS_POWER
#define USE_UNITS_POWER 1
#endif

#ifndef USE_UNITS_FREQUENCY
#define USE_UNITS_FREQUENCY 1
#endif

#ifndef USE_UNITS_ELECTRIC
#define USE_UNITS_ELECTRIC 1
#endif

#ifndef USE_UNITS_ELECTRIC_CHARGE_CONDUCTANCE
#define USE_UNITS_ELECTRIC_CHARGE_CONDUCTANCE SYSTEM_TYPE
#endif

/* define local macros depending on existance of strnlen */
#if HAVE_STRNLEN
#define SCPIDEFINE_strnlen(s, l)	strnlen((s), (l))
#else
#define SCPIDEFINE_strnlen(s, l)	BSD_strnlen((s), (l))
#endif

/* define local macros depending on existance of strncasecmp and strnicmp */
#if HAVE_STRNCASECMP
#define SCPIDEFINE_strncasecmp(s1, s2, l) strncasecmp((s1), (s2), (l))
#elif HAVE_STRNICMP
#define SCPIDEFINE_strncasecmp(s1, s2, l) strnicmp((s1), (s2), (l))
#else
#define SCPIDEFINE_strncasecmp(s1, s2, l) OUR_strncasecmp((s1), (s2), (l))
#endif

#if HAVE_DTOSTRE
#define SCPIDEFINE_floatToStr(v, s, l) dtostre((double)(v), (s), 6, DTOSTR_PLUS_SIGN | DTOSTR_ALWAYS_SIGN | DTOSTR_UPPERCASE)
#elif USE_CUSTOM_DTOSTRE
#define SCPIDEFINE_floatToStr(v, s, l) SCPI_dtostre((v), (s), (l), 6, 0)
#elif HAVE_SNPRINTF
#define SCPIDEFINE_floatToStr(v, s, l) snprintf((s), (l), "%g", (v))
#else
#define SCPIDEFINE_floatToStr(v, s, l) SCPI_dtostre((v), (s), (l), 6, 0)
#endif

#if HAVE_DTOSTRE
#define SCPIDEFINE_doubleToStr(v, s, l) dtostre((v), (s), 15, DTOSTR_PLUS_SIGN | DTOSTR_ALWAYS_SIGN | DTOSTR_UPPERCASE)
#elif USE_CUSTOM_DTOSTRE
#define SCPIDEFINE_doubleToStr(v, s, l) SCPI_dtostre((v), (s), (l), 15, 0)
#elif HAVE_SNPRINTF
#define SCPIDEFINE_doubleToStr(v, s, l) snprintf((s), (l), "%.15lg", (v))
#else
#define SCPIDEFINE_doubleToStr(v, s, l) SCPI_dtostre((v), (s), (l), 15, 0)
#endif

#if USE_DEVICE_DEPENDENT_ERROR_INFORMATION

  #if USE_MEMORY_ALLOCATION_FREE
    #include <stdlib.h>
    #include <string.h>
    #define SCPIDEFINE_DESCRIPTION_MAX_PARTS            2
    #if HAVE_STRNDUP
      #define SCPIDEFINE_strndup(h, s, l)               strndup((s), (l))
    #else
      #define SCPIDEFINE_strndup(h, s, l)               OUR_strndup((s), (l))
    #endif
    #define SCPIDEFINE_free(h, s, r)                    free((s))
  #else
    #define SCPIDEFINE_DESCRIPTION_MAX_PARTS            3
    #define SCPIDEFINE_strndup(h, s, l)                 scpiheap_strndup((h), (s), (l))
    #define SCPIDEFINE_free(h, s, r)                    scpiheap_free((h), (s), (r))
    #define SCPIDEFINE_get_parts(h, s, l1, s2, l2)      scpiheap_get_parts((h), (s), (l1), (s2), (l2))
  #endif
#else
  #define SCPIDEFINE_DESCRIPTION_MAX_PARTS              1
  #define SCPIDEFINE_strndup(h, s, l)                   NULL
  #define SCPIDEFINE_free(h, s, r)
#endif

#if HAVE_SIGNBIT
  #define SCPIDEFINE_signbit(n)                         signbit(n)
#else
  #define SCPIDEFINE_signbit(n)                         ((n)<0)
#endif

#if HAVE_FINITE
  #define SCPIDEFINE_isfinite(n)                        finite(n)
#elif HAVE_ISFINITE
  #define SCPIDEFINE_isfinite(n)                        isfinite(n)
#else
  #define SCPIDEFINE_isfinite(n)                        (!SCPIDEFINE_isnan((n)) && ((n) < INFINITY) && ((n) > -INFINITY))
#endif

#if HAVE_STRTOF
  #define SCPIDEFINE_strtof(n, p)                       strtof((n), (p))
#else
  #define SCPIDEFINE_strtof(n, p)                       strtod((n), (p))
#endif

#if HAVE_STRTOLL
  #define SCPIDEFINE_strtoll(n, p, b)                   strtoll((n), (p), (b))
  #define SCPIDEFINE_strtoull(n, p, b)                  strtoull((n), (p), (b))
#else
  #define SCPIDEFINE_strtoll(n, p, b)                   strtoll((n), (p), (b))
  #define SCPIDEFINE_strtoull(n, p, b)                  strtoull((n), (p), (b))
  extern long long int strtoll(const char *nptr, char **endptr, int base);
  extern unsigned long long int strtoull(const char *nptr, char **endptr, int base);
  /* TODO: implement OUR_strtoll and OUR_strtoull */
  /* #warning "64bit string to int conversion not implemented" */
#endif

#if HAVE_ISNAN
  #define SCPIDEFINE_isnan(n)                           isnan((n))
#else
  #define SCPIDEFINE_isnan(n)                           ((n) != (n))
#endif

#ifndef NAN
  #define NAN                                           (0.0 / 0.0)
#endif

#ifndef INFINITY
  #define INFINITY                                      (1.0 / 0.0)
#endif

#ifdef	__cplusplus
}
#endif

#endif
