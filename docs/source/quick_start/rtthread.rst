基于 RT-Thread 软件包开发指南
===============================

.. note:: CherryUSB 已经加入 RT-Thread 主线，可以选择使用主线版本，配置方式相同。

本节主要介绍使用 RT-Thread 提供的软件包管理器来配置工程，以 env 作为演示。本节操作不同芯片都一样，后续不再重复讲解。打开 env 以后使用 menuconfig 进入包管理器，并在如图所示路径中选择 CherryUSB。

.. figure:: img/env0.png

从机配置
--------------------------

* 选择 Enable usb device mode 并敲回车进入。
* 首先第一个配置是配置 USB 的速度，分为 **FS、HS**，表示使用全速还是高速功能。高速功能要求内置高速 PHY 或者外接 PHY
* 其次第二个配置则是选择 USB device ip，不清楚自己芯片是哪个 ip 的可以参考 **port** 目录下对应的 readme。
* 选择你想使用的 class
* 选择是否使用 demo 模板

.. figure:: img/env1.png

* 最后退出保存即可。
* 拷贝 `cherryusb_config_template.h` 文件到自己工程目录下，命名为 `usb_config.h`，并添加相应的目录头文件路径,并修改以下内容：

.. code-block:: C

        #include "rtthread.h"

        #define CONFIG_USB_PRINTF(...) rt_kprintf(__VA_ARGS__)

* USB IP 相关的 config 需要用户自己根据芯片实际情况修改
* 在代码中实现 `usb_dc_low_level_init` 函数
* 在 USB 中断函数中调用 `USBD_IRQHandler`，并传入 `busid`
* 调用 `usbd_initialize` 并填入 `busid` 和 USB IP 的 `reg base`， `busid` 从 0 开始，不能超过 `CONFIG_USBDEV_MAX_BUS`
* 使用 `scons --target=mdk5` 或者 `scons` 进行编译，如果是mdk，需要使用 AC6 编译器
* 如果芯片带 cache，cache 修改参考 :ref:`usb_cache` 章节

主机配置
--------------------------

* 选择 Enable usb host mode 并敲回车进入
* 选择 USB host ip，不清楚自己芯片是哪个 ip 的可以参考 **port** 目录下对应的 readme
* 根据需要勾选 class 驱动
* 选择是否开启模板 demo，推荐不用

.. figure:: img/env2.png

* 最后退出保存即可。
* 拷贝 `cherryusb_config_template.h` 文件到自己工程目录下，命名为 `usb_config.h`，并添加相应的目录头文件路径,并实现以下内容：

.. code-block:: C

        #include "rtthread.h"

        #define CONFIG_USB_PRINTF(...) rt_kprintf(__VA_ARGS__)

* USB IP 相关的 config 需要用户自己根据芯片实际情况修改
* 在代码中实现 `usb_hc_low_level_init` 函数
* 在 USB 中断函数中调用 `USBH_IRQHandler`，并传入 `busid`
* 调用 `usbh_initialize` 并填入 `busid` 和 USB IP 的 `reg base` 还有 `event_handler` 可缺省为NULL， `busid` 从 0 开始，不能超过 `CONFIG_USBHOST_MAX_BUS`
* 使用 `scons --target=mdk5` 或者 `scons` 进行编译，如果是mdk，需要使用 AC6 编译器
* 链接脚本修改参考 :ref:`usbh_link_script` 章节
* 如果芯片带 cache，cache 修改参考 :ref:`usb_cache` 章节
