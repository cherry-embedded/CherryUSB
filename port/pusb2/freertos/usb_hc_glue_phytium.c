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
#include "fmemory_pool.h"

#include "usbh_core.h"

/************************** Constant Definitions *****************************/
#define USB_MEMP_TOTAL_SIZE     SZ_1M

/**************************** Type Definitions *******************************/
void USBH_IRQHandler(uint8_t busid);

/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[USB_MEMP_TOTAL_SIZE] __attribute__((aligned(8))) = {0};
static u32 memp_ref_cnt = 0;
static const u32 irq_nums[] = {
    FUSB2_0_VHUB_IRQ_NUM, FUSB2_1_IRQ_NUM, FUSB2_2_IRQ_NUM
};

void usb_sys_mem_init(void)
{
    if (FT_COMPONENT_IS_READY != memp.is_ready)
    {
        USB_ASSERT(FT_SUCCESS == FMempInit(&memp, &memp_buf[0], &memp_buf[0] + USB_MEMP_TOTAL_SIZE));
    }
}

void usb_sys_mem_deinit(void)
{
    if (FT_COMPONENT_IS_READY == memp.is_ready)
    {
        FMempDeinit(&memp);
    }
}

void *usb_sys_malloc_align(size_t align, size_t size)
{
    void *result = FMempMallocAlign(&memp, size, align);

    if (result)
    {
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
    if (NULL != ptr)
    {
        FMempFree(&memp, ptr);
    }
}

void usb_assert(const char *filename, int linenum)
{
    FAssert(filename, linenum, 0xff);
}

static void usb_hc_pusb2_interrupt_handler(s32 vector, void *param)
{
    if (vector == FUSB2_0_VHUB_IRQ_NUM) {
        USBH_IRQHandler(FUSB2_ID_VHUB_0);
    } else if (vector == FUSB2_1_IRQ_NUM) {
        USBH_IRQHandler(FUSB2_ID_1);
    } else if (vector == FUSB2_2_IRQ_NUM) {
        USBH_IRQHandler(FUSB2_ID_2);
    }
}

static void usb_hc_setup_pusb2_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = irq_nums[id];
    u32 irq_priority = 13U;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     usb_hc_pusb2_interrupt_handler,
                     NULL,
                     NULL);

    /* enable irq */
    InterruptUmask(irq_num);

    USB_LOG_DBG("Enable irq-%d\n", irq_num);
}

static void usb_hc_revoke_pusb2_interrupt(u32 id)
{
    u32 irq_num = irq_nums[id];

    /* disable irq */
    InterruptMask(irq_num);
}

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    if (memp_ref_cnt == 0) {
        usb_sys_mem_init(); /* create memory pool before first bus init */
    }

    memp_ref_cnt++; /* one more bus is using the memory pool */

    usb_hc_setup_pusb2_interrupt(bus->busid);
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    memp_ref_cnt--; /* one more bus is leaving */

    if (memp_ref_cnt == 0) {
        usb_sys_mem_deinit(); /* release memory pool after the last bus left */
    }

    usb_hc_revoke_pusb2_interrupt(bus->busid);
}

unsigned long usb_hc_get_register_base(uint32_t id)
{
    if (id == FUSB2_ID_VHUB_0) {
        return FUSB2_0_VHUB_BASE_ADDR;
    } else if (id == FUSB2_ID_1) {
        return FUSB2_1_BASE_ADDR;
    } else if (id == FUSB2_ID_2) {
        return FUSB2_2_BASE_ADDR;
    }
}

extern int vApplicationInIrq(void);
int xPortIsInsideInterrupt(void)
{
    return vApplicationInIrq();
}