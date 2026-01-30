HID Host
=================

This section mainly introduces the use of Host HID class.

- Create a one-time thread in HID enumeration completion callback

.. code-block:: C


    void usbh_hid_run(struct usbh_hid *hid_class)
    {
        usb_osal_thread_create("usbh_hid", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_hid_thread, hid_class);
    }

    void usbh_hid_stop(struct usbh_hid *hid_class)
    {
    }


- Here we use the asynchronous operation of usbh_submit_urb, process data in interrupt and continue to receive next data.

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

- Of course, you can also not use asynchronous operations, but use synchronous operations with timeout.
- HID uses interrupt transfer, so normally we need to set a timer based on **bInterval** to trigger interrupt transfer at regular intervals. This is not used in the demo. If you have precise time requirements, you can choose to use a timer to trigger asynchronous sending.
- Taking hub communication as an example, a one-time timer is used, but a periodic timer can also be used.

.. code-block:: C

    hub->int_timer = usb_osal_timer_create("hubint_tim", USBH_GET_URB_INTERVAL(hub->intin->bInterval, hport->speed) / 1000, hub_int_timeout, hub, 0);

.. note::

    Here `USBH_GET_URB_INTERVAL` is a macro definition used to calculate the URB transfer interval time based on binterval. The unit is us, while the timer minimum is ms, so it needs to be divided by 1000. For intervals less than or equal to 1ms, no timer is needed.