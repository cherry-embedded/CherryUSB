/*
 * Copyright (c) 2024 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "ff.h"
#include "diskio.h"
#include "usbd_core.h"
#include "usb_vfs.h"

FATFS s_sd_disk;
FIL s_file;
BYTE work[FF_MAX_SS];

#define DEV_SD 0

const TCHAR driver_num_buf[4] = { DEV_SD + '0', ':', '/', '\0' };

static const char *show_error_string(FRESULT fresult);

static FRESULT sd_mount_fs(void)
{
    FRESULT fresult = f_mount(&s_sd_disk, driver_num_buf, 1);
    if (fresult == FR_OK) {
        printf("SD card has been mounted successfully\n");
    } else {
        printf("Failed to mount SD card, cause: %s\n", show_error_string(fresult));
    }

    fresult = f_chdrive(driver_num_buf);
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
    FRESULT fresult = f_open(&s_file, "readme.txt", FA_WRITE | FA_CREATE_ALWAYS);
    if (fresult != FR_OK) {
        printf("Create new file failed, cause: %s\n", show_error_string(fresult));
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

static const char *show_error_string(FRESULT fresult)
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

const char *vfs_get_fs_root_path(void)
{
    return "/";
}

const char *vfs_get_fs_description(void)
{
    return "CherryUSB VFS FATFS";
}

/* -------------------------------------------------------------------------
 * helpers
 * ---------------------------------------------------------------------- */

/** FatFs result -> VFS_ERR_*, so the host gets a readable reason. */
static int adb_err(FRESULT fr)
{
    switch (fr) {
        case FR_OK:
            return 0;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return VFS_ERR_NOENT;
        case FR_EXIST:
            return VFS_ERR_EXIST;
        case FR_INVALID_NAME:
            return VFS_ERR_INVAL;
        case FR_INVALID_PARAMETER:
            return VFS_ERR_INVAL;
        case FR_INVALID_OBJECT:
            return VFS_ERR_INVAL;
        case FR_WRITE_PROTECTED:
            return VFS_ERR_ROFS;
        case FR_DENIED:
            /* "access denied", which FatFs also uses for a full directory */
            return VFS_ERR_ROFS;
        case FR_INVALID_DRIVE:
        case FR_NOT_ENABLED:
        case FR_NO_FILESYSTEM:
        case FR_NOT_READY:
            return VFS_ERR_NODEV;
        case FR_TIMEOUT:
        case FR_LOCKED:
            return VFS_ERR_AGAIN;
        case FR_NOT_ENOUGH_CORE:
        case FR_TOO_MANY_OPEN_FILES:
            return VFS_ERR_NOMEM;
        case FR_DISK_ERR:
        case FR_INT_ERR:
        default:
            return VFS_ERR_IO;
    }
}

/**
 * days since 1970-01-01, proleptic gregorian (Howard Hinnant's algorithm).
 * Kept here rather than pulling in <time.h>, which an RTOS may not have.
 */
static int days_from_civil(int y, unsigned m, unsigned d)
{
    int era;
    unsigned yoe, doy, doe;

    y -= (m <= 2u) ? 1 : 0;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (m + (m > 2u ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;

    return era * 146097 + (int)doe - 719468;
}

/**
 * FatFs date/time -> unix seconds.
 *
 * adb sync carries the file time in STAT and DENT records, but FatFs keeps a
 * packed DOS timestamp, so it has to be converted somewhere. Doing it here
 * keeps the protocol layer free of file system specifics.
 */
static uint32_t fat_time_to_unix(uint16_t fdate, uint16_t ftime)
{
    unsigned year = 1980u + ((fdate >> 9) & 0x7Fu);
    unsigned month = (fdate >> 5) & 0x0Fu;
    unsigned day = fdate & 0x1Fu;
    unsigned hour = ((unsigned)ftime >> 11) & 0x1Fu;
    unsigned minute = ((unsigned)ftime >> 5) & 0x3Fu;
    unsigned second = ((unsigned)ftime & 0x1Fu) * 2u;

    if ((fdate == 0) || (month == 0) || (day == 0)) {
        return 0; /* FatFs stores 0 when there is no valid timestamp */
    }

    return (uint32_t)(days_from_civil((int)year, month, day) * 86400 +
                      (int)(hour * 3600u + minute * 60u + second));
}

int vfs_mkdir(const char *path)
{
    FRESULT result;

    if (!path || !path[0]) {
        return VFS_ERR_INVAL;
    }

    result = f_mkdir(path);
    /* adb push of a directory tree re-sends SEND for directories it has already
     * created, so "it is already there" has to count as success */
    if ((result != FR_OK) && (result != FR_EXIST)) {
        return adb_err(result);
    }

    return 0;
}

int vfs_rmdir(const char *path)
{
    FRESULT result = f_rmdir(path);

    if (result != FR_OK) {
        return adb_err(result);
    }

    return 0;
}

VFS_DIR *vfs_opendir(const char *name)
{
    FRESULT result;
    DIR *dir;

    if (!name || !name[0]) {
        return NULL;
    }

    dir = usb_osal_malloc(sizeof(DIR));
    if (!dir) {
        return NULL;
    }

    result = f_opendir(dir, name);
    if (result != FR_OK) {
        usb_osal_free(dir);
        return NULL;
    }

    return (VFS_DIR *)dir;
}

int vfs_closedir(VFS_DIR *dir)
{
    FRESULT result;

    if (!dir) {
        return VFS_ERR_INVAL;
    }

    result = f_closedir((DIR *)dir);
    usb_osal_free(dir); /* release it either way */

    if (result != FR_OK) {
        return adb_err(result);
    }

    return 0;
}

struct vfs_dirent *vfs_readdir(VFS_DIR *dir)
{
    static struct vfs_dirent dirent;
    FILINFO fno;
    FRESULT result;

    if (!dir) {
        return NULL;
    }

    result = f_readdir((DIR *)dir, &fno);
    if ((result != FR_OK) || (fno.fname[0] == 0)) {
        return NULL; /* end of directory, or a hard error */
    }

    memset(&dirent, 0, sizeof(dirent));
    strncpy(dirent.d_name, fno.fname, sizeof(dirent.d_name) - 1);
    dirent.d_name[sizeof(dirent.d_name) - 1] = '\0';
    dirent.d_namlen = (uint8_t)strlen(dirent.d_name);
    /* d_type is just the type field of a mode shifted down, see vfs_posix.h */
    dirent.d_type = (uint8_t)(((fno.fattrib & AM_DIR) ? VFS_S_IFDIR
                                                      : VFS_S_IFREG) >>
                              12);

    return &dirent;
}

#undef SS
#if FF_MAX_SS == FF_MIN_SS
#define SS(fs) ((UINT)FF_MAX_SS) /* Fixed sector size */
#else
#define SS(fs) ((fs)->ssize) /* Variable sector size */
#endif

int vfs_stat(const char *path, struct vfs_stat *buf)
{
    FILINFO file_info;
    FRESULT result;
    FATFS *f;

    if (!path || !path[0] || !buf) {
        return VFS_ERR_INVAL;
    }

    f = &s_sd_disk;

    result = f_stat(path, &file_info);
    if (result != FR_OK) {
        /* Stay quiet on purpose: adb stats a path precisely to find out whether
         * it exists, so "not found" is a normal answer and not an error. The
         * service reports it to the host as "mode 0". */
        return adb_err(result);
    }

    memset(buf, 0, sizeof(*buf));
    buf->st_mode = VFS_S_IFREG | VFS_S_IRUSR | VFS_S_IRGRP | VFS_S_IROTH |
                   VFS_S_IWUSR | VFS_S_IWGRP | VFS_S_IWOTH;
    if (file_info.fattrib & AM_DIR) {
        buf->st_mode &= ~VFS_S_IFREG;
        buf->st_mode |= VFS_S_IFDIR | VFS_S_IXUSR | VFS_S_IXGRP | VFS_S_IXOTH;
    }
    if (file_info.fattrib & AM_RDO) {
        buf->st_mode &= ~(VFS_S_IWUSR | VFS_S_IWGRP | VFS_S_IWOTH);
    }

    buf->st_dev = 0;
    buf->st_nlink = 1;
    buf->st_size = file_info.fsize;
    buf->st_blksize = f->csize * SS(f);
    if (file_info.fattrib & AM_ARC) {
        buf->st_blocks = file_info.fsize ? ((file_info.fsize - 1) / SS(f) / f->csize + 1) : 0;
        buf->st_blocks *= (buf->st_blksize / 512); /* st_blocks is in 512B units */
    } else {
        buf->st_blocks = f->csize;
    }
    buf->st_mtime = fat_time_to_unix(file_info.fdate, file_info.ftime);
    return 0;
}

int vfs_statfs(const char *path, struct vfs_statfs *buf)
{
    FATFS *f;
    FRESULT res;
    DWORD fre_clust, fre_sect, tot_sect;

    if (!path || !path[0] || !buf) {
        return VFS_ERR_INVAL;
    }

    f = &s_sd_disk;

    res = f_getfree(path, &fre_clust, &f);
    if (res != FR_OK) {
        return adb_err(res);
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

int vfs_open(const char *path, uint8_t mode)
{
    FRESULT result;
    BYTE flags;

    if (!path || !path[0]) {
        return VFS_ERR_INVAL;
    }

    if (mode == VFS_O_RDONLY) {
        flags = FA_READ | FA_OPEN_EXISTING;
    } else if (mode == VFS_O_WRONLY) {
        flags = FA_WRITE | FA_CREATE_ALWAYS;
    } else if (mode == VFS_O_RDWR) {
        flags = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;
    } else {
        return VFS_ERR_INVAL;
    }

    result = f_open(&s_file, path, flags);
    if (result != FR_OK) {
        return adb_err(result);
    }

    return 0;
}

int vfs_close(int fd)
{
    FRESULT result;

    (void)fd;

    result = f_close(&s_file);
    if (result != FR_OK) {
        return adb_err(result);
    }

    return 0;
}

int vfs_read(int fd, void *buf, size_t len)
{
    UINT bytes_read;
    FRESULT result;

    (void)fd;

    result = f_read(&s_file, buf, len, &bytes_read);
    if (result != FR_OK) {
        return adb_err(result);
    }

    return (int)bytes_read;
}

int vfs_write(int fd, const void *buf, size_t len)
{
    UINT bytes_written;
    FRESULT result;

    (void)fd;

    result = f_write(&s_file, buf, len, &bytes_written);
    if (result != FR_OK) {
        return adb_err(result);
    }

    return (int)bytes_written;
}

int vfs_unlink(const char *path)
{
    FRESULT result = f_unlink(path);

    if (result != FR_OK) {
        return adb_err(result);
    }

    return 0;
}

void vfs_mount(void)
{
    //sd_mkfs();

    sd_mount_fs();

    sd_write_file();
}