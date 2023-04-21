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
 * Description:  This file is for the usb input functions.
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

#include "fassert.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"

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
static struct usbh_urb kbd_intin_urb;
static struct usbh_urb mouse_intin_urb;
static uint8_t kbd_buffer[128] __attribute__((aligned(sizeof(unsigned long)))) = {0};
static uint8_t mouse_buffer[128] __attribute__((aligned(sizeof(unsigned long)))) = {0};

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
    printf("<-\r\n");
}

static inline void UsbMouseRightButtonCB(void)
{
    printf("->\r\n");
}

static inline void UsbMouseMiddleButtonCB(void)
{
    printf("C\r\n");
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
    printf("[x:%d y:%d w:%d]\r\n", input->xdisp, input->ydisp, input->wdisp);
}

/* look up new key in previous keys */
static inline boolean FindKeyInPrevInput(const struct usb_hid_kbd_report *report, u8 keycode)
{
    for (u8 i = 0; i < 6; i++)
    {
        if (report->key[i] == keycode)
        {
            return TRUE;
        }
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

static void UsbKeyboardCallback(void *arg, int nbytes)
{
    u8 intf_protocol;
    struct usbh_hid *kbd_class = (struct usbh_hid *)arg;
    intf_protocol = UsbInputGetInterfaceProtocol(kbd_class);

    if (HID_PROTOCOL_KEYBOARD == intf_protocol) /* handle input from keyboard */
    {
        if (nbytes < (int)sizeof(struct usb_hid_kbd_report))
        {
            FUSB_ERROR("nbytes = %d", nbytes);
        }
        else
        {
            UsbKeyBoardHandleInput((struct usb_hid_kbd_report *)kbd_buffer);
        }
    }
    else
    {
        FUSB_ERROR("mismatch callback for protocol-%d", intf_protocol);
        return;
    }

    usbh_submit_urb(&kbd_intin_urb); /* ask for next inputs */
}

static void UsbMouseCallback(void *arg, int nbytes)
{
    u8 intf_protocol;
    struct usbh_hid *mouse_class = (struct usbh_hid *)arg;
    intf_protocol = UsbInputGetInterfaceProtocol(mouse_class);

    if (HID_PROTOCOL_MOUSE == intf_protocol) /* handle input from mouse */
    {
        if (nbytes < (int)sizeof(struct usb_hid_mouse_report))
        {
            FUSB_ERROR("nbytes = %d", nbytes);
        }
        else
        {
            UsbMouseHandleInput((struct usb_hid_mouse_report *)mouse_buffer);
        }        
    }
    else
    {
        FUSB_ERROR("mismatch callback for protocol-%d", intf_protocol);
        return;        
    }

    usbh_submit_urb(&mouse_intin_urb); /* ask for next inputs */
}

static void UsbKeyboardTask(void *args)
{
    int ret;
    struct usbh_hid *kbd_class;
    u8 intf_protocol;
    const char *dev_name = (const char *)args;
    
    kbd_class = (struct usbh_hid *)usbh_find_class_instance(dev_name);
    if (kbd_class == NULL)
    {
        FUSB_ERROR("Do not find %s.", dev_name);
        goto err_exit;
    }

    while (TRUE)
    {
        usbh_int_urb_fill(&kbd_intin_urb, kbd_class->intin, kbd_buffer, 8, 0, UsbKeyboardCallback, kbd_class);
        ret = usbh_submit_urb(&kbd_intin_urb);

        vTaskDelay(1);
    }

err_exit:
    vTaskDelete(NULL);
}

static void UsbMouseTask(void *args)
{
    int ret;
    struct usbh_hid *mouse_class;
    u8 intf_protocol;
    const char *dev_name = (const char *)args;
    
    mouse_class = (struct usbh_hid *)usbh_find_class_instance(dev_name);
    if (mouse_class == NULL)
    {
        FUSB_ERROR("Do not find %s.", dev_name);
        goto err_exit;
    }

    while (TRUE)
    {
        usbh_int_urb_fill(&mouse_intin_urb, mouse_class->intin, mouse_buffer, 8, 0, UsbMouseCallback, mouse_class);
        ret = usbh_submit_urb(&mouse_intin_urb);

        vTaskDelay(1);
    }

err_exit:
    vTaskDelete(NULL);
}

BaseType_t FFreeRTOSRunUsbKeyboard(const char *devname)
{
    BaseType_t ret = pdPASS;

    taskENTER_CRITICAL(); /* no schedule when create task */

    ret = xTaskCreate((TaskFunction_t)UsbKeyboardTask,
                      (const char *)"UsbKeyboardTask",
                      (uint16_t)2048,
                      (void *)devname,
                      (UBaseType_t)configMAX_PRIORITIES - 1,
                      NULL);
    FASSERT_MSG(pdPASS == ret, "create task failed");

    taskEXIT_CRITICAL(); /* allow schedule since task created */

    return ret;
}

BaseType_t FFreeRTOSRunUsbMouse(const char *devname)
{
    BaseType_t ret = pdPASS;

    taskENTER_CRITICAL(); /* no schedule when create task */

    ret = xTaskCreate((TaskFunction_t)UsbMouseTask,
                      (const char *)"UsbMouseTask",
                      (uint16_t)2048,
                      (void *)devname,
                      (UBaseType_t)configMAX_PRIORITIES - 1,
                      NULL);
    FASSERT_MSG(pdPASS == ret, "create task failed");

    taskEXIT_CRITICAL(); /* allow schedule since task created */

    return ret;
}