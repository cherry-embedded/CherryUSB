CDC ACM Device
=================

This demo mainly demonstrates CDC ACM functionality. Reference the `demo/cdc_acm_template.c` template. Includes transmission/reception testing, DTR control, ZLP testing, and performance testing.

- Allocate read/write buffers for data transmission/reception. Buffers need to be modified with nocache. Here we use 2048 bytes for both read and write for subsequent ZLP testing and performance testing.

.. code-block:: C

    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_buffer[2048]; /* 2048 is only for test speed , please use CDC_MAX_MPS for common*/
    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t write_buffer[2048];


- In the protocol stack event callback, we need to start the first transmission after enumeration is complete and clear the related flags. This can be done in the reset event or in the configured event.

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

- Continue to initiate reception in the reception complete interrupt; determine whether to send ZLP in the transmission complete interrupt.

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

- The following is for testing DTR functionality and controlling USB transmission. DTR and RTS are only used in conjunction with UART; for pure USB, they are not very useful - this is just for testing. DTR switch uses any serial port host computer and check DTR.

.. code-block:: C

    void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
    {
        if (dtr) {
            dtr_enable = 1;
        } else {
            dtr_enable = 0;
        }
    }

- Keep calling send in the main function

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

- Note that we set the length to 2048 for testing ZLP functionality. In actual use, the receive length should use CDC_MAX_MPS. See :ref:`usb_ext` for specific reasons.
- For performance testing, use tools/test_srcipts/test_cdc_speed.py and remove the print statements in `usbd_cdc_acm_bulk_out` and `usbd_cdc_acm_bulk_in` before testing, otherwise it will affect the test results.


In addition, for CDC ACM with OS, we usually use asynchronous read and store data in a ringbuffer, and use synchronous write with semaphore.