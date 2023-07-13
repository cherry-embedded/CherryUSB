Porting
=========================

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

- **return** 返回 0 表示正确，其他表示错误

usbd_ep_open
""""""""""""""""""""""""""""""""""""

``usbd_ep_open`` 设置端点的属性，开启对应端点的中断。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg);

- **return** 返回 0 表示正确，其他表示错误

usbd_ep_close
""""""""""""""""""""""""""""""""""""

``usbd_ep_close`` 关闭端点。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_close(const uint8_t ep);

- **event**
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

``usbd_ep_start_write`` 启动端点发送，发送完成以后，会调用注册的 in 端点传输完成中断回调函数。该函数为异步发送。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_start_write(const uint8_t ep, const uint8_t *data, uint32_t data_len);

- **ep** in 端点地址
- **data** 发送数据缓冲区
- **data_len** 发送长度，原则上无限长，推荐 16K 字节以内
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_start_read
""""""""""""""""""""""""""""""""""""

``usbd_ep_start_read``  启动端点接收，接收完成以后，会调用注册的 out 端点传输完成中断回调函数。该函数为异步接收。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_start_read(const uint8_t ep, uint8_t *data, uint32_t data_len);

- **ep** out 端点地址
- **data** 接收数据缓冲区
- **data_len** 接收长度，原则上无限长，推荐 16K 字节以内，并且推荐是最大包长的整数倍
- **return** 返回 0 表示正确，其他表示错误

.. note:: 启动接收以后，以下两种情况，会进入传输完成中断：1、最后一包为短包；2、接收总长度等于 data_len

host controller(hcd)
------------------------

usb_hc_init
""""""""""""""""""""""""""""""""""""

``usb_hc_init`` 用于初始化 usb host controller 寄存器，设置 usb 引脚、时钟、中断等等。 **此函数不对用户开放**。

.. code-block:: C

    int usb_hc_init(void);

- **return** 返回 0 表示正确，其他表示错误

usbh_roothub_control
""""""""""""""""""""""""""""""""""""

``usbh_roothub_control`` 用来对 roothub 发起请求， **此函数不对用户开放**。

.. code-block:: C

    int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf);

- **setup** 请求
- **buf** 接收缓冲区
- **return** 返回 0 表示正确，其他表示错误

usbh_ep_pipe_reconfigure
""""""""""""""""""""""""""""""""""""

``usbh_ep_pipe_reconfigure`` 重新设置端点 0 的 pipe 属性。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t mult);

- **pipe** pipe 句柄
- **dev_addr** 端点所在设备地址
- **ep_mps** 端点最大包长
- **mult** 端点一次传输个数
- **return** 返回 0 表示正确，其他表示错误

usbh_pipe_alloc
""""""""""""""""""""""""""""""""""""

``usbh_pipe_alloc`` 为端点分配 pipe。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg);

- **pipe** pipe 句柄
- **ep_cfg** 端点初始化需要的一些信息
- **return** 返回 0 表示正确，其他表示错误

usbh_pipe_free
""""""""""""""""""""""""""""""""""""

``usbh_pipe_free`` 释放端点的一些属性。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_pipe_free(usbh_pipe_t pipe);

- **pipe** 端点信息
- **return** 返回 0 表示正确，其他表示错误

usbh_submit_urb
""""""""""""""""""""""""""""""""""""

``usbh_submit_urb`` 对某个地址上的端点进行数据请求。 **此函数对用户开放**。

.. code-block:: C

    int usbh_submit_urb(struct usbh_urb *urb);

- **urb** usb 请求块
- **return** 返回 0 表示正确，其他表示错误

其中， `urb` 结构体信息如下：

.. code-block:: C

    struct usbh_urb {
        usbh_pipe_t pipe;
        struct usb_setup_packet *setup;
        uint8_t *transfer_buffer;
        uint32_t transfer_buffer_length;
        int transfer_flags;
        uint32_t actual_length;
        uint32_t timeout;
        int errorcode;
        uint32_t num_of_iso_packets;
        usbh_complete_callback_t complete;
        void *arg;
        struct usbh_iso_frame_packet iso_packet[];
    };

- **pipe** 端点对应的 pipe 句柄
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

`errorcode` 可以返回以下值：

.. list-table::
    :widths: 30 30
    :header-rows: 1

    * - ERROR CODE
      - desc
    * - ENOMEM
      - 内存不足
    * - ENODEV
      - 设备未连接
    * - EBUSY
      - 当前数据发送或者接收还未完成
    * - ETIMEDOUT
      - 数据发送或者接收超时
    * - EPERM
      - 主机收到 STALL 包或者 BABBLE
    * - EIO
      - 数据传输错误
    * - EAGAIN
      - 主机一直收到 NAK 包
    * - EPIPE
      - 数据溢出
    * - ESHUTDOWN
      - 设备断开，传输中止

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