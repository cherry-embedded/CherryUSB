Q & A
==============================

移植提问模板
----------------

请在下列途径提交问题：
- RT-Thread 官方论坛: https://club.rt-thread.org/ask/tag/5f5f851966917b14.html
- Github issue: https://github.com/cherry-embedded/CherryUSB/issues/new/choose

提问中请包含以下信息：

- 使用的板子，引脚，USB IP
- USB 中断，时钟，引脚，寄存器地址是否正确，截图
- 是否能进 USB 中断
- 芯片是否带有 cache功能，是否做了 no cache 处理，截图
- 硬件是否正常，是否使用杜邦线连接，如果正常，请说明正常原因
- 打开 CONFGI_USBDEV_SETUP_LOG_PRINT，并提供 log
- 是否流片并销售

其余问题提问模板
------------------

具体说明现象，复现方式，使用我提供的 demo 再测试，以及提供完整 log

CherryUSB 性能能到多少
----------------------------------------------------------------

可以达到硬件极限性能，当然需要硬件理论支持到这速度，CherryUSB 就支持到这速度,举例如下：

- HPM 系列(从机可以到 42MB/S, 主机 44MB/S, 已经达到硬件极限)
- BL 系列（从机 32MB/S, 主机 25MB/S, 已经达到硬件极限）
- STM32F4 全速（从机 900KB/S, 主机 1.12MB/S, 已经达到硬件极限）

从机测速demo: cdc_acm_template.c 并且关闭 log，脚本使用 `tools/test_srcipts/test_cdc_speed.py`
主机测速demo: usb_host.c 中 TEST_USBH_CDC_SPEED=1

ST IP 命名问题
------------------

ST 命名为 USB_OTG_FS, USB_OTG_HS，并不是说明本身是高速或者全速，只是代表可以支持到高速，但是本身都是全速，需要外挂高速phy。因此，提问禁止说这两个词，请使用 USB0(PA11/PA12),USB1(PB14/PB15) 代替。其余国产厂家同理。

GD IP 问题
------------------

GD IP 采用 DWC2，但是读取的硬件参数都是 0（我也不懂为什么不给人知道），因此需要用户自行知道硬件信息，以下列举 GD32F4 的信息：

CONFIG_USBDEV_EP_NUM pa11/pa12 引脚必须为 4，PB14/PB15 引脚必须为 6，并删除 usb_dc_dwc2.c 中 while(1){}

- 当 CONFIG_USBDEV_EP_NUM 为4 时，fifo_num 不得大于 320 字
- 当 CONFIG_USBDEV_EP_NUM 为6 时，fifo_num 不得大于 1280 字

其次 GD 复位以后无法使用 EPDIS 功能关闭端点，需要用户删除 reset 中断中的以下代码：

.. code-block:: C

    USB_OTG_INEP(i)->DIEPCTL = (USB_OTG_DIEPCTL_EPDIS | USB_OTG_DIEPCTL_SNAK);
    USB_OTG_OUTEP(i)->DOEPCTL = (USB_OTG_DOEPCTL_EPDIS | USB_OTG_DOEPCTL_SNAK);

dwc2 has less endpoints than config, please check
---------------------------------------------------------------

该 IP 硬件上没有这么多端点，请修改 `CONFIG_USBDEV_EP_NUM`

Ep addr XXX overflow
------------------------------

该 IP 硬件上没有这么多端点, 请更换 IP or 减少端点使用。并且默认 demo 不做双向功能，考虑到不是所有的 IP 都支持，因此默认是 81 02 这样的而不是 81 01，
如果支持，自行修改。某些 IP 双向端点可能会占用相同的硬件信息，不一定能同时使用，自行检查。

This dwc2 version does not support dma mode, so stop working
----------------------------------------------------------------

该 DWC2 版本不支持 dma 模式，禁止使用。

__has_include 报错
------------------------------------------------------------------
如果报错，需要编译器支持 c99 语法，如果是 keil，请用 ac6 编译器

CONFIG_USB_HS 何时使用
----------------------------------------------------------------

当你的芯片硬件支持高速，并想初始化成高速模式时开启，相关 IP 会根据该宏配置内部或者外部 高速 PHY。


Failed to enable port
----------------------------------------------------------------

供电不足或者硬件 USB 电路问题