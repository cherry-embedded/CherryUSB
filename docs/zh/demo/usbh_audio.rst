Audio Host
=================

.. note:: Host UAC 框架和 ISO 驱动为商用收费，请联系官方购买授权。


MIC 使用流程
--------------

- 注册 frame 内存池, AUDIO_MIC_EP_MAX_MPS 可以改成实际能支持的最大 iso ep_in_mps 以减少 RAM 消耗。 **一个 frame 收到的数据大小范围为 0 ~ x * AUDIO_MIC_ISO_PACKETS**。x 为实际 iso ep_in_mps。

.. code-block:: C

    #define AUDIP_MIC_POOL_SIZE (10)
    static USB_MEM_ALIGNX uint8_t frame_mic_buffer[AUDIO_MIC_EP_MAX_MPS * AUDIO_MIC_ISO_PACKETS * AUDIP_MIC_POOL_SIZE];
    static struct usbh_audioframe frame_mic_pool[AUDIP_MIC_POOL_SIZE];

    for (uint8_t i = 0; i < AUDIP_MIC_POOL_SIZE; i++) {
        frame_mic_pool[i].frame_buf = frame_mic_buffer + i * AUDIO_MIC_EP_MAX_MPS * AUDIO_MIC_ISO_PACKETS;
        frame_mic_pool[i].frame_bufsize = AUDIO_MIC_EP_MAX_MPS * AUDIO_MIC_ISO_PACKETS;
    }

    usbh_audio_mic_stream_create(frame_mic_pool, AUDIP_MIC_POOL_SIZE);

- 开启 MIC 数据接收线程并处理，处理完成后需要将 frame 重新入队。

.. code-block:: C

    static void usbh_audio_mic_frame_thread(void *argument)
    {
        int ret;
        struct usbh_audioframe *frame;

        while (1) {
            ret = usbh_audio_mic_stream_dequeue(&frame, 0xfffffff);
            if (ret < 0) {
                continue;
            }

            USB_LOG_RAW("frame buf:%p,frame len:%d\r\n", frame->frame_buf, frame->frame_size);

            usbh_audio_mic_stream_enqueue(frame);
        }
    }

- 调用 `usbh_audio_mic_stream_start` 启动 MIC 流，调用 `usbh_audio_mic_stream_stop` 停止 MIC 流。

SPEAKER 使用流程
------------------

- 注册 frame 内存池, AUDIO_SPEAKER_EP_MAX_MPS 可以改成实际能支持的最大 iso ep_out_mps 以减少 RAM 消耗。一个 frame 必须按照 ep_out_mps * AUDIO_SPEAKER_ISO_PACKETS 的大小来填充数据。

.. code-block:: C

    #define AUDIO_SPEAKER_POOL_SIZE (10)
    static USB_MEM_ALIGNX uint8_t frame_speaker_buffer[AUDIO_SPEAKER_EP_MAX_MPS * AUDIO_SPEAKER_ISO_PACKETS * AUDIO_SPEAKER_POOL_SIZE];
    static struct usbh_audioframe frame_speaker_pool[AUDIO_SPEAKER_POOL_SIZE];

    for (uint8_t i = 0; i < AUDIO_SPEAKER_POOL_SIZE; i++) {
        frame_speaker_pool[i].frame_buf = frame_speaker_buffer + i * AUDIO_SPEAKER_EP_MAX_MPS * AUDIO_SPEAKER_ISO_PACKETS;
        frame_speaker_pool[i].frame_bufsize = AUDIO_SPEAKER_EP_MAX_MPS * AUDIO_SPEAKER_ISO_PACKETS;
    }

    usbh_audio_speaker_stream_create(frame_speaker_pool, AUDIO_SPEAKER_POOL_SIZE);

- 调用 `usbh_audio_speaker_stream_start` 启动 SPEAKER 流，调用 `usbh_audio_speaker_stream_stop` 停止 SPEAKER 流。
- 调用 `usbh_uac_speaker_frame_alloc` 申请一个 frame，填充数据后调用 `usbh_uac_speaker_frame_send` 将数据入队。
- 当前 speaker frame 的处理是在 `usbh_audio_speaker_stream_start` 启动以后的周期性中断中循环接收和发送，用户也可以改成使用线程来进行接收和发送。

宏的说明
-------------

.. code-block:: C

    #define AUDIO_MIC_ISO_PACKETS  (1)
    #define AUDIO_MIC_EP_MAX_MPS   1024

- AUDIO_MIC_ISO_PACKETS：每次传输的 ISO 个数，单位是 bInterval 计算出来的帧或者微针时间。例如 全速 bInterval 为1或者高速 bInterval 为4时表示 1ms传输一个包。
- AUDIO_MIC_EP_MAX_MPS：音频输入端点最大包大小，单位为字节。

.. code-block:: C

    #define AUDIO_SPEAKER_ISO_PACKETS  (1)
    #define AUDIO_SPEAKER_EP_MAX_MPS   1024

- AUDIO_SPEAKER_ISO_PACKETS：每次传输的 ISO 个数，单位是 bInterval 计算出来的帧或者微针时间。例如 全速 bInterval 为1或者高速 bInterval 为4时表示 1ms传输一个包。
- AUDIO_SPEAKER_EP_MAX_MPS：音频输出端点最大包大小，单位为字节。

.. note:: 增大 AUDIO_MIC_ISO_PACKETS or AUDIO_SPEAKER_ISO_PACKETS 可以降低传输中断频率，但是会增加 RAM 的开销，需要目标 IP 支持 scatter-gather DMA 才有效果，对于 buffer dma 或者 fifo 模式的 IP 不会降低中断频率。