WIFI Host
=================

CherryUSB only hosts the USB transport for vendor Wi-Fi dongles. Scan,
association, WPA2 and the IP stack stay in the application.

AIC8800
-----------------

Validated hardware: AIC8800D80 U02 USB stick (chip register
``0xf3078820``, revision ``0x07``, non-H) on STM32H7R7 + DWC2 host HS.
STA mode is WPA2-PSK/CCMP only.

Layers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``class/vendor/wifi/usbh_aic8800.c`` matches vendor-specific ``FF/FF/FF``
interfaces. It binds bulk pipes, then calls ``usbh_aic8800_run()``.
Override that weak hook to load firmware and start FullMAC. Override
``usbh_aic8800_stop()`` on disconnect.

The class does **not** implement Wi-Fi and does not start lwIP.

``demo/aic8800/`` is the STA stack used with this class:

- ``aic8800_boot.c`` downloads the five U02 images over BootROM
- ``aic8800_fmac.c`` runs scan / associate / EAPOL / data
- ``aic8800_wpa2.c`` derives the PMK and unwraps the GTK
- ``aic8800_lwip.c`` is optional netif + DHCP glue
- ``aic8800_wifi.c`` is the application API

Enumeration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Typical AIC8800D80 stick:

1. ``1111:1111`` virtual CD-ROM (ZeroCD)
2. ``a69c:8d80`` BootROM
3. ``a69c:8d81`` runtime Wi-Fi (often a composite device)

ZeroCD needs host MSC plus these macros in ``usb_config.h``:

.. code-block:: c

   #define CONFIG_USBHOST_HUB_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_NO_CSW
   #define CONFIG_USBHOST_MSC_MODESWITCH_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_DELAY_MS 350
   #define CONFIG_USBHOST_AIC8800_ZEROCD

Call ``usbh_aic8800_enable_zero_cd_modeswitch()`` once before
``usbh_initialize()``. The modeswitch CBW has no CSW. The reconnect
pulse is short, so the host must force hub re-enumeration. On this
board the 350 ms delay is required.

Runtime composite
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Runtime ``8d81`` usually exposes:

- interface 2: Wi-Fi ``FF/FF/FF`` (this class)
- two Bluetooth ``e0/01/01`` interfaces (use ``usbh_bluetooth`` if needed)

``USB_CLASS_MATCH_INTF_NUM`` is not set, so the class binds the matching
``FF/FF/FF`` interface even when ``bInterfaceNumber`` is 2. With
Bluetooth disabled, CherryUSB logs “class not supported” on interfaces
0 and 1; that is expected.

Endpoints
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The first bulk IN/OUT pair is the data pipe. A second pair, if present,
is the message pipe. ``usbh_aic8800_bulk_send()`` /
``usbh_aic8800_bulk_receive()`` take ``message_pipe`` to choose.
BootROM ``8d80`` uses a single pair (data=``82/01`` on the validated
stick). Runtime ``8d81`` uses data=``81/01`` and message=``82/02``.

Firmware
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``demo/aic8800/fw/`` contains the five D80 U02 binaries copied from
``ThirdParty/AIC8800D80/firmware``:

- ``fw_patch_table_8800d80_u02.bin`` (1384)
- ``fw_adid_8800d80_u02.bin`` (1708) → ``0x00201940``
- ``fw_patch_8800d80_u02.bin`` (32700) → ``0x001E0000``
- ``fw_patch_8800d80_u02_ext0.bin`` (16136) → ``0x0020B43C``
- ``fmacfw_8800d80_u02.bin`` (358072) → ``0x00120000``

Source: Radxa ``aic8800`` commit
``df4c783b663eba1956579c681acd5e45f25c671d``. License: GPL-3.0
(``demo/aic8800/fw/LICENSE.GPL-3.0``). Do not treat these blobs as
Apache-2.0 CherryUSB code.

Register them before ``usbh_initialize()``:

.. code-block:: c

   aic8800_fw_set_images(images);   /* five raw bins */
   /* or */
   aic8800_fw_set_package(pkg, len); /* packed AICFWPKG */
   /* or map AICFWPKG and set AIC8800_FW_PACKAGE_BASE */

Images must stay readable until BootROM download finishes. A mapped
package is CRC32-checked. The validated board maps 410256 bytes at
``0x91F00000``.

Application hook
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-only: ``demo/usbh_aic8800_template.c``.
STA + lwIP: ``demo/aic8800/usbh_aic8800_wifi_template.c``.

.. code-block:: c

   aic8800_wifi_init();
   aic8800_wifi_set_sta("your-ssid", "your-password");
   aic8800_wifi_set_event_callback(on_wifi, NULL);
   usbh_aic8800_enable_zero_cd_modeswitch();
   usbh_initialize(0, USB_BASE, NULL);

Copy ``aic8800_wifi_config_template.h`` to ``aic8800_wifi_config.h``.
Leave the template credentials empty.

After ``AIC8800_WIFI_EVENT_READY``:

- ``aic8800_wifi_scan()``
- ``aic8800_wifi_connect()`` (scans again, then associates)
- ``aic8800_wifi_disconnect()``

``AIC8800_WIFI_AUTO_CONNECT`` (default 1) queues connect on attach.
Events: ``READY``, ``SCAN_DONE``, ``CONNECTED``, ``GOT_IP``,
``DISCONNECTED``, ``CONNECT_FAILED``.

If lwIP already has a tcpip thread, set ``AIC8800_LWIP_OWN_TCPIP`` to
``0``. Put the FMAC TX queue in DMA-safe RAM with
``AIC8800_FMAC_TX_QUEUE_SECTION``.

Build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enable ``CONFIG_CHERRYUSB_HOST_AIC8800`` (CMake) or
``CHERRYUSB_HOST_AIC8800`` / ``PKG_CHERRYUSB_HOST_AIC8800`` (Kconfig).
Also enable the host MSC class when using ZeroCD. Link
``class/vendor/wifi/usbh_aic8800.c`` with ``-u aic8800_class_info`` so
the class table is not dropped.

Limits
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- STA WPA2-PSK/CCMP only
- D80 U02 non-H firmware set only
- First group CCMP key confirm may return ``-14`` and retry
- Data-path TX thread is still FreeRTOS-specific; boot/FMAC workers use
  ``usb_osal``

BL616
-----------------

``demo/usbh_bl616_wifi_cli.c`` talks to a BL616 AT firmware over CDC ACM.
It is not an AIC8800 FullMAC driver.
