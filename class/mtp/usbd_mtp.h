/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_MTP_H
#define USBD_MTP_H

#include "usbd_mtp_config.h"

#include "usb_mtp.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>

/* gcc toolchain does not implement dirent.h, so we define our own MTP_DIR and mtp_dirent */

typedef void MTP_DIR;

struct mtp_statfs {
    size_t f_bsize;  /* block size */
    size_t f_blocks; /* total data blocks in file system */
    size_t f_bfree;  /* free blocks in file system */
};

struct mtp_dirent {
    uint8_t d_type;                              /* The type of the file */
    uint8_t d_namlen;                            /* The length of the not including the terminating null file name */
    uint16_t d_reclen;                           /* length of this record */
    char d_name[CONFIG_USBDEV_MTP_MAX_PATHNAME]; /* The null-terminated file name */
};

#ifdef __cplusplus
extern "C" {
#endif

/* mode bits of usbd_mtp_open() (a port maps them to its own open/fopen flags) */
#define MTP_OPEN_READ  0x01
#define MTP_OPEN_WRITE 0x02
#define MTP_OPEN_TRUNC 0x04

struct usbd_interface *usbd_mtp_init_intf(uint8_t busid,
                                          struct usbd_interface *intf,
                                          const uint8_t out_ep,
                                          const uint8_t in_ep,
                                          const uint8_t int_ep);

/* Called by the port when the host selects the configuration (the event is dispatched in interrupt
 * context: this function only sets flags and arms the endpoint, cleanup is left to the class task) */
void usbd_mtp_on_configured(uint8_t busid);

/* ---- optional extensions (a port implements them as needed; without them the weak defaults are
 * used, see the end of usbd_mtp.c) ---- */

/* Multiple volumes: implementing this hook exposes several storages (one storage id = index+1 per
 * volume). Without it the single root model usbd_mtp_fs_root_path() / usbd_mtp_fs_description() is used. */
struct usbd_mtp_storage {
    uint32_t id;             /* stable storage id (0 = derive as index + 1).
                              * Keep it stable across expose/unexpose: hosts cache the
                              * storage id together with object handles, so renumbering
                              * volumes invalidates their caches. */
    const char *description; /* volume name shown on the host, e.g. "LittleFS" / "FAT" */
    const char *label;       /* volume label, e.g. "lfs" / "fat" */
    const char *root;        /* root path of this volume in the local filesystem, e.g. "/spiflash" */
    bool read_only;
};

/** Get volume number index; false means there is none left (weak default: returns false, so the single root model is used) */
int usbd_mtp_fs_storage_get(uint32_t index, struct usbd_mtp_storage *out);

/** Dynamic device identity (model and serial number usually follow the hardware); NULL means the static default is used */
enum {
    MTP_DEVNAME_MANUFACTURER = 0,
    MTP_DEVNAME_MODEL,
    MTP_DEVNAME_VERSION,
    MTP_DEVNAME_SERIAL,
};
const char *usbd_mtp_fs_device_name(uint32_t which, const char *fallback);

/** Extension description string of DeviceInfo (the weak default already contains android.com; read the
 * project's MTP troubleshooting notes (traps.md) before changing it) */
const char *usbd_mtp_fs_extension_string(void);

/* ---- optional hooks (not in the template, but some operations need them; the weak defaults report "not supported") ----
 *   seek    : position by offset (needed for partial read/write; returns 0 on success)
 *   truncate: truncate to a given length (needed by TruncateObject 0x95C3)
 *   rename  : rename/move (needed by MoveObject 0x1019 and by renaming through SetObjectPropValue)
 *   sync    : flush written data until the directory entry is visible (some filesystems only report the
 *             correct length to the host after f_sync) */
int usbd_mtp_seek(int fd, uint64_t offset);
int usbd_mtp_truncate(const char *path, uint64_t length);
int usbd_mtp_rename(const char *old_path, const char *new_path);
int usbd_mtp_sync(int fd);

/* ---- state and statistics (for field diagnostics) ---- */
struct usbd_mtp_stat {
    uint32_t rx_bytes;      /* bytes received on bulk OUT */
    uint32_t data_bytes;    /* payload bytes of the DATA containers among them (amount of uploaded data) */
    uint32_t cmds;          /* number of commands parsed */
    uint32_t drops;         /* drops because the ring was full (normally always 0: backpressure via NAK) */
    uint32_t tx_timeouts;   /* bulk IN timeouts waiting for the host to read */
    uint32_t sink_enq;      /* bytes enqueued into the sink */
    uint32_t sink_written;  /* bytes actually written to disk */
    uint32_t sink_fails;    /* number of sink write failures */
};

void usbd_mtp_get_stat(uint8_t busid, struct usbd_mtp_stat *out);

/** Whether the host has selected this configuration (true once enumeration is done) */
bool usbd_mtp_host_configured(uint8_t busid);
void usbd_mtp_set_trace(bool on);
bool usbd_mtp_get_trace(void);

int usbd_mtp_notify_object_add(const char *path);
int usbd_mtp_notify_object_remove(const char *path);

const char *usbd_mtp_fs_root_path(void);
const char *usbd_mtp_fs_description(void);

int usbd_mtp_mkdir(const char *path);
int usbd_mtp_rmdir(const char *path);
MTP_DIR *usbd_mtp_opendir(const char *name);
int usbd_mtp_closedir(MTP_DIR *d);
struct mtp_dirent *usbd_mtp_readdir(MTP_DIR *d);

int usbd_mtp_statfs(const char *path, struct mtp_statfs *buf);
int usbd_mtp_stat(const char *file, struct stat *buf);

int usbd_mtp_open(const char *path, uint8_t mode);
int usbd_mtp_close(int fd);
int usbd_mtp_read(int fd, void *buf, size_t len);
int usbd_mtp_write(int fd, const void *buf, size_t len);

int usbd_mtp_unlink(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* USBD_MTP_H */
