/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_gpio.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : GPIOA_ModeCfg
* Description    : GPIOA�˿�����ģʽ����
* Input          : pin:  PA0-PA15
					GPIO_Pin_0 - GPIO_Pin_15
				   mode:
					GPIO_ModeIN_Floating  -  ��������
					GPIO_ModeIN_PU        -  ��������
					GPIO_ModeIN_PD        -  ��������
					GPIO_ModeOut_PP_5mA   -  ����������5mA
					GPIO_ModeOut_PP_20mA  -  ����������20mA				   				
* Return         : None
*******************************************************************************/
void GPIOA_ModeCfg( UINT32 pin, GPIOModeTypeDef mode )
{	
    switch(mode)
    {
        case GPIO_ModeIN_Floating:
            R32_PA_PD_DRV &= ~pin;
            R32_PA_PU     &= ~pin;
            R32_PA_DIR    &= ~pin;
            break;

        case GPIO_ModeIN_PU:
            R32_PA_PD_DRV &= ~pin;
            R32_PA_PU     |= pin;
            R32_PA_DIR    &= ~pin;
            break;

        case GPIO_ModeIN_PD:
            R32_PA_PD_DRV |= pin;
            R32_PA_PU     &= ~pin;
            R32_PA_DIR    &= ~pin;
            break;

        case GPIO_ModeOut_PP_5mA:
            R32_PA_PD_DRV &= ~pin;
            R32_PA_DIR    |= pin;
            break;

        case GPIO_ModeOut_PP_20mA:
            R32_PA_PD_DRV |= pin;
            R32_PA_DIR    |= pin;
            break;

        default:
            break;
    }
}

/*******************************************************************************
* Function Name  : GPIOB_ModeCfg
* Description    : GPIOB�˿�����ģʽ����
* Input          : pin:  PB0-PB23
					GPIO_Pin_0 - GPIO_Pin_23
				   mode:
					GPIO_ModeIN_Floating  -  ��������
					GPIO_ModeIN_PU        -  ��������
					GPIO_ModeIN_PD        -  ��������
					GPIO_ModeOut_PP_5mA   -  ����������5mA
					GPIO_ModeOut_PP_20mA  -  ����������20mA				   				
* Return         : None
*******************************************************************************/
void GPIOB_ModeCfg( UINT32 pin, GPIOModeTypeDef mode )
{	
    switch(mode)
    {
        case GPIO_ModeIN_Floating:
            R32_PB_PD_DRV &= ~pin;
            R32_PB_PU     &= ~pin;
            R32_PB_DIR    &= ~pin;
            break;

        case GPIO_ModeIN_PU:
            R32_PB_PD_DRV &= ~pin;
            R32_PB_PU     |= pin;
            R32_PB_DIR    &= ~pin;
            break;

        case GPIO_ModeIN_PD:
            R32_PB_PD_DRV |= pin;
            R32_PB_PU     &= ~pin;
            R32_PB_DIR    &= ~pin;
            break;

        case GPIO_ModeOut_PP_5mA:
            R32_PB_PD_DRV &= ~pin;
            R32_PB_DIR    |= pin;
            break;

        case GPIO_ModeOut_PP_20mA:
            R32_PB_PD_DRV |= pin;
            R32_PB_DIR    |= pin;
            break;

        default:
            break;
    }
}

/*******************************************************************************
* Function Name  : GPIOA_ITModeCfg
* Description    : GPIOA�����ж�ģʽ����
* Input          : pin:  PA0-PA15
					GPIO_Pin_0 - GPIO_Pin_15
				   mode:
					GPIO_ITMode_LowLevel   -  �͵�ƽ����
					GPIO_ITMode_HighLevel  -  �ߵ�ƽ����
					GPIO_ITMode_FallEdge   -  �½��ش���
					GPIO_ITMode_RiseEdge   -  �����ش���				   				
* Return         : None
*******************************************************************************/
void GPIOA_ITModeCfg( UINT32 pin, GPIOITModeTpDef mode )
{
    switch( mode )
    {
        case GPIO_ITMode_LowLevel:		// �͵�ƽ����
            R16_PA_INT_MODE &= ~pin;
            R32_PA_CLR |= pin;
            break;

        case GPIO_ITMode_HighLevel:		// �ߵ�ƽ����
            R16_PA_INT_MODE &= ~pin;
            R32_PA_OUT |= pin;
            break;

        case GPIO_ITMode_FallEdge:		// �½��ش���
            R16_PA_INT_MODE |= pin;
            R32_PA_CLR |= pin;
            break;

        case GPIO_ITMode_RiseEdge:		// �����ش���	
            R16_PA_INT_MODE |= pin;
            R32_PA_OUT |= pin;
            break;

        default :
            break;
    }
    R16_PA_INT_IF = pin;
    R16_PA_INT_EN |= pin;
}

/*******************************************************************************
* Function Name  : GPIOB_ITModeCfg
* Description    : GPIOB�����ж�ģʽ����
* Input          : pin:  PB0-PB23
					GPIO_Pin_0 - GPIO_Pin_23
				   mode:
					GPIO_ITMode_LowLevel   -  �͵�ƽ����
					GPIO_ITMode_HighLevel  -  �ߵ�ƽ����
					GPIO_ITMode_FallEdge   -  �½��ش���
					GPIO_ITMode_RiseEdge   -  �����ش���				   				
* Return         : None
*******************************************************************************/
void GPIOB_ITModeCfg( UINT32 pin, GPIOITModeTpDef mode )
{
    UINT32 Pin = pin|((pin&(GPIO_Pin_22|GPIO_Pin_23))>>14);
    switch( mode )
    {
        case GPIO_ITMode_LowLevel:		// �͵�ƽ����
            R16_PB_INT_MODE &= ~Pin;
            R32_PB_CLR |= pin;
            break;

        case GPIO_ITMode_HighLevel:		// �ߵ�ƽ����
            R16_PB_INT_MODE &= ~Pin;
            R32_PB_OUT |= pin;
            break;

        case GPIO_ITMode_FallEdge:		// �½��ش���
            R16_PB_INT_MODE |= Pin;
            R32_PB_CLR |= pin;
            break;

        case GPIO_ITMode_RiseEdge:		// �����ش���	
            R16_PB_INT_MODE |= Pin;
            R32_PB_OUT |= pin;
            break;

        default :
            break;
    }
    R16_PB_INT_IF = Pin;
    R16_PB_INT_EN |= Pin;
}


/*******************************************************************************
* Function Name  : GPIOPinRemap
* Description    : ���蹦������ӳ��
* Input          : s:  
					ENABLE  - ����ӳ��    
					DISABLE - Ĭ������
				   perph:
					RB_PIN_SPI0	  -  SPI0:  PA12/PA13/PA14/PA15 -> PB12/PB13/PB14/PB15
					RB_PIN_UART1  -  UART1: PA8/PA9 ->  PB12/PB13
					RB_PIN_UART0  -  UART0: PB4/PB7 ->  PA15/PA14
					RB_PIN_TMR2	  -  TMR2:  PA11 ->  PB11
					RB_PIN_TMR1	  -  TMR1:  PA10 ->  PB10
					RB_PIN_TMR0	  -  TMR0:  PA9 ->  PB23
* Return         : None
*******************************************************************************/
void GPIOPinRemap( FunctionalState s, UINT16 perph )
{
    if( s )     R16_PIN_ALTERNATE |= perph;				
    else        R16_PIN_ALTERNATE &= ~perph;				
}

/*******************************************************************************
* Function Name  : GPIOAGPPCfg
* Description    : ģ������GPIO���Ź��ܿ���
* Input          : s: 
					ENABLE  - ��ģ�����蹦�ܣ��ر����ֹ���     
					DISABLE - �������ֹ��ܣ��ر�ģ�����蹦�� 
				   perph:
					RB_PIN_ADC0_1_IE	  -  ADC0-1ͨ��
					RB_PIN_ADC2_3_IE	  -  ADC2-3ͨ��
					RB_PIN_ADC4_5_IE	  -  ADC4-5ͨ�� 					
					RB_PIN_ADC6_7_IE	  -  ADC6-7ͨ��
					RB_PIN_ADC8_9_IE	  -  ADC8-9ͨ��
					RB_PIN_ADC10_11_IE	  -  ADC10-11ͨ��
					RB_PIN_ADC12_13_IE	  -  ADC12-13ͨ��
					RB_PIN_XT32K_IE	      -  �ⲿ32K����
					RB_PIN_USB_IE		  -  USB�����ź�����
					RB_PIN_ETH_IE		  -  ��̫�������ź�����
					RB_PIN_SEG0_3_IE	  -  LCD������SEG0-3��������
					RB_PIN_SEG4_7_IE	  -  LCD������SEG4-7��������
					RB_PIN_SEG8_11_IE	  -  LCD������SEG8-11��������
					RB_PIN_SEG12_15_IE	  -  LCD������SEG12-15��������
					RB_PIN_SEG16_19_IE	  -  LCD������SEG16-19��������
					RB_PIN_SEG20_23_IE	  -  LCD������SEG20-23��������	
* Return         : None
*******************************************************************************/
void GPIOAGPPCfg( FunctionalState s, UINT16 perph )
{
    if( s )     R16_PIN_ANALOG_IE |= perph;
    else        R16_PIN_ANALOG_IE &= ~perph;
}
