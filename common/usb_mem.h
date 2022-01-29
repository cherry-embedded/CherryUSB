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

//#include <stdint.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <malloc.h>

#define DCACHE_LINE_SIZE 32
#define DCACHE_LINEMASK (DCACHE_LINE_SIZE -1)

#ifdef CONFIG_USB_DCACHE_ENABLE
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".nocache_ram")))
#else
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
    size  = (size + DCACHE_LINEMASK) & ~DCACHE_LINEMASK;
    uint32_t no_cache_addr = (uint32_t)(uintptr_t)memalign(DCACHE_LINE_SIZE, size) & ~(1 << 30);
    return (void *)no_cache_addr;
}

static inline void usb_iofree(void *addr)
{
    uint32_t cache_addr = (uint32_t)(uintptr_t)addr | (1 << 30);
    free((void *)cache_addr);
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