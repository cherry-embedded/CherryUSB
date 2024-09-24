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
        uint8_t match_flags; /* Used for product specific matches; range is inclusive */
        uint8_t class;       /* Base device class code */
        uint8_t subclass;    /* Sub-class, depends on base class. Eg. */
        uint8_t protocol;    /* Protocol, depends on base class. Eg. */
        uint16_t vid;        /* Vendor ID (for vendor/product specific devices) */
        uint16_t pid;        /* Product ID (for vendor/product specific devices) */
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

    int usbh_initialize(uint8_t busid, uint32_t reg_base);

- **busid**  bus id，从 0开始，不能超过 `CONFIG_USBHOST_MAX_BUS`
- **reg_base**  hcd 寄存器基地址
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

CDC ACM
-----------------

HID
-----------------

MSC
-----------------

RNDIS
-----------------