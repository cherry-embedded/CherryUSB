Video Host
=================

.. note:: Host UVC 框架和 ISO 驱动为商用收费，请联系官方购买授权。

使用流程
------------

- 注册 frame 内存池，通常设计为双buffer。

.. code-block:: C

    static uint8_t frame_buffer1[IMAGE_WIDTH * IMAGE_HEIGHT * 2];
    static uint8_t frame_buffer2[IMAGE_WIDTH * IMAGE_HEIGHT * 2];
    static struct usbh_videoframe frame_pool[2];

    frame_pool[0].frame_buf = frame_buffer1;
    frame_pool[0].frame_bufsize = IMAGE_WIDTH * IMAGE_HEIGHT * 2;
    frame_pool[1].frame_buf = frame_buffer2;
    frame_pool[1].frame_bufsize = IMAGE_WIDTH * IMAGE_HEIGHT * 2;

    usbh_video_stream_create(frame_pool, 2);

- 创建 frame 接收线程并处理。frame->frame_format 指示当前帧的格式，frame->frame_buf 指向帧数据，frame->frame_size 指示帧数据大小。处理完成后需要调用 usbh_video_stream_enqueue 将 frame 重新入队。

.. code-block:: C

    static void usbh_video_frame_thread(void *argument)
    {
        int ret;
        struct usbh_videoframe *frame;

        while (1) {
            ret = usbh_video_stream_dequeue(&frame, 0xfffffff);
            if (ret < 0) {
                continue;
            }

            USB_LOG_RAW("frame buf:%p,frame len:%d\r\n", frame->frame_buf, frame->frame_size);

            usbh_video_stream_enqueue(frame);
        }
    }

    usb_osal_thread_create("uvc_frame", 3072, 5, usbh_video_frame_thread, NULL);

- `usbh_video_fps_init` 函数辅助打印帧率
- 调用 `usbh_video_stream_start` 启动视频流，调用 `usbh_video_stream_stop` 停止视频流。

宏的说明
-------------

.. code-block:: C

    #define VIDEO_ISO_PACKETS  (8 * 2)
    #define VIDEO_EP_MAX_MPS   3072

- VIDEO_ISO_PACKETS：每次传输的 ISO 个数。 **要求是 8 的倍数，对应高速设备（1ms 8个包），全速设备（8ms 8个包）**。 这里我们限定摄像头 bInterval 是 1，并且目前没有不是 1 的摄像头。
- VIDEO_EP_MAX_MPS：视频流端点的最大包大小，单位为字节。 **如果 USB IP 能够支持到 3072字节，则支持市面上所有的摄像头，如果不支持，则只能支持部分摄像头或者需要定制摄像头**。对 BULK 摄像头没有要求。

.. note:: 增大 VIDEO_ISO_PACKETS 可以降低传输中断频率，但是会增加 RAM 的开销，需要目标 IP 支持 scatter-gather DMA 才有效果，对于 buffer dma 或者 fifo 模式的 IP 不会降低中断频率。