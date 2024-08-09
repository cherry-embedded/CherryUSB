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

#include "fassert.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"
#include "fcache.h"
#include "fmemory_pool.h"

#include "usbh_core.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static void usb_hc_xhci_interrupt_handler(s32 vector, void *param)
{
    extern void USBH_IRQHandler(uint8_t busid);
    USBH_IRQHandler((uint8_t)(uintptr_t)param);
}

void usb_hc_setup_xhci_interrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = (id == FUSB3_ID_0) ? FUSB3_0_IRQ_NUM : FUSB3_1_IRQ_NUM;
    u32 irq_priority = 13U;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     usb_hc_xhci_interrupt_handler,
                     (void *)(uintptr_t)id,
                     NULL);

    /* enable irq */
    InterruptUmask(irq_num);
}

unsigned long usb_hc_get_register_base(uint32_t id)
{
    if (FUSB3_ID_0 == id)
        return FUSB3_0_BASE_ADDR + FUSB3_XHCI_OFFSET;
    else
        return FUSB3_1_BASE_ADDR + FUSB3_XHCI_OFFSET;
}