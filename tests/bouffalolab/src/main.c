#include <FreeRTOS.h>
#include "semphr.h"
#include "usbh_core.h"
#include "bflb_uart.h"
#include "board.h"
#include "shell.h"
#ifdef CONFIG_USB_EHCI_ISO
#include "usbh_uvc_stream.h"
#include "usbh_uac_stream.h"
#endif
#include "lwip/tcpip.h"

static struct bflb_device_s *uart0;

extern void shell_init_with_task(struct bflb_device_s *shell);

#ifdef CONFIG_USB_EHCI_ISO
static ATTR_NOINIT_PSRAM_SECTION USB_MEM_ALIGNX uint8_t frame_buffer1[640 * 480 * 2];
static ATTR_NOINIT_PSRAM_SECTION USB_MEM_ALIGNX uint8_t frame_buffer2[640 * 480 * 2];
static struct usbh_videoframe frame_pool[2];

static ATTR_NOINIT_PSRAM_SECTION USB_MEM_ALIGNX uint8_t frame_buffer[AUDIO_MIC_EP_MPS * 8];
static struct usbh_audioframe frame_pool2[8];

void usbh_video_run(struct usbh_video *video_class)
{
    usbh_video_stream_start(640, 480, USBH_VIDEO_FORMAT_MJPEG);
}

void usbh_video_stop(struct usbh_video *video_class)
{
    usbh_video_stream_stop();
}

void usbh_video_frame_callback(struct usbh_videoframe *frame)
{
    USB_LOG_RAW("frame buf:%p,frame len:%d\r\n", frame->frame_buf, frame->frame_size);
}
#endif

int main(void)
{
    board_init();

    uart0 = bflb_device_get_by_name("uart0");
    shell_init_with_task(uart0);

    /* Initialize the LwIP stack */
    tcpip_init(NULL, NULL);

    printf("Starting usb host task...\r\n");
#ifdef CONFIG_USB_EHCI_ISO
    extern void usbh_video_fps_init(void);
    usbh_video_fps_init();
    frame_pool[0].frame_buf = frame_buffer1;
    frame_pool[0].frame_bufsize = 640 * 480 * 2;
    frame_pool[1].frame_buf = frame_buffer2;
    frame_pool[1].frame_bufsize = 640 * 480 * 2;

    usbh_video_stream_init(5, frame_pool, 2);

    for (uint8_t i = 0; i < 8; i++) {
        frame_pool2[i].frame_buf = frame_buffer + i * AUDIO_MIC_EP_MPS;
        frame_pool2[i].frame_bufsize = AUDIO_MIC_EP_MPS;
    }

    usbh_audio_mic_stream_init(5, frame_pool2, 8);
#endif
    vTaskStartScheduler();

    while (1) {
    }
}

int usbh_deinit(int argc, char **argv)
{
    printf("usbh_deinit\r\n");
    usbh_deinitialize(0);
    return 0;
}
SHELL_CMD_EXPORT_ALIAS(usbh_deinit, usbh_deinit, usbh deinit);

int usbh_init(int argc, char **argv)
{
    printf("usbh_init\r\n");
    usbh_initialize(0, 0x20072000, NULL);
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(usbh_init, usbh_init, usbh init);

SHELL_CMD_EXPORT_ALIAS(lsusb, lsusb, ls usb);

// int uvcinit(int argc, char **argv)
// {
//     video_init(0, 0x20072000);
//     return 0;
// }
// SHELL_CMD_EXPORT_ALIAS(uvcinit, uvcinit, usbh init);

// int uvcsend(int argc, char **argv)
// {
//     extern void video_test(uint8_t busid);
//     video_test(0);
//     return 0;
// }
// SHELL_CMD_EXPORT_ALIAS(uvcsend, uvcsend, usbh init);

#ifdef CONFIG_USB_EHCI_ISO
int usbh_uvc_start(int argc, char **argv)
{
    uint8_t type;

    if (argc < 2) {
        USB_LOG_ERR("please input correct command: usbh_uvc_start type\r\n");
        USB_LOG_ERR("type 0:yuyv, type 1:mjpeg\r\n");
        return -1;
    }

    type = atoi(argv[1]);
    usbh_video_stream_start(640, 480, type);
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(usbh_uvc_start, usbh_uvc_start, usbh_uvc_start);

int usbh_uvc_stop(int argc, char **argv)
{
    usbh_video_stream_stop();
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(usbh_uvc_stop, usbh_uvc_stop, usbh_uvc_stop);

int usbh_uac_start(int argc, char **argv)
{
    uint32_t freq;

    if (argc < 2) {
        USB_LOG_ERR("please input correct command: usbh_uac_start freq\r\n");
        return -1;
    }

    freq = atoi(argv[1]);
    usbh_audio_mic_stream_start(freq);
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(usbh_uac_start, usbh_uac_start, usbh_uac_start);

int usbh_uac_stop(int argc, char **argv)
{
    usbh_audio_mic_stream_stop();
    return 0;
}

SHELL_CMD_EXPORT_ALIAS(usbh_uac_stop, usbh_uac_stop, usbh_uac_stop);
#endif