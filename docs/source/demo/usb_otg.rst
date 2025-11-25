OTG 功能的使用
=========================

如果需要使用 OTG 功能，首先使用的芯片需要支持 ID 检测功能，然后使能 ``CONFIG_USB_OTG_ENABLE`` 宏，将之前的例程中 ``usbh_initialize`` 或者 ``usbh_initialize``
替换成 ``usbotg_initialize`` 即可。

ID 检测电路根据不同的 USB 接口类型有所不同，常见的有 micro-USB 和 USB-C 两种接口类型。

- 如果是 micro-USB 接口，则将 ID 线连接到芯片的 ID 引脚，并使能 ID 功能即可。
- 如果是 USB-C 接口，由于没有 ID 引脚，则需要借助 CC 电路转换成 ID 然后连接到芯片的 ID 引脚，常见电路图如下所示（DNP 表示不焊接）：

.. figure:: img/otg.png


.. note:: 除 ID 引脚以外，还需要增加 VBUS 输出开关控制，当工作在 host 时，开启 VBUS 供电，当工作在 device 时，关闭 VBUS 供电。