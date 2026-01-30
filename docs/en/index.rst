.. CherryUSB User Guide documentation master file, created by
   sphinx-quickstart on Thu Nov 21 10:50:33 2019.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

CherryUSB User Guide
====================================================================

CherryUSB is a small, lightweight, portable USB host and device protocol stack for embedded systems. CherryUSB offers the following advantages:

**Easy to Learn USB**

To facilitate user learning of USB fundamentals, enumeration, driver loading, and IP drivers, the written code has the following advantages:

- Streamlined code with simple logic and no complex C language syntax
- Tree-structured programming with progressive code layers
- Templated and simplified Class drivers and porting drivers
- Clear API categorization (Device: initialization, class registration, command callbacks, data transmission; Host: initialization, class discovery, data transmission)

**Easy to Use USB**

To facilitate user interaction with USB interfaces, considering users' familiarity with UART and DMA, the designed data transmission interface has the following advantages:

- Equivalent to using UART TX DMA/UART RX DMA
- No length restrictions on transmission/reception; users don't need to worry about USB packetization (porting drivers handle packetization)

**Easy to Achieve USB Performance**

Considering USB performance requirements to reach theoretical USB hardware bandwidth, the designed data transmission interface has the following advantages:

- Porting drivers directly interface with registers without abstraction layer encapsulation
- Memory zero copy
- DMA mode used when IP supports DMA (DMA provides hardware packetization functionality)
- No length restrictions, facilitating hardware DMA interfacing and maximizing DMA advantages
- Packetization handled in interrupt context

**Device Protocol Stack Overall Execution Flow**

.. figure:: usbdev.svg

**Host Protocol Stack Overall Execution Flow**

.. figure:: usbhost.svg

**Other Related Links**

- **Video Tutorial**:  https://www.bilibili.com/cheese/play/ss707687201
- **GitHub**: https://github.com/sakumisu/CherryUSB
- **CherryUSB Theoretical Analysis and Application Practice - Hans Journal**: https://www.hanspub.org/journal/paperinformation?paperid=126903

.. toctree::
   :maxdepth: 1
   :caption: Quick Start

   quick_start/start
   quick_start/demo
   quick_start/transplant
   quick_start/rtthread
   quick_start/q&a
   quick_start/migration
   quick_start/share
   quick_start/opensource

.. toctree::
   :maxdepth: 1
   :caption: USB Basic Knowledge

   usb/usb2.0_basic
   usb/usb3.0_basic
   usb/usb_desc
   usb/usb_request
   usb/usb_enum
   usb/usb_ext

.. toctree::
   :maxdepth: 1
   :caption: API Manual

   api/api_device
   api/api_host
   api/api_port
   api/api_config

.. toctree::
   :maxdepth: 1
   :caption: Class Guide

   class/class_cdc
   class/class_hid
   class/class_msc
   class/class_audio
   class/class_video
   class/winusb

.. toctree::
   :maxdepth: 1
   :caption: Examples

   demo/usbd_cdc_acm
   demo/usbd_hid
   demo/usbd_msc
   demo/usbd_audiov1
   demo/usbd_audiov2
   demo/usbd_video
   demo/usbd_winusb
   demo/usbd_webusb
   demo/usbd_rndis
   demo/usbd_ecm
   demo/usbd_adb
   demo/usbd_mtp
   demo/usbh_serial
   demo/usbh_hid
   demo/usbh_msc
   demo/usbh_net
   demo/usbh_bluetooth
   demo/usbh_wifi
   demo/usbh_audio
   demo/usbh_video
   demo/usb_otg
   demo/usbd_vendor
   demo/usbh_vendor

.. toctree::
   :maxdepth: 1
   :caption: USB IP Introduction

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
   :caption: Tools Usage

   tools/index

.. toctree::
   :maxdepth: 1
   :caption: Version Information

   version

.. toctree::
   :maxdepth: 1
   :caption: Performance Showcase

   show/index

.. toctree::
   :maxdepth: 1
   :caption: Commercial Support

   support/index
