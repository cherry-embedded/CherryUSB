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
        uint8_t class;    /* Base device class code */
        uint8_t subclass; /* Sub-class, depends on base class. Eg. */
        uint8_t protocol; /* Protocol, depends on base class. Eg. */
        uint16_t vid;     /* Vendor ID (for vendor/product specific devices) */
        uint16_t pid;     /* Product ID (for vendor/product specific devices) */
        const struct usbh_class_driver *class_driver;
    };

端点结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    typedef struct usbh_endpoint {
        struct usb_endpoint_descriptor ep_desc;
    } usbh_endpoint_t;

接口结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    typedef struct usbh_interface {
        struct usb_interface_descriptor intf_desc;
        struct usbh_endpoint ep[CONFIG_USBHOST_EP_NUM];
        char devname[CONFIG_USBHOST_DEV_NAMELEN];
        struct usbh_class_driver *class_driver;
        void *priv;
    } usbh_interface_t;


配置结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    typedef struct usbh_configuration {
        struct usb_configuration_descriptor config_desc;
        struct usbh_interface intf[CONFIG_USBHOST_INTF_NUM];
    } usbh_configuration_t;

hubport 结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    typedef struct usbh_hubport {
        bool connected;    /* True: device connected; false: disconnected */
        bool port_change;  /* True: port changed; false: port do not change */
        uint8_t port;      /* Hub port index */
        uint8_t dev_addr;  /* device address */
        uint8_t speed;     /* device speed */
        usbh_epinfo_t ep0; /* control ep info */
        struct usb_device_descriptor device_desc;
        struct usbh_configuration config;
    #if 0
        uint8_t* config_desc;
    #endif
        struct usb_setup_packet *setup;
        struct usbh_hub *parent; /*if NULL, is roothub*/
    } usbh_hubport_t;

hub 结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    typedef struct usbh_hub {
        usb_slist_t list;
        uint8_t index;    /* Hub index */
        uint8_t nports;   /* Hub port number */
        uint8_t dev_addr; /* Hub device address */
        usbh_epinfo_t intin;
        uint8_t *int_buffer;
        struct hub_port_status *port_status;
        struct usb_hub_descriptor hub_desc;
        struct usbh_hubport child[CONFIG_USBHOST_EHPORTS];
        struct usbh_hubport *parent; /* Parent hub port */
        struct usb_work work;
    } usbh_hub_t;

usbh_event_notify_handler
""""""""""""""""""""""""""""""""""""

``usbh_event_notify_handler`` 是 USB 中断中的核心，主要用于处理 **设备连接** 和 **设备断开** 中断，从而唤醒线程去执行枚举。

.. code-block:: C

    void usbh_event_notify_handler(uint8_t event, uint8_t rhport);

- **event**  中断事件
- **rhport** roothub 端口号

其中 ``event`` 有如下类型：

.. code-block:: C

    enum usbh_event_type {
        USBH_EVENT_ATTACHED,
        USBH_EVENT_REMOVED,
    };

usbh_initialize
""""""""""""""""""""""""""""""""""""

``usbh_initialize`` 用来初始化 usb 主机协议栈，包括：创建插拔检测用的信号量和枚举线程、高低工作队列、初始化 roothub 端点0 配置，初始化 usb 主机控制器。

.. code-block:: C

    int usbh_initialize(void);

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

CDC ACM
-----------------

HID
-----------------

MSC
-----------------

RNDIS
-----------------

PRINTER
-----------------