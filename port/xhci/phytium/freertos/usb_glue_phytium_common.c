/*
 * usb_glue_phytium_common.c -- FreeRTOS OSAL bridge implementation
 *
 * Provides callout timers, recursive mutex and hub queue query,
 * shared by the XHCI and PUSB3 ports.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

#include "usbh_core.h"
#include "usb_glue_phytium_common.h"

/* ================================================================ */
/*  OSAL: callout timer (FreeRTOS backend)                           */
/* ================================================================ */

static void cusb_callout_timer_cb(TimerHandle_t handle)
{
    cusb_callout_t *c = (cusb_callout_t *)pvTimerGetTimerID(handle);

    USB_MTX_LOCK(c->mtx);
    c->fn(c->arg);
    USB_MTX_UNLOCK(c->mtx);
}

void cusb_callout_init_mtx(cusb_callout_t *c, void *mtx)
{
    c->mtx = mtx;
    c->timer = (void *)xTimerCreate("cb_to",
                                    pdMS_TO_TICKS(1), pdFALSE,
                                    c, (TimerCallbackFunction_t)cusb_callout_timer_cb);
}

/* Global drain sync: a lock + semaphore serializing drain operations,
 * created once in usb_hc_low_level_init. */
static void *s_callout_drain_mtx;
static usb_osal_sem_t s_callout_drain_sem;

void cusb_callout_drain_resource_init(void)
{
    if (!s_callout_drain_mtx) {
        s_callout_drain_mtx = cusb_recursive_mutex_create();
        s_callout_drain_sem = usb_osal_sem_create_counting(1);
    }
}

void cusb_callout_reset(cusb_callout_t *c, uint32_t ticks,
                        void (*fn)(void *), void *arg)
{
    TimerHandle_t t = (TimerHandle_t)c->timer;

    c->fn = fn;
    c->arg = arg;
    if (xPortIsInsideInterrupt()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xTimerChangePeriodFromISR(t, pdMS_TO_TICKS(ticks * 1000 / hz), &xHigherPriorityTaskWoken) != pdPASS)
            USB_LOG_ERR("callout_reset: change period from ISR fail\n");
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        /* xTimerChangePeriod (re)starts the timer by itself; no extra xTimerStart needed */
        if (xTimerChangePeriod(t, pdMS_TO_TICKS(ticks * 1000 / hz), portMAX_DELAY) != pdPASS)
            USB_LOG_ERR("callout_reset: change period fail\n");
    }
}

void cusb_callout_stop(cusb_callout_t *c)
{
    if (xPortIsInsideInterrupt()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerStopFromISR((TimerHandle_t)c->timer, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    } else {
        if (xTimerStop((TimerHandle_t)c->timer, portMAX_DELAY) != pdPASS)
            USB_LOG_ERR("callout_stop: stop fail\n");
    }
}

static void cusb_callout_drain_sync(void *arg, uint32_t param)
{
    (void)arg;
    (void)param;
    usb_osal_sem_give(s_callout_drain_sem);
}

/* Stop the timer and wait for in-flight callbacks to finish;
 * caller must not hold the callout's lock. */
void cusb_callout_drain(cusb_callout_t *c)
{
    if (xPortIsInsideInterrupt()) {
        /* Cannot wait in ISR context; degrade to stop (draining from ISR is also forbidden by the framework) */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerStopFromISR((TimerHandle_t)c->timer, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return;
    }
    if (!s_callout_drain_mtx)
        return;
    cusb_recursive_mutex_take(s_callout_drain_mtx);
    xTimerStop((TimerHandle_t)c->timer, portMAX_DELAY);
    usb_osal_sem_take(s_callout_drain_sem, 0); /* clear stale token */
    if (xTimerPendFunctionCall(cusb_callout_drain_sync, NULL, 0, portMAX_DELAY) != pdPASS) {
        USB_LOG_ERR("callout_drain: pend fail\n");
    } else {
        usb_osal_sem_take(s_callout_drain_sem, USB_OSAL_WAITING_FOREVER);
    }
    cusb_recursive_mutex_give(s_callout_drain_mtx);
}

/* ---- recursive mutex ---- */

struct cusb_rmtx {
    SemaphoreHandle_t sem;
    void *owner; /* TaskHandle_t of current holder, NULL if unlocked */
    int count;   /* recursion depth */
};

void *cusb_recursive_mutex_create(void)
{
    struct cusb_rmtx *m = usb_osal_malloc(sizeof(*m));
    if (!m)
        return NULL;
    m->sem = xSemaphoreCreateRecursiveMutex();
    if (!m->sem) {
        usb_osal_free(m);
        return NULL;
    }
    m->owner = NULL;
    m->count = 0;
    return m;
}

void cusb_recursive_mutex_delete(void *m)
{
    struct cusb_rmtx *r = (struct cusb_rmtx *)m;
    if (!r)
        return;
    vSemaphoreDelete(r->sem);
    usb_osal_free(r);
}

void cusb_recursive_mutex_take(void *m)
{
    struct cusb_rmtx *r = (struct cusb_rmtx *)m;
    if (!r) {
        USB_LOG_ERR("rmtx_take NULL\n");
        return;
    }
    xSemaphoreTakeRecursive(r->sem, portMAX_DELAY);
    if (r->count++ == 0)
        r->owner = (void *)xTaskGetCurrentTaskHandle();
}

void cusb_recursive_mutex_give(void *m)
{
    struct cusb_rmtx *r = (struct cusb_rmtx *)m;
    if (!r) {
        USB_LOG_ERR("rmtx_give NULL\n");
        return;
    }
    if (--r->count == 0)
        r->owner = NULL;
    xSemaphoreGiveRecursive(r->sem);
}

int cusb_recursive_mutex_is_owned(void *m)
{
    struct cusb_rmtx *r = (struct cusb_rmtx *)m;
    if (!r)
        return 0;
    return (r->owner == (void *)xTaskGetCurrentTaskHandle()) ? 1 : 0;
}

/* ---- hub message queue space query ---- */

int cusb_glue_mq_has_space(void *mq)
{
    return mq && (uxQueueSpacesAvailable((QueueHandle_t)mq) > 0);
}
