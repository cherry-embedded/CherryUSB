/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal AIC8800 Wi-Fi bring-up. Replace SSID/password and the firmware
 * source. Do not start ping/iperf here.
 */
#include "usbh_core.h"
#include "usbh_aic8800.h"
#include "aic8800_wifi.h"
#include "aic8800_lwip.h"

static void on_wifi(enum aic8800_wifi_event event, void *arg)
{
    (void)arg;
    switch (event) {
        case AIC8800_WIFI_EVENT_READY:
            USB_LOG_INFO("wifi ready, SSID=\"%s\"\r\n",
                         aic8800_wifi_get_sta_config()->ssid);
#if !AIC8800_WIFI_AUTO_CONNECT
            (void)aic8800_wifi_connect();
#endif
            break;
        case AIC8800_WIFI_EVENT_SCAN_DONE:
            USB_LOG_INFO("wifi scan: %u AP(s)\r\n",
                         aic8800_wifi_get_scan_count());
            break;
        case AIC8800_WIFI_EVENT_CONNECTED:
            USB_LOG_INFO("wifi associated\r\n");
            break;
        case AIC8800_WIFI_EVENT_GOT_IP:
            USB_LOG_INFO("wifi got IP, netif=%p\r\n",
                         (void *)aic8800_wifi_get_netif());
            break;
        case AIC8800_WIFI_EVENT_DISCONNECTED:
            USB_LOG_WRN("wifi disconnected\r\n");
            break;
        case AIC8800_WIFI_EVENT_CONNECT_FAILED:
            USB_LOG_ERR("wifi connect failed\r\n");
            break;
        default:
            break;
    }
}

void usbh_aic8800_wifi_template_init(uint8_t busid, uintptr_t reg_base)
{
    aic8800_wifi_init();
    if (AIC8800_WIFI_SSID[0] != '\0') {
        (void)aic8800_wifi_set_sta(AIC8800_WIFI_SSID, AIC8800_WIFI_PASSWORD);
    }
    aic8800_wifi_set_event_callback(on_wifi, NULL);
    usbh_initialize(busid, reg_base, NULL);
}
