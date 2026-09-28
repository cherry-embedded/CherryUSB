/*
 * Copyright (c) 2026, Jinsc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USBD_MTP_INTERNAL_H
#define USBD_MTP_INTERNAL_H

/* Internal MTP class interface (usbd_mtp.c <-> usbd_mtp_obj.c), not exported.
 * The public API lives in usbd_mtp.h: the usual CherryUSB contract (usbd_mtp_init_intf plus the
 * usbd_mtp_* filesystem hooks) extended by the two optional features of this implementation
 * (multiple storages, dynamic device identity). */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "usbd_mtp_config.h"

/* 12-byte PTP container header (little endian) */
struct mtp_container_hdr {
    uint32_t len;
    uint16_t type;
    uint16_t code;
    uint32_t tid;
};

/* ---------------- provided by the transport layer (usbd_mtp.c): sending ---------------- */
int usbd_mtp_send_container(uint8_t busid, uint16_t type, uint16_t code, uint32_t tid,
                            const uint8_t *payload, size_t payload_len);
int usbd_mtp_send_response(uint8_t busid, uint16_t code, uint32_t tid);
int usbd_mtp_send_response_params(uint8_t busid, uint16_t code, uint32_t tid,
                                  const uint32_t *params, int nparam);
int usbd_mtp_stream_begin(uint8_t busid, uint16_t type, uint16_t code, uint32_t tid, uint32_t payload_len);
int usbd_mtp_stream_write(uint8_t busid, const uint8_t *data, size_t len);
int usbd_mtp_stream_end(uint8_t busid);

/** Whether the host has selected a configuration (the object layer uses it to tell if it may work) */
bool usbd_mtp_host_configured(uint8_t busid);

/* ---------------- provided by the object layer (usbd_mtp_obj.c): receiving side ---------------- */
void usbd_mtp_obj_init(uint8_t busid);
bool usbd_mtp_obj_command(uint8_t busid, const struct mtp_container_hdr *hdr, const uint32_t *params, int nparam);
void usbd_mtp_obj_data_begin(uint8_t busid, uint32_t payload_len);
void usbd_mtp_obj_data(uint8_t busid, const uint8_t *data, size_t len);
void usbd_mtp_obj_data_end(uint8_t busid);
void usbd_mtp_obj_data_truncated(uint8_t busid);
void usbd_mtp_obj_reset(uint8_t busid);
void usbd_mtp_obj_session_close(uint8_t busid);
void usbd_mtp_obj_stat(uint8_t busid, uint32_t *enq, uint32_t *written, uint32_t *fails);

/** Runtime trace switch (for field debugging; normal logging goes through USB_LOG_*) */
void usbd_mtp_set_trace(bool on);
bool usbd_mtp_get_trace(void);

#define MTP_TRACE(...)                        \
    do {                                      \
        if (usbd_mtp_get_trace()) {           \
            USB_LOG_RAW(__VA_ARGS__);         \
        }                                     \
    } while (0)

#endif /* USBD_MTP_INTERNAL_H */
