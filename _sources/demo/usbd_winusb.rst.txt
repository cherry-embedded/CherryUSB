usbd_winusb
===============

本节主要介绍 winusb 驱动。winusb 是 windows 为了让用户友好的访问 USB 自定义类设备提供的一套通用驱动，其实本质就是 CDC ACM，只不过没有设置波特率的命令。
WINUSB 版本根据 USB 版本分为 V1/V2 版本，V2 版本需要包含 BOS 描述符，V1 版本不需要。 **V2 版本需要在设备描述符中设置为 USB2.1 的版本号**。

.. note:: 更换 winusb 描述符任意配置可能会枚举成功但是无法识别设备，需要删除 计算机\HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags 下所有注册表项，重新插拔设备后生效。

- V1 版本注册描述符

.. code-block:: C

    const struct usb_descriptor winusbv1_descriptor = {
        .device_descriptor_callback = device_descriptor_callback,
        .config_descriptor_callback = config_descriptor_callback,
        .device_quality_descriptor_callback = device_quality_descriptor_callback,
        .string_descriptor_callback = string_descriptor_callback,
        .msosv1_descriptor = &msosv1_desc
    };

    OR

    usbd_msosv1_desc_register(busid, &msosv1_desc);

- V2 版本注册描述符

.. code-block:: C

    const struct usb_descriptor winusbv2_descriptor = {
        .device_descriptor_callback = device_descriptor_callback,
        .config_descriptor_callback = config_descriptor_callback,
        .device_quality_descriptor_callback = device_quality_descriptor_callback,
        .string_descriptor_callback = string_descriptor_callback,
        .msosv2_descriptor = &msosv2_desc,
        .bos_descriptor = &bos_desc,
    };

    OR

    usbd_bos_desc_register(busid, &bos_desc);
    usbd_msosv2_desc_register(busid, &msosv2_desc);


- 接口描述符注册

.. code-block:: C

    /* Interface 0 */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02),
    /* Endpoint OUT 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_OUT_EP, USB_ENDPOINT_TYPE_BULK, WINUSB_EP_MPS, 0x00),
    /* Endpoint IN 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_IN_EP, USB_ENDPOINT_TYPE_BULK, WINUSB_EP_MPS, 0x00),

- 读写操作与 CDC ACM 相同，不再赘述