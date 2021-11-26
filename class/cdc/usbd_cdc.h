/**
 * @file
 * @brief USB Communications Device Class (CDC) public header
 *
 * Header follows the Class Definitions for
 * Communications Devices Specification (CDC120-20101103-track.pdf),
 * PSTN Devices Specification (PSTN120.pdf) and
 * Ethernet Control Model Devices Specification (ECM120.pdf).
 * Header is limited to ACM and ECM Subclasses.
 */

#ifndef _USBD_CDC_H
#define _USBD_CDC_H

#include "usb_cdc.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_cdc_add_acm_interface(usbd_class_t *class, usbd_interface_t *intf);

void usbd_cdc_acm_set_line_coding(uint32_t baudrate, uint8_t databits, uint8_t parity, uint8_t stopbits);
void usbd_cdc_acm_set_dtr(bool dtr);
void usbd_cdc_acm_set_rts(bool rts);

#ifdef __cplusplus
}
#endif

#endif /* USBD_CDC_H_ */
