主从驱动
=========================

.. note:: 部分历史接口示例省略了 busid 等参数，使用时以当前版本的头文件声明为准。

setup 和传输 buffer 按当前控制器要求放入 DMA 可访问的内存，并满足地址及 cache line 对齐；根据配置使用非缓存区或已有 cache 维护接口。

device controller(dcd)
-------------------------

usb_dc_init
""""""""""""""""""""""""""""""""""""

``usb_dc_init`` 用于初始化 usb device controller 寄存器，设置 usb 引脚、时钟、中断等等。 **此函数不对用户开放**。

.. code-block:: C

    int usb_dc_init(void);

- **return** 返回 0 表示正确，其他表示错误

usb_dc_deinit
""""""""""""""""""""""""""""""""""""

``usb_dc_deinit`` 用于反初始化 usb device controller 寄存器。 **此函数不对用户开放**。

.. code-block:: C

    int usb_dc_deinit(void);

- **return** 返回 0 表示正确，其他表示错误

usbd_set_address
""""""""""""""""""""""""""""""""""""

``usbd_set_address`` 设置设备地址。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_set_address(const uint8_t addr);

- **addr** 设备地址
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_open
""""""""""""""""""""""""""""""""""""

``usbd_ep_open`` 设置端点的属性，开启对应端点的中断。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_open(const struct usb_endpoint_descriptor *ep);

- **ep** 端点描述符
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_close
""""""""""""""""""""""""""""""""""""

``usbd_ep_close`` 关闭端点。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_close(const uint8_t ep);

- **ep** 端点地址
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_set_stall
""""""""""""""""""""""""""""""""""""

``usbd_ep_set_stall`` 将端点设置成 stall 状态并发送 stall 握手包。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_set_stall(const uint8_t ep);

- **ep** 端点地址
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_clear_stall
""""""""""""""""""""""""""""""""""""

``usbd_ep_clear_stall`` 清除端点的 stall 状态。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_clear_stall(const uint8_t ep);

- **ep** 端点地址
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_is_stalled
""""""""""""""""""""""""""""""""""""

``usbd_ep_is_stalled`` 读取当前端点的 stall 状态。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled);

- **ep** 端点地址
- **return** 返回 1 表示 stalled，0 表示没有 stall

usbd_ep_start_write
""""""""""""""""""""""""""""""""""""

``usbd_ep_start_write`` 向 IN 端点启动异步发送。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len);

- **busid** USB 总线 id
- **ep** IN 端点地址，包含方向位
- **data** 发送数据缓冲区
- **data_len** 发送长度，零长度用于发送 ZLP
- **return** 返回 0 表示启动成功，负值表示失败

发送结束后进入注册的 IN 完成回调，回调的 ``nbytes`` 为实际长度。发送 buffer 保持有效且内容稳定，直到本次传输完成。同一 IN 端点的发送由应用串行安排，根据启动返回值决定是否等待本次完成通知。

发送长度为 MPS 整数倍时，是否追加 ZLP 由 class/协议决定，按对应 class/demo 的用法处理。

usbd_ep_start_read
""""""""""""""""""""""""""""""""""""

``usbd_ep_start_read`` 向 OUT 端点挂接异步接收。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len);

- **busid** USB 总线 id
- **ep** OUT 端点地址
- **data** 接收数据缓冲区，在本次接收完成前保持有效
- **data_len** 请求接收长度，通常取端点 MPS 的整数倍；buffer 容量满足当前 DCD 的接收范围要求
- **return** 返回 0 表示启动成功，负值表示失败

接收达到请求长度或遇到短包（含 ZLP）后进入注册的 OUT 完成回调，``nbytes`` 为实际接收长度，应用据此处理数据。

普通端点在 ``USBD_EVENT_CONFIGURED`` 后开始使用，先挂接第一次 OUT 接收；持续接收时在每次完成后再次挂接。同一 OUT 端点的接收由应用串行安排，数据处理与下一次接收通过 buffer 交接协调。

.. note:: 对于 bulk 传输，data_len 通常设计为 EP MPS。固定长度、自定义协议携带长度（如 MSC）、主机发送 ZLP 或短包（如 RNDIS）时，可按协议使用多个 EP MPS。

端点完成回调在 USB ISR 中执行，适合简短处理、继续挂接传输或通过 RTOS/OSAL 通知工作线程；耗时业务放在线程中执行。

host controller(hcd)
------------------------

usb_hc_init
""""""""""""""""""""""""""""""""""""

``usb_hc_init`` 用于初始化 usb host controller 寄存器，设置 usb 引脚、时钟、中断等等。 **此函数不对用户开放**。

.. code-block:: C

    int usb_hc_init(void);

- **return** 返回 0 表示正确，其他表示错误

usb_hc_deinit
""""""""""""""""""""""""""""""""""""

``usb_hc_deinit`` 用于反初始化 usb host controller 寄存器。 **此函数不对用户开放**。

.. code-block:: C

    int usb_hc_deinit(void);

- **return** 返回 0 表示正确，其他表示错误

usbh_roothub_control
""""""""""""""""""""""""""""""""""""

``usbh_roothub_control`` 用来对 roothub 发起请求， **此函数不对用户开放**。

.. code-block:: C

    int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf);

- **setup** 请求
- **buf** 接收缓冲区
- **return** 返回 0 表示正确，其他表示错误

usbh_submit_urb
""""""""""""""""""""""""""""""""""""

``usbh_submit_urb`` 向 ``urb->hport`` 对应设备的 ``urb->ep`` 端点提交传输。 **此函数对用户开放**。

.. code-block:: C

    int usbh_submit_urb(struct usbh_urb *urb);

- **urb** USB 请求块，包含端口、端点、setup、buffer、长度、timeout 和完成回调等参数
- **return** 返回 0 表示成功，负值表示错误；同步和异步的成功含义如下

每次调用 ``usbh_submit_urb()`` 前，都要调用对应的 ``usbh_control_urb_fill()``、``usbh_bulk_urb_fill()``、``usbh_int_urb_fill()`` 等 fill 函数填写本次请求参数，包括完成回调中的再次提交。URB 在首次使用前初始化；控制传输包含 setup 包，interrupt 传输包含 interval。

- **同步传输**：``timeout > 0``，``complete = NULL``。调用线程等待完成或超时，返回 0 表示成功，实际长度读取 ``urb->actual_length``。
- **异步传输**：``timeout = 0``，设置 ``complete`` 和 ``arg``。返回 0 表示提交成功；完成后调用 ``complete(arg, nbytes)``，非负值为实际长度，负值为错误。正常传输完成回调在 USB ISR 中执行，适合简短处理、继续提交或通知工作线程。

每笔在途请求独占其 URB 和 buffer，相关 setup、buffer、arg 在传输及收尾期间保持有效。完成后调用对应的 fill 函数填写下一笔传输参数，再次提交。``hcpriv`` 由 HCD 维护。

其中， `urb` 结构体信息如下：

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

- **hcpriv** 主机控制器驱动私有成员
- **hport** 当前 urb 使用的 hport
- **ep** 当前 urb 使用的 ep
- **data_toggle** 当前 data toggle
- **interval** urb 传输间隔，单位 us，如果 interval 大于 1000us，则需要使用软件定时器来维护
- **setup** setup 请求缓冲区，端点0使用
- **transfer_buffer** 传输的数据缓冲区
- **transfer_buffer_length** 传输长度
- **transfer_flags** 传输时携带的 flag
- **actual_length** 实际传输长度
- **timeout** 传输超时时间，为 0 该函数则为非阻塞，可在中断中使用
- **errorcode** 错误码
- **num_of_iso_packets** iso 帧或者微帧个数
- **complete** 传输完成回调函数
- **arg** 传输完成时携带的参数
- **iso_packet** iso 数据包

.. note:: 同步传输按业务要求设置等待时间；同步超时时由 submit 路径负责取消和收尾，上层处理 submit 的返回结果。异步传输通过 complete 回调通知结果。

`errorcode` 可以返回以下值：

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

其中 `iso_packet` 结构体信息如下：

.. code-block:: C

  struct usbh_iso_frame_packet {
      uint8_t *transfer_buffer;
      uint32_t transfer_buffer_length;
      uint32_t actual_length;
      int errorcode;
  };

- **transfer_buffer** 传输的数据缓冲区
- **transfer_buffer_length** 传输长度
- **actual_length** 实际传输长度
- **errorcode** 错误码

usbh_kill_urb
""""""""""""""""""""""""""""""""""""

``usbh_kill_urb`` 取消指定 URB 的传输。 **此函数对用户开放**。

.. code-block:: C

    int usbh_kill_urb(struct usbh_urb *urb);

- **urb** 此前提交的同一个 USB 请求块
- **return** 返回 0 表示取消调用成功，负值表示错误

使用场景：枚举线程处理设备断连时取消 class 的在途 URB；同步传输超时时由 submit 路径负责取消和收尾，上层处理 submit 的返回结果。

取消同步请求时唤醒等待线程，取消异步请求时执行错误回调。错误回调在 kill 调用者上下文执行，只处理报错并结束本次传输；正常成功回调按业务需要继续提交。

断连处理按停止业务提交、取消在途请求、结束相关工作线程、释放应用对象的顺序进行。URB 和数据 buffer 的应用内存由调用方管理。
