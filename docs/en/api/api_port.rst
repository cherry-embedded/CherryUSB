Host and Device Drivers
=======================================

.. note:: Some older interface examples omit busid or other parameters. Use the declarations in the current version's headers when calling these interfaces.

Place setup and transfer buffers in memory accessible to the controller's DMA, and meet its address and cache-line alignment requirements. Use non-cacheable memory or the existing cache maintenance interfaces according to the configuration.

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

``usbd_ep_start_write`` starts an asynchronous transfer on an IN endpoint. **This function is open to users**.

.. code-block:: C

    int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len);

- **busid** USB bus ID
- **ep** IN endpoint address, including the direction bit
- **data** Transmission buffer
- **data_len** Transmission length; use zero to send a ZLP
- **return** Returns 0 when the transfer starts successfully, or a negative value on failure

The registered IN completion callback runs when the transfer finishes; its ``nbytes`` argument is the actual length. Keep the transmission buffer valid and its contents unchanged until completion. The application serializes transmissions on each IN endpoint and uses the start result to determine whether to wait for a completion notification.

When the transmission length is a multiple of MPS, the class or protocol determines whether to append a ZLP. Follow the corresponding class/demo usage.

usbd_ep_start_read
""""""""""""""""""""""""""""""""""""

``usbd_ep_start_read`` prepares an asynchronous receive transfer on an OUT endpoint. **This function is open to users**.

.. code-block:: C

    int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len);

- **busid** USB bus ID
- **ep** OUT endpoint address
- **data** Reception buffer, which must remain valid until this receive transfer completes
- **data_len** Requested receive length, normally a multiple of the endpoint MPS; the buffer capacity must cover the receive range programmed by the current DCD
- **return** Returns 0 when the transfer starts successfully, or a negative value on failure

The registered OUT completion callback runs when reception reaches the requested length or encounters a short packet, including a ZLP. Its ``nbytes`` argument is the actual received length; use it when processing the data.

Start using ordinary endpoints after ``USBD_EVENT_CONFIGURED`` and prepare the first OUT receive transfer. For continuous reception, prepare another receive transfer after each completion. The application serializes reception on each OUT endpoint and coordinates buffer ownership between data processing and the next receive transfer.

.. note:: For bulk transfers, data_len is normally EP MPS. A protocol may use multiple EP MPS for fixed-length transfers, a custom protocol carrying length information (such as MSC), or host transfers terminated with a ZLP or short packet (such as RNDIS).

Endpoint completion callbacks run in the USB ISR. Use them for brief processing, starting the next transfer, or notifying a worker thread through the RTOS/OSAL. Perform lengthy processing in a thread.

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

``usbh_submit_urb`` submits a transfer to endpoint ``urb->ep`` on the device associated with ``urb->hport``. **This function is open to users**.

.. code-block:: C

    int usbh_submit_urb(struct usbh_urb *urb);

- **urb** USB request block containing the port, endpoint, setup, buffer, length, timeout and completion callback parameters
- **return** Returns 0 on success, or a negative value on error; success has different meanings for synchronous and asynchronous transfers, as described below

Before every call to ``usbh_submit_urb()``, call the corresponding fill function, such as ``usbh_control_urb_fill()``, ``usbh_bulk_urb_fill()`` or ``usbh_int_urb_fill()``, to populate the current request parameters. This also applies when resubmitting from a completion callback. Initialize the URB before its first use. Control transfers include a setup packet; interrupt transfers include an interval.

- **Synchronous transfer**: Set ``timeout > 0`` and ``complete = NULL``. The calling thread waits for completion or timeout. A return value of 0 indicates success; read the actual length from ``urb->actual_length``.
- **Asynchronous transfer**: Set ``timeout = 0`` and provide ``complete`` and ``arg``. A return value of 0 indicates successful submission. Completion is reported through ``complete(arg, nbytes)``: a non-negative value is the actual length, and a negative value is an error. Normal transfer completion callbacks run in the USB ISR and are suitable for brief processing, submitting another transfer, or notifying a worker thread.

Each in-flight request has exclusive use of its URB and buffer. Keep the associated setup, buffer and arg objects valid throughout the transfer and cleanup. After completion, call the corresponding fill function to populate the next transfer parameters before submitting it. The HCD manages ``hcpriv``.

Among them, the `urb` structure information is as follows:

.. code-block:: C

    struct usbh_urb {
        usb_slist_t list;
        void *hcpriv;
        struct usbh_hubport *hport;
        struct usb_endpoint_descriptor *ep;
        uint8_t data_toggle;
        uint32_t interval;
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

.. note:: Set the synchronous wait time according to application requirements. On synchronous timeout, the submit path handles cancellation and cleanup; the caller handles the return result. Asynchronous transfers report their results through the complete callback.

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

usbh_kill_urb
""""""""""""""""""""""""""""""""""""

``usbh_kill_urb`` cancels the transfer associated with the specified URB. **This function is open to users**.

.. code-block:: C

    int usbh_kill_urb(struct usbh_urb *urb);

- **urb** The same USB request block that was previously submitted
- **return** Returns 0 when the cancellation call succeeds, or a negative value on error

Usage: during device disconnection, the enumeration thread cancels the class's in-flight URBs. On synchronous timeout, the submit path handles cancellation and cleanup; the caller handles the submit result.

Cancelling a synchronous request wakes its waiting thread. Cancelling an asynchronous request invokes its error callback. The error callback runs in the kill caller's context and handles the error to end the current transfer; normal success callbacks may submit further transfers as required by the application.

During disconnection, stop application submissions, cancel in-flight requests, terminate the associated worker threads, then release application objects. The caller manages the application memory for URBs and data buffers.
