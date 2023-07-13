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
        struct usbh_interface_altsetting altsetting[CONFIG_USBHOST_MAX_INTF_ALTSETTINGS];
        uint8_t altsetting_num;
        char devname[CONFIG_USBHOST_DEV_NAMELEN];
        struct usbh_class_driver *class_driver;
        void *priv;
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
        usbh_pipe_t ep0;  /* control ep pipe info */
        struct usb_device_descriptor device_desc;
        struct usbh_configuration config;
        const char *iManufacturer;
        const char *iProduct;
        const char *iSerialNumber;
        uint8_t *raw_config_desc;
        struct usb_setup_packet *setup;
        struct usbh_hub *parent;
    #ifdef CONFIG_USBHOST_XHCI
        uint32_t protocol; /* port protocol, for xhci, some ports are USB2.0, others are USB3.0 */
    #endif
        usb_osal_thread_t thread;
    };

hub 结构体
""""""""""""""""""""""""""""""""""""

.. code-block:: C

    struct usbh_hub {
        usb_slist_t list;
        bool connected;
        bool is_roothub;
        uint8_t index;
        uint8_t hub_addr;
        usbh_pipe_t intin;
        uint8_t *int_buffer;
        struct usbh_urb intin_urb;
        struct usb_hub_descriptor hub_desc;
        struct usbh_hubport child[CONFIG_USBHOST_MAX_EHPORTS];
        struct usbh_hubport *parent;
    };

usbh_initialize
""""""""""""""""""""""""""""""""""""

``usbh_initialize`` 用来初始化 usb 主机协议栈，包括：初始化 usb 主机控制器，创建 roothub 设备，创建 hub 检测线程。

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