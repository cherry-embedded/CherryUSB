/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_MEM_H
#define USB_MEM_H

#define USB_MEM_ALIGNX __attribute__((aligned(CONFIG_USB_ALIGN_SIZE)))

#if (CONFIG_USB_ALIGN_SIZE > 4)
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
#else
#define usb_iomalloc(size) usb_malloc(size)
#define usb_iofree(ptr)    usb_free(ptr)
#endif

#endif /* USB_MEM_H */
