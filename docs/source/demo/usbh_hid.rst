usbh_hid
===============

本节主要介绍 HID 类的使用。

- HID 枚举完成回调中创建一次性线程

.. code-block:: C


    void usbh_hid_run(struct usbh_hid *hid_class)
    {
        usb_osal_thread_create("usbh_hid", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_hid_thread, hid_class);
    }

    void usbh_hid_stop(struct usbh_hid *hid_class)
    {
    }


- 这里我们使用 usbh_submit_urb 的异步操作，在中断中处理数据并继续接收下一次数据。

.. code-block:: C

    static void usbh_hid_thread(void *argument)
    {
        int ret;
        struct usbh_hid *hid_class = (struct usbh_hid *)argument;
        ;

        /* test with only one buffer, if you have more hid class, modify by yourself */

        /* Suggest you to use timer for int transfer and use ep interval */
        usbh_int_urb_fill(&hid_class->intin_urb, hid_class->hport, hid_class->intin, hid_buffer, hid_class->intin->wMaxPacketSize, 0, usbh_hid_callback, hid_class);
        ret = usbh_submit_urb(&hid_class->intin_urb);
        if (ret < 0) {
            goto delete;
        }
        // clang-format off
    delete:
        usb_osal_thread_delete(NULL);
        // clang-format on
    }

- 当然，也可以不使用异步操作，而是使用 timeout 的同步操作。
- HID 使用的是中断传输，因此正常来说，我们需要根据 **bInterval** 来设置定时器，定时触发中断传输，demo 这里没有使用，如果对时间有精确要求，可以选择使用定时器来触发异步发送。
- 以 hub 通信为例，采用的是一次性定时器，也可以使用周期性定时器。

.. code-block:: C

    hub->int_timer = usb_osal_timer_create("hubint_tim", USBH_GET_URB_INTERVAL(hub->intin->bInterval, hport->speed), hub_int_timeout, hub, 0);
