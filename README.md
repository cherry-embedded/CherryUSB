# CherryUSB

[中文版](./README_zh.md)

CherryUSB is a tiny, beautiful and portable USB host and device stack for embedded system.

## CherryUSB Directoy Structure

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
│   └── ch32
├── docs
├── osal
├── packet capture
└── port
    ├── bouffalolab
    │   └── bl702
    ├── ch32
    ├── ehci
    ├── fsdev
    ├── mm32
    ├── synopsys
    ├── musb
    └── template
```

|   Directory       |  Description            |
|:-------------:|:---------------------------:|
|class          |  usb class driver           |
|common         |  usb spec macros and utils  |
|core           |  usb core implementation  |
|demo           |  different chip demo     |
|osal           |  os wrapper              |
|docs           |  doc for guiding         |
|packet capture |  packet capture file     |
|port           |  usb dcd and hcd porting |

## CherryUSB Device Stack Overview

CherryUSB Device Stack provides a unified framework of functions for standard device requests, CLASS requests, VENDOR requests and custom special requests. The object-oriented and chained approach allows the user to quickly get started with composite devices without having to worry about the underlying logic. At the same time, a standard dcd porting interface has been standardised for adapting different USB IPs to achieve ip-oriented programming.

How CherryUSB Device Stack is implemented, this video will tell you: <https://www.bilibili.com/video/BV1Ef4y1t73d> .

CherryUSB Device Stack has the following functions：

- Support USB2.0 full and high speed
- Support endpoint irq callback register by users, let users do whatever they wants in endpoint irq callback.
- Support Compound and Composite Device
- Support Communication Device Class (CDC)
- Support Human Interface Device (HID)
- Support Custom human Interface Device (HID)
- Support Mass Storage Class (MSC)
- Support USB VIDEO CLASS (UVC)
- Support USB AUDIO CLASS (UAC)
- Support Device Firmware Upgrade CLASS (DFU)
- Support USB MIDI CLASS (MIDI)
- Support Test and Measurement CLASS (TMC)
- Support Vendor class
- Support WINUSB1.0、WINUSB2.0

CherryUSB Device Stack resource usage：

|   file      |  FLASH (Byte)  |  RAM (Byte)  |
|:-----------:|:--------------:|:------------:|
|usbd_core.c  |  3045          | 373          |
|usbd_cdc.c   |  302           | 20           |
|usbd_msc.c   |  2452          | 132          |
|usbd_hid.c   |  784           | 201          |
|usbd_audio.c |  438           | 14           |
|usbd_video.c |  402           | 4            |

## CherryUSB Host Stack Overview

The CherryUSB Host Stack has a standard enumeration implementation for devices mounted on roothubs and external hubs, and a standard interface for the different Class to indicate what the Class driver needs to do after enumeration and after disconnection. A standard hcd porting interface has also been standardised for adapting different USB IPs for IP-oriented programming. Finally, the protocol stack is managed using os, and provides osal to make a adaptation to different os.

How CherryUSB Host Stack is implemented, the video will be provided in future.

CherryUSB Host Stack has the following functions：

- Automatic loading of supported Class drivers
- Support blocking transfers and asynchronous transfers
- Support Compound and Composite Device
- Multi-level HUB support, expandable up to 7 levels
- Support Communication Device Class (CDC)
- Support Human Interface Device (HID)
- Support Mass Storage Class (MSC)
- Support Vendor class
- Support Andriod AOA Communication

The CherryUSB Host stack also provides the lsusb function, which allows you to view information about all mounted devices, including those on external hubs, with the help of a shell plugin.

![lsusb](docs/img/lsusb.png)

## CherryUSB Device API

More of USB Device API reference, please visit : [CherryUSB Device API](docs/usb_device.md)

## CherryUSB Host API

More of USB Host API reference, please visit: [CherryUSB Host API](docs/usb_host.md)

## How To Use In RT-Thread package

How to use in RT-Thread package, please visit：[RT-Thread package Userguide with CherryUSB](docs/rt-thread.md)
