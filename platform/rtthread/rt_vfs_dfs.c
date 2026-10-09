/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <rtthread.h>
#include <rtdevice.h>

#include <string.h>
#include <errno.h>

#include <dfs_fs.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "usbd_core.h"
#include "usb_vfs.h"

#ifndef RT_USING_DFS
#error "RT_USING_DFS must be enabled to use adb sync over the RT-Thread file system"
#endif

#ifndef CONFIG_USB_DFS_MOUNT_POINT
#define CONFIG_USB_DFS_MOUNT_POINT "/"
#endif

/* -------------------------------------------------------------------------
 * helpers
 * ---------------------------------------------------------------------- */

/**
 * POSIX errno as a plain positive value.
 *
 * RT-Thread stores whatever code it was handed, and the DFS layer is not
 * consistent about the sign, so it is normalised before being looked at.
 */
static int dfs_errno(void)
{
    int err = errno;

    return (err < 0) ? -err : err;
}

/** errno -> VFS_ERR_*, so the host gets a readable reason. */
static int dfs_errno_to_vfs(int err)
{
    switch (err) {
        case ENOENT:
            return VFS_ERR_NOENT;
        case EEXIST:
            return VFS_ERR_EXIST;
        case EINVAL:
            return VFS_ERR_INVAL;
        case ENOTDIR:
            return VFS_ERR_NOTDIR;
        case EISDIR:
            return VFS_ERR_ISDIR;
        case ENOSPC:
            return VFS_ERR_NOSPC;
        case ENOMEM:
            return VFS_ERR_NOMEM;
        case EBUSY:
        case EAGAIN:
            return VFS_ERR_AGAIN;
        case ENODEV:
            return VFS_ERR_NODEV;
        case EROFS:
        case EACCES:
        case EPERM:
            return VFS_ERR_ROFS;
        case EIO:
        default:
            return VFS_ERR_IO;
    }
}

const char *vfs_get_fs_root_path(void)
{
    return CONFIG_USB_DFS_MOUNT_POINT;
}

const char *vfs_get_fs_description(void)
{
    return "CherryUSB VFS DFS";
}

int vfs_mkdir(const char *path)
{
    if (!path || !path[0]) {
        return VFS_ERR_INVAL;
    }

    if (mkdir(path, 0777) < 0) {
        /* adb push of a directory tree re-sends SEND for directories it has
         * already created, so "it is already there" has to count as success */
        int err = dfs_errno();

        if (err != EEXIST) {
            return dfs_errno_to_vfs(err);
        }
    }

    return 0;
}

int vfs_rmdir(const char *path)
{
    if (!path || !path[0]) {
        return VFS_ERR_INVAL;
    }

    if (rmdir(path) < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return 0;
}

VFS_DIR *vfs_opendir(const char *name)
{
    DIR *dir;

    if (!name || !name[0]) {
        return NULL;
    }

    dir = opendir(name);
    if (!dir) {
        return NULL;
    }

    return (VFS_DIR *)dir;
}

int vfs_closedir(VFS_DIR *dir)
{
    if (!dir) {
        return VFS_ERR_INVAL;
    }

    if (closedir((DIR *)dir) < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return 0;
}

struct vfs_dirent *vfs_readdir(VFS_DIR *dir)
{
    static struct vfs_dirent dirent;
    struct dirent *entry;

    if (!dir) {
        return NULL;
    }

    entry = readdir((DIR *)dir);
    if (!entry) {
        return NULL; /* end of directory, or a hard error */
    }

    memset(&dirent, 0, sizeof(dirent));
    strncpy(dirent.d_name, entry->d_name, sizeof(dirent.d_name) - 1);
    dirent.d_name[sizeof(dirent.d_name) - 1] = '\0';
    dirent.d_namlen = (uint8_t)strlen(dirent.d_name);
    /* d_type is just the type field of a mode shifted down, see usb_vfs.h.
     * RT-Thread hands back DT_*, so the two cases that matter are translated
     * and anything else (including DT_UNKNOWN) falls back to a regular file. */
    dirent.d_type = (entry->d_type == DT_DIR) ? (uint8_t)(VFS_S_IFDIR >> 12)
                                              : (uint8_t)(VFS_S_IFREG >> 12);

    return &dirent;
}

int vfs_stat(const char *path, struct vfs_stat *buf)
{
    struct stat st;

    if (!path || !path[0] || !buf) {
        return VFS_ERR_INVAL;
    }

    if (stat(path, &st) < 0) {
        /* Stay quiet on purpose: adb stats a path precisely to find out whether
         * it exists, so "not found" is a normal answer and not an error. The
         * service reports it to the host as "mode 0". */
        return dfs_errno_to_vfs(dfs_errno());
    }

    memset(buf, 0, sizeof(*buf));
    if (S_ISDIR(st.st_mode)) {
        buf->st_mode = VFS_S_IFDIR | VFS_S_IRWXU | VFS_S_IRWXG | VFS_S_IRWXO;
    } else {
        buf->st_mode = VFS_S_IFREG | VFS_S_IRUSR | VFS_S_IRGRP | VFS_S_IROTH |
                       VFS_S_IWUSR | VFS_S_IWGRP | VFS_S_IWOTH;
    }

    buf->st_dev = 0;
    buf->st_nlink = 1;
    buf->st_size = st.st_size;
    buf->st_mtime = st.st_mtime;
    /* st_blksize/st_blocks are left at zero: RT-Thread's struct stat does not
     * carry them consistently across DFS versions, and the adb STAT record
     * only ever reports mode, size and mtime. */
    return 0;
}

int vfs_statfs(const char *path, struct vfs_statfs *buf)
{
    struct statfs st;

    if (!path || !path[0] || !buf) {
        return VFS_ERR_INVAL;
    }

    if (dfs_statfs(path, &st) < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    buf->f_bsize = st.f_bsize;
    buf->f_blocks = st.f_blocks;
    buf->f_bfree = st.f_bfree;
    return 0;
}

int vfs_open(const char *path, uint8_t mode)
{
    int flags;
    int fd;

    if (!path || !path[0]) {
        return VFS_ERR_INVAL;
    }

    if (mode == VFS_O_RDONLY) {
        flags = O_RDONLY; /* existing file only */
    } else if (mode == VFS_O_WRONLY) {
        flags = O_WRONLY | O_CREAT | O_TRUNC; /* create / truncate */
    } else if (mode == VFS_O_RDWR) {
        flags = O_RDWR | O_CREAT; /* create, no truncate */
    } else {
        return VFS_ERR_INVAL;
    }

    fd = open(path, flags, 0666);
    if (fd < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return fd;
}

int vfs_close(int fd)
{
    if (fd < 0) {
        return VFS_ERR_INVAL;
    }

    if (close(fd) < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return 0;
}

int vfs_read(int fd, void *buf, size_t len)
{
    int ret;

    if (fd < 0) {
        return VFS_ERR_INVAL;
    }

    ret = (int)read(fd, buf, len);
    if (ret < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return ret;
}

int vfs_write(int fd, const void *buf, size_t len)
{
    int ret;

    if (fd < 0) {
        return VFS_ERR_INVAL;
    }

    ret = (int)write(fd, buf, len);
    if (ret < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return ret;
}

int vfs_unlink(const char *path)
{
    if (!path || !path[0]) {
        return VFS_ERR_INVAL;
    }

    if (unlink(path) < 0) {
        return dfs_errno_to_vfs(dfs_errno());
    }

    return 0;
}
