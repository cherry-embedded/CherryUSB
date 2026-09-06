/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Internal AIC8800 runtime FMAC bring-up. Applications should include
 * aic8800_wifi.h and change credentials in aic8800_wifi_config.h.
 */
#ifndef AIC8800_FMAC_H
#define AIC8800_FMAC_H

#include <stdint.h>

#include "aic8800_usb.h"

/* Experimental firmware coexistence scheduler.  The command mirrors the AIC
 * Windows/Linux driver, but enabling NULL/CTS with the Dec-2025 D80 FMAC made
 * WPA2 stall after EAPOL M2 in both the v64 and v65 captures.  Keep it off for
 * the validated Wi-Fi + A2DP path unless a matching firmware/patch set needs
 * an explicit coexistence command. */
#ifndef AIC8800_FMAC_WIFI_BT_COEX_ENABLE
#define AIC8800_FMAC_WIFI_BT_COEX_ENABLE 0
#endif

/* Throughput profile.  The D80 USB firmware can concatenate several RX
 * records into one bulk transfer.  Eight 2-KiB slots keep the complete
 * double-buffered RX pipeline inside the existing USB non-cacheable region;
 * no SAI MPU or audio DMA layout change is required. */
#ifndef AIC8800_FMAC_USE_5GHZ
#define AIC8800_FMAC_USE_5GHZ 1U
#endif

#ifndef AIC8800_FMAC_RX_AGGREGATE_COUNT
#define AIC8800_FMAC_RX_AGGREGATE_COUNT 8U
#endif

#if (AIC8800_FMAC_RX_AGGREGATE_COUNT < 1U) || \
    (AIC8800_FMAC_RX_AGGREGATE_COUNT > 10U)
#error "AIC8800 FMAC USB RX aggregation must be in the range 1..10"
#endif

#define AIC8800_FMAC_RX_BUFFER_SIZE \
    (2048U * AIC8800_FMAC_RX_AGGREGATE_COUNT)
#define AIC8800_FMAC_RX_PIPE_COUNT 2U

#ifdef __cplusplus
extern "C" {
#endif

void aic8800_fmac_attach(struct aic8800_usb_device *device);
void aic8800_fmac_detach(struct aic8800_usb_device *device);
int aic8800_fmac_request_scan(void);
int aic8800_fmac_request_connect(void);
int aic8800_fmac_request_disconnect(void);

struct aic8800_fmac_tx_stats {
    uint32_t enqueued_frames;
    uint32_t sent_frames;
    uint32_t sent_bytes;
    uint32_t queue_drops;
    uint32_t queue_high_water;
    uint32_t usb_errors;
    uint32_t mutex_timeouts;
    uint32_t mutex_max_ms;
    uint32_t usb_max_ms;
    uint32_t usb_slow_transfers;
    uint32_t rx_input_drops;
    uint32_t rx_stalls;
    uint32_t rx_high_water;
    uint32_t rx_amsdu_msdus;
    uint32_t rx_zerocopy_frames;
    uint32_t rx_usb_transfers;
    uint32_t rx_usb_bytes;
    uint32_t rx_usb_max;
    uint32_t rx_usb_errors;
    uint32_t rx_usb_records;
    uint32_t rx_usb_max_records;
    uint32_t rx_malformed_records;
    uint32_t rx_process_ms;
    uint32_t rx_process_max_us;
    uint32_t rx_schedule_max_us;
    uint32_t rx_lwip_lock_max_us;
    uint32_t rx_rearm_max_us;
};

void aic8800_fmac_get_tx_stats(struct aic8800_fmac_tx_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* AIC8800_FMAC_H */
