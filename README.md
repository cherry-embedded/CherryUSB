# USB Stack

[中文版](./README_zh.md)

USB Stack is a tiny, beautiful and portable USB host and device stack for embedded system.

## USB Stack Directoy Structure

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

## USB Device Stack Overview

USB Device Stack provides a unified framework of functions for standard device requests, CLASS requests, VENDOR requests and custom special requests. The object-oriented and chained approach allows the user to quickly get started with composite devices without having to worry about the underlying logic. At the same time, a standard dcd porting interface has been standardised for adapting different USB IPs to achieve ip-oriented programming.

How USB Device Stack is implemented, this video will tell you: <https://www.bilibili.com/video/BV1Ef4y1t73d> .

USB Device Stack has the following functions：

- Support USB2.0 full and high speed
- Support endpoint irq callback register by users, let users do whatever they wants in endpoint irq callback.
- Support Composite class
- Support Communication Class (CDC)
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

USB Device Stack resource usage：

|   file      |  FLASH (Byte)  |  RAM (Byte)  |
|:-----------:|:--------------:|:------------:|
|usbd_core.c  |  3045          | 373          |
|usbd_cdc.c   |  302           | 20           |
|usbd_msc.c   |  2452          | 132          |
|usbd_hid.c   |  784           | 201          |
|usbd_audio.c |  438           | 14           |
|usbd_video.c |  402           | 4            |

## USB Host Stack Overview

waiting....

## USB Device API

More of USB Device API reference, please visit : [USB Device API](docs/usb_device.md)

## USB Host API

More of USB Host API reference, please visit: [USB Host API](docs/usb_host.md)

## How To Use In RT-Thread package

How to use in RT-Thread package, please visit：[RT-Thread package Userguide with usb stack](docs/rt-thread.md)

## USB Reference Manual

- USB2.0 spec: <https://www.usb.org/document-library/usb-20-specification>
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
