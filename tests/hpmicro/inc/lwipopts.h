/*
 * Copyright (c) 2001-2003 Swedish Institute of Computer Science.
 * Copyright (c) 2023 HPMicro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Simon Goldschmidt
 *
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

#include <stdint.h>

#define SYS_LIGHTWEIGHT_PROT            0
#define LWIP_PROVIDE_ERRNO              1

#if defined(__SEGGER_RTL_VERSION)
#define LWIP_TIMEVAL_PRIVATE            1
#else
#define LWIP_TIMEVAL_PRIVATE            0
#endif

#define LWIP_RAND                       rand

#define NO_SYS                          0
#define MEM_ALIGNMENT                   4
#define LWIP_DNS                        1
#define LWIP_DNS_SERVER                 0
#define LWIP_RAW                        1
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1
#define LWIP_DHCP                       1
#define LWIP_ICMP                       1
#define ICMP_TTL                        64
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define TCP_TTL                         255
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define ETH_PAD_SIZE                    0
#define LWIP_IP_ACCEPT_UDP_PORT(p)      ((p) == PP_NTOHS(67))
#define LWIP_WND_SCALE                  1
#define TCP_RCV_SCALE                   0

#define MEM_SIZE                        (150 * 1024)
#define TCP_MSS                         (1500 /*mtu*/ - 20 /*iphdr*/ - 20 /*tcphhr*/)
#define TCP_SND_BUF                     (50 * TCP_MSS)
#define ETHARP_SUPPORT_STATIC_ENTRIES   1

#define LWIP_HTTPD_CGI                  0
#define LWIP_HTTPD_SSI                  0
#define LWIP_HTTPD_SSI_INCLUDE_TAG      0

/**
 * MEMP_MEM_MALLOC==1: Use mem_malloc/mem_free instead of the lwip pool allocator.
 */
#define MEMP_MEM_MALLOC         1

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
 *   sends a lot of data out of ROM (or other static memory), this
 *   should be set high.
 */
#define MEMP_NUM_PBUF           100

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
 * per active UDP "connection".
 */
#define MEMP_NUM_UDP_PCB        6

/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
 *   connections.
 */
#define MEMP_NUM_TCP_PCB        10

/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
 *  connections.
 */
#define MEMP_NUM_TCP_PCB_LISTEN 5

/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
 *  segments.
 */
#define MEMP_NUM_TCP_SEG        20

/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
 *  timeouts.
 */
#define MEMP_NUM_SYS_TIMEOUT    10


/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE          20

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool.*/
#define PBUF_POOL_BUFSIZE       1600

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory
*/
#define TCP_QUEUE_OOSEQ         1

/*  TCP_SND_QUEUELEN: TCP sender buffer space (pbufs). This must be at least
  as much as (2 * TCP_SND_BUF/TCP_MSS) for things to work.
*/
#define TCP_SND_QUEUELEN        (4* TCP_SND_BUF/TCP_MSS)

/* TCP receive window. */
#define TCP_WND                 (16*TCP_MSS)

/*
 *  -----------------------------------
 *  ---------- DEBUG options ----------
 *  -----------------------------------
 */

#define LWIP_DEBUG                      1

#ifdef LWIP_DEBUG

#define LWIP_DBG_MIN_LEVEL         1

#define PPP_DEBUG                  LWIP_DBG_OFF
#define MEM_DEBUG                  LWIP_DBG_OFF
#define MEMP_DEBUG                 LWIP_DBG_OFF
#define PBUF_DEBUG                 LWIP_DBG_OFF
#define API_LIB_DEBUG              LWIP_DBG_OFF
#define API_MSG_DEBUG              LWIP_DBG_OFF
#define TCPIP_DEBUG                LWIP_DBG_OFF
#define NETIF_DEBUG                LWIP_DBG_OFF
#define SOCKETS_DEBUG              LWIP_DBG_OFF
#define DNS_DEBUG                  LWIP_DBG_OFF
#define AUTOIP_DEBUG               LWIP_DBG_OFF
#define DHCP_DEBUG                 LWIP_DBG_OFF
#define IP_DEBUG                   LWIP_DBG_OFF
#define IP_REASS_DEBUG             LWIP_DBG_OFF
#define ICMP_DEBUG                 LWIP_DBG_OFF
#define IGMP_DEBUG                 LWIP_DBG_OFF
#define UDP_DEBUG                  LWIP_DBG_OFF
#define TCP_DEBUG                  LWIP_DBG_OFF
#define TCP_INPUT_DEBUG            LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG           LWIP_DBG_OFF
#define TCP_RTO_DEBUG              LWIP_DBG_OFF
#define TCP_CWND_DEBUG             LWIP_DBG_OFF
#define TCP_WND_DEBUG              LWIP_DBG_OFF
#define TCP_FR_DEBUG               LWIP_DBG_OFF
#define TCP_QLEN_DEBUG             LWIP_DBG_OFF
#define TCP_RST_DEBUG              LWIP_DBG_OFF
#define ETHARP_DEBUG               LWIP_DBG_OFF

#endif

#define DHCP_DOES_ARP_CHECK             0

/*
 *  ---------------------------------
 *  ---------- OS options ----------
 *  ---------------------------------
 */

#define TCPIP_THREAD_NAME              "tcpip"
#define TCPIP_THREAD_STACKSIZE          1500
#define TCPIP_MBOX_SIZE                 64
#define DEFAULT_RAW_RECVMBOX_SIZE       1000
#define DEFAULT_UDP_RECVMBOX_SIZE       100
#define DEFAULT_TCP_RECVMBOX_SIZE       100
#define DEFAULT_ACCEPTMBOX_SIZE         1500
#define DEFAULT_THREAD_STACKSIZE        500
#define TCPIP_THREAD_PRIO               31
#define LWIP_SINGLE_NETIF               1
#define LWIP_COMPAT_MUTEX               0

#define LWIP_TCPIP_CORE_LOCKING_INPUT 1
#define LWIP_TCPIP_CORE_LOCKING       1

#include <FreeRTOS.h>
#include <task.h>
#endif /* __LWIPOPTS_H__ */