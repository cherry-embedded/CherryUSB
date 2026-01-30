USB CONFIG Description
=======================================

General CONFIG
---------------------

CONFIG_USB_PRINTF
^^^^^^^^^^^^^^^^^^^^

USB log functionality, defaults to redirect to printf. Note that USB log will be used in interrupts, so the redirected API must not block. For example, if using RT-Thread, please change to rt-kprintf

CONFIG_USB_DBG_LEVEL
^^^^^^^^^^^^^^^^^^^^^^

Controls the log print level

CONFIG_USB_PRINTF_COLOR_ENABLE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Controls log color printing, enabled by default

CONFIG_USB_DCACHE_ENABLE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When not using nocache RAM, enable this macro to ensure data consistency. **When using EHCI, nocache RAM is still required internally**.

CONFIG_USB_ALIGN_SIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB buffer alignment size, default is 4. IP in DMA mode may have alignment requirements for input buffers, typically 4. If other alignment is needed, please modify this value.

USB_NOCACHE_RAM_SECTION
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If the chip doesn't have cache functionality, this macro is ineffective. If it does, USB input/output buffers must be placed in nocache RAM to ensure data consistency.

Device Protocol Stack CONFIG
------------------------------

CONFIG_USBDEV_REQUEST_BUFFER_LEN
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Controls the maximum length of control transfer receive and send buffer, default is 512.

CONFIG_USBDEV_SETUP_LOG_PRINT
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enable or disable setup packet dump information, disabled by default.

CONFIG_USBDEV_DESC_CHECK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Not implemented yet

CONFIG_USBDEV_TEST_MODE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Enable or disable USB test mode

CONFIG_USBDEV_MSC_MAX_BUFSIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Maximum length of MSC cache. Larger cache results in higher USB speed because storage media typically has much higher multi-block read/write speeds than single block, such as SD cards.
Default 512. For flash, needs to be changed to 4K. Cache size must be a multiple of the storage media's block size.

CONFIG_USBDEV_MSC_MANUFACTURER_STRING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_MSC_PRODUCT_STRING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_MSC_VERSION_STRING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_MSC_POLLING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Run usbd_msc_sector_read and usbd_msc_sector_write operations in while1, used in bare-metal systems.

CONFIG_USBDEV_MSC_THREAD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enable or disable MSC thread, disabled by default. usbd_msc_sector_read and usbd_msc_sector_write are executed in interrupts by default, so if OS is enabled, it's recommended to enable this macro, then usbd_msc_sector_read and usbd_msc_sector_write will execute in threads.

CONFIG_USBDEV_MSC_PRIO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Priority of MSC read/write thread, default is 4. Lower values mean higher priority.

CONFIG_USBDEV_MSC_STACKSIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Stack size of MSC read/write thread, default 2K bytes

CONFIG_USBDEV_RNDIS_RESP_BUFFER_SIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Maximum receive and send length for RNDIS control transfers. Minimum length determined by RNDIS options list, default should be greater than or equal to 156.

CONFIG_USBDEV_RNDIS_ETH_MAX_FRAME_SIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Maximum length of RNDIS Ethernet frame, default 1580

CONFIG_USBDEV_RNDIS_VENDOR_ID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_RNDIS_VENDOR_DESC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_RNDIS_USING_LWIP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

RNDIS interface with LWIP

Host Protocol Stack CONFIG
----------------------------

The following parameters determine the maximum number of supported external hubs, interfaces, endpoints per interface, and altsetting counts. Changing these values affects RAM size, it's recommended to adjust according to actual requirements.

.. code-block:: C

    #define CONFIG_USBHOST_MAX_RHPORTS          1
    #define CONFIG_USBHOST_MAX_EXTHUBS          1
    #define CONFIG_USBHOST_MAX_EHPORTS          4
    #define CONFIG_USBHOST_MAX_INTERFACES       6
    #define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 1
    #define CONFIG_USBHOST_MAX_ENDPOINTS        4

The following parameters determine the maximum number of supported class drivers. Changing these values affects RAM size, it's recommended to adjust according to actual requirements.

.. code-block:: C

    #define CONFIG_USBHOST_MAX_SERIAL_CLASS  4
    #define CONFIG_USBHOST_MAX_HID_CLASS     4
    #define CONFIG_USBHOST_MAX_MSC_CLASS     2
    #define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
    #define CONFIG_USBHOST_MAX_VIDEO_CLASS   1

CONFIG_USBHOST_PSC_PRIO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Priority of host plug/unplug thread, default is 0. Lower values mean higher priority.

CONFIG_USBHOST_PSC_STACKSIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Stack size of host plug/unplug thread, default 2K bytes

CONFIG_USBHOST_REQUEST_BUFFER_LEN
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Maximum length for control transfer receive or send

CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Timeout for control transfer send or receive, default 500 ms

CONFIG_USBHOST_MSC_TIMEOUT
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Timeout for MSC read/write transfers, default 5s