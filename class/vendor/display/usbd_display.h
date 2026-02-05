/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_DISPLAY_H
#define USBD_DISPLAY_H

#define USBD_DISPLAY_TYPE_RGB565 0
#define USBD_DISPLAY_TYPE_RGB888 1
#define USBD_DISPLAY_TYPE_YUV420 2
#define USBD_DISPLAY_TYPE_JPG    3

struct usbd_display_frame {
    uint8_t *frame_buf;
    uint32_t frame_bufsize;
    uint32_t frame_format;
    uint32_t frame_size;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Init display interface driver */
struct usbd_interface *usbd_display_init_intf(struct usbd_interface *intf,
                                              const uint8_t out_ep,
                                              const uint8_t in_ep,
                                              struct usbd_display_frame *frame,
                                              uint32_t count);

int usbd_display_dequeue(struct usbd_display_frame **frame, uint32_t timeout);
int usbd_display_enqueue(struct usbd_display_frame *frame);

#ifdef __cplusplus
}
#endif

#endif /* USBD_DISPLAY_H */
