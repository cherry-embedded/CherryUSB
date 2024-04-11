#include "usbh_uvc_queue.h"
#include "chry_pool.h"

struct chry_pool usbh_uvc_pool;
uint32_t usbh_uvc_pool_buf[10];

int usbh_uvc_frame_create(struct usbh_videoframe *frame, uint32_t count)
{
    return chry_pool_create(&usbh_uvc_pool, usbh_uvc_pool_buf, frame, sizeof(struct usbh_videoframe), count);
}

struct usbh_videoframe *usbh_uvc_frame_alloc(void)
{
    return (struct usbh_videoframe *)chry_pool_item_alloc(&usbh_uvc_pool);
}

int usbh_uvc_frame_free(struct usbh_videoframe *frame)
{
    return chry_pool_item_free(&usbh_uvc_pool, (uintptr_t *)frame);
}

int usbh_uvc_frame_send(struct usbh_videoframe *frame)
{
    return chry_pool_item_send(&usbh_uvc_pool, (uintptr_t *)frame);
}

int usbh_uvc_frame_recv(struct usbh_videoframe **frame, uint32_t timeout)
{
    return chry_pool_item_recv(&usbh_uvc_pool, (uintptr_t **)frame, timeout);
}

uint8_t frame_buffer1[1]; /* just for test */
uint8_t frame_buffer2[1]; /* just for test */
struct usbh_videoframe frame_pool[2];

void usbh_uvc_frame_init(void)
{
    frame_pool[0].frame_buf = frame_buffer1;
    frame_pool[0].frame_bufsize = 640 * 480 * 2;
    frame_pool[1].frame_buf = frame_buffer2;
    frame_pool[1].frame_bufsize = 640 * 480 * 2;
    usbh_uvc_frame_create(frame_pool, 2);
}

static void usbh_frame_thread(void *argument)
{
    int ret;
    struct usbh_videoframe *frame;

    while (1) {
        ret = usbh_uvc_frame_recv(&frame, 0xfffffff);
        if (ret < 0) {
            continue;
        }
        USB_LOG_RAW("frame buf:%p,frame len:%d\r\n", frame->frame_buf, frame->frame_size);
        usbh_uvc_frame_free(frame);
    }
}

void usbh_uvc_frame_alloc_send_test(void)
{
    struct usbh_videoframe *frame = NULL;

    frame = usbh_uvc_frame_alloc();
    if (frame) {
        frame->frame_size = 640 * 480 * 2;
        usbh_uvc_frame_send(frame);
    }
}

void usbh_uvc_frame_test(void)
{
    usbh_uvc_frame_init();

    usb_osal_thread_create("usbh_video", 3072, 5, usbh_frame_thread, NULL);
}
