/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#ifndef USB_VFS_H
#define USB_VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_O_RDONLY 0 /* +1 == FREAD, existing file only          */
#define VFS_O_WRONLY 1 /* +1 == FWRITE, create / truncate          */
#define VFS_O_RDWR   2 /* +1 == FREAD|FWRITE, create, no truncate  */

/* file mode bits, POSIX layout so they can go straight into the wire reply */
#define VFS_S_IFMT   0170000 /* type of file */
#define VFS_S_IFDIR  0040000 /* directory */
#define VFS_S_IFCHR  0020000 /* character special */
#define VFS_S_IFBLK  0060000 /* block special */
#define VFS_S_IFREG  0100000 /* regular */
#define VFS_S_IFLNK  0120000 /* symbolic link */
#define VFS_S_IFSOCK 0140000 /* socket */
#define VFS_S_IFIFO  0010000 /* fifo */
#define VFS_S_IREAD  0000400 /* read permission, owner */
#define VFS_S_IWRITE 0000200 /* write permission, owner */
#define VFS_S_IEXEC  0000100 /* execute/search permission, owner */
#define VFS_S_ENFMT  0002000 /* enforcement-mode locking */

#define VFS_S_IRWXU (VFS_S_IRUSR | VFS_S_IWUSR | VFS_S_IXUSR)
#define VFS_S_IRUSR 0000400 /* read permission, owner */
#define VFS_S_IWUSR 0000200 /* write permission, owner */
#define VFS_S_IXUSR 0000100 /* execute/search permission, owner */
#define VFS_S_IRWXG (VFS_S_IRGRP | VFS_S_IWGRP | VFS_S_IXGRP)
#define VFS_S_IRGRP 0000040 /* read permission, group */
#define VFS_S_IWGRP 0000020 /* write permission, group */
#define VFS_S_IXGRP 0000010 /* execute/search permission, group */
#define VFS_S_IRWXO (VFS_S_IROTH | VFS_S_IWOTH | VFS_S_IXOTH)
#define VFS_S_IROTH 0000004 /* read permission, other */
#define VFS_S_IWOTH 0000002 /* write permission, other */
#define VFS_S_IXOTH 0000001 /* execute/search permission, other */

#define VFS_ERR_GENERIC (-1)  /* unspecified                          */
#define VFS_ERR_NOENT   (-2)  /* no such file or directory            */
#define VFS_ERR_IO      (-3)  /* low level I/O error                  */
#define VFS_ERR_NOTDIR  (-4)  /* a path component is not a directory  */
#define VFS_ERR_ISDIR   (-5)  /* it is a directory, not a file        */
#define VFS_ERR_EXIST   (-6)  /* already exists                       */
#define VFS_ERR_NOSPC   (-7)  /* no space left on device              */
#define VFS_ERR_ROFS    (-8)  /* read-only file system                */
#define VFS_ERR_INVAL   (-9)  /* invalid argument or invalid name     */
#define VFS_ERR_NOMEM   (-10) /* out of memory                        */
#define VFS_ERR_AGAIN   (-11) /* busy, try again                      */
#define VFS_ERR_NODEV   (-12) /* no file system mounted on that path  */

#ifndef VFS_MAX_PATHNAME
#define VFS_MAX_PATHNAME 256
#endif

typedef void VFS_DIR;

struct vfs_statfs {
    size_t f_bsize;  /* block size */
    size_t f_blocks; /* total data blocks in file system */
    size_t f_bfree;  /* free blocks in file system */
};

struct vfs_stat {
    uint32_t st_dev;
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    size_t st_size;
    size_t st_blksize;
    size_t st_blocks;
    uint32_t st_mtime;
};

struct vfs_dirent {
    uint8_t d_type;                /* The type of the file */
    uint8_t d_namlen;              /* The length of the not including the terminating null file name */
    uint16_t d_reclen;             /* length of this record */
    char d_name[VFS_MAX_PATHNAME]; /* The null-terminated file name */
};

#ifdef __cplusplus
extern "C" {
#endif

const char *vfs_get_fs_root_path(void);

const char *vfs_get_fs_description(void);

int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);

VFS_DIR *vfs_opendir(const char *name);
int vfs_closedir(VFS_DIR *d);
struct vfs_dirent *vfs_readdir(VFS_DIR *d);

int vfs_statfs(const char *path, struct vfs_statfs *buf);
int vfs_stat(const char *file, struct vfs_stat *buf);

int vfs_open(const char *path, uint8_t mode);
int vfs_close(int fd);
int vfs_read(int fd, void *buf, size_t len);
int vfs_write(int fd, const void *buf, size_t len);

int vfs_unlink(const char *path);

static inline const char *vfs_strerror(int err)
{
    switch (err) {
        case 0:
            return "success";
        case VFS_ERR_NOENT:
            return "no such file or directory";
        case VFS_ERR_IO:
            return "input/output error";
        case VFS_ERR_NOTDIR:
            return "not a directory";
        case VFS_ERR_ISDIR:
            return "is a directory";
        case VFS_ERR_EXIST:
            return "file exists";
        case VFS_ERR_NOSPC:
            return "no space left on device";
        case VFS_ERR_ROFS:
            return "read-only file system";
        case VFS_ERR_INVAL:
            return "invalid argument";
        case VFS_ERR_NOMEM:
            return "out of memory";
        case VFS_ERR_AGAIN:
            return "resource temporarily unavailable";
        case VFS_ERR_NODEV:
            return "no such device or address";
        default:
            return "unspecified error";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* USB_VFS_H */
