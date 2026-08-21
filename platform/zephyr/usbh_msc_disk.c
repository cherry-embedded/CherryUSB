/*
 * Copyright (c) 2025-2026 sakumisu
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

struct usbh_msc_disk_context {
    struct usbh_msc *msc_class;
    struct disk_info disk;
    char name[CONFIG_USBHOST_DEV_NAMELEN];
    char devname[CONFIG_USBHOST_DEV_NAMELEN];
};

static struct usbh_msc_disk_context
    msc_disk_context[CONFIG_USBHOST_MAX_MSC_CLASS];

static struct usbh_msc_disk_context *disk_msc_context(struct disk_info *disk)
{
    return CONTAINER_OF(disk, struct usbh_msc_disk_context, disk);
}

static bool disk_msc_connected(const struct usbh_msc *msc_class)
{
    return msc_class != NULL && msc_class->hport != NULL &&
           msc_class->hport->connected;
}

static int disk_msc_access_init(struct disk_info *disk)
{
    struct usbh_msc_disk_context *ctx = disk_msc_context(disk);

    ctx->msc_class = (struct usbh_msc *)usbh_find_class_instance(ctx->devname);
    if (ctx->msc_class == NULL) {
        printf("do not find %s\r\n", ctx->devname);
        return -ENODEV;
    }
    if (usbh_msc_scsi_init(ctx->msc_class) < 0) {
        return -EIO;
    }
    return 0;
}

static int disk_msc_access_status(struct disk_info *disk)
{
    struct usbh_msc_disk_context *ctx = disk_msc_context(disk);

    if (!disk_msc_connected(ctx->msc_class)) {
        return DISK_STATUS_NOMEDIA;
    }

    return DISK_STATUS_OK;
}

static int disk_msc_access_read(struct disk_info *disk, uint8_t *buff,
                                uint32_t sector, uint32_t count)
{
    int ret;
    uint8_t *align_buf;
    struct usbh_msc_disk_context *ctx = disk_msc_context(disk);

    if (!disk_msc_connected(ctx->msc_class)) {
        return -ENODEV;
    }

    align_buf = (uint8_t *)buff;
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        align_buf = (uint8_t *)k_aligned_alloc(CONFIG_USB_ALIGN_SIZE, count * ctx->msc_class->blocksize);
        if (!align_buf) {
            printf("msc get align buf failed\r\n");
            return -ENOMEM;
        }
    }
#endif
    if (usbh_msc_scsi_read10(ctx->msc_class, sector, align_buf, count) < 0) {
        ret = -EIO;
    } else {
        ret = 0;
    }
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        usb_memcpy(buff, align_buf, count * ctx->msc_class->blocksize);
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
    struct usbh_msc_disk_context *ctx = disk_msc_context(disk);

    if (!disk_msc_connected(ctx->msc_class)) {
        return -ENODEV;
    }

    align_buf = (uint8_t *)buff;
#ifdef CONFIG_DCACHE
    if ((uint32_t)buff & (CONFIG_USB_ALIGN_SIZE - 1)) {
        align_buf = (uint8_t *)k_aligned_alloc(CONFIG_USB_ALIGN_SIZE, count * ctx->msc_class->blocksize);
        if (!align_buf) {
            printf("msc get align buf failed\r\n");
            return -ENOMEM;
        }
        usb_memcpy(align_buf, buff, count * ctx->msc_class->blocksize);
    }
#endif
    if (usbh_msc_scsi_write10(ctx->msc_class, sector, align_buf, count) < 0) {
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
    struct usbh_msc_disk_context *ctx = disk_msc_context(disk);

    switch (cmd) {
        case DISK_IOCTL_CTRL_SYNC:
            break;
        case DISK_IOCTL_GET_SECTOR_COUNT:
            if (!disk_msc_connected(ctx->msc_class)) {
                return -ENODEV;
            }
            *(uint32_t *)buff = ctx->msc_class->blocknum;
            break;
        case DISK_IOCTL_GET_SECTOR_SIZE:
            if (!disk_msc_connected(ctx->msc_class)) {
                return -ENODEV;
            }
            *(uint32_t *)buff = ctx->msc_class->blocksize;
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

__WEAK void usbh_msc_app_run(struct usbh_msc *msc_class)
{
    (void)msc_class;
}

__WEAK void usbh_msc_app_stop(struct usbh_msc *msc_class)
{
    (void)msc_class;
}

void usbh_msc_run(struct usbh_msc *msc_class)
{
    uint8_t index;
    struct usbh_msc_disk_context *ctx;

    if (msc_class == NULL || msc_class->sdchar < 'a' ||
        msc_class->sdchar >= 'a' + CONFIG_USBHOST_MAX_MSC_CLASS) {
        return;
    }

    index = msc_class->sdchar - 'a';
    ctx = &msc_disk_context[index];
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->name, sizeof(ctx->name), "USB%u", index);
    snprintf(ctx->devname, sizeof(ctx->devname), "/dev/sd%c", msc_class->sdchar);
    ctx->msc_class = msc_class;
    ctx->disk.name = ctx->name;
    ctx->disk.ops = &msc_disk_ops;

    if (disk_access_register(&ctx->disk) < 0) {
        return;
    }

    usbh_msc_app_run(msc_class);
}

void usbh_msc_stop(struct usbh_msc *msc_class)
{
    uint8_t index;
    struct usbh_msc_disk_context *ctx;

    usbh_msc_app_stop(msc_class);

    if (msc_class == NULL || msc_class->sdchar < 'a' ||
        msc_class->sdchar >= 'a' + CONFIG_USBHOST_MAX_MSC_CLASS) {
        return;
    }

    index = msc_class->sdchar - 'a';
    ctx = &msc_disk_context[index];
    disk_access_unregister(&ctx->disk);
    ctx->msc_class = NULL;
}
