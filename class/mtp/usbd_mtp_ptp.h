/*
 * Copyright (c) 2026, Jinsc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USBD_MTP_PTP_H
#define USBD_MTP_PTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* MTP/PTP encoding primitives: container fields and strings.
 *
 * String convention: on the wire it is "one length byte (number of UTF-16 **code units**, including
 * the terminating NUL) followed by UTF-16LE", while the filesystem and the application usually use
 * UTF-8, so **both directions must really transcode**. A simplified implementation that once put
 * every UTF-8 byte into one UTF-16 code unit worked for ASCII, but non-ASCII names came out as
 * garbage and reading them back produced different garbage again. */

size_t mtp_ptp_put16(uint8_t *p, uint16_t v);
size_t mtp_ptp_put32(uint8_t *p, uint32_t v);
size_t mtp_ptp_put64(uint8_t *p, uint64_t v);

/** UTF-8 to PTP string; returns bytes written, 0 = invalid or does not fit (**failing is better than truncating**) */
size_t mtp_ptp_put_str(uint8_t *dst, size_t cap, const char *utf8);

/** PTP string to UTF-8 (surrogate pairs included); returns input bytes consumed, 0 = invalid or does not fit */
size_t mtp_ptp_get_str(const uint8_t *src, size_t len, char *out, size_t out_n);

/** Write a "count (u32) + u16 per entry" array (DeviceInfo capability/event/format lists) */
size_t mtp_ptp_put_u16_array(uint8_t *p, const uint16_t *v, size_t n);

#endif
