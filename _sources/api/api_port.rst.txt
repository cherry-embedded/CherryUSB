主从驱动
=========================

.. note:: 请注意，v1.1 版本开始增加 busid 形参，其余保持不变，所以 API 说明不做更新

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

.. note:: 启动接收以后，以下两种情况，会进入传输完成中断：1、最后一包为短包（小于 EP MPS）；2、接收总长度等于 data_len

.. note:: 对于 bulk 传输，data_len 通常设计为 EP MPS，以下三种情况可以修改为多个 EP MPS: 固定长度；自定义协议并携带长度（MSC）; 主机手动发送 ZLP 或者短包（RNDIS）

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

``usbh_submit_urb`` 对某个地址上的端点进行数据请求。 **此函数对用户开放**。

.. code-block:: C

    int usbh_submit_urb(struct usbh_urb *urb);

- **urb** usb 请求块
- **return** 返回 0 表示正确，其他表示错误

其中， `urb` 结构体信息如下：

.. code-block:: C

  struct usbh_urb {
      void *hcpriv;
      struct usbh_hubport *hport;
      struct usb_endpoint_descriptor *ep;
      uint8_t data_toggle;
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

.. note:: timeout 如何没有特别对时间的要求，必须设置成 0xffffffff，原则上不允许超时，如果超时了，一般不能再继续工作

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