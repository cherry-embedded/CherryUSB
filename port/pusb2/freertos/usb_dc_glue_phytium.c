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

#include "usbd_core.h"

/************************** Function Declarations ***************************/
void USBD_IRQHandler(uint8_t busid);

extern void usb_sys_mem_init(void);
extern void usb_sys_mem_deinit(void);
extern void usb_sys_mem_inc_ref(void);
extern u32 usb_sys_mem_dec_ref(void);
extern u8 usb_otg_is_active(void);

/************************** Function Implementations *************************/

static void usb_dc_pusb2_interrupt_handler(s32 vector, void *param)
{
    (void)param;
    FASSERT(vector == FUSB2_0_VHUB_IRQ_NUM);
    USBD_IRQHandler(CONFIG_USB_PUSB2_BUS_ID);
}

static void usb_dc_setup_pusb2_interrupt(u32 id)
{
    u32 cpu_id;
    FASSERT(id == CONFIG_USB_PUSB2_BUS_ID);
    u32 irq_priority = 13U;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(FUSB2_0_VHUB_IRQ_NUM, cpu_id);

    InterruptSetPriority(FUSB2_0_VHUB_IRQ_NUM, irq_priority);

    /* register intr callback */
    InterruptInstall(FUSB2_0_VHUB_IRQ_NUM,
                     usb_dc_pusb2_interrupt_handler,
                     NULL,
                     NULL);

    /* enable irq */
    InterruptUmask(FUSB2_0_VHUB_IRQ_NUM);
}

static void usb_dc_revoke_pusb2_interrupt(u32 id)
{
    FASSERT(id == CONFIG_USB_PUSB2_BUS_ID);

    /* disable irq */
    InterruptMask(FUSB2_0_VHUB_IRQ_NUM);
}

unsigned long usb_dc_get_register_base(uint32_t id)
{
    FASSERT(id == CONFIG_USB_PUSB2_BUS_ID);
    return FUSB2_0_VHUB_BASE_ADDR;
}

/* implement cherryusb weak functions */
void usb_dc_low_level_init(void)
{
    /* Initialize shared memory pool if not already done */
    usb_sys_mem_init();
    usb_sys_mem_inc_ref();

    /* In OTG mode, the OTG interrupt handler is already installed by usb_otg_init.
     * Skip installing DC-specific handler to avoid conflicts. */
    if (usb_otg_is_active()) {
        return;
    }

    usb_dc_setup_pusb2_interrupt(CONFIG_USB_PUSB2_BUS_ID);
}

void usb_dc_low_level_deinit(void)
{
    /* Release memory pool if last user */
    if (usb_sys_mem_dec_ref()) {
        usb_sys_mem_deinit();
    }

    /* In OTG mode, do not revoke interrupt - OTG handles it */
    if (usb_otg_is_active()) {
        return;
    }

    usb_dc_revoke_pusb2_interrupt(CONFIG_USB_PUSB2_BUS_ID);
}