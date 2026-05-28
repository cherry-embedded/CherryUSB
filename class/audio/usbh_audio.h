/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_AUDIO_H
#define USBH_AUDIO_H

#include "usb_audio.h"

#ifndef CONFIG_USBHOST_AUDIO_MAX_STREAMS
#define CONFIG_USBHOST_AUDIO_MAX_STREAMS 2
#endif

#ifndef CONFIG_USBHOST_AUDIO_MAX_CHANNELS
#define CONFIG_USBHOST_AUDIO_MAX_CHANNELS 2
#endif

struct usbh_audio_as_msg {
    bool is_tx;
    uint8_t stream_intf;
    uint8_t feature_unit_id;
    uint8_t bTerminalLink;
    uint8_t bmaControls[(CONFIG_USBHOST_AUDIO_MAX_CHANNELS + 1) * 2];
    uint8_t bControlSize;
    uint8_t bNrChannels;
    uint8_t ep_attr;
    struct audio_cs_if_as_format_type_descriptor as_format[CONFIG_USBHOST_MAX_INTF_ALTSETTINGS];
};

struct usbh_audio {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *isoin;  /* ISO IN endpoint */
    struct usb_endpoint_descriptor *isoout; /* ISO OUT endpoint */

    uint8_t ctrl_intf; /* interface number */
    uint8_t minor;
    uint16_t isoin_mps;
    uint16_t isoout_mps;
    bool is_opened;
    uint16_t bcdADC;
    uint8_t bInCollection;
    struct usbh_audio_as_msg as_msg_table[CONFIG_USBHOST_AUDIO_MAX_STREAMS];

    void *user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_audio_open(struct usbh_audio *audio_class, uint32_t samp_freq, uint8_t bitresolution, bool is_tx);
int usbh_audio_close(struct usbh_audio *audio_class, bool is_tx);
int usbh_audio_set_volume(struct usbh_audio *audio_class, uint8_t volume, bool is_tx);
int usbh_audio_set_mute(struct usbh_audio *audio_class, bool mute, bool is_tx);

void usbh_audio_run(struct usbh_audio *audio_class);
void usbh_audio_stop(struct usbh_audio *audio_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_AUDIO_H */
