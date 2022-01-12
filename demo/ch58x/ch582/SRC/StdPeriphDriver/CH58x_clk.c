/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_clk.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : LClk32K_Select
* Description    : 32K 低频时钟来源
* Input          : hc: 
					Clk32K_LSI   -   选择内部32K
					Clk32K_LSE   -   选择外部32K
* Return         : None
*******************************************************************************/
void LClk32K_Select( LClk32KTypeDef hc)
{
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;	
    SAFEOPERATE;
    if( hc == Clk32K_LSI)
        R8_CK32K_CONFIG &= ~RB_CLK_OSC32K_XT;
    else
        R8_CK32K_CONFIG |= RB_CLK_OSC32K_XT;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : HSECFG_Current
* Description    : HSE晶体 偏置电流配置
* Input          : c: 75%,100%,125%,150%
* Return         : None
*******************************************************************************/
void HSECFG_Current( HSECurrentTypeDef c )
{
    UINT8  x32M_c;
    
    x32M_c = R8_XT32M_TUNE;
    x32M_c = (x32M_c&0xfc)|(c&0x03);
    
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;	
    SAFEOPERATE;
    R8_XT32M_TUNE = x32M_c;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : HSECFG_Capacitance
* Description    : HSE晶体 负载电容配置
* Input          : c: refer to HSECapTypeDef
* Return         : None
*******************************************************************************/
void HSECFG_Capacitance( HSECapTypeDef c )
{
    UINT8  x32M_c;
    
    x32M_c = R8_XT32M_TUNE;
    x32M_c = (x32M_c&0x8f)|(c<<4);
    
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;	
    SAFEOPERATE;
    R8_XT32M_TUNE = x32M_c;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : LSECFG_Current
* Description    : LSE晶体 偏置电流配置
* Input          : c: 70%,100%,140%,200%
* Return         : None
*******************************************************************************/
void LSECFG_Current( LSECurrentTypeDef c )
{
    UINT8  x32K_c;
    
    x32K_c = R8_XT32K_TUNE;
    x32K_c = (x32K_c&0xfc)|(c&0x03);
    
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_XT32K_TUNE = x32K_c;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : LSECFG_Capacitance
* Description    : LSE晶体 负载电容配置
* Input          : c: refer to LSECapTypeDef
* Return         : None
*******************************************************************************/
void LSECFG_Capacitance( LSECapTypeDef c )
{
    UINT8  x32K_c;
    
    x32K_c = R8_XT32K_TUNE;
    x32K_c = (x32K_c&0x0f)|(c<<4);
    
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_XT32K_TUNE = x32K_c;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : Calibration_LSI
* Description    : 校准内部32K时钟
* Input          : cali_Lv : 校准等级选择
*                           Level_32  -   用时 1.2ms 1000ppm (32M 主频)  1100ppm (64M 主频)
*                           Level_64  -   用时 2.2ms 800ppm  (32M 主频)  1000ppm (64M 主频)
*                           Level_128 -   用时 4.2ms 600ppm  (32M 主频)  800ppm  (64M 主频)
* Return         : None
*******************************************************************************/
void Calibration_LSI( Cali_LevelTypeDef cali_Lv )
{
  UINT32 i;
  INT32 cnt_offset;
  UINT8 retry=0;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R8_CK32K_CONFIG |= RB_CLK_OSC32K_FILT;
  R8_CK32K_CONFIG &= ~RB_CLK_OSC32K_FILT;
  R8_XT32K_TUNE &= ~3;
  R8_XT32K_TUNE |= 1;

  // 粗调
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R8_OSC_CAL_CTRL &= ~RB_OSC_CNT_TOTAL;
  R8_OSC_CAL_CTRL |= 1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  R8_OSC_CAL_CTRL |= RB_OSC_CNT_EN;
  R16_OSC_CAL_CNT |= RB_OSC_CAL_OV_CLR;
  while(1)
  {
    while( !( R8_OSC_CAL_CTRL & RB_OSC_CNT_HALT ) );
    i = R16_OSC_CAL_CNT;      // 用于丢弃
    while( R8_OSC_CAL_CTRL & RB_OSC_CNT_HALT );
    R16_OSC_CAL_CNT |= RB_OSC_CAL_OV_CLR;
    while( !( R8_OSC_CAL_CTRL & RB_OSC_CNT_HALT ) );
    i = R16_OSC_CAL_CNT;      // 实时校准后采样值
    cnt_offset = (i& 0x3FFF)+R8_OSC_CAL_OV_CNT*0x3FFF - 2000*(FREQ_SYS/1000)/CAB_LSIFQ;
    if( ((cnt_offset > -37*(FREQ_SYS/1000)/CAB_LSIFQ) && (cnt_offset < 37*(FREQ_SYS/1000)/CAB_LSIFQ)) || retry>2 )
      break;
    retry++;
    cnt_offset = (cnt_offset>0)? (((cnt_offset*2)/(37*(FREQ_SYS/1000)/CAB_LSIFQ))+1)/2 : (((cnt_offset*2)/(37*(FREQ_SYS/1000)/CAB_LSIFQ))-1)/2;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R16_INT32K_TUNE += cnt_offset;
  }

  // 细调
  // 配置细调参数后，丢弃2次捕获值（软件行为）上判断已有一次，这里只留一次
  while( !( R8_OSC_CAL_CTRL & RB_OSC_CNT_HALT ) );
  i = R16_OSC_CAL_CNT;      // 用于丢弃
  R16_OSC_CAL_CNT |= RB_OSC_CAL_OV_CLR;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R8_OSC_CAL_CTRL &= ~RB_OSC_CNT_TOTAL;
  R8_OSC_CAL_CTRL |= cali_Lv;
  while( R8_OSC_CAL_CTRL & RB_OSC_CNT_HALT );
  while( !( R8_OSC_CAL_CTRL & RB_OSC_CNT_HALT ) );
  i = R16_OSC_CAL_CNT;      // 实时校准后采样值
  cnt_offset = (i& 0x3FFF)+R8_OSC_CAL_OV_CNT*0x3FFF - 4000*(1<<cali_Lv)*(FREQ_SYS/1000000)/CAB_LSIFQ*1000;
  cnt_offset = (cnt_offset>0)? ((((cnt_offset*(3200/(1<<cali_Lv)))/(1366*(FREQ_SYS/1000)/CAB_LSIFQ))+1)/2)<<5 : ((((cnt_offset*(3200/(1<<cali_Lv)))/(1366*(FREQ_SYS/1000)/CAB_LSIFQ))-1)/2)<<5;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
  R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
  SAFEOPERATE;
  R16_INT32K_TUNE += cnt_offset;
  R8_OSC_CAL_CTRL &= ~RB_OSC_CNT_EN;
}

/*******************************************************************************
* Function Name  : RTCInitTime
* Description    : RTC时钟初始化当前时间
* Input          : y: 配置时间 - 年
          MAX_Y = BEGYEAR + 44
           mon: 配置时间 - 月
          MAX_MON = 12
           d: 配置时间 - 日
          MAX_D = 31

           h: 配置时间 - 小时
          MAX_H = 23
           m: 配置时间 - 分钟
          MAX_M = 59
           s: 配置时间 - 秒
          MAX_S = 59
* Return         : None
*******************************************************************************/
void RTC_InitTime( UINT16 y, UINT16 mon, UINT16 d, UINT16 h, UINT16 m, UINT16 s )
{
    UINT32  t;
    UINT16  year, month, day, sec2, t32k;
    UINT8V clk_pin;

    year = y;
    month = mon;
    day = 0;
    while ( year > BEGYEAR )
    {
      day += YearLength( year-1 );
      year--;
    }
    while ( month > 1 )
    {
      day += monthLength( IsLeapYear( y ), month-2 );
      month--;
    }

    day += d-1;
    sec2 = (h%24)*1800+m*30+s/2;
    t32k = (s&1)?(0x8000):(0);
    t = sec2;
    t = t<<16 | t32k;

    do{
      clk_pin = (R8_CK32K_CONFIG&RB_32K_CLK_PIN);
    }while( clk_pin != (R8_CK32K_CONFIG&RB_32K_CLK_PIN) );
    if(!clk_pin){
      while( !clk_pin )
      {
        do{
          clk_pin = (R8_CK32K_CONFIG&RB_32K_CLK_PIN);
        }while( clk_pin != (R8_CK32K_CONFIG&RB_32K_CLK_PIN) );
      }
    }

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R32_RTC_TRIG = day;
    R8_RTC_MODE_CTRL |= RB_RTC_LOAD_HI;
    while((R32_RTC_TRIG&0x3FFF) != (R32_RTC_CNT_DAY & 0x3FFF));
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R32_RTC_TRIG = t;
    R8_RTC_MODE_CTRL |= RB_RTC_LOAD_LO;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : RTC_GetTime
* Description    : 获取当前时间
* Input          : y: 获取到的时间 - 年
          MAX_Y = BEGYEAR + 44
           mon: 获取到的时间 - 月
          MAX_MON = 12
           d: 获取到的时间 - 日
          MAX_D = 31
           ph: 获取到的时间 - 小时
          MAX_H = 23
           pm: 获取到的时间 - 分钟
          MAX_M = 59
           ps: 获取到的时间 - 秒
          MAX_S = 59
* Return         : None
*******************************************************************************/
void RTC_GetTime( PUINT16 py, PUINT16 pmon, PUINT16 pd, PUINT16 ph, PUINT16 pm, PUINT16 ps )
{
    UINT32  t;
    UINT16  day, sec2, t32k;

    day = R32_RTC_CNT_DAY & 0x3FFF;
    sec2 = R16_RTC_CNT_2S; 
    t32k = R16_RTC_CNT_32K;

    t = sec2*2 + ((t32k<0x8000)?0:1);

    *py = BEGYEAR;
    while ( day >= YearLength( *py ) )
    {
      day -= YearLength( *py );
      (*py)++;
    }

    *pmon = 0;
    while ( day >= monthLength( IsLeapYear( *py ), *pmon ) )
    {
      day -= monthLength( IsLeapYear( *py ), *pmon );
      (*pmon)++;
    }
    (*pmon) ++;
    *pd = day+1;
    *ph = t/3600;
    *pm = t%3600/60;
    *ps = t%60;
}

/*******************************************************************************
* Function Name  : RTC_SetCycle32k
* Description    : 基于LSE/LSI时钟，配置当前RTC 周期数
* Input          : cyc: 配置周期计数初值 - cycle
					MAX_CYC = 0xA8BFFFFF = 2831155199
* Return         : None
*******************************************************************************/
void RTC_SetCycle32k( UINT32 cyc )
{
    UINT8V clk_pin;

    do{
      clk_pin = (R8_CK32K_CONFIG&RB_32K_CLK_PIN);
    }while( (clk_pin != (R8_CK32K_CONFIG&RB_32K_CLK_PIN)) || (!clk_pin) );

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R32_RTC_TRIG = cyc;
    R8_RTC_MODE_CTRL |= RB_RTC_LOAD_LO;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : RTC_GetCycle32k
* Description    : 基于LSE/LSI时钟，获取当前RTC 周期数
* Input          : None
* Return         : 返回当前周期数，MAX_CYC = 0xA8BFFFFF = 2831155199
*******************************************************************************/
UINT32 RTC_GetCycle32k( void )
{
    UINT32V i;
    
    do{
    	i = R32_RTC_CNT_32K;
    }while( i != R32_RTC_CNT_32K );
    
    return (i);
}

/*******************************************************************************
* Function Name  : RTC_TMRFunCfg
* Description    : RTC定时模式配置（注意定时基准固定为32768Hz）
* Input          : t: 
					refer to RTC_TMRCycTypeDef
* Return         : None
*******************************************************************************/
void RTC_TMRFunCfg( RTC_TMRCycTypeDef t )
{
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_RTC_MODE_CTRL &= ~(RB_RTC_TMR_EN|RB_RTC_TMR_MODE);
    R8_RTC_MODE_CTRL |= RB_RTC_TMR_EN | (t);
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : RTC_TRIGFunCfg
* Description    : RTC时间触发模式配置
* Input          : cyc: 相对当前时间的触发间隔时间，基于LSE/LSI时钟周期数
* Return         : None
*******************************************************************************/
void RTC_TRIGFunCfg( UINT32 cyc )
{
    UINT32 t;

    t = RTC_GetCycle32k() + cyc;

    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R32_RTC_TRIG = t;
    R8_RTC_MODE_CTRL |= RB_RTC_TRIG_EN;
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : RTC_ModeFunDisable
* Description    : RTC 模式功能关闭
* Input          : m: 需要关闭的当前模式
* Return         : None
*******************************************************************************/
void RTC_ModeFunDisable( RTC_MODETypeDef m )
{
    UINT8  i=0;
    
    if( m == RTC_TRIG_MODE )    i |= RB_RTC_TRIG_EN;
    else if( m == RTC_TMR_MODE )     i |= RB_RTC_TMR_EN;
    
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;		
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    SAFEOPERATE;
    R8_RTC_MODE_CTRL &= ~(i);
    R8_SAFE_ACCESS_SIG = 0;
}

/*******************************************************************************
* Function Name  : RTC_GetITFlag
* Description    : 获取RTC中断标志
* Input          : f: 
					refer to RTC_EVENTTypeDef
* Return         : 中断标志状态:
					0     -  	未发生事件
				   (!0)   -  	发生事件
*******************************************************************************/
UINT8 RTC_GetITFlag( RTC_EVENTTypeDef f )
{
    if( f == RTC_TRIG_EVENT )
        return ( R8_RTC_FLAG_CTRL & RB_RTC_TRIG_FLAG );
    else 
        return ( R8_RTC_FLAG_CTRL & RB_RTC_TMR_FLAG );
}

/*******************************************************************************
* Function Name  : RTC_ClearITFlag
* Description    : 清除RTC中断标志
* Input          : f: 
					refer to RTC_EVENTTypeDef
* Return         : None
*******************************************************************************/
void RTC_ClearITFlag( RTC_EVENTTypeDef f )
{
    switch( f ) 
    {
        case RTC_TRIG_EVENT:
            R8_RTC_FLAG_CTRL = RB_RTC_TRIG_CLR;
            break;
        case RTC_TMR_EVENT:
            R8_RTC_FLAG_CTRL = RB_RTC_TMR_CLR;
            break;
        default :
            break;
    }
}


