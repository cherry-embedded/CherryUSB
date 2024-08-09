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

#include "usbd_core.h"

/************************** Constant Definitions *****************************/
#define USB_MEMP_TOTAL_SIZE     SZ_1M

/**************************** Type Definitions *******************************/
void USBD_IRQHandler(uint8_t busid);

/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[USB_MEMP_TOTAL_SIZE] __attribute__((aligned(8))) = {0};

static void usb_sys_mem_init(void)
{
    if (FT_COMPONENT_IS_READY != memp.is_ready)
    {
        USB_ASSERT(FT_SUCCESS == FMempInit(&memp, &memp_buf[0], &memp_buf[0] + USB_MEMP_TOTAL_SIZE));
    }
}

static void usb_sys_mem_deinit(void)
{
    if (FT_COMPONENT_IS_READY == memp.is_ready)
    {
        FMempRemove(&memp);
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

static void usb_dc_pusb2_interrupt_handler(s32 vector, void *param)
{
    USBD_IRQHandler(CONFIG_USB_PUSB2_BUS_ID);
}

static void usb_dc_setup_pusb2_interrupt(u32 id)
{
    u32 cpu_id;
    USB_ASSERT(id == FUSB2_ID_VHUB_0);
    u32 irq_num = FUSB2_0_VHUB_IRQ_NUM;
    u32 irq_priority = 0U;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     usb_dc_pusb2_interrupt_handler,
                     NULL,
                     NULL);

    /* enable irq */
    InterruptUmask(irq_num);
}

static void usb_dc_revoke_pusb2_interrupt(u32 id)
{
    USB_ASSERT(id == FUSB2_ID_VHUB_0);
    u32 irq_num = FUSB2_0_VHUB_IRQ_NUM;

    /* disable irq */
    InterruptMask(irq_num);
}

unsigned long usb_dc_get_register_base(uint32_t id)
{
    USB_ASSERT(id == FUSB2_ID_VHUB_0);
    return FUSB2_0_VHUB_BASE_ADDR;
}

/* implement cherryusb weak functions */
void usb_dc_low_level_init()
{
    usb_sys_mem_init();
    usb_dc_setup_pusb2_interrupt(CONFIG_USB_PUSB2_BUS_ID);
}

void usb_dc_low_level_deinit(void)
{
    usb_sys_mem_deinit();
    usb_dc_revoke_pusb2_interrupt(CONFIG_USB_PUSB2_BUS_ID);
}

size_t usb_osal_enter_critical_section(void) 
{
    return 0;
}

void usb_osal_leave_critical_section(size_t flag) 
{
    
}