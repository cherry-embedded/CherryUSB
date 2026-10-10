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

/* OSAL glue (FreeRTOS backend) */
#include "usb_glue_phytium_common.h"
#include <FreeRTOS.h>
#include "semphr.h"
#include "timers.h"

/************************** Constant Definitions *****************************/
#define USB_MEMP_TOTAL_SIZE SZ_1M

/**************************** Type Definitions *******************************/
#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PLATFROM) || defined(CONFIG_CHERRY_USB_PORT_XHCI_PCIE)
void usb_hc_setup_xhci_interrupt(u32 id);
void usb_hc_port_poll_stop(u32 id);
#endif
#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PCIE)
unsigned long usb_hc_setup_xhci_pcie(struct usbh_bus *bus);
#endif
/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[USB_MEMP_TOTAL_SIZE] __attribute__((aligned(8))) = { 0 };
static u32 memp_ref_cnt = 0;

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

extern int vApplicationInIrq(void);
int xPortIsInsideInterrupt(void)
{
    return vApplicationInIrq();
}

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    if (memp_ref_cnt == 0) {
        usb_sys_mem_init();                 /* create memory pool before first bus init */
        cusb_callout_drain_resource_init(); /* init global drain lock/semaphore */
    }

    memp_ref_cnt++; /* one more bus is using the memory pool */

    if (bus->hcd.reg_base != 0) {
#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PLATFROM)
        /* platform XHCI controller */
        usb_hc_setup_xhci_interrupt(bus->busid);
#else
        USB_LOG_ERR("Platform XHCI not supported !!!\n");
        USB_ASSERT(0);
#endif
    } else {
#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PCIE)
        /* pcie XHCI controller */
        bus->hcd.reg_base = usb_hc_setup_xhci_pcie(bus);
        bus->busid = 0U; /* only support one pcie lane */
        usb_hc_setup_xhci_interrupt(bus->busid);
#else
        USB_LOG_ERR("Invalid register base !!!\n");
        USB_ASSERT(0);
#endif
    }
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PLATFROM) || defined(CONFIG_CHERRY_USB_PORT_XHCI_PCIE)
    /* stop the port poll timer before releasing resources */
    usb_hc_port_poll_stop(bus->busid);
#endif

    memp_ref_cnt--; /* one more bus is leaving */

    if (memp_ref_cnt == 0) {
        usb_sys_mem_deinit(); /* release memory pool after the last bus left */
    }
}

/* dcache overrides (override library weak defaults, required by both
 * platform and PCIe XHCI) */
void xhci_dcache_flush(void *addr, size_t size)
{
    FCacheDCacheFlushRange((uintptr_t)addr, size);
}

void xhci_dcache_invalidate(void *addr, size_t size)
{
    FCacheDCacheInvalidateRange((uintptr_t)addr, size);
}
