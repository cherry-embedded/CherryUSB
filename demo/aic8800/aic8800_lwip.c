/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * lwIP netif glue for the AIC8800 full-MAC data path.
 */
#include "aic8800_lwip.h"
#include "aic8800_wifi_config.h"
#include "aic8800_wifi_priv.h"

#if defined(AIC8800_WIFI_DEMO_IPERF) && AIC8800_WIFI_DEMO_IPERF
#include "aic8800_iperf.h"
#else
static inline void aic8800_iperf_stop(void)
{
}
#endif

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/inet_chksum.h"
#include "lwip/netif.h"
#include "lwip/prot/icmp.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"

#include "usb_config.h"
#include "usb_osal.h"
#include "usb_log.h"

#ifndef AIC8800_LWIP_OWN_TCPIP
#define AIC8800_LWIP_OWN_TCPIP 1
#endif

#define AIC8800_LWIP_MTU 1500U
#define AIC8800_PING_COUNT 4U
#define AIC8800_PING_PAYLOAD_SIZE 32U
#define AIC8800_PING_TIMEOUT_MS 2000U
#define AIC8800_PING_INTERVAL_MS 1000U
#define AIC8800_PING_IDENTIFIER 0xa1c8U

static struct netif g_aic8800_netif;
static volatile bool g_tcpip_ready;
static bool g_tcpip_started;
static bool g_netif_added;
static bool g_ping_task_running;
static ip4_addr_t g_ping_gateway;

static void aic8800_ping_task(void *argument)
{
    uint8_t request[sizeof(struct icmp_echo_hdr) + AIC8800_PING_PAYLOAD_SIZE];
    uint8_t response[96];
    struct sockaddr_in destination;
    struct timeval timeout;
    unsigned int sequence;
    int socket_fd;

    (void)argument;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;
    destination.sin_addr.s_addr = g_ping_gateway.addr;
    socket_fd = lwip_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (socket_fd < 0) {
        USB_LOG_ERR("AIC ping could not create raw socket\r\n");
        g_ping_task_running = false;
        vTaskDelete(NULL);
        return;
    }
    USB_LOG_INFO("AIC ping gateway %u.%u.%u.%u, %u packets\r\n",
                 ip4_addr1(&g_ping_gateway), ip4_addr2(&g_ping_gateway),
                 ip4_addr3(&g_ping_gateway), ip4_addr4(&g_ping_gateway),
                 AIC8800_PING_COUNT);

    for (sequence = 1U; sequence <= AIC8800_PING_COUNT; sequence++) {
        struct icmp_echo_hdr *echo = (struct icmp_echo_hdr *)request;
        uint32_t started;
        unsigned int index;
        int sent;
        int received;
        bool matched = false;

        memset(request, 0, sizeof(request));
        ICMPH_TYPE_SET(echo, ICMP_ECHO);
        ICMPH_CODE_SET(echo, 0U);
        echo->id = lwip_htons(AIC8800_PING_IDENTIFIER);
        echo->seqno = lwip_htons((uint16_t)sequence);
        for (index = sizeof(*echo); index < sizeof(request); index++) {
            request[index] = (uint8_t)(index + sequence);
        }
        echo->chksum = inet_chksum(request, sizeof(request));
        started = sys_now();
        sent = lwip_sendto(socket_fd, request, sizeof(request), 0,
                           (const struct sockaddr *)&destination,
                           sizeof(destination));
        if (sent == (int)sizeof(request)) {
            do {
                uint32_t elapsed = sys_now() - started;
                uint32_t remaining;

                if (elapsed >= AIC8800_PING_TIMEOUT_MS) break;
                remaining = AIC8800_PING_TIMEOUT_MS - elapsed;
                timeout.tv_sec = remaining / 1000U;
                timeout.tv_usec = (remaining % 1000U) * 1000U;
                (void)lwip_setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO,
                                      &timeout, sizeof(timeout));
                received = lwip_recvfrom(socket_fd, response,
                                         sizeof(response), 0, NULL, NULL);
                if (received < 0) break;
                if (received >= (int)(20U + sizeof(*echo))) {
                    uint32_t ip_header_length = (response[0] & 0x0fU) * 4U;

                    if ((ip_header_length >= 20U) &&
                        ((uint32_t)received >=
                         ip_header_length + sizeof(*echo))) {
                        const struct icmp_echo_hdr *reply =
                            (const struct icmp_echo_hdr *)
                            (response + ip_header_length);

                        matched =
                            (reply->type == ICMP_ER) &&
                            (reply->id ==
                             lwip_htons(AIC8800_PING_IDENTIFIER)) &&
                            (reply->seqno ==
                             lwip_htons((uint16_t)sequence));
                    }
                }
            } while (!matched);
        }
        if (matched) {
            USB_LOG_INFO("AIC ping reply seq=%u time=%u ms\r\n", sequence,
                         (unsigned int)(sys_now() - started));
        } else {
            USB_LOG_WRN("AIC ping timeout seq=%u\r\n", sequence);
        }
        if (sequence < AIC8800_PING_COUNT) {
            vTaskDelay(pdMS_TO_TICKS(AIC8800_PING_INTERVAL_MS));
        }
    }
    lwip_close(socket_fd);
    USB_LOG_INFO("AIC gateway ping test finished\r\n");
    g_ping_task_running = false;
    vTaskDelete(NULL);
}

static void aic8800_netif_status(struct netif *netif)
{
    const ip4_addr_t *address = netif_ip4_addr(netif);

    if (netif_is_up(netif) && !ip4_addr_isany_val(*address)) {
        const ip4_addr_t *gateway = netif_ip4_gw(netif);

        USB_LOG_INFO("AIC DHCP address %u.%u.%u.%u gateway %u.%u.%u.%u\r\n",
                     ip4_addr1(address), ip4_addr2(address),
                     ip4_addr3(address), ip4_addr4(address),
                     ip4_addr1(gateway), ip4_addr2(gateway),
                     ip4_addr3(gateway), ip4_addr4(gateway));
        aic8800_wifi_notify(AIC8800_WIFI_EVENT_GOT_IP);
    }
}

static void aic8800_tcpip_ready(void *argument)
{
    (void)argument;
    g_tcpip_ready = true;
}

static err_t aic8800_netif_link_output(struct netif *netif,
                                       struct pbuf *packet)
{
    (void)netif;
    return aic8800_lwip_transmit(packet);
}

static err_t aic8800_netif_init(struct netif *netif)
{
    netif->name[0] = 'w';
    netif->name[1] = 'l';
    netif->output = etharp_output;
    netif->linkoutput = aic8800_netif_link_output;
    netif->mtu = AIC8800_LWIP_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
                   NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;
    return ERR_OK;
}

__WEAK err_t aic8800_lwip_transmit(const struct pbuf *packet)
{
    (void)packet;
    return ERR_IF;
}

err_t aic8800_lwip_init(const uint8_t mac_address[6])
{
    ip4_addr_t address;
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    struct netif *result;

    if (mac_address == NULL) {
        return ERR_ARG;
    }
    if (g_netif_added) {
        return ERR_ALREADY;
    }

    if (!g_tcpip_started) {
#if AIC8800_LWIP_OWN_TCPIP
        g_tcpip_ready = false;
        tcpip_init(aic8800_tcpip_ready, NULL);
        g_tcpip_started = true;
        while (!g_tcpip_ready) {
            usb_osal_msleep(1U);
        }
#else
        /* Application already called tcpip_init(). */
        g_tcpip_started = true;
        g_tcpip_ready = true;
#endif
    }

    memset(&g_aic8800_netif, 0, sizeof(g_aic8800_netif));
    g_aic8800_netif.hwaddr_len = ETH_HWADDR_LEN;
    memcpy(g_aic8800_netif.hwaddr, mac_address, ETH_HWADDR_LEN);
    ip4_addr_set_zero(&address);
    ip4_addr_set_zero(&netmask);
    ip4_addr_set_zero(&gateway);

    LOCK_TCPIP_CORE();
    result = netif_add(&g_aic8800_netif, &address, &netmask, &gateway, NULL,
                       aic8800_netif_init, tcpip_input);
    if (result != NULL) {
        netif_set_status_callback(result, aic8800_netif_status);
        netif_set_default(result);
        netif_set_up(result);
        g_netif_added = true;
    }
    UNLOCK_TCPIP_CORE();

    if (result == NULL) {
        return ERR_IF;
    }
    USB_LOG_INFO("AIC lwIP netif ready, MAC %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 mac_address[0], mac_address[1], mac_address[2],
                 mac_address[3], mac_address[4], mac_address[5]);
    return ERR_OK;
}

struct netif *aic8800_lwip_get_netif(void)
{
    return g_netif_added ? &g_aic8800_netif : NULL;
}

void aic8800_lwip_start_gateway_ping(void)
{
    const ip4_addr_t *gateway;

    if (!g_netif_added || g_ping_task_running) {
        return;
    }
    gateway = netif_ip4_gw(&g_aic8800_netif);
    if (ip4_addr_isany_val(*gateway)) {
        return;
    }
    g_ping_gateway = *gateway;
    g_ping_task_running = true;
    if (xTaskCreate(aic8800_ping_task, "aic_ping", 512U, NULL,
                    tskIDLE_PRIORITY + 2U, NULL) != pdPASS) {
        g_ping_task_running = false;
        USB_LOG_ERR("AIC ping task creation failed\r\n");
    }
}

void aic8800_lwip_deinit(void)
{
    if (!g_netif_added) {
        return;
    }
    LOCK_TCPIP_CORE();
    aic8800_iperf_stop();
    dhcp_stop(&g_aic8800_netif);
    netif_set_link_down(&g_aic8800_netif);
    netif_set_down(&g_aic8800_netif);
    netif_remove(&g_aic8800_netif);
    g_netif_added = false;
    UNLOCK_TCPIP_CORE();
}

void aic8800_lwip_set_link(bool connected)
{
    ip4_addr_t zero;

    if (!g_netif_added) {
        return;
    }
    ip4_addr_set_zero(&zero);
    LOCK_TCPIP_CORE();
    if (connected) {
        netif_set_link_up(&g_aic8800_netif);
        if (dhcp_start(&g_aic8800_netif) != ERR_OK) {
            USB_LOG_ERR("AIC DHCP client failed to start\r\n");
        } else {
            USB_LOG_INFO("AIC DHCP client started\r\n");
        }
    } else {
        aic8800_iperf_stop();
        dhcp_stop(&g_aic8800_netif);
        netif_set_link_down(&g_aic8800_netif);
        netif_set_addr(&g_aic8800_netif, &zero, &zero, &zero);
    }
    UNLOCK_TCPIP_CORE();
}

err_t aic8800_lwip_input(const void *frame, uint16_t length)
{
    struct pbuf *packet;
    err_t result;

    if (!g_netif_added || (frame == NULL) ||
        (length < SIZEOF_ETH_HDR) ||
        (length > AIC8800_LWIP_MTU + SIZEOF_ETH_HDR)) {
        return ERR_ARG;
    }
    packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (packet == NULL) {
        return ERR_MEM;
    }
    result = pbuf_take(packet, frame, length);
    if (result == ERR_OK) {
        result = g_aic8800_netif.input(packet, &g_aic8800_netif);
    }
    if (result != ERR_OK) {
        pbuf_free(packet);
    }
    return result;
}

void aic8800_lwip_lock(void)
{
    LOCK_TCPIP_CORE();
}

void aic8800_lwip_unlock(void)
{
    UNLOCK_TCPIP_CORE();
}

struct pbuf *aic8800_lwip_alloc_frame(uint16_t length)
{
    struct pbuf *packet;

    if (!g_netif_added || (length < SIZEOF_ETH_HDR) ||
        (length > AIC8800_LWIP_MTU + SIZEOF_ETH_HDR)) {
        return NULL;
    }
    packet = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if ((packet != NULL) &&
        ((packet->payload == NULL) || (packet->len < length))) {
        pbuf_free(packet);
        return NULL;
    }
    return packet;
}

err_t aic8800_lwip_inject_locked(struct pbuf *packet)
{
    err_t result;

    LWIP_ASSERT_CORE_LOCKED();
    if (!g_netif_added || (packet == NULL)) {
        if (packet != NULL) {
            pbuf_free(packet);
        }
        return ERR_ARG;
    }
    result = ethernet_input(packet, &g_aic8800_netif);
    if (result != ERR_OK) {
        pbuf_free(packet);
    }
    return result;
}
