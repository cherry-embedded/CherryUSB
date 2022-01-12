/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_SYS.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"

/*******************************************************************************
* Function Name  : SetSysClock
* Description    : ����ϵͳ����ʱ��
* Input          : sc: ϵͳʱ��Դѡ��
          refer to SYS_CLKTypeDef
* Return         : None
*******************************************************************************/
__HIGH_CODE
void SetSysClock( SYS_CLKTypeDef sc)
{
  UINT32 i;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R8_PLL_CONFIG &= ~(1<<5);   //
  R8_SAFE_ACCESS_SIG = 0;
  if ( sc & 0x20 ){    // HSE div
    if ( !( R8_HFCK_PWR_CTRL & RB_CLK_XT32M_PON ) ){
      R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
      R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
      SAFEOPERATE;
      R8_HFCK_PWR_CTRL |= RB_CLK_XT32M_PON;    // HSE power on
      for(i=0;i<1200;i++){  __nop();__nop();  }
    }

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R16_CLK_SYS_CFG = ( 0 << 6 ) | ( sc & 0x1f );
    __nop();__nop();__nop();__nop();
    R8_SAFE_ACCESS_SIG = 0;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_FLASH_CFG = 0X01;
    R8_SAFE_ACCESS_SIG = 0;
  }

  else if ( sc & 0x40 ){    // PLL div
    if ( !( R8_HFCK_PWR_CTRL & RB_CLK_PLL_PON ) ){
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_HFCK_PWR_CTRL |= RB_CLK_PLL_PON;    // PLL power on
        for(i=0;i<2000;i++){  __nop();__nop();  }
    }
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R16_CLK_SYS_CFG = ( 1 << 6 ) | ( sc & 0x1f );
    __nop();__nop();__nop();__nop();
    R8_SAFE_ACCESS_SIG = 0;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_FLASH_CFG = 0X02;
    R8_SAFE_ACCESS_SIG = 0;
  }
  else
  {
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R16_CLK_SYS_CFG |= RB_CLK_SYS_MOD;
  }
  //����FLASH clk����������
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R8_PLL_CONFIG |= 1<<7;
  R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : GetSysClock
* Description    : ��ȡ��ǰϵͳʱ��
* Input          : None
* Return         : Hz
*******************************************************************************/
UINT32 GetSysClock( void )
{
  UINT16  rev;

  rev = R16_CLK_SYS_CFG & 0xff;
  if( (rev & 0x40) == (0<<6) ){       // 32M���з�Ƶ
      return (32000000/(rev&0x1f));
  }
  else if( (rev & RB_CLK_SYS_MOD) == (1<<6) ){    // PLL���з�Ƶ
      return (480000000/(rev&0x1f));
  }
  else {                        // 32K����Ƶ
    return (32000);
  }
}

/*******************************************************************************
* Function Name  : SYS_GetInfoSta
* Description    : ��ȡ��ǰϵͳ��Ϣ״̬
* Input          : i: 
					refer to SYS_InfoStaTypeDef
* Return         : DISABLE  -  �ر�
				   ENABLE   -  ����
*******************************************************************************/
UINT8 SYS_GetInfoSta( SYS_InfoStaTypeDef i )
{
  if (i == STA_SAFEACC_ACT)
    return (R8_SAFE_ACCESS_SIG & RB_SAFE_ACC_ACT);
  else
    return (R8_GLOB_CFG_INFO & (1 << i));
}

/*******************************************************************************
* Function Name  : SYS_ResetExecute
* Description    : ִ��ϵͳ�����λ
* Input          : None
* Return         : None
*******************************************************************************/
void SYS_ResetExecute( void )
{
  FLASH_ROM_SW_RESET();
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
  R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : SYS_DisableAllIrq
* Description    : �ر������жϣ���������ǰ�ж�ֵ
* Input          : pirqv����ǰ�����ж�ֵ
* Return         : None
*******************************************************************************/
void SYS_DisableAllIrq( PUINT32 pirqv )
{
  *pirqv = (PFIC->ISR[0] >> 8) | (PFIC->ISR[1] << 24);
  PFIC->IRER[0] = 0xffffffff;
  PFIC->IRER[1] = 0xffffffff;
}

/*******************************************************************************
 * Function Name  : SYS_RecoverIrq
 * Description    : �ָ�֮ǰ�رյ��ж�ֵ
 * Input          : irq_status����ǰ�����ж�ֵ
 * Return         : None
 *******************************************************************************/
void SYS_RecoverIrq( UINT32 irq_status )
{
  PFIC->IENR[0] = (irq_status << 8);
  PFIC->IENR[1] = (irq_status >> 24);
}

/*******************************************************************************
* Function Name  : SYS_GetSysTickCnt
* Description    : ��ȡ��ǰϵͳ(SYSTICK)����ֵ
* Input          : None
* Return         : ��ǰ����ֵ
*******************************************************************************/
UINT32 SYS_GetSysTickCnt( void )
{
	UINT32 val;

	val = SysTick->CNT;
	return( val );
}

/*******************************************************************************
* Function Name  : WWDG_ITCfg
* Description    : ���Ź���ʱ������ж�ʹ��
* Input          : DISABLE-������ж�      ENABLE-����ж�
* Return         : None
*******************************************************************************/
void  WWDG_ITCfg( FunctionalState s )
{
	R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
	R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
	SAFEOPERATE;
	if(s == DISABLE)		R8_RST_WDOG_CTRL&=~RB_WDOG_INT_EN;
	else 					R8_RST_WDOG_CTRL|=RB_WDOG_INT_EN;
	R8_SAFE_ACCESS_SIG = 0;	
}

/*******************************************************************************
* Function Name  : WWDG_ResetCfg
* Description    : ���Ź���ʱ����λ����
* Input          : DISABLE-�������λ      ENABLE-���ϵͳ��λ
* Return         : None
*******************************************************************************/
void WWDG_ResetCfg( FunctionalState s )
{
	R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
	R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
	SAFEOPERATE;
	if(s == DISABLE)		R8_RST_WDOG_CTRL&=~RB_WDOG_RST_EN;
	else 					R8_RST_WDOG_CTRL|=RB_WDOG_RST_EN;
	R8_SAFE_ACCESS_SIG = 0;	
}

/*******************************************************************************
* Function Name  : WWDG_ClearFlag
* Description    : ������Ź��жϱ�־�����¼��ؼ���ֵҲ�����
* Input          : None
* Return         : None
*******************************************************************************/
void WWDG_ClearFlag( void )
{
	R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
	R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
	SAFEOPERATE;
	R8_RST_WDOG_CTRL |= RB_WDOG_INT_FLAG;
	R8_SAFE_ACCESS_SIG = 0;	
}

/*******************************************************************************
* Function Name  : HardFault_Handler
* Description    : Ӳ�������жϣ������ִ�и�λ����λ����Ϊ�ϵ縴λ
* Input          : None
* Return         : None
*******************************************************************************/
__INTERRUPT
__HIGH_CODE
void HardFault_Handler( void )
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

/*******************************************************************************
* Function Name  : mDelayuS
* Description    : uS ��ʱ
* Input          : t: ʱ�����
* Return         : None
*******************************************************************************/
__HIGH_CODE
void mDelayuS( UINT16 t )
{
    UINT32 i;
#if     (FREQ_SYS == 60000000)
    i = t*15;
#elif (FREQ_SYS == 48000000)
    i = t*12;
#elif (FREQ_SYS == 40000000)
    i = t*10;
#elif	(FREQ_SYS == 32000000)
    i = t<<3;
#elif	(FREQ_SYS == 24000000)
    i = t*6;
#elif	(FREQ_SYS == 16000000)
    i = t<<2;
#elif	(FREQ_SYS == 8000000)
    i = t<<1;
#elif	(FREQ_SYS == 4000000)
    i = t;
#elif	(FREQ_SYS == 2000000)
    i = t>>1;
#elif	(FREQ_SYS == 1000000)
    i = t>>2;
#endif
    do
    {
    	__nop();
    }while(--i);
}

/*******************************************************************************
* Function Name  : mDelaymS
* Description    : mS ��ʱ
* Input          : t: ʱ�����
* Return         : None
*******************************************************************************/
__HIGH_CODE
void mDelaymS( UINT16 t )
{
    UINT16 i;

    for(i=0; i<t; i++)
        mDelayuS(1000);
}


#ifdef DEBUG
int _write(int fd, char *buf, int size)
{
	int i;
	for(i=0; i<size; i++)
	{
#if  DEBUG == Debug_UART0
		while( R8_UART0_TFC == UART_FIFO_SIZE );                        /* �ȴ����ݷ��� */
		R8_UART0_THR = *buf++;                                               /* �������� */
#elif DEBUG == Debug_UART1       
		while( R8_UART1_TFC == UART_FIFO_SIZE );                        /* �ȴ����ݷ��� */
		R8_UART1_THR = *buf++;                                               /* �������� */
#elif DEBUG == Debug_UART2       
		while( R8_UART2_TFC == UART_FIFO_SIZE );                        /* �ȴ����ݷ��� */
		R8_UART2_THR = *buf++;                                               /* �������� */
#elif DEBUG == Debug_UART3       
		while( R8_UART3_TFC == UART_FIFO_SIZE );                        /* �ȴ����ݷ��� */
		R8_UART3_THR = *buf++;                                               /* �������� */
#endif
	}
	return size;
}

#endif

