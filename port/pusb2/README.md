# USB 2.0 OTG 控制器 (PUSB2)

- Phytium PI 和 Phyium E2000 系列开发板提供了兼容 USB2.0 的 OTG 接口
- 相关的使用例程可以在 Phytium PI（飞腾派）和 E2000 D/Q Demo 板上运行，例程包括

---------------------------------------------

- Host 模式

- - 1. [FreeRTOS 上作为主机使用键盘/鼠标/U盘](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/pusb2_host/README.md)
- - 5. [RT-Thread 上作为主机识别键盘/鼠标/U盘](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---------------------------------------------

- Device 模式

- - 1. [裸机上模拟为一个 U 盘](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/pusb2_device/README.md) 
- - 2. [裸机上模拟为一个虚拟串口](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/pusb2_device/README.md) 
- - 3. [FreeRTOS 上模拟为一个 U 盘](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/pusb2_device/README.md)
- - 4. [FreeRTOS 上模拟为虚拟串口](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/pusb2_device/README.md)
- - 5. [RT-Thread 上模拟为一个 U 盘](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---------------------------------------------

- PUSB2 的驱动功能以静态库的方式提供，需要获取源代码请联系 `opensource_embedded@phytium.com.cn` 获取

# USB 2.0 OTG Controller (PUSB2)

- Phytium PI and the Phytium E2000 series development boards offer OTG interfaces compatible with USB 2.0.
- Relevant usage examples can be run on the Phytium PI and E2000 D/Q Demo boards, including:

---------------------------------------------

- Host Mode

  - 1. [Using a keyboard/mouse/USB flash drive as a host on FreeRTOS](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/pusb2_host/README.md)
  - 5. [Recognizing a keyboard/mouse/USB flash drive as a host on RT-Thread](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---------------------------------------------

- Device Mode
  - 1. [Simulating as a USB flash drive on a standalone system](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/pusb2_device/README.md)
  - 2. [Simulating as a virtual serial port on a standalone system](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/pusb2_device/README.md)
  - 3. [Simulating as a USB flash drive on FreeRTOS](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/pusb2_device/README.md)
  - 4. [Simulating as a virtual serial port on FreeRTOS](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/pusb2_device/README.md)
  - 5. [Simulating as a USB flash drive on RT-Thread](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---------------------------------------------

- The driver functionality for PUSB2 is provided as a static library. To obtain the source code, shall contact `opensource_embedded@phytium.com.cn`.