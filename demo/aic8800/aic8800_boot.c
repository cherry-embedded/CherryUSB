/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * AIC8800D80 U02 USB BootROM firmware loader for CherryUSB/FreeRTOS.
 */
#include "aic8800_usb.h"
#include "aic8800_fmac.h"
#include "aic8800_fw.h"

#include <stddef.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "usb_config.h"
#include "usb_osal.h"
#include "usbh_hub.h"

#ifndef AIC8800_BOOT_OSAL_PRIO
#define AIC8800_BOOT_OSAL_PRIO ((uint32_t)(configMAX_PRIORITIES - 1U - 2U))
#endif

#undef USB_DBG_TAG
#define USB_DBG_TAG "aic8800.boot"
#include "usb_log.h"

#define AIC_USB_TYPE_CONFIG          0x10U
#define AIC_USB_TYPE_COMMAND         0x11U
#define AIC_TASK_DBG                    1U
#define AIC_DRIVER_TASK               100U
#define AIC_DBG_MSG(index) ((uint16_t)((AIC_TASK_DBG << 10) | (index)))
#define AIC_DBG_MEM_READ_REQ    AIC_DBG_MSG(0U)
#define AIC_DBG_MEM_READ_CFM    AIC_DBG_MSG(1U)
#define AIC_DBG_MEM_WRITE_REQ   AIC_DBG_MSG(2U)
#define AIC_DBG_MEM_WRITE_CFM   AIC_DBG_MSG(3U)
#define AIC_DBG_MEM_BLOCK_REQ   AIC_DBG_MSG(11U)
#define AIC_DBG_MEM_BLOCK_CFM   AIC_DBG_MSG(12U)
#define AIC_DBG_START_APP_REQ   AIC_DBG_MSG(13U)

#define AIC_D80_CHIP_ID_REG       0x40500000UL
#define AIC_D80_BOOT_PRODUCT_ID       0x8d80U
#define AIC_D80_FMAC_BASE          0x00120000UL
#define AIC_D80_SETTING_OFFSET     0x00000198UL
#define AIC_D80_DEFAULT_PATCH_BUF  0x001D7000UL
#define AIC_START_APP_AUTO                  1UL

#define AIC_PATCH_TAG              "AICBT_PT_TAG"
#define AIC_PATCH_INFO_TYPE                  0U
#define AIC_PATCH_BTMODE_TYPE                3U
#define AIC_PATCH_POWER_ON_TYPE              4U
#define AIC_PATCH_VERSION_TYPE               6U
#define AIC_PATCH_MAGIC             0x48435450UL
#define AIC_PATCH_MAGIC_2           0x50544348UL
#define AIC_PATCH_STRUCT_BLOCK_SIZE_OFFSET   48U

#define AIC_BOOT_BLOCK_SIZE                1024U
#define AIC_BOOT_BLOCK_REQUEST_SIZE        1032U
#define AIC_BOOT_TX_SIZE                   1048U
#define AIC_BOOT_RX_SIZE                    512U
#define AIC_BOOT_TIMEOUT_MS                3000U
#define AIC_BOOT_PROGRESS_STEP          (64U * 1024U)
/* The D80 reference loader allows the Bluetooth power domain and RF state
 * written by patch block type 4 to settle before FMAC is uploaded/started.
 * A 1 ms delay leaves HCI register commands operational while the radio can
 * remain silent, which presents as a successful LE scan with no reports. */
#define AIC_BT_POWER_ON_DELAY_MS           100U

struct aic_patch_info {
    uint32_t adid_address;
    uint32_t patch_address;
    uint32_t ext_count;
    uint32_t ext_id;
    uint32_t ext_address;
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_boot_tx[AIC_BOOT_TX_SIZE];

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_boot_rx[AIC_BOOT_RX_SIZE];

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX
static uint8_t g_aic_boot_block[AIC_BOOT_BLOCK_REQUEST_SIZE];

static volatile bool g_aic_boot_task_running;

static uint16_t aic_get_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t aic_get_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static void aic_put_le16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void aic_put_le32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static int aic_find_confirmation(const uint8_t *buffer, uint32_t length,
                                 uint16_t expected_id, void *response,
                                 uint32_t response_capacity,
                                 uint32_t *response_length)
{
    const uint8_t *cursor = buffer;
    uint32_t remaining = length;

    if (response_length != NULL) {
        *response_length = 0U;
    }
    while (remaining >= 16U) {
        uint16_t packet_length = aic_get_le16(cursor);
        uint8_t type = cursor[2] & 0x7fU;
        uint32_t raw_length;
        uint32_t record_length;

        if ((packet_length == 0U) && (type == 0U)) {
            break;
        }
        raw_length = ((type & AIC_USB_TYPE_CONFIG) != 0U) ?
                     (uint32_t)packet_length + 4U :
                     (uint32_t)packet_length + 8U;
        if ((raw_length < 16U) || (raw_length > remaining)) {
            return -USB_ERR_INVAL;
        }
        record_length = (raw_length + 3U) & ~3U;
        if (record_length > remaining) {
            record_length = raw_length;
        }

        if (type == AIC_USB_TYPE_COMMAND) {
            uint16_t message_id = aic_get_le16(cursor + 4U);
            uint16_t parameter_length = aic_get_le16(cursor + 10U);

            if (parameter_length > (raw_length - 16U)) {
                return -USB_ERR_INVAL;
            }
            if (message_id == expected_id) {
                if ((response != NULL) &&
                    (parameter_length > response_capacity)) {
                    return -USB_ERR_RANGE;
                }
                if ((parameter_length != 0U) && (response != NULL)) {
                    memcpy(response, cursor + 16U, parameter_length);
                }
                if (response_length != NULL) {
                    *response_length = parameter_length;
                }
                return 0;
            }
        }
        cursor += record_length;
        remaining -= record_length;
    }
    return -USB_ERR_NODEV;
}

static int aic_debug_command(struct aic8800_usb_device *device,
                             uint16_t request_id, uint16_t confirmation_id,
                             const void *request, uint32_t request_length,
                             void *response, uint32_t response_capacity,
                             uint32_t *response_length)
{
    uint32_t frame_length = 16U + request_length;
    int result;

    if ((frame_length > sizeof(g_aic_boot_tx)) ||
        ((request_length != 0U) && (request == NULL))) {
        return -USB_ERR_INVAL;
    }
    memset(g_aic_boot_tx, 0, frame_length);
    aic_put_le16(g_aic_boot_tx, (uint16_t)(request_length + 12U));
    g_aic_boot_tx[2] = AIC_USB_TYPE_COMMAND;
    aic_put_le16(g_aic_boot_tx + 8U, request_id);
    aic_put_le16(g_aic_boot_tx + 10U, AIC_TASK_DBG);
    aic_put_le16(g_aic_boot_tx + 12U, AIC_DRIVER_TASK);
    aic_put_le16(g_aic_boot_tx + 14U, (uint16_t)request_length);
    if (request_length != 0U) {
        memcpy(g_aic_boot_tx + 16U, request, request_length);
    }

    result = aic8800_usb_bulk_send(device, g_aic_boot_tx, frame_length,
                                   AIC_BOOT_TIMEOUT_MS, true);
    if (result != (int)frame_length) {
        return (result < 0) ? result : -USB_ERR_IO;
    }
    if (confirmation_id == 0U) {
        return 0;
    }

    memset(g_aic_boot_rx, 0, sizeof(g_aic_boot_rx));
    result = aic8800_usb_bulk_receive(device, g_aic_boot_rx,
                                      sizeof(g_aic_boot_rx),
                                      AIC_BOOT_TIMEOUT_MS, true);
    if (result < 0) {
        return result;
    }
    return aic_find_confirmation(g_aic_boot_rx, (uint32_t)result,
                                 confirmation_id, response,
                                 response_capacity, response_length);
}

static int aic_mem_read(struct aic8800_usb_device *device,
                        uint32_t address, uint32_t *value)
{
    uint8_t request[4];
    uint8_t response[8];
    uint32_t response_length = 0U;
    int result;

    aic_put_le32(request, address);
    result = aic_debug_command(device, AIC_DBG_MEM_READ_REQ,
                               AIC_DBG_MEM_READ_CFM, request,
                               sizeof(request), response, sizeof(response),
                               &response_length);
    if (result < 0) {
        return result;
    }
    if ((response_length < sizeof(response)) ||
        (aic_get_le32(response) != address)) {
        return -USB_ERR_INVAL;
    }
    *value = aic_get_le32(response + 4U);
    return 0;
}

static int aic_mem_write(struct aic8800_usb_device *device,
                         uint32_t address, uint32_t value)
{
    uint8_t request[8];

    aic_put_le32(request, address);
    aic_put_le32(request + 4U, value);
    return aic_debug_command(device, AIC_DBG_MEM_WRITE_REQ,
                             AIC_DBG_MEM_WRITE_CFM, request,
                             sizeof(request), NULL, 0U, NULL);
}

static int aic_mem_block_write(struct aic8800_usb_device *device,
                               uint32_t address, const uint8_t *data,
                               uint32_t length)
{
    uint8_t response[4];
    uint32_t response_length = 0U;
    int result;

    if ((data == NULL) || (length == 0U) ||
        (length > AIC_BOOT_BLOCK_SIZE)) {
        return -USB_ERR_INVAL;
    }
    memset(g_aic_boot_block, 0, sizeof(g_aic_boot_block));
    aic_put_le32(g_aic_boot_block, address);
    aic_put_le32(g_aic_boot_block + 4U, length);
    memcpy(g_aic_boot_block + 8U, data, length);

    result = aic_debug_command(device, AIC_DBG_MEM_BLOCK_REQ,
                               AIC_DBG_MEM_BLOCK_CFM,
                               g_aic_boot_block,
                               AIC_BOOT_BLOCK_REQUEST_SIZE,
                               response, sizeof(response),
                               &response_length);
    if (result < 0) {
        return result;
    }
    if ((response_length >= 4U) && (aic_get_le32(response) != 0U)) {
        return -USB_ERR_IO;
    }
    return 0;
}

static int aic_upload_blob(struct aic8800_usb_device *device,
                           const char *name, const uint8_t *data,
                           uint32_t length, uint32_t address)
{
    uint32_t offset = 0U;
    uint32_t next_progress = AIC_BOOT_PROGRESS_STEP;
    int result = 0;

    USB_LOG_INFO("uploading %s: %u bytes -> 0x%08x\r\n", name,
                 (unsigned int)length, (unsigned int)address);
    while ((offset < length) && device->online) {
        uint32_t chunk = length - offset;

        if (chunk > AIC_BOOT_BLOCK_SIZE) {
            chunk = AIC_BOOT_BLOCK_SIZE;
        }
        result = aic_mem_block_write(device, address + offset,
                                     data + offset, chunk);
        if (result < 0) {
            USB_LOG_ERR("upload %s failed at 0x%x: %d\r\n", name,
                        (unsigned int)offset, result);
            return result;
        }
        offset += chunk;
        if ((offset >= next_progress) || (offset == length)) {
            USB_LOG_INFO("%s progress: %u/%u\r\n", name,
                         (unsigned int)offset, (unsigned int)length);
            next_progress += AIC_BOOT_PROGRESS_STEP;
        }
    }
    if (!device->online) {
        return -USB_ERR_NOTCONN;
    }

    if ((length >= 4U) && ((length & 3U) == 0U)) {
        uint32_t target_first;
        uint32_t target_last;
        uint32_t source_first = aic_get_le32(data);
        uint32_t source_last = aic_get_le32(data + length - 4U);

        result = aic_mem_read(device, address, &target_first);
        if (result == 0) {
            result = aic_mem_read(device, address + length - 4U,
                                  &target_last);
        }
        if ((result < 0) || (source_first != target_first) ||
            (source_last != target_last)) {
            USB_LOG_ERR("verify %s failed\r\n", name);
            return (result < 0) ? result : -USB_ERR_IO;
        }
    }
    USB_LOG_INFO("uploaded %s\r\n", name);
    return 0;
}

static int aic_parse_patch_info(const uint8_t *table, uint32_t length,
                                struct aic_patch_info *info)
{
    const uint8_t *data;
    uint32_t count;

    if ((table == NULL) || (info == NULL) || (length < 40U) ||
        (memcmp(table, AIC_PATCH_TAG, sizeof(AIC_PATCH_TAG)) != 0) ||
        (aic_get_le32(table + 32U) != AIC_PATCH_INFO_TYPE)) {
        return -USB_ERR_INVAL;
    }
    count = aic_get_le32(table + 36U);
    if ((count < 4U) || (count > ((length - 40U) / 8U))) {
        return -USB_ERR_INVAL;
    }
    data = table + 40U;
    memset(info, 0, sizeof(*info));
    info->adid_address = aic_get_le32(data + 4U);
    info->patch_address = aic_get_le32(data + 12U);
    if (count >= 5U) {
        info->ext_count = aic_get_le32(data + 36U);
        if ((info->ext_count > 1U) ||
            ((5U + info->ext_count) > count)) {
            return -USB_ERR_RANGE;
        }
        if (info->ext_count != 0U) {
            info->ext_id = aic_get_le32(data + 40U);
            info->ext_address = aic_get_le32(data + 44U);
        }
    }
    return 0;
}

static uint32_t aic_btmode_value(uint32_t index, uint32_t original)
{
    static const uint32_t values[9] = {
        1U, 0xffffffffUL, 0U, 5U, 1U, 1500000U, 1U, 0U, 0x00006f2fU
    };

    return (index < 9U) ? values[index] : original;
}

static int aic_apply_patch_table(struct aic8800_usb_device *device,
                                 const uint8_t *table, uint32_t length)
{
    uint32_t offset = 16U;

    while (offset < length) {
        const uint8_t *data;
        uint32_t type;
        uint32_t count;
        uint32_t index;

        if ((length - offset) < 24U) {
            return -USB_ERR_INVAL;
        }
        type = aic_get_le32(table + offset + 16U);
        count = aic_get_le32(table + offset + 20U);
        if (count > ((length - offset - 24U) / 8U)) {
            return -USB_ERR_INVAL;
        }
        data = table + offset + 24U;
        if (type == AIC_PATCH_VERSION_TYPE) {
            offset += 24U + count * 8U;
            continue;
        }

        USB_LOG_INFO("applying patch block type=%u pairs=%u\r\n",
                     (unsigned int)type, (unsigned int)count);
        for (index = 0U; index < count; index++) {
            uint32_t address = aic_get_le32(data + index * 8U);
            uint32_t value = aic_get_le32(data + index * 8U + 4U);
            int result;

            if (type == AIC_PATCH_BTMODE_TYPE) {
                value = aic_btmode_value(index, value);
            }
            result = aic_mem_write(device, address, value);
            if (result < 0) {
                USB_LOG_ERR("patch pair %u failed: %d\r\n",
                            (unsigned int)index, result);
                return result;
            }
        }
        if (type == AIC_PATCH_POWER_ON_TYPE) {
            USB_LOG_INFO("waiting %u ms for Bluetooth power-on\r\n",
                         (unsigned int)AIC_BT_POWER_ON_DELAY_MS);
            usb_osal_msleep(AIC_BT_POWER_ON_DELAY_MS);
        }
        offset += 24U + count * 8U;
    }
    return (offset == length) ? 0 : -USB_ERR_INVAL;
}

static int aic_configure_fmac(struct aic8800_usb_device *device)
{
    static const uint32_t offsets[3] = { 0x00b4U, 0x0170U, 0x0188U };
    static const uint32_t values[3] = {
#if AIC8800_FMAC_USE_5GHZ
        0xf3010001UL,
#else
        0xf3010000UL,
#endif
        0x00010000UL | AIC8800_FMAC_RX_AGGREGATE_COUNT,
        0x00000001UL
    };
    uint32_t setting = AIC_D80_FMAC_BASE + AIC_D80_SETTING_OFFSET;
    uint32_t config_base;
    uint32_t patch_struct;
    uint32_t patch_buffer = AIC_D80_DEFAULT_PATCH_BUF;
    uint32_t version;
    uint32_t index;
    int result;

    result = aic_mem_read(device, setting, &config_base);
    if (result == 0) {
        result = aic_mem_read(device, setting + 8U, &patch_struct);
    }
    if (result == 0) {
        result = aic_mem_read(device, AIC_D80_FMAC_BASE + 0x1cU, &version);
    }
    if ((result == 0) && (version > 0x06090100UL)) {
        result = aic_mem_read(device, setting + 12U, &patch_buffer);
    }
    if (result < 0) {
        return result;
    }
    USB_LOG_INFO("FMAC version=0x%08x config=0x%08x patch=0x%08x\r\n",
                 (unsigned int)version, (unsigned int)config_base,
                 (unsigned int)patch_buffer);
    USB_LOG_INFO("FMAC profile: %s, USB RX aggregate=%u buffer=%u bytes\r\n",
                 AIC8800_FMAC_USE_5GHZ ? "5 GHz" : "2.4 GHz",
                 (unsigned int)AIC8800_FMAC_RX_AGGREGATE_COUNT,
                 (unsigned int)AIC8800_FMAC_RX_BUFFER_SIZE);

    result = aic_mem_write(device, patch_struct, AIC_PATCH_MAGIC);
    if (result == 0) {
        result = aic_mem_write(device, patch_struct + 8U,
                               AIC_PATCH_MAGIC_2);
    }
    if (result == 0) {
        result = aic_mem_write(device, patch_struct + 4U, patch_buffer);
    }
    if (result == 0) {
        result = aic_mem_write(device, patch_struct + 12U, 3U);
    }
    for (index = 0U; (result == 0) && (index < 3U); index++) {
        result = aic_mem_write(device, patch_buffer + index * 8U,
                               config_base + offsets[index]);
        if (result == 0) {
            result = aic_mem_write(device, patch_buffer + index * 8U + 4U,
                                   values[index]);
        }
    }
    for (index = 0U; (result == 0) && (index < 4U); index++) {
        result = aic_mem_write(device,
                               patch_struct +
                               AIC_PATCH_STRUCT_BLOCK_SIZE_OFFSET +
                               index * 4U, 0U);
    }
    return result;
}

static int aic_start_fmac(struct aic8800_usb_device *device)
{
    uint8_t request[8];

    aic_put_le32(request, AIC_D80_FMAC_BASE);
    aic_put_le32(request + 4U, AIC_START_APP_AUTO);
    return aic_debug_command(device, AIC_DBG_START_APP_REQ, 0U,
                             request, sizeof(request), NULL, 0U, NULL);
}

static int aic_download_d80_firmware(struct aic8800_usb_device *device)
{
    struct aic8800_fw_blob table;
    struct aic8800_fw_blob adid;
    struct aic8800_fw_blob patch;
    struct aic8800_fw_blob ext0;
    struct aic8800_fw_blob fmac;
    struct aic_patch_info info;
    TickType_t transfer_started;
    int result;

    result = aic8800_fw_get_image(AIC8800_FW_PATCH_TABLE, &table);
    if (result == 0) {
        result = aic8800_fw_get_image(AIC8800_FW_ADID, &adid);
    }
    if (result == 0) {
        result = aic8800_fw_get_image(AIC8800_FW_PATCH, &patch);
    }
    if (result == 0) {
        result = aic8800_fw_get_image(AIC8800_FW_PATCH_EXT0, &ext0);
    }
    if (result == 0) {
        result = aic8800_fw_get_image(AIC8800_FW_FMAC, &fmac);
    }
    if (result < 0) {
        USB_LOG_ERR("AIC firmware images unavailable: %d\r\n", result);
        return result;
    }
    result = aic_parse_patch_info(table.data, table.length, &info);
    if (result < 0) {
        USB_LOG_ERR("invalid D80 patch table: %d\r\n", result);
        return result;
    }
    USB_LOG_INFO("patch destinations adid=0x%08x patch=0x%08x ext%u=0x%08x\r\n",
                 (unsigned int)info.adid_address,
                 (unsigned int)info.patch_address,
                 (unsigned int)info.ext_id,
                 (unsigned int)info.ext_address);
    USB_LOG_INFO("firmware upload block size: %u bytes\r\n",
                 (unsigned int)AIC_BOOT_BLOCK_SIZE);
    transfer_started = xTaskGetTickCount();

    result = aic_upload_blob(device, aic8800_fw_image_name(AIC8800_FW_ADID),
                             adid.data, adid.length, info.adid_address);
    if (result == 0) {
        result = aic_upload_blob(device, aic8800_fw_image_name(AIC8800_FW_PATCH),
                                 patch.data, patch.length, info.patch_address);
    }
    if ((result == 0) && (info.ext_count == 1U) &&
        (info.ext_id == 0U)) {
        result = aic_upload_blob(device,
                                 aic8800_fw_image_name(AIC8800_FW_PATCH_EXT0),
                                 ext0.data, ext0.length, info.ext_address);
    }
    if (result == 0) {
        result = aic_apply_patch_table(device, table.data, table.length);
    }
    if (result == 0) {
        result = aic_upload_blob(device, aic8800_fw_image_name(AIC8800_FW_FMAC),
                                 fmac.data, fmac.length, AIC_D80_FMAC_BASE);
    }
    if (result == 0) {
        result = aic_configure_fmac(device);
    }
    if (result == 0) {
        USB_LOG_INFO("starting D80 FMAC at 0x%08x\r\n",
                     (unsigned int)AIC_D80_FMAC_BASE);
        result = aic_start_fmac(device);
    }
    if (result == 0) {
        USB_LOG_INFO("firmware upload and patch complete in %u ms\r\n",
                     (unsigned int)((xTaskGetTickCount() - transfer_started) *
                                    portTICK_PERIOD_MS));
    }
    return result;
}

static void aic8800_boot_task(void *argument)
{
    struct aic8800_usb_device *device = argument;
#ifdef CONFIG_USBHOST_HUB_FORCE_REENUMERATE
    struct usbh_hub *parent;
    uint8_t port;
#endif
    uint32_t chip_register = 0U;
    int result;

#ifdef CONFIG_USBHOST_HUB_FORCE_REENUMERATE
    parent = device->hport->parent;
    port = device->hport->port;
#endif
    USB_LOG_INFO("probing AIC8800D80 BootROM command channel\r\n");
    result = aic_mem_read(device, AIC_D80_CHIP_ID_REG, &chip_register);
    if (result == 0) {
        uint8_t chip_id = (uint8_t)(chip_register >> 16);

        USB_LOG_INFO("D80 chip register=0x%08x revision=0x%02x high=%u mcu=%u\r\n",
                     (unsigned int)chip_register,
                     chip_id & 0x3fU,
                     (chip_id & 0xc0U) == 0xc0U,
                     ((chip_register >> 25) & 1U) == 0U);
        if ((chip_id & 0xc0U) == 0xc0U) {
            USB_LOG_ERR("this build contains only non-H D80 firmware\r\n");
            result = -USB_ERR_NOTSUPP;
        }
    }
    if (result == 0) {
        result = aic_download_d80_firmware(device);
    }
    if (result == 0) {
        USB_LOG_INFO("firmware start command sent; waiting for runtime USB\r\n");
        usb_osal_msleep(500U);
        /* The transport object is reused when the same physical adapter
         * reconnects as 8d81. Only force a port reset if it still describes
         * the original boot interface; otherwise this late fallback would
         * reset a healthy runtime device back into 8d80 BootROM. */
        if (device->online && !device->runtime &&
            (device->product_id == AIC_D80_BOOT_PRODUCT_ID)) {
#ifdef CONFIG_USBHOST_HUB_FORCE_REENUMERATE
            result = usbh_hub_force_reenumerate(parent, port);
            if (result == 0) {
                USB_LOG_INFO("runtime hub-port re-enumeration requested\r\n");
            }
#else
            USB_LOG_WRN("runtime reconnect not forced; enable CONFIG_USBHOST_HUB_FORCE_REENUMERATE\r\n");
#endif
        }
    }
    if (result < 0) {
        USB_LOG_ERR("D80 firmware loading failed: %d\r\n", result);
    }

    g_aic_boot_task_running = false;
    usb_osal_thread_delete(NULL);
}

void aic8800_usb_attached(struct aic8800_usb_device *device)
{
    if (device == NULL) {
        return;
    }
    if (device->runtime) {
        USB_LOG_INFO("AIC runtime transport reached; starting FMAC attach\r\n");
        aic8800_fmac_attach(device);
        return;
    }
    if (device->family != AIC8800_USB_FAMILY_D80) {
        return;
    }
    if (g_aic_boot_task_running) {
        USB_LOG_WRN("firmware loader task is already running\r\n");
        return;
    }

    g_aic_boot_task_running = true;
    if (usb_osal_thread_create("aic_boot", 768U * 4U, AIC8800_BOOT_OSAL_PRIO,
                               aic8800_boot_task, device) == NULL) {
        g_aic_boot_task_running = false;
        USB_LOG_ERR("failed to create firmware loader task\r\n");
    }
}

void aic8800_usb_detaching(struct aic8800_usb_device *device)
{
    if ((device != NULL) && device->runtime) {
        aic8800_fmac_detach(device);
    }
}
