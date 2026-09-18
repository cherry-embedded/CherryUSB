WIFI Host
=================

CherryUSB only hosts the USB transport for vendor Wi-Fi dongles. Scan,
association, WPA2 and the IP stack stay in the application.

AIC8800
-----------------

``class/vendor/wifi/usbh_aic8800.c`` matches vendor-specific ``FF/FF/FF``
interfaces. It binds bulk pipes, then calls ``usbh_aic8800_run()``.
Override that weak hook to load firmware and start FullMAC. Override
``usbh_aic8800_stop()`` on disconnect.

The class does **not** implement Wi-Fi. It does not start lwIP.

Enumeration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Typical AIC8800D80 stick:

1. ``1111:1111`` virtual CD-ROM (ZeroCD)
2. ``a69c:8d80`` BootROM
3. ``a69c:8d81`` runtime Wi-Fi (often a composite device)

ZeroCD needs MSC plus these macros in ``usb_config.h``:

.. code-block:: c

   #define CONFIG_USBHOST_HUB_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_NO_CSW
   #define CONFIG_USBHOST_MSC_MODESWITCH_FORCE_REENUMERATE
   #define CONFIG_USBHOST_MSC_MODESWITCH_DELAY_MS 350
   #define CONFIG_USBHOST_AIC8800_ZEROCD

Call ``usbh_aic8800_enable_zero_cd_modeswitch()`` once before
``usbh_initialize()``. The modeswitch CBW has no CSW; the reconnect pulse
is short, so the host must force hub re-enumeration.

Runtime composite
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Runtime ``8d81`` usually exposes:

- interface 2: Wi-Fi ``FF/FF/FF`` (this class)
- two Bluetooth ``e0/01/01`` interfaces (use ``usbh_bluetooth`` if needed)

``USB_CLASS_MATCH_INTF_NUM`` is not set, so the class binds the matching
``FF/FF/FF`` interface even when ``bInterfaceNumber`` is 2.

Endpoints
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The first bulk IN/OUT pair is the data pipe. A second pair, if present,
is the message pipe. ``usbh_aic8800_bulk_send()`` /
``usbh_aic8800_bulk_receive()`` take ``message_pipe`` to choose.

Minimal hook
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB-only hook: ``demo/usbh_aic8800_template.c``.
Wi-Fi + lwIP: ``demo/aic8800/`` (see the stacked STA demo).

.. code-block:: c

   usbh_aic8800_enable_zero_cd_modeswitch();
   usbh_initialize(0, USB_BASE, NULL);

   void usbh_aic8800_run(struct usbh_aic8800 *aic8800_class)
   {
       if (!aic8800_class->runtime) {
           /* download firmware, then wait for 8d81 */
       } else {
           /* start FullMAC / lwIP in the application */
       }
   }

Build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Enable ``CONFIG_CHERRYUSB_HOST_AIC8800`` (CMake) or
``CHERRYUSB_HOST_AIC8800`` / ``PKG_CHERRYUSB_HOST_AIC8800`` (Kconfig).
Also enable the host MSC class when using ZeroCD. Link with
``-u aic8800_class_info`` so the class table is not dropped.

BL616
-----------------

``demo/usbh_bl616_wifi_cli.c`` talks to a BL616 AT firmware over CDC ACM.
It is not an AIC8800 FullMAC driver.
