#include "usbh_core.h"
#include "usbh_cdc_acm.h"
#include "usbh_hid.h"
#include "usbh_msc.h"
#include "usbh_video.h"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_buffer[512];

struct usbh_urb cdc_bulkin_urb;
struct usbh_urb cdc_bulkout_urb;

void usbh_cdc_acm_callback(void *arg, int nbytes)
{
    //struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)arg;

    if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            USB_LOG_RAW("0x%02x ", cdc_buffer[i]);
        }
    }

    USB_LOG_RAW("nbytes:%d\r\n", nbytes);
}

int cdc_acm_test(void)
{
    int ret;

    struct usbh_cdc_acm *cdc_acm_class = (struct usbh_cdc_acm *)usbh_find_class_instance("/dev/ttyACM0");

    if (cdc_acm_class == NULL) {
        USB_LOG_RAW("do not find /dev/ttyACM0\r\n");
        return -1;
    }

    memset(cdc_buffer, 0, 512);

    usbh_bulk_urb_fill(&cdc_bulkin_urb, cdc_acm_class->bulkin, cdc_buffer, 64, 3000, NULL, NULL);
    ret = usbh_submit_urb(&cdc_bulkin_urb);
    if (ret < 0) {
        USB_LOG_RAW("bulk in error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("recv over:%d\r\n", cdc_bulkin_urb.actual_length);
        for (size_t i = 0; i < cdc_bulkin_urb.actual_length; i++) {
            USB_LOG_RAW("0x%02x ", cdc_buffer[i]);
        }
    }

    USB_LOG_RAW("\r\n");
    const uint8_t data1[10] = { 0x02, 0x00, 0x00, 0x00, 0x02, 0x02, 0x08, 0x14 };

    memcpy(cdc_buffer, data1, 8);
    usbh_bulk_urb_fill(&cdc_bulkout_urb, cdc_acm_class->bulkout, cdc_buffer, 8, 3000, NULL, NULL);
    ret = usbh_submit_urb(&cdc_bulkout_urb);
    if (ret < 0) {
        USB_LOG_RAW("bulk out error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("send over:%d\r\n", cdc_bulkout_urb.actual_length);
    }

    usbh_bulk_urb_fill(&cdc_bulkin_urb, cdc_acm_class->bulkin, cdc_buffer, 64, 3000, usbh_cdc_acm_callback, cdc_acm_class);
    ret = usbh_submit_urb(&cdc_bulkin_urb);
    if (ret < 0) {
        USB_LOG_RAW("bulk in error,ret:%d\r\n", ret);
    } else {
    }

    return ret;
}
#if 0
#include "ff.h"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_write_buffer[25 * 100];

USB_NOCACHE_RAM_SECTION FATFS fs;
USB_NOCACHE_RAM_SECTION FIL fnew;
UINT fnum;
FRESULT res_sd = 0;

int usb_msc_fatfs_test()
{
    const char *tmp_data = "cherryusb fatfs demo...\r\n";

    USB_LOG_RAW("data len:%d\r\n", strlen(tmp_data));
    for (uint32_t i = 0; i < 100; i++) {
        memcpy(&read_write_buffer[i * 25], tmp_data, strlen(tmp_data));
    }

    res_sd = f_mount(&fs, "2:", 1);
    if (res_sd != FR_OK) {
        USB_LOG_RAW("mount fail,res:%d\r\n", res_sd);
        return -1;
    }

    USB_LOG_RAW("test fatfs write\r\n");
    res_sd = f_open(&fnew, "2:test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res_sd == FR_OK) {
        res_sd = f_write(&fnew, read_write_buffer, sizeof(read_write_buffer), &fnum);
        if (res_sd == FR_OK) {
            USB_LOG_RAW("write success, write len：%d\n", fnum);
        } else {
            USB_LOG_RAW("write fail\r\n");
            goto unmount;
        }
        f_close(&fnew);
    } else {
        USB_LOG_RAW("open fail\r\n");
        goto unmount;
    }
    USB_LOG_RAW("test fatfs read\r\n");

    res_sd = f_open(&fnew, "2:test.txt", FA_OPEN_EXISTING | FA_READ);
    if (res_sd == FR_OK) {
        res_sd = f_read(&fnew, read_write_buffer, sizeof(read_write_buffer), &fnum);
        if (res_sd == FR_OK) {
            USB_LOG_RAW("read success, read len：%d\n", fnum);
        } else {
            USB_LOG_RAW("read fail\r\n");
            goto unmount;
        }
        f_close(&fnew);
    } else {
        USB_LOG_RAW("open fail\r\n");
        goto unmount;
    }
    f_mount(NULL, "2:", 1);
    return 0;
unmount:
    f_mount(NULL, "2:", 1);
    return -1;
}
#endif

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t partition_table[512];

int msc_test(void)
{
    int ret;
    struct usbh_msc *msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
    if (msc_class == NULL) {
        USB_LOG_RAW("do not find /dev/sda\r\n");
        return -1;
    }
#if 1
    /* get the partition table */
    ret = usbh_msc_scsi_read10(msc_class, 0, partition_table, 1);
    if (ret < 0) {
        USB_LOG_RAW("scsi_read10 error,ret:%d\r\n", ret);
        return ret;
    }
    for (uint32_t i = 0; i < 512; i++) {
        if (i % 16 == 0) {
            USB_LOG_RAW("\r\n");
        }
        USB_LOG_RAW("%02x ", partition_table[i]);
    }
    USB_LOG_RAW("\r\n");
#endif

#if 0
    usb_msc_fatfs_test();
#endif
    return ret;
}

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t hid_buffer[128];

struct usbh_urb hid_intin_urb;

void usbh_hid_callback(void *arg, int nbytes)
{
    //struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            USB_LOG_RAW("0x%02x ", hid_buffer[i]);
        }
    }

    USB_LOG_RAW("nbytes:%d\r\n", nbytes);
    usbh_submit_urb(&hid_intin_urb);
}

int hid_test(void)
{
    int ret;
    struct usbh_hid *hid_class = (struct usbh_hid *)usbh_find_class_instance("/dev/input0");
    if (hid_class == NULL) {
        USB_LOG_RAW("do not find /dev/input0\r\n");
        return -1;
    }

    usbh_int_urb_fill(&hid_intin_urb, hid_class->intin, hid_buffer, 8, 0, usbh_hid_callback, hid_class);
    ret = usbh_submit_urb(&hid_intin_urb);
    return ret;
}

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t video_buffer[10 * 1024]; /* just for reference , use ram larger than 4M */

#define VIDEO_ISO_PACKETS 512

int video_test(void)
{
    int ret;
    struct usbh_video *video_class = (struct usbh_video *)usbh_find_class_instance("/dev/video0");
    if (video_class == NULL) {
        USB_LOG_RAW("do not find /dev/video0\r\n");
        return -1;
    }

    usbh_videostreaming_set_cur_commit(video_class, 1, 1, 160 * 120 * 2, 512); /* select resolution from list ,just for reference now */

    usbh_video_open(video_class, 7); /* select ep mps from altsettings ,just for reference now */
    usb_osal_msleep(100);

    struct usbh_urb *video_urb = usb_malloc(sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet) * VIDEO_ISO_PACKETS);
    if (video_urb == NULL) {
        USB_LOG_ERR("No memory to alloc urb\r\n");
        while (1) {
        }
    }

    uint8_t *tmp_buf = video_buffer;
    memset(video_urb, 0, sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet) * VIDEO_ISO_PACKETS);
    video_urb->pipe = video_class->isoin;
    video_urb->num_of_iso_packets = VIDEO_ISO_PACKETS;
    video_urb->timeout = 0xffffffff;
    for (uint32_t i = 0; i < VIDEO_ISO_PACKETS; i++) {
        video_urb->iso_packet[i].transfer_buffer = tmp_buf;
        video_urb->iso_packet[i].transfer_buffer_length = video_class->isoin_mps;
        //tmp_buf+=video_class->isoin_mps; /* enable this when use ram larger than 4M */
    }
    while (1) {
        ret = usbh_submit_urb(video_urb);
        if (ret < 0) {
            USB_LOG_ERR("Fail to submit urb:%d\r\n", ret);
            break;
        }
    }
    usb_free(video_urb);
    return ret;
}

void usbh_device_mount_done_callback(struct usbh_hubport *hport)
{
}

void usbh_device_unmount_done_callback(struct usbh_hubport *hport)
{
}

static void usbh_class_test_thread(void *argument)
{
    while (1) {
        printf("helloworld\r\n");
        usb_osal_msleep(1000);
        cdc_acm_test();
        msc_test();
        hid_test();
        video_test();
    }
}

void usbh_class_test(void)
{
    usb_osal_thread_create("usbh_test", 4096, CONFIG_USBHOST_PSC_PRIO + 1, usbh_class_test_thread, NULL);
}