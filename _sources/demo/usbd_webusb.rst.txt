usbd_webusb
===============

本 demo 主要演示 webusb 功能，webusb 主要用于弹出网页并对 USB 设备进行访问。示例使用 webusb_hid_template.c。

- 在注册描述符时注册 BOS, MSOSV2, WEBUSB 描述符即可。

.. code-block:: C

    usbd_bos_desc_register(busid, &bos_desc);
    usbd_msosv2_desc_register(busid, &msosv2_desc);
    usbd_webusb_desc_register(busid, &webusb_url_desc);

- 增加一个接口描述符用于 webusb

.. code-block:: C

USB_INTERFACE_DESCRIPTOR_INIT(USBD_WEBUSB_INTF_NUM, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00)

- 其余使用 hid 描述符，不再赘述
- 枚举完成后，电脑右下角会弹出网页信息，点击即可打开网页