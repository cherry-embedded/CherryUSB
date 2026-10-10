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

#include "fassert.h"
#include "fparameters.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"
#include "fcache.h"
#include "fsleep.h"
#include "fmemory_pool.h"

#include "usbd_core.h"

/************************** Constant Definitions *****************************/
#define USB_MEMP_TOTAL_SIZE SZ_1M

/**************************** Type Definitions *******************************/
void USBD_IRQHandler(uint8_t busid);
void USBOTG_IRQHandler(uint8_t busid);

/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[USB_MEMP_TOTAL_SIZE] __attribute__((aligned(8))) = { 0 };

static void usb_sys_mem_init(void)
{
    if (FT_COMPONENT_IS_READY != memp.is_ready) {
        FASSERT(FT_SUCCESS == FMempInit(&memp, &memp_buf[0], &memp_buf[0] + USB_MEMP_TOTAL_SIZE));
    }
}

static void usb_sys_mem_deinit(void)
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
    FASSERT(id < FUSB3_OTG_3X2_GEN2_NUM);
    return usb_get_register_base(id) + FUSB_OTG_REG_OFF;
}

unsigned long usb_dc_get_register_base(uint32_t id)
{
    FASSERT(id < FUSB3_OTG_3X2_GEN2_NUM);
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

uint32_t usb_dc_get_otg_irq_num(uint32_t id)
{
    uint32_t irq_num[FUSB3_OTG_3X2_GEN2_NUM] = {
        FUSB_0_OTG_IRQ_NUM,
    };

    FASSERT(id < FUSB3_OTG_3X2_GEN2_NUM);
    return irq_num[id];
}

static void usb_pusb3_dc_interrupt_handler(s32 vector, void *param)
{
    uintptr_t bus_id = (uintptr_t)param;
    USBD_IRQHandler((uint8_t)bus_id);
}

static void usb_pusb3_otg_interrupt_handler(s32 vector, void *param)
{
    uintptr_t bus_id = (uintptr_t)param;
    USBOTG_IRQHandler((uint8_t)bus_id);
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

static void usb_dc_setup_pusb3_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = usb_dc_get_dev_irq_num(id);
    u32 irq_priority = 0U;

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

static void usb_dc_revoke_pusb3_interrupt(u32 id)
{
    u32 irq_num = usb_dc_get_dev_irq_num(id);

    /* disable irq */
    InterruptMask(irq_num);

    irq_num = usb_dc_get_otg_irq_num(id);

    /* disable irq */
    InterruptMask(irq_num);
}

void usb_dc_low_level_init(void)
{
    usb_sys_mem_init();
    usb_dc_setup_pusb3_interrupt(0U);
}

void usb_dc_low_level_deinit(void)
{
    usb_dc_revoke_pusb3_interrupt(0U);
    usb_sys_mem_deinit();
}

void usb_pusb3_msleep(uint32_t delay)
{
    fsleep_millisec(delay);
}

__WEAK void USBOTG_IRQHandler(uint8_t busid)
{
}

__WEAK void USBD_IRQHandler(uint8_t busid)
{
}

__WEAK void USBH_IRQHandler(uint8_t busid)
{
}