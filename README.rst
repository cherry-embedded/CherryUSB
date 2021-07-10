USB Stack
=======================

USB Stack 是一个跨平台的、用于嵌入式 MCU 的 USB 协议栈。其中 DEVICE 协议栈对标准设备请求、CLASS 请求、VENDOR 请求规范了一套统一的函数框架，从而对复合设备或者使用自定义设备类时，能够在极短的时间内进行添加和移植。同时提供了一套标准的 dcd porting 接口，供给不同的 MCU 使用,因此，通用性也非常高。此外在代码优美方面，以及内存占用方面也是相当出色。USB DEVICE 协议栈当前具有以下功能：

- 支持 USB2.0 全速和高速设备
- 支持端点中断注册功能，porting 给用户自己处理中断里的数据
- 支持复合设备
- 支持 Communication Class (CDC)
- 支持 Human Interface Device (HID)
- 支持 Custom human Interface Device (HID)
- 支持 Mass Storage Class (MSC)
- 支持 USB VIDEO CLASS (UVC)
- 支持 USB AUDIO CLASS (UAC)
- 支持 vendor 类 class
- 支持 WINUSB1.0、WINUSB2.0

当前支持的芯片（当然，是个 有 USB 的 mcu 都支持）以及 USB IP如下：

- STM32
- MicroChip
- Kinetis
- Synopsys USB IP
- faraday USB IP
- 开源 USB IP `<https://github.com/www-asics-ws/usb2_dev>`_

.. note:: USB DEVICE 协议栈的代码实现过程参考 `<https://www.bilibili.com/video/BV1Ef4y1t73d>`_

USB DEVICE 协议栈 porting 接口
-------------------------------

USB DEVICE 协议栈 porting 接口在 ``usb_stack/common/usb_dc.h`` 文件中声明，用户根据自己的 MCU 实现以下接口

    - ``usbd_set_address``      调用    :ref:`usb_dc_set_address`
    - ``usbd_ep_open``          调用    :ref:`usb_dc_ep_open`
    - ``usbd_ep_close``         调用    :ref:`usb_dc_ep_close`
    - ``usbd_ep_set_stall``     调用    :ref:`usb_dc_ep_set_stall`
    - ``usbd_ep_clear_stall``   调用    :ref:`usb_dc_ep_clear_stall`
    - ``usbd_ep_is_stalled``    调用    :ref:`usb_dc_ep_is_stalled`
    - ``usbd_ep_write``         调用    :ref:`usb_dc_ep_write`
    - ``usbd_ep_read``          调用    :ref:`usb_dc_ep_read`

USB DEVICE 控制器接口
-------------------------------

**usb_dc_init**
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
``usb_dc_init`` 用来注册 USB 设备和初始化 USB 硬件相关寄存器，注册 usb 中断回调函数。在注册之前需要打开对应 USB 设备的宏定义,例如定义宏 ``BSP_USING_USB`` 方可使用 USB 设备。

.. code-block:: C

    struct device *usb_dc_init(void)
    {
        usb_dc_register(USB_INDEX, "usb", DEVICE_OFLAG_RDWR);
        usb = device_find("usb");
        device_set_callback(usb, usb_dc_event_callback);
        device_open(usb, 0);
        return usb;
    }

- device 返回 USB 设备句柄

.. note::中断处理函数则是调用 ``usbd_event_notify_handler``

USB DEVICE 应用层接口
------------------------

USB DEVICE 通用接口
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**usbd_desc_register**
""""""""""""""""""""""""""""""""""""

**usbd_msosv1_desc_register**
""""""""""""""""""""""""""""""""""""

**usbd_class_add_interface**
""""""""""""""""""""""""""""""""""""

**usbd_interface_add_endpoint**
""""""""""""""""""""""""""""""""""""

**usb_device_is_configured**
""""""""""""""""""""""""""""""""""""

USB Device CDC 类接口
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB Device MSC 类接口
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB Device HID 类接口
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB Device AUDIO 类接口
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB Device VIDEO 类接口
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
