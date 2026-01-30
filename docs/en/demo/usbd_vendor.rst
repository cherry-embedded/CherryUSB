Writing Vendor Device Driver
============================================

This section mainly introduces how to write a vendor device driver.

- First copy a class/template/usbd_xxx.c file
- Implement the following three callback functions. Generally speaking, vendor drivers only need to implement vendor_handlerDevice 驱动编写

.. code-block:: C

    intf->class_interface_handler = xxx_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = xxx_notify_handler;

- Example as follows

case1 demonstrates processing of host IN data, copying data to *data and specifying the length of *len. The protocol stack will automatically send to the host without requiring users to manually call send API.

case2 demonstrates processing of host OUT data. When this function is executed, it means all data has been received and can directly read data from *data with length *len.

.. code-block:: C

    static int xxx_vendor_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
    {
        USB_LOG_WRN("XXX Class request: "
                    "bRequest 0x%02x\r\n",
                    setup->bRequest);

        switch (setup->bRequest) {
            case 1:
            memcpy(*data, xxx, sizeof(xxx));
            *len = sizeof(xxx);
            case 2:
            hexdump(*data, *len);
            default:
                USB_LOG_WRN("Unhandled XXX Class bRequest 0x%02x\r\n", setup->bRequest);
                return -1;
        }

        return 0;
    }

- Finally register the interface using the form usbd_add_interface(busid, usbd_xxx_init_intf(&intf))