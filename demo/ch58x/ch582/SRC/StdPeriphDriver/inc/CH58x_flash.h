


#ifndef __CH58x_FLASH_H__
#define __CH58x_FLASH_H__

#ifdef __cplusplus
 extern "C" {
#endif
     
void FLASH_ROM_READ( UINT32 StartAddr, PVOID Buffer, UINT32 len );           /* ∂¡»°Flash-ROM */
UINT8 UserOptionByteConfig( FunctionalState RESET_EN, FunctionalState BOOT_PIN, FunctionalState UART_NO_KEY_EN,
                            UINT32 FLASHProt_Size );
UINT8 UserOptionByteClose_SWD( void );
void UserOptionByte_Active( void );


#ifdef __cplusplus
}
#endif

#endif  // __CH58x_FLASH_H__

