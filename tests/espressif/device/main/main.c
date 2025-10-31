/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usbd_core.h"
#include "usbh_core.h"
#include "demo/cdc_acm_msc_template.c"

extern void cdc_acm_msc_init(uint8_t busid, uintptr_t reg_base);

void app_main(void)
{
    USB_LOG_INFO("Hello CherryUSB!\n");

    cdc_acm_msc_init(0, 0x60080000);
    while(1)
    {
        vTaskDelay(10);
    }
}
