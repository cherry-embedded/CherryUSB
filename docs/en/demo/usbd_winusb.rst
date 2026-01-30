WinUSB Device
=================

This section mainly introduces the winusb driver. Winusb is a general driver provided by Windows to allow users to access USB custom class devices in a user-friendly manner. It is essentially CDC ACM, but without baud rate setting commands.
WINUSB versions are divided into V1/V2 versions according to USB versions. V2 version requires BOS descriptor, while V1 version does not. **V2 version requires setting USB2.1 version number in device descriptor**.

.. note:: Changing any winusb descriptor configuration may result in successful enumeration but inability to recognize the device. You need to delete all registry entries under Computer\HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags, then unplug and replug the device to take effect.

- V1 version descriptor registration

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

- V2 version descriptor registration

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


- Interface descriptor registration

.. code-block:: C

    /* Interface 0 */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02),
    /* Endpoint OUT 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_OUT_EP, USB_ENDPOINT_TYPE_BULK, WINUSB_EP_MPS, 0x00),
    /* Endpoint IN 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_IN_EP, USB_ENDPOINT_TYPE_BULK, WINUSB_EP_MPS, 0x00),

- Read and write operations are the same as CDC ACM, no further elaboration