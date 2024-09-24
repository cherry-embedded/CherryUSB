USB CONFIG 说明
=========================

通用 CONFIG
---------------------

CONFIG_USB_PRINTF
^^^^^^^^^^^^^^^^^^^^

USB log 功能，默认重定向到 printf，需要注意，USB log 会在中断中使用，因此重定向的 api 不允许阻塞。举例，如果使用的是 rt-thread，请更换成 rt-kprintf

CONFIG_USB_DBG_LEVEL
^^^^^^^^^^^^^^^^^^^^^^

控制 log 的打印级别

CONFIG_USB_PRINTF_COLOR_ENABLE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

控制 log 颜色打印，默认开启

CONFIG_USB_ALIGN_SIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB buffer 的对齐大小，默认是 4。IP 在 dma 模式下可能对输入的 buffer有对齐要求，一般是4，如果是其他对齐方式，请修改此值。

USB_NOCACHE_RAM_SECTION
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

如果芯片没有 cache 功能，此宏无效。如果有，则 USB 的输入输出 buffer 必须放在 nocache ram 中，保证数据一致性。

设备协议栈 CONFIG
---------------------

CONFIG_USBDEV_REQUEST_BUFFER_LEN
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

控制传输接收和发送的 buffer 最大长度，默认是 512。

CONFIG_USBDEV_SETUP_LOG_PRINT
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

使能或者关闭 setup 包的 dump 信息，默认关闭。

CONFIG_USBDEV_DESC_CHECK
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

暂时没有实现

CONFIG_USBDEV_TEST_MODE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
使能或者关闭 usb test mode

CONFIG_USBDEV_MSC_MAX_BUFSIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

msc 缓存的最大长度，缓存越大，USB 的速度越高，因为介质一般多个 block 读写速度比单个 block 高很多，比如 sd 卡。
默认 512 ，如果是 flash 需要改成 4K, 缓存的大小需要是介质的一个 block size 的整数倍。

CONFIG_USBDEV_MSC_MANUFACTURER_STRING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_MSC_PRODUCT_STRING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_MSC_VERSION_STRING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_MSC_POLLING
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

将 usbd_msc_sector_read 和 usbd_msc_sector_write 操作放在 while1 中运行，裸机下使用。

CONFIG_USBDEV_MSC_THREAD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

使能或者关闭 msc 线程，默认关闭。usbd_msc_sector_read 和 usbd_msc_sector_write 默认是在中断中执行，所以如果开启了 os 建议开启此宏，那么，
usbd_msc_sector_read 和 usbd_msc_sector_write 就会在线程中执行。

CONFIG_USBDEV_MSC_PRIO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

MSC 读写线程的优先级，默认是 4，数值越小，优先级越高

CONFIG_USBDEV_MSC_STACKSIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

MSC 读写线程的堆栈大小，默认 2K 字节

CONFIG_USBDEV_RNDIS_RESP_BUFFER_SIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

rndis 控制传输最大接收和发送的长度，根据 RNDIS options list 决定最小长度，默认要大于等于 156

CONFIG_USBDEV_RNDIS_ETH_MAX_FRAME_SIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

rndis 以太网帧的最大长度，默认 1580

CONFIG_USBDEV_RNDIS_VENDOR_ID
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_RNDIS_VENDOR_DESC
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

CONFIG_USBDEV_RNDIS_USING_LWIP
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

rndis 与 lwip 接口的对接

主机协议栈 CONFIG
---------------------

以下参数决定了支持的最大外部hub数量，接口数，每个接口的端点数和 altsetting 数量，更改此值会影响 ram 的大小，建议根据实际情况更改。

.. code-block:: C

    #define CONFIG_USBHOST_MAX_RHPORTS          1
    #define CONFIG_USBHOST_MAX_EXTHUBS          1
    #define CONFIG_USBHOST_MAX_EHPORTS          4
    #define CONFIG_USBHOST_MAX_INTERFACES       6
    #define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 1
    #define CONFIG_USBHOST_MAX_ENDPOINTS        4

以下参数决定了支持的 class 数目，更改此值会影响 ram 的大小，建议根据实际情况更改。

.. code-block:: C

    #define CONFIG_USBHOST_MAX_CDC_ACM_CLASS 4
    #define CONFIG_USBHOST_MAX_HID_CLASS     4
    #define CONFIG_USBHOST_MAX_MSC_CLASS     2
    #define CONFIG_USBHOST_MAX_AUDIO_CLASS   1
    #define CONFIG_USBHOST_MAX_VIDEO_CLASS   1

CONFIG_USBHOST_PSC_PRIO
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

主机插拔线程的优先级，默认是 0，数值越小，优先级越高

CONFIG_USBHOST_PSC_STACKSIZE
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

主机插拔线程的堆栈大小，默认 2K 字节

CONFIG_USBHOST_REQUEST_BUFFER_LEN
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

控制传输能够接收或者发送的最大长度

CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

控制传输发送或者接收的超时时间，默认 1s

CONFIG_USBHOST_MSC_TIMEOUT
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

MSC 读写传输的超时时间，默认 5s