/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * AIC8800D80 U02 firmware table: AICFWPKG parser and raw-image registration.
 */
#include "aic8800_fw.h"
#include "aic8800_wifi_config.h"

#include <stdint.h>
#include <string.h>

#include "usb_config.h"
#include "usb_errno.h"
#include "usb_util.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "aic8800.fw"
#include "usb_log.h"

#define AIC_FW_PACKAGE_HEADER_SIZE 256U
#define AIC_FW_PACKAGE_VERSION       1U
#define AIC_FW_PACKAGE_ENTRY_OFFSET 32U
#define AIC_FW_PACKAGE_ENTRY_SIZE   16U

static const uint8_t g_aic_fw_package_magic[8] = {
    'A', 'I', 'C', 'F', 'W', 'P', 'K', 'G'
};

static const uint32_t g_aic_fw_expected_lengths[AIC8800_FW_IMAGE_COUNT] = {
    1384U, 1708U, 32700U, 16136U, 358072U
};

static const char *const g_aic_fw_names[AIC8800_FW_IMAGE_COUNT] = {
    "fw_patch_table_8800d80_u02.bin",
    "fw_adid_8800d80_u02.bin",
    "fw_patch_8800d80_u02.bin",
    "fw_patch_8800d80_u02_ext0.bin",
    "fmacfw_8800d80_u02.bin"
};

static struct aic8800_fw_blob g_aic_fw_images[AIC8800_FW_IMAGE_COUNT];
static bool g_aic_fw_ready;

static uint32_t aic_fw_get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint32_t aic_fw_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xffffffffUL;
    uint32_t index;

    for (index = 0U; index < length; index++) {
        uint32_t bit;

        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320UL & mask);
        }
    }
    return ~crc;
}

static int aic_fw_store_images(const struct aic8800_fw_blob
                               images[AIC8800_FW_IMAGE_COUNT])
{
    uint32_t index;

    for (index = 0U; index < AIC8800_FW_IMAGE_COUNT; index++) {
        if ((images[index].data == NULL) ||
            (images[index].length != g_aic_fw_expected_lengths[index])) {
            USB_LOG_ERR("invalid %s length=%u (expected %u)\r\n",
                        g_aic_fw_names[index],
                        (unsigned int)images[index].length,
                        (unsigned int)g_aic_fw_expected_lengths[index]);
            return -USB_ERR_INVAL;
        }
    }
    memcpy(g_aic_fw_images, images, sizeof(g_aic_fw_images));
    g_aic_fw_ready = true;
    return 0;
}

int aic8800_fw_set_package(const void *package, uint32_t capacity)
{
    const uint8_t *base = package;
    struct aic8800_fw_blob images[AIC8800_FW_IMAGE_COUNT];
    uint32_t total_length;
    uint32_t index;

    if ((base == NULL) || (capacity < AIC_FW_PACKAGE_HEADER_SIZE)) {
        return -USB_ERR_INVAL;
    }
    if (memcmp(base, g_aic_fw_package_magic,
               sizeof(g_aic_fw_package_magic)) != 0) {
        USB_LOG_ERR("AIC firmware package magic missing at %p\r\n", package);
        return -USB_ERR_NODEV;
    }
    if ((aic_fw_get_le32(base + 8U) != AIC_FW_PACKAGE_VERSION) ||
        (aic_fw_get_le32(base + 16U) != AIC8800_FW_IMAGE_COUNT)) {
        USB_LOG_ERR("unsupported AIC firmware package format\r\n");
        return -USB_ERR_INVAL;
    }
    total_length = aic_fw_get_le32(base + 12U);
    if ((total_length < AIC_FW_PACKAGE_HEADER_SIZE) ||
        (total_length > capacity)) {
        USB_LOG_ERR("AIC firmware package length %u exceeds capacity %u\r\n",
                    (unsigned int)total_length, (unsigned int)capacity);
        return -USB_ERR_RANGE;
    }

    memset(images, 0, sizeof(images));
    for (index = 0U; index < AIC8800_FW_IMAGE_COUNT; index++) {
        const uint8_t *entry = base + AIC_FW_PACKAGE_ENTRY_OFFSET +
                               index * AIC_FW_PACKAGE_ENTRY_SIZE;
        uint32_t offset = aic_fw_get_le32(entry);
        uint32_t length = aic_fw_get_le32(entry + 4U);
        uint32_t expected_crc = aic_fw_get_le32(entry + 8U);
        uint32_t actual_crc;

        if ((offset < AIC_FW_PACKAGE_HEADER_SIZE) ||
            (offset > total_length) || (length > total_length - offset)) {
            USB_LOG_ERR("invalid AIC firmware entry %u offset=%u length=%u\r\n",
                        (unsigned int)index, (unsigned int)offset,
                        (unsigned int)length);
            return -USB_ERR_INVAL;
        }
        images[index].data = base + offset;
        images[index].length = length;
        actual_crc = aic_fw_crc32(images[index].data, length);
        if (actual_crc != expected_crc) {
            USB_LOG_ERR("AIC firmware entry %u CRC mismatch %08x/%08x\r\n",
                        (unsigned int)index, (unsigned int)actual_crc,
                        (unsigned int)expected_crc);
            return -USB_ERR_IO;
        }
    }
    if (aic_fw_store_images(images) != 0) {
        return -USB_ERR_INVAL;
    }
    USB_LOG_INFO("AIC firmware package verified at %p: %u bytes\r\n",
                 package, (unsigned int)total_length);
    return 0;
}

int aic8800_fw_set_images(const struct aic8800_fw_blob
                          images[AIC8800_FW_IMAGE_COUNT])
{
    if (images == NULL) {
        return -USB_ERR_INVAL;
    }
    if (aic_fw_store_images(images) != 0) {
        return -USB_ERR_INVAL;
    }
    USB_LOG_INFO("AIC firmware images registered (D80 U02)\r\n");
    return 0;
}

bool aic8800_fw_is_ready(void)
{
    return g_aic_fw_ready;
}

int aic8800_fw_get_image(enum aic8800_fw_image id, struct aic8800_fw_blob *blob)
{
    int result;

    if ((blob == NULL) || (id >= AIC8800_FW_IMAGE_COUNT)) {
        return -USB_ERR_INVAL;
    }
    if (!g_aic_fw_ready) {
        result = aic8800_fw_prepare();
        if (result != 0) {
            return result;
        }
    }
    if (!g_aic_fw_ready || (g_aic_fw_images[id].data == NULL)) {
        return -USB_ERR_NODEV;
    }
    *blob = g_aic_fw_images[id];
    return 0;
}

const char *aic8800_fw_image_name(enum aic8800_fw_image id)
{
    if (id >= AIC8800_FW_IMAGE_COUNT) {
        return "unknown";
    }
    return g_aic_fw_names[id];
}

__WEAK int aic8800_fw_prepare(void)
{
    if (g_aic_fw_ready) {
        return 0;
    }
#if AIC8800_FW_PACKAGE_BASE
    return aic8800_fw_set_package((const void *)(uintptr_t)AIC8800_FW_PACKAGE_BASE,
                                  AIC8800_FW_PACKAGE_CAPACITY);
#else
    USB_LOG_ERR("no AIC firmware source; call aic8800_fw_set_package() or aic8800_fw_set_images()\r\n");
    return -USB_ERR_NODEV;
#endif
}
