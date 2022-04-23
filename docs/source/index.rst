.. CherryUSB 使用指南 documentation master file, created by
   sphinx-quickstart on Thu Nov 21 10:50:33 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

CherryUSB 使用指南
======================================================

CherryUSB 是一个小而美的、可移植性高的、用于嵌入式系统的 USB 主从协议栈。

**小在哪？**

代码中基本不使用全局的数组，使用链表的方式由用户动态的添加。并且基本不占用很大的 ram，flash 的占用也非常小。

**美在哪？**

代码从上到下是逐次递进的过程，不存在什么跳到其他文件，或者顺序倒换的问题，也就是说当你从上往下把代码看完，你也就知道 usb 协议栈是如何工作的了。

**可移植性高在哪？**

根据 usb ip 的特点，定义了标准的 dcd 和 hcd 的 porting 接口，只要依次实现，就可以使用 usb 协议栈了。

最重要的一点，无论从机还是主机协议栈，协议栈只做枚举过程的操作，其余所有应用层操作，协议栈不会做。比如使用从机时，触发了其他端点的 out 中断，那么将直接调用到用户注册的函数中，由用户自己读取，而协议栈并不会使用类似 ringbuffer 的形式读取到一块 buffer上先存着，减少 copy 次数。而类似 msc 和 rndis 这种已经成标准的则由协议栈进行操作。
此外，使用从机协议栈过程中你就发现，out 中断就像串口接收中断一样，in 中断就像 dma 完成中断一样。而主机协议栈则是在枚举完成后提供了注册的 **devname** ，用户通过 **devname**  得到收发时需要的句柄，从而进行数据的收发，无需知道收发设备的端点和地址，用户也不想知道这么多信息，太过复杂。


- 从机协议栈视频教程：https://www.bilibili.com/video/BV1Ef4y1t73d
- 主机协议栈视频教程：TODO
- github ：https://github.com/sakumisu/CherryUSB

.. toctree::
   :maxdepth: 1
   :caption: 快速上手

   quick_start/bl702
   quick_start/stm32f429
   quick_start/es32f369
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
   porting_usbip
