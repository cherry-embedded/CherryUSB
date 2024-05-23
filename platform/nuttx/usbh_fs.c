/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <nuttx/config.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/signal.h>
#include <nuttx/arch.h>
#include <nuttx/wqueue.h>
#include <nuttx/scsi.h>
#include <nuttx/fs/fs.h>
#include <nuttx/mutex.h>

#include "usbh_core.h"
#include "usbh_msc.h"

#ifdef CONFIG_ARCH_CHIP_HPMICRO
#include "hpm_misc.h"
#define usbhmsc_phy2sysaddr(a) core_local_mem_to_sys_address(0, a)
#else
#define usbhmsc_phy2sysaddr(a) (a)
#endif

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

    if (msc_class->hport && msc_class->hport->connected) {
        return OK;
    } else {
        return -ENODEV;
    }
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

    if (msc_class->hport && msc_class->hport->connected) {
        ret = usbh_msc_scsi_read10(msc_class, startsector, (uint8_t *)usbhmsc_phy2sysaddr((uint32_t)buffer), nsectors);
        if (ret < 0) {
            return ret;
        } else {
#ifdef CONFIG_USBHOST_MSC_DCACHE
            up_invalidate_dcache((uintptr_t)buffer, (uintptr_t)(buffer + nsectors * msc_class->blocksize));
#endif
            return nsectors;
        }
    } else {
        return -ENODEV;
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

    if (msc_class->hport && msc_class->hport->connected) {
#ifdef CONFIG_USBHOST_MSC_DCACHE
        up_flush_dcache((uintptr_t)buffer, (uintptr_t)(buffer + nsectors * msc_class->blocksize));
#endif
        ret = usbh_msc_scsi_write10(msc_class, startsector, (uint8_t *)usbhmsc_phy2sysaddr((uint32_t)buffer), nsectors);
        if (ret < 0) {
            return ret;
        } else {
            return nsectors;
        }
    } else {
        return -ENODEV;
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

        uinfo("nsectors: %" PRIdOFF " sectorsize: %" PRIi16 "\n",
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

#define DEV_FORMAT "/dev/sd%c"

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