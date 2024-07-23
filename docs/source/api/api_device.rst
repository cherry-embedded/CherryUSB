设备协议栈
=========================

设备协议栈主要负责枚举和驱动加载，枚举这边就不说了，驱动加载，也就是接口驱动加载，主要是依靠 `usbd_add_interface` 函数，用于记录传入的接口驱动并保存到接口数组表，当主机进行类请求时就可以查找接口表进行访问了。
在调用 `usbd_desc_register` 以后需要进行接口注册和端点注册，口诀如下：

- 有多少个接口就调用多少次 `usbd_add_interface`，参数填相关 `xxx_init_intf`, 如果没有支持的，手动创建一个 intf 填入
- 有多少个端点就调用多少次 `usbd_add_endpoint`，当中断完成时，会调用到注册的端点回调中。

CORE
-----------------

.. note:: 请注意，v1.1 版本开始增加 busid 形参，其余保持不变，所以 API 说明不做更新

端点结构体
""""""""""""""""""""""""""""""""""""

端点结构体主要用于注册不同端点地址的中断完成回调函数。

.. code-block:: C

    struct usbd_endpoint {
        uint8_t ep_addr;
        usbd_endpoint_callback ep_cb;
    };

- **list** 端点的链表节点
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
- **ep_list** 端点的链表节点

usbd_desc_register
""""""""""""""""""""""""""""""""""""

``usbd_desc_register`` 用来注册 USB 描述符，描述符种类包括：设备描述符、配置描述符（包含配置描述符、接口描述符、class 类描述符、端点描述符）、字符串描述符、设备限定描述符。

.. code-block:: C

    void usbd_desc_register(const uint8_t *desc);

- **desc**  描述符的句柄

.. note:: 当前 API 仅支持一种速度，如果需要更高级的速度切换功能，请开启 CONFIG_USBDEV_ADVANCE_DESC，并且包含了下面所有描述符注册功能


usbd_msosv1_desc_register
""""""""""""""""""""""""""""""""""""

``usbd_msosv1_desc_register`` 用来注册一个 WINUSB 1.0 描述符。

.. code-block:: C

    void usbd_msosv1_desc_register(struct usb_msosv1_descriptor *desc);

- **desc**  描述符句柄

usbd_msosv2_desc_register
""""""""""""""""""""""""""""""""""""

``usbd_msosv2_desc_register`` 用来注册一个 WINUSB 2.0 描述符。

.. code-block:: C

    void usbd_msosv2_desc_register(struct usb_msosv2_descriptor *desc);

- **desc**  描述符句柄

usbd_bos_desc_register
""""""""""""""""""""""""""""""""""""

``usbd_bos_desc_register`` 用来注册一个 BOS 描述符， USB 2.1 版本以上必须注册。

.. code-block:: C

    void usbd_bos_desc_register(struct usb_bos_descriptor *desc);

- **desc**  描述符句柄

usbd_add_interface
""""""""""""""""""""""""""""""""""""

``usbd_add_interface`` 添加一个接口驱动。 **添加顺序必须按照描述符顺序**。

.. code-block:: C

    void usbd_add_interface(struct usbd_interface *intf);

- **intf**  接口驱动句柄，通常从不同 class 的 `xxx_init_intf` 函数获取

usbd_add_endpoint
""""""""""""""""""""""""""""""""""""

``usbd_add_endpoint`` 添加一个端点中断完成回调函数。

.. code-block:: C

    void usbd_add_endpoint(struct usbd_endpoint *ep);;

- **ep**    端点句柄

usbd_initialize
""""""""""""""""""""""""""""""""""""

``usbd_initialize`` 用来初始化 usb device 寄存器配置、usb 时钟、中断等，需要注意，此函数必须在所有列出的 API 最后。 **如果使用 os，必须放在线程中执行**。

.. code-block:: C

    int usbd_initialize(void);

usbd_event_handler
""""""""""""""""""""""""""""""""""""

``usbd_event_handler`` 是协议栈中中断或者协议栈一些状态的回调函数。大部分 IP 仅支持 USBD_EVENT_RESET 和 USBD_EVENT_CONFIGURED

.. code-block:: C

    void usbd_event_handler(uint8_t event);

CDC ACM
-----------------

usbd_cdc_acm_init_intf
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_init_intf`` 用来初始化 USB CDC ACM 类接口，并实现该接口相关的函数。

- ``cdc_acm_class_interface_request_handler`` 用来处理 USB CDC ACM 类 Setup 请求。
- ``cdc_notify_handler`` 用来处理 USB CDC 其他中断回调函数。

.. code-block:: C

    struct usbd_interface *usbd_cdc_acm_init_intf(struct usbd_interface *intf);

- **return**  接口句柄

usbd_cdc_acm_set_line_coding
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_line_coding`` 用来对串口进行配置，如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_set_line_coding(uint8_t intf, struct cdc_line_coding *line_coding);

- **intf** 控制接口号
- **line_coding** 串口配置

usbd_cdc_acm_get_line_coding
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_get_line_coding`` 用来获取串口进行配置，如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_get_line_coding(uint8_t intf, struct cdc_line_coding *line_coding);

- **intf** 控制接口号
- **line_coding** 串口配置

usbd_cdc_acm_set_dtr
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_dtr`` 用来控制串口 DTR 。如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_set_dtr(uint8_t intf, bool dtr);

- **intf** 控制接口号
- **dtr** dtr 为1表示拉低电平，为0表示拉高电平

usbd_cdc_acm_set_rts
""""""""""""""""""""""""""""""""""""

``usbd_cdc_acm_set_rts`` 用来控制串口 RTS 。如果仅使用 USB 而不用 串口，该接口不用用户实现，使用默认。

.. code-block:: C

    void usbd_cdc_acm_set_rts(uint8_t intf, bool rts);

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

    struct usbd_interface *usbd_hid_init_intf(struct usbd_interface *intf, const uint8_t *desc, uint32_t desc_len);

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

    struct usbd_interface *usbd_msc_init_intf(struct usbd_interface *intf, const uint8_t out_ep, const uint8_t in_ep);

- **out_ep**     out 端点地址
- **in_ep**      in 端点地址

usbd_msc_get_cap
""""""""""""""""""""""""""""""""""""

``usbd_msc_get_cap`` 用来获取存储器的 lun、扇区个数和每个扇区大小。用户必须实现该函数。

.. code-block:: C

    void usbd_msc_get_cap(uint8_t lun, uint32_t *block_num, uint16_t *block_size);

- **lun** 存储逻辑单元，暂时无用，默认支持一个
- **block_num**  存储扇区个数
- **block_size**  存储扇区大小

usbd_msc_sector_read
""""""""""""""""""""""""""""""""""""

``usbd_msc_sector_read`` 用来对存储器某个扇区开始的地址进行数据读取。用户必须实现该函数。

.. code-block:: C

    int usbd_msc_sector_read(uint32_t sector, uint8_t *buffer, uint32_t length);

- **sector** 扇区偏移
- **buffer** 存储读取的数据的指针
- **length** 读取长度


usbd_msc_sector_write
""""""""""""""""""""""""""""""""""""

``usbd_msc_sector_write``  用来对存储器某个扇区开始写入数据。用户必须实现该函数。

.. code-block:: C

    int usbd_msc_sector_write(uint32_t sector, uint8_t *buffer, uint32_t length);

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

    struct usbd_interface *usbd_audio_init_intf(struct usbd_interface *intf);

- **class** 类的句柄
- **intf**  接口句柄

usbd_audio_open
""""""""""""""""""""""""""""""""""""

``usbd_audio_open``  用来开启音频数据传输。

.. code-block:: C

    void usbd_audio_open(uint8_t intf);

- **intf** 开启的接口号

usbd_audio_close
""""""""""""""""""""""""""""""""""""

``usbd_audio_close``  用来关闭音频数据传输。

.. code-block:: C

    void usbd_audio_close(uint8_t intf);

- **intf** 关闭的接口号

usbd_audio_add_entity
""""""""""""""""""""""""""""""""""""

``usbd_audio_add_entity``  用来添加 unit 相关控制，例如 feature unit、clock source。

.. code-block:: C

    void usbd_audio_add_entity(uint8_t entity_id, uint16_t bDescriptorSubtype);

- **entity_id** 要添加的 unit id
- **bDescriptorSubtype** entity_id 的描述符子类型

usbd_audio_set_mute
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_mute``  用来设置静音。

.. code-block:: C

    void usbd_audio_set_mute(uint8_t ch, uint8_t enable);

- **ch** 要设置静音的通道
- **enable** 为1 表示静音，0相反

usbd_audio_set_volume
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_volume``  用来设置音量。

.. code-block:: C

    void usbd_audio_set_volume(uint8_t ch, float dB);

- **ch** 要设置音量的通道
- **dB** 要设置音量的分贝，其中 UAC1.0范围从 -127 ~ +127dB，UAC2.0 从 0 ~ 256dB

usbd_audio_set_sampling_freq
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_sampling_freq``  用来设置设备上音频模块的采样率

.. code-block:: C

    void usbd_audio_set_sampling_freq(uint8_t ep_ch, uint32_t sampling_freq);

- **ch** 要设置采样率的端点或者通道，UAC1.0为端点，UAC2.0 为通道
- **dB** 要设置的采样率

usbd_audio_get_sampling_freq_table
""""""""""""""""""""""""""""""""""""

``usbd_audio_get_sampling_freq_table``  用来获取支持的采样率列表，如果函数没有实现，则使用默认采样率列表。

.. code-block:: C

    void usbd_audio_get_sampling_freq_table(uint8_t **sampling_freq_table);

- **sampling_freq_table** 采样率列表地址，格式参考默认采样率列表

usbd_audio_set_pitch
""""""""""""""""""""""""""""""""""""

``usbd_audio_set_pitch``  用来设置音频音调，仅 UAC1.0 有这功能。

.. code-block:: C

    void usbd_audio_set_pitch(uint8_t ep, bool enable);

- **ep** 要设置音调的端点
- **enable** 开启或关闭音调

UVC
-----------------

usbd_video_init_intf
""""""""""""""""""""""""""""""""""""
``usbd_video_init_intf``  用来初始化 USB Video 类接口，并实现该接口相关的函数：

- ``video_class_interface_request_handler`` 用于处理 USB Video Setup 中断请求。
- ``video_notify_handler`` 用于实现 USB Video 其他中断回调函数。

.. code-block:: C

    struct usbd_interface *usbd_video_init_intf(struct usbd_interface *intf,
                                             uint32_t dwFrameInterval,
                                             uint32_t dwMaxVideoFrameSize,
                                             uint32_t dwMaxPayloadTransferSize);

- **class** 类的句柄
- **intf**  接口句柄

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

usbd_video_payload_fill
""""""""""""""""""""""""""""""""""""

``usbd_video_payload_fill``  用来填充 mjpeg 到新的 buffer中，其中会对 mjpeg 数据按帧进行切分，切分大小由 ``dwMaxPayloadTransferSize`` 控制，并添加头部信息，当前头部字节数为 2。头部信息见 ``struct video_mjpeg_payload_header``

.. code-block:: C

    uint32_t usbd_video_payload_fill(uint8_t *input, uint32_t input_len, uint8_t *output, uint32_t *out_len);

- **input** mjpeg 格式的数据包，从 FFD8~FFD9结束
- **input_len** mjpeg数据包大小
- **output** 输出缓冲区
- **out_len** 输出实际要发送的长度大小
- **return** 返回 usb 按照 ``dwMaxPayloadTransferSize`` 大小要发多少帧

DFU
-----------------

PRINTER
-----------------

MTP
-----------------