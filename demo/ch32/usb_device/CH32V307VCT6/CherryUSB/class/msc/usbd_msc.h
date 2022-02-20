/**
 * @file
 * @brief USB Mass Storage Class public header
 *
 * Header follows the Mass Storage Class Specification
 * (Mass_Storage_Specification_Overview_v1.4_2-19-2010.pdf) and
 * Mass Storage Class Bulk-Only Transport Specification
 * (usbmassbulk_10.pdf).
 * Header is limited to Bulk-Only Transfer protocol.
 */

#ifndef _USBD_MSC_H__
#define _USBD_MSC_H__

#include "usb_msc.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_msc_class_init(uint8_t out_ep, uint8_t in_ep);
void usbd_msc_get_cap(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
int usbd_msc_sector_read(uint32_t sector, uint8_t *buffer, uint32_t length);
int usbd_msc_sector_write(uint32_t sector, uint8_t *buffer, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* USBD_MSC_H_ */
