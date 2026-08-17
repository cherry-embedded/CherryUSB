/*
 * CherryUSB CDC NCM device class glue
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_CDC_NCM_H
#define USBD_CDC_NCM_H

#include <stdint.h>
#include <stdbool.h>
#include "usb_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_interface *usbd_cdc_ncm_init_intf(struct usbd_interface *intf,
                                               const uint8_t int_ep,
                                               const uint8_t out_ep,
                                               const uint8_t in_ep,
                                               uint8_t mac[6]);

int usbd_cdc_ncm_set_connect(bool connect, uint32_t speed[2]);

void usbd_cdc_ncm_data_recv_done(uint32_t len);
void usbd_cdc_ncm_data_send_done(uint32_t len);
int usbd_cdc_ncm_start_write(uint8_t *buf, uint32_t len);
int usbd_cdc_ncm_start_read(uint8_t *buf, uint32_t len);

#ifdef CONFIG_USBDEV_CDC_NCM_USING_LWIP
#include "lwip/netif.h"
#include "lwip/pbuf.h"
struct pbuf *usbd_cdc_ncm_eth_rx(void);
int usbd_cdc_ncm_eth_tx(struct pbuf *p);
#endif

#ifdef __cplusplus
}
#endif

#endif /* USBD_CDC_NCM_H */
