# USB Stack

USB Stack 是一个小而美的、可移植性高的、用于嵌入式 MCU 的 USB 主从协议栈。

## USB 参考手册

- USB2.0规格书: <https://www.usb.org/document-library/usb-20-specification>
- CDC: <https://www.usb.org/document-library/class-definitions-communication-devices-12>
- MSC: <https://www.usb.org/document-library/mass-storage-class-specification-overview-14>
     <https://www.usb.org/document-library/mass-storage-bulk-only-10>
- HID: <https://www.usb.org/document-library/device-class-definition-hid-111>
       <https://www.usb.org/document-library/hid-usage-tables-122>
- AUDIO: <https://www.usb.org/document-library/audiovideo-device-class-v10-spec-and-adopters-agreement>
        <https://www.usb.org/document-library/audio-data-formats-10>
- VIDEO: <https://www.usb.org/document-library/video-class-v11-document-set>
- TMC: <https://www.usb.org/document-library/test-measurement-class-specification>
- DFU: <https://www.st.com/resource/zh/application_note/cd00264379-usb-dfu-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf>

## USB Stack 目录结构

```
.
├── class
│   ├── audio
│   ├── cdc
│   ├── dfu
│   ├── hid
│   ├── hub
│   ├── midi
│   ├── msc
│   ├── tmc
│   └── video
├── common
├── core
├── demo
│   ├── bouffalolab
│   └── stm32
│   └── mm32
├── docs
├── packet capture
└── port
    ├── bouffalolab
    │   └── bl702
    ├── ch32
    ├── dw2
    ├── fsdev
    ├── mm32
    ├── stm32
    └── template
```

|   目录名       |  描述                          |
|:-------------:|:------------------------------:|
|class          |  usb class 类主从驱动           |
|common         |  usb spec 定义和一些常用函数     |
|core           |  usb 主从核心实现               |
|demo           |  示例                          |
|docs           |  文档                          |
|packet capture |  抓包文件（需要使用力科软件打开）|
|port           |  usb 主从需要实现的 porting接口 |

## USB Device 协议栈简介

USB Device 协议栈对标准设备请求、CLASS 请求、VENDOR 请求以及 custom 特殊请求规范了一套统一的函数框架，采用面向对象和链表的方式，能够使得用户快速上手复合设备，不用管底层的逻辑。同时，规范了一套标准的 dcd porting 接口，用于适配不同的 USB IP，达到面向 ip 编程。

USB Device 协议栈的代码实现过程参考 <https://www.bilibili.com/video/BV1Ef4y1t73d> 。

USB Device 协议栈当前实现以下功能：

- 支持 USB2.0 全速和高速设备
- 支持端点中断注册功能，porting 给用户自己处理中断里的数据
- 支持复合设备
- 支持 Communication Class (CDC)
- 支持 Human Interface Device (HID)
- 支持 Custom human Interface Device (HID)
- 支持 Mass Storage Class (MSC)
- 支持 USB VIDEO CLASS (UVC)
- 支持 USB AUDIO CLASS (UAC)
- 支持 Device Firmware Upgrade CLASS (DFU)
- 支持 USB MIDI CLASS (MIDI)
- 支持 Test and Measurement CLASS (TMC)
- 支持 Vendor 类 class
- 支持 WINUSB1.0、WINUSB2.0

USB Device 协议栈资源占用说明：

|   file      |  FLASH (Byte)  |  RAM (Byte)  |
|:-----------:|:--------------:|:------------:|
|usbd_core.c  |  3045          | 373          |
|usbd_cdc.c   |  302           | 20           |
|usbd_msc.c   |  2452          | 132          |
|usbd_hid.c   |  784           | 201          |
|usbd_audio.c |  438           | 14           |
|usbd_video.c |  402           | 4            |

## USB Host 协议栈简介

waiting....

## USB Device API

更详细的设备协议栈 API 请参考: [USB Device API](docs/usb_device.md)

## USB Host API

更详细的主机协议栈 API 请参考: [USB Host API](docs/usb_host.md)

## RT-Thread 软件包使用

如何在 RT-Thread OS 中使用软件包，请参考：[USB Stack 在 RT-Thread package 中的使用](docs/rt-thread_zh.md)

