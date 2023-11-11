#include "usbh_core.h"
#include "usbh_cdc_acm.h"
#include "usbh_hid.h"
#include "usbh_msc.h"
#include "usbh_video.h"
#include "usbh_audio.h"

#define TEST_USBH_CDC_ACM   1
#define TEST_USBH_HID       1
#define TEST_USBH_MSC       1
#define TEST_USBH_MSC_FATFS 0
#define TEST_USBH_AUDIO     0
#define TEST_USBH_VIDEO     0
#define TEST_USBH_CDC_ECM   0

#if TEST_USBH_CDC_ACM
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
        USB_LOG_RAW("nbytes:%d\r\n", nbytes);
    }
}

static void usbh_cdc_acm_thread(void *argument)
{
    int ret;
    struct usbh_cdc_acm *cdc_acm_class;

    while (1) {
        // clang-format off
find_class:
        // clang-format on
        cdc_acm_class = (struct usbh_cdc_acm *)usbh_find_class_instance("/dev/ttyACM0");
        if (cdc_acm_class == NULL) {
            USB_LOG_RAW("do not find /dev/ttyACM0\r\n");
            usb_osal_msleep(1000);
            continue;
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

        while (1) {
            cdc_acm_class = (struct usbh_cdc_acm *)usbh_find_class_instance("/dev/ttyACM0");
            if (cdc_acm_class == NULL) {
                goto find_class;
            }
            usb_osal_msleep(1000);
        }
    }
}
#endif

#if TEST_USBH_HID
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t hid_buffer[128];

struct usbh_urb hid_intin_urb;

void usbh_hid_callback(void *arg, int nbytes)
{
    //struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            USB_LOG_RAW("0x%02x ", hid_buffer[i]);
        }
        USB_LOG_RAW("nbytes:%d\r\n", nbytes);
        usbh_submit_urb(&hid_intin_urb);
    }
}

static void usbh_hid_thread(void *argument)
{
    int ret;
    struct usbh_hid *hid_class;

    while (1) {
        // clang-format off
find_class:
        // clang-format on
        hid_class = (struct usbh_hid *)usbh_find_class_instance("/dev/input0");
        if (hid_class == NULL) {
            USB_LOG_RAW("do not find /dev/input0\r\n");
            usb_osal_msleep(1500);
            continue;
        }
        usbh_int_urb_fill(&hid_intin_urb, hid_class->intin, hid_buffer, 8, 0, usbh_hid_callback, hid_class);
        ret = usbh_submit_urb(&hid_intin_urb);
        if (ret < 0) {
            usb_osal_msleep(1500);
            goto find_class;
        }

        while (1) {
            hid_class = (struct usbh_hid *)usbh_find_class_instance("/dev/input0");
            if (hid_class == NULL) {
                goto find_class;
            }
            usb_osal_msleep(1500);
        }
    }
}
#endif

#if TEST_USBH_MSC

#if TEST_USBH_MSC_FATFS
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

static void usbh_msc_thread(void *argument)
{
    int ret;
    struct usbh_msc *msc_class;

    while (1) {
        // clang-format off
find_class:
        // clang-format on
        msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
        if (msc_class == NULL) {
            USB_LOG_RAW("do not find /dev/sda\r\n");
            usb_osal_msleep(2000);
            continue;
        }

#if 1
        /* get the partition table */
        ret = usbh_msc_scsi_read10(msc_class, 0, partition_table, 1);
        if (ret < 0) {
            USB_LOG_RAW("scsi_read10 error,ret:%d\r\n", ret);
            usb_osal_msleep(2000);
            goto find_class;
        }
        for (uint32_t i = 0; i < 512; i++) {
            if (i % 16 == 0) {
                USB_LOG_RAW("\r\n");
            }
            USB_LOG_RAW("%02x ", partition_table[i]);
        }
        USB_LOG_RAW("\r\n");
#endif

#if TEST_USBH_MSC_FATFS
        usb_msc_fatfs_test();
#endif
        while (1) {
            msc_class = (struct usbh_msc *)usbh_find_class_instance("/dev/sda");
            if (msc_class == NULL) {
                goto find_class;
            }
            usb_osal_msleep(2000);
        }
    }
}
#endif

#if 0
void usbh_videostreaming_parse_mjpeg(struct usbh_urb *urb, struct usbh_videostreaming *stream)
{
    struct usbh_iso_frame_packet *iso_packet;
    uint32_t num_of_iso_packets;
    uint8_t data_offset;
    uint32_t data_len;
    uint8_t header_len = 0;

    num_of_iso_packets = urb->num_of_iso_packets;
    iso_packet = urb->iso_packet;

    for (uint32_t i = 0; i < num_of_iso_packets; i++) {
        /*
            uint8_t frameIdentifier : 1U;
            uint8_t endOfFrame      : 1U;
            uint8_t presentationTimeStamp    : 1U;
            uint8_t sourceClockReference : 1U;
            uint8_t reserved             : 1U;
            uint8_t stillImage           : 1U;
            uint8_t errorBit             : 1U;
            uint8_t endOfHeader          : 1U;
        */
        if (iso_packet[i].actual_length == 0) { /* skip no data */
            continue;
        }

        header_len = iso_packet[i].transfer_buffer[0];

        if ((header_len > 12) || (header_len == 0)) { /* do not be illegal */
            while (1) {
            }
        }
        if (iso_packet[i].transfer_buffer[1] & (1 << 6)) { /* error bit, re-receive */
            stream->bufoffset = 0;
            continue;
        }

        if ((stream->bufoffset == 0) && ((iso_packet[i].transfer_buffer[header_len] != 0xff) || (iso_packet[i].transfer_buffer[header_len + 1] != 0xd8))) {
            stream->bufoffset = 0;
            continue;
        }

        data_offset = header_len;
        data_len = iso_packet[i].actual_length - header_len;

        /** do something here */

        stream->bufoffset += data_len;

        if (iso_packet[i].transfer_buffer[1] & (1 << 1)) {
            if ((iso_packet[i].transfer_buffer[iso_packet[i].actual_length - 2] != 0xff) || (iso_packet[i].transfer_buffer[iso_packet[i].actual_length - 1] != 0xd9)) {
                stream->bufoffset = 0;
                continue;
            }

            /** do something here */

            if (stream->video_one_frame_callback) {
                stream->video_one_frame_callback(stream);
            }
            stream->bufoffset = 0;
        }
    }
    /** do something here */
}

void usbh_videostreaming_parse_yuyv2(struct usbh_urb *urb, struct usbh_videostreaming *stream)
{
    struct usbh_iso_frame_packet *iso_packet;
    uint32_t num_of_iso_packets;
    uint8_t data_offset;
    uint32_t data_len;
    uint8_t header_len = 0;

    num_of_iso_packets = urb->num_of_iso_packets;
    iso_packet = urb->iso_packet;

    for (uint32_t i = 0; i < num_of_iso_packets; i++) {
        /*
            uint8_t frameIdentifier : 1U;
            uint8_t endOfFrame      : 1U;
            uint8_t presentationTimeStamp    : 1U;
            uint8_t sourceClockReference : 1U;
            uint8_t reserved             : 1U;
            uint8_t stillImage           : 1U;
            uint8_t errorBit             : 1U;
            uint8_t endOfHeader          : 1U;
        */

        if (iso_packet[i].actual_length == 0) { /* skip no data */
            continue;
        }

        header_len = iso_packet[i].transfer_buffer[0];

        if ((header_len > 12) || (header_len == 0)) { /* do not be illegal */
            while (1) {
            }
        }
        if (iso_packet[i].transfer_buffer[1] & (1 << 6)) { /* error bit, re-receive */
            stream->bufoffset = 0;
            continue;
        }

        data_offset = header_len;
        data_len = iso_packet[i].actual_length - header_len;

        /** do something here */

        stream->bufoffset += data_len;

        if (iso_packet[i].transfer_buffer[1] & (1 << 1)) {
            /** do something here */

            if (stream->video_one_frame_callback && (stream->bufoffset == stream->buflen)) {
                stream->video_one_frame_callback(stream);
            }
            stream->bufoffset = 0;
        }
    }

    /** do something here */
}
#endif

#if TEST_USBH_CDC_ECM
#include "usbh_cdc_ecm.h"

#include "netif/etharp.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#if LWIP_DHCP
#include "lwip/dhcp.h"
#endif

struct netif g_cdc_ecm_netif;

static err_t usbh_cdc_ecm_if_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 'E';
    netif->name[1] = 'X';
    netif->output = etharp_output;
    netif->linkoutput = usbh_cdc_ecm_linkoutput;
    return ERR_OK;
}

void usbh_cdc_ecm_run(struct usbh_cdc_ecm *cdc_ecm_class)
{
    struct netif *netif = &g_cdc_ecm_netif;

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, cdc_ecm_class->mac, 6);

    IP4_ADDR(&cdc_ecm_class->ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&cdc_ecm_class->netmask, 0, 0, 0, 0);
    IP4_ADDR(&cdc_ecm_class->gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &cdc_ecm_class->ipaddr, &cdc_ecm_class->netmask, &cdc_ecm_class->gateway, NULL, usbh_cdc_ecm_if_init, tcpip_input);
    netif_set_default(netif);
    while (!netif_is_up(netif)) {
    }

#if LWIP_DHCP
    dhcp_start(netif);
#endif
}

void usbh_cdc_ecm_stop(struct usbh_cdc_ecm *cdc_ecm_class)
{
    struct netif *netif = &g_cdc_ecm_netif;
#if LWIP_DHCP
    dhcp_stop(netif);
    dhcp_cleanup(netif);
#endif
    netif_set_down(netif);
    netif_remove(netif);
}
#endif

void usbh_cdc_acm_run(struct usbh_cdc_acm *cdc_acm_class)
{
}

void usbh_cdc_acm_stop(struct usbh_cdc_acm *cdc_acm_class)
{
}

void usbh_hid_run(struct usbh_hid *hid_class)
{
}

void usbh_hid_stop(struct usbh_hid *hid_class)
{
}

void usbh_msc_run(struct usbh_msc *msc_class)
{
}

void usbh_msc_stop(struct usbh_msc *msc_class)
{
}

void usbh_audio_run(struct usbh_audio *audio_class)
{
}

void usbh_audio_stop(struct usbh_audio *audio_class)
{
}

void usbh_video_run(struct usbh_video *video_class)
{
}

void usbh_video_stop(struct usbh_video *video_class)
{
}

void usbh_class_test(void)
{
#if TEST_USBH_CDC_ACM
    usb_osal_thread_create("usbh_cdc", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_cdc_acm_thread, NULL);
#endif
#if TEST_USBH_HID
    usb_osal_thread_create("usbh_hid", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_hid_thread, NULL);
#endif
#if TEST_USBH_MSC
    usb_osal_thread_create("usbh_msc", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_msc_thread, NULL);
#endif
#if TEST_USBH_AUDIO
#error "if you want to use iso, please contact with me"
    usb_osal_thread_create("usbh_audio", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_audio_thread, NULL);
#endif
#if TEST_USBH_VIDEO
#error "if you want to use iso, please contact with me"
    usb_osal_thread_create("usbh_video", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_video_thread, NULL);
#endif
#if TEST_USBH_CDC_ECM
    /* Initialize the LwIP stack */
    tcpip_init(NULL, NULL);

    usbh_cdc_ecm_lwip_thread_init(&g_cdc_ecm_netif);
#endif
}