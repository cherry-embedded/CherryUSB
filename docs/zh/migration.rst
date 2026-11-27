部分改动迁移指南
========================

部分改动可能使得旧代码无法用于新代码上，需要根据具体改动进行相应调整。

v1.5.0
------------------

dwc2 glue
^^^^^^^^^^^^^^^^^^^^

dwc2 从 v1.5.0 开始 glue 文件内置底层初始化，比如 `usb_dc_low_level_init`，底层依赖 `HAL_PCD_MspInit` 和 `HAL_HCD_MspInit`，必须使用 stm32cubemx 生成。第三方平台不保证有这些函数实现，自行检查。

v1.5.1
------------------

dwc2_user_params
^^^^^^^^^^^^^^^^^^^^

dwc2 从 v1.5.1 开始新增 `struct dwc2_user_params`，用于实现多 dwc2 port 不同配置。并替代 `usbd_get_dwc2_gccfg_conf` 和 `usbh_get_dwc2_hccfg_conf` 函数，
并增加 `dwc2_get_user_params` 函数实现，举例如下：

.. code-block:: C

    #ifndef CONFIG_USB_DWC2_CUSTOM_PARAM
    void dwc2_get_user_params(uint32_t reg_base, struct dwc2_user_params *params)
    {
        memcpy(params, &param_common, sizeof(struct dwc2_user_params));
    #ifdef CONFIG_USB_DWC2_CUSTOM_FIFO
        struct usb_dwc2_user_fifo_config s_dwc2_fifo_config;

        dwc2_get_user_fifo_config(reg_base, &s_dwc2_fifo_config);

        params->device_rx_fifo_size = s_dwc2_fifo_config.device_rx_fifo_size;
        for (uint8_t i = 0; i < MAX_EPS_CHANNELS; i++) {
            params->device_tx_fifo_size[i] = s_dwc2_fifo_config.device_tx_fifo_size[i];
        }
    #endif
    }
    #endif

v1.6.0
------------------

usbh_initialize
^^^^^^^^^^^^^^^^^^^^

usbh_initialize 从 v1.6.0 开始新增 event_handler 参数，通常不需要使用，可以传入 NULL。

host serial class
^^^^^^^^^^^^^^^^^^^^

从 v1.6.0 开始，主机增加 host serial 框架，用于统一所有类串口设备。以下 API 需要使用新 serial API 替换：

.. code-block:: C

    int usbh_xxx_set_line_coding(struct usbh_xxx *xxx_class, struct cdc_line_coding *line_coding);
    int usbh_xxx_get_line_coding(struct usbh_xxx *xxx_class, struct cdc_line_coding *line_coding);
    int usbh_xxx_set_line_state(struct usbh_xxx *xxx_class, bool dtr, bool rts);

    int usbh_xxx_bulk_in_transfer(struct usbh_xxx *xxx_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
    int usbh_xxx_bulk_out_transfer(struct usbh_xxx *xxx_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

替换为：

.. code-block:: C

    struct usbh_serial *usbh_serial_open(const char *devname, uint32_t open_flags);
    int usbh_serial_close(struct usbh_serial *serial);
    int usbh_serial_control(struct usbh_serial *serial, int cmd, void *arg);
    int usbh_serial_write(struct usbh_serial *serial, const void *buffer, uint32_t buflen);
    int usbh_serial_read(struct usbh_serial *serial, void *buffer, uint32_t buflen);


v1.7.0
------------------

psc thread
^^^^^^^^^^^^^^^^^^^^

主机枚举线程从默认 0 改成 29，应用线程优先级如果比枚举线程低，创建线程会报错。

host audio
^^^^^^^^^^^^^^^^^^^^

主机 audio 驱动重构，API 采用新的接口，旧接口不再兼容。 volume 单位从 db 改成 0 ~ 100%。

usbh_control_transfer
^^^^^^^^^^^^^^^^^^^^^^^^^^^

usbh_control_transfer 返回值删除了 setup 的 8 字节长度，方便做数据长度的判断。

net class
^^^^^^^^^^^^^^^^^^^^^^^^^^^

net class 内置线程，应用无需手动创建，因此需要增加  `CONFIG_USBHOST_NET_THREAD_PRIO` 和 `CONFIG_USBHOST_NET_THREAD_STACKSIZE` 配置项，用于设置线程优先级和栈大小。

video frame 描述符
^^^^^^^^^^^^^^^^^^^^

VIDEO_VS_FRAME_xxx_DESCRIPTOR_INIT 删除了多个 interval 功能，改成只用一个 interval。