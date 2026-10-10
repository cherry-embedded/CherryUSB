版本说明
==============================

如果没有特别情况，请使用最新版本。下面只列举比较重要的更新，详细更新说明请参考 https://github.com/cherry-embedded/CherryUSB/releases。

小于等于 v0.10.2 版本
----------------------

- **主从机基本框架，API 定型，仅支持单个 USB IP**
- host: **USBIP 驱动硬件 pipe 跟随 ep 固定分配，超出则无法使用**

v1.0.0 过度版本
----------------------

- device: usbd_ep_open 参数改成 struct usb_endpoint_descriptor
- host: **USBIP 驱动重构，硬件 pipe 动态分配**
- 增加 USB 专用 errorcode

v1.1.0 过度版本
----------------------

- **主从机支持多 USB IP 且要相同 IP**
- host: **增加 bluetooth, ch340, ftdi, cp210x, asix class 驱动**
- device: **msc 支持多 lun**，并且 CONFIG_USBDEV_MSC_BLOCK_SIZE 修改为 CONFIG_USBDEV_MSC_MAX_BUFSIZE

v1.2.0
----------------------

- host: **增加 rtl8152，cdc ncm class 驱动**
- host: 增加 timer 去控制中断传输（hub修改为 timer 控制）
- port: **增加 esp，aic 主机驱动**
- port: **优化 DWC2 代码方便阅读，并增加一些 FIFO 配置宏给用户（因为 dwc2 fifo 大小有限，以及配置方式很多，所以导出给用户配置，方便合理控制性能）**
- port: 优化 ehci 驱动（qtd不再使用动态申请，绑定 qh），方便代码运行的更快，intr 传输使用统一的 qh head。

v1.3.0
----------------------

- device: **支持多种速度描述符自动选择功能（需开启 CONFIG_USBDEV_ADVANCE_DESC）**
- device: 字符串描述符使用 string 形式
- device: core 代码统一 ep0 buffer 的使用，用于美化代码
- host: **增加 pl2303 驱动**
- host: **采用 id table 来支持多个 vid，pid；增加 user_data 给用户使用**
- host: 网络 class 驱动增加 tx、rx buffer 的宏，增加 LWIP_TCPIP_CORE_LOCKING_INPUT 的使用，以便实现数据的零拷贝
- port: 导入 bouffalo，aic，stm32f723 device驱动
- port: **主机部分 urb->timeout 清0 的处理有点问题（大数据量传输时会出现 no pipe alloc 异常，主要原因是刚启动传输就完成了，还没判断 timeout就被修改为0了，没有进入 take sem 流程），此版本已修复**
- port: ehci enable iaad in usbh_kill_urb，read ehci hcor offset from hccr caplength，enable ohci for ehci
- 适配 nuttx os

v1.3.1
----------------------

- host: **hub 枚举线程删除，使用 psc 线程，枚举方式更改为队列模式，取消同时枚举多个设备的功能**
- host: 扫描驱动信息和 instance 采用递归模式，删除链表扫描
- host: 网络 class 驱动优化，支持接收 16K 以上的数据（cdc ecm 不支持），采用高级 memcpy api
- device: **协议栈中打印删除（中断中不再做打印）**
- port: **musb fifo配置修改为从 fifo table 获取（此代码参考 linux），适配 es32，sunxi，beken**
- port: fix enter section before alloc pipe

v1.4.0
----------------------

- device: **开始支持 remote wakeup 功能, hid request(0x21)，完善 GET STATUS 请求（此版本开始可以通过 USB3CV 测试）**
- device: **增加 UF2, ADB, WEBUSB 功能**； msc 增加裸机的读写 polling 功能，将读写放在 while1中执行； usbd_cdc 改名为 usbd_cdc_acm
- host: **增加 usbwifi(bl616), xbox驱动**
- host: **重构 USB3.0 枚举逻辑**
- host: **修复 cdc_acm,hid,msc,serial 传输共享 buffer，如果存在多个相同的设备会有问题，修改为单独的 buffer**
- port: **重构 XHCI/PUSB2 驱动，不开源**
- port: ehci 和 ohci 文件改名
- port: **增加 remote wakeup api**
- port: **chipidea 从机驱动支持，nxp mcx 系列主从支持，esp32p4 支持**
- threadx os 支持
- esp 组件库支持

v1.4.1
----------------------

- device: **修复device 模式下使用多个 altsetting 时重复关闭端点问题，改成 altsetting 为0时关闭**
- device: video 增加 usbd_video_stream_start_write 和 usbd_video_stream_split_transfer API，之前的 API 不再使用（占用太大）
- host: **重构主机 audio 解析描述符，重构 usbh_audio_open api**
- host: 主机下 usbh_msc_get_maxlun 请求部分 U 盘不支持，不做错误返回
- host: 主机下 usbh_hid_get_report_descriptor 导出给用户调用
- port: **增加 kinetis usbip**
- 静态代码检查

v1.4.2
----------------------

- device: 实现 USB_REQUEST_GET_INTERFACE 请求
- device: **video 传输继续重构，增加双缓冲功能，实现零 copy 功能**
- device: ecm 重构，保持和 rndis 类似底层读写 API
- device: **全面使用 usb_memcpy 替换 memcpy**
- **device 和 host: audio 音量配置功能重构**
- host: **增加 AOA 驱动**
- port: **通用 OHCI 代码更新**
- 兼容 C++ 相关修改

v1.4.3
----------------------

- device: **ep0 处理增加线程模式**
- device: 增加 audio feedback 宏和demo
- device: rndis 增加透传功能（无LWIP）
- device: **fix current_desc_len += p[DESC_bLength] before p+= p[DESC_bLength]**
- host: **msc 将 scsi 初始化从枚举线程中移出，在mount阶段调用，并增加了testunity 多次尝试，兼容一部分 U 盘**
- port: **rp2040 主从支持**
- port: **dwc2、ehci、ohci 主机 dcache功能支持（v1.5.0 完善）**
- port: 新增 t113、MCXA156、CH585、stm32h7r 支持
- port: **修复 v1.4.1 中 altsetting 为0时应该关闭所有端点的问题**
- **nuttx fs，serial，net 组件支持**
- 增加 usb_phyaddr2ramaddr 和 usb_ramaddr2phyaddr 宏

v1.5.0
----------------------

- **协议栈内部全局 buffer 需要使用 USB_ALIGN_UP 对齐, 用于开启 dcache 并且不使能 nocache ram 时使用**
- device: **默认使能 CONFIG_USBDEV_ADVANCE_DESC**
- device: **增加 ep0_next_state 状态记录，重构枚举过程 debug log**
- device: msc 裸机读写采用变量模式，而不是ringbuffer
- device: msc 支持 SCSI_CMD_SYNCHCACHE10 (0x35)
- device: **重构 device mtp 驱动（收费使用）**
- device: **device tmc 驱动（收费使用）**
- device: **重构 device video 传输，直接在图像数据中填充 uvc header，达到zero memcpy**
- host: **增加 usb_osal_thread_schedule_other api，用于在释放 class 资源之前，先释放所有 class 线程，避免释放 class 资源以后线程还在使用该 class 资源**
- host: hid 增加 usbh_hid_get_protocol API
- port: **完善 ehci/ohci dcache 模式下的对齐处理**， add CONFIG_USB_EHCI_DESC_DCACHE_ENABLE for qh&qtd&itd, add CONFIG_USB_OHCI_DESC_DCACHE_ENABLE for ed&td
- port: **device sof callback 支持**
- port: **dwc2 、fsdev 下实现底层 API 和中断，STM32 glue 直接调用 HAL_PCD_MSP 和 HAL_HCD_MSP，不再需要用户复制粘贴**
- port: **DWC2 实现 SPLIT 功能，高速模式下支持外部高速 hub 对接 FS/LS 设备**
- port: ehci qtd 使用 qtd alloc & free，节省内存，目前是 qh 携带 qtd
- port: **dwc2 device 增加 dcache 支持**
- port: **bouffalo/hpm/esp/st/nxp dcache api 支持**
- port: N32H4/MM32F5 device 支持
- platform: **平台代码更新，平台相关转移到 platform，增加 lvgl 键鼠支持，blackmagic 支持，filex 支持, zephyr disk支持，esp-idf netif支持**
- **memcpy 全部使用 usb_memcpy 替换，arm 库存在非对其访问问题**
- 使用 USB_ASSERT_MSG 对部分代码检查，全面性 warning 修复
- **增加 usb_osal_thread_schedule_other api，用于在释放 class 资源之前，先释放所有 class 线程，避免释放 class 资源以后线程还在使用该 class 资源**
- liteos-m, zephyr os 支持

v1.5.1
----------------------

- host: urb interval 单位改成 ms 改成 us
- port: **dwc2 增加多个 usbport 不同参数的配置功能（比如一个全速一个高速，fifo配置和phy配置不同），之前的 get gccfg API 作废，更新 at32，stm32，espressif glue**
- port: **ehci 在控制传输中如果没有 nodata 阶段会导致 data qtd 未释放，导致内存泄漏，1.5.0带来的问题**
- port: **dwc2 读取 setup 使用 usbd_get_next_ep0_state 去判断，避免 setup 和 ep0 out 在 USB_OTG_DOEPINT_XFRC 状态下冲突**
- port: sifli usb device 初步支持
- platform: 支持 rt-thread 下使用 adb shell，host serial/device cdc_acm 对接 rtdevice 框架

v1.5.2
----------------------

- host: 主机枚举中，删除描述符溢出相关的 ASSERT 操作，改成返回错误。获取字符串描述符改成支持才获取。2 ms 延时改成 10ms，因为一些 os 使用的是 100hz，会造成延时失效
- host: CONFIG_USBHOST_MAX_INTF_ALTSETTINGS 默认使用 2 减少内存，只有 UVC 和UAC 使用（商业收费），所以不需要开很大
- host: urb interval 从 u8 改 u32，最大支持 2^15 * 125us
- port: **dwc2 ep mult 支持**
- port: **dwc2 halt 中不能清除 USB_OTG_HCCHAR_EPDIR，会导致下次使用异常，因为当前传输还未停止**
- port: **dwc2 reset port 中使用超时机制，防止在枚举时由于拔出而造成死等**
- port: **优化 split 传输中 intr 传输处理和 cache 处理**
- port: 更新 kendryte glue 代码，使用新的 dwc2 参数配置函数（v1.5.1 提供）
- port: **musb 对于标准的 IP 结构采用独立 EP 控制寄存器组，不使用 EPIDX 寄存器去控制**
- port: esp32p4 fs 支持
- platform: 对 1.5.1 下 rt-thread 组件的一些 bugfix
- 删除所有 CONFIG_USBDEV_EP_NUM & CONFIG_USBHOST_PIPE_NUM，不再使用，因为 IP 本身会携带这些信息，或者厂家 SDK 提供了对应的宏
- idf timer osal 替换为 esp timer，freertos timer会有启动失败的可能性；xTaskCreate 使用 xTaskCreatePinnedToCore 替换，方便多核使用

v1.5.3
----------------------

- device: **从机支持自定义 ep0 mps，仅支持商业性 IP**
- host: **主机增加 UVC bulk支持**
- host: 接口号匹配驱动功能
- host: **主机分配地址功能改成循环自增模式**
- host: 增加自定义 config index 选择
- host: 重构 lsusb 命令
- host: **主机控制传输增加 retry 机制，部分 device 通信不稳定，retry 次数参考 linux**
- host: 主机 rndis 驱动增加非标 02/02/ff 接口驱动匹配规则
- port: **musb IP 关闭 multipoint feature 支持**
- port: **hpmicro、chipidea device dcache 支持**
- port: 更新 hc32 glue 代码，使用新的 dwc2 参数配置函数（v1.5.1 提供）
- port: **musb pipe 使用动态分配**
- platform: **idf host msc 支持**
- **otg: otg 框架重构，当前 port 仅支持 hpmicro**
- CI 编译功能，支持 hpmicro，espressif，bouffalolab
- 增加 mongoose demo

v1.5.3.99
----------------------

bugfix for v1.5.3。

.. note:: 从这个版本开始，可以全面使用 dcache ram 取代 nocache ram，需要打开 **CONFIG_USB_DCACHE_ENABLE** 并配置 **CONFIG_USB_ALIGN_SIZE**

v1.6.0
----------------------

- device: **支持 gamepad device**
- host: **增加 serial 框架，统一所有类串口驱动**
- host: **增加 hid 报告描述符解析功能**
- host: **usbh_initialize 增加 event callback，用于通知用户主机事件变化，通常不需要使用，设置为 NULL 即可**
- port: **增加 ti xmc，infineon edge e8x port 支持**
- port: dwc2 增加 usbd_dwc2_get_system_clock 替换 SystemCoreClock；删除 __UNALIGNED_UINT32_READ 和 __UNALIGNED_UINT32_WRITE 宏；读取 setup 个数设置为 1个；第一次读取 setup 移动到 USB_OTG_GINTSTS_ENUMDNE 中断中
- port: dwc2/ehci 增加 roothub 速度设置

v1.6.1
----------------------

- device: **新增 USB 副屏驱动**
- device: **重构 DFU device 驱动**
- host: **重构 hid 报告描述符解析功能**
- host: **主机 hub 线程释放改成使用 mq 退出，替代互斥锁**
- host: **fix desc length parse**
- port: **hpmicro: using sutw semaphore by isr handler for setup**
- port: dwc2: add critical section for hc frame setting, make it more safely
- platform: rtthread 文件添加 rt 前缀
- **cherryrb 和 cherrymp 组件内置**
- 部分 bugfix 和安全性代码检查

v1.7.0
----------------------

- **新增注册驱动 API，从这个版本开始可以同时使用多个不同的 USBIP**
- device: **新增 cdc ncm class 驱动**
- device: **新增 tmc class 驱动**
- device: **新增 usb3.0 request 和配置**
- device: **重构 adb 驱动，添加 shell 和 sync 支持，并且不再支持裸机，os only**
- device: **audio 新增 usbd_audio_get_volume_range，volume 读写改成 float 类型**
- device: 新增获取 speed 和 ep info api
- device: **新增 usb_vfs api, posix 标准接口**
- device: msc 支持 SCSI_readCapacity16 命令
- host: **重构 audio 驱动和 API，volume 配置范围改成 0~ 100，audio sample rate 最大支持 8个默认**
- host: **serial 增加 RX 流控**
- host: **简化 net class rx 接收，最大不再超过 16K**
- host: **rndis msg resp 处理统一到 usbh_rndis_parse_resp**
- host: rndis 驱动增加 Android RNDIS class code (0xEF/0x04/0x01) 匹配规则
- host: 修复 hid API 漏掉的 **hport->intf**
- host: only release hport in disconnect
- port: **renesas 从机支持**
- port: **esp32s31 支持**
- port: **N32H49X和N32H7XX USBHS 支持**
- port: **N32H4x (N32H47x_48x / N32H49x) FSDEV 支持**
- port: **删除旧版本 wch 驱动，重构 usbfs，usbhs 主从驱动支持**
- port: 统一 cache 处理到 usbh_submit_urb
- port: 将部分 glue 文件放到厂家文件夹中
- port: 修复 ehci setup cache 和 qh，qtd cache 处理
- port: **fix usbd_ep_close for out ep, set global nak first then can disable ep**
- port: **musb 增加 sifli 自定义 pipe 申请规则，修复 usbh_kill_urb 时错误关闭 ep0 中断和 flush 错误问题**
- port: low level api from weak to extern, 防止用户没有实现而使用 weak api
- platform: **zmk 支持**
- platform: **zephyr 主机多 msc 设备支持**
- platform: fix idf fatfs 旧版本兼容
- platform: 修复 rtthread 驱动旧版本支持
- platform: **rtthread adb shell 和 sync 支持**
- **demo 删除 CONFIG_USB_HS，新增 config fs，hs 和 other speed 支持**
- **video 宏中的 frame interval 改成只支持一个**
- 新增 AUDIO_FEEDBACK_TO_BUF_HS_INTERVAL/AUDIO_FEEDBACK_TO_BUF_FS_INTERVAL 宏
- 更新 usb 测速工具
- CI 功能新增 wch，sifli，phytium 支持
- osal 的部分修复
- 安全性代码检查