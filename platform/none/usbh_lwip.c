/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "netif/etharp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#include "usbh_core.h"

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
#error "TCPIP_THREAD_STACKSIZE must be >= 1024"
#endif

// #define CONFIG_USBHOST_PLATFORM_CDC_ECM
// #define CONFIG_USBHOST_PLATFORM_CDC_RNDIS
// #define CONFIG_USBHOST_PLATFORM_CDC_NCM
// #define CONFIG_USBHOST_PLATFORM_ASIX
// #define CONFIG_USBHOST_PLATFORM_RTL8152
// #define CONFIG_USBHOST_PLATFORM_BL616

ip_addr_t g_ipaddr;
ip_addr_t g_netmask;
ip_addr_t g_gateway;

void usbh_lwip_eth_output_common(struct pbuf *p, uint8_t *buf)
{
    struct pbuf *q;
    uint8_t *buffer;

    buffer = buf;
    for (q = p; q != NULL; q = q->next) {
        usb_memcpy(buffer, q->payload, q->len);
        buffer += q->len;
    }
}

void usbh_lwip_eth_input_common(struct netif *netif, uint8_t *buf, uint32_t len)
{
#if LWIP_TCPIP_CORE_LOCKING_INPUT
    pbuf_type type = PBUF_REF;
#else
    pbuf_type type = PBUF_POOL;
#endif
    err_t err;
    struct pbuf *p;

    p = pbuf_alloc(PBUF_RAW, len, type);
    if (p != NULL) {
#if LWIP_TCPIP_CORE_LOCKING_INPUT
        p->payload = buf;
#else
        usb_memcpy(p->payload, buf, len);
#endif
        err = netif->input(p, netif);
        if (err != ERR_OK) {
            pbuf_free(p);
        }
    } else {
        USB_LOG_ERR("No memory to alloc pbuf\r\n");
    }
}

TimerHandle_t dhcp_handle;

static void dhcp_timeout(TimerHandle_t xTimer)
{
    struct netif *netif = (struct netif *)pvTimerGetTimerID(xTimer);
#if LWIP_DHCP
    struct dhcp *dhcp;
#endif
    if (netif_is_up(netif)) {
#if LWIP_DHCP
        dhcp = netif_dhcp_data(netif);

        if (dhcp && (dhcp->state == DHCP_STATE_BOUND)) {
#endif
            USB_LOG_INFO("IPv4 Address     : %s\r\n", ipaddr_ntoa(&netif->ip_addr));
            USB_LOG_INFO("IPv4 Subnet mask : %s\r\n", ipaddr_ntoa(&netif->netmask));
            USB_LOG_INFO("IPv4 Gateway     : %s\r\n\r\n", ipaddr_ntoa(&netif->gw));

            xTimerStop(xTimer, 0);
#if LWIP_DHCP
        }
#endif
    } else {
    }
}

#ifdef CONFIG_USBHOST_PLATFORM_CDC_ECM
#include "usbh_cdc_ecm.h"

struct netif g_cdc_ecm_netif;

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

static err_t usbh_cdc_ecm_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_cdc_ecm_linkoutput;
    return ERR_OK;
}

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

    dhcp_handle = xTimerCreate((const char *)"dhcp", (TickType_t)200, (UBaseType_t)pdTRUE, (void *const)netif, (TimerCallbackFunction_t)dhcp_timeout);
    if (dhcp_handle == NULL) {
        USB_LOG_ERR("timer creation failed! \r\n");
        while (1) {
        }
    }

    usb_osal_thread_create("usbh_cdc_ecm_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_cdc_ecm_rx_thread, NULL);
#if LWIP_DHCP
    dhcp_start(netif);
    xTimerStart(dhcp_handle, 0);
#endif
}

void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class)
{
    struct netif *netif = &g_cdc_ecm_netif;
    (void)cdc_ecm_class;

#if LWIP_DHCP
    dhcp_stop(netif);
    dhcp_cleanup(netif);
    xTimerStop(dhcp_handle, 0);
    xTimerDelete(dhcp_handle, 0);
#endif
    netif_set_down(netif);
    netif_remove(netif);
}
#endif

#ifdef CONFIG_USBHOST_PLATFORM_CDC_RNDIS
#include "usbh_rndis.h"

TimerHandle_t timer_handle;

static void rndis_dev_keepalive_timeout(TimerHandle_t xTimer)
{
    struct usbh_rndis *rndis_class = (struct usbh_rndis *)pvTimerGetTimerID(xTimer);
    usbh_rndis_keepalive(rndis_class);
}

void timer_init(struct usbh_rndis *rndis_class)
{
    timer_handle = xTimerCreate((const char *)NULL, (TickType_t)5000, (UBaseType_t)pdTRUE, (void *const)rndis_class, (TimerCallbackFunction_t)rndis_dev_keepalive_timeout);
    if (NULL != timer_handle) {
        xTimerStart(timer_handle, 0);
    } else {
        USB_LOG_ERR("timer creation failed! \r\n");
        for (;;) {
            ;
        }
    }
}

struct netif g_rndis_netif;

static err_t usbh_rndis_linkoutput(struct netif *netif, struct pbuf *p)
{
    int ret;
    (void)netif;

    usbh_lwip_eth_output_common(p, usbh_rndis_get_eth_txbuf());
    ret = usbh_rndis_eth_output(p->tot_len);
    if (ret < 0) {
        return ERR_BUF;
    } else {
        return ERR_OK;
    }
}

void usbh_rndis_eth_input(uint8_t *buf, uint32_t buflen)
{
    usbh_lwip_eth_input_common(&g_rndis_netif, buf, buflen);
}

static err_t usbh_rndis_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_rndis_linkoutput;
    return ERR_OK;
}

void usbh_rndis_run(struct usbh_rndis *rndis_class)
{
    struct netif *netif = &g_rndis_netif;

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, rndis_class->mac, 6);

    IP4_ADDR(&g_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&g_netmask, 0, 0, 0, 0);
    IP4_ADDR(&g_gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, usbh_rndis_if_init, tcpip_input);
    netif_set_default(netif);
    while (!netif_is_up(netif)) {
    }

    dhcp_handle = xTimerCreate((const char *)"dhcp", (TickType_t)200, (UBaseType_t)pdTRUE, (void *const)netif, (TimerCallbackFunction_t)dhcp_timeout);
    if (dhcp_handle == NULL) {
        USB_LOG_ERR("timer creation failed! \r\n");
        while (1) {
        }
    }

    usb_osal_thread_create("usbh_rndis_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_rndis_rx_thread, NULL);

    //timer_init(rndis_class);

#if LWIP_DHCP
    dhcp_start(netif);
    xTimerStart(dhcp_handle, 0);
#endif
}

void usbh_rndis_stop(struct usbh_rndis *rndis_class)
{
    struct netif *netif = &g_rndis_netif;
    (void)rndis_class;

#if LWIP_DHCP
    dhcp_stop(netif);
    dhcp_cleanup(netif);
    xTimerStop(dhcp_handle, 0);
    xTimerDelete(dhcp_handle, 0);
#endif
    netif_set_down(netif);
    netif_remove(netif);
    // xTimerStop(timer_handle, 0);
    // xTimerDelete(timer_handle, 0);
}
#endif

#ifdef CONFIG_USBHOST_PLATFORM_CDC_NCM
#include "usbh_cdc_ncm.h"

struct netif g_cdc_ncm_netif;

static err_t usbh_cdc_ncm_linkoutput(struct netif *netif, struct pbuf *p)
{
    int ret;
    (void)netif;

    usbh_lwip_eth_output_common(p, usbh_cdc_ncm_get_eth_txbuf());
    ret = usbh_cdc_ncm_eth_output(p->tot_len);
    if (ret < 0) {
        return ERR_BUF;
    } else {
        return ERR_OK;
    }
}

void usbh_cdc_ncm_eth_input(uint8_t *buf, uint32_t buflen)
{
    usbh_lwip_eth_input_common(&g_cdc_ncm_netif, buf, buflen);
}

static err_t usbh_cdc_ncm_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_cdc_ncm_linkoutput;
    return ERR_OK;
}

void usbh_cdc_ncm_run(struct usbh_cdc_ncm *cdc_ncm_class)
{
    struct netif *netif = &g_cdc_ncm_netif;

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, cdc_ncm_class->mac, 6);

    IP4_ADDR(&g_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&g_netmask, 0, 0, 0, 0);
    IP4_ADDR(&g_gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, usbh_cdc_ncm_if_init, tcpip_input);
    netif_set_default(netif);
    while (!netif_is_up(netif)) {
    }

    dhcp_handle = xTimerCreate((const char *)"dhcp", (TickType_t)200, (UBaseType_t)pdTRUE, (void *const)netif, (TimerCallbackFunction_t)dhcp_timeout);
    if (dhcp_handle == NULL) {
        USB_LOG_ERR("timer creation failed! \r\n");
        while (1) {
        }
    }

    usb_osal_thread_create("usbh_cdc_ncm_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_cdc_ncm_rx_thread, NULL);
#if LWIP_DHCP
    dhcp_start(netif);
    xTimerStart(dhcp_handle, 0);
#endif
}

void usbh_cdc_ncm_stop(struct usbh_cdc_ncm *cdc_ncm_class)
{
    struct netif *netif = &g_cdc_ncm_netif;
    (void)cdc_ncm_class;

#if LWIP_DHCP
    dhcp_stop(netif);
    dhcp_cleanup(netif);
    xTimerStop(dhcp_handle, 0);
    xTimerDelete(dhcp_handle, 0);
#endif
    netif_set_down(netif);
    netif_remove(netif);
}
#endif

#ifdef CONFIG_USBHOST_PLATFORM_ASIX
#include "usbh_asix.h"

struct netif g_asix_netif;

static err_t usbh_asix_linkoutput(struct netif *netif, struct pbuf *p)
{
    int ret;
    (void)netif;

    usbh_lwip_eth_output_common(p, usbh_asix_get_eth_txbuf());
    ret = usbh_asix_eth_output(p->tot_len);
    if (ret < 0) {
        return ERR_BUF;
    } else {
        return ERR_OK;
    }
}

void usbh_asix_eth_input(uint8_t *buf, uint32_t buflen)
{
    usbh_lwip_eth_input_common(&g_asix_netif, buf, buflen);
}

static err_t usbh_asix_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_asix_linkoutput;
    return ERR_OK;
}

void usbh_asix_run(struct usbh_asix *asix_class)
{
    struct netif *netif = &g_asix_netif;

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, asix_class->mac, 6);

    IP4_ADDR(&g_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&g_netmask, 0, 0, 0, 0);
    IP4_ADDR(&g_gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, usbh_asix_if_init, tcpip_input);
    netif_set_default(netif);
    while (!netif_is_up(netif)) {
    }

    dhcp_handle = xTimerCreate((const char *)"dhcp", (TickType_t)200, (UBaseType_t)pdTRUE, (void *const)netif, (TimerCallbackFunction_t)dhcp_timeout);
    if (dhcp_handle == NULL) {
        USB_LOG_ERR("timer creation failed! \r\n");
        while (1) {
        }
    }

    usb_osal_thread_create("usbh_asix_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_asix_rx_thread, NULL);
#if LWIP_DHCP
    dhcp_start(netif);
    xTimerStart(dhcp_handle, 0);
#endif
}

void usbh_asix_stop(struct usbh_asix *asix_class)
{
    struct netif *netif = &g_asix_netif;
    (void)asix_class;

#if LWIP_DHCP
    dhcp_stop(netif);
    dhcp_cleanup(netif);
    xTimerStop(dhcp_handle, 0);
    xTimerDelete(dhcp_handle, 0);
#endif
    netif_set_down(netif);
    netif_remove(netif);
}
#endif

#ifdef CONFIG_USBHOST_PLATFORM_RTL8152
#include "usbh_rtl8152.h"

struct netif g_rtl8152_netif;

static err_t usbh_rtl8152_linkoutput(struct netif *netif, struct pbuf *p)
{
    int ret;
    (void)netif;

    usbh_lwip_eth_output_common(p, usbh_rtl8152_get_eth_txbuf());
    ret = usbh_rtl8152_eth_output(p->tot_len);
    if (ret < 0) {
        return ERR_BUF;
    } else {
        return ERR_OK;
    }
}

void usbh_rtl8152_eth_input(uint8_t *buf, uint32_t buflen)
{
    usbh_lwip_eth_input_common(&g_rtl8152_netif, buf, buflen);
}

static err_t usbh_rtl8152_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_rtl8152_linkoutput;
    return ERR_OK;
}

void usbh_rtl8152_run(struct usbh_rtl8152 *rtl8152_class)
{
    struct netif *netif = &g_rtl8152_netif;

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, rtl8152_class->mac, 6);

    IP4_ADDR(&g_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&g_netmask, 0, 0, 0, 0);
    IP4_ADDR(&g_gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, usbh_rtl8152_if_init, tcpip_input);
    netif_set_default(netif);
    while (!netif_is_up(netif)) {
    }

    dhcp_handle = xTimerCreate((const char *)"dhcp", (TickType_t)200, (UBaseType_t)pdTRUE, (void *const)netif, (TimerCallbackFunction_t)dhcp_timeout);
    if (dhcp_handle == NULL) {
        USB_LOG_ERR("timer creation failed! \r\n");
        while (1) {
        }
    }

    usb_osal_thread_create("usbh_rtl8152_rx", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_rtl8152_rx_thread, NULL);
#if LWIP_DHCP
    dhcp_start(netif);
    xTimerStart(dhcp_handle, 0);
#endif
}

void usbh_rtl8152_stop(struct usbh_rtl8152 *rtl8152_class)
{
    struct netif *netif = &g_rtl8152_netif;
    (void)rtl8152_class;

#if LWIP_DHCP
    dhcp_stop(netif);
    dhcp_cleanup(netif);
    xTimerStop(dhcp_handle, 0);
    xTimerDelete(dhcp_handle, 0);
#endif
    netif_set_down(netif);
    netif_remove(netif);
}
#endif

#ifdef CONFIG_USBHOST_PLATFORM_BL616
#include "usbh_bl616.h"

struct netif g_bl616_netif;
static err_t usbh_bl616_linkoutput(struct netif *netif, struct pbuf *p)
{
    int ret;
    (void)netif;

    usbh_lwip_eth_output_common(p, usbh_bl616_get_eth_txbuf());
    ret = usbh_bl616_eth_output(p->tot_len);
    if (ret < 0) {
        return ERR_BUF;
    } else {
        return ERR_OK;
    }
}

void usbh_bl616_eth_input(uint8_t *buf, uint32_t buflen)
{
    usbh_lwip_eth_input_common(&g_bl616_netif, buf, buflen);
}

static err_t usbh_bl616_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_bl616_linkoutput;
    return ERR_OK;
}

void usbh_bl616_sta_connect_callback(void)
{
}

void usbh_bl616_sta_disconnect_callback(void)
{
    struct netif *netif = &g_bl616_netif;

    netif_set_down(netif);
}

void usbh_bl616_sta_update_ip(uint8_t ip4_addr[4], uint8_t ip4_mask[4], uint8_t ip4_gw[4])
{
    struct netif *netif = &g_bl616_netif;

    IP4_ADDR(&netif->ip_addr, ip4_addr[0], ip4_addr[1], ip4_addr[2], ip4_addr[3]);
    IP4_ADDR(&netif->netmask, ip4_mask[0], ip4_mask[1], ip4_mask[2], ip4_mask[3]);
    IP4_ADDR(&netif->gw, ip4_gw[0], ip4_gw[1], ip4_gw[2], ip4_gw[3]);

    netif_set_up(netif);
}

void usbh_bl616_run(struct usbh_bl616 *bl616_class)
{
    struct netif *netif = &g_bl616_netif;

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, bl616_class->sta_mac, 6);

    IP4_ADDR(&g_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&g_netmask, 0, 0, 0, 0);
    IP4_ADDR(&g_gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &g_ipaddr, &g_netmask, &g_gateway, NULL, usbh_bl616_if_init, tcpip_input);
    netif_set_down(netif);
    netif_set_default(netif);

    dhcp_handle = xTimerCreate((const char *)"dhcp", (TickType_t)200, (UBaseType_t)pdTRUE, (void *const)netif, (TimerCallbackFunction_t)dhcp_timeout);
    if (dhcp_handle == NULL) {
        USB_LOG_ERR("timer creation failed! \r\n");
        while (1) {
        }
    }
    xTimerStart(dhcp_handle, 0);

    usb_osal_thread_create("usbh_bl616", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_bl616_rx_thread, NULL);
}

void usbh_bl616_stop(struct usbh_bl616 *bl616_class)
{
    struct netif *netif = &g_bl616_netif;

    (void)bl616_class;

    netif_set_down(netif);
    netif_remove(netif);
}

// #include "shell.h"

// CSH_CMD_EXPORT(wifi_sta_connect, );
// CSH_CMD_EXPORT(wifi_scan, );
#endif