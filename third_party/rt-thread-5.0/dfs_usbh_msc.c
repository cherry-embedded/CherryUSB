#include <rtconfig.h>
#include "usbh_core.h"
#include "usbh_msc.h"

#include <dfs_fs.h>

#define DEV_FORMAT "sd%c"

#define CONFIG_DFS_MOUNT_POINT "/"

static rt_err_t rt_udisk_init(rt_device_t dev)
{
    return RT_EOK;
}

static rt_ssize_t rt_udisk_read(rt_device_t dev, rt_off_t pos, void *buffer,
                                rt_size_t size)
{
    struct usbh_msc *msc_class = (struct usbh_msc *)dev->user_data;
    int ret;

    ret = usbh_msc_scsi_read10(msc_class, pos, buffer, size);
    if (ret < 0) {
        rt_kprintf("usb mass_storage read failed\n");
        return 0;
    }

    return size;
}

static rt_ssize_t rt_udisk_write(rt_device_t dev, rt_off_t pos, const void *buffer,
                                 rt_size_t size)
{
    struct usbh_msc *msc_class = (struct usbh_msc *)dev->user_data;
    int ret;

    ret = usbh_msc_scsi_write10(msc_class, pos, buffer, size);
    if (ret < 0) {
        rt_kprintf("usb mass_storage write failed\n");
        return 0;
    }

    return size;
}

static rt_err_t rt_udisk_control(rt_device_t dev, int cmd, void *args)
{
    /* check parameter */
    RT_ASSERT(dev != RT_NULL);
    struct usbh_msc *msc_class = (struct usbh_msc *)dev->user_data;

    if (cmd == RT_DEVICE_CTRL_BLK_GETGEOME) {
        struct rt_device_blk_geometry *geometry;

        geometry = (struct rt_device_blk_geometry *)args;
        if (geometry == RT_NULL)
            return -RT_ERROR;

        geometry->bytes_per_sector = msc_class->blocksize;
        geometry->block_size = msc_class->blocksize;
        geometry->sector_count = msc_class->blocknum;
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

int udisk_init(struct usbh_msc *msc_class)
{
    rt_uint8_t *sector = NULL;
    rt_err_t ret = 0;
    rt_uint8_t i;
    struct dfs_partition part0;
    struct rt_device *dev;
    char name[CONFIG_USBHOST_DEV_NAMELEN];

    dev = rt_malloc(sizeof(struct rt_device));
    memset(dev, 0, sizeof(struct rt_device));

    snprintf(name, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, msc_class->sdchar);

    /* get the first sector to read partition table */
    sector = (rt_uint8_t *)rt_malloc(512);
    if (sector == RT_NULL) {
        rt_kprintf("allocate partition sector buffer failed!\n");
        return -RT_ENOMEM;
    }

    ret = usbh_msc_scsi_read10(msc_class, 0, sector, 1);
    if (ret != RT_EOK) {
        rt_kprintf("usb mass_storage read failed\n");
        goto free_res;
    }

    for (i = 0; i < 16; i++) {
        /* Get the first partition */
        ret = dfs_filesystem_get_partition(&part0, sector, i);
        if (ret == RT_EOK) {
            rt_kprintf("Found partition %d: type = %d, offet=0x%x, size=0x%x\n",
                       i, part0.type, part0.offset, part0.size);
        } else {
            break;
        }
    }

    dev->type = RT_Device_Class_Block;
#ifdef RT_USING_DEVICE_OPS
    dev->ops = &udisk_device_ops;
#else
    dev->init = rt_udisk_init;
    dev->read = rt_udisk_read;
    dev->write = rt_udisk_write;
    dev->control = rt_udisk_control;
#endif
    dev->user_data = msc_class;

    rt_device_register(dev, name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_REMOVABLE | RT_DEVICE_FLAG_STANDALONE);

    ret = dfs_mount(name, CONFIG_DFS_MOUNT_POINT, "elm", 0, 0);
    if (ret == 0) {
        rt_kprintf("udisk: %s mount successfully\n", name);
    } else {
        rt_kprintf("udisk: %s mount failed, ret = %d\n", name, ret);
    }

free_res:
    if (sector)
        rt_free(sector);
    return ret;
}

void rt_udisk_run(struct usbh_msc *msc_class)
{
    udisk_init(msc_class);
}

void rt_udisk_stop(struct usbh_msc *msc_class)
{
    char name[CONFIG_USBHOST_DEV_NAMELEN];
    snprintf(name, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, msc_class->sdchar);

    dfs_unmount(CONFIG_DFS_MOUNT_POINT);
    rt_device_unregister(rt_device_find(name));
}