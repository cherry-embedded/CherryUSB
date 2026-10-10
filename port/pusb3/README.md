# USB OTG 控制器 (PUSB3)

- PD2408 平台上提供 PUSB3 接口，例程包括

---------------------------------------------

- Host 模式

- - 1. [FreeRTOS 上作为主机使用键盘/鼠标/U盘](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/usb_host/README.md)
- - 2. [RT-Thread 上作为主机识别键盘/鼠标/U盘](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---------------------------------------------

- Device 模式

- - 1. [裸机上模拟为一个 U 盘](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/usb_device/README.md) 
- - 2. [裸机上模拟为一个虚拟串口](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/usb_device/README.md) 
- - 3. [FreeRTOS 上模拟为一个 U 盘](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/usb_device/README.md)
- - 4. [FreeRTOS 上模拟为虚拟串口](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/usb_device/README.md)
- - 5. [RT-Thread 上模拟为一个 U 盘](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---------------------------------------------

- PUSB3 的驱动功能以静态库的方式提供，
- - libpusb3_hc_a64.a : AARCH64 主机模式驱动库
- - libpusb3_dc_a64.a : AARCH64 从机模式驱动库

需要获取源代码请联系 `opensource_embedded@phytium.com.cn` 获取

# USB OTG Controller (PUSB3)

- PD2408 platform provides PUSB3 interface. Example programs include:

---

### Host Mode
1. [Use keyboard/mouse/USB drive as host on FreeRTOS](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/usb_host/README.md)  
2. [Recognize keyboard/mouse/USB drive as host on RT-Thread](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---

### Device Mode
1. [Emulate USB drive on bare-metal](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/usb_device/README.md)  
2. [Emulate virtual serial port on bare-metal](https://gitee.com/phytium_embedded/phytium-standalone-sdk/tree/master/example/peripherals/usb/usb_device/README.md)  
3. [Emulate USB drive on FreeRTOS](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/usb_device/README.md)  
4. [Emulate virtual serial port on FreeRTOS](https://gitee.com/phytium_embedded/phytium-free-rtos-sdk/tree/master/example/peripheral/usb/usb_device/README.md)  
5. [Emulate USB drive on RT-Thread](https://github.com/RT-Thread/rt-thread/blob/master/bsp/phytium/doc/use_cherryusb.md)

---

### Driver Libraries
PUSB3 driver is provided as static libraries:
- `libpusb3_hc_a64.a`: AARCH64 host mode driver
- `libpusb3_dc_a64.a`: AARCH64 device mode driver

**Source Code Access**  
Contact [`opensource_embedded@phytium.com.cn`](mailto:opensource_embedded@phytium.com.cn)