/*
 * Copyright (c) 2021 HPMicro
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/*  HPM example includes. */
#include <stdio.h>
#include "board.h"
#include "hpm_clock_drv.h"
#include "hpm_l1c_drv.h"
#include "hpm_gpio_drv.h"
#include "shell.h"
#include "usbh_core.h"
#include "lwip/tcpip.h"
#ifdef CONFIG_USB_EHCI_ISO
#include "usbh_uvc_stream.h"
#include "usbh_uac_stream.h"
#endif

SDK_DECLARE_EXT_ISR_M(BOARD_CONSOLE_UART_IRQ, shell_uart_isr)

#define task_start_PRIORITY (configMAX_PRIORITIES - 2U)

static void task_start(void *param);

int main(void)
{
    board_init();
    board_init_led_pins();
    board_init_usb((USB_Type *)CONFIG_HPM_USBH_BASE);

    /* set irq priority */
    intc_set_irq_priority(CONFIG_HPM_USBH_IRQn, 1);

    /* Initialize the LwIP stack */
    tcpip_init(NULL, NULL);

    printf("Start usb host task...\r\n");
#ifdef CONFIG_USB_EHCI_ISO
    extern void uvc2lcd_init(void);

    uvc2lcd_init();
#endif

#ifndef CONFIG_USB_OTG_ENABLE
    usbh_initialize(0, CONFIG_HPM_USBH_BASE, NULL);
#else
    extern void cdc_acm_otg_init(uint8_t busid, uintptr_t reg_base);
    cdc_acm_otg_init(0, CONFIG_HPM_USBH_BASE);
#endif
    if (pdPASS != xTaskCreate(task_start, "task_start", 1024U, NULL, task_start_PRIORITY, NULL)) {
        printf("Task start creation failed!\r\n");
        for (;;) {
            ;
        }
    }

    vTaskStartScheduler();
    printf("Unexpected scheduler exit!\r\n");
    for (;;) {
        ;
    }

    return 0;
}

static void task_start(void *param)
{
    (void)param;

    printf("Try to initialize the uart\r\n"
           "  if you are using the console uart as the shell uart\r\n"
           "  failure to initialize may result in no log\r\n");

    uart_config_t shell_uart_config = { 0 };
    uart_default_config(BOARD_CONSOLE_UART_BASE, &shell_uart_config);
    shell_uart_config.src_freq_in_hz = clock_get_frequency(BOARD_CONSOLE_UART_CLK_NAME);
    shell_uart_config.baudrate = 115200;

    if (status_success != uart_init(BOARD_CONSOLE_UART_BASE, &shell_uart_config)) {
        /* uart failed to be initialized */
        printf("Failed to initialize uart\r\n");
        for (;;) {
            ;
        }
    }

    printf("Initialize shell uart successfully\r\n");

    /* default password is : 12345678 */
    /* shell_init() must be called in-task */
    if (0 != shell_init(BOARD_CONSOLE_UART_BASE, false)) {
        /* shell failed to be initialized */
        printf("Failed to initialize shell\r\n");
        for (;;) {
            ;
        }
    }

    printf("Initialize shell successfully\r\n");

    /* irq must be enabled after shell_init() */
    uart_enable_irq(BOARD_CONSOLE_UART_BASE, uart_intr_rx_data_avail_or_timeout);
    intc_m_enable_irq_with_priority(BOARD_CONSOLE_UART_IRQ, 1);

    printf("Enable shell uart interrupt\r\n");

    printf("Exit start task\r\n");
    vTaskDelete(NULL);
}

CSH_CMD_EXPORT(lsusb, );

#ifdef CONFIG_USB_EHCI_ISO
// clang-format off
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t src_buffer[1024 * 10];
ATTR_PLACE_AT_WITH_ALIGNMENT(".framebuffer", 64) uint8_t dst_buffer[1024 * 10];
// clang-format on

void usb_dma_test()
{
    usbh_video_dma_init();
    for (size_t i = 0; i < 10 * 1024; i++) {
        src_buffer[i] = i & 0xff;
    }
    memset(dst_buffer, 0, 10 * 1024);

    for (uint8_t i = 0; i < 10; i++) {
        usbh_video_dma_lli_fill(i, (uint32_t)src_buffer + i * 1024, (uint32_t)dst_buffer + i * 1024, 1024);
    }
    volatile uint64_t start_tick = hpm_csr_get_core_mcycle();
    usbh_video_dma_start();

    while (usbh_video_dma_isbusy()) {
    }
    volatile uint64_t end_tick = hpm_csr_get_core_mcycle();

    double consumed_seconds = (end_tick - start_tick) * 1.0l / (clock_get_frequency(clock_cpu0) / 1000000);
    printf("dma done:%.2f us\n", consumed_seconds);
    l1c_dc_invalidate((uint32_t)dst_buffer, 10 * 1024);
    for (size_t i = 0; i < 10 * 1024; i++) {
        if (dst_buffer[i] != src_buffer[i]) {
            printf("error:%d\n", i);
            break;
        }
    }
}

int dma_test(int argc, char **argv)
{
    usb_dma_test();
    return 0;
}
CSH_CMD_EXPORT(dma_test, );

int usbh_uvc_start(int argc, char **argv)
{
    uint8_t type;

    if (argc < 2) {
        USB_LOG_ERR("please input correct command: usbh_uvc_start type\r\n");
        USB_LOG_ERR("type 0:yuyv, type 1:mjpeg\r\n");
        return -1;
    }

    type = atoi(argv[1]);
    usbh_video_stream_start(640, 480, type);
    return 0;
}

CSH_CMD_EXPORT(usbh_uvc_start, usbh_uvc_start);

int usbh_uvc_stop(int argc, char **argv)
{
    usbh_video_stream_stop();
    return 0;
}

CSH_CMD_EXPORT(usbh_uvc_stop, usbh_uvc_stop);

int usbh_uac_start(int argc, char **argv)
{
    uint32_t freq;

    if (argc < 2) {
        USB_LOG_ERR("please input correct command: usbh_uac_start freq\r\n");
        return -1;
    }

    freq = atoi(argv[1]);
    usbh_audio_mic_stream_start(freq);
    return 0;
}

CSH_CMD_EXPORT(usbh_uac_start, usbh_uac_start);

int usbh_uac_stop(int argc, char **argv)
{
    usbh_audio_mic_stream_stop();
    return 0;
}

CSH_CMD_EXPORT(usbh_uac_stop, usbh_uac_stop);
#endif