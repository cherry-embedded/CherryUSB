/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ff.h"
#include "diskio.h"
#include "usbh_core.h"
#include "usbh_msc.h"

struct usbh_msc *active_msc_class;

int USB_disk_initialize(void)
{
    active_msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
    if (active_msc_class == NULL) {
        printf("do not find /dev/sda\r\n");
        return RES_NOTRDY;
    }
    if (usbh_msc_scsi_init(active_msc_class) < 0) {
        return RES_NOTRDY;
    }
    return RES_OK;
}

int USB_disk_status(void)
{
    return RES_OK;
}

int USB_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
    int ret;
    uint8_t *align_buf;

    align_buf = (uint8_t *)buff;
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        align_buf = (uint8_t *)aligned_alloc(CONFIG_USB_ALIGN_SIZE, count * active_msc_class->blocksize);
        if (!align_buf) {
            printf("msc get align buf failed\r\n");
            return -USB_ERR_NOMEM;
        }
    }
#endif
    ret = usbh_msc_scsi_read10(active_msc_class, sector, align_buf, count);
    if (ret < 0) {
        ret = RES_ERROR;
    } else {
        ret = RES_OK;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        usb_memcpy(buff, align_buf, count * active_msc_class->blocksize);
        free(align_buf);
    }
#endif
    return ret;
}

int USB_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
    int ret;
    uint8_t *align_buf;

    align_buf = (uint8_t *)buff;
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        align_buf = (uint8_t *)aligned_alloc(CONFIG_USB_ALIGN_SIZE, count * active_msc_class->blocksize);
        if (!align_buf) {
            printf("msc get align buf failed\r\n");
            return -USB_ERR_NOMEM;
        }
        usb_memcpy(align_buf, buff, count * active_msc_class->blocksize);
    }
#endif
    ret = usbh_msc_scsi_write10(active_msc_class, sector, align_buf, count);
    if (ret < 0) {
        ret = RES_ERROR;
    } else {
        ret = RES_OK;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        free(align_buf);
    }
#endif
    return ret;
}

int USB_disk_ioctl(BYTE cmd, void *buff)
{
    int result = 0;

    switch (cmd) {
        case CTRL_SYNC:
            result = RES_OK;
            break;

        case GET_SECTOR_SIZE:
            *(WORD *)buff = active_msc_class->blocksize;
            result = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;
            result = RES_OK;
            break;

        case GET_SECTOR_COUNT:
            *(DWORD *)buff = active_msc_class->blocknum;
            result = RES_OK;
            break;

        default:
            result = RES_PARERR;
            break;
    }

    return result;
}
