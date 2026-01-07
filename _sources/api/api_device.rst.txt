设备协议栈
=========================

设备协议栈主要负责枚举和驱动加载，枚举这边就不说了，驱动加载，也就是接口驱动加载，主要是依靠 `usbd_add_interface` 函数，用于记录传入的接口驱动并保存到接口数组表，当主机进行类请求时就可以查找接口表进行访问了。
在调用 `usbd_desc_register` 以后需要进行接口注册和端点注册，口诀如下：

- 有多少个接口就调用多少次 `usbd_add_interface`，参数填相关 `xxx_init_intf`, 如果没有支持的，手动创建一个 intf 填入
- 有多少个端点就调用多少次 `usbd_add_endpoint`，当中断完成时，会调用到注册的端点回调中。

参考下面这张图：

.. figure:: img/api_device1.png

CORE
-----------------

端点结构体
""""""""""""""""""""""""""""""""""""

端点结构体主要用于注册不同端点地址的中断完成回调函数。

.. code-block:: C

    struct usbd_endpoint {
        uint8_t ep_addr;
        usbd_endpoint_callback ep_cb;
    };

- **ep_addr** 端点地址（带方向）
- **ep_cb** 端点完成中断回调函数。

.. note:: 总结一句话：in 回调函数等价于 dma 发送完成中断回调函数；out 回调函数等价于 dma 接收完成中断回调函数

接口结构体
""""""""""""""""""""""""""""""""""""

接口结构体主要用于注册不同类设备除了标准设备请求外的其他请求，包括类设备请求、厂商设备请求和自定义设备请求。以及协议栈中的相关通知回调函数。

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

- **class_interface_handler** class setup 请求回调函数,接收者为接口
- **class_endpoint_handler** class setup 请求回调函数,接收者为端点
- **vendor_handler** vendor setup 请求回调函数
- **notify_handler** 中断标志、协议栈相关状态回调函数
- **hid_report_descriptor** hid 报告描述符
- **hid_report_descriptor_len** hid 报告描述符长度
- **intf_num** 当前接口偏移

usbd_desc_register
""""""""""""""""""""""""""""""""""""

``usbd_desc_register`` 用来注册 USB 描述符，描述符种类包括：设备描述符、配置描述符（包含配置描述符、接口描述符、class 类描述符、端点描述符）、字符串描述符、设备限定描述符，其他速度描述符，
bos描述符，winusb 描述符。

.. code-block:: C

    // 开启 CONFIG_USBDEV_ADVANCE_DESC
    void usbd_desc_register(uint8_t busid, const struct usb_descriptor *desc);

    // 关闭 CONFIG_USBDEV_ADVANCE_DESC
    void usbd_desc_register(uint8_t busid, const uint8_t *desc);
    void usbd_msosv1_desc_register(uint8_t busid, struct usb_msosv1_descriptor *desc);
    void usbd_msosv2_desc_register(uint8_t busid, struct usb_msosv2_descriptor *desc);
    void usbd_bos_desc_register(uint8_t busid, struct usb_bos_descriptor *desc);
    void usbd_webusb_desc_register(uint8_t busid, struct usb_webusb_descriptor *desc);

- **desc**  描述符的句柄

.. note:: 当前默认开启 CONFIG_USBDEV_ADVANCE_DESC，如果需要使用旧版本 API 请关闭该宏

usbd_add_interface
""""""""""""""""""""""""""""""""""""

``usbd_add_interface`` 添加一个接口驱动。 **添加顺序必须按照描述符中接口顺序**。

.. code-block:: C

    void usbd_add_interface(uint8_t busid, struct usbd_interface *intf);

- **busid** USB 总线 id
- **intf**  接口驱动句柄，通常从不同 class 的 `xxx_init_intf` 函数获取

usbd_add_endpoint
""""""""""""""""""""""""""""""""""""

``usbd_add_endpoint`` 添加一个端点中断完成回调函数。

.. code-block:: C

    void usbd_add_endpoint(uint8_t busid, struct usbd_endpoint *ep);

- **busid** USB 总线 id
- **ep**    端点句柄

usbd_initialize
""""""""""""""""""""""""""""""""""""

``usbd_initialize`` 用来初始化 usb device 寄存器配置、usb 时钟、中断等，需要注意，此函数必须在注册描述符 API 最后。 **如果使用 os，必须放在线程中执行**。

.. code-block:: C

    int usbd_initialize(uint8_t busid, uintptr_t reg_base, usbd_event_handler_t event_handler);

- **busid** USB 总线 id
- **reg_base** USB 设备寄存器基地址
- **event_handler** 协议栈中断或者状态回调函数，event 事件
- **return** 返回 0 表示成功，其他值表示失败

event 事件包括：

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

.. note:: 大部分 IP USBD_EVENT_CONNECTED 和 USBD_EVENT_DISCONNECTED 事件都不支持，当前仅 HPM 芯片支持，其余芯片自行设计vbus检测电路替代

usbd_deinitialize
""""""""""""""""""""""""""""""""""""

``usbd_deinitialize`` 用来反初始化 usb device，关闭 usb 设备时钟、中断等。

.. code-block:: C

    int usbd_deinitialize(uint8_t busid);

- **busid** USB 总线 id
- **return** 返回 0 表示成功，其他值表示失败

CDC ACM
-----------------

usbd_cdc_acm_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_init_intf`` 用来初始化 USB CDC ACM 类接口，并实现该接口相关的函数。

- ``cdc_acm_class_interface_request_handler`` 用来处理 USB CDC ACM 类 Setup 请求。
- ``cdc_notify_handler`` 用来处理 USB CDC 其他中断回调函数。

.. code-block:: C

    struct usbd_interface *usbd_cdc_acm_init_intf(uint8_t busid, struct usbd_interface *intf);

- **busid** USB 总线 id
- **return**  接口句柄

usbd_cdc_acm_set_line_coding
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_line_coding`` 用来对串口进行配置，如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding);

- **busid** USB 总线 id
- **intf** 控制接口号
- **line_coding** 串口配置

usbd_cdc_acm_get_line_coding
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_get_line_coding`` 用来获取串口进行配置，如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_get_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *line_coding);

- **busid** USB 总线 id
- **intf** 控制接口号
- **line_coding** 串口配置

usbd_cdc_acm_set_dtr
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_dtr`` 用来控制串口 DTR 。如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr);

- **busid** USB 总线 id
- **intf** 控制接口号
- **dtr** dtr 为1表示拉低电平，为0表示拉高电平

usbd_cdc_acm_set_rts
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_rts`` 用来控制串口 RTS 。如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_set_rts(uint8_t busid, uint8_t intf, bool rts);

- **busid** USB 总线 id
- **intf** 控制接口号
- **rts** rts 为1表示拉低电平，为0表示拉高电平

CDC_ACM_DESCRIPTOR_INIT
""""""""""""""""""""""""""""""""""""

``CDC_ACM_DESCRIPTOR_INIT`` 配置了默认的 cdc acm 需要的描述符以及参数，方便用户使用。总长度为 `CDC_ACM_DESCRIPTOR_LEN` 。

.. code-block:: C

    CDC_ACM_DESCRIPTOR_INIT(bFirstInterface, int_ep, out_ep, in_ep, str_idx);

- **bFirstInterface** 表示该 cdc acm 第一个接口所在所有接口的偏移
- **int_ep** 表示中断端点地址（带方向）
- **out_ep** 表示 bulk out 端点地址（带方向）
- **in_ep** 表示 bulk in 端点地址（带方向）
- **str_idx** 控制接口对应的字符串 id

HID
-----------------

usbd_hid_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_hid_init_intf`` 用来初始化 USB HID 类接口，并实现该接口相关的函数：

- ``hid_class_interface_request_handler`` 用来处理 USB HID 类的 Setup 请求。
- ``hid_notify_handler`` 用来处理 USB HID 其他中断回调函数。

.. code-block:: C

    struct usbd_interface *usbd_hid_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t *desc, uint32_t desc_len);

- **busid** USB 总线 id
- **desc** 报告描述符
- **desc_len** 报告描述符长度

MSC
-----------------

usbd_msc_init_intf
""""""""""""""""""""""""""""""""""""
``usbd_msc_init_intf`` 用来初始化 MSC 类接口，并实现该接口相关函数，并且注册端点回调函数。（因为 msc bot 协议是固定的，所以不需要用于实现，因此端点回调函数自然不需要用户实现）。

- ``msc_storage_class_interface_request_handler`` 用于处理 USB MSC Setup 中断请求。
- ``msc_storage_notify_handler`` 用于实现 USB MSC 其他中断回调函数。

- ``mass_storage_bulk_out`` 用于处理 USB MSC 端点 out 中断。
- ``mass_storage_bulk_in`` 用于处理 USB MSC 端点 in 中断。

.. code-block:: C

    struct usbd_interface *usbd_msc_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t out_ep, const uint8_t in_ep);

- **busid** USB 总线 id
- **out_ep**     out 端点地址
- **in_ep**      in 端点地址

usbd_msc_get_cap
""""""""""""""""""""""""""""""""""""

``usbd_msc_get_cap`` 用来获取存储器的 lun、扇区个数和每个扇区大小。用户必须实现该函数。

.. code-block:: C

    void usbd_msc_get_cap(uint8_t busid, uint8_t lun, uint32_t *block_num, uint16_t *block_size);

- **busid** USB 总线 id
- **lun** 存储逻辑单元，暂时无用，默认支持一个
- **block_num**  存储扇区个数
- **block_size**  存储扇区大小

usbd_msc_sector_read
""""""""""""""""""""""""""""""""""""

``usbd_msc_sector_read`` 用来对存储器某个扇区开始的地址进行数据读取。用户必须实现该函数。

.. code-block:: C

    int usbd_msc_sector_read(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length);

- **busid** USB 总线 id
- **lun** 存储逻辑单元，暂时无用，默认支持一个
- **sector** 扇区偏移
- **buffer** 存储读取的数据的指针
- **length** 读取长度


usbd_msc_sector_write
""""""""""""""""""""""""""""""""""""

``usbd_msc_sector_write``  用来对存储器某个扇区开始写入数据。用户必须实现该函数。

.. code-block:: C

    int usbd_msc_sector_write(uint8_t busid, uint8_t lun, uint32_t sector, uint8_t *buffer, uint32_t length);

- **busid** USB 总线 id
- **lun** 存储逻辑单元，暂时无用，默认支持一个
- **sector** 扇区偏移
- **buffer** 写入数据指针
- **length** 写入长度

UAC
-----------------

usbd_audio_init_intf
""""""""""""""""""""""""""""""""""""
``usbd_audio_init_intf``  用来初始化 USB Audio 类接口，并实现该接口相关的函数：

- ``audio_class_interface_request_handler`` 用于处理 USB Audio Setup 接口接收者中断请求。
- ``audio_class_endpoint_request_handler`` 用于处理 USB Audio Setup 端点接收者中断请求。
- ``audio_notify_handler`` 用于实现 USB Audio 其他中断回调函数。

.. code-block:: C

    struct usbd_interface *usbd_audio_init_intf(uint8_t busid, struct usbd_interface *intf,
                                                uint16_t uac_version,
                                                struct audio_entity_info *table,
                                                uint8_t num);

- **busid** USB 总线 id
- **intf**  接口句柄
- **uac_version**  音频类版本，UAC1.0 或 UAC2.0
- **table** 音频实体信息表
- **num** 音频实体信息表长度

usbd_audio_open
""""""""""""""""""""""""""""""""""""

``usbd_audio_open``  用来开启音频数据传输。主机发送开启命令的回调函数。

.. code-block:: C

    void usbd_audio_open(uint8_t intf);

- **intf** 开启的接口号

usbd_audio_close
""""""""""""""""""""""""""""""""""""

``usbd_audio_close``  用来关闭音频数据传输。主机发送关闭命令的回调函数。

.. code-block:: C

    void usbd_audio_close(uint8_t intf);

- **intf** 关闭的接口号

usbd_audio_set_mute
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_mute``  用来设置静音。

.. code-block:: C

    void usbd_audio_set_mute(uint8_t busid, uint8_t ep, uint8_t ch, bool mute);

- **busid** USB 总线 id
- **ep** 要设置静音的端点
- **ch** 要设置静音的通道
- **mute** 为1 表示静音，0相反

usbd_audio_set_volume
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_volume``  用来设置音量。

.. code-block:: C

    void usbd_audio_set_volume(uint8_t busid, uint8_t ep, uint8_t ch, int volume_db);

- **busid** USB 总线 id
- **ep** 要设置音量的端点
- **ch** 要设置音量的通道
- **volume_db** 要设置音量的分贝，单位 -100dB ~ 0dB

usbd_audio_set_sampling_freq
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_sampling_freq``  用来设置设备上音频模块的采样率

.. code-block:: C

    void usbd_audio_set_sampling_freq(uint8_t busid, uint8_t ep, uint32_t sampling_freq);

- **ep** 要设置采样率的端点
- **sampling_freq** 要设置的采样率

usbd_audio_get_sampling_freq_table
""""""""""""""""""""""""""""""""""""

``usbd_audio_get_sampling_freq_table``  用来获取支持的采样率列表，如果函数没有实现，则使用默认采样率列表。 UAC2 only。

.. code-block:: C

    void usbd_audio_get_sampling_freq_table(uint8_t busid, uint8_t ep, uint8_t **sampling_freq_table);

- **ep** 要获取采样率的端点
- **sampling_freq_table** 采样率列表地址，格式参考默认采样率列表

UVC
-----------------

usbd_video_init_intf
""""""""""""""""""""""""""""""""""""
``usbd_video_init_intf``  用来初始化 USB Video 类接口，并实现该接口相关的函数：

- ``video_class_interface_request_handler`` 用于处理 USB Video Setup 中断请求。
- ``video_notify_handler`` 用于实现 USB Video 其他中断回调函数。

.. code-block:: C

    struct usbd_interface *usbd_video_init_intf(uint8_t busid, struct usbd_interface *intf,
                                                uint32_t dwFrameInterval,
                                                uint32_t dwMaxVideoFrameSize,
                                                uint32_t dwMaxPayloadTransferSize);
- **busid** USB 总线 id
- **intf**  接口句柄
- **dwFrameInterval** 视频帧间隔，单位 100ns
- **dwMaxVideoFrameSize** 最大视频帧大小
- **dwMaxPayloadTransferSize** 最大负载传输大小

usbd_video_open
""""""""""""""""""""""""""""""""""""

``usbd_video_open``  用来开启视频数据传输。

.. code-block:: C

    void usbd_video_open(uint8_t intf);

- **intf** 开启的接口号

usbd_video_close
""""""""""""""""""""""""""""""""""""

``usbd_video_close``  用来关闭视频数据传输。

.. code-block:: C

    void usbd_video_open(uint8_t intf);

- **intf** 关闭的接口号

usbd_video_stream_start_write
""""""""""""""""""""""""""""""""""""

``usbd_video_stream_start_write``  用来启动一帧视频数据流发送。需要搭配 `usbd_video_stream_split_transfer` 使用。

.. code-block:: C

    int usbd_video_stream_start_write(uint8_t busid, uint8_t ep, uint8_t *ep_buf, uint8_t *stream_buf, uint32_t stream_len, bool do_copy);

- **busid** USB 总线 id
- **ep** 视频数据端点地址
- **ep_buf** 视频数据端点传输缓冲区
- **stream_buf** 一帧视频数据源缓冲区
- **stream_len** 一帧视频数据源缓冲区大小
- **do_copy** 是否需要将 stream_buf 数据复制到 ep_buf 中，当前仅当 stream_buf 在 nocache 区域并且未开启 DCACHE_ENABLE 时该参数才为 false

usbd_video_stream_split_transfer
""""""""""""""""""""""""""""""""""""

``usbd_video_stream_split_transfer``  用来分割视频数据流发送。需要搭配 `usbd_video_stream_start_write` 使用。

.. code-block:: C

    int usbd_video_stream_split_transfer(uint8_t busid, uint8_t ep);

- **busid** USB 总线 id
- **ep** 视频数据端点地址
- **return** 返回 true 表示一帧数据发送完成，false 表示数据未发送完成

RNDIS
-----------------

CDC ECM
-----------------

MTP
-----------------
