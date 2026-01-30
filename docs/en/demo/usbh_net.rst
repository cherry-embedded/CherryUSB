Network Host
=================

This section mainly introduces the use of Host USB network cards. The following USB ne- Because the USB network card has been internally connected to LWIP, users can directly use LWIP APIs without worrying about USB implementation.

USB Network Card LWIP Configuration Macro Related Notes
-----------------------------------------------------------

**LWIP_TCPIP_CORE_LOCKING_INPUT** is used to not use lwip built-in tcpip thread, but use USB's own receive processing thread.

**LWIP_TCPIP_CORE_LOCKING** is enabled by default in current lwip versions, and it is also recommended to be mandatory.

**PBUF_POOL_BUFSIZE** is recommended to be greater than 1600, used with LWIP_TCPIP_CORE_LOCKING_INPUT, because we provide zero copy method using static pbuf instead of copying data into pbuf.

**TCPIP_THREAD_STACKSIZE** is recommended to be greater than 1K to prevent stack overflow.are currently supported and tested:

- 4G network cards: EC20(ECM/RNDIS), mobile phones (RNDIS), SIMCOM7600(RNDIS), ML307R(RNDIS), AIR780(RNDIS)

.. caution:: Please note that some 4G network cards do not have auto-dial functionality by default. Please replace the firmware or use AT commands to configure auto-dial, otherwise you cannot get an IP.

- USB Ethernet cards: ASIX AX88772, REALTEK RTL8152
- USB WIFI cards: Bouffalo Lab BL616 (RNDIS/ECM)

USB Network Card Related Macros and Files
--------------------------------------------------

The network card related macros are as follows, mainly used to register network card drivers according to different network components:

.. code-block:: C

    // #define CONFIG_USBHOST_PLATFORM_CDC_ECM
    // #define CONFIG_USBHOST_PLATFORM_CDC_RNDIS
    // #define CONFIG_USBHOST_PLATFORM_CDC_NCM
    // #define CONFIG_USBHOST_PLATFORM_ASIX
    // #define CONFIG_USBHOST_PLATFORM_RTL8152

.. note:: If Kconfig system is used, the above macros are automatically generated. For other platforms, please define manually.

USB network card transmission layer has been connected to relevant network components, listed as follows:

- Custom OS + LWIP please use **platform/lwip/usbh_lwip.c**, need to include this file yourself and enable the above relevant macros. Call `tcpip_init(NULL, NULL)` before initializing USB
- RT-THREAD + LWIP please use **platform/rtthread/usbh_lwip.c**, automatically select this file after enabling corresponding network card driver in Kconfig, automatically call `tcpip_init(NULL, NULL)` after selecting rt-thread lwip
- ESP-IDF + LWIP please use **platform/freertos/usbh_net.c**, automatically select this file after enabling corresponding network card driver in Kconfig, and call `esp_netif_init()` + `esp_event_loop_create_default()` before initializing USB
- NUTTX + NUTTX network component please use **platform/nuttx/usbh_net.c**, automatically select this file after enabling corresponding network card driver in Kconfig, automatically call after selecting network component

.. note:: If adding code yourself, don't forget to add USB network card driver related source files, such as **class/usbh_cdc_ecm.c**. So we recommend using with corresponding platforms to save the trouble of adding files yourself

USB Network Card Connection Process
---------------------------------------------

The following example shows the LWIP connection process.

- After USB network card enumeration is completed, the `usbh_xxx_run` function will be **automatically** called, at which time netif driver is registered, and DHCP client and IP acquisition timer are started.

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

- `usbh_lwip_eth_output_common` is used to assemble transmitted pbuf into USB network card data packets
- `usbh_lwip_eth_input_common` is used to assemble USB network card data into pbuf
- Actual network card transmission and reception processing

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

- After the USB network card is unplugged, the `usbh_xxx_stop` function will be **automatically** called, at which time you need to stop the DHCP client, delete the timer, and remove the netif.

.. code-block:: C

    void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class)
    {
        struct netif *netif = &g_cdc_ecm_netif;
        (void)cdc_ecm_class;

    #if LWIP_DHCP
        dhcp_stop(netif);
        dhcp_cleanup(netif);
        usb_osal_timer_delete(dhcp_handle);
    #endif
        netif_set_down(netif);
        netif_remove(netif);
    }

- Because the USB network card has been internally connected to LWIP, users can directly use LWIP APIs without worrying about USB implementation.

USB Network Card LWIP Configuration Macro Related Notes
--------------------------------------------------------------

**LWIP_TCPIP_CORE_LOCKING_INPUT** is used to not use lwip built-in tcpip thread, but use USB's own receive processing thread.

**LWIP_TCPIP_CORE_LOCKING** is enabled by default in current lwip versions, and it is also recommended to be mandatory.

**PBUF_POOL_BUFSIZE** is recommended to be greater than 1600, used with LWIP_TCPIP_CORE_LOCKING_INPUT, because we provide zero copy method using static pbuf instead of copying data into pbuf.

**TCPIP_THREAD_STACKSIZE** is recommended to be greater than 1K to prevent stack overflow.

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


Summary
--------------

.. note:: Through the above content, we can see that CherryUSB's support for USB network cards is very comprehensive. Users only need to enable corresponding macros or check options to achieve automatic recognition and driver registration of USB network cards, without manually initializing network card related configurations. Users only need to focus on the application layer, which greatly facilitates user usage.

For specific porting articles, please refer to developers' notes https://club.rt-thread.org/ask/article/5cf3e9e0b2d95800.html