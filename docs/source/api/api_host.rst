主机协议栈
=========================

关于主机协议栈中结构体的命名、分类、成员组成，参考下面这两张图：

.. figure:: img/api_host1.png
.. figure:: img/api_host2.png

CORE
-----------------

CLASS 驱动信息结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_class_info {
        uint8_t match_flags;           /* Used for product specific matches; range is inclusive */
        uint8_t bInterfaceClass;       /* Base device class code */
        uint8_t bInterfaceSubClass;    /* Sub-class, depends on base class. Eg. */
        uint8_t bInterfaceProtocol;    /* Protocol, depends on base class. Eg. */
        const uint16_t (*id_table)[2]; /* List of Vendor/Product ID pairs */
        const struct usbh_class_driver *class_driver;
    };

端点结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_endpoint {
        struct usb_endpoint_descriptor ep_desc;
    };

接口备用结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_interface_altsetting {
        struct usb_interface_descriptor intf_desc;
        struct usbh_endpoint ep[CONFIG_USBHOST_MAX_ENDPOINTS];
    };

接口结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_interface {
        char devname[CONFIG_USBHOST_DEV_NAMELEN];
        struct usbh_class_driver *class_driver;
        void *priv;
        struct usbh_interface_altsetting altsetting[CONFIG_USBHOST_MAX_INTF_ALTSETTINGS];
        uint8_t altsetting_num;
    };

配置结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_configuration {
        struct usb_configuration_descriptor config_desc;
        struct usbh_interface intf[CONFIG_USBHOST_MAX_INTERFACES];
    };

hubport 结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_hubport {
        bool connected;   /* True: device connected; false: disconnected */
        uint8_t port;     /* Hub port index */
        uint8_t dev_addr; /* device address */
        uint8_t speed;    /* device speed */
        uint8_t depth;    /* distance from root hub */
        uint8_t route;    /* route string */
        uint8_t slot_id;  /* slot id */
        struct usb_device_descriptor device_desc;
        struct usbh_configuration config;
        const char *iManufacturer;
        const char *iProduct;
        const char *iSerialNumber;
        uint8_t *raw_config_desc;
        struct usb_setup_packet *setup;
        struct usbh_hub *parent;
        struct usbh_hub *self; /* if this hubport is a hub */
        struct usbh_bus *bus;
        struct usb_endpoint_descriptor ep0;
        struct usbh_urb ep0_urb;
        usb_osal_mutex_t mutex;
    };

hub 结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_hub {
        bool connected;
        bool is_roothub;
        uint8_t index;
        uint8_t hub_addr;
        uint8_t speed;
        uint8_t nports;
        uint8_t powerdelay;
        uint8_t tt_think;
        bool ismtt;
        struct usb_hub_descriptor hub_desc; /* USB 2.0 only */
        struct usb_hub_ss_descriptor hub_ss_desc; /* USB 3.0 only */
        struct usbh_hubport child[CONFIG_USBHOST_MAX_EHPORTS];
        struct usbh_hubport *parent;
        struct usbh_bus *bus;
        struct usb_endpoint_descriptor *intin;
        struct usbh_urb intin_urb;
        uint8_t *int_buffer;
        struct usb_osal_timer *int_timer;
    };

usbh_initialize
""""""""""""""""""""""""""""""""""""

``usbh_initialize`` 用来初始化 usb 主机协议栈，包括：初始化 usb 主机控制器，创建 roothub 设备，创建 hub 检测线程。

.. code-block:: C

    int usbh_initialize(uint8_t busid, uint32_t reg_base, usbh_event_handler_t event_handler);

- **busid**  bus id，从 0开始，不能超过 `CONFIG_USBHOST_MAX_BUS`
- **reg_base**  hcd 寄存器基地址
- **event_handler**  host 事件回调函数，可以为NULL
- **return**  0 表示正常其他表示错误

usbh_find_class_instance
""""""""""""""""""""""""""""""""""""

``usbh_find_class_instance`` 根据注册的 class 名称查找对应的 class 结构体句柄。

.. code-block:: C

    void *usbh_find_class_instance(const char *devname);

- **devname**  class 名称
- **return**  class 结构体句柄

lsusb
""""""""""""""""""""""""""""""""""""

``lsusb`` 用来查看和操作 hub 上的设备信息。需要借助 shell 插件使用。

.. code-block:: C

    int lsusb(int argc, char **argv);

SERIAL
-----------------

usbh_serial_open
""""""""""""""""""""""""""""""""""""

``usbh_serial_open`` 根据路径打开一个串口设备。

.. code-block:: C

    struct usbh_serial *usbh_serial_open(const char *devname, uint32_t open_flags);

- **devname**  串口路径
- **open_flags**  打开标志，参考 `USBH_SERIAL_OFLAG_*` 定义
- **return**  serial 结构体句柄

usbh_serial_close
""""""""""""""""""""""""""""""""""""

``usbh_serial_close`` 关闭串口设备。

.. code-block:: C

    void usbh_serial_close(struct usbh_serial *serial);

- **serial**  serial 结构体句柄

usbh_serial_control
""""""""""""""""""""""""""""""""""""

``usbh_serial_control`` 对串口进行配置。

.. code-block:: C

    int usbh_serial_control(struct usbh_serial *serial, int cmd, void *arg);

- **serial**  serial 结构体句柄
- **cmd**  控制命令，参考 `USBH_SERIAL_CMD_*` 定义
- **arg**  控制参数指针
- **return**  0 表示正常其他表示错误

usbh_serial_write
""""""""""""""""""""""""""""""""""""

``usbh_serial_write`` 向串口写数据。

.. code-block:: C

    int usbh_serial_write(struct usbh_serial *serial, const void *buffer, uint32_t buflen);

- **serial**  serial 结构体句柄
- **buffer**  数据缓冲区指针
- **buflen**  要写入的数据长度，如果是 USB2TTL 设备，一次最高 wMaxPacketSize
- **return**  实际写入的数据长度或者错误码

.. note:: 有无设置波特率都可以使用该 API，当未设置波特率时，长度无限制，如果设置了波特率则为 wMaxPacketSize。

usbh_serial_read
""""""""""""""""""""""""""""""""""""

``usbh_serial_read`` 从串口读数据。 **如果没有设置波特率，不允许使用该 API，设置波特率后，内部会开启 rx 接收并将数据写入 ringbuf **。

.. code-block:: C

    int usbh_serial_read(struct usbh_serial *serial, void *buffer, uint32_t buflen);

- **serial**  serial 结构体句柄
- **buffer**  数据缓冲区指针
- **buflen**  要读取的最大数据长度
- **return**  实际读取的数据长度或者错误码

usbh_serial_cdc_write_async
""""""""""""""""""""""""""""""""""""

``usbh_serial_cdc_write_async`` 异步从串口读数据。 **如果设置了波特率，不允许使用该 API**。

.. code-block:: C

    int usbh_serial_cdc_write_async(struct usbh_serial *serial, uint8_t *buffer, uint32_t buflen, usbh_complete_callback_t complete, void *arg);

- **serial**  serial 结构体句柄
- **buffer**  数据缓冲区指针
- **buflen**  要发送的数据长度
- **complete**  读数据完成回调函数
- **arg**  回调函数参数
- **return**  0 表示正常其他表示错误

usbh_serial_cdc_read_async
""""""""""""""""""""""""""""""""""""

``usbh_serial_cdc_read_async`` 异步从串口读数据。 **如果设置了波特率，不允许使用该 API，设置波特率后，内部会开启 rx 接收并将数据写入 ringbuf **。

.. code-block:: C

    int usbh_serial_cdc_read_async(struct usbh_serial *serial, uint8_t *buffer, uint32_t buflen, usbh_complete_callback_t complete, void *arg);

- **serial**  serial 结构体句柄
- **buffer**  数据缓冲区指针
- **buflen**  要读取的最大数据长度，一次最高 16K。并且需要是 wMaxPacketSize 的整数倍
- **complete**  读数据完成回调函数
- **arg**  回调函数参数
- **return**  0 表示正常其他表示错误


HID
-----------------

MSC
-----------------

NETWORK
-----------------