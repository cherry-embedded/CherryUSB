# CherryUSB

[中文版](./README_zh.md)

CherryUSB is a tiny, beautiful and portable USB host and device stack for embedded system with USB ip.

![CherryUSB](./docs/assets/usb_outline.png)

## Why choose

- Streamlined code with small memory usage which also can be further trimmed
- Comprehensive class drivers and all master and slave class drivers are templated,making it easy for users to add new class drivers and find patterns when learning
- The APIs available to the users are very few and clearly categorised. Device: initialisation + registration apis, command callback apis, data sending and receiving apis; Host: initialisation + lookup apis, data sending and receiving apis
- Tree-based programming with a hierarchy of code that makes it easy for the user to sort out function call relationships, enumerations and class-driven loading processes
- Standardised porting interface, no need to rewrite the driver for the same ip, and porting drivers are templated to make it easier for users to add new ports
- The use of the device or host transceiver apis are equivalent to the use of the uart tx/rx dma, and there is no limit to the length
- Capable of achieving theoretical USB hardware bandwidth

## Directoy Structure

```
.
├── class
├── common
├── core
├── demo
├── docs
├── osal
├── packet capture
└── port
└── tools

```

|   Directory       |  Description            |
|:-------------:|:---------------------------:|
|class          |  usb class driver           |
|common         |  usb spec macros and utils  |
|core           |  usb core implementation  |
|demo           |  different chips demo     |
|osal           |  os wrapper              |
|docs           |  doc for guiding         |
|packet capture |  packet capture file     |
|port           |  usb dcd and hcd porting |
|tools           |  tool used url          |

## Device Stack Overview

CherryUSB Device Stack provides a unified framework of functions for standard device requests, CLASS requests, VENDOR requests and custom special requests. The object-oriented and chained approach allows the user to quickly get started with composite devices without having to worry about the underlying logic. At the same time, a standard dcd porting interface has been standardised for adapting different USB IPs to achieve ip-oriented programming.

CherryUSB Device Stack has the following functions：

- Support USB2.0 full and high speed
- Support endpoint irq callback register by users, let users do whatever they wants in endpoint irq callback.
- Support Composite Device
- Support Communication Device Class (CDC)
- Support Human Interface Device (HID)
- Support Mass Storage Class (MSC)
- Support USB VIDEO CLASS (UVC1.0、UVC1.5)
- Support USB AUDIO CLASS (UAC1.0、UAC2.0)
- Support Device Firmware Upgrade CLASS (DFU)
- Support USB MIDI CLASS (MIDI)
- Support Remote NDIS (RNDIS)
- Support WINUSB1.0、WINUSB2.0(with BOS)
- Support Vendor class

CherryUSB Device Stack resource usage (GCC 10.2 with -O2):

|   file        |  FLASH (Byte)  |  No Cache RAM (Byte)      |  RAM (Byte)   |  Heap (Byte)     |
|:-------------:|:--------------:|:-------------------------:|:-------------:|:----------------:|
|usbd_core.c    |  3263          | 384                       | 17            | 0                |
|usbd_cdc.c     |  490           | 0                         | 0             | 0                |
|usbd_msc.c     |  2772          | 128 + 512(default)        | 16            | 0                |
|usbd_hid.c     |  501           | 0                         | 0             | 0                |
|usbd_audio.c   |  1208          | 0                         | 4             | 0                |
|usbd_video.c   |  2272          | 0                         | 82            | 0                |

## Host Stack Overview

The CherryUSB Host Stack has a standard enumeration implementation for devices mounted on roothubs and external hubs, and a standard interface for different Classes to indicate what the Class driver needs to do after enumeration and after disconnection. A standard hcd porting interface has also been standardised for adapting different USB IPs for IP-oriented programming. Finally, the host stack is managed using os, and provides osal to make a adaptation for different os.

CherryUSB Host Stack has the following functions：

- Automatic loading of supported Class drivers
- Support blocking transfers and asynchronous transfers
- Support Composite Device
- Multi-level HUB support, expandable up to 7 levels
- Support Communication Device Class (CDC)
- Support Human Interface Device (HID)
- Support Mass Storage Class (MSC)
- Support USB VIDEO CLASS
- Support Remote NDIS (RNDIS)
- Support Vendor class

The CherryUSB Host stack also provides the lsusb function, which allows you to view information about all mounted devices, including those on external hubs, with the help of a shell plugin.

CherryUSB Host Stack resource usage (GCC 10.2 with -O2):

|   file        |  FLASH (Byte)  |  No Cache RAM (Byte)            |  RAM (Byte)                 |  Heap (Byte)                    |
|:-------------:|:--------------:|:-------------------------------:|:---------------------------:|:-------------------------------:|
|usbh_core.c    |  4261          | 512                             | 28                          | sizeof(struct usbh_urb)         |
|usbh_hub.c     |  4633          | sizeof(struct usbh_hub) * (1+n) | sizeof(struct usbh_hubport) + 20 | 0                          |
|usbh_cdc_acm.c |  1004          | 7                               | 4                           | sizeof(struct usbh_cdc_acm) * x |
|usbh_msc.c     |  1776          | 32                              | 4                           | sizeof(struct usbh_msc) * x     |
|usbh_hid.c     |  822           | 128                             | 4                           | sizeof(struct usbh_hid) * x     |
|usbh_video.c   |  3587          | 128                             | 4100(yuv2rgb)               | sizeof(struct usbh_video) * x   |

Among them, `sizeof(struct usbh_hub)` and `sizeof(struct usbh_hubport)` are affected by the following macros：

```
#define CONFIG_USBHOST_MAX_EXTHUBS          1
#define CONFIG_USBHOST_MAX_EHPORTS          4
#define CONFIG_USBHOST_MAX_INTERFACES       6
#define CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 1
#define CONFIG_USBHOST_MAX_ENDPOINTS        4
```

## Documentation Tutorial

Quickly start, USB basic concepts, API manual, Class basic concepts and examples, see [CherryUSB Documentation Tutorial](https://cherryusb.readthedocs.io/)

## Video Tutorial

USB basic concepts and how the CherryUSB Device stack is implemented, see [CherryUSB Device Stack Tutorial](https://www.bilibili.com/video/BV1Ef4y1t73d).

## Graphical Config Tool

[chryusb_configurator](https://github.com/Egahp/chryusb_configurator) is written in **electron + vite2 + ts** framework，currently used to automate the generation of descriptor arrays, with additional functionality to be added later.

## Demo Repo

Note: After version 0.4.1, the dcd drivers have been refactored and some repositories are not maintained for a long time, so if you use a higher version, you need to update it yourself.

|   Manufacturer       |  CHIP or Series    | USB IP| Repo Url |Corresponds to master version|
|:--------------------:|:------------------:|:-----:|:--------:|:---------------------------:|
|Bouffalolab    |  BL702 | bouffalolab|[bl_mcu_sdk](https://github.com/bouffalolab/bl_mcu_sdk/tree/master/examples/usb)| latest |
|Essemi    |  ES32F36xx | musb |[es32f369_repo](https://github.com/sakumisu/CherryUSB/tree/master/demo/es32)|latest |
|AllwinnerTech    |  F1C100S | musb |[cherryusb_rtt_f1c100s](https://github.com/CherryUSB/cherryusb_rtt_f1c100s)|latest |
|ST    |  STM32F103C8T6 | fsdev |[stm32f103_repo](https://github.com/sakumisu/CherryUSB/tree/master/demo/stm32/usb_device/stm32f103c8t6)|latest |
|ST    |  STM32F4 | dwc2 |[stm32f429_device_repo](https://github.com/sakumisu/CherryUSB/tree/master/demo/stm32/usb_device/stm32f429igt6)   [stm32f429_host_repo](https://github.com/sakumisu/CherryUSB/tree/master/demo/stm32/usb_host/stm32f429igt6)|latest |
|ST    |  STM32H7 | dwc2 |[stm32h743_device_repo](https://github.com/sakumisu/CherryUSB/tree/master/demo/stm32/usb_device/stm32h743vbt6)   [stm32h743_host_repo](https://github.com/sakumisu/CherryUSB/tree/master/demo/stm32/usb_host/stm32h743xih6)|latest |
|HPMicro    |  HPM6750 | hpm/ehci |[hpm_repo](https://github.com/CherryUSB/cherryusb_hpmicro)|latest |
|Phytium |  e2000 | xhci |[phytium _repo](https://gitee.com/phytium_embedded/phytium-standalone-sdk)|latest |
|WCH    |  CH32V307 | ch32_usbfs/ch32_usbhs|[ch32v307_repo](https://github.com/CherryUSB/cherryusb_ch32v307)|latest |
|WCH    |  CH57x | ch58x |[ch57x_repo](https://github.com/CherryUSB/cherryusb_ch57x)|v0.4.1 |
|Nuvoton    |  Nuc442 | nuvoton |[nuc442_repo](https://github.com/CherryUSB/cherryusb_nuc442)|v0.4.1 |
|Geehy    |  APM32E10x APM32F0xx| fsdev |[apm32_repo](https://github.com/CherryUSB/cherryusb_apm32)|v0.4.1 |
|Nordicsemi |  Nrf52840 | nrf5x |[nrf5x_repo](https://github.com/CherryUSB/cherryusb_nrf5x)|latest |
|Espressif    |  esp32 | dwc2 |[esp32_repo](https://github.com/CherryUSB/cherryusb_esp32)|v0.4.1 |
|Mindmotion    |  MM32L3xx | mm32 |[mm32_repo](https://github.com/CherryUSB/cherryusb_mm32)|v0.4.1 |

## Contact

QQ group: 642693751