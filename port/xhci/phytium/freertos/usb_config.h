/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef CHERRYUSB_CONFIG_H
#define CHERRYUSB_CONFIG_H

#include <stdio.h>
#include <stdlib.h>
#include "sdkconfig.h"
#include "fparameters.h"

/* ================ USB common Configuration ================ */

#define CONFIG_USB_PRINTF(...) printf(__VA_ARGS__)

void *usb_sys_mem_malloc(size_t size);
void usb_sys_mem_free(void *ptr);
void *usb_sys_malloc_align(size_t align, size_t size);
unsigned long usb_hc_get_register_base(uint32_t id);
unsigned long usb_dc_get_register_base(uint32_t id);
void usb_hc_disable_interrupt(uint32_t id);
void usb_hc_enable_interrupt(uint32_t id);

#define usb_malloc(size)       usb_sys_mem_malloc(size)
#define usb_calloc(num, size)  usb_sys_mem_malloc((num) * (size))
#define usb_free(ptr)          usb_sys_mem_free(ptr)
#define usb_align(align, size) usb_sys_malloc_align(align, size)

#ifndef CONFIG_USB_DBG_LEVEL
#if defined(CONFIG_LOG_ERROR)
#define CONFIG_USB_DBG_LEVEL USB_DBG_ERROR
#elif defined(CONFIG_LOG_WARN)
#define CONFIG_USB_DBG_LEVEL USB_DBG_WARNING
#elif defined(CONFIG_LOG_INFO)
#define CONFIG_USB_DBG_LEVEL USB_DBG_INFO
#elif defined(CONFIG_LOG_DEBUG) || defined(CONFIG_LOG_VERBOS)
#define CONFIG_USB_DBG_LEVEL USB_DBG_LOG
#elif defined(CONFIG_LOG_NONE)
#define CONFIG_USB_DBG_LEVEL USB_DBG_ERROR
#undef CONFIG_USB_PRINTF
#define CONFIG_USB_PRINTF(...)
#else
#define CONFIG_USB_DBG_LEVEL USB_DBG_ERROR
#endif
#endif

#define CONFIG_USBDEV_ADVANCE_DESC

/* Enable print with color */
#define CONFIG_USB_PRINTF_COLOR_ENABLE

/* data align size when use dma or use dcache */
#ifndef CONFIG_USB_ALIGN_SIZE
#define CONFIG_USB_ALIGN_SIZE 4
#endif

/* attribute data into no cache ram */
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".noncacheable")))

/* ================= USB Device Stack Configuration ================ */

/* ================ USB HOST Stack Configuration ================== */

#define CONFIG_USBHOST_MAX_RHPORTS          8
#define CONFIG_USBHOST_MAX_EXTHUBS          4
#define CONFIG_USBHOST_MAX_EHPORTS          8
#define CONFIG_USBHOST_MAX_INTERFACES       8
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 8
#define CONFIG_USBHOST_MAX_ENDPOINTS        8

#define CONFIG_USBHOST_MAX_CDC_ACM_CLASS 4
#define CONFIG_USBHOST_MAX_HID_CLASS     4
#define CONFIG_USBHOST_MAX_MSC_CLASS     2
#define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
#define CONFIG_USBHOST_MAX_VIDEO_CLASS   1

#define CONFIG_USBHOST_DEV_NAMELEN 16

#ifndef CONFIG_USBHOST_PSC_PRIO
#define CONFIG_USBHOST_PSC_PRIO 29
#endif
#ifndef CONFIG_USBHOST_PSC_STACKSIZE
#define CONFIG_USBHOST_PSC_STACKSIZE 8192
#endif

/* XHCI deferred interrupt thread priority (0 = highest, CherryUSB
 * convention; must match hub priority PSC_PRIO) */
#ifndef CONFIG_USB_XHCI_IRQ_PRIO
#define CONFIG_USB_XHCI_IRQ_PRIO 0
#endif

/* GIC interrupt priority: must be >= configMAX_API_CALL_INTERRUPT_PRIORITY
 * so xSemaphoreGiveFromISR from a hard ISR is well-defined */
#define USB_XHCI_GIC_IRQ_PRIO 13U
#ifdef configMAX_API_CALL_INTERRUPT_PRIORITY
_Static_assert(USB_XHCI_GIC_IRQ_PRIO >= configMAX_API_CALL_INTERRUPT_PRIORITY,
               "XHCI GIC irq priority must be >= configMAX_API_CALL_INTERRUPT_PRIORITY");
#endif

#ifndef CONFIG_USBHOST_MSOS_VENDOR_CODE
#define CONFIG_USBHOST_MSOS_VENDOR_CODE 0x00
#endif

/* Ep0 max transfer buffer */
#ifndef CONFIG_USBHOST_REQUEST_BUFFER_LEN
#define CONFIG_USBHOST_REQUEST_BUFFER_LEN 512
#endif

#ifndef CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
#define CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT 500
#endif

#ifndef CONFIG_USBHOST_MSC_TIMEOUT
#define CONFIG_USBHOST_MSC_TIMEOUT 0xffffffff
#endif

#ifndef CONFIG_INPUT_MOUSE_WHEEL
#define CONFIG_INPUT_MOUSE_WHEEL
#endif

/* ================ USB Device Port Configuration ================*/

/* ================ USB Host Port Configuration ==================*/
#ifndef CONFIG_USBHOST_MAX_BUS
#define CONFIG_USBHOST_MAX_BUS 4
#endif

#ifndef CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USBHOST_PIPE_NUM 10
#endif

/* ================ USB Dcache Configuration ==================*/

/* ================ USB Device Framework Configuration ============*/

#ifndef USB_HAVE_BUSDMA
#define USB_HAVE_BUSDMA 1
#endif
#define USB_HAVE_PER_BUS_PROCESS 1
#ifndef USB_HAVE_FIXED_IFACE
#define USB_HAVE_FIXED_IFACE 1
#endif
#ifndef USB_HAVE_FIXED_ENDPOINT
#define USB_HAVE_FIXED_ENDPOINT 1
#endif
#ifndef USB_HAVE_MALLOC_WAITOK
#define USB_HAVE_MALLOC_WAITOK 0
#endif
#ifndef USB_HAVE_UGEN
#define USB_HAVE_UGEN 0
#endif
#ifndef USB_HAVE_PF
#define USB_HAVE_PF 0
#endif
#ifndef USB_HAVE_POWERD
#define USB_HAVE_POWERD 0
#endif
#ifndef USB_HAVE_TT_SUPPORT
#define USB_HAVE_TT_SUPPORT 0
#endif

#endif
