Device Protocol Stack
=======================================

The device protocol stack is mainly responsible for enumeration and driver loading. We won't discuss enumeration here, but for driver loading (i.e., interface driver loading), it mainly relies on the `usbd_add_interface` function to record the passed-in interface driver and save it to the interface array table. When the host makes class requests, it can search the interface table for access.
After calling `usbd_desc_register`, interface registration and endpoint registration need to be performed according to the following rules:

- Call `usbd_add_interface` as many times as there are interfaces, with parameters filling in the relevant `xxx_init_intf`. If not supported, manually create an intf and fill it in
- Call `usbd_add_endpoint` as many times as there are endpoints. When interrupts complete, the registered endpoint callback will be called.

Refer to the diagram below:

.. figure:: img/api_device1.png

CORE
-----------------

Endpoint Structure
""""""""""""""""""""""""""""""""""""

The endpoint structure is mainly used to register interrupt completion callback functions for different endpoint addresses.

.. code-block:: C

    struct usbd_endpoint {
        uint8_t ep_addr;
        usbd_endpoint_callback ep_cb;
    };

- **ep_addr** Endpoint address (with direction)
- **ep_cb** Endpoint completion interrupt callback function.

.. note:: To summarize in one sentence: in callback function is equivalent to DMA transmission completion interrupt callback function; out callback function is equivalent to DMA reception completion interrupt callback function

Interface Structure
""""""""""""""""""""""""""""""""""""

The interface structure is mainly used to register requests other than standard device requests for different class devices, including class device requests, vendor device requests, and custom device requests, as well as related notification callback functions in the protocol stack.

.. code-block:: C

    struct usbd_interface {
        usbd_request_handler class_interface_handler;
        usbd_request_handler class_endpoint_handler;
        usbd_request_handler vendor_handler;
        usbd_notify_handler notify_handler;
        const uint8_t *hid_report_descriptor;
        uint32_t hid_report_descriptor_len;
        uint8_t intf_num;
    };

- **class_interface_handler** class setup request callback function, recipient is interface
- **class_endpoint_handler** class setup request callback function, recipient is endpoint
- **vendor_handler** vendor setup request callback function
- **notify_handler** interrupt flag, protocol stack related status callback function
- **hid_report_descriptor** hid report descriptor
- **hid_report_descriptor_len** hid report descriptor length
- **intf_num** current interface offset

usbd_desc_register
""""""""""""""""""""""""""""""""""""

``usbd_desc_register`` is used to register USB descriptors. Descriptor types include: device descriptor, configuration descriptor (including configuration descriptor, interface descriptor, class descriptor, endpoint descriptor), string descriptor, device qualifier descriptor, other speed descriptor, BOS descriptor, WinUSB descriptor.

.. code-block:: C

    // Enable CONFIG_USBDEV_ADVANCE_DESC
    void usbd_desc_register(uint8_t busid, const struct usb_descriptor *desc);

    // Disable CONFIG_USBDEV_ADVANCE_DESC
    void usbd_desc_register(uint8_t busid, const uint8_t *desc);
    void usbd_msosv1_desc_register(uint8_t busid, struct usb_msosv1_descriptor *desc);
    void usbd_msosv2_desc_register(uint8_t busid, struct usb_msosv2_descriptor *desc);
    void usbd_bos_desc_register(uint8_t busid, struct usb_bos_descriptor *desc);
    void usbd_webusb_desc_register(uint8_t busid, struct usb_webusb_descriptor *desc);

- **desc**  Descriptor handle

.. note:: Currently CONFIG_USBDEV_ADVANCE_DESC is enabled by default. If you need to use the old version API, please disable this macro. Starting from v1.6.0, only APIs with CONFIG_USBDEV_ADVANCE_DESC enabled are available

usbd_add_interface
""""""""""""""""""""""""""""""""""""

``usbd_add_interface`` adds an interface driver. **The addition order must follow the interface order in the descriptor**.

.. code-block:: C

    void usbd_add_interface(uint8_t busid, struct usbd_interface *intf);

- **busid** USB bus ID
- **intf**  Interface driver handle, usually obtained from different class `xxx_init_intf` functions

usbd_add_endpoint
""""""""""""""""""""""""""""""""""""

``usbd_add_endpoint`` adds an endpoint interrupt completion callback function.

.. code-block:: C

    void usbd_add_endpoint(uint8_t busid, struct usbd_endpoint *ep);

- **busid** USB bus ID
- **ep**    Endpoint handle

usbd_initialize
""""""""""""""""""""""""""""""""""""

``usbd_initialize`` is used to initialize USB device register configuration, USB clock, interrupts, etc. Note that this function must be called last after registering descriptor APIs. **If using an OS, it must be executed within a thread**.

.. code-block:: C

    int usbd_initialize(uint8_t busid, uintptr_t reg_base, usbd_event_handler_t event_handler);

- **busid** USB bus ID
- **reg_base** USB device register base address
- **event_handler** Protocol stack interrupt or status callback function, event events
- **return** Returns 0 for success, other values indicate failure

Event events include:

.. code-block:: C

    USBD_EVENT_ERROR,        /** USB error reported by the controller */
    USBD_EVENT_RESET,        /** USB reset */
    USBD_EVENT_SOF,          /** Start of Frame received */
    USBD_EVENT_CONNECTED,    /** USB connected*/
    USBD_EVENT_DISCONNECTED, /** USB disconnected */
    USBD_EVENT_SUSPEND,      /** USB connection suspended by the HOST */
    USBD_EVENT_RESUME,       /** USB connection resumed by the HOST */

    /* USB DEVICE STATUS */
    USBD_EVENT_CONFIGURED,        /** USB configuration done */
    USBD_EVENT_SET_INTERFACE,     /** USB interface selected */
    USBD_EVENT_SET_REMOTE_WAKEUP, /** USB set remote wakeup */
    USBD_EVENT_CLR_REMOTE_WAKEUP, /** USB clear remote wakeup */
    USBD_EVENT_INIT,              /** USB init done when call usbd_initialize */
    USBD_EVENT_DEINIT,            /** USB deinit done when call usbd_deinitialize */
    USBD_EVENT_UNKNOWN

.. note:: Most IPs do not support USBD_EVENT_CONNECTED and USBD_EVENT_DISCONNECTED events. Currently only HPM chips support them. For other chips, design your own VBUS detection circuit as an alternative

usbd_deinitialize
""""""""""""""""""""""""""""""""""""

``usbd_deinitialize`` is used to deinitialize USB device, turn off USB device clock, interrupts, etc.

.. code-block:: C

    int usbd_deinitialize(uint8_t busid);

- **busid** USB bus ID
- **return** Returns 0 for success, other values indicate failure

CDC ACM
-----------------

usbd_cdc_acm_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_init_intf`` is used to initialize USB CDC ACM class interface and implement related functions for this interface.

- ``cdc_acm_class_interface_request_handler`` is used to handle USB CDC ACM class Setup requests.
- ``cdc_notify_handler`` is used to handle other USB CDC interrupt callback functions.

.. code-block:: C

    struct usbd_interface *usbd_cdc_acm_init_intf(uint8_t busid, struct usbd_interface *intf);

- **busid** USB bus ID
- **return**  Interface handle

usbd_cdc_acm_set_line_coding
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_line_coding`` is used to configure the serial port. If only using USB without serial port, this interface does not need to be implemented by the user and can use the default.

.. code-block:: C

    void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding);

- **busid** USB bus ID
- **intf** Control interface number
- **line_coding** Serial port configuration

usbd_cdc_acm_get_line_coding
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_get_line_coding`` is used to get serial port configuration. If only using USB without serial port, this interface does not need to be implemented by the user and can use the default.

.. code-block:: C

    void usbd_cdc_acm_get_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding);

- **busid** USB bus ID
- **intf** Control interface number
- **line_coding** Serial port configuration

usbd_cdc_acm_set_dtr
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_dtr`` is used to control serial port DTR. If only using USB without serial port, this interface does not need to be implemented by the user and can use the default.

.. code-block:: C

    void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr);

- **busid** USB bus ID
- **intf** Control interface number
- **dtr** dtr = 1 means pull low level, 0 means pull high level

usbd_cdc_acm_set_rts
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_rts`` is used to control serial port RTS. If only using USB without serial port, this interface does not need to be implemented by the user and can use the default.

.. code-block:: C

    void usbd_cdc_acm_set_rts(uint8_t busid, uint8_t intf, bool rts);

- **busid** USB bus ID
- **intf** Control interface number
- **rts** rts = 1 means pull low level, 0 means pull high level

CDC_ACM_DESCRIPTOR_INIT
""""""""""""""""""""""""""""""""""""

``CDC_ACM_DESCRIPTOR_INIT`` configures the default CDC ACM required descriptors and parameters for user convenience. Total length is `CDC_ACM_DESCRIPTOR_LEN`.

.. code-block:: C

    CDC_ACM_DESCRIPTOR_INIT(bFirstInterface, int_ep, out_ep, in_ep, str_idx);

- **bFirstInterface** Indicates the offset of the first interface of this CDC ACM in all interfaces
- **int_ep** Indicates interrupt endpoint address (with direction)
- **out_ep** Indicates bulk out endpoint address (with direction)
- **in_ep** Indicates bulk in endpoint address (with direction)
- **str_idx** String ID corresponding to control interface

HID
-----------------

usbd_hid_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_hid_init_intf`` is used to initialize USB HID class interface and implement related functions for this interface:

- ``hid_class_interface_request_handler`` is used to handle USB HID class Setup requests.
- ``hid_notify_handler`` is used to handle other USB HID interrupt callback functions.

.. code-block:: C

    struct usbd_interface *usbd_hid_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t *desc, uint32_t desc_len);

- **busid** USB bus ID
- **desc** Report descriptor
- **desc_len** Report descriptor length

MSC
-----------------

usbd_msc_init_intf
""""""""""""""""""""""""""""""""""""
``usbd_msc_init_intf`` is used to initialize MSC class interface, implement related functions for this interface, and register endpoint callback functions. (Since MSC BOT protocol is fixed, user implementation is not needed, so endpoint callback functions naturally don't need user implementation).

- ``msc_storage_class_interface_request_handler`` is used to handle USB MSC Setup interrupt requests.
- ``msc_storage_notify_handler`` is used to implement other USB MSC interrupt callback functions.

- ``mass_storage_bulk_out`` is used to handle USB MSC endpoint out interrupts.
- ``mass_storage_bulk_in`` is used to handle USB MSC endpoint in interrupts.

.. code-block:: C

    struct usbd_interface *usbd_msc_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t out_ep, const uint8_t in_ep);

- **busid** USB bus ID
- **out_ep**     out endpoint address
- **in_ep**      in endpoint address

usbd_msc_get_cap
""""""""""""""""""""""""""""""""""""

``usbd_msc_get_cap`` is used to get the LUN, number of sectors, and sector size of the storage device. Users must implement this function.

.. code-block:: C

    void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint16_t *block_size);

- **busid** USB bus ID
- **lun** Storage logical unit, currently unused, defaults to supporting one
- **block_num**  Number of storage sectors
- **block_size**  Storage sector size

usbd_msc_sector_read
""""""""""""""""""""""""""""""""""""

``usbd_msc_sector_read`` is used to read data from a storage device starting at a specific sector address. Users must implement this function.

.. code-block:: C

    int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length);

- **busid** USB bus ID
- **lun** Storage logical unit, currently unused, defaults to supporting one
- **sector** Sector offset
- **buffer** Pointer to store read data
- **length** Read length


usbd_msc_sector_write
""""""""""""""""""""""""""""""""""""

``usbd_msc_sector_write`` is used to write data to a storage device starting at a specific sector. Users must implement this function.

.. code-block:: C

    int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length);

- **busid** USB bus ID
- **lun** Storage logical unit, currently unused, defaults to supporting one
- **sector** Sector offset
- **buffer** Write data pointer
- **length** Write length

UAC
-----------------

usbd_audio_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_audio_init_intf`` is used to initialize USB Audio class interface and implement related functions for this interface:

- ``audio_class_interface_request_handler`` is used to handle USB Audio Setup interface recipient interrupt requests.
- ``audio_class_endpoint_request_handler`` is used to handle USB Audio Setup endpoint recipient interrupt requests.
- ``audio_notify_handler`` is used to implement other USB Audio interrupt callback functions.

.. code-block:: C

    struct usbd_interface *usbd_audio_init_intf(uint8_t busid, struct usbd_interface *intf,
                                                uint16_t uac_version,
                                                struct audio_entity_info *table,
                                                uint8_t num);

- **busid** USB bus ID
- **intf**  Interface handle
- **uac_version**  Audio class version, UAC1.0 or UAC2.0
- **table** Audio entity information table
- **num** Audio entity information table length

usbd_audio_open
""""""""""""""""""""""""""""""""""""

``usbd_audio_open`` is used to start audio data transmission. Host sends start command callback function.

.. code-block:: C

    void usbd_audio_open(uint8_t intf);

- **intf** Interface number to open

usbd_audio_close
""""""""""""""""""""""""""""""""""""

``usbd_audio_close`` is used to stop audio data transmission. Host sends stop command callback function.

.. code-block:: C

    void usbd_audio_close(uint8_t intf);

- **intf** Interface number to close

usbd_audio_set_mute
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_mute`` is used to set mute.

.. code-block:: C

    void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute);

- **busid** USB bus ID
- **ep** Endpoint to set mute
- **ch** Channel to set mute
- **mute** 1 means mute, 0 means opposite

usbd_audio_set_volume
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_volume`` is used to set volume.

.. code-block:: C

    void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume_db);

- **busid** USB bus ID
- **ep** Endpoint to set volume
- **ch** Channel to set volume
- **volume_db** Volume to set in decibels, range -100dB ~ 0dB

usbd_audio_set_sampling_freq
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_sampling_freq`` is used to set the sampling rate of the audio module on the device

.. code-block:: C

    void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq);

- **ep** Endpoint to set sampling rate
- **sampling_freq** Sampling rate to set

usbd_audio_get_sampling_freq_table
""""""""""""""""""""""""""""""""""""

``usbd_audio_get_sampling_freq_table`` is used to get the list of supported sampling rates. If the function is not implemented, the default sampling rate list is used. UAC2 only.

.. code-block:: C

    void usbd_audio_get_sampling_freq_table(uint8_t busid, uint8_t ep, uint8_t **sampling_freq_table);

- **ep** Endpoint to get sampling rate
- **sampling_freq_table** Sampling rate list address, format refers to default sampling rate list

UVC
-----------------

usbd_video_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_video_init_intf`` is used to initialize USB Video class interface and implement related functions for this interface:

- ``video_class_interface_request_handler`` is used to handle USB Video Setup interrupt requests.
- ``video_notify_handler`` is used to implement other USB Video interrupt callback functions.

.. code-block:: C

    struct usbd_interface *usbd_video_init_intf(uint8_t busid, struct usbd_interface *intf,
                                                uint32_t dwFrameInterval,
                                                uint32_t dwMaxVideoFrameSize,
                                                uint32_t dwMaxPayloadTransferSize);
- **busid** USB bus ID
- **intf**  Interface handle
- **dwFrameInterval** Video frame interval, unit 100ns
- **dwMaxVideoFrameSize** Maximum video frame size
- **dwMaxPayloadTransferSize** Maximum payload transfer size

usbd_video_open
""""""""""""""""""""""""""""""""""""

``usbd_video_open`` is used to start video data transmission.

.. code-block:: C

    void usbd_video_open(uint8_t intf);

- **intf** Interface number to open

usbd_video_close
""""""""""""""""""""""""""""""""""""

``usbd_video_close`` is used to stop video data transmission.

.. code-block:: C

    void usbd_video_open(uint8_t intf);

- **intf** Interface number to close

usbd_video_stream_start_write
""""""""""""""""""""""""""""""""""""

``usbd_video_stream_start_write`` is used to start sending one frame of video data stream. Must be used together with `usbd_video_stream_split_transfer`.

.. code-block:: C

    int usbd_video_stream_start_write(uint8_t busid, uint8_t ep, uint8_t *ep_buf, uint8_t *stream_buf, uint32_t stream_len, bool do_copy);

- **busid** USB bus ID
- **ep** Video data endpoint address
- **ep_buf** Video data endpoint transfer buffer
- **stream_buf** One frame video data source buffer
- **stream_len** One frame video data source buffer size
- **do_copy** Whether to copy stream_buf data to ep_buf. This parameter is false only when stream_buf is in nocache area and DCACHE_ENABLE is not enabled

usbd_video_stream_split_transfer
""""""""""""""""""""""""""""""""""""

``usbd_video_stream_split_transfer`` is used to split video data stream transmission. Must be used together with `usbd_video_stream_start_write`.

.. code-block:: C

    int usbd_video_stream_split_transfer(uint8_t busid, uint8_t ep);

- **busid** USB bus ID
- **ep** Video data endpoint address
- **return** Returns true when one frame data transmission is complete, false when data transmission is not complete

RNDIS
-----------------

CDC ECM
-----------------

MTP
-----------------
