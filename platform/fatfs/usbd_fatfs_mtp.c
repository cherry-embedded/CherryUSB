/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ff.h"
#include "diskio.h"
#include "usbd_core.h"
#include "usb_osal.h"
#include "usbd_mtp.h"

FATFS s_sd_disk;
FIL s_file;
BYTE work[FF_MAX_SS];

const TCHAR driver_num_buf[4] = { '0', ':', '/', '\0' };

const char *show_error_string(FRESULT fresult);

static FRESULT sd_mount_fs(void)
{
    FRESULT fresult = f_mount(&s_sd_disk, driver_num_buf, 1);
    if (fresult == FR_OK) {
        printf("SD card has been mounted successfully\n");
    } else {
        printf("Failed to mount SD card, cause: %s\n", show_error_string(fresult));
    }

    return fresult;
}

#if 0
static FRESULT sd_mkfs(void)
{
    printf("Formatting the SD card, depending on the SD card capacity, the formatting process may take a long time\n");
    FRESULT fresult = f_mkfs(driver_num_buf, NULL, work, sizeof(work));
    if (fresult != FR_OK) {
        printf("Making File system failed, cause: %s\n", show_error_string(fresult));
    } else {
        printf("Making file system is successful\n");
    }

    return fresult;
}
#endif

static FRESULT sd_write_file(void)
{
    FRESULT fresult = f_open(&s_file, "0:/readme.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fresult != FR_OK) {
        printf("Create new file failed, cause: %d\n", show_error_string(fresult));
    } else {
        printf("Create new file successfully, status=%d\n", fresult);
    }
    char hello_str[] = "Hello, this is SD card FATFS demo\n";
    UINT byte_written;
    fresult = f_write(&s_file, hello_str, sizeof(hello_str), &byte_written);
    if (fresult != FR_OK) {
        printf("Write file failed, cause: %s\n", show_error_string(fresult));
    } else {
        printf("Write file operation is successfully\n");
    }

    f_close(&s_file);

    return fresult;
}

const char *show_error_string(FRESULT fresult)
{
    const char *result_str;

    switch (fresult) {
        case FR_OK:
            result_str = "succeeded";
            break;
        case FR_DISK_ERR:
            result_str = "A hard error occurred in the low level disk I/O level";
            break;
        case FR_INT_ERR:
            result_str = "Assertion failed";
            break;
        case FR_NOT_READY:
            result_str = "The physical drive cannot work";
            break;
        case FR_NO_FILE:
            result_str = "Could not find the file";
            break;
        case FR_NO_PATH:
            result_str = "Could not find the path";
            break;
        case FR_INVALID_NAME:
            result_str = "Tha path name format is invalid";
            break;
        case FR_DENIED:
            result_str = "Access denied due to prohibited access or directory full";
            break;
        case FR_EXIST:
            result_str = "Access denied due to prohibited access";
            break;
        case FR_INVALID_OBJECT:
            result_str = "The file/directory object is invalid";
            break;
        case FR_WRITE_PROTECTED:
            result_str = "The physical drive is write protected";
            break;
        case FR_INVALID_DRIVE:
            result_str = "The logical driver number is invalid";
            break;
        case FR_NOT_ENABLED:
            result_str = "The volume has no work area";
            break;
        case FR_NO_FILESYSTEM:
            result_str = "There is no valid FAT volume";
            break;
        case FR_MKFS_ABORTED:
            result_str = "THe f_mkfs() aborted due to any problem";
            break;
        case FR_TIMEOUT:
            result_str = "Could not get a grant to access the volume within defined period";
            break;
        case FR_LOCKED:
            result_str = "The operation is rejected according to the file sharing policy";
            break;
        case FR_NOT_ENOUGH_CORE:
            result_str = "LFN working buffer could not be allocated";
            break;
        case FR_TOO_MANY_OPEN_FILES:
            result_str = "Number of open files > FF_FS_LOCK";
            break;
        case FR_INVALID_PARAMETER:
            result_str = "Given parameter is invalid";
            break;
        default:
            result_str = "Unknown error";
            break;
    }
    return result_str;
}

const char *usbd_mtp_fs_root_path(void)
{
    return driver_num_buf;
}

const char *usbd_mtp_fs_description(void)
{
    return "CherryUSB MTP";
}

int usbd_mtp_mkdir(const char *path)
{
    FRESULT result = f_mkdir(path);
    if (result != FR_OK) {
        printf("f_mkdir failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return 0; // Directory created successfully
}

int usbd_mtp_rmdir(const char *path)
{
    FRESULT result = f_rmdir(path);
    if (result != FR_OK) {
        printf("f_mkdir failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return 0; // Directory created successfully
}

MTP_DIR *usbd_mtp_opendir(const char *name)
{
    FRESULT result;
    DIR *dir;

    dir = usb_osal_malloc(sizeof(DIR));
    result = f_opendir(dir, name);
    if (result != FR_OK) {
        printf("f_opendir failed, cause: %s\n", show_error_string(result));
        usb_osal_free(dir);
        return NULL; // Failed to open directory
    }
    return (MTP_DIR *)dir;
}

int usbd_mtp_closedir(MTP_DIR *dir)
{
    FRESULT result;
    result = f_closedir((DIR *)dir);
    if (result != FR_OK) {
        printf("f_closedir failed, cause: %s\n", show_error_string(result));
        return -1; // Failed to close directory
    }
    usb_osal_free(dir); // Free the directory structure
    return result;
}

struct mtp_dirent *usbd_mtp_readdir(MTP_DIR *dir)
{
    FILINFO fno;
    FRESULT result;

    result = f_readdir((DIR *)dir, &fno);
    if (result != FR_OK || fno.fname[0] == 0)
        return NULL;

    static struct mtp_dirent dirent;
    memset(&dirent, 0, sizeof(struct mtp_dirent));
    strncpy(dirent.d_name, fno.fname, sizeof(dirent.d_name) - 1);
    dirent.d_name[sizeof(dirent.d_name) - 1] = '\0';
    dirent.d_namlen = strlen(dirent.d_name);

    return &dirent;
}

#undef SS
#if FF_MAX_SS == FF_MIN_SS
#define SS(fs) ((UINT)FF_MAX_SS) /* Fixed sector size */
#else
#define SS(fs) ((fs)->ssize) /* Variable sector size */
#endif

int usbd_mtp_stat(const char *path, struct stat *buf)
{
    FILINFO file_info;
    FRESULT result;
    FATFS *f;
    f = &s_sd_disk;

    result = f_stat(path, &file_info);
    if (result != FR_OK) {
        printf("f_stat failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    buf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH |
                   S_IWUSR | S_IWGRP | S_IWOTH;
    if (file_info.fattrib & AM_DIR) {
        buf->st_mode &= ~S_IFREG;
        buf->st_mode |= S_IFDIR | S_IXUSR | S_IXGRP | S_IXOTH;
    }
    if (file_info.fattrib & AM_RDO)
        buf->st_mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);

    buf->st_size = file_info.fsize;
    buf->st_blksize = f->csize * SS(f);
    if (file_info.fattrib & AM_ARC) {
        buf->st_blocks = file_info.fsize ? ((file_info.fsize - 1) / SS(f) / f->csize + 1) : 0;
        buf->st_blocks *= (buf->st_blksize / 512); // man say st_blocks is number of 512B blocks allocated
    } else {
        buf->st_blocks = f->csize;
    }
    return 0;
}

int usbd_mtp_statfs(const char *path, struct mtp_statfs *buf)
{
    FATFS *f;
    FRESULT res;
    DWORD fre_clust, fre_sect, tot_sect;

    f = &s_sd_disk;

    res = f_getfree(path, &fre_clust, &f);
    if (res != FR_OK) {
        printf("f_getfree failed, cause: %s\n", show_error_string(res));
        return -1;
    }
    tot_sect = (f->n_fatent - 2) * f->csize;
    fre_sect = fre_clust * f->csize;

    buf->f_blocks = tot_sect;
    buf->f_bfree = fre_sect;
#if FF_MAX_SS != FF_MIN_SS
    buf->f_bsize = f->ssize;
#else
    buf->f_bsize = FF_MIN_SS;
#endif
    return 0;
}

int usbd_mtp_open(const char *path, uint8_t mode)
{
    BYTE flags;

    if (mode == O_RDONLY) {
        flags = FA_READ | FA_OPEN_EXISTING;
    } else if (mode == O_WRONLY) {
        flags = FA_WRITE | FA_OPEN_ALWAYS;
    } else if (mode == O_RDWR) {
        flags = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;
    } else {
        return -1; // Invalid mode
    }

    FRESULT result = f_open(&s_file, path, flags);
    if (result != FR_OK) {
        printf("f_open failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return 0;
}

int usbd_mtp_close(int fd)
{
    FRESULT result = f_close(&s_file);
    if (result != FR_OK) {
        printf("f_close failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return 0;
}

int usbd_mtp_read(int fd, void *buf, size_t len)
{
    UINT bytes_read;
    FRESULT result = f_read(&s_file, buf, len, &bytes_read);
    if (result != FR_OK) {
        printf("f_read failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return bytes_read; // Return number of bytes read
}

int usbd_mtp_write(int fd, const void *buf, size_t len)
{
    UINT bytes_written;
    FRESULT result = f_write(&s_file, buf, len, &bytes_written);
    if (result != FR_OK) {
        printf("f_write failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return bytes_written; // Return number of bytes written
}

int usbd_mtp_unlink(const char *path)
{
    FRESULT result = f_unlink(path);
    if (result != FR_OK) {
        printf("f_unlink failed, cause: %s\n", show_error_string(result));
        return -1;
    }
    return 0; // File deleted successfully
}

void usbd_mtp_mount()
{
    sd_mount_fs();

    // write a file to test the SD card
    sd_write_file();
}
