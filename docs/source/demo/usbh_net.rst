usbh_net
===============

本节主要介绍 USB 网卡的使用，USB 网卡推荐采用 AIR780(RNDIS)，EC20(ECM/RNDIS), 手机（RNDIS）,RTL8152 USB 网卡，AX88772 USB 网卡。

USB 网卡传输层面已经对接好了 LWIP 的收发接口，因此，用户只需要包含 **platform/XXX/usbh_lwip.c** 并根据需要开启对应的网卡类的宏即可。

- 当前支持以下网卡类：

.. code-block:: C

    // #define CONFIG_USBHOST_PLATFORM_CDC_ECM
    // #define CONFIG_USBHOST_PLATFORM_CDC_RNDIS
    // #define CONFIG_USBHOST_PLATFORM_CDC_NCM
    // #define CONFIG_USBHOST_PLATFORM_ASIX
    // #define CONFIG_USBHOST_PLATFORM_RTL8152
    // #define CONFIG_USBHOST_PLATFORM_BL616

- 包含了对接 LWIP 的输入输出接口，举例如下

.. code-block:: C

    static err_t usbh_cdc_ecm_linkoutput(struct netif *netif, struct pbuf *p)
    {
        int ret;
        (void)netif;

        usbh_lwip_eth_output_common(p, usbh_cdc_ecm_get_eth_txbuf());
        ret = usbh_cdc_ecm_eth_output(p->tot_len);
        if (ret < 0) {
            return ERR_BUF;
        } else {
            return ERR_OK;
        }
    }

    void usbh_cdc_ecm_eth_input(uint8_t *buf, uint32_t buflen)
    {
        usbh_lwip_eth_input_common(&g_cdc_ecm_netif, buf, buflen);
    }


- 网卡类枚举完成后，注册 netif，并且创建网卡接收线程（因此使用 RTTHREAD 时不需要使用 RTT 的接收线程模块）。
- 必须开启 DHCP client 服务，用于从 USB 网卡获取 IP 地址。

.. code-block:: C

    void usbh_cdc_ecm_run(struct usbh_cdc_ecm *cdc_ecm_class)
    {
        struct netif *netif = &g_cdc_ecm_netif;

        netif->hwaddr_len = 6;
        memcpy(netif->hwaddr, cdc_ecm_class->mac, 6);

        IP4_ADDR(&g_ipaddr, 0, 0, 0, 0);
        IP4_ADDR(&g_netmask, 0, 0, 0, 0);
        IP4_ADDR(&g_gateway, 0, 0, 0, 0);

        netif = netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, usbh_cdc_ecm_if_init, tcpip_input);
        netif_set_default(netif);
        while (!netif_is_up(netif)) {
        }

        dhcp_handle = usb_osal_timer_create("dhcp", 200, dhcp_timeout, netif, true);
        if (dhcp_handle == NULL) {
            USB_LOG_ERR("timer creation failed! \r\n");
            while (1) {
            }
        }

        usb_osal_thread_create("usbh_cdc_ecm_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_cdc_ecm_rx_thread, NULL);
    #if LWIP_DHCP
        dhcp_start(netif);
        usb_osal_timer_start(dhcp_handle);
    #endif
    }

- 获取到 IP 以后，就与 USB 没有关系了，直接使用 LWIP 的接口即可。

- 需要注意以下参数

LWIP_TCPIP_CORE_LOCKING_INPUT 用于不使用 lwip 内置的 tcpip 线程，而使用 USB 自己的处理线程。

LWIP_TCPIP_CORE_LOCKING 在现在 lwip 版本中默认是打开的，也推荐必须打开。

PBUF_POOL_BUFSIZE 推荐大于1600，搭配 LWIP_TCPIP_CORE_LOCKING_INPUT 使用，因为我们提供了使用 zero mempy 的方式，使用静态 pbuf，而不是把数据 copy 到 pbuf 中。

TCPIP_THREAD_STACKSIZE 推荐大于 1K，防止栈溢出。

.. code-block:: C

    #if LWIP_TCPIP_CORE_LOCKING_INPUT != 1
    #warning suggest you to set LWIP_TCPIP_CORE_LOCKING_INPUT to 1, usb handles eth input with own thread
    #endif

    #if LWIP_TCPIP_CORE_LOCKING != 1
    #error must set LWIP_TCPIP_CORE_LOCKING to 1
    #endif

    #if PBUF_POOL_BUFSIZE < 1600
    #error PBUF_POOL_BUFSIZE must be larger than 1600
    #endif

    #if TCPIP_THREAD_STACKSIZE < 1024
    #error TCPIP_THREAD_STACKSIZE must be >= 1024
    #endif
