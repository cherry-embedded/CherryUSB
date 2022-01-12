/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_pwr.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : PWR_DCDCCfg
* Description    : �����ڲ�DC/DC��Դ�����ڽ�Լϵͳ����
* Input          : s:  
                    ENABLE  - ��DCDC��Դ
                    DISABLE - �ر�DCDC��Դ   				
* Return         : None
*******************************************************************************/
void PWR_DCDCCfg( FunctionalState s )
{
    if(s == DISABLE)
    {		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_AUX_POWER_ADJ &= ~RB_DCDC_CHARGE;
        R16_POWER_PLAN &= ~(RB_PWR_DCDC_EN|RB_PWR_DCDC_PRE);		// ��· DC/DC 
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG0;		
    }
    else
    {
        __nop();
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_AUX_POWER_ADJ |= RB_DCDC_CHARGE;
        R16_POWER_PLAN |= RB_PWR_DCDC_PRE;
        DelayUs(10);
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R16_POWER_PLAN |= RB_PWR_DCDC_EN;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG0;			
    }
}

/*******************************************************************************
* Function Name  : PWR_UnitModCfg
* Description    : �ɿص�Ԫģ��ĵ�Դ����
* Input          : s:  
                    ENABLE  - ��   
                    DISABLE - �ر�
                   unit:
                    please refer to unit of controllable power supply 				
* Return         : None
*******************************************************************************/
void PWR_UnitModCfg( FunctionalState s, UINT8 unit )
{
    if(s == DISABLE)		//�ر�
    {
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_HFCK_PWR_CTRL &= ~(unit&0x1c);
        R8_CK32K_CONFIG &= ~(unit&0x03);
    }
    else					//��
    {
        __nop();
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_HFCK_PWR_CTRL |= (unit&0x1c);
        R8_CK32K_CONFIG |= (unit&0x03);
    }
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : PWR_PeriphClkCfg
* Description    : ����ʱ�ӿ���λ
* Input          : s:  
                    ENABLE  - ������ʱ��   
                    DISABLE - �ر�����ʱ��
                   perph:
                    please refer to Peripher CLK control bit define						
* Return         : None
*******************************************************************************/
void PWR_PeriphClkCfg( FunctionalState s, UINT16 perph )
{
    if( s == DISABLE )
    {
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R32_SLEEP_CONTROL |= perph;
    }
    else
    {
        __nop();
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R32_SLEEP_CONTROL &= ~perph;
    }
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : PWR_PeriphWakeUpCfg
* Description    : ˯�߻���Դ����
* Input          : s:  
                    ENABLE  - �򿪴�����˯�߻��ѹ���   
                    DISABLE - �رմ�����˯�߻��ѹ���
                   perph:
                    RB_SLP_USB_WAKE	    -  USB Ϊ����Դ
                    RB_SLP_RTC_WAKE	    -  RTC Ϊ����Դ
                    RB_SLP_GPIO_WAKE	  -  GPIO Ϊ����Դ
                    RB_SLP_BAT_WAKE	    -  BAT Ϊ����Դ
                   mode: refer to WakeUP_ModeypeDef
* Return         : None
*******************************************************************************/
void PWR_PeriphWakeUpCfg( FunctionalState s, UINT8 perph, WakeUP_ModeypeDef mode )
{
	UINT8  m;

    if( s == DISABLE )
    {
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_SLP_WAKE_CTRL &= ~perph;		
    }
    else
    {
    	switch( mode )
    	{
    	case Short_Delay:
    		m = 0x01;
    		break;

    	case Long_Delay:
    		m = 0x00;
    		break;

    	default:
    		m = 0x01;
    		break;
    	}
        __nop();
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_SLP_WAKE_CTRL |= RB_WAKE_EV_MODE | perph;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_SLP_POWER_CTRL &= ~(RB_WAKE_DLY_MOD);
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_SLP_POWER_CTRL |= m;
    }
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : PowerMonitor
* Description    : ��Դ���
* Input          : s:  
                    ENABLE  - �򿪴˹���   
                    DISABLE - �رմ˹���
                   vl: refer to VolM_LevelypeDef
* Return         : None
*******************************************************************************/
void PowerMonitor( FunctionalState s , VolM_LevelypeDef vl)
{
    if( s == DISABLE )
    {
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_BAT_DET_CTRL = 0;
        R8_SAFE_ACCESS_SIG = 0; 
    }
    else
    {
    	if( vl & 0x80 )
    	{
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
    		R8_BAT_DET_CFG = vl&0x03;
    		R8_BAT_DET_CTRL = RB_BAT_MON_EN|((vl>>2)&1);
    	}
    	else
    	{
    	  __nop();
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
    		R8_BAT_DET_CFG = vl&0x03;
    		R8_BAT_DET_CTRL = RB_BAT_DET_EN;
    	}
        R8_SAFE_ACCESS_SIG = 0; 
        mDelayuS(1); 	
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
        R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
        SAFEOPERATE;
        R8_BAT_DET_CTRL |= RB_BAT_LOW_IE|RB_BAT_LOWER_IE;
        R8_SAFE_ACCESS_SIG = 0;  
    }   
}

/*******************************************************************************
* Function Name  : LowPower_Idle
* Description    : �͹���-Idleģʽ
* Input          : None
* Return         : None
*******************************************************************************/
__HIGH_CODE
void LowPower_Idle( void )
{
  FLASH_ROM_SW_RESET();
  R8_FLASH_CTRL = 0x04;   //flash�ر�

	PFIC -> SCTLR &= ~(1<<2);				// sleep
	__WFI();
	__nop();__nop();
}

/*******************************************************************************
* Function Name  : LowPower_Halt
* Description    : �͹���-Haltģʽ��
                   �˵͹����е�HSI/5ʱ�����У����Ѻ���Ҫ�û��Լ�����ѡ��ϵͳʱ��Դ
* Input          : None
* Return         : None
*******************************************************************************/
__HIGH_CODE
void LowPower_Halt( void )
{
    UINT8  x32Kpw, x32Mpw;
    
    FLASH_ROM_SW_RESET();
    R8_FLASH_CTRL = 0x04;   //flash�ر�
    x32Kpw = R8_XT32K_TUNE;
    x32Mpw = R8_XT32M_TUNE;
    x32Mpw = (x32Mpw&0xfc)|0x03;            // 150%�����
    if(R16_RTC_CNT_32K>0x3fff){     // ����500ms
        x32Kpw = (x32Kpw&0xfc)|0x01;        // LSE�����������͵������
    }
    
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_BAT_DET_CTRL = 0;                              // �رյ�ѹ���
    R8_XT32K_TUNE = x32Kpw;
    R8_XT32M_TUNE = x32Mpw;
    R8_PLL_CONFIG |= (1<<5);
    R8_SAFE_ACCESS_SIG = 0;
        
    PFIC -> SCTLR |= (1<<2);				//deep sleep
    __WFI();
    __nop();__nop();
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_PLL_CONFIG &= ~(1<<5);
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : LowPower_Sleep
* Description    : �͹���-Sleepģʽ��
                   ע�⵱��ƵΪ80Mʱ��˯�߻����жϲ��ɵ���flash�ڴ��룬���˳��˺���ǰ��Ҫ����30us�ӳ١�
* Input          : rm:
                    RB_PWR_RAM2K	-	2K retention SRAM ����
                    RB_PWR_RAM30K	-	30K main SRAM ����
                    RB_PWR_EXTEND	-	USB �� BLE ��Ԫ�������򹩵�
                    RB_PWR_XROM   - FlashROM ����
                   NULL	-	���ϵ�Ԫ���ϵ�
* Return         : None
*******************************************************************************/
__HIGH_CODE
void LowPower_Sleep( UINT8 rm )
{
    UINT8  x32Kpw, x32Mpw;
    UINT16 DCDCState;
    x32Kpw = R8_XT32K_TUNE;
    x32Mpw = R8_XT32M_TUNE;
    x32Mpw = (x32Mpw&0xfc)|0x03;            // 150%�����
    if(R16_RTC_CNT_32K>0x3fff){     // ����500ms
        x32Kpw = (x32Kpw&0xfc)|0x01;        // LSE�����������͵������
    } 

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_BAT_DET_CTRL = 0;                // �رյ�ѹ���
    R8_XT32K_TUNE = x32Kpw;
    R8_XT32M_TUNE = x32Mpw;
    R8_SAFE_ACCESS_SIG = 0;

    PFIC -> SCTLR |= (1<<2);				//deep sleep

    DCDCState = R16_POWER_PLAN&(RB_PWR_DCDC_EN|RB_PWR_DCDC_PRE);
    DCDCState |= RB_PWR_PLAN_EN|RB_PWR_MUST_0010|RB_PWR_CORE|rm;
    __nop();
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_SLP_POWER_CTRL |= RB_RAM_RET_LV;
    R8_PLL_CONFIG |= (1<<5);
    R16_POWER_PLAN = DCDCState;
    __WFI();
    __nop();__nop();
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_PLL_CONFIG &= ~(1<<5);
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : LowPower_Shutdown
* Description    : �͹���-Shutdownģʽ��
                   �˵͹����е�HSI/5ʱ�����У����Ѻ���Ҫ�û��Լ�����ѡ��ϵͳʱ��Դ
                   ע����ô˺�����DCDC����ǿ�ƹرգ����Ѻ�����ֶ��ٴδ�
* Input          : rm:
                    RB_PWR_RAM2K  - 2K retention SRAM ����
                    RB_PWR_RAM16K - 16K main SRAM ����
                   NULL	-	���ϵ�Ԫ���ϵ�
* Return         : None
*******************************************************************************/
__HIGH_CODE
void LowPower_Shutdown( UINT8 rm )
{	
    UINT8  x32Kpw, x32Mpw;
    
    FLASH_ROM_SW_RESET();
    x32Kpw = R8_XT32K_TUNE;
    x32Mpw = R8_XT32M_TUNE;
    x32Mpw = (x32Mpw&0xfc)|0x03;            // 150%�����
    if(R16_RTC_CNT_32K>0x3fff){     // ����500ms
        x32Kpw = (x32Kpw&0xfc)|0x01;        // LSE�����������͵������
    }

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_BAT_DET_CTRL = 0;                // �رյ�ѹ���
    R8_XT32K_TUNE = x32Kpw;
    R8_XT32M_TUNE = x32Mpw;
    R8_SAFE_ACCESS_SIG = 0;    
    SetSysClock( CLK_SOURCE_HSE_6_4MHz );

    PFIC -> SCTLR |= (1<<2);				//deep sleep

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_SLP_POWER_CTRL |= RB_RAM_RET_LV;
    R16_POWER_PLAN = RB_PWR_PLAN_EN       \
                    |RB_PWR_MUST_0010   \
                    |rm;
    __WFI();
    __nop();__nop();
    FLASH_ROM_SW_RESET();
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_RST_WDOG_CTRL |= RB_SOFTWARE_RESET;
    R8_SAFE_ACCESS_SIG = 0;

}




