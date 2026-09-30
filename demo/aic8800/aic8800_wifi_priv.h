/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Driver-internal AIC8800 Wi-Fi helpers. Application code should use
 * aic8800_wifi.h instead of this header.
 */
#ifndef AIC8800_WIFI_PRIV_H
#define AIC8800_WIFI_PRIV_H

#include "aic8800_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

void aic8800_wifi_reset_scan(void);
void aic8800_wifi_offer_scan_ap(const uint8_t bssid[6], uint16_t freq_mhz,
                                int8_t rssi, const char *ssid,
                                uint32_t ssid_len, const uint8_t *rsn_ie,
                                uint32_t rsn_len);
int aic8800_wifi_select_best_ap(struct aic8800_wifi_ap *ap);
int aic8800_wifi_get_pmk(uint8_t pmk[32]);
void aic8800_wifi_set_mac(const uint8_t mac[6]);
void aic8800_wifi_set_connected_ap(const struct aic8800_wifi_ap *ap);
void aic8800_wifi_mark_connecting(void);
void aic8800_wifi_on_transport_lost(void);
void aic8800_wifi_notify(enum aic8800_wifi_event event);

#ifdef __cplusplus
}
#endif

#endif /* AIC8800_WIFI_PRIV_H */
