/*
 * Copyright : (C) 2022 Phytium Information Technology, Inc.
 * All Rights Reserved.
 *
 * This program is OPEN SOURCE software: you can redistribute it and/or modify it
 * under the terms of the Phytium Public License as published by the Phytium Technology Co.,Ltd,
 * either version 1.0 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the Phytium Public License for more details.
 *
 *
 * FilePath: usb_host.c
 * Date: 2022-07-22 13:57:42
 * LastEditTime: 2022-07-22 13:57:43
 * Description:  This file is for the usb host functions.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/20  init commit
 */
/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "fassert.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"
#include "fcache.h"
#include "fmemory_pool.h"

#include "usbh_core.h"
/************************** Constant Definitions *****************************/
#define FUSB_MEMP_TOTAL_SIZE     SZ_1M

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[FUSB_MEMP_TOTAL_SIZE];

/***************** Macros (Inline Functions) Definitions *********************/
#define FUSB_DEBUG_TAG "USB-HC"
#define FUSB_ERROR(format, ...) FT_DEBUG_PRINT_E(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_WARN(format, ...)  FT_DEBUG_PRINT_W(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_INFO(format, ...)  FT_DEBUG_PRINT_I(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_DEBUG(format, ...) FT_DEBUG_PRINT_D(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)

/************************** Function Prototypes ******************************/
extern void USBH_IRQHandler(void *);

/*****************************************************************************/
static void UsbHcInterrruptHandler(s32 vector, void *param)
{
    USBH_IRQHandler(param);
}

static void UsbHcSetupInterrupt(u32 id)
{
    u32 cpu_id;
    u32 irq_num = (id == FUSB3_ID_0) ? FUSB3_0_IRQ_NUM : FUSB3_1_IRQ_NUM;
    u32 irq_priority = 13U;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     UsbHcInterrruptHandler,
                     NULL,
                     NULL);

    /* enable irq */
    InterruptUmask(irq_num);
}

void UsbHcSetupMemp(void)
{
    if (FT_COMPONENT_IS_READY != memp.is_ready)
    {
        USB_ASSERT(FT_SUCCESS == FMempInit(&memp, &memp_buf[0], &memp_buf[0] + FUSB_MEMP_TOTAL_SIZE));
    }
}

/* implement cherryusb weak functions */
void usb_hc_low_level_init()
{
    UsbHcSetupMemp();
    UsbHcSetupInterrupt(CONFIG_USBHOST_XHCI_ID);
}

unsigned long usb_hc_get_register_base()
{
    if (FUSB3_ID_0 == CONFIG_USBHOST_XHCI_ID)
        return FUSB3_0_BASE_ADDR + FUSB3_XHCI_OFFSET;
    else
        return FUSB3_1_BASE_ADDR + FUSB3_XHCI_OFFSET;
}

void *usb_hc_malloc(size_t size)
{
    return usb_hc_malloc_align(sizeof(void *), size);
}

void *usb_hc_malloc_align(size_t align, size_t size)
{
    void *result = FMempMallocAlign(&memp, size, align);

    if (result)
    {
        memset(result, 0U, size);
    }

    return result;
}

void usb_hc_free(void *ptr)
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

void usb_hc_dcache_invalidate(void *addr, unsigned long len)
{
    FCacheDCacheInvalidateRange((uintptr)addr, len);
}
/*****************************************/

static void UsbInitTask(void *args)
{
    if (0 == usbh_initialize())
    {
        printf("Init cherryusb host successfully.put 'usb lsusb -t' to see devices.\r\n");
    }
    else
    {
        FUSB_ERROR("Init cherryusb host failed.");
    }

    vTaskDelete(NULL);
}

BaseType_t FFreeRTOSInitUsb()
{
    BaseType_t ret = pdPASS;

    taskENTER_CRITICAL(); /* no schedule when create task */

    ret = xTaskCreate((TaskFunction_t)UsbInitTask,
                      (const char *)"UsbInitTask",
                      (uint16_t)2048,
                      NULL,
                      (UBaseType_t)configMAX_PRIORITIES - 1,
                      NULL);
    FASSERT_MSG(pdPASS == ret, "create task failed");

    taskEXIT_CRITICAL(); /* allow schedule since task created */

    return ret;
}

BaseType_t FFreeRTOSListUsbDev(int argc, char *argv[])
{
    return lsusb(argc, argv);
}