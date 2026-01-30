Writing Vendor Host Driver
============================================

This section mainly introduces how to write a vendor host driver.

- First copy a class/template/usbh_xxx.c file

- Define class driver and use CLASS_INFO_DEFINE prefix, so that after enumeration is completed, the protocol stack automatically finds the corresponding driver through usbd_class_find_driver.

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


- Implement connect and disconnect functions. In the connect function, you need to allocate an xxx_class structure. In the disconnect function, release the urb and xxx_class.

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

- Initialize endpoints

.. code-block:: C

        for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            USBH_EP_INIT(xxx_class->intin, ep_desc);
        } else {
            USBH_EP_INIT(xxx_class->intout, ep_desc);
        }
    }

- Finally design send/receive APIs, design them as synchronous or asynchronous according to actual conditions.

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