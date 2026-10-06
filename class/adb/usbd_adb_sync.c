/*
 * Copyright (c) 2024 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_adb.h"

#include "stdarg.h"

#define ID_STAT ADB_FOURCC('S', 'T', 'A', 'T') /* "lstat_v1": the v1 stat      */
#define ID_LIST ADB_FOURCC('L', 'I', 'S', 'T')
#define ID_DENT ADB_FOURCC('D', 'E', 'N', 'T')
#define ID_SEND ADB_FOURCC('S', 'E', 'N', 'D')
#define ID_RECV ADB_FOURCC('R', 'E', 'C', 'V')
#define ID_DONE ADB_FOURCC('D', 'O', 'N', 'E')
#define ID_DATA ADB_FOURCC('D', 'A', 'T', 'A')
#define ID_OKAY ADB_FOURCC('O', 'K', 'A', 'Y')
#define ID_FAIL ADB_FOURCC('F', 'A', 'I', 'L')
#define ID_QUIT ADB_FOURCC('Q', 'U', 'I', 'T')

struct adb_sync_wire_hdr {
    uint32_t id;
    uint32_t len;
};

struct adb_sync_wire_stat {
    uint32_t id;
    uint32_t mode;
    uint32_t size;
    uint32_t mtime;
};

struct adb_sync_wire_dent {
    uint32_t id;
    uint32_t mode;
    uint32_t size;
    uint32_t mtime;
    uint32_t namelen;
};

/* The one size a request has: the client refuses to send anything whose path is
 * longer than this (SendRequest(), and the same check on the "path,mode" of a
 * SEND), so a body arriving here cannot be longer, and a body is all a request
 * carries. Every path this file keeps is held with one byte over, for its
 * terminator. */
#define ADB_SYNC_PATH_MAX 1024u
/* How much is read from the card and sent in one DATA record. The client sends
 * up to 64KiB at a time and this side reassembles whatever arrives in whatever
 * pieces it arrives in, so this only decides how big the records are - and it is
 * what sizes the payload of the one record buffer, see struct adb_sync_packet. */
#define ADB_SYNC_DATA_MAX 2048u
#define ADB_SYNC_NAME_MAX VFS_MAX_PATHNAME
/* Room for the longest reason this file builds, which is a path and an errno. */
#define ADB_SYNC_MSG_MAX 160u

#define SYNC_RX_STATE_HEADER 0u
#define SYNC_RX_STATE_BODY   1u
#define SYNC_RX_STATE_DATA   2u
#define SYNC_RX_STATE_SKIP   3u

struct adb_sync_packet {
    struct adb_sync_wire_hdr hdr;
    uint8_t payload[ADB_SYNC_DATA_MAX];
};

struct adb_sync_priv {
    uint32_t req;

    uint8_t rx_state;
    uint32_t hdr_len;
    uint32_t body_len;
    uint32_t rx_remain;

    bool push_failed;
    bool push_isdir;
    int fd;

    bool have_target;
    bool target_isdir;
    char target[ADB_SYNC_PATH_MAX + 1u];

    char path[ADB_SYNC_PATH_MAX + 1u];

    struct adb_sync_packet packet;
} g_adb_sync;

static void adb_sync_send(const void *data, uint32_t len)
{
    if (len) {
        usbd_adb_write(ADB_LOCALID_SYNC, (const uint8_t *)data, (uint32_t)len);
    }
}

static void adb_sync_send_record(uint32_t id, const void *payload, uint32_t len)
{
    g_adb_sync.packet.hdr.id = id;
    g_adb_sync.packet.hdr.len = len;

    if (len) {
        memcpy(g_adb_sync.packet.payload, payload, len);
    }

    adb_sync_send(&g_adb_sync.packet, sizeof(g_adb_sync.packet.hdr) + len);
}

static void adb_sync_send_data_record(uint32_t len)
{
    g_adb_sync.packet.hdr.id = ID_DATA;
    g_adb_sync.packet.hdr.len = len;

    adb_sync_send(&g_adb_sync.packet, sizeof(g_adb_sync.packet.hdr) + len);
}

static void adb_sync_send_stat(uint32_t mode, uint32_t size, uint32_t mtime)
{
    struct adb_sync_wire_stat *st = (struct adb_sync_wire_stat *)&g_adb_sync.packet;

    st->id = ID_STAT;
    st->mode = mode;
    st->size = size;
    st->mtime = mtime;

    adb_sync_send(st, (uint32_t)sizeof(*st));
}

static void adb_sync_send_dent(uint32_t mode, uint32_t size, uint32_t mtime,
                               const char *name, uint32_t namelen)
{
    struct adb_sync_wire_dent *dent = (struct adb_sync_wire_dent *)&g_adb_sync.packet;

    dent->id = ID_DENT;
    dent->mode = mode;
    dent->size = size;
    dent->mtime = mtime;
    dent->namelen = namelen;

    memcpy((uint8_t *)dent + sizeof(*dent), name, namelen);

    adb_sync_send(dent, (uint32_t)sizeof(*dent) + namelen);
}

static void adb_sync_send_list_end(void)
{
    struct adb_sync_wire_dent *dent = (struct adb_sync_wire_dent *)&g_adb_sync.packet;

    memset(dent, 0, sizeof(*dent));
    dent->id = ID_DONE;

    adb_sync_send(dent, (uint32_t)sizeof(*dent));
}

static void adb_sync_send_recv_end(void)
{
    adb_sync_send_record(ID_DONE, NULL, 0u);
}

static void adb_sync_fail(const char *fmt, ...)
{
    char msg[ADB_SYNC_MSG_MAX];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    if ((n < 0) || ((size_t)n >= sizeof(msg))) {
        n = (int)sizeof(msg) - 1; /* the reason was long: it got cut off */
    }
    msg[n] = '\0';

    USB_LOG_ERR("adb sync: %s\r\n", msg);

    switch (g_adb_sync.req) {
        /* A SEND has its payload and the DONE behind it still on their way, and
         * a pushed file is answered exactly once: the rest of it is read and
         * dropped, and only this FAIL goes back. adbd does the same, for the
         * same reason. Only a SEND and a pull can read a FAIL at all. */
        case ID_SEND:
        case ID_RECV:
            if (g_adb_sync.req == ID_SEND) {
                g_adb_sync.push_failed = true;
                g_adb_sync.push_isdir = false;
            }

            adb_sync_send_record(ID_FAIL, msg, (uint32_t)n);
            break;

        case ID_STAT:
            /* A stat is a query and is answered with the record it asked for,
             * with nothing in it: it is a valid answer, and the client reads a
             * FAIL in its place as a protocol fault. */
            adb_sync_send_stat(0u, 0u, 0u);
            break;

        case ID_LIST:
            adb_sync_send_list_end();
            break;

        default:
            break; /* no request behind it, so there is nothing to answer */
    }
}

static bool adb_sync_path_is_root(const char *path)
{
    return strcmp(path, vfs_get_fs_root_path()) == 0;
}

static bool adb_sync_name_is_dir(const char *path)
{
    size_t i = strlen(path);

    while ((i > 0u) && (path[i - 1u] != '/')) {
        if (path[i - 1u] == '.') {
            return false; /* it has an extension, so it names a file */
        }
        i--;
    }

    return true;
}

/* Where a pushed directory lands is decided by the client, and it says so in
 * the path it sends: "src" at an existing "dst" arrives as "dst/Basename(src)/",
 * while "src/." arrives as "dst/./" - whose "." is a component like any other
 * here, so folding it away is exactly "the contents of src land in dst".
 * Both spellings therefore already arrive as the path they mean, and this side
 * never has to guess which of the two was asked for.
 *
 * A Windows host spells its local source with backslashes and the client takes
 * that basename verbatim, so "adb push .\output /sdc" arrives as
 * "/sdc/.\output/...". FatFs takes both separators alike, so the backslashes
 * are folded into forward ones before any component is looked at - otherwise
 * ".\output" stays a single, unmakable name and the push dies on the first
 * file. The checks above run first, so this never turns a relative path into
 * something that looks absolute. */
static bool adb_sync_build_full_path(const char *root, char *path, bool *wants_dir)
{
    char *dst0;
    char *dst;
    char *src;
    size_t len;
    size_t root_len;
    size_t i;

    *wants_dir = false;

    root_len = strlen(root);
    len = strlen(path);

    if ((root_len == 0u) || (len < root_len) || (strncmp(path, root, root_len) != 0)) {
        return false;
    }

    if ((len > root_len) && (root[root_len - 1u] != '/') && (path[root_len] != '/')) {
        return false;
    }

    for (i = 0u; i < len; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }

    *wants_dir = (len > 1u) && (path[len - 1u] == '/');

    dst0 = path;
    dst = path;
    src = path;

    for (;;) {
        char c = *src;

        if (c == '.') {
            if (src[1] == '\0') {
                src++; /* "." and the path ends there */
            } else if (src[1] == '/') {
                src += 2; /* "./" */
                while (*src == '/') {
                    src++;
                }
                continue;
            } else if ((src[1] == '.') && ((src[2] == '\0') || (src[2] == '/'))) {
                src += (src[2] == '/') ? 3 : 2;
                while (*src == '/') {
                    src++;
                }
                goto up_one; /* ".." */
            }
        }

        while (((c = *src++) != '\0') && (c != '/')) {
            *dst++ = c;
        }

        if (c != '/') {
            break; /* the path ends here */
        }

        *dst++ = '/';
        while ((c = *src++) == '/') {
            /* an empty component, "//" - nothing to copy */
        }
        src--;

        continue;

    up_one:
        /* keep the topmost root directory: there is nothing above it */
        if (((dst - dst0) != 1) || (dst[-1] != '/')) {
            if (dst > dst0) {
                dst--;
            }
        }
        while ((dst0 < dst) && (dst[-1] != '/')) {
            dst--;
        }
    }

    *dst = '\0';

    /* the separator that said "directory" is not part of the path */
    if (((dst - dst0) > 1) && (dst[-1] == '/')) {
        dst[-1] = '\0';
    }

    return true;
}

static bool adb_sync_path_join(const char *dir, const char *name, char *out, size_t out_size)
{
    size_t dir_len = strlen(dir);
    int n;

    if ((dir_len == 0u) || (dir[dir_len - 1u] == '/')) {
        n = snprintf(out, out_size, "%s%s", dir, name);
    } else {
        n = snprintf(out, out_size, "%s/%s", dir, name);
    }

    return (n > 0) && ((size_t)n < out_size);
}

static int adb_sync_path_type(const char *path, struct vfs_stat *info)
{
    if (adb_sync_path_is_root(path)) {
        memset(info, 0, sizeof(*info));
        info->st_mode = VFS_S_IFDIR | VFS_S_IRWXU | VFS_S_IRWXG | VFS_S_IRWXO;
        return 0;
    }

    return vfs_stat(path, info);
}

static int adb_sync_mkdir_one(const char *path)
{
    struct vfs_stat info;
    bool there = (adb_sync_path_type(path, &info) == 0);
    int ret;

    if (there && ((info.st_mode & VFS_S_IFMT) == VFS_S_IFDIR)) {
        return 0; /* already a directory: nothing to make, nothing to do */
    }

    if (there && (adb_sync_name_is_dir(path) == false)) {
        return VFS_ERR_NOTDIR; /* it is a file, and it is meant to be one */
    }

    if (there) {
        if (vfs_unlink(path) != 0) {
            return VFS_ERR_NOTDIR; /* still in the way, and it could not be taken away */
        }
    }

    ret = vfs_mkdir(path);
    if (ret != 0) {
        return ret; /* the port layer's own reason is the one worth passing on */
    }

    /* It says it worked, so say so too: which directories a push had to make is
     * the first thing worth knowing when a tree lands one level off. */
    USB_LOG_INFO("adb sync: mkdir '%s'\r\n", path);

    if ((adb_sync_path_type(path, &info) != 0) ||
        ((info.st_mode & VFS_S_IFMT) != VFS_S_IFDIR)) {
        /* It says it worked and the directory cannot be found: believe neither,
         * and let the caller report a failure rather than write into nothing. */
        return VFS_ERR_NOENT;
    }

    return 0;
}

static int adb_sync_mkdirs(char *path, bool last_is_dir)
{
    size_t end = strlen(path);
    size_t cut;
    size_t i;
    char saved;
    int ret;

    if (adb_sync_path_is_root(path)) {
        return 0; /* the root of the volume is always there */
    }

    if (last_is_dir == false) {
        for (cut = end; cut > 0u; cut--) {
            if (path[cut - 1u] == '/') {
                break;
            }
        }
        if (cut <= 1u) {
            return 0; /* the parent is the root, which always exists */
        }
        end = cut - 1u;
    }

    for (i = 1u; i < end; i++) {
        if (path[i] != '/') {
            continue;
        }

        path[i] = '\0';
        ret = adb_sync_mkdir_one(path);
        path[i] = '/';
        if (ret != 0) {
            return ret;
        }
    }

    saved = path[end];
    path[end] = '\0'; /* in front of the file name, or of the trailing slash */
    ret = adb_sync_mkdir_one(path);
    path[end] = saved;

    return ret;
}

static int adb_sync_write_err(const char *path, int err)
{
    struct vfs_stat info;

    if ((err == VFS_ERR_ROFS) && (adb_sync_path_type(path, &info) == 0) &&
        ((info.st_mode & VFS_S_IFMT) == VFS_S_IFDIR)) {
        return VFS_ERR_ISDIR;
    }

    return err;
}

static void adb_sync_fail_write(char *path, int err)
{
    struct vfs_stat info;
    char saved;
    size_t cut;
    size_t len = strlen(path);

    if (err == VFS_ERR_NOENT) {
        for (cut = 1u; cut < len; cut++) {
            if (path[cut] != '/') {
                continue;
            }

            saved = path[cut];
            path[cut] = '\0';

            if (adb_sync_path_type(path, &info) != 0) {
                adb_sync_fail("cannot write into '%s': that directory is not there", path);
                path[cut] = saved;
                return;
            }

            if ((info.st_mode & VFS_S_IFMT) != VFS_S_IFDIR) {
                adb_sync_fail("cannot write into '%s': a file is where a directory has to be", path);
                path[cut] = saved;
                return;
            }

            path[cut] = saved; /* an ancestor, and a directory as it should be */
        }
    }

    adb_sync_fail("cannot write '%s': %s", path, vfs_strerror(adb_sync_write_err(path, err)));
}

static void adb_sync_remember_target(const char *path, bool is_dir)
{
    size_t len = strlen(path);

    if (g_adb_sync.have_target) {
        return;
    }

    if (len >= sizeof(g_adb_sync.target)) {
        return;
    }

    memcpy(g_adb_sync.target, path, len);
    g_adb_sync.target[len] = '\0';
    g_adb_sync.target_isdir = is_dir;
    g_adb_sync.have_target = true;
}

static void adb_sync_do_stat(const char *path, bool wants_dir)
{
    struct vfs_stat info;
    bool as_dir = (wants_dir || adb_sync_name_is_dir(path));
    bool is_dir;
    int ret;

    USB_LOG_INFO("adb sync: stat '%s'\r\n", path);

    ret = adb_sync_path_type(path, &info);
    is_dir = (ret == 0) && ((info.st_mode & VFS_S_IFMT) == VFS_S_IFDIR);

    if (is_dir) {
        adb_sync_remember_target(path, true);
        adb_sync_send_stat(info.st_mode, (uint32_t)info.st_size, info.st_mtime);
    } else if (as_dir) {
        adb_sync_remember_target(path, true);
        adb_sync_send_stat(VFS_S_IFDIR | VFS_S_IRWXU | VFS_S_IRWXG | VFS_S_IRWXO, 0u, 0u);
    } else if (ret == 0) {
        adb_sync_remember_target(path, false);
        adb_sync_send_stat(info.st_mode, (uint32_t)info.st_size, info.st_mtime);
    } else {
        adb_sync_remember_target(path, false);
        adb_sync_fail("'%s' does not exist", path);
    }
}

static void adb_sync_do_list(char *path)
{
    struct vfs_dirent *de;
    struct vfs_stat info;
    VFS_DIR *dir;
    uint32_t namelen;
    char *entry = (char *)g_adb_sync.packet.payload;

    USB_LOG_INFO("adb sync: listing '%s'\r\n", path);

    dir = vfs_opendir(path);
    if (!dir) {
        adb_sync_fail("cannot read '%s' as a directory", path);
        return;
    }

    while ((de = vfs_readdir(dir)) != NULL) {
        namelen = de->d_namlen;

        /* "." and ".." are never part of a listing */
        if ((namelen == 0u) || (namelen > ADB_SYNC_NAME_MAX)) {
            continue;
        }
        if ((strcmp(de->d_name, ".") == 0) || (strcmp(de->d_name, "..") == 0)) {
            continue;
        }

        if (adb_sync_path_join(path, de->d_name, entry,
                               sizeof(g_adb_sync.packet.payload)) == false) {
            continue;
        }

        if (vfs_stat(entry, &info) == 0) {
            adb_sync_send_dent(info.st_mode, (uint32_t)info.st_size, info.st_mtime,
                               de->d_name, namelen);
        }
    }

    vfs_closedir(dir);

    adb_sync_send_list_end();
}

static void adb_sync_do_recv(const char *path)
{
    int fd;
    int n;

    USB_LOG_INFO("adb sync: reading '%s'\r\n", path);

    fd = vfs_open(path, VFS_O_RDONLY);
    if (fd < 0) {
        adb_sync_fail("cannot open '%s' for reading: %s", path, vfs_strerror(fd));
        return;
    }

    for (;;) {
        n = vfs_read(fd, g_adb_sync.packet.payload, ADB_SYNC_DATA_MAX);
        if (n < 0) {
            vfs_close(fd);
            adb_sync_fail("cannot read '%s': %s", path, vfs_strerror(n));
            return; /* the FAIL is the answer; the client reads it as failure */
        }

        if (n == 0) {
            break;
        }

        adb_sync_send_data_record((uint32_t)n);
    }

    vfs_close(fd);

    adb_sync_send_recv_end();
}

static void adb_sync_do_send(char *path, bool wants_dir, uint32_t mode)
{
    int ret;

    if ((mode & VFS_S_IFMT) == VFS_S_IFDIR) {
        ret = adb_sync_mkdirs(path, true);
        if (ret != 0) {
            adb_sync_fail("cannot make '%s' a directory: %s", path, vfs_strerror(ret));
            return;
        }

        g_adb_sync.push_isdir = true;
        g_adb_sync.fd = -1;
        return;
    }

    if (g_adb_sync.have_target && g_adb_sync.target_isdir &&
        (strcmp(path, g_adb_sync.target) == 0)) {
        ret = adb_sync_mkdirs(path, true);
        if (ret != 0) {
            adb_sync_fail("cannot make '%s' a directory: %s", path, vfs_strerror(ret));
        } else {
            adb_sync_fail("'%s' is a directory, so no file can be written at it", path);
        }
        return;
    }

    if (wants_dir) {
        ret = adb_sync_mkdirs(path, true);
        if (ret != 0) {
            adb_sync_fail("cannot make '%s' a directory: %s", path, vfs_strerror(ret));
        } else {
            adb_sync_fail("'%s' is a directory, so no file can be written at it", path);
        }
        return;
    }

    USB_LOG_INFO("adb sync: writing '%s'\r\n", path);

    g_adb_sync.fd = vfs_open(path, VFS_O_WRONLY);
    if (g_adb_sync.fd == VFS_ERR_NOENT) {
        ret = adb_sync_mkdirs(path, false);
        if (ret != 0) {
            adb_sync_fail_write(path, ret);
            return;
        }
        g_adb_sync.fd = vfs_open(path, VFS_O_WRONLY);
    }

    if (g_adb_sync.fd < 0) {
        ret = g_adb_sync.fd;
        g_adb_sync.fd = -1;
        adb_sync_fail_write(path, ret);
        return;
    }
}

static void adb_sync_do_send_done(void)
{
    if (g_adb_sync.fd >= 0) {
        vfs_close(g_adb_sync.fd);
        g_adb_sync.fd = -1;
    }

    if (g_adb_sync.push_isdir) {
        g_adb_sync.push_isdir = false;
        adb_sync_send_record(ID_OKAY, NULL, 0u);
        return;
    }

    if (g_adb_sync.push_failed) {
        g_adb_sync.push_failed = false;
        return;
    }

    adb_sync_send_record(ID_OKAY, NULL, 0u);
}

static void adb_sync_data_write(const uint8_t *data, uint32_t len)
{
    int n;

    if (g_adb_sync.fd < 0) {
        return; /* refused, or a directory: the bytes are dropped */
    }

    n = vfs_write(g_adb_sync.fd, data, len);
    if (n != (int)len) {
        int err = (n < 0) ? n : VFS_ERR_IO; /* a short write counts as I/O */

        vfs_close(g_adb_sync.fd);
        g_adb_sync.fd = -1;

        adb_sync_fail("cannot write '%s': %s", g_adb_sync.path, vfs_strerror(err));
    }
}

static void adb_sync_reset(void)
{
    if (g_adb_sync.fd >= 0) {
        vfs_close(g_adb_sync.fd);
        g_adb_sync.fd = -1;
    }

    g_adb_sync.rx_state = SYNC_RX_STATE_HEADER;
    g_adb_sync.hdr_len = 0;
    g_adb_sync.body_len = 0;
    g_adb_sync.rx_remain = 0;
    g_adb_sync.req = 0;
    g_adb_sync.push_failed = false;
    g_adb_sync.push_isdir = false;
    g_adb_sync.have_target = false;
    g_adb_sync.target_isdir = false;
}

static void adb_sync_dispatch(uint32_t id, char *body)
{
    bool wants_dir = false;
    uint32_t mode = 0u;
    size_t len;

    g_adb_sync.req = id;

    if (id == ID_QUIT) {
        adb_sync_reset();
        return;
    }

    if ((id != ID_STAT) && (id != ID_LIST) && (id != ID_RECV) && (id != ID_SEND)) {
        adb_sync_fail("unsupported request 0x%08x", (unsigned)id);
        return;
    }

    if (id == ID_SEND) {
        char *comma = strrchr(body, ',');

        if (comma == NULL) {
            adb_sync_fail("the SEND request came without a mode behind its path");
            return;
        }

        mode = (uint32_t)strtoul(comma + 1, NULL, 0);
        *comma = '\0';
    }

    len = strlen(body);
    memcpy(g_adb_sync.path, body, len + 1u);

    if (adb_sync_build_full_path(vfs_get_fs_root_path(), g_adb_sync.path, &wants_dir) == false) {
        adb_sync_fail("the path has to be absolute, starting with '%s', not '%s'",
                      vfs_get_fs_root_path(), g_adb_sync.path);
        return;
    }

    switch (id) {
        case ID_STAT:
            adb_sync_do_stat(g_adb_sync.path, wants_dir);
            break;
        case ID_LIST:
            adb_sync_do_list(g_adb_sync.path);
            break;
        case ID_RECV:
            adb_sync_do_recv(g_adb_sync.path);
            break;
        case ID_SEND:
            adb_sync_do_send(g_adb_sync.path, wants_dir, mode);
            break;
        default:
            break; /* not reachable: the four above are the only ones left */
    }
}

static void adb_sync_header_ready(void)
{
    uint32_t id = g_adb_sync.packet.hdr.id;
    uint32_t len = g_adb_sync.packet.hdr.len;

    if (id == ID_DONE) {
        adb_sync_do_send_done();
        g_adb_sync.rx_state = SYNC_RX_STATE_HEADER;
        return;
    }

    if (id == ID_DATA) {
        g_adb_sync.rx_remain = len;
        g_adb_sync.rx_state = len ? SYNC_RX_STATE_DATA : SYNC_RX_STATE_HEADER;
        return;
    }

    if ((id == ID_STAT) || (id == ID_LIST) || (id == ID_RECV) || (id == ID_SEND)) {
        if (len > ADB_SYNC_PATH_MAX) {
            g_adb_sync.req = id;
            g_adb_sync.rx_remain = len;
            g_adb_sync.rx_state = SYNC_RX_STATE_SKIP; /* read it and drop it */
            adb_sync_fail("a request of %u bytes is longer than the %u bytes a path may be",
                          (unsigned)len, (unsigned)ADB_SYNC_PATH_MAX);
            return;
        }

        g_adb_sync.body_len = 0;
        g_adb_sync.rx_remain = len;

        if (len == 0) {
            g_adb_sync.packet.payload[0] = '\0';
            adb_sync_dispatch(id, (char *)g_adb_sync.packet.payload);
            g_adb_sync.rx_state = SYNC_RX_STATE_HEADER;
        } else {
            g_adb_sync.rx_state = SYNC_RX_STATE_BODY;
        }
        return;
    }

    g_adb_sync.rx_remain = len;
    g_adb_sync.rx_state = len ? SYNC_RX_STATE_SKIP : SYNC_RX_STATE_HEADER;
    adb_sync_dispatch(id, (char *)g_adb_sync.packet.payload);
}

static void adb_sync_consume(const uint8_t *data, uint32_t len)
{
    uint8_t *body = g_adb_sync.packet.payload;
    uint32_t take;

    while (len) {
        switch (g_adb_sync.rx_state) {
            case SYNC_RX_STATE_HEADER:
                take = sizeof(struct adb_sync_wire_hdr) - g_adb_sync.hdr_len;
                if (take > len) {
                    take = len;
                }
                memcpy((uint8_t *)&g_adb_sync.packet.hdr + g_adb_sync.hdr_len, data, take);
                g_adb_sync.hdr_len += take;
                data += take;
                len -= take;

                if (g_adb_sync.hdr_len == sizeof(struct adb_sync_wire_hdr)) {
                    g_adb_sync.hdr_len = 0;
                    adb_sync_header_ready();
                }
                break;

            case SYNC_RX_STATE_BODY:
                take = (g_adb_sync.rx_remain < len) ? g_adb_sync.rx_remain : len;
                memcpy(body + g_adb_sync.body_len, data, take);
                g_adb_sync.body_len += take;
                g_adb_sync.rx_remain -= take;
                data += take;
                len -= take;

                if (g_adb_sync.rx_remain == 0) {
                    body[g_adb_sync.body_len] = '\0';
                    adb_sync_dispatch(g_adb_sync.packet.hdr.id, (char *)body);
                    g_adb_sync.rx_state = SYNC_RX_STATE_HEADER;
                }
                break;

            case SYNC_RX_STATE_DATA:
                take = (g_adb_sync.rx_remain < len) ? g_adb_sync.rx_remain : len;
                adb_sync_data_write(data, take);
                g_adb_sync.rx_remain -= take;
                data += take;
                len -= take;

                if (g_adb_sync.rx_remain == 0) {
                    g_adb_sync.rx_state = SYNC_RX_STATE_HEADER;
                }
                break;

            case SYNC_RX_STATE_SKIP:
            default:
                take = (g_adb_sync.rx_remain < len) ? g_adb_sync.rx_remain : len;
                g_adb_sync.rx_remain -= take;
                data += take;
                len -= take;

                if (g_adb_sync.rx_remain == 0) {
                    g_adb_sync.rx_state = SYNC_RX_STATE_HEADER;
                }
                break;
        }
    }
}

static void usbd_adb_sync_on_open(uint32_t remoteid)
{
    adb_sync_reset();
    USB_LOG_INFO("adb sync open, remoteid:%u\r\n", (unsigned)remoteid);
}

static void usbd_adb_sync_on_close(uint32_t remoteid)
{
    (void)remoteid;

    adb_sync_reset();
    USB_LOG_INFO("adb sync close, remoteid:%u\r\n", (unsigned)remoteid);
}

static void usbd_adb_sync_on_write(uint32_t remoteid, const uint8_t *data, uint32_t len)
{
    (void)remoteid;

    if (!data || !len) {
        return;
    }

    adb_sync_consume(data, len);
}

static const struct adb_service adb_sync_service = {
    .name = "sync:",
    .localid = ADB_LOCALID_SYNC,
    .on_open = usbd_adb_sync_on_open,
    .on_close = usbd_adb_sync_on_close,
    .on_write = usbd_adb_sync_on_write,
};

void usbd_adb_sync_init(void)
{
    g_adb_sync.fd = -1;
    adb_sync_reset();

    if (usbd_adb_service_register(&adb_sync_service) != 0) {
        USB_LOG_ERR("adb sync: registering the service failed\r\n");
    }
}
