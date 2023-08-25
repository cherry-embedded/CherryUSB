/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_VIDEO_H
#define USBH_VIDEO_H

#include "usb_video.h"

#define USBH_VIDEO_FORMAT_UNCOMPRESSED 0
#define USBH_VIDEO_FORMAT_MJPEG        1

struct usbh_video_resolution {
    uint16_t wWidth;
    uint16_t wHeight;
};

struct usbh_video_format {
    struct usbh_video_resolution frame[12];
    uint8_t format_type;
    uint8_t num_of_frames;
};

struct usbh_videostreaming {
    uint32_t bufoffset;
    uint32_t buflen;
    uint16_t width;
    uint16_t heigth;
    void (*video_one_frame_callback)(struct usbh_videostreaming *stream);
};

struct usbh_video {
    struct usbh_hubport *hport;

    uint8_t ctrl_intf; /* interface number */
    uint8_t data_intf; /* interface number */
    uint8_t minor;
    usbh_pipe_t isoin;  /* ISO IN endpoint */
    usbh_pipe_t isoout; /* ISO OUT endpoint */
    struct video_probe_and_commit_controls probe;
    struct video_probe_and_commit_controls commit;
    uint16_t isoin_mps;
    uint16_t isoout_mps;
    bool is_opened;
    uint8_t current_format;
    uint16_t bcdVDC;
    uint8_t num_of_intf_altsettings;
    uint8_t num_of_formats;
    struct usbh_video_format format[3];
};

#ifdef __cplusplus
extern "C" {
#endif

int usbh_video_get_cur(struct usbh_video *video_class, uint8_t request, uint8_t intf, uint8_t entity_id, uint8_t cs, uint8_t *buf, uint16_t len);
int usbh_video_set_cur(struct usbh_video *video_class, uint8_t request, uint8_t intf, uint8_t entity_id, uint8_t cs, uint8_t *buf, uint16_t len);

int usbh_video_open(struct usbh_video *video_class,
                    uint8_t format_type,
                    uint16_t wWidth,
                    uint16_t wHeight,
                    uint8_t altsetting);
int usbh_video_close(struct usbh_video *video_class);

void usbh_video_list_info(struct usbh_video *video_class);

void usbh_video_run(struct usbh_video *video_class);
void usbh_video_stop(struct usbh_video *video_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_VIDEO_H */
