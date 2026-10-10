/*
 * usb_glue_phytium_common.h -- FreeRTOS OSAL glue declarations
 *
 * Shared by the XHCI and PUSB3 ports; compiled in both SOURCE_CODE and
 * STATIC_LIB modes.
 */

#ifndef _CUSB_GLUE_PHYTIUM_COMMON_H_
#define _CUSB_GLUE_PHYTIUM_COMMON_H_

#include <stdint.h>
#include <stddef.h>

#include "usb_osal.h"

/* ---- struct mtx (storage for the recursive-mutex handle) ---- */

struct mtx {
    void *handle;
};

/* ---- Callout timer (implementation in usb_glue_phytium_common.c) ---- */

#define hz 1000

typedef struct {
    void *mtx;
    void *timer;
    void (*fn)(void *);
    void *arg;
} cusb_callout_t;

void cusb_callout_init_mtx(cusb_callout_t *c, void *mtx);
void cusb_callout_reset(cusb_callout_t *c, uint32_t ticks,
                        void (*fn)(void *), void *arg);
void cusb_callout_stop(cusb_callout_t *c);
void cusb_callout_drain(cusb_callout_t *c);
void cusb_callout_drain_resource_init(void);

/* ---- Recursive mutex (implementation in usb_glue_phytium_common.c) ---- */

void *cusb_recursive_mutex_create(void);
void cusb_recursive_mutex_delete(void *m);
void cusb_recursive_mutex_take(void *m);
void cusb_recursive_mutex_give(void *m);
int cusb_recursive_mutex_is_owned(void *m);

#define USB_MTX_LOCK(_m)                                 \
    do {                                                 \
        struct mtx *_pm = (struct mtx *)(uintptr_t)(_m); \
        cusb_recursive_mutex_take(_pm->handle);          \
    } while (0)

#define USB_MTX_UNLOCK(_m)                               \
    do {                                                 \
        struct mtx *_pm = (struct mtx *)(uintptr_t)(_m); \
        cusb_recursive_mutex_give(_pm->handle);          \
    } while (0)

/* ---- hub message queue space query ---- */

int cusb_glue_mq_has_space(void *mq);

#endif /* _CUSB_GLUE_PHYTIUM_COMMON_H_ */
