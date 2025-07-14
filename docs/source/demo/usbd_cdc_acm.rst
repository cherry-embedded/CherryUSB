usbd_cdc_acm
===============

本 demo 主要用于演示 cdc acm 功能，包含收发测试，DTR 控制，ZLP 测试，性能测试。

- 开辟读写 buffer，用于收发数据，并且buffer需要用 nocache 修饰，这里我们读写都是用 2048字节，是为了后面的 ZLP 测试和性能测试使用。

.. code-block:: C

    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_buffer[2048]; /* 2048 is only for test speed , please use CDC_MAX_MPS for common*/
    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t write_buffer[2048];


- 在协议栈事件回调中，我们需要在枚举完成后启动第一次传输，并清除相关 flag，可以在 reset 事件中清除，也可以在 configured 事件中清除。

.. code-block:: C

    static void usbd_event_handler(uint8_t busid, uint8_t event)
    {
        switch (event) {
            case USBD_EVENT_RESET:
                break;
            case USBD_EVENT_CONNECTED:
                break;
            case USBD_EVENT_DISCONNECTED:
                break;
            case USBD_EVENT_RESUME:
                break;
            case USBD_EVENT_SUSPEND:
                break;
            case USBD_EVENT_CONFIGURED:
                ep_tx_busy_flag = false;
                /* setup first out ep read transfer */
                usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, 2048);
                break;
            case USBD_EVENT_SET_REMOTE_WAKEUP:
                break;
            case USBD_EVENT_CLR_REMOTE_WAKEUP:
                break;

            default:
                break;
        }
    }

- 在接收完成中断中继续发起接收；在发送完成中断中判断是否需要发送 ZLP。

.. code-block:: C

    void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
    {
        USB_LOG_RAW("actual out len:%d\r\n", nbytes);
        // for (int i = 0; i < 100; i++) {
        //     printf("%02x ", read_buffer[i]);
        // }
        // printf("\r\n");
        /* setup next out ep read transfer */
        usbd_ep_start_read(busid, CDC_OUT_EP, read_buffer, 2048);
    }

    void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
    {
        USB_LOG_RAW("actual in len:%d\r\n", nbytes);

        if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes) {
            /* send zlp */
            usbd_ep_start_write(busid, CDC_IN_EP, NULL, 0);
        } else {
            ep_tx_busy_flag = false;
        }
    }

- 以下是为了测试 DTR 功能并控制 USB 发送，DTR 和 RTS 只用于搭配 UART 使用，如果是纯 USB，没什么用，这里仅做测试。DTR 开关使用任意串口上位机并勾选 DTR。

.. code-block:: C

    void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
    {
        if (dtr) {
            dtr_enable = 1;
        } else {
            dtr_enable = 0;
        }
    }

- 在主函数中一直调用发送即可

.. code-block:: C

    void cdc_acm_data_send_with_dtr_test(uint8_t busid)
    {
        if (dtr_enable) {
            ep_tx_busy_flag = true;
            usbd_ep_start_write(busid, CDC_IN_EP, write_buffer, 2048);
            while (ep_tx_busy_flag) {
            }
        }
    }

- 上述我们需要注意，长度设置为 2048 是为了测试 ZLP 功能，通常实际使用时，接收长度应该使用 CDC_MAX_MPS 。具体原因参考 :ref:`usb_ext`
- 如果需要做性能测试，使用 tools/test_srcipts/test_cdc_speed.py 进行测试,并在测试之前删除 `usbd_cdc_acm_bulk_out` 和 `usbd_cdc_acm_bulk_in` 中的打印，否则会影响测试结果。


此外，对于 CDC ACM 搭配 OS 的情况，通常我们 read 使用异步并将数据存储到 ringbuffer 中，write 使用同步搭配 sem 使用。