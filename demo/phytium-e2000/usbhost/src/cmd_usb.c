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
 * FilePath: cmd_usb.c
 * Date: 2022-09-19 14:34:44
 * LastEditTime: 2022-09-19 14:34:45
 * Description:  This file is for USB shell command.
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2022/9/20  init commit
 */
#include <string.h>
#include <stdio.h>
#include "strto.h"
#include "sdkconfig.h"

#include "FreeRTOS.h"

#include "../src/shell.h"
#include "usb_host.h"

static int USBCmdEntry(int argc, char *argv[])
{
    int ret = 0;
    u32 usb_id = 0;
    const char *devname;

    if (!strcmp(argv[1], "init"))
    {
        if (argc < 3) {
            return -2;
        }

        usb_id = (uint8_t)simple_strtoul(argv[2], NULL, 10);
        ret = FFreeRTOSInitUsb(usb_id);
    }
    else if (!strcmp(argv[1], "lsusb"))
    {
        ret = FFreeRTOSListUsbDev(argc - 1, &argv[1]);
    }
    else if (!strcmp(argv[1], "disk"))
    {
        if (argc < 3) {
            return -2;
        }

        devname = argv[2];
        ret = FFreeRTOSRunUsbDisk(devname);
    }
    else if (!strcmp(argv[1], "kbd"))
    {
        if (argc < 3) {
            return -2;
        }

        devname = argv[2];
        ret = FFreeRTOSRunUsbKeyboard(devname);
    }
    else if (!strcmp(argv[1], "mouse"))
    {
        if (argc < 3) {
            return -2;
        }

        devname = argv[2];
        ret = FFreeRTOSRunUsbMouse(devname);
    }

    return ret;
}
SHELL_EXPORT_CMD(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), usb, USBCmdEntry, test freertos usb driver);