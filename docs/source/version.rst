版本说明
==============================

如果没有特别情况，请使用最新版本。下面只列举比较重要的更新，详细更新说明请参考 https://github.com/cherry-embedded/CherryUSB/releases。

<= v0.10.2 初代版本
----------------------

- **用于定基本的主从机框架，仅支持单 USB IP**。
- **host 驱动每个 ep 占用一个 硬件 pipe，不支持动态使用硬件 pipe**。
- 相关 porting 需要使用此版本，后续不再支持（比如 ch32，rp2040），以及旧版本pusb2 和 xhci（新版本不再提供源码）。

v1.0.0 过度版本
----------------------

- **host 支持动态使用硬件 pipe，不再固定**

v1.1.0 过度版本
----------------------

- **主从机支持多 USB IP 且要相同 IP**
- **host 增加 bluetooth, ch340, ftdi, cp210x, asix 驱动**
- device msc 支持多 lun，并且 CONFIG_USBDEV_MSC_BLOCK_SIZE 修改为 CONFIG_USBDEV_MSC_MAX_BUFSIZE

v1.2.0
----------------------

- **host 增加 rtl8152，cdc ncm 驱动**
- host 增加 timer 去控制中断传输（hub修改为 timer 控制）
- porting 增加 esp，aic 主机驱动
- **优化 DWC2 优化代码方便阅读，并增加一些 FIFO 配置宏给用户（因为 dwc2 fifo 大小有限，以及配置方式很多，所以导出给用户配置，方便合理控制性能）**
- 优化 ehci 驱动（qtd不再使用动态申请，绑定 qh），方便代码运行的更快

v1.3.0
----------------------

- **device 支持多种速度描述符自动选择功能（开启 CONFIG_USBDEV_ADVANCE_DESC）**
- device core 代码统一 ep0 buffer 的使用，用于美化代码
- host 增加 pl2303 驱动；采用 id table 来支持多个 vid，pid；增加 user_data 给用户使用
- host 网络 class 驱动增加 tx、rx buffer 的宏，增加 LWIP_TCPIP_CORE_LOCKING_INPUT 的使用，以便实现数据的零拷贝
- porting 导入 bouffalo，aic，stm32f723 device驱动
- **porting 中主机部分 urb->timeout 清0 的处理有点问题（大数据量传输时会出现 no pipe alloc 异常，主要原因是刚启动传输就完成了，还没判断 timeout就被修改为0了，没有进入 take sem 流程），此版本已修复**
- ehci enable iaad in usbh_kill_urb，read ehci hcor offset from hccr caplength，enable ohci for ehci
- 适配 nuttx os

v1.3.1
----------------------

- bugfix（audio，video，cdc ecm 相关宏，结构体，api）
- **host hub 枚举线程删除，使用 psc 线程，枚举方式更改为队列模式，取消同时枚举多个设备的功能**
- host 扫描驱动信息和 instance 采用递归模式，删除链表扫描
- host 网络 class 驱动优化，支持接收 16K 以上的数据（cdc ecm 不支持），采用高级 memcpy api
- **device 协议栈中打印删除（中断中不再做打印）**
- porting 中 musb fifo配置修改为从 fifo table 获取（此代码参考 linux），适配 es32，sunxi，beken

v1.4.0
----------------------

- **device 开始支持 remote wakeup 功能, hid request(0x21)，完善 GET STATUS 请求（此版本开始可以通过 USB3CV 测试）**
- device 增加 UF2, ADB, WEBUSB 功能； msc 增加裸机的读写 polling 功能，将读写放在 while1中执行； usbd_cdc 改名为 usbd_cdc_acm
- host 增加 usbwifi(bl616), xbox驱动； **重构 USB3.0 枚举逻辑**
- **host 中 cdc_acm,hid,msc,serial 传输共享 buffer，如果存在多个相同的设备会有问题，修改为单独的 buffer**
- **porting 重构 XHCI/PUSB2 驱动，不开源**；ehci 和 ohci 文件改名；增加 remote wakeup api
- esp 组件库支持
- **chipidea 从机驱动支持，nxp mcx 系列主从支持**
- threadx os 支持

v1.4.1
----------------------

- **修复device 模式下使用多个 altsetting 时重复关闭端点问题，改成 altsetting 为0时关闭**
- **重构主机 audio 解析描述符**
- **增加 kinetis usbip**
- 主机下 usbh_msc_get_maxlun 请求部分 U 盘不支持，不做错误返回
- 主机下 usbh_hid_get_report_descriptor 导出给用户调用
- 静态代码检查
- github action 功能

v1.4.2
----------------------

- device 实现 USB_REQUEST_GET_INTERFACE 请求
- **device video 传输重构，增加双缓冲功能**
- device ecm 重构，保持和 rndis 类似 API
- device 和 host audio 音量配置功能重构
- host 增加 AOA 驱动
- 兼容 C++ 相关修改
- fsdev 不支持 ISO 和 DWC2 高速 hub 不支持全速低速检查
- **通用 OHCI 代码更新**

v1.4.3
----------------------

- **device ep0 处理增加线程模式**
- device audio feedback 宏和demo
- device rndis 增加透传功能（无LWIP）
- **host msc 将 scsi 初始化从枚举线程中移出，在mount阶段调用，并增加了testunity 多次尝试，兼容一部分 U 盘**
- rp2040 主从支持
- **nuttx fs，serial，net 组件支持**
- dwc2、ehci、ohci 主机 dcache功能支持（v1.5.0 完善）
- t113、MCXA156、CH585 、 **stm32h7r 支持**
- 修复 v1.4.1 中 altsetting 为0时应该关闭所有端点的问题

v1.5.0
----------------------

- **协议栈内部全局 buffer 需要使用 USB_ALIGN_UP 对齐, 用于开启 dcache 并且不使能 nocache 时使用**
- **完善 ehci/ohci dcache 模式下的处理**， add CONFIG_USB_EHCI_DESC_DCACHE_ENABLE for qh&qtd&itd, add CONFIG_USB_OHCI_DESC_DCACHE_ENABLE for ed&td
- **平台代码更新，平台相关转移到 platform，增加 lvgl 键鼠支持，blackmagic 支持，filex 支持**
- **device sof callback 支持**
- **dwc2 、fsdev st 下实现底层 API 和中断，直接调用 HAL_PCD_MSP 和 HAL_HCD_MSP，不需要用户复制粘贴**
- **DWC2 实现 SPLIT 功能，高速模式下支持外部高速 hub 对接 FS/LS 设备**
- liteos-m, zephyr os 支持
- device msc 裸机读写采用变量模式，而不是ringbuffer
- ehci qtd 使用 qtd alloc & free，节省内存，目前是 qh 携带 qtd
- rndis/ECM device， msc demo 更新，支持 rt-thread 下免修改
- **memcpy 全部使用 usb_memcpy 替换，arm 库存在非对其访问问题**
- **重构 device mtp 驱动（收费使用）**
- **重构device video 传输，直接在图像数据中填充 uvc header，达到zero memcpy**
- **增加 usb_osal_thread_schedule_other api，用于在释放 class 资源之前，先释放所有 class 线程，避免释放 class 资源以后线程还在使用该 class 资源**
- **dwc2 device 增加 dcache 功能，可用于 STM32H7/H7R/ESP32P4**
- ch32 device iso 更新
- cmake，scons，kconfig 更新
- 使用 USB_ASSERT_MSG 对部分代码检查
- N32H4，mm32f5 支持