usbd_video
===============

本节主要演示 USB UAC 功能，支持 YUYV, MJPEG, H264 格式。为了方便演示，都采用的静态图。

demo 包含 **video_static_yuyv_template**, **video_static_mjpeg_template**, **video_static_h264_template**, 仅描述符和图片数据不同。

- 在高速模式下，默认最大是1024字节，但是如果芯片支持 additional transcations，可以配置为最高 2048字节或者3072字节，这样可以提高传输效率。

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

- 通常只需要修改 WIDTH 和 HEIGHT

.. code-block:: C

    #define WIDTH  (unsigned int)(640)
    #define HEIGHT (unsigned int)(480)

    #define CAM_FPS        (30)
    #define INTERVAL       (unsigned long)(10000000 / CAM_FPS)
    #define MIN_BIT_RATE   (unsigned long)(WIDTH * HEIGHT * 16 * CAM_FPS) //16 bit
    #define MAX_BIT_RATE   (unsigned long)(WIDTH * HEIGHT * 16 * CAM_FPS)
    #define MAX_FRAME_SIZE (unsigned long)(WIDTH * HEIGHT * 2)

- USB 端点配置，默认 interval 为 1，也就是全速模式下 1ms，高速模式下 125us。同步类型使用异步模式。

.. code-block:: C

    /* 1.2.2.2 Standard VideoStream Isochronous Video Data Endpoint Descriptor */
    USB_ENDPOINT_DESCRIPTOR_INIT(VIDEO_IN_EP, 0x05, VIDEO_PACKET_SIZE, 0x01),


- 使用 `usbd_video_stream_start_write` 传输数据

1，传输采用双缓冲的形式， **MAX_PACKETS_IN_ONE_TRANSFER** 表示一次传输可以携带多少个 **MAX_PAYLOAD_SIZE**，通常 IP 只能为 1。

2，在中断完成中，调用 `usbd_video_stream_split_transfer` 继续下一次传输，直到返回为 true 表示传输完成。这边的分裂传输只是表示将图片数据拆成 **MAX_PACKETS_IN_ONE_TRANSFER * MAX_PAYLOAD_SIZE** 份传输。

3，通常 IP 不支持一次传输非常大的数据，比如传输 1MB，因此需要做分裂传输，但是会增加中断次数。并且一次传输非常大数据也是需要足够的 RAM。

.. code-block:: C

    void usbd_video_iso_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
    {
        if (usbd_video_stream_split_transfer(busid, ep)) {
            /* one frame has done */
            iso_tx_busy = false;
        }
    }

    USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t packet_buffer[2][MAX_PACKETS_IN_ONE_TRANSFER * MAX_PAYLOAD_SIZE];

    void video_test(uint8_t busid)
    {
        memset(packet_buffer, 0, sizeof(packet_buffer));

        while (1) {
            if (tx_flag) {
                iso_tx_busy = true;
                usbd_video_stream_start_write(busid, VIDEO_IN_EP, &packet_buffer[0][0], &packet_buffer[1][0], MAX_PACKETS_IN_ONE_TRANSFER * MAX_PAYLOAD_SIZE, (uint8_t *)cherryusb_mjpeg, sizeof(cherryusb_mjpeg));
                while (iso_tx_busy) {
                    if (tx_flag == 0) {
                        break;
                    }
                }
            }
        }
    }