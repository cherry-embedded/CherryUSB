/********************************** (C) COPYRIGHT *******************************
 * File Name          : CH58x_flash.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description
 *******************************************************************************/

#include "CH58x_common.h"

/* RESET_EN */
#define RESET_Enable  0x00000008
#define RESET_Disable 0xFFFFFFF7

/* LOCKUP_RST_EN */
#define UART_NO_KEY_Enable  0x00000100
#define UART_NO_KEY_Disable 0xFFFFFEFF

/* BOOT_PIN */
#define BOOT_PIN_PB22  0x00000200
#define BOOT_PIN_PB11  0xFFFFFDFF

/* FLASH_WRProt */
#define FLASH_WRProt 0xFFF003FF

/*******************************************************************************
 * Function Name  : FLASH_ROM_READ
 * Description    : Read Flash
 * Input          :
 * Return         : None
 *******************************************************************************/
void FLASH_ROM_READ( UINT32 StartAddr, PVOID Buffer, UINT32 len )
{
  UINT32 i, Length = ( len + 3 ) >> 2;
  PUINT32 pCode = ( PUINT32 ) StartAddr;
  PUINT32 pBuf = ( PUINT32 ) Buffer;

  for ( i = 0; i < Length; i++ )
  {
    *pBuf++ = *pCode++;
  }
}

/*******************************************************************************
* Function Name  : UserOptionByteConfig
* Description    : Configure User Option Byte.���ڵ����û���������Ч��������Ч,��ÿ����¼��ֻ���޸�һ��
*                 (ʹ�øú���������ʹ�ùٷ��ṩ��.S�ļ���ͬʱ���øú����������ϵ�����ߵ��Խӿ�Ĭ�Ϲر�)
* Input          : RESET_EN:
*                     ENABLE-�ⲿ��λ����ʹ��
*                     DISABLE-��ʹ��
*                  BOOT_PIN:
*                     ENABLE-ʹ��Ĭ��boot��-PB22
*                     DISABLE-ʹ��boot��-PB11
*                  UART_NO_KEY_EN�������ⰴ������ʹ��
*                     ENABLE-�����ⰴ������ʹ��
*                     DISABLE-��������
*                  FLASHProt_Size��д������С(��λ4K)
* Return         : 0: Success
*                  1: Err
*******************************************************************************/
UINT8 UserOptionByteConfig( FunctionalState RESET_EN, FunctionalState BOOT_PIN, FunctionalState UART_NO_KEY_EN,
                            UINT32 FLASHProt_Size )
{
  UINT32 s, t;

  FLASH_ROM_READ( 0x14, &s, 4 );

  if ( s == 0xF5F9BDA9 )
  {
    s=0;
    FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, 0x7EFFC, &s, 4 );
    s &= 0xFF;

    if ( RESET_EN == ENABLE )
      s |= RESET_Enable;
    else
      s &= RESET_Disable;

    /* bit[7:0]-bit[31-24] */
    s |= ( ( ~( s << 24 ) ) & 0xFF000000 );    //��8λ ������Ϣȡ����

    if ( BOOT_PIN == ENABLE )
      s |= BOOT_PIN_PB22;
    if ( UART_NO_KEY_EN == ENABLE )
      s |= UART_NO_KEY_Enable;

    /* bit[23-10] */
    s &= 0xFF0003FF;
    s |= ( ( FLASHProt_Size << 10 ) | ( 5 << 20 ) ) & 0x00FFFC00;

    /*Write user option byte*/
    FLASH_ROM_WRITE( 0x14, &s, 4 );

    /* Verify user option byte */
    FLASH_ROM_READ( 0x14, &t, 4 );

    if ( s == t )
      return 0;
    else
      return 1;
  }

  return 1;
}


/*******************************************************************************
* Function Name  : UserOptionByteClose_SWD
* Description    : �����ߵ��Խӿڣ���������ֵ���ֲ���.���ڵ����û���������Ч��������Ч,��ÿ����¼��ֻ���޸�һ��
*                  (ʹ�øú���������ʹ�ùٷ��ṩ��.S�ļ���ͬʱ���øú����������ϵ�����ߵ��Խӿ�Ĭ�Ϲر�)
* Input          : None
* Return         : 0: Success
*                  1: Err
 *******************************************************************************/
UINT8 UserOptionByteClose_SWD( void )
{
  UINT32 s, t;

  FLASH_ROM_READ( 0x14, &s, 4 );

  if ( s == 0xF5F9BDA9 )
  {
    FLASH_EEPROM_CMD( CMD_GET_ROM_INFO, 0x7EFFC, &s, 4 );

    s &= ~( ( 1 << 4 ) | ( 1 << 7 ) );    //���õ��Թ��ܣ� ����SPI��дFLASH

    /* bit[7:0]-bit[31-24] */
    s &= 0x00FFFFFF;
    s |= ( ( ~( s << 24 ) ) & 0xFF000000 );    //��8λ ������Ϣȡ����

    /*Write user option byte*/
    FLASH_ROM_WRITE( 0x14, &s, 4 );

    /* Verify user option byte */
    FLASH_ROM_READ( 0x14, &t, 4 );

    if ( s == t )
      return 0;
    else
      return 1;
  }

  return 1;
}

/*******************************************************************************
* Function Name  : UserOptionByte_Active
* Description    : �û���������Ч������ִ�к��Զ���λ
* Input          : None
* Return         : 0: Success
*                  1: Err
 *******************************************************************************/
void UserOptionByte_Active( void )
{
  FLASH_ROM_SW_RESET();
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R16_INT32K_TUNE = 0xFFFF;
  R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
  R8_SAFE_ACCESS_SIG = 0;
  while(1);
}
