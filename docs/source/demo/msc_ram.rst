USB 模拟 U 盘
============================

软件实现
------------

详细代码参考 `demo/msc_ram_template.c`

.. code-block:: C

    usbd_desc_register(msc_ram_descriptor);
    usbd_add_interface(usbd_msc_alloc_intf(MSC_OUT_EP, MSC_IN_EP));

    usbd_initialize();

- 调用 `msc_ram_init` 配置 msc 描述符并初始化 usb 硬件
- 因为 msc 有1个接口，所以我们需要调用 `usbd_add_interface` 1次
- msc 中的端点的数据流是协议栈这边管理，所以不需要用户注册端点的回调函数。同理 `usbd_configure_done_callback` 也不需要，为空即可

.. code-block:: C

    void usbd_msc_get_cap(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
    {
        *block_num = 1000; //Pretend having so many buffer,not has actually.
        *block_size = BLOCK_SIZE;
    }
    int usbd_msc_sector_read(uint32_t sector, uint8_t *buffer, uint32_t length)
    {
        return 0;
    }

    int usbd_msc_sector_write(uint32_t sector, uint8_t *buffer, uint32_t length)
    {
        return 0;
    }

- 实现三个接口即可使用 msc，读写操作如果没有 os 则是在中断中
- `CONFIG_USBDEV_MSC_BLOCK_SIZE` 可以为 512 的整数倍，更改此项，可以增加 msc 的读写速度，当然，也会消耗更多的 ram


.. note:: MSC 一般配合 rtos 使用，因为读写操作是阻塞的，放中断是不合适的， `CONFIG_USBDEV_MSC_THREAD` 则是使能 os 管理

