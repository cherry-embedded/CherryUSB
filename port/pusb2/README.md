# USB2.0 OTG 控制器 (PUSB2)

- Phytium PI 和 Phyium E2000 系列开发板提供了兼容 USB2.0 的 OTG 接口
- 当前 Port 在 [RT-Thread](https://github.com/RT-Thread/rt-thread/tree/master/bsp/phytium) 上完成测试，具体使用方法参考 RT-Thread Phytium BSP 中的说明 
- usb_dc_pusb2.c 主要实现 Device 模式，测试过 msc_ram_template.c 和 cdc_acm_template.c 两个 Demo
- usb_hc_pusb2.c 主要实现 Host 模式，测试过 usb_host.c，可以连接 USB Disk, HID 设备鼠标和键盘
- PUSB2 的驱动代码欢迎联系 `opensource_embedded@phytium.com.cn` 获取