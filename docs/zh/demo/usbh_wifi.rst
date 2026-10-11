WIFI Host
=================

CherryUSB 只提供厂商 Wi-Fi 网卡的 USB 传输。扫描、关联、WPA2 和 IP 协议栈
都在应用侧。

AIC8800
-----------------

``class/vendor/wifi/usbh_aic8800.c`` 匹配厂商接口 ``FF/FF/FF``，绑定 bulk
管道后调用 ``usbh_aic8800_run()``。覆盖这个弱符号以下载固件、启动 FullMAC；
断开时覆盖 ``usbh_aic8800_stop()``。

class **不实现 Wi-Fi**，也不会启动 lwIP。

枚举过程
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

常见 AIC8800D80 网卡：

1. ``1111:1111`` 虚拟光驱（ZeroCD）
2. ``a69c:8d80`` BootROM
3. ``a69c:8d81`` 运行时 Wi-Fi（常是复合设备）

ZeroCD 需要 MSC，并在 ``usb_config.h`` 打开：

.. code-block:: c

   #define CONFIG_USBHOST_HUB_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_NO_CSW
   #define CONFIG_USBHOST_MSC_MODESWITCH_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_DELAY_MS 350
   #define CONFIG_USBHOST_AIC8800_ZEROCD

在 ``usbh_initialize()`` 之前调用一次
``usbh_aic8800_enable_zero_cd_modeswitch()``。该 CBW 没有 CSW，重连脉冲
很短，主机必须强制 hub 重新枚举。

运行时复合设备
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``8d81`` 通常是：

- 接口 2：Wi-Fi ``FF/FF/FF``（本 class）
- 两个蓝牙 ``e0/01/01`` 接口（需要时用 ``usbh_bluetooth``）

没有设置 ``USB_CLASS_MATCH_INTF_NUM``，所以即使 ``bInterfaceNumber`` 是 2
也能绑到 ``FF/FF/FF``。

端点
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

第一对 bulk IN/OUT 是数据管道，第二对（如果有）是消息管道。
``usbh_aic8800_bulk_send()`` / ``usbh_aic8800_bulk_receive()`` 用
``message_pipe`` 选择。

最小对接
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

只做 USB：``demo/usbh_aic8800_template.c``。
上网：``demo/aic8800/``。

.. code-block:: c

   usbh_aic8800_enable_zero_cd_modeswitch();
   usbh_initialize(0, USB_BASE, NULL);

   void usbh_aic8800_run(struct usbh_aic8800 *aic8800_class)
   {
       if (!aic8800_class->runtime) {
           /* 下载固件，等待 8d81 */
       } else {
           /* 应用里启动 FullMAC / lwIP */
       }
   }

编译
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

打开 ``CONFIG_CHERRYUSB_HOST_AIC8800``（CMake）或
``CHERRYUSB_HOST_AIC8800`` / ``PKG_CHERRYUSB_HOST_AIC8800``（Kconfig）。
使用 ZeroCD 时还要打开 host MSC。链接时加 ``-u aic8800_class_info``。

BL616
-----------------

``demo/usbh_bl616_wifi_cli.c`` 是对 BL616 AT 固件走 CDC ACM，不是 AIC8800
FullMAC 驱动。
