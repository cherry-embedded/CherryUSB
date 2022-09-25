USB 虚拟串口（无 UART 功能）
============================

USB 虚拟串口主要是借助 USB CDC ACM 类实现，将其模拟成一个 VCP 设备，当插在电脑上的时候，可以显示成一个串口设备。跟市面上的 USB2TTL模块的区别在于，虚拟串口仅仅只使用到了 USB ,没有与串口（UART外设）进行连动。

软件实现
------------

详细代码参考 `demo/cdc_acm_template.c`

.. code-block:: C

    usbd_desc_register(cdc_descriptor);
    usbd_add_interface(usbd_cdc_acm_alloc_intf());
    usbd_add_interface(usbd_cdc_acm_alloc_intf());
    usbd_add_endpoint(&cdc_out_ep);
    usbd_add_endpoint(&cdc_in_ep);
    usbd_initialize();

- 调用 `cdc_acm_init` 配置 cdc acm 描述符并初始化 usb 硬件
- 因为 cdc 有 2 个接口，所以我们需要调用 `usbd_add_interface` 2 次

.. code-block:: C

    void usbd_configure_done_callback(void)
    {
        /* setup first out ep read transfer */
        usbd_ep_start_read(CDC_OUT_EP, read_buffer, 2048);
    }

    void usbd_cdc_acm_bulk_out(uint8_t ep, uint32_t nbytes)
    {
        USB_LOG_RAW("actual out len:%d\r\n", nbytes);
        // for (int i = 0; i < 100; i++) {
        //     printf("%02x ", read_buffer[i]);
        // }
        // printf("\r\n");
        /* setup next out ep read transfer */
        usbd_ep_start_read(CDC_OUT_EP, read_buffer, 2048);
    }

    void usbd_cdc_acm_bulk_in(uint8_t ep, uint32_t nbytes)
    {
        USB_LOG_RAW("actual in len:%d\r\n", nbytes);

        if ((nbytes % CDC_MAX_MPS) == 0 && nbytes) {
            /* send zlp */
            usbd_ep_start_write(CDC_IN_EP, NULL, 0);
        } else {
            ep_tx_busy_flag = false;
        }
    }

    void usbd_cdc_acm_set_dtr(uint8_t intf, bool dtr)
    {
        if (dtr) {
            dtr_enable = 1;
        } else {
            dtr_enable = 0;
        }
    }

    void cdc_acm_data_send_with_dtr_test(void)
    {
        if (dtr_enable) {
            memset(&write_buffer[10], 'a', 2038);
            ep_tx_busy_flag = true;
            usbd_ep_start_write(CDC_IN_EP, write_buffer, 2048);
            while (ep_tx_busy_flag) {
            }
        }
    }

- `usbd_cdc_acm_set_dtr` 函数是主机发送流控命令时的回调函数，这里我们使用 dtr ，当开启 dtr 时，启动发送
- `usbd_configure_done_callback` 是枚举完成的回调函数，因为 cdc acm 有 out 端点，所以我们需要在这里启动第一次数据的接收，当然，如果你现在没有能力接收数据，可以不启动。 **数据长度需要是最大包长的整数倍**。
- `usbd_cdc_acm_bulk_out` 是接收完成中断回调，我们在这里面启动下一次接收
- `usbd_cdc_acm_bulk_in` 是发送完成中断回调，我们在这里检查发送长度是否是最大包长的整数，如果是，需要发送 zlp 包表示结束
- 调用 `usbd_ep_start_write` 进行发送，需要注意，如果返回值小于0，不能执行下面的 while