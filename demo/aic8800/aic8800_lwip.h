/*
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef AIC8800_LWIP_H
#define AIC8800_LWIP_H

#include <stdbool.h>
#include <stdint.h>

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

err_t aic8800_lwip_init(const uint8_t mac_address[6]);
void aic8800_lwip_deinit(void);
void aic8800_lwip_set_link(bool connected);
struct netif *aic8800_lwip_get_netif(void);
void aic8800_lwip_start_gateway_ping(void);
err_t aic8800_lwip_input(const void *frame, uint16_t length);
void aic8800_lwip_lock(void);
void aic8800_lwip_unlock(void);
struct pbuf *aic8800_lwip_alloc_frame(uint16_t length);
err_t aic8800_lwip_inject_locked(struct pbuf *packet);

/* The pbuf chain is valid only for this synchronous call. */
err_t aic8800_lwip_transmit(const struct pbuf *packet);

#ifdef __cplusplus
}
#endif

#endif /* AIC8800_LWIP_H */
