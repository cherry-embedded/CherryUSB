/**
  *********************************************************************************
  *
  * @file    main.c
  * @brief   Main file for DEMO
  *
  * @version V1.0
  * @date    26 Jun 2019
  * @author  AE Team
  * @note
  *          Change Logs:
  *          Date            Author          Notes
  *          26 Jun 2019     AE Team         The first version
  *
  * Copyright (C) Shanghai Eastsoft Microelectronics Co. Ltd. All rights reserved.
  *
  * SPDX-License-Identifier: Apache-2.0
  *
  * Licensed under the Apache License, Version 2.0 (the License); you may
  * not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an AS IS BASIS, WITHOUT
  * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  **********************************************************************************
  */
#include "main.h"
#include "stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usbh_core.h"

static void vtask_led(void *pvParameters);
uart_handle_t h_uart = { 0 };
/** @addtogroup Projects_Examples_USB
  * @{
  */
int fputc(int ch, FILE *f)
{
    ald_uart_send(&h_uart, (uint8_t *)&ch, 1, 1000);
    return ch;
}

/**
  * @brief  Initializate pin of USB.
  * @retval None
  */
void usb_pin_init(void)
{
    gpio_init_t x;

    /* Initialize vbus pin */
    x.mode = GPIO_MODE_OUTPUT;
    x.odos = GPIO_PUSH_PULL;
    x.pupd = GPIO_PUSH_UP;
    x.podrv = GPIO_OUT_DRIVE_6;
    x.nodrv = GPIO_OUT_DRIVE_6;
    x.flt = GPIO_FILTER_DISABLE;
    x.type = GPIO_TYPE_TTL;
    x.func = GPIO_FUNC_5;
    ald_gpio_init(GPIOB, GPIO_PIN_15, &x);

    return;
}
/**
  * @brief  Initializate pin of uart module.
  * @retval None
  */
void uart_pin_init(void)
{
    gpio_init_t x;

    /* Initialize tx pin */
    x.mode = GPIO_MODE_OUTPUT;
    x.odos = GPIO_PUSH_PULL;
    x.pupd = GPIO_PUSH_UP;
    x.podrv = GPIO_OUT_DRIVE_1;
    x.nodrv = GPIO_OUT_DRIVE_0_1;
    x.flt = GPIO_FILTER_DISABLE;
    x.type = GPIO_TYPE_TTL;
    x.func = GPIO_FUNC_3;
    ald_gpio_init(GPIOB, GPIO_PIN_10, &x);

    /* Initialize rx pin */
    x.mode = GPIO_MODE_INPUT;
    x.odos = GPIO_PUSH_PULL;
    x.pupd = GPIO_PUSH_UP;
    x.podrv = GPIO_OUT_DRIVE_1;
    x.nodrv = GPIO_OUT_DRIVE_0_1;
    x.flt = GPIO_FILTER_DISABLE;
    x.type = GPIO_TYPE_TTL;
    x.func = GPIO_FUNC_3;
    ald_gpio_init(GPIOB, GPIO_PIN_11, &x);

    /* Initialize uart */
    h_uart.perh = UART0;
    h_uart.init.baud = 115200;
    h_uart.init.word_length = UART_WORD_LENGTH_8B;
    h_uart.init.stop_bits = UART_STOP_BITS_1;
    h_uart.init.parity = UART_PARITY_NONE;
    h_uart.init.mode = UART_MODE_UART;
    h_uart.init.fctl = UART_HW_FLOW_CTL_DISABLE;
    h_uart.tx_cplt_cbk = NULL;
    h_uart.rx_cplt_cbk = NULL;
    h_uart.error_cbk = NULL;
    ald_uart_init(&h_uart);
}

void usb_hc_low_level_init(void)
{
    ald_pmu_perh_power_config(PMU_POWER_USB, ENABLE);
    ald_cmu_perh_clock_config(CMU_PERH_USB, ENABLE);
    ald_cmu_perh_clock_config(CMU_PERH_GPIO, ENABLE);
    ald_cmu_usb_clock_config(CMU_USB_CLOCK_SEL_HOSC, CMU_USB_DIV_1);
    ald_rmu_reset_periperal(RMU_PERH_USB);
    ald_mcu_irq_config(USB_INT_IRQn, 2, 2, ENABLE);
    ald_mcu_irq_config(USB_DMA_IRQn, 2, 2, ENABLE);
    usb_pin_init();
}

/**
  * @brief  Test main function
  * @retval Status.
  */
int main()
{
    int i;

    /* Initialize ALD */
    ald_cmu_init();
    /* Configure system clock */
    ald_cmu_pll1_config(CMU_PLL1_INPUT_HOSC_3, CMU_PLL1_OUTPUT_48M);
    ald_cmu_clock_config(CMU_CLOCK_PLL1, 48000000);
    ald_cmu_perh_clock_config(CMU_PERH_ALL, ENABLE);

    uart_pin_init();
    printf("\rSystem start...\r\n");
    extern void usbh_class_test(void);
    usbh_initialize();
    usbh_class_test();
    vTaskStartScheduler();
    while (1) {
    }
}

/**
  * @brief  led task entry function
  * @param  parameter: user's paramter
  * @retval None
  */
static void vtask_led(void *pvParameters)
{
    TickType_t xlast_wake_time;
    const TickType_t xfreq = 500;

    xlast_wake_time = xTaskGetTickCount();

    while (1) {
        printf("test\r\n");
        vTaskDelayUntil(&xlast_wake_time, pdMS_TO_TICKS(xfreq));
    }
}
/**
  * @}
  */
/**
  * @}
  */
