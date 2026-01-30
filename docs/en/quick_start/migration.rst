Partial Changes Migration Guide
======================================

usbh_initialize
------------------

usbh_initialize has added event_handler parameter starting from v1.6.0. Usually not needed, can pass NULL.

dwc2 glue st
----------------

Starting from v1.5.0, dwc2 glue file has built-in low-level initialization, such as `usb_dc_low_level_init`, which depends on `HAL_PCD_MspInit` and `HAL_HCD_MspInit`. Must use stm32cubemx generation. Third-party platforms do not guarantee having these function implementations, check by yourself.


dwc2 glue
----------------

Starting from v1.5.1, dwc2 adds `struct dwc2_user_params` for implementing different configurations for multiple dwc2 ports. It replaces `usbd_get_dwc2_gccfg_conf` and `usbh_get_dwc2_hccfg_conf` functions,
and adds `dwc2_get_user_params` function implementation, example as follows:

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

host serial
----------------

Starting from v1.6.0, host adds host serial framework for unifying all serial-like devices. The following APIs need to be replaced with new serial APIs:

.. code-block:: C

    int usbh_xxx_set_line_coding(struct usbh_xxx *xxx_class, struct cdc_line_coding *line_coding);
    int usbh_xxx_get_line_coding(struct usbh_xxx *xxx_class, struct cdc_line_coding *line_coding);
    int usbh_xxx_set_line_state(struct usbh_xxx *xxx_class, bool dtr, bool rts);

    int usbh_xxx_bulk_in_transfer(struct usbh_xxx *xxx_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);
    int usbh_xxx_bulk_out_transfer(struct usbh_xxx *xxx_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

Replace with:

.. code-block:: C

    struct usbh_serial *usbh_serial_open(const char *devname, uint32_t open_flags);
    int usbh_serial_close(struct usbh_serial *serial);
    int usbh_serial_control(struct usbh_serial *serial, int cmd, void *arg);
    int usbh_serial_write(struct usbh_serial *serial, const void *buffer, uint32_t buflen);
    int usbh_serial_read(struct usbh_serial *serial, void *buffer, uint32_t buflen);
