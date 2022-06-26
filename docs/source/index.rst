.. CherryUSB 使用指南 documentation master file, created by
   sphinx-quickstart on Thu Nov 21 10:50:33 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

CherryUSB 使用指南
======================================================

CherryUSB 是一个小而美的、可移植性高的、用于嵌入式系统的 USB 主从协议栈。同时 CherryUSB 具有以下优点：

- 比较全面的 class 驱动，并且 class 驱动全部模板化，方便自主添加
- 协议栈采用链表动态注册的方式，减少内存占用
- 树状化编程，方便理清 class 驱动与接口、端点的关系，hub、port、class 之间的关系
- 标准化的 porting 接口
- 设备协议栈的使用简化到类如 uart 、dma 的使用，主机协议栈的使用简化到文件的使用
- 协议栈实现代码简短，并且从上往下看完就能理清 usb 枚举过程和 class 加载机制
- Api 少，并且分为三类：dcd/hcd api、注册 api、命令回调 api

其他相关链接：

- **从机协议栈视频教程** https://www.bilibili.com/video/BV1Ef4y1t73d
- **主机协议栈视频教程** TODO
- **github** https://github.com/sakumisu/CherryUSB

.. toctree::
   :maxdepth: 1
   :caption: 快速上手

   quick_start/bl702
   quick_start/stm32
   quick_start/es32
   quick_start/rt-thread/rtthread
   quick_start/other_chip

.. toctree::
   :maxdepth: 1
   :caption: USB 基本知识点

   usb/usb_basic
   usb/usb_desc
   usb/usb_request
   usb/usb_enum

.. toctree::
   :maxdepth: 1
   :caption: API 手册

   api/api_device
   api/api_host
   api/api_port
   api/api_common
   api/api_config

.. toctree::
   :maxdepth: 1
   :caption: Class 指南

   class/class_cdc
   class/class_hid
   class/class_msc
   class/class_audio
   class/class_video
   class/winusb

.. toctree::
   :maxdepth: 1
   :caption: 综合例程

   demo/usb2uart
   demo/mouse_keyboard
   demo/msc_boot
   demo/video
   demo/speaker_mic
   demo/daplink

.. toctree::
   :maxdepth: 1
   :caption: Porting 说明

   porting
   porting_usbip

.. toctree::
   :maxdepth: 1
   :caption: 工具使用

   tools/index