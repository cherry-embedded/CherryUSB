/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "diskio_impl.h"
#include "ffconf.h"
#include "ff.h"
#include "esp_vfs_fat.h"

#include "usbh_core.h"
#include "usbh_msc.h"

static char *TAG = "MSC";

#define DRIVE_STR_LEN 3

typedef struct msc_host_vfs {
    uint8_t pdrv;
    FATFS *fs;
    char base_path[0];
} msc_host_vfs_t;

static struct usbh_msc *s_mscs[FF_VOLUMES] = { NULL };

static DSTATUS usb_disk_initialize(BYTE pdrv)
{
    return RES_OK;
}

static DSTATUS usb_disk_status(BYTE pdrv)
{
    return RES_OK;
}

static DRESULT usb_disk_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    struct usbh_msc *msc_class;
    assert(pdrv < FF_VOLUMES);

    msc_class = s_mscs[pdrv];
    assert(msc_class);
    if (sector >= msc_class->blocknum - count) {
        ESP_LOGW(TAG, "%s: sector 0x%"PRIX32" out of range", __FUNCTION__, (uint32_t)sector);
        return RES_PARERR;
    }

    uint8_t *dma_buff = buff;
    size_t len = msc_class->blocksize * count;

    if (((uint32_t)dma_buff & (CONFIG_USB_ALIGN_SIZE - 1)) || (len & (CONFIG_USB_ALIGN_SIZE - 1))) {
        dma_buff = heap_caps_aligned_alloc(CONFIG_USB_ALIGN_SIZE, len, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (dma_buff == NULL) {
            return RES_ERROR;
        }
    }

    int ret = usbh_msc_scsi_read10(msc_class, sector, dma_buff, count);
    if (dma_buff != buff) {
        if (ret == 0) {
            memcpy(buff, dma_buff, len);
        }
        heap_caps_free(dma_buff);
    }
    if (ret != 0) {
        ESP_LOGE(TAG, "usbh_msc_scsi_read10 failed (%d)", ret);
        return RES_ERROR;
    }

    return RES_OK;
}

static DRESULT usb_disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    struct usbh_msc *msc_class;
    assert(pdrv < FF_VOLUMES);

    msc_class = s_mscs[pdrv];
    assert(msc_class);
    if (sector >= msc_class->blocknum - count) {
        ESP_LOGW(TAG, "%s: sector 0x%"PRIX32" out of range", __FUNCTION__, (uint32_t)sector);
        return RES_PARERR;
    }

    const uint8_t *dma_buff = buff;
    size_t len = msc_class->blocksize * count;

    if (((uint32_t)dma_buff & (CONFIG_USB_ALIGN_SIZE - 1)) || (len & (CONFIG_USB_ALIGN_SIZE - 1))) {
        dma_buff = heap_caps_aligned_alloc(CONFIG_USB_ALIGN_SIZE, len, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (dma_buff == NULL) {
            return RES_ERROR;
        }
        memcpy((uint8_t *)dma_buff, buff, len);
    }

    int ret = usbh_msc_scsi_write10(msc_class, sector, dma_buff, count);
    if (dma_buff != buff) {
        heap_caps_free((uint8_t *)dma_buff);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "usbh_msc_scsi_write10 failed (%d)", ret);
        return RES_ERROR;
    }
    return RES_OK;
}

static DRESULT usb_disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    struct usbh_msc *msc_class;
    assert(pdrv < FF_VOLUMES);

    msc_class = s_mscs[pdrv];
    assert(msc_class);

    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *((DWORD *) buff) = msc_class->blocknum;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *((WORD *) buff) = msc_class->blocksize;
        return RES_OK;
    case GET_BLOCK_SIZE:
        return RES_ERROR;
    }
    return RES_ERROR;
}

void ff_diskio_register_msc(BYTE pdrv, struct usbh_msc *msc_class)
{
    assert(pdrv < FF_VOLUMES);

    static const ff_diskio_impl_t usb_disk_impl = {
        .init = &usb_disk_initialize,
        .status = &usb_disk_status,
        .read = &usb_disk_read,
        .write = &usb_disk_write,
        .ioctl = &usb_disk_ioctl
    };
    s_mscs[pdrv] = msc_class;
    ff_diskio_register(pdrv, &usb_disk_impl);
}

BYTE ff_diskio_get_pdrv_disk(const struct usbh_msc *msc_class)
{
    for (int i = 0; i < FF_VOLUMES; i++) {
        if (msc_class == s_mscs[i]) {
            return i;
        }
    }
    return 0xff;
}

static esp_err_t msc_host_format(struct usbh_msc *msc_class, size_t allocation_size)
{
    ESP_RETURN_ON_FALSE((msc_class != NULL && msc_class->user_data != NULL), ESP_ERR_INVALID_ARG, TAG, "");
    void *workbuf = NULL;
    const size_t workbuf_size = 4096;
    msc_host_vfs_t *vfs = (msc_host_vfs_t *)msc_class->user_data;

    char drive[DRIVE_STR_LEN] = {(char)('0' + vfs->pdrv), ':', 0};
    ESP_RETURN_ON_FALSE((workbuf = ff_memalloc(workbuf_size)), ESP_ERR_NO_MEM, TAG, "");

    // Valid value of cluster size is between sector_size and 128 * sector_size.
    size_t cluster_size = MIN(MAX(allocation_size, msc_class->blocksize), 128 * msc_class->blocksize);

    ESP_LOGW(TAG, "Formatting card, allocation unit size=%d", cluster_size);
    f_mount(0, drive, 0);

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    FRESULT err = f_mkfs(drive, FM_ANY | FM_SFD, cluster_size, workbuf, workbuf_size);
#else
    const MKFS_PARM opt = {(BYTE)(FM_ANY | FM_SFD), 0, 0, 0, cluster_size};
    FRESULT err = f_mkfs(drive, &opt, workbuf, workbuf_size);
#endif

    free(workbuf);
    if (err != FR_OK || (err = f_mount(vfs->fs, drive, 0)) != FR_OK) {
        ESP_LOGE(TAG, "Formatting failed with error: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t msc_host_vfs_register(struct usbh_msc *msc_class,
                                const char *base_path,
                                const esp_vfs_fat_mount_config_t *mount_config)
{
    ESP_RETURN_ON_FALSE((msc_class != NULL && msc_class->user_data == NULL && base_path != NULL && mount_config != NULL), ESP_ERR_INVALID_ARG, TAG, "");

    FATFS *fs = NULL;
    BYTE pdrv;

    if (ff_diskio_get_drive(&pdrv) != ESP_OK) {
        ESP_LOGW(TAG, "the maximum count of volumes is already mounted");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret;

    msc_host_vfs_t *vfs = malloc(sizeof(msc_host_vfs_t) + strlen(base_path) + 1);
    ESP_RETURN_ON_FALSE(vfs != NULL, ESP_ERR_NO_MEM, TAG, "");

    ff_diskio_register_msc(pdrv, msc_class);
    char drive[DRIVE_STR_LEN] = {(char)('0' + pdrv), ':', 0};
    strcpy(vfs->base_path, base_path);
    vfs->pdrv = pdrv;

    ret = esp_vfs_fat_register(base_path, drive, mount_config->max_files, &fs);
    ESP_GOTO_ON_ERROR(ret, fail, TAG, "Failed to register filesystem, error=%s", esp_err_to_name(ret));
    vfs->fs = fs;

    msc_class->user_data = vfs;
    if (f_mount(fs, drive, 1) != FR_OK) {
        if ((!mount_config->format_if_mount_failed) || msc_host_format(msc_class, mount_config->allocation_unit_size) != ESP_OK) {
            ret = ESP_FAIL;
            goto fail;
        }
    }
    return ESP_OK;

fail:
    msc_class->user_data = NULL;
    if (fs) {
        f_mount(NULL, drive, 0);
    }
    esp_vfs_fat_unregister_path(base_path);
    ff_diskio_unregister(pdrv);
    s_mscs[pdrv] = NULL;
    return ret;
}

esp_err_t msc_host_vfs_unregister(struct usbh_msc *msc_class)
{
    ESP_RETURN_ON_FALSE((msc_class != NULL && ff_diskio_get_pdrv_disk(msc_class) != 0XFF), ESP_ERR_INVALID_ARG, TAG, "");
    msc_host_vfs_t *vfs = (msc_host_vfs_t *)msc_class->user_data;
    msc_class->user_data = NULL;

    char drive[DRIVE_STR_LEN] = {(char)('0' + vfs->pdrv), ':', 0};
    f_mount(NULL, drive, 0);
    ff_diskio_unregister(vfs->pdrv);
    s_mscs[vfs->pdrv] = NULL;
    esp_vfs_fat_unregister_path(vfs->base_path);
    heap_caps_free(vfs);
    return ESP_OK;
}

static void usbh_msc_thread(CONFIG_USB_OSAL_THREAD_SET_ARGV)
{
    struct usbh_msc *msc_class = (struct usbh_msc *)CONFIG_USB_OSAL_THREAD_GET_ARGV;
    int ret;

    ret = usbh_msc_scsi_init(msc_class);
    if (ret < 0) {
        ESP_LOGE(TAG, "scsi_init error,ret:%d", ret);
        return;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 4 * 1024
    };
    ESP_LOGI(TAG, "Mounting msc host filesystem");

    if (msc_host_vfs_register(msc_class, "/usb", &mount_config) != ESP_OK) {
        ESP_LOGE(TAG, "msc_host_vfs_register fail");
        return;
    }
    ESP_LOGI(TAG, "MSC host filesystem mounted");

    usb_osal_thread_delete(NULL);
}

void usbh_msc_run(struct usbh_msc *msc_class)
{
    usb_osal_thread_create("usbh_msc", 2048, CONFIG_USBHOST_PSC_PRIO + 1, usbh_msc_thread, msc_class);
}

void usbh_msc_stop(struct usbh_msc *msc_class)
{
    msc_host_vfs_unregister(msc_class);
}