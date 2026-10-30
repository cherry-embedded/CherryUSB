/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Application-facing AIC8800 STA facade: credentials, scan table, events.
 */
#include "aic8800_wifi.h"
#include "aic8800_wifi_priv.h"
#include "aic8800_fw.h"
#include "aic8800_usb.h"
#include "aic8800_fmac.h"
#include "aic8800_lwip.h"
#include "aic8800_wpa2.h"

#include <string.h>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "usb_errno.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "aic8800.wifi"
#include "usb_log.h"

static const uint8_t g_aic_default_rsn_ie[] = {
    0x30U, 0x14U, 0x01U, 0x00U, 0x00U, 0x0fU, 0xacU, 0x04U,
    0x01U, 0x00U, 0x00U, 0x0fU, 0xacU, 0x04U, 0x01U, 0x00U,
    0x00U, 0x0fU, 0xacU, 0x02U, 0x0cU, 0x00U,
};

static struct aic8800_wifi_sta_config g_aic_sta;
static struct aic8800_wifi_ap g_aic_scan[AIC8800_WIFI_SCAN_MAX];
static struct aic8800_wifi_ap g_aic_connected_ap;
static unsigned int g_aic_scan_count;
static uint8_t g_aic_mac[6];
static bool g_aic_mac_valid;
static bool g_aic_has_connected_ap;
static bool g_aic_sta_configured;
static enum aic8800_wifi_state g_aic_state;
static enum aic8800_wifi_event g_aic_last_event = (enum aic8800_wifi_event)0xffU;
static aic8800_wifi_event_cb_t g_aic_event_cb;
static void *g_aic_event_arg;
static uint8_t g_aic_pmk_cache[32];
static char g_aic_pmk_ssid[AIC8800_WIFI_SSID_MAX + 1U];
static char g_aic_pmk_password[AIC8800_WIFI_PASSWORD_MAX + 1U];
static bool g_aic_pmk_cache_valid;

static size_t aic_bounded_len(const char *text, size_t maximum)
{
    size_t length = 0U;

    if (text == NULL) {
        return 0U;
    }
    while ((length < maximum) && (text[length] != '\0')) {
        length++;
    }
    return length;
}

static bool aic_bytes_zero(const uint8_t *data, uint32_t length)
{
    uint32_t index;

    for (index = 0U; index < length; index++) {
        if (data[index] != 0U) {
            return false;
        }
    }
    return true;
}

static void aic_wifi_clear_pmk_cache(void)
{
    memset(g_aic_pmk_cache, 0, sizeof(g_aic_pmk_cache));
    memset(g_aic_pmk_ssid, 0, sizeof(g_aic_pmk_ssid));
    memset(g_aic_pmk_password, 0, sizeof(g_aic_pmk_password));
    g_aic_pmk_cache_valid = false;
}

static void aic_wifi_load_defaults(void)
{
    static const uint8_t default_bssid[6] = { AIC8800_WIFI_BSSID };

    memset(&g_aic_sta, 0, sizeof(g_aic_sta));
    strncpy(g_aic_sta.ssid, AIC8800_WIFI_SSID, AIC8800_WIFI_SSID_MAX);
    strncpy(g_aic_sta.password, AIC8800_WIFI_PASSWORD,
            AIC8800_WIFI_PASSWORD_MAX);
    g_aic_sta.ssid[AIC8800_WIFI_SSID_MAX] = '\0';
    g_aic_sta.password[AIC8800_WIFI_PASSWORD_MAX] = '\0';
    memcpy(g_aic_sta.bssid, default_bssid, sizeof(g_aic_sta.bssid));
    g_aic_sta.band = (uint8_t)AIC8800_WIFI_BAND;
    aic_wifi_clear_pmk_cache();
}

static bool aic_wifi_ssid_match(const struct aic8800_wifi_ap *ap)
{
    size_t wanted = strlen(g_aic_sta.ssid);

    if ((wanted == 0U) || (wanted > AIC8800_WIFI_SSID_MAX)) {
        return false;
    }
    if (ap->ssid_len == 0U) {
        return !aic_bytes_zero(g_aic_sta.bssid, sizeof(g_aic_sta.bssid));
    }
    return (ap->ssid_len == wanted) &&
           (memcmp(ap->ssid, g_aic_sta.ssid, wanted) == 0);
}

static bool aic_wifi_bssid_match(const uint8_t bssid[6])
{
    if (aic_bytes_zero(g_aic_sta.bssid, sizeof(g_aic_sta.bssid))) {
        return true;
    }
    return memcmp(g_aic_sta.bssid, bssid, 6U) == 0;
}

static bool aic_wifi_band_match(uint16_t freq_mhz)
{
    if (g_aic_sta.band == AIC8800_WIFI_BAND_2GHZ) {
        return freq_mhz < 5000U;
    }
    if (g_aic_sta.band == AIC8800_WIFI_BAND_5GHZ) {
        return freq_mhz >= 5000U;
    }
    return true;
}

static int aic_wifi_score(const struct aic8800_wifi_ap *ap)
{
    int score = (int)ap->rssi;

#if AIC8800_WIFI_PREFER_5GHZ
    if (ap->freq_mhz >= 5000U) {
        score += AIC8800_WIFI_5GHZ_RSSI_BONUS;
    }
#endif
    return score;
}

int aic8800_wifi_init(void)
{
    if (!g_aic_sta_configured) {
        aic_wifi_load_defaults();
        g_aic_sta_configured = true;
    }
    memset(g_aic_mac, 0, sizeof(g_aic_mac));
    memset(&g_aic_connected_ap, 0, sizeof(g_aic_connected_ap));
    g_aic_mac_valid = false;
    g_aic_has_connected_ap = false;
    g_aic_state = AIC8800_WIFI_STATE_IDLE;
    g_aic_last_event = (enum aic8800_wifi_event)0xffU;
    aic8800_wifi_reset_scan();
    if (aic8800_fw_prepare() != 0) {
        USB_LOG_WRN("AIC firmware source not ready; boot will fail until set_package/set_images\r\n");
    }
    aic8800_usb_enable_zero_cd_modeswitch();
    USB_LOG_INFO("AIC Wi-Fi STA ready: SSID=\"%s\" band=%u BSSID %s\r\n",
                 g_aic_sta.ssid, g_aic_sta.band,
                 aic_bytes_zero(g_aic_sta.bssid, 6U) ? "any" : "locked");
    return 0;
}

void aic8800_wifi_set_event_callback(aic8800_wifi_event_cb_t callback,
                                     void *arg)
{
    g_aic_event_cb = callback;
    g_aic_event_arg = arg;
}

int aic8800_wifi_set_sta(const char *ssid, const char *password)
{
    struct aic8800_wifi_sta_config config = g_aic_sta;
    size_t ssid_len;
    size_t password_len;

    if ((ssid == NULL) || (password == NULL)) {
        return -USB_ERR_INVAL;
    }
    ssid_len = strlen(ssid);
    password_len = strlen(password);
    if ((ssid_len == 0U) || (ssid_len > AIC8800_WIFI_SSID_MAX) ||
        (password_len < 8U) || (password_len > AIC8800_WIFI_PASSWORD_MAX)) {
        return -USB_ERR_INVAL;
    }
    memset(config.ssid, 0, sizeof(config.ssid));
    memset(config.password, 0, sizeof(config.password));
    memcpy(config.ssid, ssid, ssid_len);
    memcpy(config.password, password, password_len);
    memset(config.pmk, 0, sizeof(config.pmk));
    config.pmk_valid = false;
    return aic8800_wifi_set_sta_ex(&config);
}

int aic8800_wifi_set_sta_ex(const struct aic8800_wifi_sta_config *config)
{
    size_t ssid_len;
    size_t password_len;

    if ((config == NULL) || (config->band > AIC8800_WIFI_BAND_5GHZ)) {
        return -USB_ERR_INVAL;
    }
    ssid_len = aic_bounded_len(config->ssid, AIC8800_WIFI_SSID_MAX);
    password_len = aic_bounded_len(config->password, AIC8800_WIFI_PASSWORD_MAX);
    if ((ssid_len == 0U) || (ssid_len > AIC8800_WIFI_SSID_MAX) ||
        (password_len > AIC8800_WIFI_PASSWORD_MAX) ||
        ((password_len != 0U) && (password_len < 8U))) {
        return -USB_ERR_INVAL;
    }
    g_aic_sta = *config;
    g_aic_sta.ssid[AIC8800_WIFI_SSID_MAX] = '\0';
    g_aic_sta.password[AIC8800_WIFI_PASSWORD_MAX] = '\0';
    if (password_len != 0U) {
        memset(g_aic_sta.pmk, 0, sizeof(g_aic_sta.pmk));
        g_aic_sta.pmk_valid = false;
    }
    aic_wifi_clear_pmk_cache();
    g_aic_sta_configured = true;
    return 0;
}

const struct aic8800_wifi_sta_config *aic8800_wifi_get_sta_config(void)
{
    return &g_aic_sta;
}

enum aic8800_wifi_state aic8800_wifi_get_state(void)
{
    return g_aic_state;
}

bool aic8800_wifi_is_connected(void)
{
    return (g_aic_state == AIC8800_WIFI_STATE_CONNECTED) ||
           (g_aic_state == AIC8800_WIFI_STATE_GOT_IP);
}

int aic8800_wifi_get_mac(uint8_t mac[6])
{
    if ((mac == NULL) || !g_aic_mac_valid) {
        return -USB_ERR_NODEV;
    }
    memcpy(mac, g_aic_mac, 6U);
    return 0;
}

int aic8800_wifi_get_ip4(uint8_t ip[4], uint8_t mask[4], uint8_t gateway[4])
{
    struct netif *netif = aic8800_lwip_get_netif();
    const ip4_addr_t *address;
    const ip4_addr_t *netmask;
    const ip4_addr_t *gw;

    if ((ip == NULL) || (mask == NULL) || (gateway == NULL) ||
        (netif == NULL) || !netif_is_up(netif)) {
        return -USB_ERR_NODEV;
    }
    address = netif_ip4_addr(netif);
    netmask = netif_ip4_netmask(netif);
    gw = netif_ip4_gw(netif);
    if (ip4_addr_isany_val(*address)) {
        return -USB_ERR_NODEV;
    }
    ip[0] = ip4_addr1(address);
    ip[1] = ip4_addr2(address);
    ip[2] = ip4_addr3(address);
    ip[3] = ip4_addr4(address);
    mask[0] = ip4_addr1(netmask);
    mask[1] = ip4_addr2(netmask);
    mask[2] = ip4_addr3(netmask);
    mask[3] = ip4_addr4(netmask);
    gateway[0] = ip4_addr1(gw);
    gateway[1] = ip4_addr2(gw);
    gateway[2] = ip4_addr3(gw);
    gateway[3] = ip4_addr4(gw);
    return 0;
}

int aic8800_wifi_get_connected_ap(struct aic8800_wifi_ap *ap)
{
    if ((ap == NULL) || !g_aic_has_connected_ap) {
        return -USB_ERR_NODEV;
    }
    *ap = g_aic_connected_ap;
    return 0;
}

struct netif *aic8800_wifi_get_netif(void)
{
    return aic8800_lwip_get_netif();
}

unsigned int aic8800_wifi_get_scan_count(void)
{
    return g_aic_scan_count;
}

int aic8800_wifi_get_scan_ap(unsigned int index, struct aic8800_wifi_ap *ap)
{
    if ((ap == NULL) || (index >= g_aic_scan_count)) {
        return -USB_ERR_INVAL;
    }
    *ap = g_aic_scan[index];
    return 0;
}

int aic8800_wifi_scan(void)
{
    if (g_aic_state < AIC8800_WIFI_STATE_READY) {
        return -USB_ERR_NOTCONN;
    }
    g_aic_state = AIC8800_WIFI_STATE_SCANNING;
    return aic8800_fmac_request_scan();
}

int aic8800_wifi_connect(void)
{
    if (g_aic_state < AIC8800_WIFI_STATE_READY) {
        return -USB_ERR_NOTCONN;
    }
    if ((g_aic_state == AIC8800_WIFI_STATE_CONNECTED) ||
        (g_aic_state == AIC8800_WIFI_STATE_GOT_IP) ||
        (g_aic_state == AIC8800_WIFI_STATE_CONNECTING)) {
        return -USB_ERR_BUSY;
    }
    g_aic_state = AIC8800_WIFI_STATE_CONNECTING;
    return aic8800_fmac_request_connect();
}

int aic8800_wifi_disconnect(void)
{
    return aic8800_fmac_request_disconnect();
}

void aic8800_wifi_reset_scan(void)
{
    memset(g_aic_scan, 0, sizeof(g_aic_scan));
    g_aic_scan_count = 0U;
}

void aic8800_wifi_offer_scan_ap(const uint8_t bssid[6], uint16_t freq_mhz,
                                int8_t rssi, const char *ssid,
                                uint32_t ssid_len, const uint8_t *rsn_ie,
                                uint32_t rsn_len)
{
    struct aic8800_wifi_ap *slot = NULL;
    unsigned int index;
    uint32_t copy_ssid;

    if ((bssid == NULL) || aic_bytes_zero(bssid, 6U)) {
        return;
    }
    for (index = 0U; index < g_aic_scan_count; index++) {
        if (memcmp(g_aic_scan[index].bssid, bssid, 6U) == 0) {
            slot = &g_aic_scan[index];
            if (rssi <= slot->rssi) {
                return;
            }
            break;
        }
    }
    if (slot == NULL) {
        if (g_aic_scan_count < AIC8800_WIFI_SCAN_MAX) {
            slot = &g_aic_scan[g_aic_scan_count++];
        } else {
            unsigned int weakest = 0U;

            for (index = 1U; index < g_aic_scan_count; index++) {
                if (g_aic_scan[index].rssi < g_aic_scan[weakest].rssi) {
                    weakest = index;
                }
            }
            if (rssi <= g_aic_scan[weakest].rssi) {
                return;
            }
            slot = &g_aic_scan[weakest];
        }
        memset(slot, 0, sizeof(*slot));
    }

    memcpy(slot->bssid, bssid, 6U);
    slot->freq_mhz = freq_mhz;
    slot->band = (freq_mhz >= 5000U) ? AIC8800_WIFI_BAND_5GHZ :
                                       AIC8800_WIFI_BAND_2GHZ;
    slot->rssi = rssi;
    copy_ssid = (ssid_len > AIC8800_WIFI_SSID_MAX) ?
                AIC8800_WIFI_SSID_MAX : ssid_len;
    if ((ssid != NULL) && (copy_ssid != 0U)) {
        memcpy(slot->ssid, ssid, copy_ssid);
    }
    slot->ssid_len = (uint8_t)copy_ssid;
    if ((rsn_ie != NULL) && (rsn_len > 0U) &&
        (rsn_len <= AIC8800_WIFI_RSN_IE_MAX)) {
        memcpy(slot->rsn_ie, rsn_ie, rsn_len);
        slot->rsn_len = (uint8_t)rsn_len;
    }
}

int aic8800_wifi_select_best_ap(struct aic8800_wifi_ap *ap)
{
    const struct aic8800_wifi_ap *best = NULL;
    int best_score = -1000;
    unsigned int index;

    if (ap == NULL) {
        return -USB_ERR_INVAL;
    }
    for (index = 0U; index < g_aic_scan_count; index++) {
        const struct aic8800_wifi_ap *candidate = &g_aic_scan[index];
        int score;

        if (!aic_wifi_band_match(candidate->freq_mhz) ||
            !aic_wifi_bssid_match(candidate->bssid) ||
            !aic_wifi_ssid_match(candidate)) {
            continue;
        }
        score = aic_wifi_score(candidate);
        if ((best == NULL) || (score > best_score)) {
            best = candidate;
            best_score = score;
        }
    }
    if (best == NULL) {
        return -USB_ERR_NODEV;
    }
    *ap = *best;
    if (ap->rsn_len == 0U) {
        memcpy(ap->rsn_ie, g_aic_default_rsn_ie, sizeof(g_aic_default_rsn_ie));
        ap->rsn_len = (uint8_t)sizeof(g_aic_default_rsn_ie);
    }
    return 0;
}

int aic8800_wifi_get_pmk(uint8_t pmk[32])
{
    size_t password_len;
    size_t ssid_len;

    if (pmk == NULL) {
        return -USB_ERR_INVAL;
    }
    password_len = strlen(g_aic_sta.password);
    ssid_len = strlen(g_aic_sta.ssid);
    if (password_len != 0U) {
        if (g_aic_pmk_cache_valid &&
            (strcmp(g_aic_pmk_ssid, g_aic_sta.ssid) == 0) &&
            (strcmp(g_aic_pmk_password, g_aic_sta.password) == 0)) {
            memcpy(pmk, g_aic_pmk_cache, 32U);
            return 0;
        }
        USB_LOG_INFO("deriving WPA2 PMK from passphrase (PBKDF2 %u)\r\n",
                     (unsigned int)AIC8800_WIFI_PBKDF2_ITERATIONS);
        aic8800_pbkdf2_sha1((const uint8_t *)g_aic_sta.password,
                            password_len,
                            (const uint8_t *)g_aic_sta.ssid, ssid_len,
                            AIC8800_WIFI_PBKDF2_ITERATIONS, pmk, 32U);
        memcpy(g_aic_pmk_cache, pmk, 32U);
        memset(g_aic_pmk_ssid, 0, sizeof(g_aic_pmk_ssid));
        memset(g_aic_pmk_password, 0, sizeof(g_aic_pmk_password));
        memcpy(g_aic_pmk_ssid, g_aic_sta.ssid, ssid_len);
        memcpy(g_aic_pmk_password, g_aic_sta.password, password_len);
        g_aic_pmk_cache_valid = true;
        return 0;
    }
    if (g_aic_sta.pmk_valid) {
        memcpy(pmk, g_aic_sta.pmk, 32U);
        return 0;
    }
    USB_LOG_ERR("AIC Wi-Fi needs AIC8800_WIFI_PASSWORD (8..63 characters)\r\n");
    return -USB_ERR_INVAL;
}

void aic8800_wifi_set_mac(const uint8_t mac[6])
{
    if (mac == NULL) {
        return;
    }
    memcpy(g_aic_mac, mac, 6U);
    g_aic_mac_valid = true;
}

void aic8800_wifi_mark_connecting(void)
{
    if (g_aic_state >= AIC8800_WIFI_STATE_READY) {
        g_aic_state = AIC8800_WIFI_STATE_CONNECTING;
    }
}

void aic8800_wifi_on_transport_lost(void)
{
    enum aic8800_wifi_state previous = g_aic_state;

    g_aic_state = AIC8800_WIFI_STATE_IDLE;
    g_aic_has_connected_ap = false;
    if ((previous != AIC8800_WIFI_STATE_CONNECTED) &&
        (previous != AIC8800_WIFI_STATE_GOT_IP)) {
        return;
    }
    g_aic_last_event = AIC8800_WIFI_EVENT_DISCONNECTED;
    if (g_aic_event_cb != NULL) {
        g_aic_event_cb(AIC8800_WIFI_EVENT_DISCONNECTED, g_aic_event_arg);
    }
}

void aic8800_wifi_set_connected_ap(const struct aic8800_wifi_ap *ap)
{
    if (ap == NULL) {
        memset(&g_aic_connected_ap, 0, sizeof(g_aic_connected_ap));
        g_aic_has_connected_ap = false;
        return;
    }
    g_aic_connected_ap = *ap;
    g_aic_has_connected_ap = true;
}

void aic8800_wifi_notify(enum aic8800_wifi_event event)
{
    switch (event) {
    case AIC8800_WIFI_EVENT_READY:
        g_aic_state = AIC8800_WIFI_STATE_READY;
        break;
    case AIC8800_WIFI_EVENT_SCAN_DONE:
        if (g_aic_state < AIC8800_WIFI_STATE_CONNECTING) {
            g_aic_state = AIC8800_WIFI_STATE_READY;
        }
        break;
    case AIC8800_WIFI_EVENT_CONNECTED:
        g_aic_state = AIC8800_WIFI_STATE_CONNECTED;
        break;
    case AIC8800_WIFI_EVENT_GOT_IP:
        g_aic_state = AIC8800_WIFI_STATE_GOT_IP;
        break;
    case AIC8800_WIFI_EVENT_DISCONNECTED:
        if ((g_aic_state != AIC8800_WIFI_STATE_CONNECTED) &&
            (g_aic_state != AIC8800_WIFI_STATE_GOT_IP) &&
            (g_aic_last_event == AIC8800_WIFI_EVENT_DISCONNECTED)) {
            return;
        }
        g_aic_state = AIC8800_WIFI_STATE_READY;
        g_aic_has_connected_ap = false;
        break;
    case AIC8800_WIFI_EVENT_CONNECT_FAILED:
        g_aic_state = AIC8800_WIFI_STATE_READY;
        g_aic_has_connected_ap = false;
        break;
    default:
        break;
    }
    g_aic_last_event = event;
    if (g_aic_event_cb != NULL) {
        g_aic_event_cb(event, g_aic_event_arg);
    }
}
