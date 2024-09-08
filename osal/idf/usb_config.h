/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_rom_sys.h"

/* ================ USB common Configuration ================ */

#define CONFIG_USB_PRINTF(...) esp_rom_printf(__VA_ARGS__)

#ifndef CONFIG_USB_DBG_LEVEL
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

/* Enable print with color */
#define CONFIG_USB_PRINTF_COLOR_ENABLE

/* data align size when use dma */
#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

/* attribute data into no cache ram */
#define USB_NOCACHE_RAM_SECTION

/* ================= USB Device Stack Configuration ================ */
// NOTE: Below configurations are removed to Kconfig, `idf.py menuconfig` to config them

/* Ep0 in and out transfer buffer */
#ifndef CONFIG_USBDEV_REQUEST_BUFFER_LEN
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 512
#endif

/* Setup packet log for debug */
// #define CONFIG_USBDEV_SETUP_LOG_PRINT

/* Send ep0 in data from user buffer instead of copying into ep0 reqdata
 * Please note that user buffer must be aligned with CONFIG_USB_ALIGN_SIZE
*/
// #define CONFIG_USBDEV_EP0_INDATA_NO_COPY

/* Check if the input descriptor is correct */
// #define CONFIG_USBDEV_DESC_CHECK

/* Enable test mode */
// #define CONFIG_USBDEV_TEST_MODE

#ifndef CONFIG_USBDEV_MSC_MAX_LUN
#define CONFIG_USBDEV_MSC_MAX_LUN 1
#endif

#ifndef CONFIG_USBDEV_MSC_MAX_BUFSIZE
#define CONFIG_USBDEV_MSC_MAX_BUFSIZE 512
#endif

#ifndef CONFIG_USBDEV_MSC_MANUFACTURER_STRING
#define CONFIG_USBDEV_MSC_MANUFACTURER_STRING ""
#endif

#ifndef CONFIG_USBDEV_MSC_PRODUCT_STRING
#define CONFIG_USBDEV_MSC_PRODUCT_STRING ""
#endif

#ifndef CONFIG_USBDEV_MSC_VERSION_STRING
#define CONFIG_USBDEV_MSC_VERSION_STRING "0.01"
#endif

// #define CONFIG_USBDEV_MSC_THREAD

#ifndef CONFIG_USBDEV_MSC_PRIO
#define CONFIG_USBDEV_MSC_PRIO 4
#endif

#ifndef CONFIG_USBDEV_MSC_STACKSIZE
#define CONFIG_USBDEV_MSC_STACKSIZE 2048
#endif

#ifndef CONFIG_USBDEV_RNDIS_RESP_BUFFER_SIZE
#define CONFIG_USBDEV_RNDIS_RESP_BUFFER_SIZE 156
#endif

/* rndis transfer buffer size, must be a multiple of (1536 + 44)*/
#ifndef CONFIG_USBDEV_RNDIS_ETH_MAX_FRAME_SIZE
#define CONFIG_USBDEV_RNDIS_ETH_MAX_FRAME_SIZE 1580
#endif

#ifndef CONFIG_USBDEV_RNDIS_VENDOR_ID
#define CONFIG_USBDEV_RNDIS_VENDOR_ID 0x0000ffff
#endif

#ifndef CONFIG_USBDEV_RNDIS_VENDOR_DESC
#define CONFIG_USBDEV_RNDIS_VENDOR_DESC "CherryUSB"
#endif

#define CONFIG_USBDEV_RNDIS_USING_LWIP

/* ================ USB HOST Stack Configuration ================== */

#define CONFIG_USBHOST_MAX_RHPORTS          1
#define CONFIG_USBHOST_MAX_EXTHUBS          1
#define CONFIG_USBHOST_MAX_EHPORTS          4
#define CONFIG_USBHOST_MAX_INTERFACES       8
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 8
#define CONFIG_USBHOST_MAX_ENDPOINTS        4

#define CONFIG_USBHOST_MAX_CDC_ACM_CLASS 4
#define CONFIG_USBHOST_MAX_HID_CLASS     4
#define CONFIG_USBHOST_MAX_MSC_CLASS     2
#define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
#define CONFIG_USBHOST_MAX_VIDEO_CLASS   1

#define CONFIG_USBHOST_DEV_NAMELEN 16

#ifndef CONFIG_USBHOST_PSC_PRIO
#define CONFIG_USBHOST_PSC_PRIO 0
#endif
#ifndef CONFIG_USBHOST_PSC_STACKSIZE
#define CONFIG_USBHOST_PSC_STACKSIZE 2048
#endif

// #define CONFIG_USBHOST_GET_STRING_DESC

// #define CONFIG_USBHOST_MSOS_ENABLE
#ifndef CONFIG_USBHOST_MSOS_VENDOR_CODE
#define CONFIG_USBHOST_MSOS_VENDOR_CODE 0x00
#endif

/* Ep0 max transfer buffer */
#ifndef CONFIG_USBHOST_REQUEST_BUFFER_LEN
#define CONFIG_USBHOST_REQUEST_BUFFER_LEN 2048
#endif

#ifndef CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
#define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT 500
#endif

#ifndef CONFIG_USBHOST_MSC_TIMEOUT
#define CONFIG_USBHOST_MSC_TIMEOUT 5000
#endif

/* This parameter affects usb performance, and depends on (TCP_WND)tcp eceive windows size,
 * you can change to 2K ~ 16K and must be larger than TCP RX windows size in order to avoid being overflow.
 */
#ifndef CONFIG_USBHOST_RNDIS_ETH_MAX_RX_SIZE
#define CONFIG_USBHOST_RNDIS_ETH_MAX_RX_SIZE (2048)
#endif

/* Because lwip do not support multi pbuf at a time, so increasing this variable has no performance improvement */
#ifndef CONFIG_USBHOST_RNDIS_ETH_MAX_TX_SIZE
#define CONFIG_USBHOST_RNDIS_ETH_MAX_TX_SIZE (2048)
#endif

/* This parameter affects usb performance, and depends on (TCP_WND)tcp eceive windows size,
 * you can change to 2K ~ 16K and must be larger than TCP RX windows size in order to avoid being overflow.
 */
#ifndef CONFIG_USBHOST_CDC_NCM_ETH_MAX_RX_SIZE
#define CONFIG_USBHOST_CDC_NCM_ETH_MAX_RX_SIZE (2048)
#endif
/* Because lwip do not support multi pbuf at a time, so increasing this variable has no performance improvement */
#ifndef CONFIG_USBHOST_CDC_NCM_ETH_MAX_TX_SIZE
#define CONFIG_USBHOST_CDC_NCM_ETH_MAX_TX_SIZE (2048)
#endif

/* This parameter affects usb performance, and depends on (TCP_WND)tcp eceive windows size,
 * you can change to 2K ~ 16K and must be larger than TCP RX windows size in order to avoid being overflow.
 */
#ifndef CONFIG_USBHOST_ASIX_ETH_MAX_RX_SIZE
#define CONFIG_USBHOST_ASIX_ETH_MAX_RX_SIZE (2048)
#endif
/* Because lwip do not support multi pbuf at a time, so increasing this variable has no performance improvement */
#ifndef CONFIG_USBHOST_ASIX_ETH_MAX_TX_SIZE
#define CONFIG_USBHOST_ASIX_ETH_MAX_TX_SIZE (2048)
#endif

/* This parameter affects usb performance, and depends on (TCP_WND)tcp eceive windows size,
 * you can change to 2K ~ 16K and must be larger than TCP RX windows size in order to avoid being overflow.
 */
#ifndef CONFIG_USBHOST_RTL8152_ETH_MAX_RX_SIZE
#define CONFIG_USBHOST_RTL8152_ETH_MAX_RX_SIZE (2048)
#endif
/* Because lwip do not support multi pbuf at a time, so increasing this variable has no performance improvement */
#ifndef CONFIG_USBHOST_RTL8152_ETH_MAX_TX_SIZE
#define CONFIG_USBHOST_RTL8152_ETH_MAX_TX_SIZE (2048)
#endif

#define CONFIG_USBHOST_BLUETOOTH_HCI_H4
// #define CONFIG_USBHOST_BLUETOOTH_HCI_LOG

#ifndef CONFIG_USBHOST_BLUETOOTH_TX_SIZE
#define CONFIG_USBHOST_BLUETOOTH_TX_SIZE 2048
#endif
#ifndef CONFIG_USBHOST_BLUETOOTH_RX_SIZE
#define CONFIG_USBHOST_BLUETOOTH_RX_SIZE 2048
#endif

/* ================ USB Device Port Configuration ================*/

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define ESP_USBD_BASE           0x60080000

#define CONFIG_USBDEV_MAX_BUS 1
// esp32s2/s3 has 7 endpoints in device mode (include ep0)
#define CONFIG_USBDEV_EP_NUM 7

/* ---------------- DWC2 Configuration ---------------- */
//esp32s2/s3 can support up to 5 IN endpoints(include ep0) at the same time
#define CONFIG_USB_DWC2_RXALL_FIFO_SIZE (208 / 4)
#define CONFIG_USB_DWC2_TX0_FIFO_SIZE (64 / 4)
#define CONFIG_USB_DWC2_TX1_FIFO_SIZE (136 / 4)
#define CONFIG_USB_DWC2_TX2_FIFO_SIZE (136 / 4)
#define CONFIG_USB_DWC2_TX3_FIFO_SIZE (128 / 4)
#define CONFIG_USB_DWC2_TX4_FIFO_SIZE (128 / 4)
#define CONFIG_USB_DWC2_TX5_FIFO_SIZE (0 / 4)
#define CONFIG_USB_DWC2_TX6_FIFO_SIZE (0 / 4)
#define CONFIG_USB_DWC2_TX7_FIFO_SIZE (0 / 4)
#define CONFIG_USB_DWC2_TX8_FIFO_SIZE (0 / 4)

#define CONFIG_USB_DWC2_DMA_ENABLE

#elif CONFIG_IDF_TARGET_ESP32C5 || CONFIG_IDF_TARGET_ESP32P4
#define CONFIG_USB_HS
#define ESP_USBD_BASE           0x60080000
// todo: check c5, p4 in later
#define CONFIG_USBDEV_EP_NUM 7
#else
#error "Unsupported SoC"
#endif

/* ================ USB Host Port Configuration ==================*/

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define ESP_USBH_BASE               0x60080000

#define CONFIG_USBHOST_MAX_BUS 1
// esp32s2/s3 has 8 endpoints in host mode (include ep0)
#define CONFIG_USBHOST_PIPE_NUM 8

/* ---------------- DWC2 Configuration ---------------- */
/* largest non-periodic USB packet used / 4 */
#define CONFIG_USB_DWC2_NPTX_FIFO_SIZE (240 / 4)
/* largest periodic USB packet used / 4 */
#define CONFIG_USB_DWC2_PTX_FIFO_SIZE (240 / 4)
/*
 * (largest USB packet used / 4) + 1 for status information + 1 transfer complete +
 * 1 location each for Bulk/Control endpoint for handling NAK/NYET scenario
 */
#define CONFIG_USB_DWC2_RX_FIFO_SIZE ((200 - CONFIG_USB_DWC2_NPTX_FIFO_SIZE - CONFIG_USB_DWC2_PTX_FIFO_SIZE))

#elif CONFIG_IDF_TARGET_ESP32C5 || CONFIG_IDF_TARGET_ESP32P4
// todo: check c5, p4 in later
#define ESP_USBH_BASE               0x60080000
#define CONFIG_USBHOST_PIPE_NUM 8
#else
#error "Unsupported SoC"
#endif