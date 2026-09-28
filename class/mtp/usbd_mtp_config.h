/*
 * Copyright (c) 2026, Jinsc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USBD_MTP_CONFIG_H
#define USBD_MTP_CONFIG_H

/* Options reuse the existing CherryUSB naming (CONFIG_USBDEV_MTP_MAX_BUFSIZE / MAX_OBJECTS /
 * MAX_PATHNAME / THREAD / PRIO / STACKSIZE from cherryusb_config_template.h) with defaults given
 * here; a port overrides them in its own usb_config.h.
 * The options added by this implementation all use the CONFIG_USBDEV_MTP_* prefix to stay in style.
 *
 * The host compatibility reasons behind the values below are recorded in the project's MTP
 * troubleshooting notes (traps.md), read them before changing anything here. */

#ifndef CONFIG_USBDEV_MTP_MAX_BUFSIZE
#define CONFIG_USBDEV_MTP_MAX_BUFSIZE 2048 /* non-streaming container buffer (DeviceInfo/StorageInfo/ObjectInfo/properties) */
#endif

#ifndef CONFIG_USBDEV_MTP_MAX_OBJECTS
#define CONFIG_USBDEV_MTP_MAX_OBJECTS 256 /* max objects returned by one GetObjectHandles */
#endif

#ifndef CONFIG_USBDEV_MTP_MAX_PATHNAME
#define CONFIG_USBDEV_MTP_MAX_PATHNAME 256 /* max name / in-volume path length (bytes, UTF-8) */
#endif

/* Task stack: the object layer walks directories recursively (one relative path buffer per level),
 * 4KB overflows the stack */
#ifndef CONFIG_USBDEV_MTP_STACKSIZE
#define CONFIG_USBDEV_MTP_STACKSIZE 8192
#endif

#ifndef CONFIG_USBDEV_MTP_PRIO
#define CONFIG_USBDEV_MTP_PRIO 5
#endif

/* bulk endpoint max packet size: must match wMaxPacketSize in the descriptors (the ZLP rule uses it) */
#ifndef CONFIG_USBDEV_MTP_EP_MPS
#define CONFIG_USBDEV_MTP_EP_MPS 64
#endif

/* ---------------- options added by this implementation ---------------- */

/* Receive ring: pushed by the ISR, popped by the task; a full ring just does not re-arm the endpoint
 * (the host sees NAK), which gives zero packet loss backpressure.
 * It **must live in memory the ISR can access in a single cycle** (on ESP32: internal RAM; the ISR
 * must not touch PSRAM -- it dies while the flash cache is disabled during a write). */
#ifndef CONFIG_USBDEV_MTP_RX_RING_SIZE
#define CONFIG_USBDEV_MTP_RX_RING_SIZE 8192
#endif

/* Length of one pending bulk OUT transfer (absorbs bursts; bigger means fewer interrupt and re-arm overheads) */
#ifndef CONFIG_USBDEV_MTP_RX_ARM_SIZE
#define CONFIG_USBDEV_MTP_RX_ARM_SIZE 1024
#endif

/* Max directory tree walk / recursion depth */
#ifndef CONFIG_USBDEV_MTP_WALK_DEPTH
#define CONFIG_USBDEV_MTP_WALK_DEPTH 8
#endif

/* Sink stream buffer: the heart of decoupling reception from disk writes (no waiting for the disk
 * while the host bursts). The large size is preferred; if allocation fails, fall back to the BACKUP
 * size (still fully functional, just easier to be slowed down by the disk). */
#ifndef CONFIG_USBDEV_MTP_SINK_SIZE
#define CONFIG_USBDEV_MTP_SINK_SIZE (192 * 1024)
#endif
#ifndef CONFIG_USBDEV_MTP_SINK_SIZE_BACKUP
#define CONFIG_USBDEV_MTP_SINK_SIZE_BACKUP (24 * 1024)
#endif

/* One disk write chunk: keep it >4KB so the filesystem/flash translation layer gets whole sectors
 * (measured throughput differs by 64 times) */
#ifndef CONFIG_USBDEV_MTP_WR_CHUNK
#define CONFIG_USBDEV_MTP_WR_CHUNK (16 * 1024)
#endif

/* ---------------- timeouts ----------------
 * Values are derived from the **host side timeout budget**: libmtp uses a 20s bulk transfer timeout
 * (libmtp: USB_TIMEOUT_DEFAULT = 20000), so every wait on the device side has to stay clearly below
 * it, otherwise the host times out first and the stream loses sync. */
#define MTP_DATA_TMO_MS   20000 /* overall limit for one data container */
#define MTP_TX_TMO_MS     5000  /* wait for one bulk IN to complete */
#define MTP_SINK_WAIT_MS  12000 /* producer waits for the sink to free space, last resort */
#define MTP_SINK_IDLE_MS  20    /* idle poll period of the write task (upper bound of barrier latency) */
#define MTP_CMD_TMO_MS    2000  /* wait for one command container to arrive in full */

/* DeviceInfo StandardVersion / VendorExtensionVersion */
#define MTP_DEVICE_VERSION 100
#define MTP_RXTMO_MS      5000  /* wait for the next batch of bytes inside a data container */

#endif /* USBD_MTP_CONFIG_H */
