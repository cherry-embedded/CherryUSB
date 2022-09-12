/*
 * Copyright (c) 2021 hpmicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include "board.h"
#include "hpm_debug_console.h"

#define LED_FLASH_PERIOD_IN_MS 300

int main(void)
{
    int u;

    board_init();
    board_init_led_pins();
    board_init_usb_pins();

    board_timer_create(LED_FLASH_PERIOD_IN_MS, board_led_toggle);

    printf("hello world\n");

    extern void cdc_acm_msc_init(void);
    
    cdc_acm_msc_init();
    while(1)
    {
        extern void cdc_acm_data_send_with_dtr_test(void);
        cdc_acm_data_send_with_dtr_test();
    }
    return 0;
}
