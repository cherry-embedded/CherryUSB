/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <nuttx/fs/fs.h>

#include "usbd_core.h"
#include "usbd_msc.h"

#ifndef CONFIG_USBDEV_MSC_THREAD
#error "CONFIG_USBDEV_MSC_THREAD must be enabled"
#endif

static FAR struct inode *inode;
static struct geometry geo;
static char devpath[32];

void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
{
    int ret;

    /* Open the block driver */

    ret = open_blockdriver(devpath, 0, &inode);
    if (ret < 0) {
        *block_num = 0;
        *block_size = 0;
        return;
    }

    /* Get the drive geometry */

    if (!inode || !inode->u.i_bops || !inode->u.i_bops->geometry ||
        inode->u.i_bops->geometry(inode, &geo) != OK || !geo.geo_available) {
        *block_num = 0;
        *block_size = 0;
        return;
    }

    *block_num = geo.geo_nsectors;
    *block_size = geo.geo_sectorsize;

    USB_LOG_INFO("block_num: %ld, block_size: %ld\n", *block_num, *block_size);
}

int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if (inode->u.i_bops->read) {
        inode->u.i_bops->read(inode, buffer, sector, length / geo.geo_sectorsize);
    }

    return 0;
}

int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
{
    if (inode->u.i_bops->write) {
        inode->u.i_bops->write(inode, buffer, sector, length / geo.geo_sectorsize);
    }

    return 0;
}

static struct usbd_interface intf0;

void usbd_msc_init(uint8_t busid, char *path, uint8_t outep, uint8_t inep)
{
    memset(devpath, 0, sizeof(devpath));
    strncpy(devpath, path, sizeof(devpath) - 1);
    usbd_add_interface(busid, usbd_msc_init_intf(busid, &intf0, outep, inep));
}