/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_RNDIS_H
#define USBD_RNDIS_H

#include "usb_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init rndis interface driver */
struct usbd_interface *usbd_rndis_init_intf(uint8_t busid,
                                            struct usbd_interface *intf,
                                            const uint8_t out_ep,
                                            const uint8_t in_ep,
                                            const uint8_t int_ep, uint8_t mac[6]);

void usbd_rndis_data_recv_done(uint8_t busid);

#ifdef CONFIG_USBDEV_RNDIS_USING_LWIP
struct pbuf *usbd_rndis_eth_rx(uint8_t busid);
int usbd_rndis_eth_tx(uint8_t busid, struct pbuf *p);
#endif

#ifdef __cplusplus
}
#endif

#endif /* USBD_RNDIS_H */
