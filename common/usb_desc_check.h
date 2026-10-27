/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * USB configuration descriptor checker (header-only).
 *
 * Walks a Configuration descriptor once and verifies:
 *
 *   Configuration header
 *     - bLength == 9, bDescriptorType == CONFIGURATION
 *     - wTotalLength equals the length passed in by the caller
 *
 *   Descriptor chain
 *     - every bLength > 2 and stays inside wTotalLength
 *     - CS_INTERFACE and CS_ENDPOINT descriptors carry more than 3 bytes
 *     - the chain ends exactly at wTotalLength
 *
 *   Interface
 *     - bLength == 9
 *     - bInterfaceNumber starts at 0 and increments by 1
 *     - bAlternateSetting starts at 0 and increments by 1 per interface
 *     - bNumEndpoints matches the number of endpoints actually present
 *     - the number of distinct interfaces equals bNumInterfaces
 *
 *   Endpoint
 *     - bLength >= 7
 *     - appears only after an interface descriptor
 *     - endpoint number is not 0, address reserved bits are clear
 *     - transfer type is not Control
 *     - endpoint address is unique within one alternate setting and across all
 *       interfaces; different alternate settings of the same interface may reuse it
 *     - wMaxPacketSize matches the transfer type and the given speed:
 *         Low    Interrupt 1..8, Bulk and Isochronous not allowed
 *         Full   Bulk 8/16/32/64, Interrupt 1..64, Iso 1..1023
 *         High   Bulk 512, Interrupt/Iso 1..1024 with high-bandwidth limits
 *         Super  Bulk 1024, Interrupt/Iso 1..1024 (Super and SuperPlus share the rules)
 *     - every SuperSpeed endpoint is followed by an endpoint companion descriptor
 *     - bInterval matches the transfer type and the given speed
 *
 *   Interface association (IAD)
 *     - CDC control interfaces may use a Union functional descriptor instead of
 *       an IAD; its master must match the current interface and its subordinate
 *       interfaces must exist and differ from the master
 *     - other CDC, Audio, Video and RNDIS control interfaces must be covered by
 *       an IAD of the matching function class
 *     - every IAD describes at least two interfaces and sits before its first interface
 *     - IAD interface ranges must not overlap
 *     - the first interface of an IAD is the control interface of its declared
 *       function class; all remaining interfaces in the range must be data
 *       interfaces of that function class
 */
#ifndef USB_DESC_CHECK_H
#define USB_DESC_CHECK_H

#include <stdint.h>
#include <stddef.h>

#include "usb_util.h"
#include "usb_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USB_CLASS_AUDIO
#define USB_CLASS_AUDIO     0x01u
#endif
#ifndef USB_CLASS_CDC
#define USB_CLASS_CDC       0x02u
#endif
#ifndef USB_CLASS_CDC_DATA
#define USB_CLASS_CDC_DATA  0x0Au
#endif
#ifndef USB_CLASS_VIDEO
#define USB_CLASS_VIDEO     0x0Eu
#endif
#ifndef USB_CLASS_WIRELESS
#define USB_CLASS_WIRELESS  0xE0u
#endif
#ifndef AUDIO_SUBCLASS_AUDIOCONTROL
#define AUDIO_SUBCLASS_AUDIOCONTROL    0x01u
#endif
#ifndef AUDIO_SUBCLASS_AUDIOSTREAMING
#define AUDIO_SUBCLASS_AUDIOSTREAMING  0x02u
#endif
#ifndef AUDIO_SUBCLASS_MIDISTREAMING
#define AUDIO_SUBCLASS_MIDISTREAMING   0x03u
#endif
#ifndef VIDEO_SUBCLASS_VIDEOCONTROL
#define VIDEO_SUBCLASS_VIDEOCONTROL    0x01u
#endif
#ifndef VIDEO_SUBCLASS_VIDEOSTREAMING
#define VIDEO_SUBCLASS_VIDEOSTREAMING  0x02u
#endif
#ifndef WIRELESS_SUBCLASS_RF
#define WIRELESS_SUBCLASS_RF           0x01u
#endif
#ifndef WIRELESS_PROTOCOL_RNDIS
#define WIRELESS_PROTOCOL_RNDIS        0x03u
#endif

#ifndef USB_DESC_CHECK_MAX_IFACE
#define USB_DESC_CHECK_MAX_IFACE      16u
#endif
#ifndef USB_DESC_CHECK_MAX_IAD
#define USB_DESC_CHECK_MAX_IAD        4u
#endif

#if (USB_DESC_CHECK_MAX_IFACE < 1u) || (USB_DESC_CHECK_MAX_IFACE > 255u)
#error "USB_DESC_CHECK_MAX_IFACE must be 1..255"
#endif
#if (USB_DESC_CHECK_MAX_IAD < 1u) || (USB_DESC_CHECK_MAX_IAD > 255u)
#error "USB_DESC_CHECK_MAX_IAD must be 1..255"
#endif

#define CFG_OFF_BLENGTH     0u
#define CFG_OFF_TYPE        1u
#define CFG_OFF_TOTAL_LEN   2u
#define CFG_OFF_NUM_IFACE   4u

#define IF_OFF_NUM          2u
#define IF_OFF_ALT          3u
#define IF_OFF_NUM_EP       4u
#define IF_OFF_CLASS        5u
#define IF_OFF_SUBCLASS     6u
#define IF_OFF_PROTOCOL     7u

#define EP_OFF_ADDR         2u
#define EP_OFF_ATTR         3u
#define EP_OFF_MPS          4u
#define EP_OFF_INTERVAL     6u

#ifndef USB_SIZEOF_ENDPOINT_COMPANION_DESC
#define USB_SIZEOF_ENDPOINT_COMPANION_DESC  6u
#endif

#define USB_DESC_CHECK_EP_MIN_LEN   7u
#define USB_DESC_CHECK_IF_LEN       9u
#define USB_DESC_CHECK_IAD_LEN      8u
#define USB_DESC_CHECK_CDC_UNION    0x06u

typedef enum {
    USB_DESC_CHECK_OK              =   0,
    USB_DESC_CHECK_ERR_PARAM       =  -1,
    USB_DESC_CHECK_ERR_SPEED       =  -2,
    USB_DESC_CHECK_ERR_BUF_SHORT   =  -3,
    USB_DESC_CHECK_ERR_CFG_HEADER  =  -4,
    USB_DESC_CHECK_ERR_TOTAL_LEN   =  -5,
    USB_DESC_CHECK_ERR_DESC_LEN    =  -6,
    USB_DESC_CHECK_ERR_IFACE_NUM   =  -7,
    USB_DESC_CHECK_ERR_EP_ADDR     =  -8,
    USB_DESC_CHECK_ERR_EP_ATTR     =  -9,
    USB_DESC_CHECK_ERR_EP_MPS      = -10,
    USB_DESC_CHECK_ERR_EP_INTERVAL = -11,
    USB_DESC_CHECK_ERR_IAD         = -12,
    USB_DESC_CHECK_ERR_CDC         = -13,
    USB_DESC_CHECK_ERR_AUDIO       = -14,
    USB_DESC_CHECK_ERR_VIDEO       = -15,
    USB_DESC_CHECK_ERR_DESC_ORDER  = -16,
    USB_DESC_CHECK_ERR_RNDIS       = -17,
    USB_DESC_CHECK_ERR_EP_COUNT    = -18,
    USB_DESC_CHECK_ERR_EP_DUP      = -19,
    USB_DESC_CHECK_ERR_ALT_SETTING = -20,
    USB_DESC_CHECK_ERR_EP_COMPANION = -21,
} usb_desc_check_err_t;

struct usb_desc_check_iface {
    uint16_t off;
    uint8_t  cls;
    uint8_t  sub;
    uint8_t  proto;
    uint8_t  cdc_union;
};

struct usb_desc_check_iad {
    uint16_t off;
    uint8_t  first;
    uint8_t  count;
    uint8_t  func_cls;
    uint8_t  func_sub;
    uint8_t  func_proto;
};

static inline int32_t usb_desc_check_fail(int32_t err, uint32_t offset, uint32_t *err_offset)
{
    if (err_offset != NULL) {
        *err_offset = offset;
    }
    return err;
}

static inline uint16_t usb_desc_check_rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}

static inline uint8_t usb_desc_check_is_ss(uint8_t speed)
{
    return (uint8_t)(((speed == (uint8_t)USB_SPEED_SUPER) ||
                      (speed == (uint8_t)USB_SPEED_SUPER_PLUS)) ? 1u : 0u);
}

static inline uint8_t usb_desc_check_is_ctrl(uint8_t cls, uint8_t sub, uint8_t proto)
{
    if (cls == USB_CLASS_CDC) {
        return 1u;
    }
    if ((cls == USB_CLASS_AUDIO) && (sub == AUDIO_SUBCLASS_AUDIOCONTROL)) {
        return 1u;
    }
    if ((cls == USB_CLASS_VIDEO) && (sub == VIDEO_SUBCLASS_VIDEOCONTROL)) {
        return 1u;
    }
    if ((cls == USB_CLASS_WIRELESS) && (sub == WIRELESS_SUBCLASS_RF) &&
        (proto == WIRELESS_PROTOCOL_RNDIS)) {
        return 1u;
    }
    return 0u;
}

static inline uint8_t usb_desc_check_is_data(uint8_t cls, uint8_t sub)
{
    if (cls == USB_CLASS_CDC_DATA) {
        return 1u;
    }
    if (cls == USB_CLASS_AUDIO) {
        if ((sub == AUDIO_SUBCLASS_AUDIOSTREAMING) || (sub == AUDIO_SUBCLASS_MIDISTREAMING)) {
            return 1u;
        }
    }
    if ((cls == USB_CLASS_VIDEO) && (sub == VIDEO_SUBCLASS_VIDEOSTREAMING)) {
        return 1u;
    }
    return 0u;
}

static inline int32_t usb_desc_check_group_err(uint8_t func_cls)
{
    if (func_cls == USB_CLASS_CDC) {
        return USB_DESC_CHECK_ERR_CDC;
    }
    if (func_cls == USB_CLASS_AUDIO) {
        return USB_DESC_CHECK_ERR_AUDIO;
    }
    if (func_cls == USB_CLASS_VIDEO) {
        return USB_DESC_CHECK_ERR_VIDEO;
    }
    if (func_cls == USB_CLASS_WIRELESS) {
        return USB_DESC_CHECK_ERR_RNDIS;
    }
    return USB_DESC_CHECK_ERR_IAD;
}

static inline int32_t usb_desc_check_ep_mps(uint8_t speed, uint8_t ep_type, uint16_t mps)
{
    uint16_t base;
    uint8_t  hb;

    if ((mps & 0xE000u) != 0u) {
        return USB_DESC_CHECK_ERR_EP_MPS;
    }

    base = (uint16_t)(mps & (uint16_t)USB_MAXPACKETSIZE_MASK);
    hb   = (uint8_t)((mps & (uint16_t)USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_MASK)
                     >> USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_SHIFT);

    if (usb_desc_check_is_ss(speed) != 0u) {
        if ((mps & 0x1800u) != 0u) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        }
        if (ep_type == USB_ENDPOINT_TYPE_BULK) {
            if (mps != 1024u) {
                return USB_DESC_CHECK_ERR_EP_MPS;
            }
        } else if ((base == 0u) || (base > 1024u)) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        } else {
        }
    } else if (speed == USB_SPEED_HIGH) {
        if (ep_type == USB_ENDPOINT_TYPE_BULK) {
            if (mps != USB_BULK_EP_MPS_HS) {
                return USB_DESC_CHECK_ERR_EP_MPS;
            }
        } else if ((base == 0u) || (base > 1024u) || (hb == 3u) ||
                   ((hb == 1u) && (base < 513u)) ||
                   ((hb == 2u) && (base < 683u))) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        } else {
        }
    } else if (speed == USB_SPEED_LOW) {
        if ((hb != 0u) ||
            (ep_type == USB_ENDPOINT_TYPE_BULK) ||
            (ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS)) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        }
        if ((base == 0u) || (base > 8u)) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        }
    } else {
        if (hb != 0u) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        }
        if (ep_type == USB_ENDPOINT_TYPE_BULK) {
            if ((base != 8u) && (base != 16u) && (base != 32u) && (base != 64u)) {
                return USB_DESC_CHECK_ERR_EP_MPS;
            }
        } else if (ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
            if ((base == 0u) || (base > 64u)) {
                return USB_DESC_CHECK_ERR_EP_MPS;
            }
        } else if ((base == 0u) || (base > 1023u)) {
            return USB_DESC_CHECK_ERR_EP_MPS;
        } else {
        }
    }

    return USB_DESC_CHECK_OK;
}

static inline int32_t usb_desc_check_ep_interval(uint8_t speed, uint8_t ep_type, uint8_t interval)
{
    if (ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        if ((interval == 0u) || (interval > 16u)) {
            return USB_DESC_CHECK_ERR_EP_INTERVAL;
        }
    } else if (ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
        if (interval == 0u) {
            return USB_DESC_CHECK_ERR_EP_INTERVAL;
        }
        if (((speed == USB_SPEED_HIGH) || (usb_desc_check_is_ss(speed) != 0u)) &&
            (interval > 16u)) {
            return USB_DESC_CHECK_ERR_EP_INTERVAL;
        }
    } else {
    }

    return USB_DESC_CHECK_OK;
}

static inline uint8_t usb_desc_check_is_ctrl_of(uint8_t func_cls,
                                                const struct usb_desc_check_iface *c)
{
    if (func_cls == USB_CLASS_CDC) {
        return (uint8_t)((c->cls == USB_CLASS_CDC) ? 1u : 0u);
    }
    if (func_cls == USB_CLASS_AUDIO) {
        return (uint8_t)(((c->cls == USB_CLASS_AUDIO) &&
                          (c->sub == AUDIO_SUBCLASS_AUDIOCONTROL)) ? 1u : 0u);
    }
    if (func_cls == USB_CLASS_VIDEO) {
        return (uint8_t)(((c->cls == USB_CLASS_VIDEO) &&
                          (c->sub == VIDEO_SUBCLASS_VIDEOCONTROL)) ? 1u : 0u);
    }
    if (func_cls == USB_CLASS_WIRELESS) {
        return (uint8_t)(((c->cls == USB_CLASS_WIRELESS) &&
                          (c->sub == WIRELESS_SUBCLASS_RF) &&
                          (c->proto == WIRELESS_PROTOCOL_RNDIS)) ? 1u : 0u);
    }
    return usb_desc_check_is_ctrl(c->cls, c->sub, c->proto);
}

static inline uint8_t usb_desc_check_is_data_of(uint8_t func_cls,
                                                const struct usb_desc_check_iface *c)
{
    if (func_cls == USB_CLASS_CDC) {
        return (uint8_t)((c->cls == USB_CLASS_CDC_DATA) ? 1u : 0u);
    }
    if (func_cls == USB_CLASS_AUDIO) {
        return (uint8_t)(((c->cls == USB_CLASS_AUDIO) &&
                          ((c->sub == AUDIO_SUBCLASS_AUDIOSTREAMING) ||
                           (c->sub == AUDIO_SUBCLASS_MIDISTREAMING))) ? 1u : 0u);
    }
    if (func_cls == USB_CLASS_VIDEO) {
        return (uint8_t)(((c->cls == USB_CLASS_VIDEO) &&
                          (c->sub == VIDEO_SUBCLASS_VIDEOSTREAMING)) ? 1u : 0u);
    }
    if (func_cls == USB_CLASS_WIRELESS) {
        return (uint8_t)((c->cls == USB_CLASS_CDC_DATA) ? 1u : 0u);
    }
    return usb_desc_check_is_data(c->cls, c->sub);
}

static inline int32_t usb_desc_check_iad_group(const struct usb_desc_check_iad *iad,
                                               const struct usb_desc_check_iface *ifs)
{
    uint8_t j;
    uint8_t end = (uint8_t)(iad->first + iad->count);

    if ((iad->func_cls == USB_CLASS_WIRELESS) &&
        (iad->func_proto != WIRELESS_PROTOCOL_RNDIS)) {
        return USB_DESC_CHECK_OK;
    }

    if (usb_desc_check_is_ctrl_of(iad->func_cls, &ifs[iad->first]) == 0u) {
        return usb_desc_check_group_err(iad->func_cls);
    }

    for (j = (uint8_t)(iad->first + 1u); j < end; j++) {
        if (usb_desc_check_is_data_of(iad->func_cls, &ifs[j]) == 0u) {
            return usb_desc_check_group_err(iad->func_cls);
        }
    }

    return USB_DESC_CHECK_OK;
}

static inline uint8_t usb_desc_check_iad_covers(const struct usb_desc_check_iad *iads,
                                                uint8_t iad_cnt,
                                                const struct usb_desc_check_iface *ifs,
                                                uint8_t num)
{
    uint8_t i;

    for (i = 0u; i < iad_cnt; i++) {
        if (iads[i].func_cls != ifs[num].cls) {
            continue;
        }
        if ((ifs[num].cls == USB_CLASS_WIRELESS) &&
            ((iads[i].func_sub != WIRELESS_SUBCLASS_RF) ||
             (iads[i].func_proto != WIRELESS_PROTOCOL_RNDIS))) {
            continue;
        }
        if ((num >= iads[i].first) &&
            ((uint16_t)num < ((uint16_t)iads[i].first + (uint16_t)iads[i].count))) {
            return 1u;
        }
    }
    return 0u;
}

static inline int32_t usb_desc_check_config_desc(uint8_t speed, const uint8_t *buffer,
                                                 uint32_t len, uint32_t *err_offset)
{
    struct usb_desc_check_iface ifs[USB_DESC_CHECK_MAX_IFACE];
    struct usb_desc_check_iad   iads[USB_DESC_CHECK_MAX_IAD];

    uint8_t  alt_next[USB_DESC_CHECK_MAX_IFACE];
    uint8_t  ep_mask[32];
    uint8_t  ep_owner[32];

    uint32_t total;
    uint32_t off;
    uint8_t  num_iface;
    uint8_t  if_num     = 0u;
    uint8_t  if_seen    = 0u;
    uint8_t  if_cnt     = 0u;
    uint8_t  iad_cnt    = 0u;
    uint8_t  in_iface   = 0u;
    uint8_t  need_comp  = 0u;
    uint32_t cur_if_off = 0u;
    uint8_t  cur_ep_decl = 0u;
    uint8_t  cur_ep_cnt  = 0u;
    uint8_t  i;
    uint8_t  j;
    int32_t  rc;

    if (err_offset != NULL) {
        *err_offset = 0u;
    }

    if (buffer == NULL) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_PARAM, 0u, err_offset);
    }
    if (len < (uint32_t)USB_SIZEOF_CONFIG_DESC) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_BUF_SHORT, 0u, err_offset);
    }
    if ((speed != (uint8_t)USB_SPEED_LOW) && (speed != (uint8_t)USB_SPEED_FULL) &&
        (speed != (uint8_t)USB_SPEED_HIGH) && (speed != (uint8_t)USB_SPEED_SUPER) &&
        (speed != (uint8_t)USB_SPEED_SUPER_PLUS)) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_SPEED, 0u, err_offset);
    }

    if ((buffer[CFG_OFF_BLENGTH] != (uint8_t)USB_SIZEOF_CONFIG_DESC) ||
        (buffer[CFG_OFF_TYPE] != (uint8_t)USB_DESCRIPTOR_TYPE_CONFIGURATION)) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_CFG_HEADER, 0u, err_offset);
    }

    total = (uint32_t)usb_desc_check_rd_u16(&buffer[CFG_OFF_TOTAL_LEN]);
    if (total != len) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_TOTAL_LEN, 0u, err_offset);
    }

    num_iface = buffer[CFG_OFF_NUM_IFACE];
    if ((num_iface == 0u) || (num_iface > USB_DESC_CHECK_MAX_IFACE)) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_IFACE_NUM, 0u, err_offset);
    }

    for (i = 0u; i < USB_DESC_CHECK_MAX_IFACE; i++) {
        alt_next[i]  = 0u;
        ifs[i].off   = 0u;
        ifs[i].cls   = 0u;
        ifs[i].sub   = 0u;
        ifs[i].proto = 0u;
        ifs[i].cdc_union = 0u;
    }
    for (i = 0u; i < 32u; i++) {
        ep_mask[i]  = 0u;
        ep_owner[i] = 0xFFu;
    }

    off = (uint32_t)USB_SIZEOF_CONFIG_DESC;

    while (off < total) {
        uint8_t bl;
        uint8_t type;

        if ((off + 2u) > total) {
            return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_LEN, off, err_offset);
        }

        bl   = buffer[off];
        type = buffer[off + 1u];

        if ((bl <= 2u) || ((off + (uint32_t)bl) > total)) {
            return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_LEN, off, err_offset);
        }

        if (type == USB_DESCRIPTOR_TYPE_INTERFACE) {
            uint8_t n;
            uint8_t alt;
            uint8_t nep;

            if (need_comp != 0u) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COMPANION, off, err_offset);
            }
            if (bl != USB_DESC_CHECK_IF_LEN) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_LEN, off, err_offset);
            }

            n   = buffer[off + IF_OFF_NUM];
            alt = buffer[off + IF_OFF_ALT];
            nep = buffer[off + IF_OFF_NUM_EP];

            if (in_iface != 0u) {
                if (cur_ep_cnt != cur_ep_decl) {
                    return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COUNT,
                                               cur_if_off, err_offset);
                }
            }

            if (if_seen == 0u) {
                if (n != 0u) {
                    return usb_desc_check_fail(USB_DESC_CHECK_ERR_IFACE_NUM, off, err_offset);
                }
                if_seen = 1u;
                if_num  = n;
                if_cnt  = 1u;
            } else if (n != if_num) {
                if (n != (uint8_t)(if_num + 1u)) {
                    return usb_desc_check_fail(USB_DESC_CHECK_ERR_IFACE_NUM, off, err_offset);
                }
                if_cnt++;
                if_num = n;
            } else {
            }

            if ((n >= num_iface) || (if_cnt > USB_DESC_CHECK_MAX_IFACE)) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_IFACE_NUM, off, err_offset);
            }

            if ((alt_next[n] == 0xFFu) || (alt != alt_next[n])) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_ALT_SETTING, off, err_offset);
            }
            alt_next[n]++;

            if (alt == 0u) {
                ifs[n].off   = (uint16_t)off;
                ifs[n].cls   = buffer[off + IF_OFF_CLASS];
                ifs[n].sub   = buffer[off + IF_OFF_SUBCLASS];
                ifs[n].proto = buffer[off + IF_OFF_PROTOCOL];
            }

            in_iface    = 1u;
            cur_if_off  = off;
            cur_ep_decl = nep;
            cur_ep_cnt  = 0u;
            for (j = 0u; j < 32u; j++) {
                ep_mask[j] = 0u;
            }
        } else if (type == USB_DESCRIPTOR_TYPE_ENDPOINT) {
            uint8_t  ea;
            uint8_t  at;
            uint8_t  ep_num;
            uint8_t  dir_in;
            uint8_t  oidx;
            uint16_t mps;

            if (in_iface == 0u) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_ORDER, off, err_offset);
            }
            if (need_comp != 0u) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COMPANION, off, err_offset);
            }
            if (bl < USB_DESC_CHECK_EP_MIN_LEN) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_LEN, off, err_offset);
            }

            ea     = buffer[off + EP_OFF_ADDR];
            at     = (uint8_t)USB_GET_ENDPOINT_TYPE(buffer[off + EP_OFF_ATTR]);
            ep_num = (uint8_t)(ea & 0x0Fu);
            dir_in = (uint8_t)(((ea & USB_EP_DIR_MASK) != 0u) ? 1u : 0u);
            mps    = usb_desc_check_rd_u16(&buffer[off + EP_OFF_MPS]);

            if ((ep_num == 0u) || ((ea & 0x70u) != 0u)) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_ADDR, off, err_offset);
            }
            if (at == USB_ENDPOINT_TYPE_CONTROL) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_ATTR, off, err_offset);
            }

            oidx = (uint8_t)(ep_num + ((dir_in != 0u) ? 16u : 0u));
            if (ep_mask[oidx] != 0u) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_DUP, off, err_offset);
            }
            ep_mask[oidx] = 1u;

            if (ep_owner[oidx] == 0xFFu) {
                ep_owner[oidx] = if_num;
            } else if (ep_owner[oidx] != if_num) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_DUP, off, err_offset);
            }

            cur_ep_cnt++;
            if (cur_ep_cnt > cur_ep_decl) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COUNT, cur_if_off, err_offset);
            }

            rc = usb_desc_check_ep_mps(speed, at, mps);
            if (rc != USB_DESC_CHECK_OK) {
                return usb_desc_check_fail(rc, off, err_offset);
            }
            rc = usb_desc_check_ep_interval(speed, at, buffer[off + EP_OFF_INTERVAL]);
            if (rc != USB_DESC_CHECK_OK) {
                return usb_desc_check_fail(rc, off, err_offset);
            }

            if (usb_desc_check_is_ss(speed) != 0u) {
                need_comp = 1u;
            }
        } else if (type == USB_DESCRIPTOR_TYPE_SUPERSPEED_ENDPOINT_COMPANION) {
            if ((need_comp == 0u) || (bl != USB_SIZEOF_ENDPOINT_COMPANION_DESC)) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COMPANION, off, err_offset);
            }
            need_comp = 0u;
        } else if (type == USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION) {
            if (bl != USB_DESC_CHECK_IAD_LEN) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_IAD, off, err_offset);
            }
            if (iad_cnt >= USB_DESC_CHECK_MAX_IAD) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_IAD, off, err_offset);
            }
            iads[iad_cnt].off      = (uint16_t)off;
            iads[iad_cnt].first    = buffer[off + 2u];
            iads[iad_cnt].count    = buffer[off + 3u];
            iads[iad_cnt].func_cls   = buffer[off + 4u];
            iads[iad_cnt].func_sub   = buffer[off + 5u];
            iads[iad_cnt].func_proto = buffer[off + 6u];
            iad_cnt++;
        } else if ((type == USB_CS_DESCRIPTOR_TYPE_INTERFACE) ||
                   (type == USB_CS_DESCRIPTOR_TYPE_ENDPOINT)) {
            if (bl <= 3u) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_LEN, off, err_offset);
            }
            if ((type == USB_CS_DESCRIPTOR_TYPE_INTERFACE) && (in_iface != 0u) &&
                (alt_next[if_num] == 1u) && (ifs[if_num].cls == USB_CLASS_CDC) &&
                (buffer[off + 2u] == USB_DESC_CHECK_CDC_UNION)) {
                if (buffer[off + 3u] != if_num) {
                    return usb_desc_check_fail(USB_DESC_CHECK_ERR_CDC, off, err_offset);
                }
                for (j = 4u; j < bl; j++) {
                    if ((buffer[off + j] >= num_iface) || (buffer[off + j] == if_num)) {
                        return usb_desc_check_fail(USB_DESC_CHECK_ERR_CDC, off, err_offset);
                    }
                }
                ifs[if_num].cdc_union = 1u;
            }
        } else {
        }

        off += (uint32_t)bl;
    }

    if (need_comp != 0u) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COMPANION, total, err_offset);
    }
    if (in_iface != 0u) {
        if (cur_ep_cnt != cur_ep_decl) {
            return usb_desc_check_fail(USB_DESC_CHECK_ERR_EP_COUNT, cur_if_off, err_offset);
        }
    }
    if (off != total) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_DESC_LEN, off, err_offset);
    }
    if (if_cnt != num_iface) {
        return usb_desc_check_fail(USB_DESC_CHECK_ERR_IFACE_NUM, 0u, err_offset);
    }

    for (i = 0u; i < iad_cnt; i++) {
        if (iads[i].count < 2u) {
            return usb_desc_check_fail(USB_DESC_CHECK_ERR_IAD, iads[i].off, err_offset);
        }
        if (((uint16_t)iads[i].first + (uint16_t)iads[i].count) > (uint16_t)num_iface) {
            return usb_desc_check_fail(USB_DESC_CHECK_ERR_IAD, iads[i].off, err_offset);
        }
        if (ifs[iads[i].first].off <= iads[i].off) {
            return usb_desc_check_fail(USB_DESC_CHECK_ERR_IAD, iads[i].off, err_offset);
        }
        for (j = 0u; j < i; j++) {
            uint16_t a0 = iads[j].first;
            uint16_t a1 = (uint16_t)(a0 + iads[j].count);
            uint16_t b0 = iads[i].first;
            uint16_t b1 = (uint16_t)(b0 + iads[i].count);

            if ((b0 < a1) && (a0 < b1)) {
                return usb_desc_check_fail(USB_DESC_CHECK_ERR_IAD, iads[i].off, err_offset);
            }
        }

        rc = usb_desc_check_iad_group(&iads[i], ifs);
        if (rc != USB_DESC_CHECK_OK) {
            return usb_desc_check_fail(rc, iads[i].off, err_offset);
        }
    }

    for (i = 0u; i < num_iface; i++) {
        if (usb_desc_check_is_ctrl(ifs[i].cls, ifs[i].sub, ifs[i].proto) == 0u) {
            continue;
        }
        if ((ifs[i].cls == USB_CLASS_CDC) && (ifs[i].cdc_union != 0u)) {
            continue;
        }
        if (usb_desc_check_iad_covers(iads, iad_cnt, ifs, i) == 0u) {
            return usb_desc_check_fail(usb_desc_check_group_err(ifs[i].cls),
                                       ifs[i].off, err_offset);
        }
    }

    return USB_DESC_CHECK_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* USB_DESC_CHECK_H */
