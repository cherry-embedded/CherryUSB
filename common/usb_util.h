/**
 * @file usb_util.h
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#ifndef _USB_UTIL_H
#define _USB_UTIL_H

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "usb_errno.h"
#include "usb_list.h"
#include "usb_mem.h"

#if defined(__CC_ARM)
#ifndef __USED
#define __USED __attribute__((used))
#endif
#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif
#ifndef __PACKED
#define __PACKED __attribute__((packed))
#endif
#ifndef __PACKED_STRUCT
#define __PACKED_STRUCT __packed struct
#endif
#ifndef __PACKED_UNION
#define __PACKED_UNION __packed union
#endif
#ifndef __ALIGNED
#define __ALIGNED(x) __attribute__((aligned(x)))
#endif
#elif defined(__GNUC__)
#ifndef __USED
#define __USED __attribute__((used))
#endif
#ifndef __WEAK
#define __WEAK __attribute__((weak))
#endif
#ifndef __PACKED
#define __PACKED __attribute__((packed, aligned(1)))
#endif
#ifndef __PACKED_STRUCT
#define __PACKED_STRUCT struct __attribute__((packed, aligned(1)))
#endif
#ifndef __PACKED_UNION
#define __PACKED_UNION union __attribute__((packed, aligned(1)))
#endif
#ifndef __ALIGNED
#define __ALIGNED(x) __attribute__((aligned(x)))
#endif
#elif defined(__ICCARM__)
#ifndef __USED
#if __ICCARM_V8
#define __USED __attribute__((used))
#else
#define __USED _Pragma("__root")
#endif
#endif

#ifndef __WEAK
#if __ICCARM_V8
#define __WEAK __attribute__((weak))
#else
#define __WEAK _Pragma("__weak")
#endif
#endif

#ifndef __PACKED
#if __ICCARM_V8
#define __PACKED __attribute__((packed, aligned(1)))
#else
/* Needs IAR language extensions */
#define __PACKED __packed
#endif
#endif

#ifndef __PACKED_STRUCT
#if __ICCARM_V8
#define __PACKED_STRUCT struct __attribute__((packed, aligned(1)))
#else
/* Needs IAR language extensions */
#define __PACKED_STRUCT __packed struct
#endif
#endif

#ifndef __PACKED_UNION
#if __ICCARM_V8
#define __PACKED_UNION union __attribute__((packed, aligned(1)))
#else
/* Needs IAR language extensions */
#define __PACKED_UNION __packed union
#endif
#endif

#ifndef __ALIGNED
#if __ICCARM_V8
#define __ALIGNED(x) __attribute__((aligned(x)))
#elif (__VER__ >= 7080000)
/* Needs IAR language extensions */
#define __ALIGNED(x) __attribute__((aligned(x)))
#else
#warning No compiler specific solution for __ALIGNED.__ALIGNED is ignored.
#define __ALIGNED(x)
#endif
#endif

#endif

#ifndef __ALIGN_BEGIN
#define __ALIGN_BEGIN
#endif
#ifndef __ALIGN_END
#define __ALIGN_END __attribute__((aligned(4)))
#endif

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

#ifndef LO_BYTE
#define LO_BYTE(x) ((uint8_t)(x & 0x00FF))
#endif

#ifndef HI_BYTE
#define HI_BYTE(x) ((uint8_t)((x & 0xFF00) >> 8))
#endif

/**
 * @def MAX
 * @brief The larger value between @p a and @p b.
 * @note Arguments are evaluated twice.
 */
#ifndef MAX
/* Use Z_MAX for a GCC-only, single evaluation version */
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

/**
 * @def MIN
 * @brief The smaller value between @p a and @p b.
 * @note Arguments are evaluated twice.
 */
#ifndef MIN
/* Use Z_MIN for a GCC-only, single evaluation version */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef BCD
#define BCD(x) ((((x) / 10) << 4) | ((x) % 10))
#endif

#ifdef BIT
#undef BIT
#define BIT(n) (1UL << (n))
#else
#define BIT(n) (1UL << (n))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) \
    ((int)((sizeof(array) / sizeof((array)[0]))))
#endif

#ifndef BSWAP16
#define BSWAP16(u16) (__builtin_bswap16(u16))
#endif
#ifndef BSWAP32
#define BSWAP32(u32) (__builtin_bswap32(u32))
#endif

#define GET_BE16(field) \
    (((uint16_t)(field)[0] << 8) | ((uint16_t)(field)[1]))

#define GET_BE32(field) \
    (((uint32_t)(field)[0] << 24) | ((uint32_t)(field)[1] << 16) | ((uint32_t)(field)[2] << 8) | ((uint32_t)(field)[3] << 0))

#define SET_BE16(field, value)                \
    do {                                      \
        (field)[0] = (uint8_t)((value) >> 8); \
        (field)[1] = (uint8_t)((value) >> 0); \
    } while (0)

#define SET_BE24(field, value)                 \
    do {                                       \
        (field)[0] = (uint8_t)((value) >> 16); \
        (field)[1] = (uint8_t)((value) >> 8);  \
        (field)[2] = (uint8_t)((value) >> 0);  \
    } while (0)

#define SET_BE32(field, value)                 \
    do {                                       \
        (field)[0] = (uint8_t)((value) >> 24); \
        (field)[1] = (uint8_t)((value) >> 16); \
        (field)[2] = (uint8_t)((value) >> 8);  \
        (field)[3] = (uint8_t)((value) >> 0);  \
    } while (0)

#define REQTYPE_GET_DIR(x)   (((x) >> 7) & 0x01)
#define REQTYPE_GET_TYPE(x)  (((x) >> 5) & 0x03U)
#define REQTYPE_GET_RECIP(x) ((x)&0x1F)

#define GET_DESC_TYPE(x)  (((x) >> 8) & 0xFFU)
#define GET_DESC_INDEX(x) ((x)&0xFFU)

#define WBVAL(x) (x & 0xFF), ((x >> 8) & 0xFF)
#define DBVAL(x) (x & 0xFF), ((x >> 8) & 0xFF), ((x >> 16) & 0xFF), ((x >> 24) & 0xFF)

#define USB_DESC_SECTION __attribute__((section("usb_desc"))) __USED __ALIGNED(1)

/* DEBUG level */
#define USB_DBG_ERROR   0
#define USB_DBG_WARNING 1
#define USB_DBG_INFO    2
#define USB_DBG_LOG     3

#ifndef USB_DBG_LEVEL
#define USB_DBG_LEVEL USB_DBG_INFO
#endif

#ifndef USB_DBG_TAG
#define USB_DBG_TAG "USB"
#endif
/*
 * The color for terminal (foreground)
 * BLACK    30
 * RED      31
 * GREEN    32
 * YELLOW   33
 * BLUE     34
 * PURPLE   35
 * CYAN     36
 * WHITE    37
 */
#define _USB_DBG_COLOR(n) printf("\033[" #n "m")
#define _USB_DBG_LOG_HDR(lvl_name, color_n) \
    printf("\033[" #color_n "m[" lvl_name "/" USB_DBG_TAG "] ")
#define _USB_DBG_LOG_X_END \
    printf("\033[0m")

#define usb_dbg_log_line(lvl, color_n, fmt, ...) \
    do {                                         \
        _USB_DBG_LOG_HDR(lvl, color_n);          \
        printf(fmt, ##__VA_ARGS__);              \
        _USB_DBG_LOG_X_END;                      \
    } while (0)

#if (USB_DBG_LEVEL >= USB_DBG_LOG)
#define USB_LOG_DBG(fmt, ...) usb_dbg_log_line("D", 0, fmt, ##__VA_ARGS__)
#else
#define USB_LOG_DBG(...)
#endif

#if (USB_DBG_LEVEL >= USB_DBG_INFO)
#define USB_LOG_INFO(fmt, ...) usb_dbg_log_line("I", 32, fmt, ##__VA_ARGS__)
#else
#define USB_LOG_INFO(...)
#endif

#if (USB_DBG_LEVEL >= USB_DBG_WARNING)
#define USB_LOG_WRN(fmt, ...) usb_dbg_log_line("W", 33, fmt, ##__VA_ARGS__)
#else
#define USB_LOG_WRN(...)
#endif

#if (USB_DBG_LEVEL >= USB_DBG_ERROR)
#define USB_LOG_ERR(fmt, ...) usb_dbg_log_line("E", 31, fmt, ##__VA_ARGS__)
#else
#define USB_LOG_ERR(...)
#endif

void usb_assert(const char *filename, int linenum);
#define USB_ASSERT(f)                       \
    do {                                    \
        if (!(f))                           \
            usb_assert(__FILE__, __LINE__); \
    } while (0)

#endif
