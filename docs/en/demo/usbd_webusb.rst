WebUSB Device
=================

This demo mainly demonstrates webusb functionality. Webusb is mainly used to pop up web pages and access USB devices. The example uses webusb_hid_template.c.

- When registering descriptors, just register BOS, MSOSV2, WEBUSB descriptors.

.. code-block:: C

    usbd_bos_desc_register(busid, &bos_desc);
    usbd_msosv2_desc_register(busid, &msosv2_desc);
    usbd_webusb_desc_register(busid, &webusb_url_desc);

- Add an interface descriptor for webusb

.. code-block:: C

    USB_INTERFACE_DESCRIPTOR_INIT(USBD_WEBUSB_INTF_NUM, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00)

- The rest use hid descriptors, no further elaboration
- After enumeration is completed, webpage information will pop up in the lower right corner of the computer, click to open the webpage