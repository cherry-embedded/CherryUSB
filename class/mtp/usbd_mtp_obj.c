/*
 * Copyright (c) 2026, Jinsc
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usbd_mtp_obj.c
 * @brief MTP object layer: directories, properties, file read/write and the sink path
 *
 * All file access goes through the **port supplied** hooks declared in usbd_mtp.h (open/read/write/
 * close/stat/opendir/readdir/closedir/mkdir/rmdir/unlink plus the optional truncate/rename/sync), this
 * layer knows nothing about any particular filesystem.
 *
 * Handle scheme: **31-bit FNV-1a hash of the path (computed on the lower case form)** -- no catalog
 * table, constant memory usage, stable across sessions; resolving a handle back to a path walks the
 * volume tree, and a "last hit" cache covers the consecutive lookups the host makes. Lower case
 * because FAT upper cases file names.
 *
 * Sink path (reception decoupled from disk writes): a data container is only memcpy'ed into the stream
 * buffer and the upload response goes out as soon as the data is queued; a separate write task writes
 * it to disk in large chunks. Every read-like command, delete, rename, truncate, EndEdit and
 * CloseSession calls sink_barrier() first (queue empty + current chunk written + synced) so that the
 * host always reads complete content.
 *
 * The host compatibility reasons behind the details below (FAT directory entry lengths, the
 * ObjectPropDesc layout, volume root listing semantics, the upload state machine, ...) are recorded in
 * the project's MTP troubleshooting notes (traps.md); read them before changing this file.
 */

#include "usbd_core.h"
#include "usbd_mtp.h"
#include "usbd_mtp_internal.h"
#include "usbd_mtp_ptp.h"

#include <string.h>
#include <sys/stat.h>

#include "usb_osal.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbd_mtp"
#include "usb_log.h"

/* object layer limits */
#define MTP_NAME_MAX CONFIG_USBDEV_MTP_MAX_PATHNAME
#define MTP_PATH_MAX CONFIG_USBDEV_MTP_MAX_PATHNAME
#define MTP_WALK_DEPTH CONFIG_USBDEV_MTP_WALK_DEPTH

/* object properties (only these few are enabled; enabling more requires implementing their Desc/Value,
 * the host PANICs when it cannot get them) */
#define MTP_PROP_OBJECTSIZE   0xDC04u
#define MTP_PROP_FILENAME     0xDC07u
#define MTP_PROP_DATECREATED  0xDC08u
#define MTP_PROP_DATEMODIFIED 0xDC09u

#define OBJINFO_FIXED 52      /* fixed-size part before the filename (16-bit AssociationType layout) */
#define OBJINFO_FIXED_WIN64 56 /* some hosts (Windows) send a 64-bit ObjectSize */

#define MTP_SINK_SIZE CONFIG_USBDEV_MTP_SINK_SIZE
#define MTP_SINK_SIZE_BACKUP CONFIG_USBDEV_MTP_SINK_SIZE_BACKUP

/* ---------------- small helpers ---------------- */

/** Join a child path; every path has to be built here (handles hash the path string, so the format must stay identical) */
static bool join_path(char *out, size_t n, const char *dir, const char *name)
{
    size_t dl = strlen(dir), nl = strlen(name);

    /* decided in bytes and **never truncated halfway**: names and paths are mostly multi-byte UTF-8
     * and a truncation would split one character in two */
    if (dl + 1 + nl + 1 > n) {
        return false;
    }
    memcpy(out, dir, dl);
    out[dl] = '/';
    memcpy(out + dl + 1, name, nl + 1);
    return true;
}

static uint32_t path_handle(const char *path)
{
    uint32_t h = 2166136261u;

    for (const char *p = path; *p; p++) {
        char c = *p;

        if ((c >= 'A') && (c <= 'Z')) {
            c = (char)(c - 'A' + 'a'); /* hash the lower case form: FAT upper cases file names */
        }
        h ^= (uint8_t)c;
        h *= 16777619u;
    }
    h &= 0x7fffffffu;
    if ((h == 0) || (h == 0xffffffffu)) {
        h = 1;
    }
    return h;
}

/* ---------------- stale handle aliases ----------------
 * A handle is a hash of the path, so a rename changes it -- but hosts keep using the handle they
 * cached *before* the rename: gvfs (do_set_display_name / do_move) updates its path table with the
 * old id after calling SetObjectPropValue. Without remembering the mapping the host's next call on
 * that object fails (user sees "rename failed" although it went through, and a refresh, which
 * re-enumerates and gets fresh handles, makes it look fine again).
 * A small ring of old handle -> current path is enough: hosts re-enumerate after a reconnect, so
 * nothing has to survive a power cycle. Every lookup is verified with a stat, so an alias of a
 * deleted (or renamed again) object simply stops resolving. */
#define MTP_ALIAS_MAX 32

static struct {
    uint32_t handle;
    uint32_t storage;
    char path[MTP_PATH_MAX];
} s_alias[MTP_ALIAS_MAX];
static uint8_t s_alias_next;

static void alias_add(uint32_t handle, uint32_t storage, const char *path)
{
    if ((handle == 0) || (path == NULL) || (path[0] == '\0')) {
        return;
    }
    s_alias[s_alias_next].handle = handle;
    s_alias[s_alias_next].storage = storage;
    snprintf(s_alias[s_alias_next].path, sizeof(s_alias[s_alias_next].path), "%s", path);
    s_alias_next = (uint8_t)((s_alias_next + 1) % MTP_ALIAS_MAX);
}

static void alias_forget(const char *path)
{
    for (uint32_t i = 0; i < MTP_ALIAS_MAX; i++) {
        if ((s_alias[i].handle != 0) && (strcmp(s_alias[i].path, path) == 0)) {
            s_alias[i].handle = 0;
        }
    }
}

static bool alias_lookup(uint32_t want, char *out, size_t out_n, uint32_t *storage_out)
{
    for (uint32_t i = 0; i < MTP_ALIAS_MAX; i++) {
        struct stat st;

        if ((s_alias[i].handle == 0) || (s_alias[i].handle != want)) {
            continue;
        }
        if (usbd_mtp_stat(s_alias[i].path, &st) != 0) {
            s_alias[i].handle = 0; /* the object is gone, the alias goes with it */
            continue;
        }
        snprintf(out, out_n, "%s", s_alias[i].path);
        if (storage_out != NULL) {
            *storage_out = s_alias[i].storage;
        }
        return true;
    }
    return false;
}

/* ---------------- volume access ---------------- */

static bool volume_at(uint32_t index, struct usbd_mtp_storage *out);

static uint32_t volume_id(uint32_t index, struct usbd_mtp_storage *out)
{
    /* stable id supplied by the port; fall back to index + 1 */
    return (out->id != 0) ? out->id : (index + 1);
}

static bool volume_by_id(uint32_t storage_id, struct usbd_mtp_storage *out)
{
    struct usbd_mtp_storage probe;
    uint32_t index = 0;

    while (volume_at(index, &probe)) {
        if (volume_id(index, &probe) == storage_id) {
            *out = probe;
            return true;
        }
        index++;
    }
    return false;
}

static bool volume_at(uint32_t index, struct usbd_mtp_storage *out)
{
    if (usbd_mtp_fs_storage_get(index, out)) {
        return (out->root != NULL);
    }
    if (index == 0) {
        const char *root = usbd_mtp_fs_root_path();

        if (root == NULL) {
            return false;
        }
        memset(out, 0, sizeof(*out));
        out->root = root;
        out->description = usbd_mtp_fs_description();
        out->label = "";
        return true;
    }
    return false;
}

static uint32_t volume_count(void)
{
    struct usbd_mtp_storage st;
    uint32_t n = 0;

    while ((n < 8) && volume_at(n, &st)) {
        n++;
    }
    return n;
}

/* ---------------- handle to path ---------------- */

static char s_cache_path[MTP_PATH_MAX];
static uint32_t s_cache_handle, s_cache_storage;

static char s_abs_path[MTP_PATH_MAX]; /* shared static: only one task runs in this layer (big buffers must not go on the stack) */

/** Drop the path cache entry of a handle (a renamed object moved to another path). */
static void cache_forget(uint32_t handle)
{
    if (s_cache_handle == handle) {
        s_cache_handle = 0;
        s_cache_path[0] = '\0';
    }
}

static int build_abs(const char *vol_root, const char *rel, char *out, size_t n)
{
    if ((rel == NULL) || (rel[0] == '\0')) {
        return snprintf(out, n, "%s", vol_root);
    }
    return join_path(out, n, vol_root, rel) ? 1 : 0;
}

static bool find_in_dir(uint32_t want, const char *vol_root, const char *rel, char *out, size_t out_n,
                        uint32_t *storage_out, uint32_t volume_index, int depth)
{
    struct usbd_mtp_storage st_probe;
    uint32_t vol_id = 0;

    char child_rel[MTP_PATH_MAX];
    struct mtp_dirent *e;
    MTP_DIR *d;
    bool found = false;

    if (build_abs(vol_root, rel, s_abs_path, sizeof(s_abs_path)) <= 0) {
        return false;
    }
    d = usbd_mtp_opendir(s_abs_path);
    if (d == NULL) {
        return false;
    }
    (void)volume_at(volume_index, &st_probe);
    vol_id = volume_id(volume_index, &st_probe);
    while (!found && ((e = usbd_mtp_readdir(d)) != NULL)) {
        if ((strcmp(e->d_name, ".") == 0) || (strcmp(e->d_name, "..") == 0)) {
            continue;
        }
        if (rel[0] == '\0') {
            snprintf(child_rel, sizeof(child_rel), "%s", e->d_name);
        } else if (!join_path(child_rel, sizeof(child_rel), rel, e->d_name)) {
            continue; /* skip entries that are too long (they take no part in the handle scheme) */
        }
        if (build_abs(vol_root, child_rel, s_abs_path, sizeof(s_abs_path)) <= 0) {
            continue;
        }
        if (path_handle(s_abs_path) == want) {
            snprintf(out, out_n, "%s", s_abs_path);
            *storage_out = vol_id;
            found = true;
            break;
        }
        {
            struct stat st_child;

            if ((depth + 1 >= MTP_WALK_DEPTH) || (usbd_mtp_stat(s_abs_path, &st_child) != 0) ||
                !S_ISDIR(st_child.st_mode)) {
                continue;
            }
        }
        {
            if (find_in_dir(want, vol_root, child_rel, out, out_n, storage_out, volume_index, depth + 1)) {
                found = true;
            }
        }
    }
    usbd_mtp_closedir(d);
    return found;
}

static bool handle_to_path(uint32_t handle, char *out, size_t out_n, uint32_t *storage_out)
{
    uint32_t n = volume_count();

    if ((handle == 0) || (handle == 0xffffffffu)) {
        return false;
    }
    /* A renamed object: the host keeps using the handle it cached before the rename. The aliases
     * are newer information than the path cache below (which may still hold the pre-rename path
     * of exactly this handle), so they are consulted first. Missing this makes the host's next
     * metadata call fail, and gvfs turns such a failure into a crash in libmtp (it passes the
     * NULL from LIBMTP_Get_Filemetadata straight into LIBMTP_Set_File_Name). */
    if (alias_lookup(handle, out, out_n, storage_out)) {
        return true;
    }
    if ((handle == s_cache_handle) && (s_cache_path[0] != '\0')) {
        snprintf(out, out_n, "%s", s_cache_path);
        if (storage_out) {
            *storage_out = s_cache_storage;
        }
        return true;
    }
    if (alias_lookup(handle, out, out_n, storage_out)) {
        return true;
    }
    for (uint32_t i = 0; i < n; i++) {
        struct usbd_mtp_storage st;
        uint32_t storage = 0;

        if (!volume_at(i, &st)) {
            continue;
        }
        if (find_in_dir(handle, st.root, "", out, out_n, &storage, i, 0)) {
            snprintf(s_cache_path, sizeof(s_cache_path), "%s", out);
            s_cache_handle = handle;
            s_cache_storage = storage;
            if (storage_out) {
                *storage_out = storage;
            }
            return true;
        }
    }
    return false;
}

/* ---------------- ObjectInfo ---------------- */

static size_t pack_object_info(uint8_t *buf, size_t cap, const char *name, uint32_t storage_id,
                               uint32_t obj_handle, const struct stat *st, bool is_dir, uint32_t parent_handle)
{
    size_t o = 0;

    if (parent_handle == 0) {
        parent_handle = 0xffffffffu; /* under a volume root: Windows expects 0xFFFFFFFF */
    }
    memset(buf, 0, 128);
    o += mtp_ptp_put32(buf + o, storage_id);
    o += mtp_ptp_put16(buf + o, is_dir ? MTP_FORMAT_ASSOCIATION : MTP_FORMAT_UNDEFINED);
    o += mtp_ptp_put16(buf + o, 0);                                  /* ProtectionStatus */
    /* A directory must report 0: reporting the real size makes Nautilus add 4GiB to every directory
     * (a reference implementation ran into this) */
    o += mtp_ptp_put32(buf + o, is_dir ? 0u : (uint32_t)st->st_size);
    o += mtp_ptp_put16(buf + o, 0);                                  /* ThumbFormat */
    o += mtp_ptp_put32(buf + o, 0);                                  /* ThumbCompressedSize */
    o += mtp_ptp_put32(buf + o, 0);                                  /* ThumbPixWidth */
    o += mtp_ptp_put32(buf + o, 0);                                  /* ThumbPixHeight */
    o += mtp_ptp_put32(buf + o, 0);                                  /* ImagePixWidth */
    o += mtp_ptp_put32(buf + o, 0);                                  /* ImagePixHeight */
    o += mtp_ptp_put32(buf + o, 0);                                  /* ImageBitDepth */
    o += mtp_ptp_put32(buf + o, parent_handle);                      /* ParentObject */
    o += mtp_ptp_put16(buf + o, is_dir ? 0x0001 : 0);                /* AssociationType (16 bits!) */
    o += mtp_ptp_put32(buf + o, 0);                                  /* AssociationDesc */
    o += mtp_ptp_put32(buf + o, obj_handle);                         /* SequenceNumber */
    o += mtp_ptp_put_str(buf + o, (cap > o) ? cap - o : 0, name);
    o += mtp_ptp_put_str(buf + o, (cap > o) ? cap - o : 0, "");      /* DateCreated */
    o += mtp_ptp_put_str(buf + o, (cap > o) ? cap - o : 0, "");      /* DateModified */
    o += mtp_ptp_put_str(buf + o, (cap > o) ? cap - o : 0, "");      /* Keywords */
    return o;
}

static bool stat_object(uint32_t handle, char *path, size_t path_n, uint32_t *storage,
                        struct stat *st, const char **name)
{
    if (!handle_to_path(handle, path, path_n, storage)) {
        return false;
    }
    if (usbd_mtp_stat(path, st) != 0) {
        return false;
    }
    {
        const char *slash = strrchr(path, '/');

        *name = slash ? (slash + 1) : path;
    }
    return true;
}

/* ---------------- sink path ---------------- */

struct mtp_sink {
    uint8_t *buf;
    uint32_t size;
    volatile uint32_t w, r;
    usb_osal_mutex_t mux;
    usb_osal_thread_t thread;
    int fd;
    char path[MTP_PATH_MAX];
    uint32_t logical;      /* logical write position (file length once everything queued has been written) */
    volatile bool busy;    /* the write task is writing */
    volatile bool dirty;   /* data not synced yet (the file length is not visible to the host yet) */
    volatile bool fail;
    volatile bool stop_req;
    /* statistics */
    volatile uint32_t enq, written, fails;
    bool inited;
};

static struct mtp_sink g_sink[CONFIG_USBDEV_MAX_BUS];
static struct mtp_sink *sink_of(uint8_t busid)
{
    return &g_sink[busid];
}

static uint32_t sink_used(struct mtp_sink *s)
{
    return (s->w - s->r + s->size) % s->size;
}

static uint32_t sink_room(struct mtp_sink *s)
{
    return s->size - 1 - sink_used(s);
}

/** Write task: take data out of the stream buffer and write it to disk in large chunks */
static void mtp_sink_thread(void *argument)
{
    uint8_t busid = (uint8_t)(uintptr_t)argument;
    struct mtp_sink *s = sink_of(busid);
    static uint8_t chunk[CONFIG_USBDEV_MTP_WR_CHUNK];

    for (;;) {
        uint32_t n = 0;

        usb_osal_mutex_take(s->mux);
        {
            uint32_t used = sink_used(s);

            if (used > 0) {
                uint32_t want = (used > sizeof(chunk)) ? sizeof(chunk) : used;

                n = ((s->r + want) <= s->size) ? want : (s->size - s->r);
                memcpy(chunk, s->buf + s->r, n);
                s->r = (s->r + n) % s->size;
            }
        }
        usb_osal_mutex_give(s->mux);

        if (n > 0) {
            if (s->fd >= 0) {
                int w;

                s->busy = true;
                w = usbd_mtp_write(s->fd, chunk, n);
                s->busy = false;
                if (w != (int)n) {
                    s->fail = true;
                    s->fails++;
                    USB_LOG_WRN("sink short write %d/%u\r\n", w, (unsigned)n);
                } else {
                    s->written += (uint32_t)w;
                    s->dirty = true;
                }
            } else {
                s->fails++; /* no target file: can only happen after an abnormal teardown */
            }
            continue;
        }

        /* Idle: push the data down with sync (some filesystems only update the length in the directory
         * entry after f_sync, otherwise the host keeps reading the old length) */
        if (s->dirty && (s->fd >= 0)) {
            (void)usbd_mtp_sync(s->fd); /* weak default: writes are immediately visible */
            s->dirty = false;
        }
        usb_osal_msleep(MTP_SINK_IDLE_MS);
    }
}

void usbd_mtp_obj_init(uint8_t busid)
{
    struct mtp_sink *s = sink_of(busid);
    size_t size = MTP_SINK_SIZE;

    if (s->inited) {
        return;
    }
    s->fd = -1;
    s->buf = (uint8_t *)usb_osal_malloc(size + 16);
    if (s->buf == NULL) {
        size = MTP_SINK_SIZE_BACKUP;
        s->buf = (uint8_t *)usb_osal_malloc(size + 16);
    }
    if (s->buf == NULL) {
        USB_LOG_ERR("sink buffer alloc failed\r\n");
        return;
    }
    s->size = (uint32_t)size;
    s->mux = usb_osal_mutex_create();
    s->inited = true;
    s->thread = usb_osal_thread_create("mtp_sink", CONFIG_USBDEV_MTP_STACKSIZE / 2, CONFIG_USBDEV_MTP_PRIO,
                                       mtp_sink_thread, (void *)(uintptr_t)busid);
    USB_LOG_INFO("mtp sink ready: %u KB\r\n", (unsigned)(size / 1024));
}

static bool sink_idle(struct mtp_sink *s)
{
    return (sink_used(s) == 0) && !s->busy && !s->dirty;
}

static bool sink_barrier(uint8_t busid, uint32_t timeout_ms)
{
    struct mtp_sink *s = sink_of(busid);
    uint32_t waited = 0;

    if (!s->inited) {
        return true;
    }
    while (!sink_idle(s)) {
        usb_osal_msleep(MTP_SINK_IDLE_MS);
        waited += MTP_SINK_IDLE_MS;
        if (waited > timeout_ms) {
            USB_LOG_WRN("sink barrier timeout (%u bytes queued)\r\n", (unsigned)sink_used(s));
            return false;
        }
    }
    return true;
}

static void sink_close_locked(struct mtp_sink *s)
{
    if (s->fd >= 0) {
        (void)usbd_mtp_sync(s->fd);
        (void)usbd_mtp_close(s->fd);
        s->fd = -1;
    }
    s->path[0] = '\0';
    s->logical = 0;
    s->dirty = false;
}

/** Finish: catch up with the queue and close the target file */
static void sink_finish(uint8_t busid)
{
    struct mtp_sink *s = sink_of(busid);

    if (!s->inited) {
        return;
    }
    (void)sink_barrier(busid, MTP_SINK_WAIT_MS);
    sink_close_locked(s);
}

/** Drop everything not yet written and close (session interrupted or failed: leave no partial data on disk) */
static void sink_discard(uint8_t busid)
{
    struct mtp_sink *s = sink_of(busid);

    if (!s->inited) {
        return;
    }
    usb_osal_mutex_take(s->mux);
    s->r = s->w = 0;
    usb_osal_mutex_give(s->mux);
    s->logical = 0;
    sink_finish(busid);
}

/**
 * @brief Point the sink target at (path, offset); a no-op while the same file keeps being written.
 * fd is only touched while the write task is idle (the producer opens/seeks/closes, the write task only
 * writes and syncs).
 */
static bool sink_target(uint8_t busid, const char *path, uint32_t offset, bool truncate)
{
    struct mtp_sink *s = sink_of(busid);

    if (!s->inited) {
        return false;
    }
    if ((s->fd >= 0) && (strcmp(s->path, path) == 0) && !truncate && (offset == s->logical)) {
        return true; /* same file, contiguous offset */
    }
    if (!sink_barrier(busid, MTP_SINK_WAIT_MS)) {
        return false;
    }
    if ((s->fd >= 0) && ((strcmp(s->path, path) != 0) || truncate)) {
        sink_close_locked(s);
    }
    if (s->fd < 0) {
        s->fd = usbd_mtp_open(path, MTP_OPEN_READ | MTP_OPEN_WRITE | (truncate ? MTP_OPEN_TRUNC : 0));
        if (s->fd < 0) {
            USB_LOG_WRN("sink open failed: %s\r\n", path);
            s->fail = true;
            return false;
        }
        snprintf(s->path, sizeof(s->path), "%s", path);
        s->logical = 0;
    }
    if (offset != s->logical) {
        if (usbd_mtp_seek(s->fd, offset) == 0) {
            s->logical = offset;
        } else {
            USB_LOG_WRN("sink seek to %u not supported\r\n", (unsigned)offset);
            s->fail = true;
            return false;
        }
    }
    return true;
}

/** Producer enqueue (polls until the write task frees space when the buffer is full) */
static bool sink_put(uint8_t busid, const uint8_t *data, uint32_t len)
{
    struct mtp_sink *s = sink_of(busid);
    uint32_t off = 0, waited = 0;

    if (!s->inited) {
        return false;
    }
    while (off < len) {
        uint32_t n = 0;

        usb_osal_mutex_take(s->mux);
        {
            uint32_t room = sink_room(s);

            if (room > 0) {
                uint32_t want = (len - off > room) ? room : (len - off);

                n = ((s->w + want) <= s->size) ? want : (s->size - s->w);
                memcpy(s->buf + s->w, data + off, n);
                s->w = (s->w + n) % s->size;
            }
        }
        usb_osal_mutex_give(s->mux);

        off += n;
        s->enq += n;
        s->logical += n;
        if (off < len) {
            usb_osal_msleep(1);
            waited += 1;
            if (waited > MTP_SINK_WAIT_MS) {
                USB_LOG_WRN("sink full timeout (%u bytes left)\r\n", (unsigned)(len - off));
                s->fail = true;
                return false;
            }
        }
    }
    return true;
}

/* ---------------- upload state machine ---------------- */

static struct {
    bool expect_objinfo;  /* waiting for the ObjectInfo dataset of SendObjectInfo */
    bool waiting_data;    /* waiting for the file bytes (SendObject / SendPartialObject) */
    bool open_pending;    /* open the sink target only when the first chunk of data arrives */
    bool open_truncate;
    bool sink_empty;      /* 0-byte placeholder object: the empty data of SendObject has to be accepted */
    bool rename_pending;  /* SetObjectPropValue(FileName): the data holds the new name */
    bool sink_err;
    uint32_t tid, storage_id, parent_handle, handle, offset;
    char path[MTP_PATH_MAX];
    uint32_t declared_size;
} g_up[CONFIG_USBDEV_MAX_BUS];

static bool send_object_begin(uint8_t busid, uint32_t tid);

static uint8_t s_oi_buf[CONFIG_USBDEV_MTP_MAX_BUFSIZE];
static uint32_t s_oi_len;

static void upload_cleanup(uint8_t busid)
{
    /* the previous transfer may still have data in the sink queue: let the sink side finish it
     * (including the close), do not close the file here */
    sink_finish(busid);
    g_up[busid].expect_objinfo = false;
    g_up[busid].waiting_data = false;
    g_up[busid].open_pending = false;
    g_up[busid].sink_empty = false;
    g_up[busid].rename_pending = false;
    g_up[busid].sink_err = false;
    g_up[busid].path[0] = '\0';
}

void usbd_mtp_obj_reset(uint8_t busid)
{
    if (g_up[busid].waiting_data && (g_up[busid].path[0] != '\0') && !g_up[busid].sink_empty) {
        USB_LOG_WRN("upload interrupted, removing partial file %s\r\n", g_up[busid].path);
        sink_discard(busid);
        (void)usbd_mtp_unlink(g_up[busid].path);
        alias_forget(g_up[busid].path);
        g_up[busid].waiting_data = false;
        upload_cleanup(busid);
        return;
    }
    upload_cleanup(busid);
}

void usbd_mtp_obj_session_close(uint8_t busid)
{
    sink_finish(busid);
    upload_cleanup(busid);
}

void usbd_mtp_obj_data_truncated(uint8_t busid)
{
    g_up[busid].sink_err = true; /* answer with a failure: data must never be dropped silently */
}

void usbd_mtp_obj_stat(uint8_t busid, uint32_t *enq, uint32_t *written, uint32_t *fails)
{
    struct mtp_sink *s = sink_of(busid);

    if (enq) {
        *enq = s->enq;
    }
    if (written) {
        *written = s->written;
    }
    if (fails) {
        *fails = s->fails;
    }
}

/* ---------------- delete (recursive) ---------------- */

static int remove_tree(const char *path, int depth)
{
    struct stat st;
    MTP_DIR *d;
    struct mtp_dirent *e;
    char child[MTP_PATH_MAX];

    if (usbd_mtp_stat(path, &st) != 0) {
        return -1;
    }
    if (!S_ISDIR(st.st_mode)) {
        return usbd_mtp_unlink(path);
    }
    if (depth >= MTP_WALK_DEPTH) {
        return -1;
    }
    d = usbd_mtp_opendir(path);
    if (d == NULL) {
        return -1;
    }
    while ((e = usbd_mtp_readdir(d)) != NULL) {
        if ((strcmp(e->d_name, ".") == 0) || (strcmp(e->d_name, "..") == 0)) {
            continue;
        }
        if (join_path(child, sizeof(child), path, e->d_name)) {
            (void)remove_tree(child, depth + 1);
        }
    }
    usbd_mtp_closedir(d);
    return usbd_mtp_rmdir(path);
}

/* ---------------- command handling ---------------- */

/** Every operation except the write commands catches up with the sink queue first (what the host reads or deletes has to be the complete object) */
static bool is_write_flow(uint16_t code)
{
    return (code == MTP_OPERATION_SEND_OBJECT_INFO) || (code == MTP_OPERATION_SEND_OBJECT) ||
           (code == MTP_OPERATION_SEND_PARTIAL_OBJECT) || (code == 0x101C) ||
           (code == MTP_OPERATION_BEGIN_EDIT_OBJECT) || (code == 0x9816) ||
           (code == MTP_OPERATION_END_EDIT_OBJECT) || (code == MTP_OPERATION_SET_OBJECT_PROP_VALUE);
}

bool usbd_mtp_obj_command(uint8_t busid, const struct mtp_container_hdr *hdr, const uint32_t *params, int nparam)
{
    static uint8_t payload[CONFIG_USBDEV_MTP_MAX_BUFSIZE];
    static char path[MTP_PATH_MAX];
    uint32_t storage = 0;
    uint32_t nvol = volume_count();

    if (!is_write_flow(hdr->code)) {
        (void)sink_barrier(busid, MTP_SINK_WAIT_MS);
    }

    switch (hdr->code) {
        case MTP_OPERATION_GET_OBJECT_HANDLES: {
            /* Handle list: with parent=0xFFFFFFFF only the **direct children of the volume root** are
             * returned (that is the semantics clients really implement; returning the whole tree
             * recursively as the specification implies makes gvfs invent "phantom" entries at the
             * volume root and deletes then fail). */
            uint32_t storage_sel = (nparam >= 1) ? params[0] : 0xffffffffu;
            uint32_t parent = (nparam >= 3) ? params[2] : 0xffffffffu;
            uint32_t max = CONFIG_USBDEV_MTP_MAX_OBJECTS;
            uint32_t count = 0;
            size_t o = 4;

            if ((4 + max * 4) > sizeof(payload)) {
                max = (uint32_t)((sizeof(payload) - 4) / 4);
            }
            for (uint32_t i = 0; i < nvol; i++) {
                struct usbd_mtp_storage st;
                const char *dir = NULL;
                static char dirbuf[MTP_PATH_MAX];

                if (!volume_at(i, &st)) {
                    continue;
                }
                struct usbd_mtp_storage st_sel;

                (void)volume_at(i, &st_sel);
                if ((storage_sel != 0xffffffffu) && (storage_sel != volume_id(i, &st_sel))) {
                    continue;
                }
                if ((parent == 0xffffffffu) || (parent == 0)) {
                    dir = st.root;
                } else if (handle_to_path(parent, dirbuf, sizeof(dirbuf), &storage) && (storage == volume_id(i, &st_sel))) {
                    dir = dirbuf; /* the direct children of that directory */
                }
                if (dir == NULL) {
                    continue;
                }
                {
                    MTP_DIR *d = usbd_mtp_opendir(dir);
                    struct mtp_dirent *e;
                    static char child[MTP_PATH_MAX];

                    if (d == NULL) {
                        continue;
                    }
                    while ((e = usbd_mtp_readdir(d)) != NULL) {
                        if ((strcmp(e->d_name, ".") == 0) || (strcmp(e->d_name, "..") == 0)) {
                            continue;
                        }
                        if (!join_path(child, sizeof(child), dir, e->d_name)) {
                            continue;
                        }
                        if (count < max) {
                            (void)mtp_ptp_put32(payload + o, path_handle(child));
                            o += 4;
                        }
                        count++;
                    }
                    usbd_mtp_closedir(d);
                }
            }
            if (count > max) {
                USB_LOG_WRN("too many objects (%u), returning %u\r\n", (unsigned)count, (unsigned)max);
                count = max;
            }
            (void)mtp_ptp_put32(payload, count);
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, o);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_GET_OBJECT_INFO: {
            struct stat st;
            const char *name = NULL;
            const struct usbd_mtp_storage *vol;
            struct usbd_mtp_storage stbuf;
            size_t plen;
            uint32_t parent;

            if (nparam < 1) {
                return false;
            }
            if (!stat_object(params[0], path, sizeof(path), &storage, &st, &name) ||
                !volume_by_id(storage, &stbuf)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            vol = &stbuf;
            {
                const char *slash = strrchr(path, '/');
                uint32_t parent_h = 0xffffffffu;

                if ((slash != NULL) && (slash != path)) {
                    static char pbuf[MTP_PATH_MAX];
                    size_t n = (size_t)(slash - path);

                    memcpy(pbuf, path, n);
                    pbuf[n] = '\0';
                    if (strcmp(pbuf, vol->root) != 0) {
                        parent_h = path_handle(pbuf);
                    }
                }
                parent = parent_h;
            }
            plen = pack_object_info(payload, sizeof(payload), name, storage, params[0], &st,
                                    S_ISDIR(st.st_mode), parent);
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, plen);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_GET_OBJECT: {
            struct stat st;
            int fd;

            if ((nparam < 1) || !handle_to_path(params[0], path, sizeof(path), &storage) ||
                (usbd_mtp_stat(path, &st) != 0) || S_ISDIR(st.st_mode)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            fd = usbd_mtp_open(path, MTP_OPEN_READ);
            if (fd < 0) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_ACCESS_DENIED, hdr->tid);
                return true;
            }
            if (usbd_mtp_stream_begin(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid,
                                      (uint32_t)st.st_size) == 0) {
                static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t chunk[512];
                uint32_t sent = 0;
                int n;

                while ((n = usbd_mtp_read(fd, chunk, sizeof(chunk))) > 0) {
                    if (usbd_mtp_stream_write(busid, chunk, (size_t)n) != 0) {
                        break; /* the host cancelled midway: still send the response, do not get stuck */
                    }
                    sent += (uint32_t)n;
                }
                (void)usbd_mtp_stream_end(busid);
                MTP_TRACE("mtp get_object %s sent %u bytes\r\n", path, (unsigned)sent);
            }
            (void)usbd_mtp_close(fd);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_GET_PARTIAL_OBJECT_64: /* 0x95C1: 64-bit offset, same semantics as 0x101B */
        case MTP_OPERATION_GET_PARTIAL_OBJECT: {
            uint32_t off, want;
            struct stat st;
            int fd;

            if ((nparam < 3) || !handle_to_path(params[0], path, sizeof(path), &storage)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            if (hdr->code == MTP_OPERATION_GET_PARTIAL_OBJECT_64) {
                off = params[1];
                want = (nparam >= 4) ? params[3] : 0;
            } else {
                off = params[1];
                want = params[2];
            }
            fd = usbd_mtp_open(path, MTP_OPEN_READ);
            if ((fd < 0) || (usbd_mtp_seek(fd, off) != 0)) {
                if (fd >= 0) {
                    (void)usbd_mtp_close(fd);
                }
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_ACCESS_DENIED, hdr->tid);
                return true;
            }
            if ((usbd_mtp_stat(path, &st) == 0) && (off < (uint32_t)st.st_size)) {
                uint32_t avail = (uint32_t)st.st_size - off;

                if (want > avail) {
                    want = avail;
                }
            } else {
                want = 0;
            }
            if (usbd_mtp_stream_begin(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, want) == 0) {
                static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t chunk[512];
                uint32_t remain = want;
                int n;

                while (remain > 0) {
                    uint32_t want_n = (remain > sizeof(chunk)) ? (uint32_t)sizeof(chunk) : remain;

                    n = usbd_mtp_read(fd, chunk, want_n);
                    if (n <= 0) {
                        break;
                    }
                    if (usbd_mtp_stream_write(busid, chunk, (size_t)n) != 0) {
                        break;
                    }
                    remain -= (uint32_t)n;
                }
                (void)usbd_mtp_stream_end(busid);
            }
            (void)usbd_mtp_close(fd);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_DELETE_OBJECT: {
            struct usbd_mtp_storage st;

            if ((nparam < 1) || !handle_to_path(params[0], path, sizeof(path), &storage) ||
                !volume_by_id(storage, &st)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            if (st.read_only) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_ACCESS_DENIED, hdr->tid);
                return true;
            }
            sink_finish(busid); /* do not touch this file while data is still waiting to be written */
            if (remove_tree(path, 0) != 0) {
                USB_LOG_WRN("delete failed: %s\r\n", path);
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            USB_LOG_INFO("deleted %s\r\n", path);
            alias_forget(path); /* the old handles of a deleted object must not resolve to a new one */
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_SEND_OBJECT_INFO:
            /* The response has to wait until the ObjectInfo dataset has been parsed (that is what the
             * protocol requires), so only the state is recorded here */
            if (nparam < 2) {
                return false;
            }
            upload_cleanup(busid);
            g_up[busid].expect_objinfo = true;
            g_up[busid].tid = hdr->tid;
            g_up[busid].storage_id = params[0];
            g_up[busid].parent_handle = params[1];
            return true;

        case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED: {
            size_t o = 4;

            (void)mtp_ptp_put32(payload, 2);
            o += mtp_ptp_put16(payload + o, MTP_PROP_OBJECTSIZE);
            o += mtp_ptp_put16(payload + o, MTP_PROP_FILENAME);
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, o);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_GET_OBJECT_PROP_DESC: {
            /* It has to be the **MTP layout**: PropertyCode, DataType, GetSet, **DefaultValue**,
             * **GroupCode(4)**, FormFlag, [FORM]. Omitting DefaultValue/GroupCode the way the PTP
             * PropertyDesc does makes libmtp's ptp_unpack_OPD read out of bounds -> PANIC and the host
             * backend dies (symptom: renaming from the file manager always crashes). */
            uint16_t prop = (uint16_t)params[0];
            uint16_t dt = 0;
            size_t o = 0;

            if (nparam < 1) {
                return false;
            }
            if (prop == MTP_PROP_OBJECTSIZE) {
                dt = MTP_TYPE_UINT64;
            } else if ((prop == MTP_PROP_FILENAME) || (prop == MTP_PROP_DATECREATED) ||
                       (prop == MTP_PROP_DATEMODIFIED)) {
                dt = MTP_TYPE_STR;
            } else {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED, hdr->tid);
                return true;
            }
            o += mtp_ptp_put16(payload + o, prop);
            o += mtp_ptp_put16(payload + o, dt);
            /* GetSet: a property that can be changed has to report Get/Set, otherwise the host does not
             * consider it writable */
            payload[o++] = (prop == MTP_PROP_FILENAME) ? 0x01 : 0x00;
            if (dt == MTP_TYPE_STR) {
                payload[o++] = 0x00; /* DefaultValue: empty string */
            } else {
                o += mtp_ptp_put64(payload + o, 0);
            }
            o += mtp_ptp_put32(payload + o, 0); /* GroupCode = General */
            payload[o++] = 0x00;                /* FormFlag = None */
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, o);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_GET_OBJECT_PROP_VALUE: {
            struct stat st;
            const char *name = NULL;
            size_t o;

            if ((nparam < 2) || !stat_object(params[0], path, sizeof(path), &storage, &st, &name)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            switch (params[1]) {
                case MTP_PROP_OBJECTSIZE:
                    o = mtp_ptp_put64(payload, (uint64_t)st.st_size);
                    break;
                case MTP_PROP_FILENAME:
                    o = mtp_ptp_put_str(payload, sizeof(payload), name);
                    break;
                case MTP_PROP_DATECREATED:
                case MTP_PROP_DATEMODIFIED:
                    o = mtp_ptp_put_str(payload, sizeof(payload), "");
                    break;
                default:
                    (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED, hdr->tid);
                    return true;
            }
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, o);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_SET_OBJECT_PROP_VALUE:
            /* the data (the new name) arrives in the following data container, rename once it is complete */
            if ((nparam < 2) || (params[1] != MTP_PROP_FILENAME)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED, hdr->tid);
                return true;
            }
            upload_cleanup(busid);
            g_up[busid].rename_pending = true;
            g_up[busid].tid = hdr->tid;
            g_up[busid].handle = params[0];
            s_oi_len = 0;
            return true;

        case MTP_OPERATION_GET_OBJECT_PROP_LIST: {
            uint32_t want_handle = (nparam >= 1) ? params[0] : 0xffffffffu;
            uint32_t want_prop = (nparam >= 3) ? params[2] : 0xffffffffu;
            uint32_t count = 0;
            size_t o = 4;
            uint32_t cap = (uint32_t)((sizeof(payload) - 4) / (4 + 2 + 2 + 8 + 4));

            /* answer for a single object only: that is how the host uses it (one lookup per object
             * while enumerating a directory) */
            if ((want_handle != 0xffffffffu) && (want_handle != 0)) {
                struct stat st;
                const char *name = NULL;

                if (stat_object(want_handle, path, sizeof(path), &storage, &st, &name)) {
                    for (uint32_t pi = 0; pi < 2 && count < cap; pi++) {
                        uint16_t prop = (pi == 0) ? MTP_PROP_OBJECTSIZE : MTP_PROP_FILENAME;

                        if ((want_prop != 0xffffffffu) && (want_prop != prop)) {
                            continue;
                        }
                        o += mtp_ptp_put32(payload + o, want_handle);
                        o += mtp_ptp_put16(payload + o, prop);
                        o += mtp_ptp_put16(payload + o, (prop == MTP_PROP_OBJECTSIZE) ? MTP_TYPE_UINT64 : MTP_TYPE_STR);
                        if (prop == MTP_PROP_OBJECTSIZE) {
                            o += mtp_ptp_put64(payload + o, (uint64_t)st.st_size);
                        } else {
                            o += mtp_ptp_put_str(payload + o, sizeof(payload) - o, name);
                        }
                        count++;
                    }
                }
            }
            (void)mtp_ptp_put32(payload, count);
            (void)usbd_mtp_send_container(busid, MTP_CONTAINER_TYPE_DATA, hdr->code, hdr->tid, payload, o);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_SEND_OBJECT: /* 0x100D: the file bytes follow; a 0-byte object is answered immediately */
            return send_object_begin(busid, hdr->tid);

        case MTP_OPERATION_SEND_PARTIAL_OBJECT: /* 0x95C2 */
        case 0x101C: {                          /* generic SendPartialObject: the host writes into an existing object at an offset */
            static char ppath[MTP_PATH_MAX];

            if (nparam < 3) {
                return false;
            }
            if (!handle_to_path(params[0], ppath, sizeof(ppath), &storage)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            snprintf(g_up[busid].path, sizeof(g_up[busid].path), "%s", ppath);
            g_up[busid].handle = params[0];
            g_up[busid].offset = params[1];
            g_up[busid].sink_empty = false; /* clear the empty-data flag that may be left over from the placeholder phase */
            g_up[busid].waiting_data = true;
            g_up[busid].open_pending = true;
            g_up[busid].open_truncate = false;
            g_up[busid].tid = hdr->tid;
            return true;
        }

        case MTP_OPERATION_BEGIN_EDIT_OBJECT: /* 0x95C4 */
        case 0x9816:                          /* UpdateObject: the host announces that this object is about to be modified */
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, hdr->tid);
            return true;

        case MTP_OPERATION_END_EDIT_OBJECT: { /* 0x95C5: the point where the sink is finished */
            bool err;

            sink_finish(busid);
            err = g_up[busid].sink_err;
            g_up[busid].sink_err = false;
            g_up[busid].waiting_data = false;
            g_up[busid].path[0] = '\0';
            (void)usbd_mtp_send_response(busid, err ? MTP_RESPONSE_ACCESS_DENIED : MTP_RESPONSE_OK, hdr->tid);
            return true;
        }

        case MTP_OPERATION_TRUNCATE_OBJECT: { /* 0x95C3 */
            int rc;

            if ((nparam < 2) || !handle_to_path(params[0], path, sizeof(path), &storage)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            sink_finish(busid);
            rc = usbd_mtp_truncate(path, params[1]); /* the weak default returns -1 (not implemented) */
            (void)usbd_mtp_send_response(busid, (rc == 0) ? MTP_RESPONSE_OK : MTP_RESPONSE_ACCESS_DENIED, hdr->tid);
            return true;
        }

        case MTP_OPERATION_MOVE_OBJECT: { /* 0x1019: move / rename */
            static char dst_dir[MTP_PATH_MAX];
            static char new_path[MTP_PATH_MAX];
            const char *base;
            struct usbd_mtp_storage dst_vol;
            uint32_t dst_storage = 0;
            int rc;

            if ((nparam < 3) || !handle_to_path(params[0], path, sizeof(path), &storage)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, hdr->tid);
                return true;
            }
            if ((params[2] == 0xffffffffu) || (params[2] == 0)) {
                if (!volume_by_id(params[1], &dst_vol)) {
                    (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_PARENT_OBJECT, hdr->tid);
                    return true;
                }
                snprintf(dst_dir, sizeof(dst_dir), "%s", dst_vol.root);
                dst_storage = params[1]; /* the storage id the host used */
            } else {
                uint32_t ds = 0;

                if (!handle_to_path(params[2], dst_dir, sizeof(dst_dir), &ds)) {
                    (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_PARENT_OBJECT, hdr->tid);
                    return true;
                }
                dst_storage = ds;
            }
            base = strrchr(path, '/');
            if ((base == NULL) || !join_path(new_path, sizeof(new_path), dst_dir, base + 1)) {
                (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_PARENT_OBJECT, hdr->tid);
                return true;
            }
            sink_finish(busid);
            rc = usbd_mtp_rename(path, new_path); /* the weak default returns -1 (not implemented) */
            USB_LOG_INFO("move %s -> %s: %s\r\n", path, new_path, (rc == 0) ? "ok" : "failed");
            if (rc == 0) {
                cache_forget(params[0]);                     /* it pointed at the old path */
                alias_add(params[0], dst_storage, new_path); /* hosts keep using the pre-move handle */
            }
            (void)usbd_mtp_send_response(busid, (rc == 0) ? MTP_RESPONSE_OK : MTP_RESPONSE_ACCESS_DENIED, hdr->tid);
            return true;
        }

        default:
            return false;
    }
}

/* ---------------- data containers ---------------- */

void usbd_mtp_obj_data_begin(uint8_t busid, uint32_t payload_len)
{
    (void)payload_len;
    if (g_up[busid].expect_objinfo || g_up[busid].rename_pending) {
        s_oi_len = 0; /* the ObjectInfo dataset may be split across packets: collect it all, parse it when the container ends */
    }
}

static void oi_complete(uint8_t busid)
{
    static char name[MTP_NAME_MAX];
    uint16_t fmt;
    uint32_t size;
    size_t name_off = OBJINFO_FIXED;
    struct usbd_mtp_storage vol;
    static char dir[MTP_PATH_MAX];
    uint32_t handle;
    uint32_t resp[3];

    name[0] = '\0';
    if (s_oi_len < (OBJINFO_FIXED + 1)) {
        USB_LOG_WRN("SendObjectInfo: dataset too short (%u)\r\n", (unsigned)s_oi_len);
        upload_cleanup(busid);
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_DATASET, g_up[busid].tid);
        return;
    }
    memcpy(&fmt, s_oi_buf + 4, 2);
    memcpy(&size, s_oi_buf + 8, 4);
    /* filename offset: 52 first; on some hosts (Windows) ObjectSize is 64 bits -> 56 */
    {
        static char tmp[MTP_NAME_MAX];

        if ((mtp_ptp_get_str(s_oi_buf + name_off, s_oi_len - name_off, tmp, sizeof(tmp)) == 0) &&
            (s_oi_len >= OBJINFO_FIXED_WIN64)) {
            name_off = OBJINFO_FIXED_WIN64;
        }
    }
    if (mtp_ptp_get_str(s_oi_buf + name_off, s_oi_len - name_off, name, sizeof(name)) == 0) {
        USB_LOG_WRN("SendObjectInfo: bad filename\r\n");
        upload_cleanup(busid);
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_DATASET, g_up[busid].tid);
        return;
    }
    if (!volume_by_id(g_up[busid].storage_id, &vol)) {
        upload_cleanup(busid);
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_STORAGE_ID, g_up[busid].tid);
        return;
    }
    if (vol.read_only) {
        upload_cleanup(busid);
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_STORE_READ_ONLY, g_up[busid].tid);
        return;
    }
    if ((g_up[busid].parent_handle == 0xffffffffu) || (g_up[busid].parent_handle == 0)) {
        snprintf(dir, sizeof(dir), "%s", vol.root);
    } else if (!handle_to_path(g_up[busid].parent_handle, dir, sizeof(dir), NULL)) {
        upload_cleanup(busid);
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_PARENT_OBJECT, g_up[busid].tid);
        return;
    }
    if (!join_path(g_up[busid].path, sizeof(g_up[busid].path), dir, name)) {
        upload_cleanup(busid);
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_DATASET, g_up[busid].tid);
        return;
    }

    if (fmt == MTP_FORMAT_ASSOCIATION) { /* directory: there is no data phase */
        int rc = usbd_mtp_mkdir(g_up[busid].path);
        uint32_t tid = g_up[busid].tid;

        USB_LOG_INFO("mkdir %s: %s\r\n", g_up[busid].path, (rc == 0) ? "ok" : "failed");
        resp[0] = g_up[busid].storage_id;
        resp[1] = (g_up[busid].parent_handle == 0) ? 0xffffffffu : g_up[busid].parent_handle;
        resp[2] = path_handle(g_up[busid].path);
        upload_cleanup(busid);
        if (rc == 0) {
            (void)usbd_mtp_send_response_params(busid, MTP_RESPONSE_OK, tid, resp, 3);
        } else {
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_ACCESS_DENIED, tid);
        }
        return;
    }

    /* create (truncate) the file: this only creates it and checks that it is writable, the sink opens
     * it when the data arrives */
    {
        int fd = usbd_mtp_open(g_up[busid].path, MTP_OPEN_WRITE | MTP_OPEN_TRUNC);
        uint32_t tid = g_up[busid].tid;

        if (fd < 0) {
            USB_LOG_WRN("create %s failed\r\n", g_up[busid].path);
            upload_cleanup(busid);
            (void)usbd_mtp_send_response(busid, MTP_RESPONSE_ACCESS_DENIED, tid);
            return;
        }
        (void)usbd_mtp_close(fd);
    }
    handle = path_handle(g_up[busid].path);
    g_up[busid].expect_objinfo = false;
    g_up[busid].handle = handle;
    resp[0] = g_up[busid].storage_id;
    resp[1] = (g_up[busid].parent_handle == 0) ? 0xffffffffu : g_up[busid].parent_handle;
    resp[2] = handle;
    /* **Answer SendObjectInfo right away (with the handle)**: the protocol requires it directly after
     * the ObjectInfo dataset. It used to wait until all file data had been received, and the host (the
     * drag and drop / paste path of the file manager) then idled here for the whole 20s timeout,
     * reporting "Could not send object info". */
    (void)usbd_mtp_send_response_params(busid, MTP_RESPONSE_OK, g_up[busid].tid, resp, 3);

    if (size > 0) {
        g_up[busid].waiting_data = true;
        g_up[busid].open_pending = true;
        g_up[busid].open_truncate = true;
        g_up[busid].offset = 0;
        g_up[busid].declared_size = size;
        USB_LOG_INFO("ready to receive %s (%u bytes)\r\n", g_up[busid].path, (unsigned)size);
    } else {
        /* 0-byte placeholder object: the file exists already, the host may follow up with
         * BeginEdit/SendPartial to write data into it */
        USB_LOG_INFO("0-byte placeholder created, handle=0x%08x\r\n", (unsigned)handle);
        g_up[busid].path[0] = '\0';
        g_up[busid].declared_size = 0;
    }
}

static void rename_complete(uint8_t busid)
{
    static char new_name[MTP_NAME_MAX];
    static char old_path[MTP_PATH_MAX];
    static char dir[MTP_PATH_MAX];
    static char new_path[MTP_PATH_MAX];
    uint32_t storage = 0;
    const char *slash;
    int rc;

    g_up[busid].rename_pending = false;
    if ((mtp_ptp_get_str(s_oi_buf, s_oi_len, new_name, sizeof(new_name)) == 0) ||
        !handle_to_path(g_up[busid].handle, old_path, sizeof(old_path), &storage)) {
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_OBJECT_HANDLE, g_up[busid].tid);
        return;
    }
    slash = strrchr(old_path, '/');
    if ((slash == NULL) || (slash == old_path)) {
        snprintf(dir, sizeof(dir), "/");
    } else {
        size_t dn = (size_t)(slash - old_path);

        if (dn >= sizeof(dir)) {
            dn = sizeof(dir) - 1;
        }
        memcpy(dir, old_path, dn);
        dir[dn] = '\0';
    }
    if (!join_path(new_path, sizeof(new_path), dir, new_name)) {
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_INVALID_DATASET, g_up[busid].tid);
        return;
    }
    rc = usbd_mtp_rename(old_path, new_path);
    USB_LOG_INFO("rename %s -> %s: %s\r\n", old_path, new_path, (rc == 0) ? "ok" : "failed");
    if (rc == 0) {
        cache_forget(g_up[busid].handle);                     /* it pointed at the old path */
        alias_add(g_up[busid].handle, storage, new_path);     /* hosts keep using the pre-rename handle */
    }
    (void)usbd_mtp_send_response(busid, (rc == 0) ? MTP_RESPONSE_OK : MTP_RESPONSE_ACCESS_DENIED, g_up[busid].tid);
}

void usbd_mtp_obj_data(uint8_t busid, const uint8_t *data, size_t len)
{
    if (g_up[busid].rename_pending || g_up[busid].expect_objinfo) {
        if (s_oi_len + len > sizeof(s_oi_buf)) {
            s_oi_len = 0;
            return;
        }
        memcpy(s_oi_buf + s_oi_len, data, len);
        s_oi_len += (uint32_t)len;
        return;
    }
    if (g_up[busid].sink_empty) {
        return; /* empty data of a 0-byte object: drop it */
    }
    if (g_up[busid].waiting_data) {
        if (g_up[busid].sink_err) {
            return; /* already failed: do not make the error bigger */
        }
        if (g_up[busid].open_pending) {
            g_up[busid].open_pending = false;
            if (!sink_target(busid, g_up[busid].path, g_up[busid].offset, g_up[busid].open_truncate)) {
                g_up[busid].sink_err = true;
                return;
            }
        }
        if (!sink_put(busid, data, (uint32_t)len)) {
            g_up[busid].sink_err = true;
        }
    }
}

void usbd_mtp_obj_data_end(uint8_t busid)
{
    if (g_up[busid].expect_objinfo) {
        oi_complete(busid);
        return;
    }
    if (g_up[busid].rename_pending) {
        rename_complete(busid);
        return;
    }
    if (g_up[busid].sink_empty) {
        g_up[busid].sink_empty = false;
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, g_up[busid].tid);
        return;
    }
    if (g_up[busid].waiting_data) {
        /* The data is already in the sink queue: only the response is sent here (writing continues in
         * the write task). The SendObject response carries **no parameters**, and answering **as soon
         * as the data is queued** is deliberate -- waiting for the disk would make the host time out. */
        bool err = g_up[busid].sink_err;
        uint32_t tid = g_up[busid].tid;

        g_up[busid].waiting_data = false;
        g_up[busid].open_pending = false;
        MTP_TRACE("mtp data end: queued %u bytes\r\n", (unsigned)g_sink[busid].enq);
        (void)usbd_mtp_send_response(busid, err ? MTP_RESPONSE_ACCESS_DENIED : MTP_RESPONSE_OK, tid);
        return;
    }
}

/* ---------------- SendObject: wait for the file bytes (or the empty data of a 0-byte placeholder) ---------------- */

static bool send_object_begin(uint8_t busid, uint32_t tid)
{
    if (!g_up[busid].waiting_data) {
        /* 0-byte placeholder object: the file exists already and the host sends another SendObject for
         * it. **Answer immediately**: most hosts produce no data container for a 0-byte object, so
         * waiting for data blocks the host in a read. */
        g_up[busid].sink_empty = true;
        g_up[busid].tid = tid;
        (void)usbd_mtp_send_response(busid, MTP_RESPONSE_OK, tid);
        return true;
    }
    g_up[busid].tid = tid;
    g_up[busid].open_pending = true;
    g_up[busid].open_truncate = true;
    return true;
}
