/**
 * @file
 * @brief USB HUB Device Class public header
 *
 */

#ifndef _USBD_HUB_H_
#define _USBD_HUB_H_

#include "usb_hub.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_hub_add_interface(usbd_class_t *class, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* _USBD_HUB_H_ */
