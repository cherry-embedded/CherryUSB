#include "rtthread.h"

#ifdef PKG_CHERRYUSB_HOST

#ifndef RT_USING_TIMER_SOFT
#error must enable RT_USING_TIMER_SOFT to support timer callback in thread
#endif

#if IDLE_THREAD_STACK_SIZE < 2048
#error "IDLE_THREAD_STACK_SIZE must be greater than 2048"
#endif

#if RT_TIMER_THREAD_STACK_SIZE < 2048
#error "RT_TIMER_THREAD_STACK_SIZE must be greater than 2048"
#endif

#ifdef RT_USING_LWIP

#include "lwip/opt.h"

#ifndef RT_USING_LWIP212
#error must enable RT_USING_LWIP212
#endif

#ifndef LWIP_NO_RX_THREAD
#error must enable LWIP_NO_RX_THREAD, we do not use rtthread eth rx thread
#endif

#if LWIP_TCPIP_CORE_LOCKING_INPUT !=1
#warning suggest you to set LWIP_TCPIP_CORE_LOCKING_INPUT to 1, usb handles eth input with own thread
#endif

#if LWIP_TCPIP_CORE_LOCKING !=1
#error must set LWIP_TCPIP_CORE_LOCKING to 1
#endif

#if PBUF_POOL_BUFSIZE < 1514
#error PBUF_POOL_BUFSIZE must be larger than 1514
#endif

#endif

#endif
