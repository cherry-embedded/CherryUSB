/*
 * Copyright : (C) 2024 Phytium Information Technology, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2024/6/26 first commit
 */
/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include "usb_config.h"
#include "usb_log.h"
#include "usb_osal.h"
#include "fassert.h"
#include "fparameters.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"
#include "fcache.h"
#include "fmemory_pool.h"
#include "fsleep.h"

/************************** Constant Definitions *****************************/
#define USB_MEMP_TOTAL_SIZE SZ_1M

/**************************** Type Definitions *******************************/
void USBD_IRQHandler(uint8_t busid);
void USBOTG_IRQHandler(uint8_t busid);

/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[USB_MEMP_TOTAL_SIZE] __attribute__((section(".noncacheable"), aligned(8))) = { 0 };

/* XHCI deferred interrupt thread + port poll timer: hard ISR only
 * wakes the IRQ task, so lock/semaphore calls run in thread context */
#define USB_XHCI_IRQ_TASK_STACK 8192U
static SemaphoreHandle_t s_xhci_irq_sem[FUSB_TOT_NUM] = { NULL };
static TimerHandle_t s_port_poll_timer[FUSB_TOT_NUM] = { NULL };

/* OTG mode flag: set by usb_otg_init so glue layers can skip duplicate IRQ setup */
static u8 g_otg_mode_active = 0;

u8 usb_otg_is_active(void)
{
    return g_otg_mode_active;
}

void usb_otg_set_active(u8 active)
{
    g_otg_mode_active = active;
}

void usb_sys_mem_init(void)
{
    if (FT_COMPONENT_IS_READY != memp.is_ready) {
        FASSERT(FT_SUCCESS == FMempInit(&memp, &memp_buf[0], &memp_buf[0] + USB_MEMP_TOTAL_SIZE));
    }
}

void usb_sys_mem_deinit(void)
{
    if (FT_COMPONENT_IS_READY == memp.is_ready) {
        FMempDeinit(&memp);
    }
}

void *usb_sys_malloc_align(size_t align, size_t size)
{
    void *result = FMempMallocAlign(&memp, size, align);

    if (result) {
        memset(result, 0U, size);
    } else {
        USB_LOG_ERR("malloc_align: fail size=%u align=%u\n", (unsigned)size, (unsigned)align);
    }

    return result;
}

void *usb_sys_mem_malloc(size_t size)
{
    return usb_sys_malloc_align(sizeof(void *), size);
}

void usb_sys_mem_free(void *ptr)
{
    if (NULL != ptr) {
        FMempFree(&memp, ptr);
    }
}

void usb_assert(const char *filename, int linenum)
{
    FAssert(filename, linenum, 0xff);
}

unsigned long usb_get_register_base(uint32_t id)
{
    unsigned long base_reg[FUSB_TOT_NUM] = {
        FUSB_0_BASE_ADDR,
        FUSB_1_BASE_ADDR,
        FUSB_2_BASE_ADDR,
        FUSB_3_BASE_ADDR,
        FUSB_4_BASE_ADDR,
        FUSB_5_BASE_ADDR,
        FUSB_6_BASE_ADDR,
    };

    FASSERT(id < FUSB_TOT_NUM);
    return base_reg[id];
}

unsigned long usb_otg_get_register_base(uint32_t id)
{
    return usb_get_register_base(id) + FUSB_OTG_REG_OFF;
}

unsigned long usb_dc_get_register_base(uint32_t id)
{
    return usb_get_register_base(id) + FUSB_OTG_DC_REG_OFF;
}

unsigned long usb_hc_get_register_base(uint32_t id)
{
    return usb_get_register_base(id) + FUSB_OTG_HC_REG_OFFSET;
}

uint32_t usb_dc_get_dev_irq_num(uint32_t id)
{
    uint32_t irq_num[FUSB3_OTG_3X2_GEN2_NUM] = {
        FUSB_0_HC_DC_IRQ_NUM,
    };

    FASSERT(id < FUSB3_OTG_3X2_GEN2_NUM);
    return irq_num[id];
}

uint32_t usb_hc_get_irq_num(uint32_t id)
{
    uint32_t irq_num[FUSB_TOT_NUM] = {
        FUSB_0_HC_DC_IRQ_NUM,
        FUSB_1_IRQ_NUM,
        FUSB_2_IRQ_NUM,
        FUSB_3_IRQ_NUM,
        FUSB_4_IRQ_NUM,
        FUSB_5_IRQ_NUM,
        FUSB_6_IRQ_NUM
    };

    FASSERT(id < FUSB_TOT_NUM);
    return irq_num[id];
}

uint32_t usb_dc_get_otg_irq_num(uint32_t id)
{
    uint32_t irq_num[FUSB_TOT_NUM] = {
        FUSB_0_OTG_IRQ_NUM,
    };

    FASSERT(id < FUSB_TOT_NUM);
    return irq_num[id];
}

static void usb_pusb3_dc_interrupt_handler(s32 vector, void *param)
{
    (void)vector;
    USBD_IRQHandler((uint8_t)(uintptr_t)param);
}

static void usb_pusb3_otg_interrupt_handler(s32 vector, void *param)
{
    (void)vector;
    USBOTG_IRQHandler((uint8_t)(uintptr_t)param);
}

void usb_dc_disable_interrupt(u32 id)
{
    u32 irq_num = usb_dc_get_dev_irq_num(id);

    InterruptMask(irq_num);
}

void usb_dc_enable_interrupt(u32 id)
{
    u32 irq_num = usb_dc_get_dev_irq_num(id);

    InterruptUmask(irq_num);
}

void usb_dc_setup_pusb3_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = usb_dc_get_dev_irq_num(id);
    u32 irq_priority = USB_XHCI_GIC_IRQ_PRIO;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     usb_pusb3_dc_interrupt_handler,
                     (void *)(uintptr_t)id,
                     NULL);

    irq_num = usb_dc_get_otg_irq_num(id);

    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     usb_pusb3_otg_interrupt_handler,
                     (void *)(uintptr_t)id,
                     NULL);
}

void usb_dc_setup_pusb3_dev_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = usb_dc_get_dev_irq_num(id);
    u32 irq_priority = USB_XHCI_GIC_IRQ_PRIO;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);
    InterruptSetPriority(irq_num, irq_priority);

    InterruptInstall(irq_num,
                     usb_pusb3_dc_interrupt_handler,
                     (void *)(uintptr_t)id,
                     NULL);
}

void usb_dc_revoke_pusb3_interrupt(u32 id)
{
    u32 irq_num = usb_dc_get_dev_irq_num(id);

    /* disable irq */
    InterruptMask(irq_num);
}

void usb_hc_disable_interrupt(u32 id)
{
    u32 irq_num = usb_hc_get_irq_num(id);

    InterruptMask(irq_num);
}

void usb_hc_enable_interrupt(u32 id)
{
    u32 irq_num = usb_hc_get_irq_num(id);

    InterruptUmask(irq_num);
}

/* Port poll timer callback: forwards pending PORTSC change bits */
static void cusb_port_poll_timer_cb(TimerHandle_t handle)
{
    extern void cusb_xhci_port_poll(uint8_t busid);

    cusb_xhci_port_poll((uint8_t)(uintptr_t)pvTimerGetTimerID(handle));
}

/* Deferred IRQ task: runs USBH_IRQHandler in thread context */
static void usb_hc_xhci_irq_task(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    extern void USBH_IRQHandler(uint8_t busid);

    for (;;) {
        if (s_xhci_irq_sem[id] == NULL) {
            USB_LOG_ERR("xhci_irq sem NULL id=%lu\n", (unsigned long)id);
            while (1)
                ;
        }
        xSemaphoreTake(s_xhci_irq_sem[id], portMAX_DELAY);
        USBH_IRQHandler((uint8_t)id);
    }
}

static void usb_hc_xhci_interrupt_handler(s32 vector, void *param)
{
    uint32_t id = (uint32_t)(uintptr_t)param;
    BaseType_t higher_prio_woken = pdFALSE;
    extern void cusb_xhci_irq_ack(uint8_t busid);

    (void)vector;

    /* Ack at controller level only (IMAN.IP); port changes arrive as
     * PSC TRBs on the event ring and are handled in the IRQ thread. */
    cusb_xhci_irq_ack((uint8_t)id);

    if (s_xhci_irq_sem[id] != NULL) {
        xSemaphoreGiveFromISR(s_xhci_irq_sem[id], &higher_prio_woken);
        portYIELD_FROM_ISR(higher_prio_woken);
    }
}

void usb_hc_setup_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = usb_hc_get_irq_num(id);
    u32 irq_priority = USB_XHCI_GIC_IRQ_PRIO;
    char task_name[configMAX_TASK_NAME_LEN];

    if (s_xhci_irq_sem[id] == NULL) {
        s_xhci_irq_sem[id] = xSemaphoreCreateBinary();
        FASSERT(s_xhci_irq_sem[id] != NULL);
        snprintf(task_name, sizeof(task_name), "xhci_irq%lu", (unsigned long)id);
        FASSERT(usb_osal_thread_create(task_name, USB_XHCI_IRQ_TASK_STACK,
                                       CONFIG_USB_XHCI_IRQ_PRIO, usb_hc_xhci_irq_task,
                                       (void *)(uintptr_t)id) != NULL);
    }

    /* Port poll timer: deterministic event fallback; deinit stops it
     * but never deletes it. */
    if (s_port_poll_timer[id] == NULL) {
        s_port_poll_timer[id] = xTimerCreate("usb_poll",
                                             pdMS_TO_TICKS(100), pdTRUE,
                                             (void *)(uintptr_t)id, cusb_port_poll_timer_cb);
        FASSERT(s_port_poll_timer[id] != NULL);
    }
    xTimerStart(s_port_poll_timer[id], portMAX_DELAY);

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     usb_hc_xhci_interrupt_handler,
                     (void *)(uintptr_t)id,
                     NULL);
}

void usb_hc_port_poll_stop(u32 id)
{
    /* stop polling so it cannot wake the hub after detach */
    if (s_port_poll_timer[id] != NULL) {
        xTimerStop(s_port_poll_timer[id], portMAX_DELAY);
    }
}

void usb_hc_revoke_interrupt(u32 id)
{
    u32 irq_num = usb_hc_get_irq_num(id);

    /* disable irq */
    InterruptMask(irq_num);

    irq_num = usb_dc_get_otg_irq_num(id);

    /* disable irq */
    InterruptMask(irq_num);
}

extern int vApplicationInIrq(void);
int xPortIsInsideInterrupt(void)
{
    return vApplicationInIrq();
}

void usb_pusb3_msleep(uint32_t delay)
{
    fsleep_millisec(delay);
}

#ifdef CONFIG_USB_OTG_ENABLE

extern void USBOTG_IRQHandler(uint8_t busid);

static void usb_pusb3_otg_irq_handler(s32 vector, void *param)
{
    (void)vector;
    USBOTG_IRQHandler((uint8_t)(uintptr_t)param);
}

int usb_otg_init(uint8_t busid)
{
    u32 cpu_id;
    u32 irq_num = FUSB_0_OTG_IRQ_NUM;

    FASSERT(busid == FUSB_ID_0);
    usb_otg_set_active(1);

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);
    InterruptSetPriority(irq_num, USB_XHCI_GIC_IRQ_PRIO);
    InterruptInstall(irq_num, usb_pusb3_otg_irq_handler, (void *)(uintptr_t)busid, NULL);
    InterruptUmask(irq_num);

    return 0;
}

int usb_otg_deinit(uint8_t busid)
{
    FASSERT(busid == FUSB_ID_0);
    InterruptMask(FUSB_0_OTG_IRQ_NUM);
    usb_otg_set_active(0);
    return 0;
}

/* PUSB3 does not support AUTO mode. Provide stubs so common OTG
 * code (status command, stop) can call these unconditionally. */
#define USBOTG_MODE_HOST   0
#define USBOTG_MODE_DEVICE 1

int usb_otg_auto_poll_start(uint8_t busid, int default_role)
{
    (void)busid;
    (void)default_role;
    printf("OTG Auto mode is not supported on PUSB3, use manual mode\n");
    return -1;
}

void usb_otg_auto_poll_stop(void)
{
}

uint8_t usb_otg_auto_get_role(void)
{
    return USBOTG_MODE_HOST;
}

#endif /* CONFIG_USB_OTG_ENABLE */

/* Strong dcache definitions override the weak empty stubs in
 * fpusb3/core.c, required by the device framework DMA map pre/post sync. */
void xhci_dcache_flush(void *addr, size_t size)
{
    FCacheDCacheFlushRange((uintptr_t)addr, size);
}

void xhci_dcache_invalidate(void *addr, size_t size)
{
    FCacheDCacheInvalidateRange((uintptr_t)addr, size);
}