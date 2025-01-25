/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <nuttx/fs/fs.h>

#include "usbh_core.h"
#include "usbh_msc.h"

#ifndef CONFIG_FS_FAT
#error "CONFIG_FS_FAT must be enabled"
#endif

#ifdef CONFIG_ARCH_DCACHE
#ifndef CONFIG_FAT_DMAMEMORY
#error "USBH MSC requires CONFIG_FAT_DMAMEMORY"
#endif
#endif

#define DEV_FORMAT "/dev/sd%c"

static int nuttx_errorcode(int error)
{
    int err = 0;

    switch (error) {
        case -USB_ERR_NOMEM:
            err = -EIO;
            break;
        case -USB_ERR_INVAL:
            err = -EINVAL;
            break;
        case -USB_ERR_NODEV:
            err = -ENODEV;
            break;
        case -USB_ERR_NOTCONN:
            err = -ENOTCONN;
            break;
        case -USB_ERR_NOTSUPP:
            err = -EIO;
            break;
        case -USB_ERR_BUSY:
            err = -EBUSY;
            break;
        case -USB_ERR_RANGE:
            err = -ERANGE;
            break;
        case -USB_ERR_STALL:
            err = -EPERM;
            break;
        case -USB_ERR_NAK:
            err = -EAGAIN;
            break;
        case -USB_ERR_DT:
            err = -EIO;
            break;
        case -USB_ERR_IO:
            err = -EIO;
            break;
        case -USB_ERR_SHUTDOWN:
            err = -ESHUTDOWN;
            break;
        case -USB_ERR_TIMEOUT:
            err = -ETIMEDOUT;
            break;

        default:
            break;
    }
    return err;
}

static int usbhost_open(FAR struct inode *inode);
static int usbhost_close(FAR struct inode *inode);
static ssize_t usbhost_read(FAR struct inode *inode, unsigned char *buffer,
                            blkcnt_t startsector, unsigned int nsectors);
static ssize_t usbhost_write(FAR struct inode *inode,
                             FAR const unsigned char *buffer,
                             blkcnt_t startsector, unsigned int nsectors);
static int usbhost_geometry(FAR struct inode *inode,
                            FAR struct geometry *geometry);
static int usbhost_ioctl(FAR struct inode *inode, int cmd, unsigned long arg);
/* Block driver operations.  This is the interface exposed to NuttX by the
 * class that permits it to behave like a block driver.
 */

static const struct block_operations g_bops = {
    usbhost_open,     /* open     */
    usbhost_close,    /* close    */
    usbhost_read,     /* read     */
    usbhost_write,    /* write    */
    usbhost_geometry, /* geometry */
    usbhost_ioctl     /* ioctl    */
};

static int usbhost_open(FAR struct inode *inode)
{
    struct usbh_msc *msc_class;

    DEBUGASSERT(inode->i_private);
    msc_class = (struct usbh_msc *)inode->i_private;

    if (usbh_msc_scsi_init(msc_class) < 0) {
        return -ENODEV;
    }

    return OK;
}

static int usbhost_close(FAR struct inode *inode)
{
    DEBUGASSERT(inode->i_private);
    return 0;
}

static ssize_t usbhost_read(FAR struct inode *inode, unsigned char *buffer,
                            blkcnt_t startsector, unsigned int nsectors)
{
    struct usbh_msc *msc_class;
    int ret;

    DEBUGASSERT(inode->i_private);
    msc_class = (struct usbh_msc *)inode->i_private;

    ret = usbh_msc_scsi_read10(msc_class, startsector, (uint8_t *)buffer, nsectors);
    if (ret < 0) {
        return nuttx_errorcode(ret);
    } else {
#if defined(CONFIG_ARCH_DCACHE) && !defined(CONFIG_USB_DCACHE_ENABLE)
        up_invalidate_dcache((uintptr_t)buffer, (uintptr_t)(buffer + nsectors * msc_class->blocksize));
#endif
        return nsectors;
    }
}

static ssize_t usbhost_write(FAR struct inode *inode,
                             FAR const unsigned char *buffer,
                             blkcnt_t startsector, unsigned int nsectors)
{
    struct usbh_msc *msc_class;
    int ret;

    DEBUGASSERT(inode->i_private);
    msc_class = (struct usbh_msc *)inode->i_private;

#if defined(CONFIG_ARCH_DCACHE) && !defined(CONFIG_USB_DCACHE_ENABLE)
    up_clean_dcache((uintptr_t)buffer, (uintptr_t)(buffer + nsectors * msc_class->blocksize));
#endif
    ret = usbh_msc_scsi_write10(msc_class, startsector, (uint8_t *)buffer, nsectors);
    if (ret < 0) {
        return nuttx_errorcode(ret);
    } else {
        return nsectors;
    }
}

static int usbhost_geometry(FAR struct inode *inode,
                            FAR struct geometry *geometry)
{
    struct usbh_msc *msc_class;

    DEBUGASSERT(inode->i_private);
    msc_class = (struct usbh_msc *)inode->i_private;

    if (msc_class->hport && msc_class->hport->connected) {
        memset(geometry, 0, sizeof(*geometry));

        geometry->geo_available = true;
        geometry->geo_mediachanged = false;
        geometry->geo_writeenabled = true;
        geometry->geo_nsectors = msc_class->blocknum;
        geometry->geo_sectorsize = msc_class->blocksize;

        USB_LOG_DBG("nsectors: %ld, sectorsize: %ld\n",
              geometry->geo_nsectors, geometry->geo_sectorsize);
        return OK;
    } else {
        return -ENODEV;
    }

    return 0;
}

static int usbhost_ioctl(FAR struct inode *inode, int cmd, unsigned long arg)
{
    struct usbh_msc *msc_class;

    DEBUGASSERT(inode->i_private);
    msc_class = (struct usbh_msc *)inode->i_private;

    if (msc_class->hport && msc_class->hport->connected) {
        return -ENOTTY;
    } else {
        return -ENODEV;
    }

    return 0;
}

void usbh_msc_run(struct usbh_msc *msc_class)
{
    char devname[32];

    snprintf(devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, msc_class->sdchar);

    register_blockdriver(devname, &g_bops, 0, msc_class);
}

void usbh_msc_stop(struct usbh_msc *msc_class)
{
    char devname[32];

    snprintf(devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, msc_class->sdchar);

    unregister_blockdriver(devname);
}