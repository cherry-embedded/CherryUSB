USB Video Device
=================

This section mainly demonstrates USB UVC functionality, supporting YUYV, MJPEG, H264 formats. For demonstration convenience, static images are used throughout.

The demo includes **video_static_yuyv_template**, **video_static_mjpeg_template**, **video_static_h264_template**, with only descriptors and image data being different.

- In high-speed mode, the default maximum is 1024 bytes, but if the chip supports additional transactions, it can be configured up to 2048 bytes or 3072 bytes, which can improve transmission efficiency.

.. code-block:: C

    #ifdef CONFIG_USB_HS
    #define MAX_PAYLOAD_SIZE  1024 // for high speed with one transcations every one micro frame
    #define VIDEO_PACKET_SIZE (unsigned int)(((MAX_PAYLOAD_SIZE / 1)) | (0x00 << 11))

    // #define MAX_PAYLOAD_SIZE  2048 // for high speed with two transcations every one micro frame
    // #define VIDEO_PACKET_SIZE (unsigned int)(((MAX_PAYLOAD_SIZE / 2)) | (0x01 << 11))

    // #define MAX_PAYLOAD_SIZE  3072 // for high speed with three transcations every one micro frame
    // #define VIDEO_PACKET_SIZE (unsigned int)(((MAX_PAYLOAD_SIZE / 3)) | (0x02 << 11))

    #else
    #define MAX_PAYLOAD_SIZE  1020
    #define VIDEO_PACKET_SIZE (unsigned int)(((MAX_PAYLOAD_SIZE / 1)) | (0x00 << 11))
    #endif

- Usually only need to modify WIDTH and HEIGHT

.. code-block:: C

    #define WIDTH  (unsigned int)(640)
    #define HEIGHT (unsigned int)(480)

    #define CAM_FPS        (30)
    #define INTERVAL       (unsigned long)(10000000 / CAM_FPS)
    #define MIN_BIT_RATE   (unsigned long)(WIDTH * HEIGHT * 16 * CAM_FPS) //16 bit
    #define MAX_BIT_RATE   (unsigned long)(WIDTH * HEIGHT * 16 * CAM_FPS)
    #define MAX_FRAME_SIZE (unsigned long)(WIDTH * HEIGHT * 2)

- USB endpoint configuration, default interval is 1, which is 1ms in full-speed mode and 125us in high-speed mode. Synchronization type uses asynchronous mode.

.. code-block:: C

    /* 1.2.2.2 Standard VideoStream Isochronous Video Data Endpoint Descriptor */
    USB_ENDPOINT_DESCRIPTOR_INIT(VIDEO_IN_EP, 0x05, VIDEO_PACKET_SIZE, 0x01),


- Use `usbd_video_stream_start_write` to transfer data. The final **do_copy** option indicates whether to copy data to packet_buffer.
If copy is not selected, header information will be directly filled in the original image data and sent directly, achieving zero copy functionality.

- Because static data is provided and cannot be modified, a new frame_buffer needs to be allocated for image transmission. In actual camera integration scenarios, dynamic data is used and the camera's data buffer can be used directly.


.. code-block:: C

    void usbd_video_iso_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
    {
        if (usbd_video_stream_split_transfer(busid, ep)) {
            /* one frame has done */
            iso_tx_busy = false;
        }
    }

    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t packet_buffer[MAX_PAYLOAD_SIZE];
    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t frame_buffer[32 * 1024];

    void video_test(uint8_t busid)
    {
        memset(packet_buffer, 0, sizeof(packet_buffer));

        while (1) {
            if (tx_flag) {
                iso_tx_busy = true;
                memcpy(frame_buffer, cherryusb_mjpeg, sizeof(cherryusb_mjpeg)); // cherryusb_mjpeg is a static MJPEG frame buffer, so we need copy it to frame_buffer
                usbd_video_stream_start_write(busid, VIDEO_IN_EP, packet_buffer, (uint8_t *)frame_buffer, sizeof(cherryusb_mjpeg), false);
                while (iso_tx_busy) {
                    if (tx_flag == 0) {
                        break;
                    }
                }
            }
        }
    }