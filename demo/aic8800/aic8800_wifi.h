/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Public AIC8800 STA API. Application code should include this header and
 * aic8800_wifi_config.h only. USB, FMAC, WPA2 and the lwIP netif stay
 * behind this facade.
 *
 * Typical bring-up:
 *   aic8800_wifi_init();
 *   aic8800_wifi_set_event_callback(...);
 *   usbh_initialize(...);
 *   wait for AIC8800_WIFI_EVENT_GOT_IP, then use lwIP sockets / the netif.
 *
 * STA mode is WPA2-PSK/CCMP. Open / WPA3 / AP mode are not implemented.
 * Event callbacks may run in the FMAC worker or the lwIP tcpip thread;
 * keep them short and do not block.
 */
#ifndef AIC8800_WIFI_H
#define AIC8800_WIFI_H

#include <stdbool.h>
#include <stdint.h>

#include "aic8800_wifi_config.h"
#include "aic8800_fw.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AIC8800_WIFI_SSID_MAX     32U
#define AIC8800_WIFI_PASSWORD_MAX 63U

enum aic8800_wifi_event {
    AIC8800_WIFI_EVENT_READY = 0,
    AIC8800_WIFI_EVENT_SCAN_DONE,
    AIC8800_WIFI_EVENT_CONNECTED,
    AIC8800_WIFI_EVENT_GOT_IP,
    AIC8800_WIFI_EVENT_DISCONNECTED,
    AIC8800_WIFI_EVENT_CONNECT_FAILED
};

enum aic8800_wifi_state {
    AIC8800_WIFI_STATE_IDLE = 0,
    AIC8800_WIFI_STATE_READY,
    AIC8800_WIFI_STATE_SCANNING,
    AIC8800_WIFI_STATE_CONNECTING,
    AIC8800_WIFI_STATE_CONNECTED,
    AIC8800_WIFI_STATE_GOT_IP
};

struct aic8800_wifi_ap {
    uint8_t bssid[6];
    uint16_t freq_mhz;
    uint8_t band;
    int8_t rssi;
    uint8_t ssid[AIC8800_WIFI_SSID_MAX + 1U];
    uint8_t ssid_len;
    uint8_t rsn_ie[AIC8800_WIFI_RSN_IE_MAX];
    uint8_t rsn_len;
};

struct aic8800_wifi_sta_config {
    char ssid[AIC8800_WIFI_SSID_MAX + 1U];
    char password[AIC8800_WIFI_PASSWORD_MAX + 1U];
    uint8_t pmk[32];
    uint8_t bssid[6];
    uint8_t band;
    bool pmk_valid;
};

struct netif;

typedef void (*aic8800_wifi_event_cb_t)(enum aic8800_wifi_event event,
                                        void *arg);

/*
 * Load STA defaults from aic8800_wifi_config.h and register the ZeroCD
 * mode-switch. Call once before usbh_initialize().
 */
int aic8800_wifi_init(void);

void aic8800_wifi_set_event_callback(aic8800_wifi_event_cb_t callback,
                                     void *arg);

/* Replaces SSID/password for the next association. Password must be 8..63. */
int aic8800_wifi_set_sta(const char *ssid, const char *password);
int aic8800_wifi_set_sta_ex(const struct aic8800_wifi_sta_config *config);
const struct aic8800_wifi_sta_config *aic8800_wifi_get_sta_config(void);

enum aic8800_wifi_state aic8800_wifi_get_state(void);
bool aic8800_wifi_is_connected(void);
int aic8800_wifi_get_mac(uint8_t mac[6]);
int aic8800_wifi_get_ip4(uint8_t ip[4], uint8_t mask[4], uint8_t gateway[4]);
int aic8800_wifi_get_connected_ap(struct aic8800_wifi_ap *ap);
struct netif *aic8800_wifi_get_netif(void);

/* Snapshot after AIC8800_WIFI_EVENT_SCAN_DONE. */
unsigned int aic8800_wifi_get_scan_count(void);
int aic8800_wifi_get_scan_ap(unsigned int index, struct aic8800_wifi_ap *ap);

/*
 * Manual radio control. Safe after AIC8800_WIFI_EVENT_READY.
 * If AIC8800_WIFI_AUTO_CONNECT is 1, attach already queues connect().
 */
int aic8800_wifi_scan(void);
int aic8800_wifi_connect(void);
int aic8800_wifi_disconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* AIC8800_WIFI_H */
