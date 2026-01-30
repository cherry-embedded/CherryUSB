Host and Device Drivers
=======================================

.. note:: Please note that starting from version v1.1, the busid parameter has been added, while everything else remains unchanged, so API documentation is not updated

device controller(dcd)
-------------------------

usb_dc_init
""""""""""""""""""""""""""""""""""""

``usb_dc_init`` is used to initialize USB device controller registers, set USB pins, clock, interrupts, etc. **This function is not open to users**.

.. code-block:: C

    int usb_dc_init(void);

- **return** Returns 0 for success, other values indicate error

usb_dc_deinit
""""""""""""""""""""""""""""""""""""

``usb_dc_deinit`` is used to de-initialize USB device controller registers. **This function is not open to users**.

.. code-block:: C

    int usb_dc_deinit(void);

- **return** Returns 0 for success, other values indicate error

usbd_set_address
""""""""""""""""""""""""""""""""""""

``usbd_set_address`` sets the device address. **This function is not open to users**.

.. code-block:: C

    int usbd_set_address(const uint8_t addr);

- **addr** Device address
- **return** Returns 0 for success, other values indicate error

usbd_ep_open
""""""""""""""""""""""""""""""""""""

``usbd_ep_open`` sets endpoint properties and enables corresponding endpoint interrupts. **This function is not open to users**.

.. code-block:: C

    int usbd_ep_open(const struct usb_endpoint_descriptor *ep);

- **ep** Endpoint descriptor
- **return** Returns 0 for success, other values indicate error

usbd_ep_close
""""""""""""""""""""""""""""""""""""

``usbd_ep_close`` closes an endpoint. **This function is not open to users**.

.. code-block:: C

    int usbd_ep_close(const uint8_t ep);

- **ep** Endpoint address
- **return** Returns 0 for success, other values indicate error

usbd_ep_set_stall
""""""""""""""""""""""""""""""""""""

``usbd_ep_set_stall`` sets an endpoint to stall state and sends a stall handshake packet. **This function is open to users**.

.. code-block:: C

    int usbd_ep_set_stall(const uint8_t ep);

- **ep** Endpoint address
- **return** Returns 0 for success, other values indicate error

usbd_ep_clear_stall
""""""""""""""""""""""""""""""""""""

``usbd_ep_clear_stall`` clears the stall state of an endpoint. **This function is not open to users**.

.. code-block:: C

    int usbd_ep_clear_stall(const uint8_t ep);

- **ep** Endpoint address
- **return** Returns 0 for success, other values indicate error

usbd_ep_is_stalled
""""""""""""""""""""""""""""""""""""

``usbd_ep_is_stalled`` reads the current stall state of an endpoint. **This function is not open to users**.

.. code-block:: C

    int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled);

- **ep** Endpoint address
- **return** Returns 1 for stalled, 0 for not stalled

usbd_ep_start_write
""""""""""""""""""""""""""""""""""""

``usbd_ep_start_write`` starts endpoint transmission. After transmission completion, it will call the registered IN endpoint transfer completion interrupt callback function. This function performs asynchronous transmission. **This function is open to users**.

.. code-block:: C

    int usbd_ep_start_write(const uint8_t ep, const uint8_t *data, uint32_t data_len);

- **ep** IN endpoint address
- **data** Transmission data buffer
- **data_len** Transmission length, theoretically unlimited, recommended within 16K bytes
- **return** Returns 0 for success, other values indicate error

usbd_ep_start_read
""""""""""""""""""""""""""""""""""""

``usbd_ep_start_read`` starts endpoint reception. After reception completion, it will call the registered OUT endpoint transfer completion interrupt callback function. This function performs asynchronous reception. **This function is open to users**.

.. code-block:: C

    int usbd_ep_start_read(const uint8_t ep, uint8_t *data, uint32_t data_len);

- **ep** OUT endpoint address
- **data** Reception data buffer
- **data_len** Reception length, theoretically unlimited, recommended within 16K bytes, and preferably a multiple of maximum packet size
- **return** Returns 0 for success, other values indicate error

.. note:: After starting reception, transfer completion interrupt will be triggered under two conditions: 1. Last packet is a short packet (less than EP MPS); 2. Total received length equals data_len

.. note:: For bulk transfers, data_len is usually designed as EP MPS. The following three cases can be modified to multiple EP MPS: fixed length; custom protocol with length information (MSC); host manually sends ZLP or short packet (RNDIS)

host controller(hcd)
------------------------

usb_hc_init
""""""""""""""""""""""""""""""""""""

``usb_hc_init`` is used to initialize USB host controller registers, set USB pins, clock, interrupts, etc. **This function is not open to users**.

.. code-block:: C

    int usb_hc_init(void);

- **return** Returns 0 for success, other values indicate error

usb_hc_deinit
""""""""""""""""""""""""""""""""""""

``usb_hc_deinit`` is used to de-initialize USB host controller registers. **This function is not open to users**.

.. code-block:: C

    int usb_hc_deinit(void);

- **return** Returns 0 for success, other values indicate error

usbh_roothub_control
""""""""""""""""""""""""""""""""""""

``usbh_roothub_control`` is used to send requests to the root hub. **This function is not open to users**.

.. code-block:: C

    int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf);

- **setup** Request
- **buf** Reception buffer
- **return** Returns 0 for success, other values indicate error

usbh_submit_urb
""""""""""""""""""""""""""""""""""""

``usbh_submit_urb`` performs data requests to endpoints at a specific address. **This function is open to users**.

.. code-block:: C

    int usbh_submit_urb(struct usbh_urb *urb);

- **urb** USB request block
- **return** Returns 0 for success, other values indicate error

Among them, the `urb` structure information is as follows:

.. code-block:: C

    struct usbh_urb {
        usb_slist_t list;
        void *hcpriv;
        struct usbh_hubport *hport;
        struct usb_endpoint_descriptor *ep;
        uint8_t data_toggle;
        uint8_t interval;
        struct usb_setup_packet *setup;
        uint8_t *transfer_buffer;
        uint32_t transfer_buffer_length;
        int transfer_flags;
        uint32_t actual_length;
        uint32_t timeout;
        int errorcode;
        uint32_t num_of_iso_packets;
        uint32_t start_frame;
        usbh_complete_callback_t complete;
        void *arg;
    #if defined(__ICCARM__) || defined(__ICCRISCV__) || defined(__ICCRX__)
        struct usbh_iso_frame_packet *iso_packet;
    #else
        struct usbh_iso_frame_packet iso_packet[0];
    #endif
    };

- **hcpriv** Host controller driver private member
- **hport** The hport used by current URB
- **ep** The endpoint used by current URB
- **data_toggle** Current data toggle
- **interval** URB transfer interval in microseconds. If interval is greater than 1000us, software timer needs to be used for maintenance
- **setup** Setup request buffer, used by endpoint 0
- **transfer_buffer** Transfer data buffer
- **transfer_buffer_length** Transfer length
- **transfer_flags** Flags carried during transfer
- **actual_length** Actual transfer length
- **timeout** Transfer timeout. If 0, the function is non-blocking and can be used in interrupts
- **errorcode** Error code
- **num_of_iso_packets** Number of ISO frames or microframes
- **complete** Transfer completion callback function
- **arg** Parameters carried when transfer completes
- **iso_packet** ISO data packet

.. note:: If there are no special time requirements for timeout, it must be set to 0xffffffff. In principle, timeout is not allowed. If timeout occurs, generally cannot continue working

`errorcode` can return the following values:

.. code-block:: C

  #define USB_ERR_NOMEM    1
  #define USB_ERR_INVAL    2
  #define USB_ERR_NODEV    3
  #define USB_ERR_NOTCONN  4
  #define USB_ERR_NOTSUPP  5
  #define USB_ERR_BUSY     6
  #define USB_ERR_RANGE    7
  #define USB_ERR_STALL    8
  #define USB_ERR_BABBLE   9
  #define USB_ERR_NAK      10
  #define USB_ERR_DT       11
  #define USB_ERR_IO       12
  #define USB_ERR_SHUTDOWN 13
  #define USB_ERR_TIMEOUT  14

Among them, the `iso_packet` structure information is as follows:

.. code-block:: C

  struct usbh_iso_frame_packet {
      uint8_t *transfer_buffer;
      uint32_t transfer_buffer_length;
      uint32_t actual_length;
      int errorcode;
  };

- **transfer_buffer** Transfer data buffer
- **transfer_buffer_length** Transfer length
- **actual_length** Actual transfer length
- **errorcode** Error code