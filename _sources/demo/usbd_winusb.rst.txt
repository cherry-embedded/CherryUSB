usbd_winusb
===============

本节主要介绍 winusb 驱动。winusb 是 windows 为了让用户友好的访问 USB 自定义类设备提供的一套通用驱动，其实本质就是 CDC ACM。
WINUSB 版本根据 USB 版本分为 V1/V2 版本，V2 版本需要包含 BOS 描述符，V1 版本不需要。V2 版本需要在设备描述符中设置为 USB2.1 的版本号。

.. note:: 如果使用复合设备，必须使用 V2 版本的 winusb 驱动。

- V1 版本注册描述符

.. code-block:: C

    usbd_msosv1_desc_register(busid, &msosv1_desc);

- V2 版本注册描述符

.. code-block:: C

    usbd_bos_desc_register(busid, &bos_desc);
    usbd_msosv2_desc_register(busid, &msosv2_desc);


- 接口描述符注册

.. code-block:: C

    /* Interface 0 */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02),
    /* Endpoint OUT 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_OUT_EP, USB_ENDPOINT_TYPE_BULK, WINUSB_EP_MPS, 0x00),
    /* Endpoint IN 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(WINUSB_IN_EP, USB_ENDPOINT_TYPE_BULK, WINUSB_EP_MPS, 0x00),

- 其余操作与 CDC ACM 相同，不再赘述

在 V2 版本中，WINUSB 需要提供 WCID 描述符，有两个宏，分别是 `USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_SINGLE_DESCRIPTOR_INIT` 和 `USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_INIT`，这里需要注意，
前者用于单 WINUSB 设备（也就是接口总数只能是1），后者用于复合设备，可以使用 1 ~ 多个 WINUSB 接口的设备。因此，即使同样是 1个 WINUSB 设备，不同情况宏的配置也是不一样的，只能说，微软工程师的脑回路就是跟正常人不一样。

.. code-block:: C

    const uint8_t WINUSB_WCIDDescriptor[] = {
    #if WINUSB_NUM == 1
        USB_MSOSV2_COMP_ID_SET_HEADER_DESCRIPTOR_INIT(10 + USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_SINGLE_DESCRIPTOR_LEN),
        USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_SINGLE_DESCRIPTOR_INIT(),
    #else
        USB_MSOSV2_COMP_ID_SET_HEADER_DESCRIPTOR_INIT(10 + WINUSB_NUM * USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_LEN),
        USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_INIT(0x00),
        USB_MSOSV2_COMP_ID_FUNCTION_WINUSB_MULTI_DESCRIPTOR_INIT(0x01),
    #endif
    };