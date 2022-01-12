/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_adc.c
* Author             : WCH
* Version            : V1.1
* Date               : 2020/04/01
* Description 
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : ADC_DataCalib_Rough
* Description    : 采样数据粗调,获取偏差值
* Input          : None
* Return         : 偏差值
*******************************************************************************/
signed short ADC_DataCalib_Rough( void )        // 采样数据粗调,获取偏差值
{
    UINT16  i;
    UINT32  sum=0;
    UINT8  ch=0;        // 备份通道
    UINT8   ctrl=0;     // 备份控制寄存器
    
    ch = R8_ADC_CHANNEL;
    ctrl = R8_ADC_CFG;
    R8_ADC_CFG = 0;
    
    ADC_ChannelCfg( 1 );		// ADC校准通道请选择通道1
    R8_ADC_CFG |= RB_ADC_OFS_TEST|RB_ADC_POWER_ON|(2<<4);      // 进入测试模式
    R8_ADC_CONVERT = RB_ADC_START;
    while( R8_ADC_CONVERT & RB_ADC_START );
    for(i=0; i<16; i++)
    {
        R8_ADC_CONVERT = RB_ADC_START;
        while( R8_ADC_CONVERT & RB_ADC_START );
        sum += (~R16_ADC_DATA)&RB_ADC_DATA;
    }    
    sum = (sum+8)>>4;
    R8_ADC_CFG &= ~RB_ADC_OFS_TEST;      // 关闭测试模式
    
    R8_ADC_CHANNEL = ch;
    R8_ADC_CFG = ctrl;
    return (2048 - sum); 
}

/*******************************************************************************
* Function Name  : ADC_ExtSingleChSampInit
* Description    : 外部信号单通道采样初始化
* Input          : sp:
					refer to ADC_SampClkTypeDef
				   ga:
					refer to ADC_SignalPGATypeDef
* Return         : None
*******************************************************************************/
void ADC_ExtSingleChSampInit( ADC_SampClkTypeDef sp, ADC_SignalPGATypeDef ga )
{
	R8_TKEY_CFG &= ~RB_TKEY_PWR_ON;
    R8_ADC_CFG = RB_ADC_POWER_ON			\
                |RB_ADC_BUF_EN				\
                |( sp<<6 )					\
                |( ga<<4 )	;               
}

/*******************************************************************************
* Function Name  : ADC_ExtDiffChSampInit
* Description    : 外部信号差分通道采样初始化
* Input          : sp:
					refer to ADC_SampClkTypeDef
				   ga:
					refer to ADC_SignalPGATypeDef
* Return         : None
*******************************************************************************/
void ADC_ExtDiffChSampInit( ADC_SampClkTypeDef sp, ADC_SignalPGATypeDef ga )
{
	R8_TKEY_CFG &= ~RB_TKEY_PWR_ON;
    R8_ADC_CFG = RB_ADC_POWER_ON			\
                |RB_ADC_DIFF_EN             \
                |( sp<<6 )					\
                |( ga<<4 )	;
}

/*******************************************************************************
* Function Name  : ADC_InterTSSampInit
* Description    : 内置温度传感器采样初始化
* Input          : None
* Return         : None
*******************************************************************************/
void ADC_InterTSSampInit( void )
{
	R8_TKEY_CFG &= ~RB_TKEY_PWR_ON;
    R8_TEM_SENSOR = RB_TEM_SEN_PWR_ON;
    R8_ADC_CHANNEL = CH_INTE_VTEMP;
    R8_ADC_CFG = RB_ADC_POWER_ON			\
                |RB_ADC_DIFF_EN             \
                |( 3<<4 )	;
}

/*******************************************************************************
* Function Name  : ADC_InterBATSampInit
* Description    : 内置电池电压采样初始化
* Input          : None
* Return         : None
*******************************************************************************/
void ADC_InterBATSampInit( void )
{
	R8_TKEY_CFG &= ~RB_TKEY_PWR_ON;
    R8_ADC_CHANNEL = CH_INTE_VBAT;
    R8_ADC_CFG = RB_ADC_POWER_ON			\
                |RB_ADC_BUF_EN				\
                |( 0<<4 )	;       // 使用-12dB模式，
}


/*******************************************************************************
* Function Name  : TouchKey_ChSampInit
* Description    : 触摸按键通道采样初始化
* Input          : None
* Return         : None
*******************************************************************************/
void TouchKey_ChSampInit( void )
{
    R8_ADC_CFG = RB_ADC_POWER_ON | RB_ADC_BUF_EN | ( 2<<4 );
    R8_TKEY_CFG |= RB_TKEY_PWR_ON;
}

/*******************************************************************************
* Function Name  : ADC_ExcutSingleConver
* Description    : ADC执行单次转换
* Input          : None
* Return         : ADC转换后的数据
*******************************************************************************/
UINT16 ADC_ExcutSingleConver( void )
{
    R8_ADC_CONVERT = RB_ADC_START;
    while( R8_ADC_CONVERT & RB_ADC_START );

    return ( R16_ADC_DATA&RB_ADC_DATA );
}

/*******************************************************************************
* Function Name  : TouchKey_ExcutSingleConver
* Description    : TouchKey转换后数据
* Input          : charg:  Touchkey充电时间,5bits有效, t=charg*Tadc
*                  disch：    Touchkey放电时间,3bits有效, t=disch*Tadc
* Return         : 当前TouchKey等效数据
*******************************************************************************/
UINT16 TouchKey_ExcutSingleConver( UINT8 charg, UINT8 disch )
{
	R8_TKEY_COUNT = (disch<<5)|(charg&0x1f);
	R8_TKEY_CONVERT = RB_TKEY_START;
	while( R8_TKEY_CONVERT &  RB_TKEY_START );
    return ( R16_ADC_DATA&RB_ADC_DATA );
}


/*******************************************************************************
* Function Name  : ADC_GetCurrentTS
* Description    : 获取当前采样的温度值（℃）
* Input          : ts_v：当前温度传感器采样输出
* Return         : 转换后的温度值（℃）
*******************************************************************************/
int ADC_GetCurrentTS( UINT16 ts_v )
{
  int  C25;
  int  cal;

  C25 = (*((PUINT32)ROM_CFG_TMP_25C));
  cal = ( ( (ts_v * 1050) + 2048 ) >> 12 ) + ( 1050 >> 1 );
  cal = 25 + ((cal - C25)*10/14);
  return (  cal );
}



