WIFI Host
=================

CherryUSB 只提供厂商 Wi-Fi 网卡的 USB 传输。扫描、关联、WPA2 和 IP 协议栈
都在应用侧。

AIC8800
-----------------

已验证硬件：AIC8800D80 U02 USB 网卡（芯片寄存器 ``0xf3078820``，revision
``0x07``，非 H）+ STM32H7R7 + DWC2 Host HS。STA 只支持 WPA2-PSK/CCMP。

分层
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``class/vendor/wifi/usbh_aic8800.c`` 匹配厂商接口 ``FF/FF/FF``，绑定
bulk 管道后调用 ``usbh_aic8800_run()``。覆盖这个弱符号以下载固件、启动
FullMAC；断开时覆盖 ``usbh_aic8800_stop()``。

class **不实现 Wi-Fi**，也不会启动 lwIP。

``demo/aic8800/`` 是配合该 class 的 STA 协议栈：

- ``aic8800_boot.c``：在 BootROM 上下载五份 U02 镜像
- ``aic8800_fmac.c``：扫描 / 关联 / EAPOL / 数据通路
- ``aic8800_wpa2.c``：PMK 与 GTK
- ``aic8800_lwip.c``：可选 netif + DHCP
- ``aic8800_wifi.c``：应用 API

枚举过程
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

常见 AIC8800D80 网卡：

1. ``1111:1111`` 虚拟光驱（ZeroCD）
2. ``a69c:8d80`` BootROM
3. ``a69c:8d81`` 运行时 Wi-Fi（常是复合设备）

ZeroCD 需要 host MSC，并在 ``usb_config.h`` 打开：

.. code-block:: c

   #define CONFIG_USBHOST_HUB_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_NO_CSW
   #define CONFIG_USBHOST_MSC_MODESWITCH_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_DELAY_MS 350
   #define CONFIG_USBHOST_AIC8800_ZEROCD

在 ``usbh_initialize()`` 之前调用一次
``usbh_aic8800_enable_zero_cd_modeswitch()``。该 CBW 没有 CSW，重连脉冲
很短，主机必须强制 hub 重新枚举。本板需要 350 ms 延时。

运行时复合设备
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``8d81`` 通常是：

- 接口 2：Wi-Fi ``FF/FF/FF``（本 class）
- 两个蓝牙 ``e0/01/01`` 接口（需要时用 ``usbh_bluetooth``）

没有设置 ``USB_CLASS_MATCH_INTF_NUM``，所以即使 ``bInterfaceNumber`` 是 2
也能绑到 ``FF/FF/FF``。蓝牙关闭时，接口 0/1 打印 “class not supported”
是预期现象。

端点
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

第一对 bulk IN/OUT 是数据管道，第二对（如果有）是消息管道。
``usbh_aic8800_bulk_send()`` / ``usbh_aic8800_bulk_receive()`` 用
``message_pipe`` 选择。已验证网卡上 BootROM ``8d80`` 只有一对
（data=``82/01``），runtime ``8d81`` 为 data=``81/01``、
message=``82/02``。

固件
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``demo/aic8800/fw/`` 里是从 ``ThirdParty/AIC8800D80/firmware`` 拷来的
五份 D80 U02 镜像：

- ``fw_patch_table_8800d80_u02.bin``（1384）
- ``fw_adid_8800d80_u02.bin``（1708）→ ``0x00201940``
- ``fw_patch_8800d80_u02.bin``（32700）→ ``0x001E0000``
- ``fw_patch_8800d80_u02_ext0.bin``（16136）→ ``0x0020B43C``
- ``fmacfw_8800d80_u02.bin``（358072）→ ``0x00120000``

来源：Radxa ``aic8800`` 提交
``df4c783b663eba1956579c681acd5e45f25c671d``。许可证：GPL-3.0
（``demo/aic8800/fw/LICENSE.GPL-3.0``）。不要把这些 bin 当成
Apache-2.0 的 CherryUSB 源码。

在 ``usbh_initialize()`` 之前注册：

.. code-block:: c

   aic8800_fw_set_images(images);    /* 五份原始 bin */
   /* 或 */
   aic8800_fw_set_package(pkg, len); /* 打包后的 AICFWPKG */
   /* 或映射 AICFWPKG，并设置 AIC8800_FW_PACKAGE_BASE */

镜像必须保持可读，直到 BootROM 下载结束。映射包会做 CRC32 校验。
已验证板子把 410256 字节映射在 ``0x91F00000``。

应用对接
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

只做 USB：``demo/usbh_aic8800_template.c``。
上网：``demo/aic8800/usbh_aic8800_wifi_template.c``。

.. code-block:: c

   aic8800_wifi_init();
   aic8800_wifi_set_sta("your-ssid", "your-password");
   aic8800_wifi_set_event_callback(on_wifi, NULL);
   usbh_aic8800_enable_zero_cd_modeswitch();
   usbh_initialize(0, USB_BASE, NULL);

把 ``aic8800_wifi_config_template.h`` 复制为
``aic8800_wifi_config.h``。模板里不要填真实密码。

``AIC8800_WIFI_EVENT_READY`` 之后可用：

- ``aic8800_wifi_scan()``
- ``aic8800_wifi_connect()``（会再扫一遍再关联）
- ``aic8800_wifi_disconnect()``

``AIC8800_WIFI_AUTO_CONNECT``（默认 1）在枚举后自动排队连接。
事件：``READY``、``SCAN_DONE``、``CONNECTED``、``GOT_IP``、
``DISCONNECTED``、``CONNECT_FAILED``。

若 lwIP 已经有 tcpip 线程，把 ``AIC8800_LWIP_OWN_TCPIP`` 设为 ``0``。
FMAC TX 队列用 ``AIC8800_FMAC_TX_QUEUE_SECTION`` 放到 DMA 安全 RAM。

编译
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

打开 ``CONFIG_CHERRYUSB_HOST_AIC8800``（CMake）或
``CHERRYUSB_HOST_AIC8800`` / ``PKG_CHERRYUSB_HOST_AIC8800``（Kconfig）。
使用 ZeroCD 时还要打开 host MSC。链接
``class/vendor/wifi/usbh_aic8800.c`` 时加 ``-u aic8800_class_info``，
避免 class 表被丢掉。

限制
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- 仅 STA WPA2-PSK/CCMP
- 仅 D80 U02 非 H 固件组
- group CCMP key 第一次确认可能返回 ``-14``，随后会重试
- 数据通路 TX 线程仍依赖 FreeRTOS；boot / FMAC 工作线程已用
  ``usb_osal``

BL616
-----------------

``demo/usbh_bl616_wifi_cli.c`` 是对 BL616 AT 固件走 CDC ACM，不是 AIC8800
FullMAC 驱动。
