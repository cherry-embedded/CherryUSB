/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_MTP_H
#define USBD_MTP_H

#include "usb_mtp.h"

#define MTP_O_RDONLY 0 /* +1 == FREAD */
#define MTP_O_WRONLY 1 /* +1 == FWRITE */
#define MTP_O_RDWR   2 /* +1 == FREAD|FWRITE */

#define MTP_S_IFMT   0170000 /* type of file */
#define MTP_S_IFDIR  0040000 /* directory */
#define MTP_S_IFCHR  0020000 /* character special */
#define MTP_S_IFBLK  0060000 /* block special */
#define MTP_S_IFREG  0100000 /* regular */
#define MTP_S_IFLNK  0120000 /* symbolic link */
#define MTP_S_IFSOCK 0140000 /* socket */
#define MTP_S_IFIFO  0010000 /* fifo */
#define MTP_S_IREAD  0000400 /* read permission, owner */
#define MTP_S_IWRITE 0000200 /* write permission, owner */
#define MTP_S_IEXEC  0000100 /* execute/search permission, owner */
#define MTP_S_ENFMT  0002000 /* enforcement-mode locking */

#define MTP_S_IRWXU (MTP_S_IRUSR | MTP_S_IWUSR | MTP_S_IXUSR)
#define MTP_S_IRUSR 0000400 /* read permission, owner */
#define MTP_S_IWUSR 0000200 /* write permission, owner */
#define MTP_S_IXUSR 0000100 /* execute/search permission, owner */
#define MTP_S_IRWXG (MTP_S_IRGRP | MTP_S_IWGRP | MTP_S_IXGRP)
#define MTP_S_IRGRP 0000040 /* read permission, group */
#define MTP_S_IWGRP 0000020 /* write permission, grougroup */
#define MTP_S_IXGRP 0000010 /* execute/search permission, group */
#define MTP_S_IRWXO (MTP_S_IROTH | MTP_S_IWOTH | MTP_S_IXOTH)
#define MTP_S_IROTH 0000004 /* read permission, other */
#define MTP_S_IWOTH 0000002 /* write permission, other */
#define MTP_S_IXOTH 0000001 /* execute/search permission, other */

typedef void MTP_DIR;

struct mtp_statfs {
    size_t f_bsize;  /* block size */
    size_t f_blocks; /* total data blocks in file system */
    size_t f_bfree;  /* free blocks in file system */
};

struct mtp_stat {
    uint32_t st_dev;
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    size_t st_size;
    size_t st_blksize;
    size_t st_blocks;
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

struct usbd_interface *usbd_mtp_init_intf(struct usbd_interface *intf,
                                          const uint8_t out_ep,
                                          const uint8_t in_ep,
                                          const uint8_t int_ep);

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
int usbd_mtp_stat(const char *file, struct mtp_stat *buf);

int usbd_mtp_open(const char *path, uint8_t mode);
int usbd_mtp_close(int fd);
int usbd_mtp_read(int fd, void *buf, size_t len);
int usbd_mtp_write(int fd, const void *buf, size_t len);

int usbd_mtp_unlink(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* USBD_MTP_H */
