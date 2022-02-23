#include "diskio.h"
#include "string.h"
#include "hal_flash.h"

#define FLASH_START_ADDR  0x00040000 /*addr start from 256k */
#define FLASH_BLOCK_SIZE  4096
#define FLASH_BLOCK_COUNT 64

extern const char *FR_Table[];

int USB_disk_status(void)
{
    return 0;
}
int USB_disk_initialize(void)
{
    return RES_OK;
}
int USB_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
    flash_read(FLASH_START_ADDR + sector * FLASH_BLOCK_SIZE, (uint8_t *)buff, count * FLASH_BLOCK_SIZE);
    return 0;
}
int USB_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
    flash_erase(FLASH_START_ADDR + sector * FLASH_BLOCK_SIZE, 4096);
    flash_write(FLASH_START_ADDR + sector * FLASH_BLOCK_SIZE, (uint8_t *)buff, count * FLASH_BLOCK_SIZE);
    return 0;
}
int USB_disk_ioctl(BYTE cmd, void *buff)
{
    int result = 0;

    switch (cmd) {
        case CTRL_SYNC:
            result = RES_OK;
            break;

        case GET_SECTOR_SIZE:
            *(WORD *)buff = FLASH_BLOCK_SIZE;
            result = RES_OK;
            break;

        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;
            result = RES_OK;
            break;

        case GET_SECTOR_COUNT:
            *(DWORD *)buff = FLASH_BLOCK_COUNT;
            result = RES_OK;
            break;

        default:
            result = RES_PARERR;
            break;
    }

    return result;
}