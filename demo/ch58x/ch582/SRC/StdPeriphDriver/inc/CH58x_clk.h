


#ifndef __CH58x_CLK_H__
#define __CH58x_CLK_H__

#ifdef __cplusplus
 extern "C" {
#endif


typedef enum
{
  CLK_SOURCE_LSI = 0x00,
  CLK_SOURCE_LSE,

  CLK_SOURCE_HSE_16MHz = 0x22,
  CLK_SOURCE_HSE_8MHz = 0x24,
  CLK_SOURCE_HSE_6_4MHz = 0x25,
  CLK_SOURCE_HSE_4MHz = 0x28,
  CLK_SOURCE_HSE_2MHz = (0x20|16),
  CLK_SOURCE_HSE_1MHz = (0x20|0),

  CLK_SOURCE_PLL_80MHz = 0x46,
  CLK_SOURCE_PLL_60MHz = 0x48,
  CLK_SOURCE_PLL_48MHz = (0x40|10),
  CLK_SOURCE_PLL_40MHz = (0x40|12),
  CLK_SOURCE_PLL_36_9MHz = (0x40|13),
  CLK_SOURCE_PLL_32MHz = (0x40|15),
  CLK_SOURCE_PLL_30MHz = (0x40|16),
  CLK_SOURCE_PLL_24MHz = (0x40|20),
  CLK_SOURCE_PLL_20MHz = (0x40|24),
  CLK_SOURCE_PLL_15MHz = (0x40|0),
}SYS_CLKTypeDef;

typedef enum
{
	Clk32K_LSI = 0,
	Clk32K_LSE,
	
}LClk32KTypeDef;

typedef enum
{
	HSE_RCur_75 = 0,
	HSE_RCur_100,
    HSE_RCur_125,
    HSE_RCur_150
	
}HSECurrentTypeDef;

typedef enum
{
	HSECap_10p = 0,
	HSECap_12p,  HSECap_14p,  HSECap_16p,  HSECap_18p,  
    HSECap_20p,  HSECap_22p,  HSECap_24p
	
}HSECapTypeDef;

typedef enum
{
	LSE_RCur_70 = 0,
	LSE_RCur_100,
    LSE_RCur_140,
    LSE_RCur_200
	
}LSECurrentTypeDef;

typedef enum
{
	LSECap_2p = 0,
	LSECap_13p,  LSECap_14p,  LSECap_15p,  LSECap_16p,  
    LSECap_17p,  LSECap_18p,  LSECap_19p,  LSECap_20p,
    LSECap_21p,  LSECap_22p,  LSECap_23p,  LSECap_24p,
    LSECap_25p,  LSECap_26p,  LSECap_27p
	
}LSECapTypeDef;

#define  MAX_DAY		0x00004000 
#define	 MAX_2_SEC		0x0000A8C0
//#define	 MAX_SEC		0x545FFFFF	

#define BEGYEAR                         2020
#define IsLeapYear(yr)                  (!((yr) % 400) || (((yr) % 100) && !((yr) % 4)))
#define YearLength(yr)                  (IsLeapYear(yr) ? 366 : 365)
#define monthLength(lpyr,mon)           ((mon==1) ? (28+lpyr) : ((mon>6) ? ((mon&1)?31:30) : ((mon&1)?30:31)))


/**
  * @brief  rtc timer mode period define
  */
typedef enum
{
	Period_0_125_S = 0,			// 0.125s 周期
	Period_0_25_S,				// 0.25s 周期
	Period_0_5_S,				// 0.5s 周期
	Period_1_S,					// 1s 周期
	Period_2_S,					// 2s 周期
	Period_4_S,					// 4s 周期
	Period_8_S,					// 8s 周期
	Period_16_S,				// 16s 周期
}RTC_TMRCycTypeDef;	 
	 

/**
  * @brief  rtc interrupt event define
  */
typedef enum
{
	RTC_TRIG_EVENT = 0,			// RTC 触发事件
	RTC_TMR_EVENT,				// RTC 周期定时事件

}RTC_EVENTTypeDef;	 

/**
  * @brief  rtc interrupt event define
  */
typedef enum
{
	RTC_TRIG_MODE = 0,			// RTC 触发模式
	RTC_TMR_MODE,				// RTC 周期定时模式

}RTC_MODETypeDef;

typedef enum
{
 /* 校准精度越高，耗时越长 */
  Level_32 = 3,    // 用时 1.2ms 1000ppm (32M 主频)  1100ppm (64M 主频)
  Level_64,        // 用时 2.2ms 800ppm  (32M 主频)  1000ppm (64M 主频)
  Level_128,       // 用时 4.2ms 600ppm  (32M 主频)  800ppm  (64M 主频)

}Cali_LevelTypeDef;

void LClk32K_Select( LClk32KTypeDef hc);		/* 32K 低频时钟来源 */

void HSECFG_Current( HSECurrentTypeDef c );     /* HSE晶体 偏置电流配置 */
void HSECFG_Capacitance( HSECapTypeDef c );     /* HSE晶体 负载电容配置 */
void LSECFG_Current( LSECurrentTypeDef c );     /* LSE晶体 偏置电流配置 */
void LSECFG_Capacitance( LSECapTypeDef c );     /* LSE晶体 负载电容配置 */

void Calibration_LSI( Cali_LevelTypeDef cali_Lv );			/* 用主频校准内部32K时钟 */
	 
void RTC_InitTime( UINT16 y, UINT16 mon, UINT16 d, UINT16 h, UINT16 m, UINT16 s );      /* RTC时钟初始化当前时间 */
void RTC_GetTime( PUINT16 py, PUINT16 pmon, PUINT16 pd, PUINT16 ph, PUINT16 pm, PUINT16 ps );   /* 获取当前时间 */
	 
void RTC_SetCycle32k( UINT32 cyc );							/* 基于LSE/LSI时钟，配置当前RTC 周期数 */	 
UINT32 RTC_GetCycle32k( void );				                /* 基于LSE/LSI时钟，获取当前RTC 周期数 */

void RTC_TRIGFunCfg( UINT32 cyc );							/* RTC触发模式配置间隔时间,基于LSE/LSI时钟，匹配周期数 */
void RTC_TMRFunCfg( RTC_TMRCycTypeDef t );					/* RTC定时模式配置 */
void RTC_ModeFunDisable( RTC_MODETypeDef m );               /* RTC 模式功能关闭 */

UINT8 RTC_GetITFlag( RTC_EVENTTypeDef f );					/* 获取RTC中断标志 */	 
void RTC_ClearITFlag( RTC_EVENTTypeDef f );					/* 清除RTC中断标志 */ 

	 

	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_CLK_H__

