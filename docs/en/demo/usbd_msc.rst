MSC Device
=================

This section mainly demonstrates USB mass storage device functionality. By default, RAM is used as storage medium to simulate a USB drive.

- Implement read/write and capacity acquisition interfaces for the USB drive. Note that the capacity block_num is virtual - there aren't actually that many blocks. Read/write data exceeding BLOCK_COUNT will be discarded.

block_size is generally 512/2048/4096.

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

- By default, the above APIs execute in interrupt context. If you need to execute in non-interrupt context, you can choose the following:

1. In bare metal, enable `CONFIG_USBDEV_MSC_POLLING` and call `usbd_msc_polling` in while1, then read/write functions execute in while1.

2. In OS, enable `CONFIG_USBDEV_MSC_THREAD`, then read/write functions execute in thread.

- Modifying `CONFIG_USBDEV_MSC_BUFSIZE` will affect U disk read/write speed. It must be an integer multiple of block_size, of course, it will also increase RAM usage.

- If RAM example works but doesn't work after changing medium to SD or FLASH, it must be a medium driver problem.