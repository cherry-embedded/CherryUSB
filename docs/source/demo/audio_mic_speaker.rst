USB 双通道麦克风和扬声器
============================

软件实现
------------

详细代码参考 `demo/audio_v1_mic_speaker_multichan_template.c`

.. code-block:: C

    usbd_desc_register(audio_descriptor);
    usbd_add_interface(usbd_audio_alloc_intf());
    usbd_add_interface(usbd_audio_alloc_intf());
    usbd_add_interface(usbd_audio_alloc_intf());
    usbd_add_endpoint(&audio_in_ep);
    usbd_add_endpoint(&audio_out_ep);

    usbd_audio_add_entity(0x02, AUDIO_CONTROL_FEATURE_UNIT);
    usbd_audio_add_entity(0x05, AUDIO_CONTROL_FEATURE_UNIT);

    usbd_initialize();

- 调用 `audio_init` 配置 audio 描述符并初始化 usb 硬件
- 因为 麦克风+扬声器+控制需要 3 个接口，所以我们需要调用 `usbd_add_interface` 3 次
- 默认描述符中开启了 mute 和 volume 的控制，所以需要注册对应的 entity，使用 `usbd_audio_add_entity`

.. code-block:: C

    void usbd_audio_open(uint8_t intf)
    {
    }
    void usbd_audio_close(uint8_t intf)
    {
    }

- 当我们打开 PC 的音量图标，或者音乐播放器、麦克风界面时，会调用到这两个接口，用于启动或者停止数据传输

.. code-block:: C

    usbd_ep_start_write(AUDIO_IN_EP, write_buffer, 2048);

- 由于 audio 协议中没有应用层相关的协议，传输的只有音频的原始数据，所以直接调用 `usbd_ep_start_write` 即可，发送完成会进入完成中断
- 由于扬声器需要使用 out 端点，所以需要在 `usbd_configure_done_callback` 中启动第一次接收，当然如果没有能力接收，可以不启动，在想启动的时候启动