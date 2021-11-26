/**
 * @file
 * @brief USB Video Device Class public header
 *
 * Header follows below documentation:
 * - USB Device Class Definition for Video Devices UVC 1.5 Class specification.pdf
 */

#ifndef _USBD_VIDEO_H_
#define _USBD_VIDEO_H_

#include "usb_video.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_video_sof_callback(void);
void usbd_video_set_interface_callback(uint8_t value);
void usbd_video_add_interface(usbd_class_t *class, usbd_interface_t *intf);

#ifdef __cplusplus
}
#endif

#endif /* USBD_VIDEO_H_ */
