/*
 * Copyright : (C) 2024 Phytium Information Technology, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * -----  ------     --------    --------------------------------------
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

#include "usbh_core.h"

/************************** Function Declarations ***************************/
extern void USBH_IRQHandler(uint8_t busid);
extern void usb_sys_mem_init(void);
extern void usb_sys_mem_deinit(void);
extern void usb_sys_mem_inc_ref(void);
extern u32 usb_sys_mem_dec_ref(void);
extern u8 usb_otg_is_active(void);

/************************** Variable Definitions *****************************/
static const u32 irq_nums[] = {
    FUSB2_0_VHUB_IRQ_NUM, FUSB2_1_IRQ_NUM, FUSB2_2_IRQ_NUM
};

/************************** Function Implementations *************************/

static void usb_hc_pusb2_interrupt_handler(s32 vector, void *param)
{
    (void)param;
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
    /* Initialize shared memory pool if not already done */
    usb_sys_mem_init();
    usb_sys_mem_inc_ref();

    /* In OTG mode, the OTG interrupt handler is already installed by usb_otg_init.
     * Skip installing HC-specific handler to avoid conflicts. */
    if (usb_otg_is_active()) {
        return;
    }
    usb_hc_setup_pusb2_interrupt(bus->busid);
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    /* Release memory pool if last user */
    if (usb_sys_mem_dec_ref()) {
        usb_sys_mem_deinit();
    }

    /* In OTG mode, do not revoke interrupt - OTG handles it */
    if (usb_otg_is_active()) {
        return;
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
    return 0;
}