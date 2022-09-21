#ifndef SDK_CONFIG_H__
#define SDK_CONFIG_H__

/* Freertos Configuration */

#define CONFIG_TARGET_NAME "e2000d_freertos_a64"
/* end of Freertos Configuration */

/* Standalone Setting */

#define CONFIG_USE_FREERTOS

/* Arch Configuration */

/* CONFIG_TARGET_ARMV8_AARCH32 is not set */
#define CONFIG_TARGET_ARMV8_AARCH64
#define CONFIG_USE_CACHE
#define CONFIG_USE_MMU
#define CONFIG_USE_SYS_TICK
/* CONFIG_MMU_DEBUG_PRINTS is not set */
/* end of Arch Configuration */

/* Board Configuration */

/* CONFIG_TARGET_F2000_4 is not set */
/* CONFIG_TARGET_D2000 is not set */
/* CONFIG_TARGET_E2000Q is not set */
#define CONFIG_TARGET_E2000D
/* CONFIG_TARGET_E2000S is not set */
#define CONFIG_TARGET_E2000
#define CONFIG_DEFAULT_DEBUG_PRINT_UART1
/* CONFIG_DEFAULT_DEBUG_PRINT_UART0 is not set */
/* CONFIG_DEFAULT_DEBUG_PRINT_UART2 is not set */
/* end of Board Configuration */

/* Components Configuration */

#define CONFIG_USE_SPI
#define CONFIG_USE_FSPIM
/* CONFIG_USE_QSPI is not set */
#define CONFIG_USE_GIC
#define CONFIG_ENABLE_GICV3
#define CONFIG_USE_SERIAL

/* Usart Configuration */

#define CONFIG_ENABLE_Pl011_UART
/* end of Usart Configuration */
/* CONFIG_USE_GPIO is not set */
/* CONFIG_USE_ETH is not set */
/* CONFIG_USE_CAN is not set */
/* CONFIG_USE_I2C is not set */
/* CONFIG_USE_TIMER is not set */
/* CONFIG_USE_MIO is not set */
/* CONFIG_USE_SDMMC is not set */
/* CONFIG_USE_PCIE is not set */
/* CONFIG_USE_WDT is not set */
#define CONFIG_USE_DMA
/* CONFIG_ENABLE_FGDMA is not set */
#define CONFIG_ENABLE_FDDMA
/* CONFIG_USE_NAND is not set */
/* CONFIG_USE_RTC is not set */
/* CONFIG_USE_SATA is not set */
/* CONFIG_USE_USB is not set */
/* CONFIG_USE_ADC is not set */
/* CONFIG_USE_PWM is not set */
/* CONFIG_USE_IPC is not set */
/* end of Components Configuration */
#define CONFIG_USE_NEW_LIBC
/* end of Standalone Setting */

/* Building Option */

/* CONFIG_LOG_VERBOS is not set */
#define CONFIG_LOG_DEBUG
/* CONFIG_LOG_INFO is not set */
/* CONFIG_LOG_WARN is not set */
/* CONFIG_LOG_ERROR is not set */
/* CONFIG_LOG_NONE is not set */
#define CONFIG_USE_DEFAULT_INTERRUPT_CONFIG
#define CONFIG_INTERRUPT_ROLE_MASTER
/* CONFIG_INTERRUPT_ROLE_SLAVE is not set */
#define CONFIG_LOG_EXTRA_INFO
/* CONFIG_BOOTUP_DEBUG_PRINTS is not set */

/* Linker Options */

/* CONFIG_AARCH32_RAM_LD is not set */
#define CONFIG_AARCH64_RAM_LD
/* CONFIG_USER_DEFINED_LD is not set */
#define CONFIG_LINK_SCRIPT_ROM
#define CONFIG_ROM_START_UP_ADDR 0x80100000
#define CONFIG_ROM_SIZE_MB 1
#define CONFIG_LINK_SCRIPT_RAM
#define CONFIG_RAM_START_UP_ADDR 0x81000000
#define CONFIG_RAM_SIZE_MB 64
#define CONFIG_HEAP_SIZE 1
#define CONFIG_STACK_SIZE 0x100000
#define CONFIG_FPU_STACK_SIZE 0x1000
/* end of Linker Options */

/* Compiler Options */

#define CONFIG_OUTPUT_BINARY
/* end of Compiler Options */
/* end of Building Option */

/* Component Configuration */

/* Freertos Uart Drivers */

#define CONFIG_FREERTOS_USE_UART
/* end of Freertos Uart Drivers */

/* Freertos Pwm Drivers */

/* CONFIG_FREERTOS_USE_PWM is not set */
/* end of Freertos Pwm Drivers */

/* Freertos Qspi Drivers */

/* CONFIG_FREERTOS_USE_QSPI is not set */
/* end of Freertos Qspi Drivers */

/* Freertos Wdt Drivers */

/* CONFIG_FREERTOS_USE_WDT is not set */
/* end of Freertos Wdt Drivers */

/* Freertos Eth Drivers */

/* CONFIG_FREERTOS_USE_XMAC is not set */
/* end of Freertos Eth Drivers */

/* Freertos Gpio Drivers */

/* CONFIG_FREERTOS_USE_GPIO is not set */
/* end of Freertos Gpio Drivers */

/* Freertos Spim Drivers */

#define CONFIG_FREERTOS_USE_FSPIM
/* end of Freertos Spim Drivers */

/* Freertos DMA Drivers */

#define CONFIG_FREERTOS_USE_FDDMA
/* CONFIG_FREERTOS_USE_FGDMA is not set */
/* end of Freertos DMA Drivers */

/* Freertos MMC Drivers */

/* CONFIG_FREERTOS_USE_FSDIO is not set */
/* end of Freertos MMC Drivers */

/* Freertos Adc Drivers */

/* CONFIG_FREERTOS_USE_ADC is not set */
/* end of Freertos Adc Drivers */
/* end of Component Configuration */

/* FreeRTOS Setting */

/* CONFIG_USE_LWIP is not set */
#define CONFIG_USE_BACKTRACE
/* CONFIG_USE_FATFS is not set */
/* CONFIG_USE_SFUD is not set */
/* CONFIG_USE_SPIFFS is not set */
/* CONFIG_USE_AMP is not set */
#define CONFIG_USE_LETTER_SHELL

/* Letter Shell Configuration */

#define CONFIG_LS_PL011_UART
#define CONFIG_DEFAULT_LETTER_SHELL_USE_UART1
/* CONFIG_DEFAULT_LETTER_SHELL_USE_UART0 is not set */
/* CONFIG_DEFAULT_LETTER_SHELL_USE_UART2 is not set */
/* end of Letter Shell Configuration */
#define CONFIG_USE_TLSF
/* CONFIG_USE_SDMMC_CMD is not set */
#define CONFIG_USE_CHERRY_USB

/* CherryUSB Configuration */

#define CONFIG_CHERRY_USB_PORT_XHCI
/* CONFIG_CHERRY_USB_PORT_PHYTIUM_OTG is not set */
#define CONFIG_CHERRYUSB_HOST
/* CONFIG_CHERRYUSB_DEVICE is not set */
#define CONFIG_CHERRY_USB_HOST_HUB
#define CONFIG_CHERRY_USB_HOST_MSC
#define CONFIG_CHERRY_USB_HOST_HID
/* CONFIG_CHERRY_USB_HOST_MTP is not set */
/* CONFIG_CHERRY_USB_HOST_CDC is not set */
/* CONFIG_CHERRY_USB_HOST_PRINTER is not set */
/* CONFIG_CHERRY_USB_HOST_AXU_NET is not set */
/* CONFIG_CHERRY_USB_HOST_RNDIS_WIRELESS is not set */
/* end of CherryUSB Configuration */
/* end of FreeRTOS Setting */

#endif
