.. CherryUSB 使用指南 documentation master file, created by
   sphinx-quickstart on Thu Nov 21 10:50:33 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

CherryUSB 使用指南
======================================================

CherryUSB 是一个小而美的、可移植性高的、用于嵌入式系统的 USB 主从协议栈。同时 CherryUSB 具有以下优点：

**易于学习 USB**

为了方便用户学习 USB 基本知识、枚举、驱动加载、IP 驱动，因此，编写的代码具备以下优点：

- 代码精简，逻辑简单，无复杂 C 语言语法
- 树状化编程，代码层层递进
- Class 驱动和 porting 驱动模板化、精简化
- API 分类清晰（从机：初始化、注册类、命令回调类、数据收发类；主机：初始化、查找类、数据收发类）

**易于使用 USB**

为了方便用户使用 USB 接口，考虑到用户学习过 uart 和 dma，因此，设计的数据收发类接口具备以下优点：

- 等价于使用 uart tx dma/uart rx dma
- 收发长度没有限制，用户不需要关心 USB 分包过程（porting 驱动做分包过程）

**易于发挥 USB 性能**

考虑到 USB 性能问题，尽量达到 USB 硬件理论带宽，因此，设计的数据收发类接口具备以下优点：

- Porting 驱动直接对接寄存器，无抽象层封装
- Memory zero copy
- IP 如果带 DMA 则使用 DMA 模式（DMA 带硬件分包功能）
- 长度无限制，方便对接硬件 DMA 并且发挥 DMA 的优势
- 分包功能在中断中处理

**从机协议栈整体执行流程**

.. figure:: usbdev.svg

**主机协议栈整体执行流程**

.. figure:: usbhost.svg

**其他相关链接**

- **CherryUSB 大纲** https://www.bilibili.com/video/BV1st4y1H7K2
- **CherryUSB 从机协议栈视频教程** https://www.bilibili.com/video/BV1Ef4y1t73d
- **CherryUSB 腾讯会议** https://www.bilibili.com/video/BV16x421y7mM
- **github** https://github.com/sakumisu/CherryUSB

.. toctree::
   :maxdepth: 1
   :caption: 快速上手

   quick_start/start
   quick_start/demo
   quick_start/transplant
   quick_start/rtthread
   quick_start/esp
   q&a
   opensource
   share

.. toctree::
   :maxdepth: 1
   :caption: USB 基本知识点

   usb/usb2.0_basic
   usb/usb3.0_basic
   usb/usb_desc
   usb/usb_request
   usb/usb_enum
   usb/usb_ext

.. toctree::
   :maxdepth: 1
   :caption: API 手册

   api/api_device
   api/api_host
   api/api_port
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
   :caption: 例程说明

   demo/usbd_cdc_acm
   demo/usbd_hid
   demo/usbd_msc
   demo/usbd_rndis
   demo/usbd_ecm
   demo/usbd_audiov1
   demo/usbd_audiov2
   demo/usbd_video
   demo/usbd_winusb
   demo/usbd_webusb
   demo/usbh_serial
   demo/usbh_hid
   demo/usbh_msc
   demo/usbh_net
   demo/usbh_bluetooth
   demo/usbh_wifi
   demo/usbd_vendor
   demo/usbh_vendor

.. toctree::
   :maxdepth: 1
   :caption: USBIP 介绍

   usbip/ohci
   usbip/ehci
   usbip/xhci
   usbip/chipidea
   usbip/dwc2
   usbip/musb
   usbip/fotg210
   usbip/cdns2
   usbip/cdns3
   usbip/dwc3

.. toctree::
   :maxdepth: 1
   :caption: 工具使用

   tools/index

.. toctree::
   :maxdepth: 1
   :caption: 版本说明

   version

.. toctree::
   :maxdepth: 1
   :caption: 性能展示

   show/index

.. toctree::
   :maxdepth: 1
   :caption: 商业支持

   support/index