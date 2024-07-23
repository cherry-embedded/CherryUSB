版本说明
==============================

如果没有特别情况，请使用最新版本.详细版本更新说明请参考 https://github.com/cherry-embedded/CherryUSB/releases。

<= v0.10.2 初代版本
----------------------

- **用于定基本的框架，仅支持单 USB IP**。
- **host 驱动每个 ep占用一个 硬件 pipe，不支持动态使用硬件 pipe**。
- 相关 porting 需要使用此版本，后续不再支持（比如 ch32，rp2040），以及旧版本pusb2 和 xhci（新版本不再提供源码）。

v1.0.0 过度版本
----------------------

- **host 支持动态使用硬件 pipe，不再固定**

v1.1.0 过度版本
----------------------

- **主从机支持多 USB IP 且要相同 IP**
- host 增加 bluetooth, ch340, ftdi, cp210x, asix 驱动
- device msc 支持多 lun，并且 CONFIG_USBDEV_MSC_BLOCK_SIZE 修改为 CONFIG_USBDEV_MSC_MAX_BUFSIZE

v1.2.0
----------------------

- host 增加 rtl8152，cdc ncm 驱动
- host 增加 timer 去控制中断传输（hub修改为 timer 控制）
- porting 增加 esp，aic 主机驱动
- 优化 DWC2 优化代码方便阅读，并增加一些 FIFO 配置宏给用户（因为 dwc2 fifo 大小有限，以及配置方式很多，所以导出给用户配置，方便合理控制性能）
- 优化 ehci 驱动（qtd不再使用动态申请，绑定 qh），方便代码运行的更快

v1.3.0
----------------------

- device 支持多种速度描述符自动选择功能（开启 CONFIG_USBDEV_ADVANCE_DESC）
- device core 代码统一 ep0 buffer 的使用，用于美化代码
- host 增加 pl2303 驱动，使用 id table 来支持多个 vid，pid，增加 user_data 给用户使用
- host 网络 class 驱动增加 tx、rx buffer的宏，增加 LWIP_TCPIP_CORE_LOCKING_INPUT 的使用，以便实现数据的零拷贝
- host hid 增加report api
- porting 导入 bouffalo，aic，stm32f723 device驱动
- porting 中主机部分 urb->timeout 清0 的处理有点问题（大数据量传输时会出现 no pipe alloc 异常，主要原因是刚启动传输就完成了，还没判断 timeout就被修改为0了，没有进入 take sem 流程），此版本已修复
- ehci enable iaad in usbh_kill_urb，read ehci hcor offset from hccr caplength，enable ohci for ehci
- 适配 nuttx os

v1.3.1
----------------------

- bugfix（audio，video，cdc ecm 相关宏，结构体，api）
- host hub 枚举线程删除，使用 psc 线程，枚举方式更改为队列模式，取消同时枚举多个设备的功能
- host 扫描驱动信息和 instance 采用递归模式，删除链表扫描
- host 网络 class 驱动优化，支持接收 16K 以上的数据（cdc ecm 不支持）
- 增加高级 memcpy api
- device 枚举相关删除打印（中断中不再做打印）
- porting 中 musb fifo配置修改为从 fifo table 获取（此代码参考 linux），适配 es32，sunxi，beken