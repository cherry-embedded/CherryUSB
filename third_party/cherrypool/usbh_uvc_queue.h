#ifndef USBH_UVC_QUEUE_H
#define USBH_UVC_QUEUE_H

#include "chry_pool.h"
#include "usbh_core.h"
#include "usbh_video.h"

int usbh_uvc_frame_create(struct usbh_videoframe *frame, uint32_t count);
struct usbh_videoframe *usbh_uvc_frame_alloc(void);
int usbh_uvc_frame_free(struct usbh_videoframe *frame);
int usbh_uvc_frame_send(struct usbh_videoframe *frame);
int usbh_uvc_frame_recv(struct usbh_videoframe **frame, uint32_t timeout);

/* test uvc frame */
void usbh_uvc_frame_alloc_send_test(void);
void usbh_uvc_frame_test(void);

#endif