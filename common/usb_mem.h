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

#define usb_malloc(size) malloc(size)
#define usb_free(ptr)    free(ptr)

#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

#ifndef USB_NOCACHE_RAM_SECTION
#define USB_NOCACHE_RAM_SECTION
#endif
#define USB_MEM_ALIGNX __attribute__((aligned(CONFIG_USB_ALIGN_SIZE)))

#ifdef CONFIG_USB_DCACHE_ENABLE
static inline void *usb_iomalloc(size_t size)
{
    void *ptr;
    void *align_ptr;
    int uintptr_size;
    size_t align_size;
    uint32_t align = CONFIG_USB_ALIGN_SIZE;

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
        if (((unsigned long)ptr & (align - 1)) == 0) {
            align_ptr = (void *)((unsigned long)ptr + align);
        } else {
            align_ptr = (void *)(((unsigned long)ptr + (align - 1)) & ~(align - 1));
        }

        /* set the pointer before alignment pointer to the real pointer */
        *((unsigned long *)((unsigned long)align_ptr - sizeof(void *))) = (unsigned long)ptr;

        ptr = align_ptr;
    }

    return ptr;
}

static inline void usb_iofree(void *ptr)
{
    void *real_ptr;

    real_ptr = (void *)*(unsigned long *)((unsigned long)ptr - sizeof(void *));
    usb_free(real_ptr);
}

void usb_dcache_clean(uintptr_t addr, uint32_t len);
void usb_dcache_invalidate(uintptr_t addr, uint32_t len);
void usb_dcache_clean_invalidate(uintptr_t addr, uint32_t len);
#else
#define usb_iomalloc(size) usb_malloc(size)
#define usb_iofree(ptr)    usb_free(ptr)

#define usb_dcache_clean(addr, len)
#define usb_dcache_invalidate(addr, len)
#define usb_dcache_clean_invalidate(addr, len)

#endif

#endif
