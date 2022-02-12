/**
 * @file usb_mem.h
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
#ifndef _USB_MEM_H
#define _USB_MEM_H

#define DCACHE_LINE_SIZE 32
#define DCACHE_LINEMASK  (DCACHE_LINE_SIZE - 1)

#ifdef CONFIG_USB_DCACHE_ENABLE
#ifdef CONFIG_USB_NOCACHE_RAM
#define USB_MEM_ALIGN32
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".nocache_ram")))
#else
#define USB_MEM_ALIGN32 __attribute__((aligned(DCACHE_LINE_SIZE)))
#define USB_NOCACHE_RAM_SECTION
#endif
#else
#define USB_MEM_ALIGN32
#define USB_NOCACHE_RAM_SECTION
#endif

static inline void *usb_malloc(size_t size)
{
    return malloc(size);
}

static inline void usb_free(void *ptr)
{
    free(ptr);
}

#ifdef CONFIG_USB_DCACHE_ENABLE
static inline void *usb_iomalloc(size_t size)
{
    size = (size + DCACHE_LINEMASK) & ~DCACHE_LINEMASK;
    return malloc(size);
}

static inline void usb_iofree(void *addr)
{
    free(addr);
}
#else
static inline void *usb_iomalloc(size_t size)
{
    return malloc(size);
}

static inline void usb_iofree(void *ptr)
{
    free(ptr);
}
#endif

#endif