/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2011-12-12     Yi Qiu      first version
 */

#include <rtthread.h>
#include <dfs_fs.h>

#include "usbh_core.h"
#include "usbh_msc.h"

#define MAX_PARTITION_COUNT 5
#define CONFIG_DFS_MOUNT_POINT "/"

struct ustor_data {
    struct dfs_partition part;
    struct usbh_msc *msc_class;
    int udisk_id;
    const char path;
};

struct ustor {
    rt_uint32_t capicity[2];

    struct rt_device dev[MAX_PARTITION_COUNT];
    rt_uint8_t dev_cnt;
};
typedef struct ustor *ustor_t;

#define UDISK_MAX_COUNT 8
static rt_uint8_t _udisk_idset = 0;

ustor_t stor_r;

static int udisk_get_id(void)
{
    int i;

    for (i = 0; i < UDISK_MAX_COUNT; i++) {
        if ((_udisk_idset & (1 << i)) != 0)
            continue;
        else
            break;
    }

    /* it should not happen */
    if (i == UDISK_MAX_COUNT)
        RT_ASSERT(0);

    _udisk_idset |= (1 << i);
    return i;
}

static void udisk_free_id(int id)
{
    RT_ASSERT(id < UDISK_MAX_COUNT)

    _udisk_idset &= ~(1 << id);
}

/**
 * This function will initialize the udisk device
 *
 * @param dev the pointer of device driver structure
 *
 * @return RT_EOK
 */
static rt_err_t rt_udisk_init(rt_device_t dev)
{
    return RT_EOK;
}

/**
 * This function will read some data from a device.
 *
 * @param dev the pointer of device driver structure
 * @param pos the position of reading
 * @param buffer the data buffer to save read data
 * @param size the size of buffer
 *
 * @return the actually read size on successful, otherwise negative returned.
 */
static rt_size_t rt_udisk_read(rt_device_t dev, rt_off_t pos, void *buffer,
                               rt_size_t size)
{
    rt_err_t ret;
    struct usbh_msc *msc_class;
    struct ustor_data *data;

    /* check parameter */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(buffer != RT_NULL);

    data = (struct ustor_data *)dev->user_data;
    msc_class = data->msc_class;

    ret = usbh_msc_scsi_read10(msc_class, pos, (rt_uint8_t *)buffer, size);

    if (ret != RT_EOK) {
        rt_kprintf("usb mass_storage read failed\n");
        return 0;
    }

    return size;
}

/**
 * This function will write some data to a device.
 *
 * @param dev the pointer of device driver structure
 * @param pos the position of written
 * @param buffer the data buffer to be written to device
 * @param size the size of buffer
 *
 * @return the actually written size on successful, otherwise negative returned.
 */
static rt_size_t rt_udisk_write(rt_device_t dev, rt_off_t pos, const void *buffer,
                                rt_size_t size)
{
    rt_err_t ret;
    struct usbh_msc *msc_class;
    struct ustor_data *data;

    /* check parameter */
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(buffer != RT_NULL);

    data = (struct ustor_data *)dev->user_data;
    msc_class = data->msc_class;

    ret = usbh_msc_scsi_write10(msc_class, pos, (rt_uint8_t *)buffer, size);
    if (ret != RT_EOK) {
        rt_kprintf("usb mass_storage write %d sector failed\n", size);
        return 0;
    }

    return size;
}

/**
 * This function will execute SCSI_INQUIRY_CMD command to get inquiry data.
 *
 * @param intf the interface instance.
 * @param buffer the data buffer to save inquiry data
 *
 * @return the error code, RT_EOK on successfully.
 */
static rt_err_t rt_udisk_control(rt_device_t dev, int cmd, void *args)
{
    struct ustor_data *data;

    /* check parameter */
    RT_ASSERT(dev != RT_NULL);

    data = (struct ustor_data *)dev->user_data;

    if (cmd == RT_DEVICE_CTRL_BLK_GETGEOME) {
        struct rt_device_blk_geometry *geometry;

        geometry = (struct rt_device_blk_geometry *)args;
        if (geometry == RT_NULL)
            return -RT_ERROR;

        geometry->bytes_per_sector = SECTOR_SIZE;
        geometry->block_size = stor_r->capicity[1];
        geometry->sector_count = stor_r->capicity[0];
    }

    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops udisk_device_ops = {
    rt_udisk_init,
    RT_NULL,
    RT_NULL,
    rt_udisk_read,
    rt_udisk_write,
    rt_udisk_control
};
#endif

/**
 * This function will run udisk driver when usb disk is detected.
 *
 * @param intf the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
rt_err_t rt_udisk_run(struct usbh_msc *msc_class)
{
    int i = 0;
    rt_err_t ret;
    char dname[8];
    char sname[8];
    rt_uint8_t max_lun, *sector, sense[18], inquiry[36];
    struct dfs_partition part[MAX_PARTITION_COUNT];

    /* check parameter */
    RT_ASSERT(msc_class != RT_NULL);
    stor_r = (struct ustor *)rt_malloc(sizeof(struct ustor));
    rt_memset(stor_r, 0, sizeof(struct ustor));

    /* get the first sector to read partition table */
    sector = (rt_uint8_t *)rt_malloc(SECTOR_SIZE);
    if (sector == RT_NULL) {
        rt_kprintf("allocate partition sector buffer failed\n");
        return -RT_ERROR;
    }

    rt_memset(sector, 0, SECTOR_SIZE);

    /* get the partition table */
    ret = usbh_msc_scsi_read10(msc_class, 0, sector, 1);
    if (ret != RT_EOK) {
        rt_kprintf("read parition table error\n");

        rt_free(sector);
        return -RT_ERROR;
    }

    for (i = 0; i < MAX_PARTITION_COUNT; i++) {
        /* get the first partition */
        ret = dfs_filesystem_get_partition(&part[i], sector, i);
        if (ret == RT_EOK) {
            struct ustor_data *data = rt_malloc(sizeof(struct ustor_data));
            rt_memset(data, 0, sizeof(struct ustor_data));
            data->msc_class = msc_class;
            data->udisk_id = udisk_get_id();
            rt_snprintf(dname, 6, "ud%d-%d", data->udisk_id, i);
            rt_snprintf(sname, 8, "sem_ud%d", i);
            data->part.lock = rt_sem_create(sname, 1, RT_IPC_FLAG_FIFO);

            /* register sdcard device */
            stor_r->dev[i].type = RT_Device_Class_Block;
#ifdef RT_USING_DEVICE_OPS
            stor->dev[i].ops = &udisk_device_ops;
#else
            stor_r->dev[i].init = rt_udisk_init;
            stor_r->dev[i].read = rt_udisk_read;
            stor_r->dev[i].write = rt_udisk_write;
            stor_r->dev[i].control = rt_udisk_control;
#endif
            stor_r->dev[i].user_data = (void *)data;

            rt_device_register(&stor_r->dev[i], dname, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_REMOVABLE | RT_DEVICE_FLAG_STANDALONE);

            stor_r->dev_cnt++;

            if (dfs_mount(stor_r->dev[i].parent.name, CONFIG_DFS_MOUNT_POINT, "elm", 0, 0) == 0) {
                rt_kprintf("udisk part %d mount successfully\n", i);
            } else {
                rt_kprintf("udisk part %d mount failed\n", i);
            }
        } else {
            if (i == 0) {
                struct ustor_data *data = rt_malloc(sizeof(struct ustor_data));
                rt_memset(data, 0, sizeof(struct ustor_data));
                data->udisk_id = udisk_get_id();

                /* there is no partition table */
                data->part.offset = 0;
                data->part.size = 0;
                data->msc_class = msc_class;
                data->part.lock = rt_sem_create("sem_ud", 1, RT_IPC_FLAG_FIFO);

                rt_snprintf(dname, 7, "udisk%d", data->udisk_id);

                /* register sdcard device */
                stor_r->dev[0].type = RT_Device_Class_Block;
#ifdef RT_USING_DEVICE_OPS
                stor->dev[i].ops = &udisk_device_ops;
#else
                stor_r->dev[0].init = rt_udisk_init;
                stor_r->dev[0].read = rt_udisk_read;
                stor_r->dev[0].write = rt_udisk_write;
                stor_r->dev[0].control = rt_udisk_control;
#endif
                stor_r->dev[0].user_data = (void *)data;

                rt_device_register(&stor_r->dev[0], dname, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_REMOVABLE | RT_DEVICE_FLAG_STANDALONE);

                stor_r->dev_cnt++;
                if (dfs_mount(stor_r->dev[0].parent.name, CONFIG_DFS_MOUNT_POINT, "elm", 0, 0) == 0) {
                    rt_kprintf("Mount FAT on Udisk successful.\n");
                } else {
                    rt_kprintf("Mount FAT on Udisk failed.\n");
                }
            }

            break;
        }
    }

    rt_free(sector);

    return RT_EOK;
}

/**
 * This function will be invoked when usb disk plug out is detected and it would clean
 * and release all udisk related resources.
 *
 * @param intf the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
rt_err_t rt_udisk_stop(struct usbh_msc *msc_class)
{
    int i;

    struct ustor_data *data;

    /* check parameter */
    RT_ASSERT(msc_class != RT_NULL);

    RT_ASSERT(stor_r != RT_NULL);

    for (i = 0; i < stor_r->dev_cnt; i++) {
        rt_device_t dev = &stor_r->dev[i];
        data = (struct ustor_data *)dev->user_data;

        /* unmount filesystem */
        dfs_unmount(CONFIG_DFS_MOUNT_POINT);

        /* delete semaphore */
        rt_sem_delete(data->part.lock);
        udisk_free_id(data->udisk_id);
        rt_free(data);

        /* unregister device */
        rt_device_unregister(&stor_r->dev[i]);
    }

    rt_free(stor_r);

    return RT_EOK;
}