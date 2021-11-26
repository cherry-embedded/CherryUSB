/**
 * @file
 * @brief USB DFU Device Class public header
 *
 */

#ifndef _USBD_DFU_H_
#define _USBD_DFU_H_

#include "usb_dfu.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_dfu_add_interface(usbd_class_t *class, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* _USBD_DFU_H_ */
