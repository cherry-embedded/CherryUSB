vendor host 驱动编写
===========================

本节主要介绍如何编写一个 vendor host 驱动。

- 首先复制一份 class/template/usbh_xxx.c 文件

- 定义 class 驱动并使用 CLASS_INFO_DEFINE 前缀，这样，枚举完成后，协议栈自动通过 usbd_class_find_driver 来查找对应的驱动。

.. code-block:: C

    static const struct usbh_class_driver xxx_class_driver = {
        .driver_name = "xxx",
        .connect = usbh_xxx_connect,
        .disconnect = usbh_xxx_disconnect
    };

    CLASS_INFO_DEFINE const struct usbh_class_info xxx_class_info = {
        .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
        .bInterfaceClass = 0,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .id_table = NULL,
        .class_driver = &xxx_class_driver
    };


- 实现 connect 和 disconnect 函数, 在 connect 函数中，需要分配一个 xxx_class 结构体，在 disconnect 函数中释放 urb 和 xxx_class。

.. code-block:: C

    struct usbh_xxx {
        struct usbh_hubport *hport;
        struct usb_endpoint_descriptor *xxxin;
        struct usb_endpoint_descriptor *xxxout;
        struct usbh_urb xxxin_urb;
        struct usbh_urb xxxout_urb;

        uint8_t intf; /* interface number */
        uint8_t minor;

        void *user_data;
    };

    static int usbh_xxx_connect(struct usbh_hubport *hport, uint8_t intf)
    {
        struct usb_endpoint_descriptor *ep_desc;
        int ret;

        struct usbh_xxx *xxx_class = usbh_xxx_class_alloc();
        if (xxx_class == NULL) {
            USB_LOG_ERR("Fail to alloc xxx_class\r\n");
            return -USB_ERR_NOMEM;
        }

        return ret;
    }


    static int usbh_xxx_disconnect(struct usbh_hubport *hport, uint8_t intf)
    {
        int ret = 0;

        struct usbh_xxx *xxx_class = (struct usbh_xxx *)hport->config.intf[intf].priv;

        if (xxx_class) {
            if (xxx_class->xxxin) {
                usbh_kill_urb(&xxx_class->xxxin_urb);
            }

            if (xxx_class->xxxout) {
                usbh_kill_urb(&xxx_class->xxxout_urb);
            }

            if (hport->config.intf[intf].devname[0] != '\0') {
                USB_LOG_INFO("Unregister xxx Class:%s\r\n", hport->config.intf[intf].devname);
                usbh_xxx_stop(xxx_class);
            }

            usbh_xxx_class_free(xxx_class);
        }

        return ret;
    }

- 初始化端点

.. code-block:: C

        for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(xxx_class->intin, ep_desc);
        } else {
            USBH_EP_INIT(xxx_class->intout, ep_desc);
        }
    }

- 最后设计收发 API，根据实际情况设计成同步 or 异步。

.. code-block:: C

    int usbh_xxx_in_transfer(struct usbh_xxx *xxx_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
    {
        int ret;
        struct usbh_urb *urb = &xxx_class->xxxin_urb;

        usbh_xxx_urb_fill(urb, xxx_class->hport, xxx_class->xxxin, buffer, buflen, timeout, NULL, NULL);
        ret = usbh_submit_urb(urb);
        if (ret == 0) {
            ret = urb->actual_length;
        }
        return ret;
    }

    int usbh_xxx_out_transfer(struct usbh_xxx *xxx_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
    {
        int ret;
        struct usbh_urb *urb = &xxx_class->xxxout_urb;

        usbh_xxx_urb_fill(urb, xxx_class->hport, xxx_class->xxxout, buffer, buflen, timeout, NULL, NULL);
        ret = usbh_submit_urb(urb);
        if (ret == 0) {
            ret = urb->actual_length;
        }
        return ret;
    }