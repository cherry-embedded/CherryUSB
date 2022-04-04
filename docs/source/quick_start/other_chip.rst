芯片通用移植指南
=========================

本节主要介绍所有带 USB IP 的芯片，移植 CherryUSB 主从协议栈时的通用步骤和注意事项。在往下看之前，需要你准备好一个可以打印 helloworld 的基本工程，并且实现了 `printf` 、 `malloc`、 `free`。如果是主机，需要准备好可以打印 helloworld 的带 OS 的工程。

USB Device 移植要点
-----------------------

- 拷贝 CherryUSB 源码到工程里
- 添加 `usbd_core.c`
- 添加 `usb_dc_xxx.c`，它是芯片所对应的 USB IP dcd 部分驱动，如果不知道自己芯片属于那个 USB IP，参考 **port** 目录下的不同 USB IP 的 readme。如果使用的 USB IP 没有支持，只能自己实现了
- 添加 `USBD_IRQHandler=xxxx` 和 `USB_NUM_BIDIR_ENDPOINTS=x` 的 CFLAG，如果没有添加则使用 `usb_dc_xxx.c` 默认配置
- 添加 CherryUSB 源码中使用到的源文件的对应头文件路径，不要添加CherryUSB 根目录路径
- 根据自己的需求添加 对应 **class** 目录下的文件，并且添加的文件形似 `usbd_xxx.c`
- 实现 `usb_dc_low_level_init` 函数，该函数主要负责 USB 时钟、引脚、中断的初始化（此函数在对应的 `usb_dc_xxx.c` 中为弱定义）。该函数可以放在你想要放的任何参与编译的 c 文件中。如何进行 USB 的时钟、引脚、中断等初始化，请自行根据你使用的芯片原厂提供的源码中进行添加。
- 描述符的注册、class的注册、接口的注册、端点中断的注册。不会的参考 demo 下的 template
- 调用 `usbd_initialize` 初始化 usb 硬件
- 编译使用。各个 class 如何使用，参考 demo 下的 template


USB Host 移植要点
-----------------------

- 拷贝 CherryUSB 源码到工程里
- 添加 `usbh_core.c`
- 添加 `usb_hc_xxx.c`，它是芯片所对应的 USB IP hcd 部分驱动，如果不知道自己芯片属于那个 USB IP，参考 **port** 目录下的不同 USB IP 的 readme。如果使用的 USB IP 没有支持，只能自己实现了
- 添加 `USBH_IRQHandler=xxxx` 的 CFLAG，如果没有添加则使用 `usb_hc_xxx.c` 默认配置
- 拷贝一份 `usb_config.h` 文件到自己的工程中，根据实际情况修改主机相关的 CONFIG 宏。CherryUSB 根目录下的为 template，不要使用
- 添加 **osal** 目录下文件，不同 os 只能选择一个
- 添加 CherryUSB 源码中使用到的源文件的对应头文件路径，不要添加CherryUSB 根目录路径
- 根据自己的需求添加 对应 **class** 目录下的文件，并且添加的文件形似 `usbh_xxx.c`。推荐全部添加。
- 实现 `usb_hc_low_level_init` 函数，该函数主要负责 USB 时钟、引脚、中断的初始化（此函数在对应的 `usb_hc_xxx.c` 中为弱定义）。该函数可以放在你想要放的任何参与编译的 c 文件中。如何进行 USB 的时钟、引脚、中断等初始化，请自行根据你使用的芯片原厂提供的源码中进行添加。
- 调用 `usbh_initialize` 初始化 usb 硬件
- 如果使用的是 GCC，需要在链接脚本中添加如下代码：

.. code-block:: C

        /* section information for usbh class */
        . = ALIGN(4);
        _usbh_class_info_start = .;
        KEEP(*(usbh_class_info))
        _usbh_class_info_end = .;

- 编译使用。各个 class 如何使用，参考 demo 下的 `usb_host.c` 文件
