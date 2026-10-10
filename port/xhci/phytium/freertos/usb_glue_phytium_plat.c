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

#include "sdkconfig.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "fassert.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"
#include "fcache.h"
#include "fmemory_pool.h"

#include "usbh_core.h"

/************************** Constant Definitions *****************************/
#define USB_XHCI_IRQ_TASK_STACK 8192U /* in bytes, same as usbproc */

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static SemaphoreHandle_t s_xhci_irq_sem[FUSB3_NUM] = { NULL };
static TimerHandle_t s_port_poll_timer[FUSB3_NUM] = { NULL };

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
static uint32_t usb_hc_get_xhci_irq_num(u32 id);

/* Port poll timer callback: runs in the timer daemon task context, forwards pending port change bits */
static void cusb_port_poll_timer_cb(TimerHandle_t handle)
{
    extern void cusb_xhci_port_poll(uint8_t busid);

    cusb_xhci_port_poll((uint8_t)(uintptr_t)pvTimerGetTimerID(handle));
}

/******************************* Functions ***********************************/

/* Deferred interrupt thread: hard ISR wakes this thread; interrupt
 * processing runs here in thread context where lock macros work. */
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

static uint32_t usb_hc_get_xhci_irq_num(u32 id)
{
#if defined(CONFIG_TARGET_PE2204) || defined(CONFIG_TARGET_PE2202)
    uint32_t irq_num[FUSB3_NUM] = {
        FUSB3_0_IRQ_NUM,
        FUSB3_1_IRQ_NUM
    };
    FASSERT(id < FUSB3_NUM);
#endif

    return irq_num[id];
}

void usb_hc_setup_xhci_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_priority = USB_XHCI_GIC_IRQ_PRIO;
    u32 irq_num = usb_hc_get_xhci_irq_num(id);
    char task_name[configMAX_TASK_NAME_LEN];

    if (s_xhci_irq_sem[id] == NULL) {
        s_xhci_irq_sem[id] = xSemaphoreCreateBinary();
        FASSERT(s_xhci_irq_sem[id] != NULL);
        snprintf(task_name, sizeof(task_name), "xhci_irq%lu", (unsigned long)id);
        FASSERT(usb_osal_thread_create(task_name, USB_XHCI_IRQ_TASK_STACK,
                                       CONFIG_USB_XHCI_IRQ_PRIO, usb_hc_xhci_irq_task,
                                       (void *)(uintptr_t)id) != NULL);
    }

    /* Port poll timer: deterministic fallback scanning PORTSC every
     * 100 ms; deinit stops it but never deletes it. */
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

void usb_hc_disable_interrupt(u32 id)
{
    u32 irq_num = usb_hc_get_xhci_irq_num(id);

    InterruptMask(irq_num);
}

void usb_hc_enable_interrupt(u32 id)
{
    u32 irq_num = usb_hc_get_xhci_irq_num(id);

    InterruptUmask(irq_num);
}

unsigned long usb_hc_get_register_base(uint32_t id)
{
#if defined(CONFIG_TARGET_PE2204) || defined(CONFIG_TARGET_PE2202)
    unsigned long base_reg[FUSB3_NUM] = {
        FUSB3_0_BASE_ADDR + FUSB3_XHCI_OFFSET,
        FUSB3_1_BASE_ADDR + FUSB3_XHCI_OFFSET
    };
    FASSERT(id < FUSB3_NUM);
#endif

    return base_reg[id];
}
