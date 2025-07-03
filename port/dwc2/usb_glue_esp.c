/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "esp_idf_version.h"
#include "esp_intr_alloc.h"
#include "esp_private/usb_phy.h"
#include "soc/periph_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usbd_core.h"
#include "usbh_core.h"
#include "usb_dwc2_param.h"

#ifdef CONFIG_IDF_TARGET_ESP32S2
#define DEFAULT_CPU_FREQ_MHZ    CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_USB_INTR_SOURCE ETS_USB_INTR_SOURCE
#elif CONFIG_IDF_TARGET_ESP32S3
#define DEFAULT_CPU_FREQ_MHZ    CONFIG_ESP32S3_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_USB_INTR_SOURCE ETS_USB_INTR_SOURCE
#elif CONFIG_IDF_TARGET_ESP32P4
#define DEFAULT_CPU_FREQ_MHZ    CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ
#define DEFAULT_USB_INTR_SOURCE ETS_USB_OTG_INTR_SOURCE
#else
#define DEFAULT_CPU_FREQ_MHZ 160
#endif

uint32_t SystemCoreClock = (DEFAULT_CPU_FREQ_MHZ * 1000 * 1000);
static usb_phy_handle_t s_phy_handle = NULL;
static intr_handle_t s_interrupt_handle = NULL;

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
const struct dwc2_user_params param_fs = {
    .phy_type = DWC2_PHY_TYPE_PARAM_FS,
    .device_dma_enable = true,
    .device_dma_desc_enable = false,
    .device_rx_fifo_size = (200 - 16 * 7),
    .device_tx_fifo_size = {
        [0] = 16, // 64 byte
        [1] = 16, // 64 byte
        [2] = 16, // 64 byte
        [3] = 16, // 64 byte
        [4] = 16, // 64 byte
        [5] = 16, // 64 byte
        [6] = 16, // 64 byte
        [7] = 0,
        [8] = 0,
        [9] = 0,
        [10] = 0,
        [11] = 0,
        [12] = 0,
        [13] = 0,
        [14] = 0,
        [15] = 0 },

    .host_dma_desc_enable = false,
    .host_rx_fifo_size = 80,
    .host_nperio_tx_fifo_size = 60, // 240 byte
    .host_perio_tx_fifo_size = 60,  // 240 byte
};
#elif CONFIG_IDF_TARGET_ESP32P4
const struct dwc2_user_params param_fs = {
    .phy_type = DWC2_PHY_TYPE_PARAM_FS,
    .device_dma_enable = true,
    .device_dma_desc_enable = false,
    .device_rx_fifo_size = (200 - 16 * 7),
    .device_tx_fifo_size = {
        [0] = 16, // 64 byte
        [1] = 16, // 64 byte
        [2] = 16, // 64 byte
        [3] = 16, // 64 byte
        [4] = 16, // 64 byte
        [5] = 16, // 64 byte
        [6] = 16, // 64 byte
        [7] = 0,
        [8] = 0,
        [9] = 0,
        [10] = 0,
        [11] = 0,
        [12] = 0,
        [13] = 0,
        [14] = 0,
        [15] = 0 },

    .host_dma_desc_enable = false,
    .host_rx_fifo_size = (200 - 60 - 60),
    .host_nperio_tx_fifo_size = 60, // 240 byte
    .host_perio_tx_fifo_size = 60,  // 240 byte
};

const struct dwc2_user_params param_hs = {
    .phy_type = DWC2_PHY_TYPE_PARAM_UTMI,
    .device_dma_enable = true,
    .device_dma_desc_enable = false,
    .device_rx_fifo_size = (896 - 16 - 128 - 128 - 128 - 128 - 16 - 16),
    .device_tx_fifo_size = {
        [0] = 16,  // 64 byte
        [1] = 128, // 512 byte
        [2] = 128, // 512 byte
        [3] = 128, // 512 byte
        [4] = 128, // 512 byte
        [5] = 16,  // 64 byte
        [6] = 16,  // 64 byte
        [7] = 0,
        [8] = 0,
        [9] = 0,
        [10] = 0,
        [11] = 0,
        [12] = 0,
        [13] = 0,
        [14] = 0,
        [15] = 0 },

    .host_dma_desc_enable = false,
    .host_rx_fifo_size = (896 - 128 - 128),
    .host_nperio_tx_fifo_size = 128, // 512 byte
    .host_perio_tx_fifo_size = 128,  // 512 byte
};
#endif

static void usb_dc_interrupt_cb(void *arg_pv)
{
    extern void USBD_IRQHandler(uint8_t busid);
    USBD_IRQHandler(0);
}

void usb_dc_low_level_init(uint8_t busid)
{
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
        .otg_mode = USB_OTG_MODE_DEVICE,
#if CONFIG_IDF_TARGET_ESP32P4 // ESP32-P4 has 2 USB-DWC peripherals, each with its dedicated PHY. We support HS+UTMI only ATM.
        .target = USB_PHY_TARGET_UTMI,
#else
        .target = USB_PHY_TARGET_INT,
#endif
    };

    esp_err_t ret = usb_new_phy(&phy_config, &s_phy_handle);
    if (ret != ESP_OK) {
        USB_LOG_ERR("USB Phy Init Failed!\r\n");
        return;
    }

    // TODO: Check when to enable interrupt
    ret = esp_intr_alloc(DEFAULT_USB_INTR_SOURCE, ESP_INTR_FLAG_LOWMED, usb_dc_interrupt_cb, 0, &s_interrupt_handle);
    if (ret != ESP_OK) {
        USB_LOG_ERR("USB Interrupt Init Failed!\r\n");
        return;
    }
    USB_LOG_INFO("cherryusb, version: " CHERRYUSB_VERSION_STR "\r\n");
}

void usb_dc_low_level_deinit(uint8_t busid)
{
    if (s_interrupt_handle) {
        esp_intr_free(s_interrupt_handle);
        s_interrupt_handle = NULL;
    }
    if (s_phy_handle) {
        usb_del_phy(s_phy_handle);
        s_phy_handle = NULL;
    }
}

static void usb_hc_interrupt_cb(void *arg_pv)
{
    extern void USBH_IRQHandler(uint8_t busid);
    USBH_IRQHandler(0);
}

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    // Host Library defaults to internal PHY
    usb_phy_config_t phy_config = {
        .controller = USB_PHY_CTRL_OTG,
#if CONFIG_IDF_TARGET_ESP32P4 // ESP32-P4 has 2 USB-DWC peripherals, each with its dedicated PHY. We support HS+UTMI only ATM.
        .target = USB_PHY_TARGET_UTMI,
#else
        .target = USB_PHY_TARGET_INT,
#endif
        .otg_mode = USB_OTG_MODE_HOST,
        .otg_speed = USB_PHY_SPEED_UNDEFINED, // In Host mode, the speed is determined by the connected device
        .ext_io_conf = NULL,
        .otg_io_conf = NULL,
    };

    esp_err_t ret = usb_new_phy(&phy_config, &s_phy_handle);
    if (ret != ESP_OK) {
        USB_LOG_ERR("USB Phy Init Failed!\r\n");
        return;
    }

    // TODO: Check when to enable interrupt
    ret = esp_intr_alloc(DEFAULT_USB_INTR_SOURCE, ESP_INTR_FLAG_LOWMED, usb_hc_interrupt_cb, 0, &s_interrupt_handle);
    if (ret != ESP_OK) {
        USB_LOG_ERR("USB Interrupt Init Failed!\r\n");
        return;
    }
    USB_LOG_INFO("cherryusb, version: " CHERRYUSB_VERSION_STR "\r\n");
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    if (s_interrupt_handle) {
        esp_intr_free(s_interrupt_handle);
        s_interrupt_handle = NULL;
    }
    if (s_phy_handle) {
        usb_del_phy(s_phy_handle);
        s_phy_handle = NULL;
    }
}

void dwc2_get_user_params(uint32_t reg_base, struct dwc2_user_params *params)
{
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    memcpy(params, &param_fs, sizeof(struct dwc2_user_params));
#elif CONFIG_IDF_TARGET_ESP32P4
    if (reg_base == 0x50000000UL) {
        memcpy(params, &param_hs, sizeof(struct dwc2_user_params));
    } else {
        memcpy(params, &param_fs, sizeof(struct dwc2_user_params));
    }
#endif
}

void usbd_dwc2_delay_ms(uint8_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

#ifdef CONFIG_USB_DCACHE_ENABLE
#include "esp_cache.h"

void usb_dcache_clean(uintptr_t addr, size_t size)
{
    if (size == 0)
        return;

    esp_cache_msync((void *)addr, size, ESP_CACHE_MSYNC_FLAG_TYPE_DATA | ESP_CACHE_MSYNC_FLAG_DIR_C2M);
}

void usb_dcache_invalidate(uintptr_t addr, size_t size)
{
    if (size == 0)
        return;

    esp_cache_msync((void *)addr, size, ESP_CACHE_MSYNC_FLAG_TYPE_DATA | ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}

void usb_dcache_flush(uintptr_t addr, size_t size)
{
    if (size == 0)
        return;

    esp_cache_msync((void *)addr, size, ESP_CACHE_MSYNC_FLAG_TYPE_DATA | ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_DIR_M2C);
}
#endif