/*
 * Copyright (c) 2026, Jinsc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usbd_mtp_ptp.c
 * @brief MTP/PTP encoding primitives (integers and strings), independent of the USB stack and the filesystem
 */

#include <string.h>

#include "usbd_mtp_ptp.h"

size_t mtp_ptp_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)(v >> 8);
    return 2;
}

size_t mtp_ptp_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
    return 4;
}

size_t mtp_ptp_put64(uint8_t *p, uint64_t v)
{
    uint32_t i;

    for (i = 0; i < 8; i++) {
        p[i] = (uint8_t)(v >> (8 * i));
    }
    return 8;
}

/* ---------------- UTF-8 <-> UTF-16LE ---------------- */

/** Append one code point; nothing is written when space runs out, the caller sees the failure from the returned position */
static size_t utf8_append(char *out, size_t out_n, size_t w, uint32_t cp)
{
    if (cp < 0x80) {
        if (w + 1 > out_n) {
            return w;
        }
        out[w++] = (char)cp;
    } else if (cp < 0x800) {
        if (w + 2 > out_n) {
            return w;
        }
        out[w++] = (char)(0xc0 | (cp >> 6));
        out[w++] = (char)(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        if (w + 3 > out_n) {
            return w;
        }
        out[w++] = (char)(0xe0 | (cp >> 12));
        out[w++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[w++] = (char)(0x80 | (cp & 0x3f));
    } else {
        if (w + 4 > out_n) {
            return w;
        }
        out[w++] = (char)(0xf0 | (cp >> 18));
        out[w++] = (char)(0x80 | ((cp >> 12) & 0x3f));
        out[w++] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[w++] = (char)(0x80 | (cp & 0x3f));
    }
    return w;
}

/** Decode one UTF-8 code point; returns bytes consumed (>=1), an invalid sequence returns 1 with cp='?' */
static size_t utf8_next(const uint8_t *p, uint32_t *cp)
{
    uint8_t c = p[0];

    if (c < 0x80) {
        *cp = c;
        return 1;
    }
    if ((c & 0xe0) == 0xc0) {
        uint32_t u;

        if ((p[1] & 0xc0) != 0x80) {
            *cp = '?';
            return 1;
        }
        u = ((uint32_t)(c & 0x1f) << 6) | (p[1] & 0x3f);
        *cp = (u < 0x80) ? '?' : u; /* reject overlong encodings */
        return 2;
    }
    if ((c & 0xf0) == 0xe0) {
        uint32_t u;

        if (((p[1] & 0xc0) != 0x80) || ((p[2] & 0xc0) != 0x80)) {
            *cp = '?';
            return 1;
        }
        u = ((uint32_t)(c & 0x0f) << 12) | ((uint32_t)(p[1] & 0x3f) << 6) | (p[2] & 0x3f);
        *cp = (u < 0x800) ? '?' : u;
        return 3;
    }
    if ((c & 0xf8) == 0xf0) {
        uint32_t u;

        if (((p[1] & 0xc0) != 0x80) || ((p[2] & 0xc0) != 0x80) || ((p[3] & 0xc0) != 0x80)) {
            *cp = '?';
            return 1;
        }
        u = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3f) << 12) |
            ((uint32_t)(p[2] & 0x3f) << 6) | (p[3] & 0x3f);
        *cp = ((u < 0x10000) || (u > 0x10ffff)) ? '?' : u;
        return 4;
    }
    *cp = '?';
    return 1;
}

static size_t utf8_count_units(const char *s)
{
    size_t units = 0;

    for (const uint8_t *p = (const uint8_t *)s; *p != 0;) {
        uint32_t cp;

        p += utf8_next(p, &cp);
        units += (cp >= 0x10000) ? 2 : 1; /* supplementary planes take a surrogate pair (2 code units) */
    }
    return units;
}

size_t mtp_ptp_put_str(uint8_t *dst, size_t cap, const char *utf8)
{
    const char *s = (utf8 != NULL) ? utf8 : "";
    size_t units = utf8_count_units(s);
    size_t need = 1 + (units + 1) * 2;
    size_t o = 1;

    if (((units + 1) > 255) || (need > cap)) {
        return 0; /* does not fit: the caller treats it as a failure, never truncate silently */
    }
    dst[0] = (uint8_t)(units + 1);
    for (const uint8_t *p = (const uint8_t *)s; *p != 0;) {
        uint32_t cp;

        p += utf8_next(p, &cp);
        if (cp >= 0x10000) {
            uint32_t v = cp - 0x10000u;
            uint16_t hi = (uint16_t)(0xd800u + (v >> 10));
            uint16_t lo = (uint16_t)(0xdc00u + (v & 0x3ffu));

            dst[o++] = (uint8_t)(hi & 0xff);
            dst[o++] = (uint8_t)(hi >> 8);
            dst[o++] = (uint8_t)(lo & 0xff);
            dst[o++] = (uint8_t)(lo >> 8);
        } else {
            dst[o++] = (uint8_t)(cp & 0xff);
            dst[o++] = (uint8_t)(cp >> 8);
        }
    }
    dst[o++] = 0;
    dst[o++] = 0;
    return o;
}

size_t mtp_ptp_get_str(const uint8_t *src, size_t len, char *out, size_t out_n)
{
    size_t nchars;
    size_t w = 0;

    if ((out == NULL) || (out_n == 0)) {
        return 0;
    }
    out[0] = '\0';
    if (len < 1) {
        return 0;
    }
    nchars = src[0];
    if ((nchars == 0) || ((1 + nchars * 2) > len)) {
        return 0; /* empty or out of bounds -> invalid */
    }
    for (size_t i = 0; i + 1 < nchars; i++) { /* the last one is the terminating NUL */
        uint32_t u = (uint32_t)src[1 + i * 2] | ((uint32_t)src[2 + i * 2] << 8);

        if ((u >= 0xd800) && (u <= 0xdbff)) { /* high surrogate: merge with the low surrogate right after it */
            uint32_t lo = 0;
            bool pair = false;

            if (i + 2 < nchars) {
                lo = (uint32_t)src[1 + (i + 1) * 2] | ((uint32_t)src[2 + (i + 1) * 2] << 8);
                pair = (lo >= 0xdc00) && (lo <= 0xdfff);
            }
            if (pair) {
                i++;
                u = 0x10000u + ((u - 0xd800u) << 10) + (lo - 0xdc00u);
            } else {
                u = '?';
            }
        } else if ((u >= 0xdc00) && (u <= 0xdfff)) { /* lone low surrogate */
            u = '?';
        }
        if (u == 0) {
            continue; /* embedded NUL: skip it instead of cutting the name in half */
        }
        {
            size_t nw = utf8_append(out, out_n - 1, w, u); /* keep 1 byte for the terminating NUL */

            if (nw == w) {
                return 0; /* the whole name does not fit: fail rather than truncate */
            }
            w = nw;
        }
    }
    out[w] = '\0';
    return 1 + nchars * 2;
}

size_t mtp_ptp_put_u16_array(uint8_t *p, const uint16_t *v, size_t n)
{
    size_t off;

    if (n > 0xffff) {
        n = 0xffff;
    }
    off = mtp_ptp_put32(p, (uint32_t)n);
    for (size_t i = 0; i < n; i++) {
        off += mtp_ptp_put16(p + off, v[i]);
    }
    return off;
}
