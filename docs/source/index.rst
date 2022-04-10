.. CherryUSB 使用指南 documentation master file, created by
   sphinx-quickstart on Thu Nov 21 10:50:33 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

CherryUSB 使用指南
======================================================

CherryUSB 是一个小而美的、可移植性高的、用于嵌入式系统的 USB 主从协议栈。

- 从机协议栈视频教程：https://www.bilibili.com/video/BV1Ef4y1t73d
- 主机协议栈视频教程：TODO
- github ：https://github.com/sakumisu/CherryUSB

.. toctree::
   :maxdepth: 1
   :caption: 快速上手

   quick_start/bl702
   quick_start/stm32f429
   quick_start/es32f369
   quick_start/rt-thread/rt-thread_zh
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
   api/api_common

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
