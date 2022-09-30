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
 * Description:  This files is for 
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 */
/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "ft_assert.h"
#include "interrupt.h"
#include "cpu_info.h"
#include "ft_debug.h"

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

static void UsbMscTask(void * args)
{
    int ret;
    struct usbh_msc *msc_class;
    static uint8_t partition_table[512] = {0};

    while (TRUE)
    {
        msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
        if (msc_class == NULL) 
        {
            USB_LOG_RAW("do not find /dev/sda\r\n");
            goto err_exit;
        }

        /* get the partition table */
        ret = usbh_msc_scsi_read10(msc_class, 0, partition_table, 1);
        if (ret < 0) 
        {
            USB_LOG_RAW("scsi_read10 error,ret:%d\r\n", ret);
            goto err_exit;
        }
        
        for (uint32_t i = 0; i < 512; i++) 
        {
            if (i % 16 == 0) 
            {
                USB_LOG_RAW("\r\n");
            }
            USB_LOG_RAW("%02x ", partition_table[i]);
        }
        USB_LOG_RAW("\r\n");

        /* write partition table */
        for (uint32_t i = 0; i < 512; i++) 
        {
            partition_table[i] ^= 0xfffff;
        }

        ret = usbh_msc_scsi_write10(msc_class, 0, partition_table, 1);
        if (ret < 0) 
        {
            USB_LOG_RAW("scsi_write10 error,ret:%d\r\n", ret);
            goto err_exit;
        }

        vTaskDelay(10);
    }

err_exit:
    vTaskDelete(NULL);
}

BaseType_t FFreeRTOSWriteReadUsbDisk(void)
{
    BaseType_t ret = pdPASS;

    taskENTER_CRITICAL(); /* no schedule when create task */

    ret = xTaskCreate((TaskFunction_t )UsbMscTask,
                            (const char* )"UsbMscTask",
                            (uint16_t )2048,
                            NULL,
                            (UBaseType_t )configMAX_PRIORITIES - 1,
                            NULL);
    FASSERT_MSG(pdPASS == ret, "create task failed");

    taskEXIT_CRITICAL(); /* allow schedule since task created */

    return ret;    
}
