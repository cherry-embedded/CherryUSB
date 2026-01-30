MSC Host
=================

This section mainly introduces the use of Host MSC. Read and write functions are implemented with the help of FATFS.

- Register a thread in the callback after MSC enumeration is completed, used for read and write operations.

.. code-block:: C

    void usbh_msc_run(struct usbh_msc *msc_class)
    {
        usb_osal_thread_create("usbh_msc", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_msc_thread, msc_class);
    }

    void usbh_msc_stop(struct usbh_msc *msc_class)
    {
    }


- Without using fatfs, directly use usbh_msc_scsi_read10 or usbh_msc_scsi_write10 functions for read and write operations.
- If using fatfs, you need to call fatfs interfaces in usbh_msc_thread for read and write operations. For MSC read/write adaptation to fatfs, refer to `platform/fatfs/usbh_fatfs.c`

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

- Finally, after processing is completed or failed, delete the thread.