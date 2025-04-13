usbd_msc
===============

本节主要演示 USB 模拟 U 盘功能。默认使用RAM 作为存储介质模拟 U 盘。

- 实现 U 盘的读写和获取容量接口，注意，容量 block_num 为虚拟的，实际没有这么多 block，读写的数据超过 BLOCK_COUNT 会丢弃。

block_size 一般为 512/2048/4096。

.. code-block:: C

    void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint32_t *block_size)
    {
        *block_num = 1000; //Pretend having so many buffer,not has actually.
        *block_size = BLOCK_SIZE;
    }
    int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
    {
        if (sector < BLOCK_COUNT)
            memcpy(buffer, mass_block[sector].BlockSpace, length);
        return 0;
    }

    int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length)
    {
        if (sector < BLOCK_COUNT)
            memcpy(mass_block[sector].BlockSpace, buffer, length);
        return 0;
    }

- 默认上述 API 在中断中执行，如果需要在非中断中执行，可以选择如下：

1，裸机下开启 `CONFIG_USBDEV_MSC_POLLING` 并在 while1 中调用 `usbd_msc_polling`，则读写函数在 while1 中执行。

2, OS 下开启 `CONFIG_USBDEV_MSC_THREAD`，则读写函数在线程中执行。

- 修改  `CONFIG_USBDEV_MSC_BUFSIZE` 会影响 U 盘的读写速度，必须是 block_size 的整数倍，当然，也会增加 RAM 的占用。

- 如果 RAM 例程可以用，但是介质更换成 SD 或者 FLASH 后不可用，则一定是介质驱动问题。