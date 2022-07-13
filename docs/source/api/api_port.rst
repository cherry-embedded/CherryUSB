Porting
=========================

device controller(dcd)
-------------------------

usb_dc_init
""""""""""""""""""""""""""""""""""""

``usb_dc_init`` 用于初始化 usb device controller 寄存器，设置 usb 引脚、时钟、中断等等。 **此函数不对用户开放**。

.. code-block:: C

    int usb_dc_init(void);

- **return**

usb_dc_deinit
""""""""""""""""""""""""""""""""""""

``usb_dc_deinit`` 用于反初始化 usb device controller 寄存器。 **此函数不对用户开放**。

.. code-block:: C

    int usb_dc_deinit(void);

- **return**

usb_dc_attach
""""""""""""""""""""""""""""""""""""

``usb_dc_attach`` 使能上拉或者下拉电阻，从而能够让设备被主机枚举。 **此函数对用户开放**。

.. code-block:: C

    int usb_dc_attach(void);

- **return**

usb_dc_detach
""""""""""""""""""""""""""""""""""""

``usb_dc_detach``断开设备与主机的连接。 **此函数对用户开放**。

.. code-block:: C

    int usb_dc_detach(void);

- **return**

usbd_set_address
""""""""""""""""""""""""""""""""""""

``usbd_set_address`` 设置设备地址。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_set_address(const uint8_t addr);

- **return**

usbd_ep_open
""""""""""""""""""""""""""""""""""""

``usbd_ep_open`` 设置端点的属性，开启对应端点的中断。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg);

- **return**

usbd_ep_close
""""""""""""""""""""""""""""""""""""

``usbd_ep_close`` 关闭端点。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_close(const uint8_t ep);

- **event**

usbd_ep_set_stall
""""""""""""""""""""""""""""""""""""

``usbd_ep_set_stall`` 将端点设置成 stall 状态并发送 stall 握手包。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_set_stall(const uint8_t ep);

- **ep** 端点地址

usbd_ep_clear_stall
""""""""""""""""""""""""""""""""""""

``usbd_ep_clear_stall`` 清除端点的 stall 状态。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_clear_stall(const uint8_t ep);

- **ep** 端点地址

usbd_ep_is_stalled
""""""""""""""""""""""""""""""""""""

``usbd_ep_is_stalled`` 读取当前端点的 stall 状态。 **此函数不对用户开放**。

.. code-block:: C

    int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled);

- **ep** 端点地址
- **return** 返回 1 表示 stalled，0 表示没有 stall

usbd_ep_write
""""""""""""""""""""""""""""""""""""

``usbd_ep_write`` 向某个端点发送数据， **如果该函数在中断中使用则是异步传输，否则是阻塞传输**。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes);

- **ep** in 端点地址
- **data** 要发送的数据缓冲区
- **data_len** 发送长度，需要小于等于端点最大包长
- **ret_bytes** 实际发送的长度，异步传输该参数无效。 **如果长度为 0，表示发送 0 长数据包（zero length packet）**
- **return** 返回 0 表示正确，其他表示错误

.. note:: 如果第一次在非中使用，该函数也是异步的哦，只有第二次调用会变成阻塞，所以可以配合完成中断当非阻塞用

usbd_ep_read
""""""""""""""""""""""""""""""""""""

``usbd_ep_read`` 从某个端点接收数据， **该函数仅能在 usb out 中断中使用**。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes);

- **ep** out 端点地址
- **data** 要接收的数据缓冲区
- **data_len** 接收长度，需要小于等于端点最大包长，推荐直接设置成最大包长。 **如果长度为 0 表示启动下次接收**
- **ret_bytes** 实际接收的长度
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_write_async(todo)
""""""""""""""""""""""""""""""""""""

``usbd_ep_write_async`` 向某个端点发送数据， 该函数为异步传输。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_write_async(const uint8_t ep, const uint8_t *data, uint32_t data_len);

- **ep** in 端点地址
- **data** 要发送的数据缓冲区
- **data_len** 发送长度，需要小于等于端点最大包长
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_read_async(todo)
""""""""""""""""""""""""""""""""""""

``usbd_ep_read_async`` 预先设置一块内存，并启动接收，通常配合 dma 使用，接收完成以后，触发注册的 out 中断。此函数一般在支持高速或者超高速的 ip 中使用，达到极致的带宽，如果 ip 没有该功能，则禁止使用。 **此函数对用户开放**。

.. code-block:: C

    int usbd_ep_read_async(const uint8_t ep, uint8_t *data, uint32_t max_data_len);

- **ep** out 端点地址
- **data** 要接收的数据缓冲区
- **data_len** 接收长度，需要小于等于端点最大包长，推荐直接设置成最大包长。 **如果长度为 0 表示准备接收 0 包**
- **return** 返回 0 表示正确，其他表示错误

usbd_ep_get_read_len(todo)
""""""""""""""""""""""""""""""""""""

``usbd_ep_get_read_len`` 获取实际接收长度，此函数搭配 ``usbd_ep_read_async`` 使用。 **此函数对用户开放**。

.. code-block:: C

    uint32_t usbd_ep_get_read_len(const uint8_t ep);

- **ep** out 端点地址
- **return** 实际接收长度

usbd_ep_get_mps(todo)
""""""""""""""""""""""""""""""""""""

``usbd_ep_get_mps`` 查询端点最大数据包长。 **此函数对用户开放**。

.. code-block:: C

    uint16_t usbd_ep_get_mps(const uint8_t ep);

- **ep** 端点地址
- **return** 返回端点最大数据包长

host controller(hcd)
------------------------

usb_hc_init
""""""""""""""""""""""""""""""""""""

``usb_hc_init`` 用于初始化 usb host controller 寄存器，设置 usb 引脚、时钟、中断等等。 **此函数不对用户开放**。

.. code-block:: C

    int usb_hc_init(void);

- **return** 返回 0 表示正确，其他表示错误

usbh_get_port_connect_status
""""""""""""""""""""""""""""""""""""

``usbh_get_port_connect_status`` 获取当前 hubport 连接状态。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_get_port_connect_status(const uint8_t port);

- **port** 端口号
- **return** 返回 1 表示连接，0 表示未连接

usbh_reset_port
""""""""""""""""""""""""""""""""""""

``usbh_reset_port`` 复位指定的 hubport **此函数不对用户开放**。

.. code-block:: C

    int usbh_reset_port(const uint8_t port);

- **port** 端口号
- **return** 返回 0 表示正确，其他表示错误

usbh_get_port_speed
""""""""""""""""""""""""""""""""""""

``usbh_get_port_speed`` 获取当前 hubport 上连接的设备速度。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_get_port_speed(const uint8_t port);

- **port** 端口号
- **return** 返回 1 表示低速，2 表示全速，3 表示高速

usbh_ep0_reconfigure
""""""""""""""""""""""""""""""""""""

``usbh_ep0_reconfigure`` 重新设置端点 0 的属性。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed);

- **ep** 端点信息
- **dev_addr** 端点所在设备地址
- **ep_mps** 端点最大包长
- **speed** 端点所在设备的速度
- **return** 返回 0 表示正确，其他表示错误

usbh_ep_alloc
""""""""""""""""""""""""""""""""""""

``usbh_ep_alloc`` 为端点分配相关属性，初始化相关寄存器，并保存相关信息到 **ep** 句柄中。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg);

- **ep** 端点信息
- **ep_cfg** 端点初始化需要的一些信息
- **return** 返回 0 表示正确，其他表示错误

usbh_ep_free
""""""""""""""""""""""""""""""""""""

``usbh_ep_free`` 释放端点的一些属性。 **此函数不对用户开放**。

.. code-block:: C

    int usbh_ep_free(usbh_epinfo_t ep);

- **ep** 端点信息
- **return** 返回 0 表示正确，其他表示错误

usbh_control_transfer
""""""""""""""""""""""""""""""""""""

``usbh_control_transfer`` 对端点 0 进行控制传输，并且 **此函数为阻塞式传输，默认超时时间 5s**。 **此函数对用户开放**。

.. code-block:: C

    int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer);

- **ep** 端点信息
- **setup** setup 包
- **buffer** 要发送或者读取的数据缓冲区，为 NULL 表示没有数据要发送或者接收
- **return** 返回 0 表示正确，其他表示错误

usbh_ep_bulk_transfer
""""""""""""""""""""""""""""""""""""

``usbh_ep_bulk_transfer`` 对指定端点进行批量传输， **此函数为阻塞式传输**。 **此函数对用户开放**。

.. code-block:: C

    int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

- **ep** 端点信息
- **buffer** 要发送或者读取的数据缓冲区
- **buflen** 要发送或者接收的长度，最大不得高于 16K
- **timeout** 超时时间，单位 ms
- **return** 大于等于0 表示实际发送或者接收的长度，小于 0 表示错误

其中小于 0 的错误码如下：

.. list-table::
    :widths: 30 30
    :header-rows: 1

    * - ERROR CODE
      - desc
    * - ENODEV
      - 设备未连接
    * - EBUSY
      - 当前数据发送或者接收还未完成
    * - EAGAIN
      - 主机一直收到 NAK 包
    * - ETIMEDOUT
      - 数据发送或者接收超时
    * - EPERM
      - 主机收到 STALL 包
    * - EIO
      - 数据传输错误
    * - EPIPE
      - 数据溢出
    * - ENXIO
      - 设备断开，传输中止

usbh_ep_intr_transfer
""""""""""""""""""""""""""""""""""""

``usbh_ep_intr_transfer`` 同上。

usbh_ep_bulk_async_transfer
""""""""""""""""""""""""""""""""""""

``usbh_ep_bulk_async_transfer`` 对指定端点进行批量传输，传输完成将触发指定回调函数， **此函数为异步传输**。 **此函数对用户开放**。

.. code-block:: C

    int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg);

- **ep** 端点信息
- **buffer** 要发送或者读取的数据缓冲区
- **buflen** 要发送或者接收的长度，最大不得高于 16K
- **callback** 传输完成回调函数， **该函数最终处于中断上下文**
- **arg** 用户自定义参数
- **return** 为 0 表示配置正常，小于0 表示错误

usbh_ep_intr_async_transfer
""""""""""""""""""""""""""""""""""""

``usbh_ep_intr_async_transfer`` 同上。

usb_ep_cancel
""""""""""""""""""""""""""""""""""""

``usb_ep_cancel`` 中止当前端点传输， **此函数不对用户开放**。

.. code-block:: C

    int usb_ep_cancel(usbh_epinfo_t ep);

- **ep** 端点信息
- **return** 为 0 表示正确，小于0 表示错误
