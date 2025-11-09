/*
 * netutils: ping implementation
 */

#include <FreeRTOS.h>
#include <task.h>

#include <lwip/opt.h>
#include <lwip/init.h>
#include <lwip/mem.h>
#include <lwip/icmp.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/inet.h>
#include <lwip/inet_chksum.h>
#include <lwip/ip.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

/**
 * PING_DEBUG: Enable debugging for PING.
 */
#ifndef PING_DEBUG
#define PING_DEBUG LWIP_DBG_ON
#endif

/** ping receive timeout - in milliseconds */
#define PING_RCV_TIMEO (2000 * portTICK_PERIOD_MS)
/** ping delay - in milliseconds */
#define PING_DELAY     (1000 * portTICK_PERIOD_MS)

/** ping identifier - must fit on a u16_t */
#ifndef PING_ID
#define PING_ID 0xAFAF
#endif

/** ping additional data size to include in the packet */
#ifndef PING_DATA_SIZE
#define PING_DATA_SIZE 32
#endif

/* ping variables */
static u16_t ping_seq_num;
struct _ip_addr {
    uint8_t addr0, addr1, addr2, addr3;
};

/** Prepare a echo ICMP request */
static void ping_prepare_echo(struct icmp_echo_hdr *iecho, u16_t len)
{
    size_t i;
    size_t data_len = len - sizeof(struct icmp_echo_hdr);

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id = PING_ID;
    iecho->seqno = htons(++ping_seq_num);

    /* fill the additional data buffer with some data */
    for (i = 0; i < data_len; i++) {
        ((char *)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
    }

    iecho->chksum = inet_chksum(iecho, len);
}

/* Ping using the socket ip */
err_t lwip_ping_send(int s, ip_addr_t *addr, int size)
{
    int err;
    struct icmp_echo_hdr *iecho;
    struct sockaddr_in to;
    int ping_size = sizeof(struct icmp_echo_hdr) + size;
    LWIP_ASSERT("ping_size is too big", ping_size <= 0xffff);

    iecho = mem_malloc(ping_size);
    if (iecho == NULL) {
        return ERR_MEM;
    }

    ping_prepare_echo(iecho, (u16_t)ping_size);

    to.sin_len = sizeof(to);
    to.sin_family = AF_INET;
#if LWIP_IPV4 && LWIP_IPV6
    to.sin_addr.s_addr = addr->u_addr.ip4.addr;
#elif LWIP_IPV4
    to.sin_addr.s_addr = addr->addr;
#elif LWIP_IPV6
#error Not supported IPv6.
#endif

    err = lwip_sendto(s, iecho, ping_size, 0, (struct sockaddr *)&to, sizeof(to));
    mem_free(iecho);

    return (err == ping_size ? ERR_OK : ERR_VAL);
}

int lwip_ping_recv(int s, int *ttl)
{
    char buf[64];
    int fromlen = sizeof(struct sockaddr_in), len;
    struct sockaddr_in from;
    struct ip_hdr *iphdr;
    struct icmp_echo_hdr *iecho;

    while ((len = lwip_recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, (socklen_t *)&fromlen)) > 0) {
        if (len >= (int)(sizeof(struct ip_hdr) + sizeof(struct icmp_echo_hdr))) {
            iphdr = (struct ip_hdr *)buf;
            iecho = (struct icmp_echo_hdr *)(buf + (IPH_HL(iphdr) * 4));
            if ((iecho->id == PING_ID) && (iecho->seqno == htons(ping_seq_num))) {
                *ttl = iphdr->_ttl;
                return len;
            }
        }
    }

    return len;
}

/* using the lwIP custom ping */
uint32_t cmd_ping(char *target_name, uint16_t interval, uint16_t size, uint32_t count)
{
#if LWIP_VERSION_MAJOR >= 2U
    struct timeval timeout = { PING_RCV_TIMEO / (1000 * portTICK_PERIOD_MS), PING_RCV_TIMEO % (1000 * portTICK_PERIOD_MS) };
#else
    int timeout = PING_RCV_TIMEO * 1000UL / (1000 * portTICK_PERIOD_MS);
#endif

    int s, ttl = 0, recv_len;
    ip_addr_t target_addr;
    uint32_t send_times;
    uint32_t recv_start_tick;
    struct addrinfo hint, *res = NULL;
    struct sockaddr_in *h = NULL;
    struct in_addr ina;

    send_times = 0;
    ping_seq_num = 0;

    if (size == 0) {
        size = PING_DATA_SIZE;
    }

    memset(&hint, 0, sizeof(hint));
    /* convert URL to IP */
    if (lwip_getaddrinfo(target_name, NULL, &hint, &res) != 0) {
        printf("ping: unknown host %s\n\r", target_name);
        return -1;
    }
    memcpy(&h, &res->ai_addr, sizeof(struct sockaddr_in *));
    memcpy(&ina, &h->sin_addr, sizeof(ina));
    lwip_freeaddrinfo(res);
    if (inet_aton(inet_ntoa(ina), &target_addr) == 0) {
        printf("ping: unknown host %s\n\r", target_name);
        return -1;
    }
    /* new a socket */
    if ((s = lwip_socket(AF_INET, SOCK_RAW, IP_PROTO_ICMP)) < 0) {
        printf("ping: create socket failed\n\r");
        return -1;
    }

    lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        int elapsed_time;

        if (lwip_ping_send(s, &target_addr, size) == ERR_OK) {
            recv_start_tick = sys_now();
            if ((recv_len = lwip_ping_recv(s, &ttl)) >= 0) {
                elapsed_time = (sys_now() - recv_start_tick) * 1000UL / (1000 * portTICK_PERIOD_MS);
                printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n\r", recv_len, inet_ntoa(ina), send_times,
                       ttl, elapsed_time);
            } else {
                printf("From %s icmp_seq=%d timeout\n\r", inet_ntoa(ina), send_times);
            }
        } else {
            printf("Send %s - error\n\r", inet_ntoa(ina));
        }

        send_times++;
        if (send_times >= count) {
            /* send ping times reached, stop */
            break;
        }

        vTaskDelay(interval); /* take a delay */
    }

    lwip_close(s);

    return 0;
}

#include <shell.h>
#include "utils_getopt.h"

#define PING_USAGE \
"ping [-c count] [-i interval] [-s size] [-h help] destination\r\n" \
"\t\t-c count of ping requests. default is 4\r\n" \
"\t\t-i interval in ms. default is 1000\r\n" \
"\t\t-s ICMP payload size in bytes. default is 32\r\n" \
"\t\t-h print this help\r\n"

int ping(int argc, char **argv)
{
    int opt;
    getopt_env_t getopt_env;
    u16_t interval = PING_DELAY;
    u16_t data_size = PING_DATA_SIZE;
    u32_t total_count = 4;

    if (argc == 1) {
        goto usage;
    } else {
        utils_getopt_init(&getopt_env, 0);

        while ((opt = utils_getopt(&getopt_env, argc, argv, ":i:s:c:W:h")) != -1) {
            switch (opt) {
                case 'i':
                    interval = atoi(getopt_env.optarg);
                    break;
                case 's':
                    data_size = atoi(getopt_env.optarg);
                    break;
                case 'c':
                    total_count = atoi(getopt_env.optarg);
                    break;
                case 'h':
                    goto usage;
                case ':':
                    printf("%s: %c requires an argument\r\n", *argv, getopt_env.optopt);
                    goto usage;
                case '?':
                    printf("%s: unknown option %c\r\n", *argv, getopt_env.optopt);
                    goto usage;
            }
        }

        if (getopt_env.optind + 1 == argc) {
            cmd_ping(argv[getopt_env.optind], interval, data_size, total_count);
        } else {
            printf("Need target address\r\n");
            goto usage;
        }
    }

    return 0;

usage:
    printf("%s", PING_USAGE);
    return 0;
}
CSH_CMD_EXPORT(ping, ping network host);

