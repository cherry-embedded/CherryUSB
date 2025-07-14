usbh_msc
===============

本节主要介绍主机 MSC 使用。借助 FATFS 实现读写功能。

- 在 msc 枚举完成的回调中注册一个线程，用于读写操作。

.. code-block:: C

    void usbh_msc_run(struct usbh_msc *msc_class)
    {
        usb_osal_thread_create("usbh_msc", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_msc_thread, msc_class);
    }

    void usbh_msc_stop(struct usbh_msc *msc_class)
    {
    }


- 不使用 fatfs，则直接使用 usbh_msc_scsi_read10 或者 usbh_msc_scsi_write10 函数进行读写操作。
- 如果使用 fatfs，则需要在 usbh_msc_thread 中调用 fatfs 的接口进行读写操作。msc读写适配fatfs 参考 `platform/none/usbh_fatfs.c`

.. code-block:: C

    static void usbh_msc_thread(void *argument)
    {
        int ret;
        struct usbh_msc *msc_class = (struct usbh_msc *)argument;

        /* test with only one buffer, if you have more msc class, modify by yourself */
    #if 1
        /* get the partition table */
        ret = usbh_msc_scsi_read10(msc_class, 0, partition_table, 1);
        if (ret < 0) {
            USB_LOG_RAW("scsi_read10 error,ret:%d\r\n", ret);
            goto delete;
        }
        for (uint32_t i = 0; i < 512; i++) {
            if (i % 16 == 0) {
                USB_LOG_RAW("\r\n");
            }
            USB_LOG_RAW("%02x ", partition_table[i]);
        }
        USB_LOG_RAW("\r\n");
    #endif

    #if TEST_USBH_MSC_FATFS
        usb_msc_fatfs_test();
    #endif
        // clang-format off
    delete:
        usb_osal_thread_delete(NULL);
        // clang-format on
    }

- 最后处理完成或者失败后，删除线程。