/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/drivers/disk.h>
#include "usbh_core.h"
#include "usbh_msc.h"

#ifdef CONFIG_DCACHE
#ifndef CONFIG_USB_DCACHE_ENABLE
#error CONFIG_USB_DCACHE_ENABLE must be enabled to use msc disk
#endif
#endif

struct usbh_msc *active_msc_class;

static int disk_msc_access_init(struct disk_info *disk)
{
    active_msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
    if (active_msc_class == NULL) {
        printf("do not find /dev/sda\r\n");
        return -ENODEV;
    }
    if (usbh_msc_scsi_init(active_msc_class) < 0) {
        return -EIO;
    }
    return 0;
}

static int disk_msc_access_status(struct disk_info *disk)
{
    return DISK_STATUS_OK;
}

static int disk_msc_access_read(struct disk_info *disk, uint8_t *buff,
                                uint32_t sector, uint32_t count)
{
    int ret;
    uint8_t *align_buf;

    align_buf = (uint8_t *)buff;
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        align_buf = (uint8_t *)k_aligned_alloc(CONFIG_USB_ALIGN_SIZE, count * active_msc_class->blocksize);
        if (!align_buf) {
            printf("msc get align buf failed\r\n");
            return -ENOMEM;
        }
    }
#endif
    if (usbh_msc_scsi_read10(active_msc_class, sector, align_buf, count) < 0) {
        ret = -EIO;
    } else {
        ret = 0;
    }
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        usb_memcpy(buff, align_buf, count * active_msc_class->blocksize);
        k_free(align_buf);
    }
#endif
    return ret;
}

static int disk_msc_access_write(struct disk_info *disk, const uint8_t *buff,
                                 uint32_t sector, uint32_t count)
{
    int ret;
    uint8_t *align_buf;

    align_buf = (uint8_t *)buff;
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        align_buf = (uint8_t *)k_aligned_alloc(CONFIG_USB_ALIGN_SIZE, count * active_msc_class->blocksize);
        if (!align_buf) {
            printf("msc get align buf failed\r\n");
            return -ENOMEM;
        }
        usb_memcpy(align_buf, buff, count * active_msc_class->blocksize);
    }
#endif
    if (usbh_msc_scsi_write10(active_msc_class, sector, align_buf, count) < 0) {
        ret = -EIO;
    } else {
        ret = 0;
    }
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        k_free(align_buf);
    }
#endif
    return ret;
}

static int disk_msc_access_ioctl(struct disk_info *disk, uint8_t cmd, void *buff)
{
    switch (cmd) {
        case DISK_IOCTL_CTRL_SYNC:
            break;
        case DISK_IOCTL_GET_SECTOR_COUNT:
            *(uint32_t *)buff = active_msc_class->blocknum;
            break;
        case DISK_IOCTL_GET_SECTOR_SIZE:
            *(uint32_t *)buff = active_msc_class->blocksize;
            break;
        case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
            *(uint32_t *)buff = 1U;
            break;
        case DISK_IOCTL_CTRL_INIT:
            return disk_msc_access_init(disk);
        case DISK_IOCTL_CTRL_DEINIT:
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static const struct disk_operations msc_disk_ops = {
    .init = disk_msc_access_init,
    .status = disk_msc_access_status,
    .read = disk_msc_access_read,
    .write = disk_msc_access_write,
    .ioctl = disk_msc_access_ioctl,
};

static struct disk_info usbh_msc_disk = {
    .name = "USB",
    .ops = &msc_disk_ops,
};

void usbh_msc_run(struct usbh_msc *msc_class)
{
    disk_access_register(&usbh_msc_disk);
}

void usbh_msc_stop(struct usbh_msc *msc_class)
{
    disk_access_unregister(&usbh_msc_disk);
}