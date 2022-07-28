/**
 * @file usbd_cdc.c
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#include "usbd_core.h"
#include "usbd_cdc.h"

const char *stop_name[] = { "1", "1.5", "2" };
const char *parity_name[] = { "N", "O", "E", "M", "S" };

/* Device data structure */
struct usbd_cdc {
    struct cdc_line_coding line_coding;
    bool dtr;
    bool rts;
    uint8_t intf_num;
    usb_slist_t list;
};

static usb_slist_t usbd_cdc_head = USB_SLIST_OBJECT_INIT(usbd_cdc_head);

#ifndef CONFIG_USBDEV_CDC_ACM_UART
struct usbd_cdc g_usbd_cdc_acm_class;
static void usbd_cdc_acm_reset(void)
{
    memset(&g_usbd_cdc_acm_class, 0, sizeof(struct usbd_cdc));
    g_usbd_cdc_acm_class.line_coding.dwDTERate = 2000000;
    g_usbd_cdc_acm_class.line_coding.bDataBits = 8;
    g_usbd_cdc_acm_class.line_coding.bParityType = 0;
    g_usbd_cdc_acm_class.line_coding.bCharFormat = 0;
}
#endif
/**
 * @brief Handler called for Class requests not handled by the USB stack.
 *
 * @param setup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
static int cdc_acm_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("CDC Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    struct usbd_cdc *current_cdc_acm_class = NULL;
    uint8_t intf = LO_BYTE(setup->wIndex);
#ifdef CONFIG_USBDEV_CDC_ACM_UART
    usb_slist_t *i;

    usb_slist_for_each(i, &usbd_cdc_head)
    {
        struct usbd_cdc *cdc_acm_class = usb_slist_entry(i, struct usbd_cdc, list);
        if (cdc_acm_class->intf_num == intf) {
            current_cdc_acm_class = cdc_acm_class;
            break;
        }
    }

    if (current_cdc_acm_class == NULL) {
        return -2;
    }
#else
    current_cdc_acm_class = &g_usbd_cdc_acm_class;
#endif
    switch (setup->bRequest) {
        case CDC_REQUEST_SET_LINE_CODING:

            /*******************************************************************************/
            /* Line Coding Structure                                                       */
            /*-----------------------------------------------------------------------------*/
            /* Offset | Field       | Size | Value  | Description                          */
            /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
            /* 4      | bCharFormat |   1  | Number | Stop bits                            */
            /*                                        0 - 1 Stop bit                       */
            /*                                        1 - 1.5 Stop bits                    */
            /*                                        2 - 2 Stop bits                      */
            /* 5      | bParityType |  1   | Number | Parity                               */
            /*                                        0 - None                             */
            /*                                        1 - Odd                              */
            /*                                        2 - Even                             */
            /*                                        3 - Mark                             */
            /*                                        4 - Space                            */
            /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
            /*******************************************************************************/
            memcpy(&current_cdc_acm_class->line_coding, *data, setup->wLength);
            USB_LOG_DBG("Set intf:%d linecoding <%d %d %s %s>\r\n",
                        intf,
                        current_cdc_acm_class->line_coding.dwDTERate,
                        current_cdc_acm_class->line_coding.bDataBits,
                        parity_name[current_cdc_acm_class->line_coding.bParityType],
                        stop_name[current_cdc_acm_class->line_coding.bCharFormat]);
            usbd_cdc_acm_set_line_coding(intf, current_cdc_acm_class->line_coding.dwDTERate, current_cdc_acm_class->line_coding.bDataBits,
                                         current_cdc_acm_class->line_coding.bParityType, current_cdc_acm_class->line_coding.bCharFormat);
            break;

        case CDC_REQUEST_SET_CONTROL_LINE_STATE: {
            current_cdc_acm_class->dtr = (setup->wValue & 0x0001);
            current_cdc_acm_class->rts = (setup->wValue & 0x0002);
            USB_LOG_DBG("Set intf:%d DTR 0x%x,RTS 0x%x\r\n",
                        intf,
                        current_cdc_acm_class->dtr,
                        current_cdc_acm_class->rts);
            usbd_cdc_acm_set_dtr(intf, current_cdc_acm_class->dtr);
            usbd_cdc_acm_set_rts(intf, current_cdc_acm_class->rts);
        } break;

        case CDC_REQUEST_GET_LINE_CODING:
            *data = (uint8_t *)(&current_cdc_acm_class->line_coding);
            *len = 7;
            USB_LOG_DBG("Get intf:%d linecoding %d %d %d %d\r\n",
                        intf,
                        current_cdc_acm_class->line_coding.dwDTERate,
                        current_cdc_acm_class->line_coding.bCharFormat,
                        current_cdc_acm_class->line_coding.bParityType,
                        current_cdc_acm_class->line_coding.bDataBits);
            break;

        default:
            USB_LOG_WRN("Unhandled CDC Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void cdc_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
#ifndef CONFIG_USBDEV_CDC_ACM_UART
            usbd_cdc_acm_reset();
#endif
            break;
        case USBD_EVENT_CONFIGURED:
            usbd_cdc_acm_setup();
            break;

        default:
            break;
    }
}

int usbd_cdc_acm_alloc(uint8_t intf)
{
    struct usbd_cdc *cdc_acm_class = usb_malloc(sizeof(struct usbd_cdc));

    if (cdc_acm_class == NULL) {
        USB_LOG_ERR("no memory to alloc for cdc_acm_class\r\n");
        return -1;
    }
    memset(cdc_acm_class, 0, sizeof(struct usbd_cdc));
    cdc_acm_class->line_coding.dwDTERate = 2000000;
    cdc_acm_class->line_coding.bDataBits = 8;
    cdc_acm_class->line_coding.bParityType = 0;
    cdc_acm_class->line_coding.bCharFormat = 0;
    cdc_acm_class->intf_num = intf;

    usb_slist_add_tail(&usbd_cdc_head, &cdc_acm_class->list);
    return 0;
}

void usbd_cdc_add_acm_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = cdc_acm_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = cdc_notify_handler;
    usbd_class_add_interface(devclass, intf);
#ifdef CONFIG_USBDEV_CDC_ACM_UART
    usbd_cdc_acm_alloc(intf->intf_num);
#endif
}

__WEAK void usbd_cdc_acm_set_line_coding(uint8_t intf, uint32_t baudrate, uint8_t databits, uint8_t parity, uint8_t stopbits)
{
}

__WEAK void usbd_cdc_acm_set_dtr(uint8_t intf, bool dtr)
{
}

__WEAK void usbd_cdc_acm_set_rts(uint8_t intf, bool rts)
{
}

__WEAK void usbd_cdc_acm_setup(void)
{
}
