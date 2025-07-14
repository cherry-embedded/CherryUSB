vendor device 驱动编写
===========================

本节主要介绍如何编写一个 vendor device 驱动。

- 首先复制一份 class/template/usbd_xxx.c 文件
- 实现以下三个回调函数，通常来说，vendor 驱动只需要实现 vendor_handler

.. code-block:: C

    intf->class_interface_handler = xxx_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = xxx_notify_handler;

- 举例如下

case1 演示对于主机 IN 数据的处理，将数据拷贝到 *data 中，并指定*len 的长度。协议栈会自动发送给主机，不需要用户手动调用发送 API。

case2 演示对于主机 OUT 数据的处理，当执行到此函数时，说明数据都已经接收完成，可以直接读取 *data 中的数据，长度为 *len。

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

- 最后使用形如 usbd_add_interface(busid, usbd_xxx_init_intf(&intf)) 注册接口