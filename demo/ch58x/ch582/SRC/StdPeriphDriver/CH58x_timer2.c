/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_timer2.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : TMR2_TimerInit
* Description    : ��ʱ���ܳ�ʼ��
* Input          : t: ��ʱʱ�䣬���ڵ�ǰϵͳʱ��Tsys, ���ʱ���� 67108864
					
* Return         : None
*******************************************************************************/
void TMR2_TimerInit( UINT32 t )
{	
    R32_TMR2_CNT_END = t;
    R8_TMR2_CTRL_MOD = RB_TMR_ALL_CLEAR;	
    R8_TMR2_CTRL_MOD = RB_TMR_COUNT_EN;
}

/*******************************************************************************
* Function Name  : TMR2_EXTSingleCounterInit
* Description    : ���ؼ������ܳ�ʼ��
* Input          : cap: �ɼ���������
                    CAP_NULL - ������
                    Edge_To_Edge - �����������
                    FallEdge_To_FallEdge - �����½���
					RiseEdge_To_RiseEdge - ����������
* Return         : None
*******************************************************************************/
void TMR2_EXTSingleCounterInit( CapModeTypeDef cap )
{
    R8_TMR2_CTRL_MOD = RB_TMR_ALL_CLEAR;
    R8_TMR2_CTRL_MOD = RB_TMR_COUNT_EN      \
                      |RB_TMR_CAP_COUNT     \
                      |RB_TMR_MODE_IN       \
                      |(cap<<6);    
}

/*******************************************************************************
* Function Name  : TMR2_PWMInit
* Description    : PWM �����ʼ��
* Input          : pr:  select wave polar 	
					refer to PWMX_PolarTypeDef	
				   ts:	set pwm repeat times
					refer to PWM_RepeatTsTypeDef					
* Return         : None
*******************************************************************************/
void TMR2_PWMInit( PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts )
{
//    R8_TMR2_CTRL_MOD = RB_TMR_ALL_CLEAR;
    R8_TMR2_CTRL_MOD = RB_TMR_COUNT_EN      \
                        |RB_TMR_OUT_EN      \
                        |(pr<<4)            \
                        |(ts<<6);
}


/*******************************************************************************
* Function Name  : TMR2_CapInit
* Description    : �ⲿ�źŲ�׽���ܳ�ʼ��
* Input          : cap:  select capture mode 	
					refer to CapModeTypeDef						
* Return         : None
*******************************************************************************/
void TMR2_CapInit( CapModeTypeDef cap )
{
        R8_TMR2_CTRL_MOD = RB_TMR_ALL_CLEAR;
        R8_TMR2_CTRL_MOD = RB_TMR_COUNT_EN      \
                            |RB_TMR_MODE_IN     \
                            |(cap<<6);	
}

/*******************************************************************************
* Function Name  : TMR2_DMACfg
* Description    : ����DMA����
* Input          : s:  
                    ENABLE  - ��   
                    DISABLE - �ر�	
                   startAddr�� DMA ��ʼ��ַ
                   endAddr�� DMA������ַ
                   m������DMAģʽ
* Return         : None
*******************************************************************************/
void TMR2_DMACfg( UINT8 s, UINT16 startAddr, UINT16 endAddr, DMAModeTypeDef m )
{
        if(s == DISABLE){
            R8_TMR2_CTRL_DMA = 0;
        }
        else{            
            R16_TMR2_DMA_BEG = startAddr;
            R16_TMR2_DMA_END = endAddr;
            if(m)   R8_TMR2_CTRL_DMA = RB_TMR_DMA_LOOP|RB_TMR_DMA_ENABLE;
            else    R8_TMR2_CTRL_DMA = RB_TMR_DMA_ENABLE;
        }
}




