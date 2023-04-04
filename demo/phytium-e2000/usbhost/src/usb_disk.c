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
 * FilePath: usb_disk.c
 * Date: 2022-09-23 08:24:09
 * LastEditTime: 2022-09-23 08:24:10
 * Description:  This file is for the usb disk functions.
 *
 * Modify History:
 *  Ver   Who         Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/10/19   init commit
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

#include "usbh_core.h"
#include "usbh_msc.h"
/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/

/***************** Macros (Inline Functions) Definitions *********************/
#define FUSB_DEBUG_TAG "USB-DISK"
#define FUSB_ERROR(format, ...) FT_DEBUG_PRINT_E(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_WARN(format, ...)  FT_DEBUG_PRINT_W(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_INFO(format, ...)  FT_DEBUG_PRINT_I(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_DEBUG(format, ...) FT_DEBUG_PRINT_D(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)

/************************** Function Prototypes ******************************/


/*****************************************************************************/
static void UsbMscTask(void *args)
{
    int ret;
    struct usbh_msc *msc_class;
    static uint8_t rd_table[512] = {0};
    static uint8_t wr_table[512] = {0};
    u32 loop = 0;
    const char *devname = (const char *)args;

    msc_class = (struct usbh_msc *)usbh_find_class_instance(devname);
    if (msc_class == NULL)
    {
        USB_LOG_RAW("Do not find %s. \r\n", devname);
        goto err_exit;
    }

    while (TRUE)
    {
        /* write partition table */
        memcpy(wr_table, rd_table, sizeof(rd_table));
        for (uint32_t i = 0; i < 512; i++)
        {
            wr_table[i] ^= 0xfffff;
        }

        ret = usbh_msc_scsi_write10(msc_class, 0, wr_table, 1);
        if (ret < 0)
        {
            USB_LOG_ERR("Error in scsi_write10 error, ret:%d", ret);
            goto err_exit;
        }

        /* get the partition table */
        ret = usbh_msc_scsi_read10(msc_class, 0, rd_table, 1);
        if (ret < 0)
        {
            USB_LOG_RAW("Error in scsi_read10, ret:%d", ret);
            goto err_exit;
        }

        /* check if read table == write table */
        if (0 != memcmp(wr_table, rd_table, sizeof(rd_table)))
        {
            USB_LOG_ERR("Failed to check read and write.\r\n");
            goto err_exit;
        }
        else
        {
            printf("[%d] disk read and write successfully.\r\n", loop++);
        }

        if (loop > 10) {
            break;
        }

        vTaskDelay(100);
    }

err_exit:
    vTaskDelete(NULL);
}

BaseType_t FFreeRTOSRunUsbDisk(const char *devname)
{
    BaseType_t ret = pdPASS;

    taskENTER_CRITICAL(); /* no schedule when create task */

    ret = xTaskCreate((TaskFunction_t)UsbMscTask,
                      (const char *)"UsbMscTask",
                      (uint16_t)2048,
                      (void *)devname,
                      (UBaseType_t)configMAX_PRIORITIES - 1,
                      NULL);
    FASSERT_MSG(pdPASS == ret, "create task failed");

    taskEXIT_CRITICAL(); /* allow schedule since task created */

    return ret;
}
