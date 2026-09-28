/*
 * Copyright (c) 2026, Jinsc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usbd_mtp.c
 * @brief MTP (Media Transfer Protocol / PTP) device class: USB transport, container send/receive and command dispatch
 *
 * Layering:
 *   usbd_mtp.c      (this file) descriptors / EP0 class requests / container send and receive / task
 *                   -- this file alone is the part to rewrite for another USB stack
 *   usbd_mtp_obj.c  object semantics (handles/directories/properties/read and write) plus the sink path
 *   All storage access goes through the **port supplied** filesystem hooks declared in usbd_mtp.h
 *   (usbd_mtp_open/read/write/stat/opendir/...), so the class depends on no particular filesystem or
 *   OS -- threads, semaphores and critical sections all use usb_osal_*.
 *
 * Every reason behind the host compatibility decisions here (libmtp capability bits, the ZLP rule,
 * the ObjectPropDesc layout, volume root listing semantics, ...) is recorded in the project's MTP
 * troubleshooting notes (traps.md); read them before touching this file.
 *
 * Endpoints (must match the descriptors and the FIFO allocation):
 *   bulk OUT  : commands + file write data    bulk IN: file read data + responses    interrupt IN: events
 */

#include "usbd_core.h"
#include "usbd_mtp.h"
#include "usbd_mtp_internal.h"
#include "usbd_mtp_ptp.h"

#include <string.h>

#include "usb_osal.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbd_mtp"
#include "usb_log.h"

/* ---------------- private data per busid ---------------- */

struct usbd_mtp_priv {
    /* endpoints */
    uint8_t out_ep;
    uint8_t in_ep;
    uint8_t int_ep;

    /* receive: the ISR pushes, the task pops; when the ring cannot hold a whole packet the endpoint
     * is not re-armed (the host sees NAK, zero packet loss) */
    uint8_t ring[CONFIG_USBDEV_MTP_RX_RING_SIZE];
    volatile uint32_t ring_w, ring_r;
    usb_osal_sem_t rx_sem; /* ISR -> task: new data in the ring */
    usb_osal_sem_t tx_done; /* IN completed */
    volatile bool ep_deferred; /* ring full, not re-armed yet, the task will do it once there is room */
    volatile bool ep_armed;
    volatile bool configured;

    /* task */
    usb_osal_thread_t thread;

    /* state */
    bool session_open;
    volatile bool reconfigure_pending; /* set by the ISR: post-reconfiguration cleanup runs in the task (an ISR must not touch files or log) */

    /* statistics (first hand field evidence) */
    volatile uint32_t rx_bytes, data_bytes, cmds, drops, tx_timeouts;
};

static struct usbd_mtp_priv g_mtp[CONFIG_USBDEV_MAX_BUS];

/* DMA buffers must be a standalone cache-aligned section (on ESP32: cache line aligned even though DWC2 does no DMA) */
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX static uint8_t g_rx_pkt[CONFIG_USBDEV_MAX_BUS][CONFIG_USBDEV_MTP_RX_ARM_SIZE];
/* The DWC2 port hands these straight to the DMA engine and asserts that they are aligned to
 * CONFIG_USB_ALIGN_SIZE (a cache line, 64 bytes, on chips with D-Cache like the ESP32-P4). Keep
 * every buffer that reaches usbd_ep_start_write() out of the cacheless/again-aligned pool. */
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_tx_buf[CONFIG_USBDEV_MAX_BUS][CONFIG_USBDEV_MTP_MAX_BUFSIZE];

/* Streaming send state (there can only ever be one stream: GetObject and GetPartialObject both run in the task context) */
static uint32_t g_stream_payload;
static uint32_t g_stream_sent;

static bool g_trace;

void usbd_mtp_set_trace(bool on)
{
    g_trace = on;
}

bool usbd_mtp_get_trace(void)
{
    return g_trace;
}

/* ---------------- ring buffer ---------------- */

static uint32_t ring_used(struct usbd_mtp_priv *p)
{
    return (p->ring_w - p->ring_r + CONFIG_USBDEV_MTP_RX_RING_SIZE) % CONFIG_USBDEV_MTP_RX_RING_SIZE;
}

static uint32_t ring_push(struct usbd_mtp_priv *p, const uint8_t *data, uint32_t len)
{
    uint32_t space = CONFIG_USBDEV_MTP_RX_RING_SIZE - 1 - ring_used(p);
    uint32_t i;

    if (len > space) {
        len = space;
    }
    for (i = 0; i < len; i++) {
        p->ring[p->ring_w] = data[i];
        p->ring_w = (p->ring_w + 1) % CONFIG_USBDEV_MTP_RX_RING_SIZE;
    }
    return len;
}

static uint32_t ring_pop(struct usbd_mtp_priv *p, uint8_t *out, uint32_t want)
{
    uint32_t n = ring_used(p);
    uint32_t i;

    if (n > want) {
        n = want;
    }
    for (i = 0; i < n; i++) {
        out[i] = p->ring[p->ring_r];
        p->ring_r = (p->ring_r + 1) % CONFIG_USBDEV_MTP_RX_RING_SIZE;
    }
    return n;
}

/** Re-arm the OUT endpoint once the ring can hold a whole packet (task context; the ISR side is usbd_mtp_ep_out_cb) */
static void mtp_out_rearm(uint8_t busid)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];
    bool arm = false;

    size_t cs_flag = usb_osal_enter_critical_section();
    if (p->ep_deferred &&
        ((CONFIG_USBDEV_MTP_RX_RING_SIZE - 1 - ring_used(p)) >= CONFIG_USBDEV_MTP_RX_ARM_SIZE)) {
        p->ep_deferred = false;
        arm = true;
    }
    usb_osal_leave_critical_section(cs_flag);

    if (arm) {
        p->ep_armed = true;
        usbd_ep_start_read(busid, p->out_ep, g_rx_pkt[busid], sizeof(g_rx_pkt[busid]));
    }
}

/** Wait until the ring holds need bytes; returns false on timeout */
static bool mtp_ring_wait(uint8_t busid, uint32_t need, uint32_t timeout_ms)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];
    uint32_t waited = 0;

    for (;;) {
        if (ring_used(p) >= need) {
            return true;
        }
        if (usb_osal_sem_take(p->rx_sem, 10) == 0) {
            continue;
        }
        waited += 10;
        if (waited >= timeout_ms) {
            return false;
        }
    }
}

/* ---------------- sending ---------------- */

/** Wait for one IN to complete */
static int mtp_ep_write_wait(uint8_t busid, const void *buf, size_t len)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];

    usb_osal_sem_take(p->tx_done, 0); /* clear leftovers */
    usbd_ep_start_write(busid, p->in_ep, buf, len);
    if (usb_osal_sem_take(p->tx_done, MTP_TX_TMO_MS) != 0) {
        p->tx_timeouts++;
        USB_LOG_WRN("bulk IN timeout (host not reading), len=%u\r\n", (unsigned)len);
        return -1;
    }
    return 0;
}

/** Bulk IN packet size of the current connection.
 *  A high speed build can also run at full speed (host port or hub without HS), so the short
 *  packet rule has to follow the negotiated speed instead of the build time value: at FS a
 *  payload that is a multiple of 512 but not of 64 would never be terminated. Read back from
 *  the endpoint, falling back to the configured value before the descriptor has been parsed. */
static uint16_t mtp_in_mps(uint8_t busid)
{
    uint16_t mps = usbd_get_ep_mps(busid, g_mtp[busid].in_ep);

    return mps ? mps : (uint16_t)CONFIG_USBDEV_MTP_EP_MPS;
}

int usbd_mtp_send_container(uint8_t busid, uint16_t type, uint16_t code, uint32_t tid,
                            const uint8_t *payload, size_t payload_len)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];
    size_t total = MTP_CONTAINER_HEADER_SIZE + payload_len;
    uint8_t *buf = g_tx_buf[busid];

    if (total > sizeof(g_tx_buf[busid])) {
        USB_LOG_ERR("container too large: %u\r\n", (unsigned)total);
        return -1;
    }
    buf[0] = (uint8_t)(total & 0xff);
    buf[1] = (uint8_t)((total >> 8) & 0xff);
    buf[2] = (uint8_t)((total >> 16) & 0xff);
    buf[3] = (uint8_t)((total >> 24) & 0xff);
    buf[4] = (uint8_t)(type & 0xff);
    buf[5] = (uint8_t)(type >> 8);
    buf[6] = (uint8_t)(code & 0xff);
    buf[7] = (uint8_t)(code >> 8);
    buf[8] = (uint8_t)(tid & 0xff);
    buf[9] = (uint8_t)((tid >> 8) & 0xff);
    buf[10] = (uint8_t)((tid >> 16) & 0xff);
    buf[11] = (uint8_t)((tid >> 24) & 0xff);
    if (payload != NULL && payload_len != 0) {
        memcpy(buf + MTP_CONTAINER_HEADER_SIZE, payload, payload_len);
    }

    if (mtp_ep_write_wait(busid, buf, total) != 0) {
        return -1;
    }
    /* The whole container is sent at once: the host reads it as a single unit, so a total length that
     * is an exact packet multiple must be followed by a ZLP */
    if ((total % mtp_in_mps(busid)) == 0) {
        usbd_ep_start_write(busid, p->in_ep, NULL, 0);
        (void)usb_osal_sem_take(p->tx_done, 2000);
    }
    return 0;
}

int usbd_mtp_send_response(uint8_t busid, uint16_t code, uint32_t tid)
{
    return usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_RESPONSE, code, tid, NULL, 0);
}

int usbd_mtp_send_response_params(uint8_t busid, uint16_t code, uint32_t tid,
                                  const uint32_t *params, int nparam)
{
    return usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_RESPONSE, code, tid,
                                   (const uint8_t *)params, (size_t)nparam * 4);
}

int usbd_mtp_stream_begin(uint8_t busid, uint16_t type, uint16_t code, uint32_t tid, uint32_t payload_len)
{
    USB_MEM_ALIGNX uint8_t hdr[MTP_CONTAINER_HEADER_SIZE]; /* DMA source: must be aligned */
    uint32_t len = MTP_CONTAINER_HEADER_SIZE + payload_len;

    hdr[0] = (uint8_t)(len & 0xff);
    hdr[1] = (uint8_t)((len >> 8) & 0xff);
    hdr[2] = (uint8_t)((len >> 16) & 0xff);
    hdr[3] = (uint8_t)((len >> 24) & 0xff);
    hdr[4] = (uint8_t)(type & 0xff);
    hdr[5] = (uint8_t)(type >> 8);
    hdr[6] = (uint8_t)(code & 0xff);
    hdr[7] = (uint8_t)(code >> 8);
    hdr[8] = (uint8_t)(tid & 0xff);
    hdr[9] = (uint8_t)((tid >> 8) & 0xff);
    hdr[10] = (uint8_t)((tid >> 16) & 0xff);
    hdr[11] = (uint8_t)((tid >> 24) & 0xff);

    g_stream_payload = payload_len;
    g_stream_sent = MTP_CONTAINER_HEADER_SIZE;
    return mtp_ep_write_wait(busid, hdr, sizeof(hdr));
}

int usbd_mtp_stream_write(uint8_t busid, const uint8_t *data, size_t len)
{
    /* DMA source: the caller's buffer may be unaligned (a stack buffer, a file cache, ...), so copy
     * through an aligned staging buffer like the CherryUSB demo templates do */
    static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t stage[512];
    size_t off = 0;

    while (off < len) {
        size_t chunk = len - off;
        const uint8_t *src = data + off;

        if (chunk > sizeof(stage)) {
            chunk = sizeof(stage); /* keep one transfer small: splitting shortens the gap before the next interrupt/task switch */
        }
        if ((uintptr_t)src % CONFIG_USB_ALIGN_SIZE) {
            memcpy(stage, src, chunk);
            src = stage;
        }
        if (mtp_ep_write_wait(busid, src, chunk) != 0) {
            return -1;
        }
        off += chunk;
        g_stream_sent += (uint32_t)chunk;
    }
    return 0;
}

int usbd_mtp_stream_end(uint8_t busid)
{
    /* The ZLP rule is judged on the **payload length**: reading a payload means the host asks for a
     * large amount at once and loops until it gets a short packet, so a payload that is an exact
     * multiple of the max packet size without a trailing ZLP never yields that short packet (measured:
     * downloading a file whose size is a multiple of 64 always hangs). The container header goes out
     * as a separate transfer and does not take part in this rule. */
    if ((g_stream_payload % mtp_in_mps(busid)) == 0) {
        (void)mtp_ep_write_wait(busid, NULL, 0);
    }
    return 0;
}

bool usbd_mtp_host_configured(uint8_t busid)
{
    return g_mtp[busid].configured;
}

/* ---------------- DeviceInfo / StorageInfo ---------------- */

/** Volume table access: prefer the extended hook (multiple volumes), otherwise fall back to the single root model */
static bool mtp_storage_get(uint32_t index, struct usbd_mtp_storage *out)
{
    if (usbd_mtp_fs_storage_get(index, out)) {
        return true;
    }
    if (index == 0) {
        memset(out, 0, sizeof(*out));
        out->description = usbd_mtp_fs_description();
        out->root = usbd_mtp_fs_root_path();
        out->label = "";
        out->read_only = false;
        return (out->root != NULL);
    }
    return false;
}

/* DeviceInfo: declare **only the operations that are really implemented**. Declaring one and then
 * answering "not supported" makes libmtp PANIC (measured: "could not inspect object property
 * description"). */
static const uint16_t mtp_ops[] = {
    MTP_OPERATION_GET_DEVICE_INFO, MTP_OPERATION_OPEN_SESSION, MTP_OPERATION_CLOSE_SESSION,
    MTP_OPERATION_GET_STORAGE_IDS, MTP_OPERATION_GET_STORAGE_INFO,
    MTP_OPERATION_GET_OBJECT_HANDLES, MTP_OPERATION_GET_OBJECT_INFO, MTP_OPERATION_GET_OBJECT,
    MTP_OPERATION_DELETE_OBJECT, MTP_OPERATION_SEND_OBJECT_INFO, MTP_OPERATION_SEND_OBJECT,
    /* object property layer (gvfs/libmtp asks for the size and the name before reading or writing a
     * file) plus partial transfers */
    MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED, MTP_OPERATION_GET_OBJECT_PROP_DESC,
    MTP_OPERATION_GET_OBJECT_PROP_VALUE, MTP_OPERATION_SET_OBJECT_PROP_VALUE,
    MTP_OPERATION_GET_OBJECT_PROP_LIST, MTP_OPERATION_GET_PARTIAL_OBJECT,
    /* the "writable device" gate of gvfs/libmtp (LIBMTP_Check_Capability) needs these */
    MTP_OPERATION_SEND_PARTIAL_OBJECT,               /* generic 0x101C */
    0x9816,                                          /* UpdateObject = EditObjects */
    MTP_OPERATION_MOVE_OBJECT,
    0x95C1,                                          /* Android GetPartialObject64 */
    MTP_OPERATION_SEND_PARTIAL_OBJECT,               /* same as 0x95C2 (Android variant) */
    MTP_OPERATION_TRUNCATE_OBJECT,                   /* Android TruncateObject 0x95C3 */
    MTP_OPERATION_BEGIN_EDIT_OBJECT,                 /* 0x95C4 */
    MTP_OPERATION_END_EDIT_OBJECT,                   /* 0x95C5 */
};

static const uint16_t mtp_events[] = { 0x4002, 0x4003, 0x4004, 0x4005, 0x4007 };
static const uint16_t mtp_dev_props[] = { 0 };      /* no device properties declared yet (declaring them without a Desc/Value implementation makes the host ask for them) */
static const uint16_t mtp_capture_fmts[] = { MTP_FORMAT_EXIF_JPEG };
static const uint16_t mtp_image_fmts[] = { MTP_FORMAT_UNDEFINED };

static size_t mtp_build_device_info(uint8_t *buf, size_t cap)
{
    size_t off = 0;

    off += mtp_ptp_put16(buf + off, MTP_DEVICE_VERSION); /* StandardVersion */
    off += mtp_ptp_put32(buf + off, 6);                    /* VendorExtensionID (6 = MTP) */
    off += mtp_ptp_put16(buf + off, MTP_DEVICE_VERSION); /* VendorExtensionVersion */
    /* Extension description string: it is **semicolon separated** and must contain android.com --
     * libmtp splits on ';' and gvfs uses it to decide whether the device is readable and writable
     * (with a comma separated string gvfs reports "operation not supported" for everything). */
    off += mtp_ptp_put_str(buf + off, cap - off, usbd_mtp_fs_extension_string());
    off += mtp_ptp_put16(buf + off, 0);                    /* FunctionalMode */
    off += mtp_ptp_put_u16_array(buf + off, mtp_ops, sizeof(mtp_ops) / sizeof(mtp_ops[0]));
    off += mtp_ptp_put_u16_array(buf + off, mtp_events, sizeof(mtp_events) / sizeof(mtp_events[0]));
    off += mtp_ptp_put_u16_array(buf + off, mtp_dev_props, sizeof(mtp_dev_props) / sizeof(mtp_dev_props[0]));
    off += mtp_ptp_put_u16_array(buf + off, mtp_capture_fmts, sizeof(mtp_capture_fmts) / sizeof(mtp_capture_fmts[0]));
    off += mtp_ptp_put_u16_array(buf + off, mtp_image_fmts, sizeof(mtp_image_fmts) / sizeof(mtp_image_fmts[0]));
    /* Device identity: prefer the port's dynamic implementation (model and serial number usually follow
     * the hardware), otherwise use the static defaults */
    off += mtp_ptp_put_str(buf + off, cap - off, usbd_mtp_fs_device_name(MTP_DEVNAME_MANUFACTURER, "Jinsc"));
    off += mtp_ptp_put_str(buf + off, cap - off, usbd_mtp_fs_device_name(MTP_DEVNAME_MODEL, "MTP Device"));
    off += mtp_ptp_put_str(buf + off, cap - off, usbd_mtp_fs_device_name(MTP_DEVNAME_VERSION, "1.0"));
    off += mtp_ptp_put_str(buf + off, cap - off, usbd_mtp_fs_device_name(MTP_DEVNAME_SERIAL, "00000001"));
    return off;
}

/* StorageInfo: libmtp parses it as a **26-byte fixed-size section** (2+2+2+8+8+**4**) --
 * FreeSpaceInObjects is a UINT32, writing it as 64 bits shifts the start of the following
 * description string by 4 bytes and the volume name parses as empty (measured). */
static size_t mtp_build_storage_info(uint8_t *buf, size_t cap, uint32_t storage_id)
{
    struct usbd_mtp_storage st;
    struct mtp_statfs fs;
    uint32_t index = 0;
    struct usbd_mtp_storage probe;
    size_t o = 0;

    /* find the storage whose (stable) id matches; a port may leave id == 0,
     * in which case the class falls back to index + 1 */
    for (;;) {
        if (!mtp_storage_get(index, &probe)) {
            return 0; /* no such storage */
        }
        if ((probe.id != 0 ? probe.id : (index + 1)) == storage_id) {
            break;
        }
        index++;
    }
    st = probe;
    memset(&fs, 0, sizeof(fs));
    (void)usbd_mtp_statfs(st.root, &fs); /* the weak default returns -1: capacity is then shown as 0 */
    o += mtp_ptp_put16(buf + o, 0x0004);                                     /* StorageType: RemovableRAM */
    o += mtp_ptp_put16(buf + o, 0x0002);                                     /* FilesystemType: Hierarchical */
    o += mtp_ptp_put16(buf + o, st.read_only ? 0x0001 : 0x0000);             /* AccessCapability */
    o += mtp_ptp_put64(buf + o, (uint64_t)fs.f_blocks * fs.f_bsize);         /* MaxCapacity */
    o += mtp_ptp_put64(buf + o, (uint64_t)fs.f_bfree * fs.f_bsize);          /* FreeSpaceInBytes */
    o += mtp_ptp_put32(buf + o, 0);                                          /* FreeSpaceInObjects (UINT32!) */
    o += mtp_ptp_put_str(buf + o, cap - o, st.description ? st.description : "Storage");
    o += mtp_ptp_put_str(buf + o, cap - o, st.label ? st.label : "");
    return o;
}

/* ---------------- EP0 class requests (PTP over USB) ---------------- */

static int mtp_class_request(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    (void)busid;
    switch (setup->bRequest) {
        case 0x65: /* GET_EVENT (IN): no pending event -> zero length */
            if (setup->bmRequestType & 0x80) {
                *data = NULL;
                *len = 0;
                return 0;
            }
            return -1;

        case 0x64: /* CANCEL_REQUEST */
        case 0x66: /* DEVICE_RESET: clear the session */
            g_mtp[busid].session_open = false;
            *data = NULL;
            *len = 0;
            return 0;

        case 0x67: { /* GET_DEVICE_STATUS: 4 bytes {len=4, code=OK} */
            static USB_MEM_ALIGNX uint8_t st[4] = { 4, 0, 0x01, 0x20 };
            *data = st;
            *len = sizeof(st);
            return 0;
        }

        default:
            USB_LOG_WRN("unsupported class request 0x%02x\r\n", setup->bRequest);
            return -1;
    }
}

/* ---------------- endpoint callbacks ---------------- */

static void usbd_mtp_ep_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];

    (void)ep;
    (void)nbytes;
    if (p->tx_done != NULL) {
        usb_osal_sem_give(p->tx_done);
    }
}

static void usbd_mtp_ep_int_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes; /* the event has already been sent */
}

static void usbd_mtp_ep_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];
    bool arm = false;

    (void)ep;
    if (nbytes > 0) {
        uint32_t pushed;

        size_t cs_flag = usb_osal_enter_critical_section();
        pushed = ring_push(p, g_rx_pkt[busid], nbytes);
        usb_osal_leave_critical_section(cs_flag);
        p->rx_bytes += pushed;
        if (pushed < nbytes) {
            p->drops++; /* should never happen (the full-ring re-arm trick provides the backpressure) */
        }
    }

    size_t cs_flag = usb_osal_enter_critical_section();
    if ((CONFIG_USBDEV_MTP_RX_RING_SIZE - 1 - ring_used(p)) >= CONFIG_USBDEV_MTP_RX_ARM_SIZE) {
        arm = true;
    } else {
        p->ep_deferred = true;
    }
    usb_osal_leave_critical_section(cs_flag);

    if (arm) {
        usbd_ep_start_read(busid, p->out_ep, g_rx_pkt[busid], sizeof(g_rx_pkt[busid]));
    }

    if (p->rx_sem != NULL) {
        usb_osal_sem_give(p->rx_sem);
    }
}

/* ---------------- receive task: assemble containers and dispatch them ---------------- */

static void mtp_handle_command(uint8_t busid, const struct mtp_container_hdr *hdr,
                               const uint32_t *params, int nparam)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];
    static uint8_t payload[CONFIG_USBDEV_MTP_MAX_BUFSIZE];

    p->cmds++;

    switch (hdr->code) {
        case MTP_OPERATION_GET_DEVICE_INFO:
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload,
                                          mtp_build_device_info(payload, sizeof(payload)));
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            break;

        case MTP_OPERATION_OPEN_SESSION:
            if (p->session_open) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_SESSION_ALREADY_OPEN, hdr->tid);
            } else {
                p->session_open = true;
                USB_LOG_INFO("mtp session opened\r\n");
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            }
            break;

        case MTP_OPERATION_CLOSE_SESSION:
            p->session_open = false;
            usbd_mtp_obj_session_close(busid); /* flush the sink queue first: the host may read a file right away */
            USB_LOG_INFO("mtp session closed\r\n");
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            break;

        case MTP_OPERATION_GET_STORAGE_IDS: {
            struct usbd_mtp_storage st;
            uint32_t n = 0;

            while ((n < 8) && mtp_storage_get(n, &st)) {
                n++;
            }
            if (n == 0) {
                /* No storage at all: the port implemented neither usbd_mtp_fs_storage_get()
                 * nor usbd_mtp_fs_root_path() -- hosts then see a device with no volume. */
                USB_LOG_ERR("no storage registered: implement usbd_mtp_fs_storage_get() or usbd_mtp_fs_root_path()\r\n");
            }
            (void)mtp_ptp_put32(payload, n);
            for (uint32_t i = 0; i < n; i++) {
                struct usbd_mtp_storage st;

                if (mtp_storage_get(i, &st)) {
                    (void)mtp_ptp_put32(payload + 4 + 4 * i, st.id != 0 ? st.id : (i + 1));
                }
            }
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload,
                                          4 + 4 * n);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            break;
        }

        case MTP_OPERATION_GET_STORAGE_INFO: {
            uint32_t want = (nparam > 0) ? params[0] : 1;
            size_t o = mtp_build_storage_info(payload, sizeof(payload), want);

            if (o == 0) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_STORAGE_ID, hdr->tid);
                break;
            }
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, o);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            break;
        }

        default:
            /* Object commands (listing/properties/read/write/delete) are all handled by
             * usbd_mtp_obj.c. Do not leave a branch for the same opcode in this file: it would answer
             * first and the new implementation would never be reached. */
            if (usbd_mtp_obj_command(busid, hdr, params, nparam)) {
                break;
            }
            USB_LOG_WRN("unsupported opcode 0x%04x\r\n", hdr->code);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OPERATION_NOT_SUPPORTED, hdr->tid);
            break;
    }
}

static void usbd_mtp_thread(void *argument)
{
    uint8_t busid = (uint8_t)(uintptr_t)argument;
    struct usbd_mtp_priv *p = &g_mtp[busid];
    uint8_t cmd_buf[256];
    static uint8_t stage[CONFIG_USBDEV_MTP_RX_ARM_SIZE];

    for (;;) {
        /* The host reconfigured the device: leftovers of the previous upload have to be cleaned up
         * (they may be a half written file). **This must be done in the task**: logging or touching
         * files from an ISR takes newlib's recursive lock and aborts right away (measured). */
        if (p->reconfigure_pending) {
            p->reconfigure_pending = false;
            usbd_mtp_obj_reset(busid);
            USB_LOG_INFO("mtp ready, waiting for ptp commands\r\n");
        }

        if (!mtp_ring_wait(busid, MTP_CONTAINER_HEADER_SIZE, MTP_CMD_TMO_MS)) {
            continue;
        }

        struct mtp_container_hdr hdr;
        size_t cs_flag = usb_osal_enter_critical_section();
        {
            uint32_t save_r = p->ring_r; /* peek only, do not consume: wait until the whole container has arrived */
            (void)ring_pop(p, (uint8_t *)&hdr, sizeof(hdr));
            p->ring_r = save_r;
        }
        usb_osal_leave_critical_section(cs_flag);

        /* The length limit only exists to detect a desynchronized stream, it still has to accept the
         * data container of a large file (it used to be 64KB: the data container of a 100KB upload
         * was judged invalid, the ring was flushed and all data was lost). */
        if ((hdr.len < sizeof(hdr)) || (hdr.len > (64u * 1024u * 1024u))) {
            USB_LOG_WRN("bad container length %u, flush ring\r\n", (unsigned)hdr.len);
            size_t cs_flag = usb_osal_enter_critical_section();
            p->ring_r = p->ring_w;
            usb_osal_leave_critical_section(cs_flag);
            mtp_out_rearm(busid);
            continue;
        }

        if ((hdr.type == MTP_CONTAINER_TYPE_COMMAND) && (hdr.len <= sizeof(cmd_buf))) {
            uint32_t params[5] = { 0 };
            int nparam;

            if (!mtp_ring_wait(busid, hdr.len, MTP_CMD_TMO_MS)) {
                continue;
            }
            size_t cs_flag = usb_osal_enter_critical_section();
            (void)ring_pop(p, cmd_buf, hdr.len);
            usb_osal_leave_critical_section(cs_flag);

            nparam = (int)((hdr.len - MTP_CONTAINER_HEADER_SIZE) / 4);
            if (nparam > 5) {
                nparam = 5;
            }
            if (nparam > 0) {
                memcpy(params, cmd_buf + MTP_CONTAINER_HEADER_SIZE, (size_t)nparam * 4);
            }
            mtp_handle_command(busid, &hdr, params, nparam);
        } else if (hdr.type == MTP_CONTAINER_TYPE_DATA) {
            /* Data container: an ObjectInfo dataset or file bytes. **Eat the 12-byte header first**
             * (everything above only peeked at it); forgetting this feeds the header to the object
             * layer as if it were dataset bytes. */
            uint32_t skip = MTP_CONTAINER_HEADER_SIZE;
            uint32_t remain = hdr.len - MTP_CONTAINER_HEADER_SIZE;
            bool broken = false;

            while (skip > 0) {
                uint32_t n;

                size_t cs_flag = usb_osal_enter_critical_section();
                n = ring_pop(p, stage, skip);
                usb_osal_leave_critical_section(cs_flag);
                if (n == 0) {
                    break;
                }
                skip -= n;
            }

            usbd_mtp_obj_data_begin(busid, remain);
            while (remain > 0) {
                uint32_t chunk = (remain > sizeof(stage)) ? sizeof(stage) : remain;
                uint32_t n;

                if (!mtp_ring_wait(busid, 1, MTP_RXTMO_MS)) {
                    break;
                }
                size_t cs_flag = usb_osal_enter_critical_section();
                n = ring_pop(p, stage, chunk);
                usb_osal_leave_critical_section(cs_flag);
                if (n == 0) {
                    break;
                }
                if (!broken) {
                    usbd_mtp_obj_data(busid, stage, n);
                    p->data_bytes += n;
                }
                remain -= n;
                /* **Re-arm after every chunk**: when the ring is full the ISR sets ep_deferred and
                 * gives up re-arming, so re-arming only at the end of the container means that a
                 * large container which fills the ring halfway is never re-armed again and the host
                 * NAKs forever (measured: only 7168 bytes of a 1MB upload came through). */
                mtp_out_rearm(busid);
            }
            if (remain > 0) {
                USB_LOG_WRN("data container incomplete (%u bytes missing)\r\n", (unsigned)remain);
                usbd_mtp_obj_data_truncated(busid);
            }
            usbd_mtp_obj_data_end(busid);
        } else {
            /* other types (events, ...): the host must not send them, just drop them */
            uint32_t remain = hdr.len - MTP_CONTAINER_HEADER_SIZE;

            while (remain > 0) {
                uint32_t chunk = (remain > sizeof(stage)) ? sizeof(stage) : remain;
                uint32_t n;

                if (!mtp_ring_wait(busid, 1, MTP_CMD_TMO_MS)) {
                    break;
                }
                size_t cs_flag = usb_osal_enter_critical_section();
                n = ring_pop(p, stage, chunk);
                usb_osal_leave_critical_section(cs_flag);
                if (n == 0) {
                    break;
                }
                remain -= n;
            }
        }

        mtp_out_rearm(busid);
    }
}

/* ---------------- public: initialization / event hookup ---------------- */

struct usbd_interface *usbd_mtp_init_intf(uint8_t busid, struct usbd_interface *intf,
                                          const uint8_t out_ep, const uint8_t in_ep, const uint8_t int_ep)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];
    static struct usbd_endpoint mtp_out_ep;
    static struct usbd_endpoint mtp_in_ep;
    static struct usbd_endpoint mtp_int_ep;

    memset(p, 0, sizeof(*p));
    p->out_ep = out_ep;
    p->in_ep = in_ep;
    p->int_ep = int_ep;

    intf->class_interface_handler = mtp_class_request;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = NULL;

    mtp_out_ep.ep_addr = out_ep;
    mtp_out_ep.ep_cb = usbd_mtp_ep_out_cb;
    mtp_in_ep.ep_addr = in_ep;
    mtp_in_ep.ep_cb = usbd_mtp_ep_in_cb;
    mtp_int_ep.ep_addr = int_ep;
    mtp_int_ep.ep_cb = usbd_mtp_ep_int_cb;

    usbd_add_endpoint(busid, &mtp_out_ep);
    usbd_add_endpoint(busid, &mtp_in_ep);
    usbd_add_endpoint(busid, &mtp_int_ep);

    p->rx_sem = usb_osal_sem_create(0);
    p->tx_done = usb_osal_sem_create(0);
    if ((p->rx_sem == NULL) || (p->tx_done == NULL)) {
        USB_LOG_ERR("mtp sem create failed\r\n");
        return intf;
    }

    /* sink path (stream buffer + write task) */
    usbd_mtp_obj_init(busid);

    if (p->thread == NULL) {
        p->thread = usb_osal_thread_create("usbd_mtp", CONFIG_USBDEV_MTP_STACKSIZE, CONFIG_USBDEV_MTP_PRIO,
                                           usbd_mtp_thread, (void *)(uintptr_t)busid);
    }

    USB_LOG_INFO("mtp class registered: out=0x%02x in=0x%02x int=0x%02x\r\n", out_ep, in_ep, int_ep);
    return intf;
}

/** Host selected the configuration: called by the port from USBD_EVENT_CONFIGURED (**ISR context**, sets flags and arms the endpoint only) */
void usbd_mtp_on_configured(uint8_t busid)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];

    p->configured = true;
    p->session_open = false;
    p->ep_deferred = false;
    p->ep_armed = true;
    p->reconfigure_pending = true; /* cleanup of an interrupted upload is left to the task (an ISR must not touch files) */
    usbd_ep_start_read(busid, p->out_ep, g_rx_pkt[busid], sizeof(g_rx_pkt[busid]));
    if (p->rx_sem != NULL) {
        usb_osal_sem_give(p->rx_sem);
    }
}

void usbd_mtp_get_stat(uint8_t busid, struct usbd_mtp_stat *out)
{
    struct usbd_mtp_priv *p = &g_mtp[busid];

    if (out == NULL) {
        return;
    }
    out->rx_bytes = p->rx_bytes;
    out->data_bytes = p->data_bytes;
    out->cmds = p->cmds;
    out->drops = p->drops;
    out->tx_timeouts = p->tx_timeouts;
    usbd_mtp_obj_stat(busid, &out->sink_enq, &out->sink_written, &out->sink_fails);
}

int usbd_mtp_notify_object_add(const char *path)
{
    (void)path; /* this implementation sends no asynchronous events (a host polling GET_EVENT always gets "no event") */
    return 0;
}

int usbd_mtp_notify_object_remove(const char *path)
{
    (void)path;
    return 0;
}


/* ---------------- filesystem hooks: weak default implementations ----------------
 * Every hook the class calls has a weak default that logs an error and reports failure, so a
 * port that forgets one gets a clear runtime message instead of an unresolved symbol at link
 * time -- the same pattern the other CherryUSB device classes use for their port hooks.
 * The port's strong definitions override these. */
__WEAK int usbd_mtp_open(const char *path, uint8_t mode)
{
    (void)path;
    (void)mode;
    USB_LOG_ERR("usbd_mtp_open is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_close(int fd)
{
    (void)fd;
    USB_LOG_ERR("usbd_mtp_close is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_read(int fd, void *buf, size_t len)
{
    (void)fd;
    (void)buf;
    (void)len;
    USB_LOG_ERR("usbd_mtp_read is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_write(int fd, const void *buf, size_t len)
{
    (void)fd;
    (void)buf;
    (void)len;
    USB_LOG_ERR("usbd_mtp_write is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_stat(const char *path, struct stat *buf)
{
    (void)path;
    (void)buf;
    USB_LOG_ERR("usbd_mtp_stat is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_mkdir(const char *path)
{
    (void)path;
    USB_LOG_ERR("usbd_mtp_mkdir is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_rmdir(const char *path)
{
    (void)path;
    USB_LOG_ERR("usbd_mtp_rmdir is not implemented\r\n");
    return -1;
}

__WEAK int usbd_mtp_unlink(const char *path)
{
    (void)path;
    USB_LOG_ERR("usbd_mtp_unlink is not implemented\r\n");
    return -1;
}

__WEAK MTP_DIR *usbd_mtp_opendir(const char *name)
{
    (void)name;
    USB_LOG_ERR("usbd_mtp_opendir is not implemented\r\n");
    return NULL;
}

__WEAK struct mtp_dirent *usbd_mtp_readdir(MTP_DIR *d)
{
    (void)d;
    USB_LOG_ERR("usbd_mtp_readdir is not implemented\r\n");
    return NULL;
}

__WEAK int usbd_mtp_closedir(MTP_DIR *d)
{
    (void)d;
    USB_LOG_ERR("usbd_mtp_closedir is not implemented\r\n");
    return -1;
}

/* ---------------- optional porting hooks: weak default implementations ----------------
 * A port implements only what it needs:
 *   - multiple volumes: usbd_mtp_fs_storage_get()
 *   - single volume: usbd_mtp_fs_root_path() / usbd_mtp_fs_description()
 *   - capacity: usbd_mtp_statfs()
 *   - device identity / extension string: usbd_mtp_fs_device_name() / usbd_mtp_fs_extension_string()
 */

__WEAK int usbd_mtp_fs_storage_get(uint32_t index, struct usbd_mtp_storage *out)
{
    (void)index;
    (void)out;
    return 0;
}

__WEAK const char *usbd_mtp_fs_root_path(void)
{
    return NULL;
}

__WEAK const char *usbd_mtp_fs_description(void)
{
    return "MTP";
}

__WEAK int usbd_mtp_statfs(const char *path, struct mtp_statfs *buf)
{
    (void)path;
    (void)buf;
    return -1;
}

__WEAK const char *usbd_mtp_fs_extension_string(void)
{
    /* semicolon separated and containing android.com: libmtp splits on ';' and gvfs uses it to decide the device is writable */
    return "microsoft.com: 1.0; android.com: 1.0; ";
}

__WEAK const char *usbd_mtp_fs_device_name(uint32_t which, const char *fallback)
{
    (void)which;
    return fallback;
}

__WEAK int usbd_mtp_seek(int fd, uint64_t offset)
{
    (void)fd;
    (void)offset;
    return -1;
}

__WEAK int usbd_mtp_truncate(const char *path, uint64_t length)
{
    (void)path;
    (void)length;
    return -1;
}

__WEAK int usbd_mtp_rename(const char *old_path, const char *new_path)
{
    (void)old_path;
    (void)new_path;
    return -1;
}

__WEAK int usbd_mtp_sync(int fd)
{
    (void)fd;
    return 0; /* default: writes are immediately visible (most RTOS filesystems); a port that needs f_sync implements it */
}
