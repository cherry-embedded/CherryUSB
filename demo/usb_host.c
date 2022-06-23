#include "usbh_core.h"
#include "usbh_cdc_acm.h"
#include "usbh_hid.h"
#include "usbh_msc.h"

USB_MEM_ALIGN32 uint8_t cdc_buffer[512];

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
    ret = usbh_ep_bulk_transfer(cdc_acm_class->bulkin, cdc_buffer, 512, 3000);
    if (ret < 0) {
        USB_LOG_RAW("bulk in error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("recv over:%d\r\n", ret);
        for (size_t i = 0; i < ret; i++) {
            USB_LOG_RAW("0x%02x ", cdc_buffer[i]);
        }
    }

    USB_LOG_RAW("\r\n");
    const uint8_t data1[10] = { 0x02, 0x00, 0x00, 0x00, 0x02, 0x02, 0x08, 0x14 };

    memcpy(cdc_buffer, data1, 8);
    ret = usbh_ep_bulk_transfer(cdc_acm_class->bulkout, cdc_buffer, 8, 3000);
    if (ret < 0) {
        USB_LOG_RAW("bulk out error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("send over:%d\r\n", ret);
    }

#if 0
    usbh_ep_bulk_async_transfer(cdc_acm_class->bulkin, cdc_buffer, 512, usbh_cdc_acm_callback, cdc_acm_class);
#else
    ret = usbh_ep_bulk_transfer(cdc_acm_class->bulkin, cdc_buffer, 512, 3000);
    if (ret < 0) {
        USB_LOG_RAW("bulk in error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("recv over:%d\r\n", ret);
        for (size_t i = 0; i < ret; i++) {
            USB_LOG_RAW("0x%02x ", cdc_buffer[i]);
        }
    }

    USB_LOG_RAW("\r\n");

    return ret;
#endif
}
#if 0
#include "ff.h"
#endif

USB_MEM_ALIGN32 uint8_t partition_table[512];

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
    uint8_t *msc_buffer = usb_iomalloc(8192);
    ret = usbh_msc_scsi_read10(msc_class, 0, msc_buffer, 16);
    usb_iofree(msc_buffer);
    // for (uint32_t i = 0; i < 1024; i++) {
    //     if (i % 16 == 0) {
    //         USB_LOG_RAW("\r\n");
    //     }
    //     USB_LOG_RAW("%02x ", msc_buffer[i]);
    // }
    // USB_LOG_RAW("\r\n");
#endif

#if 0

    FATFS fs;
    FIL fnew;
    UINT fnum;
    FRESULT res_sd = 0;
    uint8_t *ReadBuffer;

    ReadBuffer = usb_iomalloc(512);
    f_mount(&fs, "2:", 1);
    res_sd = f_open(&fnew, "2:test.c", FA_OPEN_EXISTING | FA_READ);
    if (res_sd == FR_OK) {
        res_sd = f_read(&fnew, ReadBuffer, 512, &fnum);
        for (uint32_t i = 0; i < fnum; i++) {
            if (i % 16 == 0) {
                USB_LOG_RAW("\r\n");
            }
            USB_LOG_RAW("%02x ", ReadBuffer[i]);
        }
        USB_LOG_RAW("\r\n");
        f_close(&fnew);
        /*unmount*/
        f_mount(NULL, "2:", 1);
    } else {
        USB_LOG_RAW("open error:%d\r\n", res_sd);
    }
    usb_iofree(ReadBuffer);
#endif
    return ret;
}

USB_MEM_ALIGN32 uint8_t hid_buffer[128];

void usbh_hid_callback(void *arg, int nbytes)
{
    //struct usbh_hid *hid_class = (struct usbh_hid *)arg;

    if (nbytes > 0) {
        for (size_t i = 0; i < nbytes; i++) {
            USB_LOG_RAW("0x%02x ", hid_buffer[i]);
        }
    }

    USB_LOG_RAW("nbytes:%d\r\n", nbytes);
}

int hid_test(void)
{
    int ret;
    struct usbh_hid *hid_class = (struct usbh_hid *)usbh_find_class_instance("/dev/input0");
    if (hid_class == NULL) {
        USB_LOG_RAW("do not find /dev/input0\r\n");
        return -1;
    }
#if 1
    ret = usbh_ep_intr_async_transfer(hid_class->intin, hid_buffer, 128, usbh_hid_callback, hid_class);
    if (ret < 0) {
        USB_LOG_RAW("intr asnyc in error,ret:%d\r\n", ret);
    }
#else
    ret = usbh_ep_intr_transfer(hid_class->intin, hid_buffer, 128, 1000);
    if (ret < 0) {
        USB_LOG_RAW("intr in error,ret:%d\r\n", ret);
        return ret;
    }
    USB_LOG_RAW("recv len:%d\r\n", ret);
#endif
    return ret;
}