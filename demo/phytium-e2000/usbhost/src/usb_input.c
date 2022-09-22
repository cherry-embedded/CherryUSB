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
 * FilePath: usb_input.c
 * Date: 2022-07-22 13:57:42
 * LastEditTime: 2022-07-22 13:57:43
 * Description:  This files is for 
 * 
 * Modify History: 
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/20  init commit
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
#include "usbh_hid.h"
/************************** Constant Definitions *****************************/
#define HID_KEYCODE_TO_ASCII    \
    {0     , 0      }, /* 0x00 */ \
    {0     , 0      }, /* 0x01 */ \
    {0     , 0      }, /* 0x02 */ \
    {0     , 0      }, /* 0x03 */ \
    {'a'   , 'A'    }, /* 0x04 */ \
    {'b'   , 'B'    }, /* 0x05 */ \
    {'c'   , 'C'    }, /* 0x06 */ \
    {'d'   , 'D'    }, /* 0x07 */ \
    {'e'   , 'E'    }, /* 0x08 */ \
    {'f'   , 'F'    }, /* 0x09 */ \
    {'g'   , 'G'    }, /* 0x0a */ \
    {'h'   , 'H'    }, /* 0x0b */ \
    {'i'   , 'I'    }, /* 0x0c */ \
    {'j'   , 'J'    }, /* 0x0d */ \
    {'k'   , 'K'    }, /* 0x0e */ \
    {'l'   , 'L'    }, /* 0x0f */ \
    {'m'   , 'M'    }, /* 0x10 */ \
    {'n'   , 'N'    }, /* 0x11 */ \
    {'o'   , 'O'    }, /* 0x12 */ \
    {'p'   , 'P'    }, /* 0x13 */ \
    {'q'   , 'Q'    }, /* 0x14 */ \
    {'r'   , 'R'    }, /* 0x15 */ \
    {'s'   , 'S'    }, /* 0x16 */ \
    {'t'   , 'T'    }, /* 0x17 */ \
    {'u'   , 'U'    }, /* 0x18 */ \
    {'v'   , 'V'    }, /* 0x19 */ \
    {'w'   , 'W'    }, /* 0x1a */ \
    {'x'   , 'X'    }, /* 0x1b */ \
    {'y'   , 'Y'    }, /* 0x1c */ \
    {'z'   , 'Z'    }, /* 0x1d */ \
    {'1'   , '!'    }, /* 0x1e */ \
    {'2'   , '@'    }, /* 0x1f */ \
    {'3'   , '#'    }, /* 0x20 */ \
    {'4'   , '$'    }, /* 0x21 */ \
    {'5'   , '%'    }, /* 0x22 */ \
    {'6'   , '^'    }, /* 0x23 */ \
    {'7'   , '&'    }, /* 0x24 */ \
    {'8'   , '*'    }, /* 0x25 */ \
    {'9'   , '('    }, /* 0x26 */ \
    {'0'   , ')'    }, /* 0x27 */ \
    {'\r'  , '\r'   }, /* 0x28 */ \
    {'\x1b', '\x1b' }, /* 0x29 */ \
    {'\b'  , '\b'   }, /* 0x2a */ \
    {'\t'  , '\t'   }, /* 0x2b */ \
    {' '   , ' '    }, /* 0x2c */ \
    {'-'   , '_'    }, /* 0x2d */ \
    {'='   , '+'    }, /* 0x2e */ \
    {'['   , '{'    }, /* 0x2f */ \
    {']'   , '}'    }, /* 0x30 */ \
    {'\\'  , '|'    }, /* 0x31 */ \
    {'#'   , '~'    }, /* 0x32 */ \
    {';'   , ':'    }, /* 0x33 */ \
    {'\''  , '\"'   }, /* 0x34 */ \
    {'`'   , '~'    }, /* 0x35 */ \
    {','   , '<'    }, /* 0x36 */ \
    {'.'   , '>'    }, /* 0x37 */ \
    {'/'   , '?'    }, /* 0x38 */ \
                                  \
    {0     , 0      }, /* 0x39 */ \
    {0     , 0      }, /* 0x3a */ \
    {0     , 0      }, /* 0x3b */ \
    {0     , 0      }, /* 0x3c */ \
    {0     , 0      }, /* 0x3d */ \
    {0     , 0      }, /* 0x3e */ \
    {0     , 0      }, /* 0x3f */ \
    {0     , 0      }, /* 0x40 */ \
    {0     , 0      }, /* 0x41 */ \
    {0     , 0      }, /* 0x42 */ \
    {0     , 0      }, /* 0x43 */ \
    {0     , 0      }, /* 0x44 */ \
    {0     , 0      }, /* 0x45 */ \
    {0     , 0      }, /* 0x46 */ \
    {0     , 0      }, /* 0x47 */ \
    {0     , 0      }, /* 0x48 */ \
    {0     , 0      }, /* 0x49 */ \
    {0     , 0      }, /* 0x4a */ \
    {0     , 0      }, /* 0x4b */ \
    {0     , 0      }, /* 0x4c */ \
    {0     , 0      }, /* 0x4d */ \
    {0     , 0      }, /* 0x4e */ \
    {0     , 0      }, /* 0x4f */ \
    {0     , 0      }, /* 0x50 */ \
    {0     , 0      }, /* 0x51 */ \
    {0     , 0      }, /* 0x52 */ \
    {0     , 0      }, /* 0x53 */ \
                                  \
    {'/'   , '/'    }, /* 0x54 */ \
    {'*'   , '*'    }, /* 0x55 */ \
    {'-'   , '-'    }, /* 0x56 */ \
    {'+'   , '+'    }, /* 0x57 */ \
    {'\r'  , '\r'   }, /* 0x58 */ \
    {'1'   , 0      }, /* 0x59 */ \
    {'2'   , 0      }, /* 0x5a */ \
    {'3'   , 0      }, /* 0x5b */ \
    {'4'   , 0      }, /* 0x5c */ \
    {'5'   , '5'    }, /* 0x5d */ \
    {'6'   , 0      }, /* 0x5e */ \
    {'7'   , 0      }, /* 0x5f */ \
    {'8'   , 0      }, /* 0x60 */ \
    {'9'   , 0      }, /* 0x61 */ \
    {'0'   , 0      }, /* 0x62 */ \
    {'0'   , 0      }, /* 0x63 */ \
    {'='   , '='    }, /* 0x67 */ \

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static u8 const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };

/***************** Macros (Inline Functions) Definitions *********************/
#define FUSB_DEBUG_TAG "USB-INPUT"
#define FUSB_ERROR(format, ...) FT_DEBUG_PRINT_E(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_WARN(format, ...)  FT_DEBUG_PRINT_W(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_INFO(format, ...)  FT_DEBUG_PRINT_I(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)
#define FUSB_DEBUG(format, ...) FT_DEBUG_PRINT_D(FUSB_DEBUG_TAG, format, ##__VA_ARGS__)

/************************** Function Prototypes ******************************/


/*****************************************************************************/
static inline void UsbMouseLeftButtonCB(void)
{
    printf("    Button Left\r\n");
}

static inline void UsbMouseRightButtonCB(void)
{
    printf("    Button Right\r\n");
}

static inline void UsbMouseMiddleButtonCB(void)
{
    printf("    Button Middle\r\n");
}

static void UsbMouseHandleInput(struct usb_hid_mouse_report *input)
{
    /*------------- button state  -------------*/
    if (input->buttons & HID_MOUSE_INPUT_BUTTON_LEFT)
    {
        UsbMouseLeftButtonCB();
    }

    if (input->buttons & HID_MOUSE_INPUT_BUTTON_MIDDLE)
    {
        UsbMouseMiddleButtonCB();
    }

    if (input->buttons & HID_MOUSE_INPUT_BUTTON_RIGHT)
    {
        UsbMouseRightButtonCB();
    }

    /*------------- cursor movement -------------*/
    printf("    Cursor %d %d %d\r\n", input->xdisp, input->ydisp, input->wdisp);
}

/* look up new key in previous keys */
static inline boolean FindKeyInPrevInput(const struct usb_hid_kbd_report *report, u8 keycode)
{
    for(u8 i=0; i < 6; i++)
    {
        if (report->key[i] == keycode)  
            return TRUE;
    }

    return FALSE;
}

static void UsbKeyBoardHandleInput(struct usb_hid_kbd_report *input)
{
    static struct usb_hid_kbd_report prev_input = { 0, 0, {0} }; /* previous report to check key released */

    /* ------------- example code ignore control (non-printable) key affects ------------- */
    for (u8 i = 0; i < 6; i++)
    {
        if (input->key[i])
        {
            if (FindKeyInPrevInput(&prev_input, input->key[i]))
            {
                /* exist in previous report means the current key is holding */
            }
            else
            {
                /* not existed in previous report means the current key is pressed */
                boolean is_shift = input->modifier & (HID_MODIFER_LSHIFT | HID_MODIFER_RSHIFT);
                u8 ch = keycode2ascii[input->key[i]][is_shift ? 1 : 0];
                putchar(ch);
                if ('\r' == ch)
                {
                    putchar('\n');
                }

                fflush(stdout); /* flush right away, else libc will wait for newline */
            }
        }
    }

    prev_input = *input;
}

static u8 UsbInputGetInterfaceProtocol(struct usbh_hid *hid_class)
{
    struct usbh_hubport *hport = hid_class->hport;
    u8 intf = hid_class->intf;
    const struct usb_interface_descriptor *intf_desc = &hport->config.intf->altsetting[intf].intf_desc;

    return intf_desc->bInterfaceProtocol;
}

static void UsbInputTask(void * args)
{
    int ret;
    struct usbh_hid *hid_class;
    static uint8_t hid_buffer[128] = {0};
    u8 intf_protocol;

    FUSB_INFO("%s", __func__);

    while (TRUE)
    {
        hid_class = (struct usbh_hid *)usbh_find_class_instance("/dev/input0");
        if (hid_class == NULL) 
        {
            USB_LOG_RAW("do not find /dev/input0\r\n");
            goto err_exit;
        }

        ret = usbh_int_transfer(hid_class->intin, hid_buffer, 8, 1000);
        if (ret < 0) 
        {
            USB_LOG_RAW("intr in error,ret:%d\r\n", ret);
        }

        intf_protocol = UsbInputGetInterfaceProtocol(hid_class);
        if (HID_PROTOCOL_KEYBOARD == intf_protocol)
        {
            UsbKeyBoardHandleInput((struct usb_hid_kbd_report *)hid_buffer);
        }
        else if (HID_PROTOCOL_MOUSE == intf_protocol)
        {
            UsbMouseHandleInput((struct usb_hid_mouse_report *)hid_buffer);            
        }
        else
        {
            FUSB_ERROR("unsupported hid interface %d", intf_protocol);
            goto err_exit;
        }

        vTaskDelay(10);
    }

err_exit:
    vTaskDelete(NULL);
}

BaseType_t FFreeRTOSRecvInput(void)
{
    BaseType_t ret = pdPASS;

    taskENTER_CRITICAL(); /* no schedule when create task */

    ret = xTaskCreate((TaskFunction_t )UsbInputTask,
                        (const char* )"UsbInputTask",
                        (uint16_t )2048,
                        NULL,
                        (UBaseType_t )configMAX_PRIORITIES - 1,
                        NULL);
    FASSERT_MSG(pdPASS == ret, "create task failed");

    taskEXIT_CRITICAL(); /* allow schedule since task created */

    return ret;      
}