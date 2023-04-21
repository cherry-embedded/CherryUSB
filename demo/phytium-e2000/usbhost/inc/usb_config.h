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
 * FilePath: usb_config.h
 * Date: 2022-09-19 17:28:44
 * LastEditTime: 2022-09-19 17:28:45
 * Description:  This file is for usb hc xhci configuration.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/19   init commit
 * 1.1  liqiaozhong 2023/2/10   update to v0.7.0
 */

#ifndef USB_CONFIG_H
#define USB_CONFIG_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ================ USB common Configuration ================ */
void *usb_hc_malloc(size_t size);
void usb_hc_free();
void *usb_hc_malloc_align(size_t align, size_t size);

#define usb_malloc(size)                    usb_hc_malloc(size)
#define usb_free(ptr)                       usb_hc_free(ptr)
#define usb_align(align, size)              usb_hc_malloc_align(align, size)  

#ifndef CONFIG_USB_DBG_LEVEL
    #define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#endif

#ifndef CONFIG_USB_PRINTF
    #define CONFIG_USB_PRINTF printf
#endif

/* Enable print with color */
#define CONFIG_USB_PRINTF_COLOR_ENABLE

/* data align size when use dma */
#ifndef CONFIG_USB_ALIGN_SIZE
    #define CONFIG_USB_ALIGN_SIZE 4
#endif

/* attribute data into no cache ram */
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".noncacheable")))

/* ================= USB Device Stack Configuration ================ */

/* Ep0 max transfer buffer, specially for receiving data from ep0 out */
#define CONFIG_USBDEV_REQUEST_BUFFER_LEN 256


#ifndef CONFIG_USBDEV_MSC_BLOCK_SIZE
    #define CONFIG_USBDEV_MSC_BLOCK_SIZE 512
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

// #define CONFIG_USBHOST_GET_STRING_DESC
#define CONFIG_USBHOST_GET_DEVICE_DESC

// #define CONFIG_USBDEV_MSC_THREAD
#define CONFIG_INPUT_MOUSE_WHEEL

#ifdef CONFIG_USBDEV_MSC_THREAD
    #ifndef CONFIG_USBDEV_MSC_STACKSIZE
        #define CONFIG_USBDEV_MSC_STACKSIZE 2048
    #endif

    #ifndef CONFIG_USBDEV_MSC_PRIO
        #define CONFIG_USBDEV_MSC_PRIO 4
    #endif
#endif

#ifndef CONFIG_USBDEV_AUDIO_VERSION
    #define CONFIG_USBDEV_AUDIO_VERSION 0x0100
#endif

#ifndef CONFIG_USBDEV_AUDIO_MAX_CHANNEL
    #define CONFIG_USBDEV_AUDIO_MAX_CHANNEL 8
#endif

/* ================ USB HOST Stack Configuration ================== */

#define CONFIG_USBHOST_MAX_RHPORTS          2
#define CONFIG_USBHOST_MAX_EXTHUBS          2
#define CONFIG_USBHOST_MAX_EHPORTS          4
#define CONFIG_USBHOST_MAX_INTERFACES       6
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 1
#define CONFIG_USBHOST_MAX_ENDPOINTS        4

#define CONFIG_USBHOST_DEV_NAMELEN 16

#ifndef CONFIG_USBHOST_PSC_PRIO
    #define CONFIG_USBHOST_PSC_PRIO 4
#endif
#ifndef CONFIG_USBHOST_PSC_STACKSIZE
    #define CONFIG_USBHOST_PSC_STACKSIZE 4096
#endif

/* Ep0 max transfer buffer */
#define CONFIG_USBHOST_REQUEST_BUFFER_LEN 512

#ifndef CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
    #define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT 500
#endif

#ifndef CONFIG_USBHOST_MSC_TIMEOUT
    #define CONFIG_USBHOST_MSC_TIMEOUT 5000
#endif

/* do not try to enumrate one interface */
#ifndef CONFIG_USBHOST_ENUM_FIRST_INTERFACE_ONLY
    #define CONFIG_USBHOST_ENUM_FIRST_INTERFACE_ONLY
#endif

/* ================ USB Device Port Configuration ================*/

#define CONFIG_XHCI_PAGE_SIZE   4096U
#define CONFIG_XHCI_PAGE_SHIFT  12U

/* ================ USB Host Port Configuration ==================*/

#define CONFIG_USBHOST_PIPE_NUM 10

/* ================ XHCI Configuration ================ */
#define CONFIG_USBHOST_XHCI
#define CONFIG_USBHOST_XHCI_ID  0

#ifdef __cplusplus
}
#endif

#endif
