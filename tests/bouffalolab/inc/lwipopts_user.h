/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

#ifdef USE_LWIPOPTS_APP_H
#include "lwipopts_app.h"
#endif

/**
 * SYS_LIGHTWEIGHT_PROT==1: if you want inter-task protection for certain
 * critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT 1
#define IP_REASSEMBLY        0
#define IP_FRAG              0
#define ARP_QUEUEING         0
#define NO_SYS               0
#define LWIP_RAND            rand

#define LWIP_NETIF_HOSTNAME  0
#define LWIP_TIMEVAL_PRIVATE 0
#define LWIP_TIMERS          1
#define LWIP_RAW             1
#define LWIP_IPV4            1
#define LWIP_IGMP            1
#define LWIP_ICMP            1
#define ICMP_TTL             64
#define LWIP_TCP             1
#define TCP_TTL              255
#define LWIP_UDP             1
#define UDP_TTL              255
#define LWIP_DNS             1

/**
 * LWIP_NETIF_API==1: Support netif api (in netifapi.c)
 */
#define LWIP_NETIF_API 1
/**
 * LWIP_NETCONN==1: Enable Netconn API (require to use api_lib.c)
 */
#define LWIP_NETCONN 1
/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 */
#define LWIP_SOCKET 1

/* ---------- Memory options ---------- */
#define MEMP_MEM_MALLOC 0

/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT 64
#endif

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#ifndef MEM_SIZE
#define MEM_SIZE (32 * 1024)
#endif

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#ifndef MEMP_NUM_PBUF
#define MEMP_NUM_PBUF 100
#endif

/**
 * MEMP_NUM_RAW_PCB: Number of raw connection PCBs
 * (requires the LWIP_RAW option)
 */
#ifndef MEMP_NUM_RAW_PCB
#define MEMP_NUM_RAW_PCB 4
#endif

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB 4
#endif

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB 4
#endif

/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB_LISTEN
#define MEMP_NUM_TCP_PCB_LISTEN 5
#endif

/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#ifndef MEMP_NUM_TCP_SEG
#define MEMP_NUM_TCP_SEG 40
#endif

/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#ifndef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT (LWIP_NUM_SYS_TIMEOUT_INTERNAL)
#endif

/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */
#ifndef MEMP_NUM_NETCONN
#define MEMP_NUM_NETCONN 4
#endif

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#ifndef PBUF_POOL_SIZE
#define PBUF_POOL_SIZE 20
#endif

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
#ifndef PBUF_POOL_BUFSIZE
#define PBUF_POOL_BUFSIZE 1600
#endif

/* ---------- TCP options ---------- */
/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ 0
#endif

/* TCP Maximum segment size. */
#ifndef TCP_MSS
#define TCP_MSS (1500 - 40) /* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */
#endif

/* TCP sender buffer space (bytes). */
#ifndef TCP_SND_BUF
#define TCP_SND_BUF (8 * TCP_MSS)
#endif

/*  TCP_SND_QUEUELEN: TCP sender buffer space (pbufs). This must be at least
  as much as (2 * TCP_SND_BUF/TCP_MSS) for things to work. */
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN (4 * TCP_SND_BUF / TCP_MSS)
#endif

/* TCP receive window. */
#ifndef TCP_WND
#define TCP_WND (16 * TCP_MSS)
#endif

/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */
#ifndef LWIP_DHCP
#define LWIP_DHCP 1
#endif

/* ---------- Statistics options ---------- */
#ifndef LWIP_STATS
#define LWIP_STATS 0
#endif

#ifndef LWIP_PROVIDE_ERRNO
#define LWIP_PROVIDE_ERRNO 1
#endif

/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/
/**
 * LWIP_SINGLE_NETIF==1: use a single netif only. This is the common case for
 * small real-life targets. Some code like routing etc. can be left out.
 */
#ifndef LWIP_SINGLE_NETIF
#define LWIP_SINGLE_NETIF 1
#endif

/* ---------- link callback options ---------- */
/* LWIP_NETIF_LINK_CALLBACK==1: Support a callback function from an interface
 * whenever the link changes (i.e., link down)
 */
#ifndef LWIP_NETIF_LINK_CALLBACK
#define LWIP_NETIF_LINK_CALLBACK 1
#endif

/**
 * LWIP_NETIF_TX_SINGLE_PBUF: if this is set to 1, lwIP *tries* to put all data
 * to be sent into one single pbuf. This is for compatibility with DMA-enabled
 * MACs that do not support scatter-gather.
 * Beware that this might involve CPU-memcpy before transmitting that would not
 * be needed without this flag! Use this only if you need to!
 *
 * ATTENTION: a driver should *NOT* rely on getting single pbufs but check TX
 * pbufs for being in one piece. If not, @ref pbuf_clone can be used to get
 * a single pbuf:
 *   if (p->next != NULL) {
 *     struct pbuf *q = pbuf_clone(PBUF_RAW, PBUF_RAM, p);
 *     if (q == NULL) {
 *       return ERR_MEM;
 *     }
 *     p = q; ATTENTION: do NOT free the old 'p' as the ref belongs to the caller!
 *   }
 */
#ifndef LWIP_NETIF_TX_SINGLE_PBUF
#define LWIP_NETIF_TX_SINGLE_PBUF 0
#endif

/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/*
 * Some MCUs allow computing and verifying the IP, UDP, TCP and ICMP checksums by hardware:
 * To use this feature let the following define uncommented.
 * To disable it and process by CPU comment the  the checksum.
*/
#ifdef CHECKSUM_BY_HARDWARE
/* CHECKSUM_GEN_IP==0: Generate checksums by hardware for outgoing IP packets.*/
#define CHECKSUM_GEN_IP 0
/* CHECKSUM_GEN_UDP==0: Generate checksums by hardware for outgoing UDP packets.*/
#define CHECKSUM_GEN_UDP 0
/* CHECKSUM_GEN_TCP==0: Generate checksums by hardware for outgoing TCP packets.*/
#define CHECKSUM_GEN_TCP 0
/* CHECKSUM_CHECK_IP==0: Check checksums by hardware for incoming IP packets.*/
#define CHECKSUM_CHECK_IP 0
/* CHECKSUM_CHECK_UDP==0: Check checksums by hardware for incoming UDP packets.*/
#define CHECKSUM_CHECK_UDP 0
/* CHECKSUM_CHECK_TCP==0: Check checksums by hardware for incoming TCP packets.*/
#define CHECKSUM_CHECK_TCP 0
/* CHECKSUM_CHECK_ICMP==0: Check checksums by hardware for incoming ICMP packets.*/
#define CHECKSUM_GEN_ICMP 0
#else
/* CHECKSUM_GEN_IP==1: Generate checksums in software for outgoing IP packets.*/
#define CHECKSUM_GEN_IP    1
/* CHECKSUM_GEN_UDP==1: Generate checksums in software for outgoing UDP packets.*/
#define CHECKSUM_GEN_UDP   1
/* CHECKSUM_GEN_TCP==1: Generate checksums in software for outgoing TCP packets.*/
#define CHECKSUM_GEN_TCP   1
/* CHECKSUM_CHECK_IP==1: Check checksums in software for incoming IP packets.*/
#define CHECKSUM_CHECK_IP  1
/* CHECKSUM_CHECK_UDP==1: Check checksums in software for incoming UDP packets.*/
#define CHECKSUM_CHECK_UDP 1
/* CHECKSUM_CHECK_TCP==1: Check checksums in software for incoming TCP packets.*/
#define CHECKSUM_CHECK_TCP 1
/* CHECKSUM_CHECK_ICMP==1: Check checksums by software for incoming ICMP packets.*/
#define CHECKSUM_GEN_ICMP  1
#endif

/*
   -----------------------------------
   ---------- DEBUG options ----------
   -----------------------------------
*/
#ifdef LWIP_DEBUG

#ifndef LWIP_DBG_MIN_LEVEL
#define LWIP_DBG_MIN_LEVEL 0
#endif

#ifndef PPP_DEBUG
#define PPP_DEBUG LWIP_DBG_OFF
#endif

#ifndef MEM_DEBUG
#define MEM_DEBUG LWIP_DBG_OFF
#endif

#ifndef MEMP_DEBUG
#define MEMP_DEBUG LWIP_DBG_OFF
#endif

#ifndef PBUF_DEBUG
#define PBUF_DEBUG LWIP_DBG_OFF
#endif

#ifndef API_LIB_DEBUG
#define API_LIB_DEBUG LWIP_DBG_OFF
#endif

#ifndef API_MSG_DEBUG
#define API_MSG_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCPIP_DEBUG
#define TCPIP_DEBUG LWIP_DBG_OFF
#endif

#ifndef NETIF_DEBUG
#define NETIF_DEBUG LWIP_DBG_OFF
#endif

#ifndef SOCKETS_DEBUG
#define SOCKETS_DEBUG LWIP_DBG_OFF
#endif

#ifndef DNS_DEBUG
#define DNS_DEBUG LWIP_DBG_OFF
#endif

#ifndef AUTOIP_DEBUG
#define AUTOIP_DEBUG LWIP_DBG_OFF
#endif

#ifndef DHCP_DEBUG
#define DHCP_DEBUG LWIP_DBG_OFF
#endif

#ifndef IP_DEBUG
#define IP_DEBUG LWIP_DBG_OFF
#endif

#ifndef IP_REASS_DEBUG
#define IP_REASS_DEBUG LWIP_DBG_OFF
#endif

#ifndef ICMP_DEBUG
#define ICMP_DEBUG LWIP_DBG_OFF
#endif

#ifndef IGMP_DEBUG
#define IGMP_DEBUG LWIP_DBG_OFF
#endif

#ifndef UDP_DEBUG
#define UDP_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_DEBUG
#define TCP_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_INPUT_DEBUG
#define TCP_INPUT_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_OUTPUT_DEBUG
#define TCP_OUTPUT_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_RTO_DEBUG
#define TCP_RTO_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_CWND_DEBUG
#define TCP_CWND_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_WND_DEBUG
#define TCP_WND_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_FR_DEBUG
#define TCP_FR_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_QLEN_DEBUG
#define TCP_QLEN_DEBUG LWIP_DBG_OFF
#endif

#ifndef TCP_RST_DEBUG
#define TCP_RST_DEBUG LWIP_DBG_OFF
#endif

#ifndef ETHARP_DEBUG
#define ETHARP_DEBUG LWIP_DBG_OFF
#endif

#endif

/*
   ---------------------------------
   ---------- OS options ----------
   ---------------------------------
*/
#ifndef TCPIP_THREAD_NAME
#define TCPIP_THREAD_NAME "tcpip"
#endif

#ifndef TCPIP_THREAD_STACKSIZE
#define TCPIP_THREAD_STACKSIZE 2048
#endif

#ifndef TCPIP_MBOX_SIZE
#define TCPIP_MBOX_SIZE 8
#endif

#ifndef DEFAULT_RAW_RECVMBOX_SIZE
#define DEFAULT_RAW_RECVMBOX_SIZE 50
#endif

#ifndef DEFAULT_UDP_RECVMBOX_SIZE
#define DEFAULT_UDP_RECVMBOX_SIZE 50
#endif

#ifndef DEFAULT_TCP_RECVMBOX_SIZE
#define DEFAULT_TCP_RECVMBOX_SIZE 50
#endif

#ifndef DEFAULT_ACCEPTMBOX_SIZE
#define DEFAULT_ACCEPTMBOX_SIZE 50
#endif

#ifndef DEFAULT_THREAD_STACKSIZE
#define DEFAULT_THREAD_STACKSIZE 500
#endif

#ifndef TCPIP_THREAD_PRIO
#define TCPIP_THREAD_PRIO 10
#endif

#define LWIP_COMPAT_MUTEX       0
#define LWIP_TCPIP_CORE_LOCKING 1

#ifndef LWIP_TCPIP_CORE_LOCKING_INPUT
#define LWIP_TCPIP_CORE_LOCKING_INPUT 1
#endif

#ifndef LWIP_MEM_SECTION
#define LWIP_MEM_SECTION ".bss"
#endif

// bouffalo patch
#define LWIP_DNS_SERVER          0
#define LWIP_SUPPORT_CUSTOM_PBUF 1

#define LWIP_DECLARE_MEMORY_ALIGNED(variable_name, size) u8_t variable_name[LWIP_MEM_ALIGN_BUFFER(size)] __attribute__((section(LWIP_MEM_SECTION)))

#endif /* __LWIPOPTS_H__ */
