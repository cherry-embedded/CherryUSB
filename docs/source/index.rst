.. CherryUSB 使用指南 documentation master file, created by
   sphinx-quickstart on Thu Nov 21 10:50:33 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

CherryUSB 使用指南
======================================================

CherryUSB 是一个小而美的、可移植性高的、用于嵌入式系统的 USB 主从协议栈。同时 CherryUSB 具有以下优点：

- 代码精简，并且内存占用极小，而且还可进一步的裁剪
- 全面的 class 驱动，并且主从 class 驱动全部模板化，方便用户增加新的 class 驱动以及学习的时候查找规律
- 可供用户使用的 API 非常少，并且分类清晰。从机：初始化 + 注册、命令回调类、数据收发类；主机：初始化 + 查找类、数据收发类
- 树状化编程，代码层层递进，方便用户理清函数调用关系、枚举和 class 驱动加载过程
- 标准化的 porting 接口，相同 ip 无需重写驱动，并且 porting 驱动也进行了模板化，方便用户新增 porting。
- 主从收发接口的使用等价于 uart tx/rx dma 的使用，长度也没有限制
- 能够达到 USB 硬件理论带宽

从机协议栈整体执行流程：

.. figure:: usbdev.svg

主机协议栈整体执行流程：

.. figure:: usbhost.svg

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

   usb/usb2.0_basic
   usb/usb3.0_basic
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
   :caption: 基本例程

   demo/cdc_acm
   demo/msc_ram
   demo/audio_mic_speaker
   demo/usb_video

.. toctree::
   :maxdepth: 1
   :caption: Porting 说明

   porting
   porting_usbip

.. toctree::
   :maxdepth: 1
   :caption: 工具使用

   tools/index