/**
 * @file
 * @brief USB Audio Device Class public header
 *
 * Header follows below documentation:
 * - USB Device Class Definition for Audio Devices (audio10.pdf)
 *
 * Additional documentation considered a part of USB Audio v1.0:
 * - USB Device Class Definition for Audio Data Formats (frmts10.pdf)
 * - USB Device Class Definition for Terminal Types (termt10.pdf)
 */

#ifndef _USBD_AUDIO_H_
#define _USBD_AUDIO_H_

#include "usb_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_audio_control_info {
    uint16_t vol_min;
    uint16_t vol_max;
    uint16_t vol_res;
    uint16_t vol_current;
    uint8_t mute;
};

void usbd_audio_add_interface(usbd_class_t *class, usbd_interface_t *intf);
void usbd_audio_set_interface_callback(uint8_t value);
void usbd_audio_set_volume(uint8_t vol);
#ifdef __cplusplus
}
#endif

#endif /* _USB_AUDIO_H_ */
