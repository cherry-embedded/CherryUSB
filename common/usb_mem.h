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

#ifndef CONFIG_DCACHE_LINE_SIZE
#define CONFIG_DCACHE_LINE_SIZE 32
#endif

#ifdef CONFIG_USB_DCACHE_ENABLE
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".nocache_ram")))
#define USB_MEM_ALIGN32         __attribute__((aligned(CONFIG_DCACHE_LINE_SIZE)))
#else
#define USB_NOCACHE_RAM_SECTION
#define USB_MEM_ALIGN32
#endif

#define usb_malloc(size) malloc(size)
#define usb_free(ptr) free(ptr)

#ifdef CONFIG_USB_DCACHE_ENABLE
static inline void *usb_iomalloc(size_t size)
{
    void *ptr;
    void *align_ptr;
    int uintptr_size;
    size_t align_size;
    uint32_t align = CONFIG_DCACHE_LINE_SIZE;

    /* sizeof pointer */
    uintptr_size = sizeof(void *);
    uintptr_size -= 1;

    /* align the alignment size to uintptr size byte */
    align = ((align + uintptr_size) & ~uintptr_size);

    /* get total aligned size */
    align_size = ((size + uintptr_size) & ~uintptr_size) + align;
    /* allocate memory block from heap */
    ptr = usb_malloc(align_size);
    if (ptr != NULL) {
        /* the allocated memory block is aligned */
        if (((uint32_t)ptr & (align - 1)) == 0) {
            align_ptr = (void *)((uint32_t)ptr + align);
        } else {
            align_ptr = (void *)(((uint32_t)ptr + (align - 1)) & ~(align - 1));
        }

        /* set the pointer before alignment pointer to the real pointer */
        *((uint32_t *)((uint32_t)align_ptr - sizeof(void *))) = (uint32_t)ptr;

        ptr = align_ptr;
    }

    return ptr;
}

static inline void usb_iofree(void *ptr)
{
    void *real_ptr;

    real_ptr = (void *)*(uint32_t *)((uint32_t)ptr - sizeof(void *));
    usb_free(real_ptr);
}
#else
#define usb_iomalloc(size) usb_malloc(size)
#define usb_iofree(ptr) usb_free(ptr)
#endif

#endif
