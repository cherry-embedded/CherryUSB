/* CH583 Flash-ROM & Data-Flash  */
/* Website:  http://wch.cn       */
/* Email:    tech@wch.cn         */
/* Author:   W.ch 2020.06        */
/* V1.0 FlashROM library for USER/BOOT */
/* for the target in USER code area on the chip divided into USER code area and BOOT area */
/* 用于具有用户代码区和引导区的芯片、操作目标为用户代码区的情况，
   可以在用户代码中被调用（IAP，擦写自身），也可以在引导代码中被调用（更新用户代码） */

/* Flash-ROM feature:
     for store program code, support block erasing, dword and page writing, dword verifying, unit for Length is byte,
     minimal quantity for write or verify is one dword (4-bytes),
     256 bytes/page for writing, FLASH_ROM_WRITE support one dword or more dword writing, but multiple of 256 is the best,
     4KB (4096 bytes) bytes/block for erasing, so multiple of 4096 is the best */

/* Data-Flash(EEPROM) feature:
     for store data, support block erasing, byte and page writing, byte reading,
     minimal quantity for write or read is one byte,
     256 bytes/page for writing, EEPROM_WRITE support one byte or more byte writing, but multiple of 256 is the best,
     0.25KB/4KB (256/4096 bytes) bytes/block for erasing, so multiple of 256 or 4096 is the best */


#ifndef EEPROM_PAGE_SIZE
#define EEPROM_PAGE_SIZE    256                       // Flash-ROM & Data-Flash page size for writing
#define EEPROM_BLOCK_SIZE   4096                      // Flash-ROM & Data-Flash block size for erasing
#define EEPROM_MIN_ER_SIZE  EEPROM_PAGE_SIZE          // Data-Flash minimal size for erasing
//#define EEPROM_MIN_ER_SIZE  EEPROM_BLOCK_SIZE         // Flash-ROM  minimal size for erasing
#define EEPROM_MIN_WR_SIZE  1                         // Data-Flash minimal size for writing
#define EEPROM_MAX_SIZE     0x8000                    // Data-Flash maximum size, 32KB
#endif
#ifndef FLASH_MIN_WR_SIZE
#define FLASH_MIN_WR_SIZE   4                         // Flash-ROM minimal size for writing
#endif
#ifndef FLASH_ROM_MAX_SIZE
#define FLASH_ROM_MAX_SIZE  0x070000                  // Flash-ROM maximum program size, 448KB
#endif

#ifndef CMD_FLASH_ROM_SW_RESET
// CMD_* for caller from FlashROM or RAM, auto execute CMD_FLASH_ROM_SW_RESET before command

#define CMD_FLASH_ROM_START_IO	0x00		// start FlashROM I/O, without parameter
#define CMD_FLASH_ROM_SW_RESET	0x04		// software reset FlashROM, without parameter
#define CMD_GET_ROM_INFO		0x06		// get information from FlashROM, parameter @Address,Buffer
#define CMD_GET_UNIQUE_ID		0x07		// get 64 bit unique ID, parameter @Buffer
#define CMD_FLASH_ROM_PWR_DOWN	0x0D		// power-down FlashROM, without parameter
#define CMD_FLASH_ROM_PWR_UP	0x0C		// power-up FlashROM, without parameter
#define CMD_FLASH_ROM_LOCK		0x08		// lock(protect)/unlock FlashROM data block, return 0 if success, parameter @StartAddr
// StartAddr: 0=unlock all, 1=lock boot code, 3=lock all code and data

#define CMD_EEPROM_ERASE		0x09		// erase Data-Flash block, return 0 if success, parameter @StartAddr,Length
#define CMD_EEPROM_WRITE		0x0A		// write Data-Flash data block, return 0 if success, parameter @StartAddr,Buffer,Length
#define CMD_EEPROM_READ			0x0B		// read Data-Flash data block, parameter @StartAddr,Buffer,Length
#define CMD_FLASH_ROM_ERASE		0x01		// erase FlashROM block, return 0 if success, parameter @StartAddr,Length
#define CMD_FLASH_ROM_WRITE		0x02		// write FlashROM data block, minimal block is dword, return 0 if success, parameter @StartAddr,Buffer,Length
#define CMD_FLASH_ROM_VERIFY	0x03		// read FlashROM data block, minimal block is dword, return 0 if success, parameter @StartAddr,Buffer,Length
#endif

#define ROM_CFG_MAC_ADDR	0x7F018			// address for MAC address information
#define ROM_CFG_BOOT_INFO	0x7DFF8			// address for BOOT information

extern UINT32 FLASH_EEPROM_CMD( UINT8 cmd, UINT32 StartAddr, PVOID Buffer, UINT32 Length );  // execute Flash/EEPROM command, caller from FlashROM or RAM


#define FLASH_ROM_START_IO( )						FLASH_EEPROM_CMD( CMD_FLASH_ROM_START_IO, 0, NULL, 0 )  // start FlashROM I/O

#define FLASH_ROM_SW_RESET( )						FLASH_EEPROM_CMD( CMD_FLASH_ROM_SW_RESET, 0, NULL, 0 )  // software reset FlashROM

#define GetMACAddress(Buffer)						FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, ROM_CFG_MAC_ADDR, Buffer, 0 )  // get 6 bytes MAC address

#define GET_BOOT_INFO(Buffer)						FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, ROM_CFG_BOOT_INFO, Buffer, 0 )  // get 8 bytes BOOT information

#define GET_UNIQUE_ID(Buffer)						FLASH_EEPROM_CMD( CMD_GET_UNIQUE_ID, 0, Buffer, 0 )  // get 64 bit unique ID

#define FLASH_ROM_PWR_DOWN( )						FLASH_EEPROM_CMD( CMD_FLASH_ROM_PWR_DOWN, 0, NULL, 0 )  // power-down FlashROM

#define FLASH_ROM_PWR_UP( )							FLASH_EEPROM_CMD( CMD_FLASH_ROM_PWR_UP, 0, NULL, 0 )  // power-up FlashROM

#define EEPROM_READ(StartAddr,Buffer,Length)		FLASH_EEPROM_CMD( CMD_EEPROM_READ, StartAddr, Buffer, Length )  // read Data-Flash data block

#define EEPROM_ERASE(StartAddr,Length)				FLASH_EEPROM_CMD( CMD_EEPROM_ERASE, StartAddr, NULL, Length )  // erase Data-Flash block, return 0 if success

#define EEPROM_WRITE(StartAddr,Buffer,Length)		FLASH_EEPROM_CMD( CMD_EEPROM_WRITE, StartAddr, Buffer, Length )  // write Data-Flash data block, return 0 if success

#define FLASH_ROM_ERASE(StartAddr,Length)			FLASH_EEPROM_CMD( CMD_FLASH_ROM_ERASE, StartAddr, NULL, Length )  // erase FlashROM block, return 0 if success

#define FLASH_ROM_WRITE(StartAddr,Buffer,Length)	FLASH_EEPROM_CMD( CMD_FLASH_ROM_WRITE, StartAddr, Buffer, Length )  // write FlashROM data block, minimal block is dword, return 0 if success

#define FLASH_ROM_VERIFY(StartAddr,Buffer,Length)	FLASH_EEPROM_CMD( CMD_FLASH_ROM_VERIFY, StartAddr, Buffer, Length )  // verify FlashROM data block, minimal block is dword, return 0 if success

#define FLASH_ROM_LOCK(LockFlag)					FLASH_EEPROM_CMD( CMD_FLASH_ROM_LOCK, LockFlag, NULL, 0 )  // lock(protect)/unlock FlashROM data block, return 0 if success
/* LockFlag: 0=unlock all, 1=lock inform area (default status), 2=lock boot code, 3=lock all code and data */
