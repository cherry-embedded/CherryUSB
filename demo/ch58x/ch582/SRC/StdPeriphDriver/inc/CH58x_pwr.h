


#ifndef __CH58x_PWR_H__
#define __CH58x_PWR_H__

#ifdef __cplusplus
 extern "C" {
#endif


/** 
  * @brief	Peripher CLK control bit define
  */
#define BIT_SLP_CLK_TMR0                 (0x00000001)  /*!< TMR0 peripher clk bit */
#define BIT_SLP_CLK_TMR1                 (0x00000002)  /*!< TMR1 peripher clk bit */
#define BIT_SLP_CLK_TMR2                 (0x00000004)  /*!< TMR2 peripher clk bit */
#define BIT_SLP_CLK_TMR3                 (0x00000008)  /*!< TMR3 peripher clk bit */
#define BIT_SLP_CLK_UART0                (0x00000010)  /*!< UART0 peripher clk bit */
#define BIT_SLP_CLK_UART1                (0x00000020)  /*!< UART1 peripher clk bit */
#define BIT_SLP_CLK_UART2                (0x00000040)  /*!< UART2 peripher clk bit */
#define BIT_SLP_CLK_UART3                (0x00000080)  /*!< UART3 peripher clk bit */
#define BIT_SLP_CLK_SPI0                 (0x00000100)  /*!< SPI0 peripher clk bit */
//#define BIT_SLP_CLK_SPI1                 (0x00000200)  /*!< SPI1 peripher clk bit */
#define BIT_SLP_CLK_PWMX                 (0x00000400)  /*!< PWMX peripher clk bit */
//#define BIT_SLP_CLK_LCD                  (0x00000800)  /*!< LCD peripher clk bit */
#define BIT_SLP_CLK_USB                  (0x00001000)  /*!< USB peripher clk bit */
//#define BIT_SLP_CLK_ETH                  (0x00002000)  /*!< ETH peripher clk bit */
//#define BIT_SLP_CLK_LED                  (0x00004000)  /*!< LED peripher clk bit */
#define BIT_SLP_CLK_BLE                  (0x00008000)  /*!< BLE peripher clk bit */

#define BIT_SLP_CLK_RAMX                 (0x10000000)  /*!< main SRAM RAM16K peripher clk bit */
#define BIT_SLP_CLK_RAM2K                (0x20000000)  /*!< RAM2K peripher clk bit */
#define BIT_SLP_CLK_ALL                  (0x3000FFFF)  /*!< All peripher clk bit */	 

/**
  * @brief  unit of controllable power supply 
  */
#define UNIT_SYS_LSE                RB_CLK_XT32K_PON        // �ⲿ32K ʱ����
#define UNIT_SYS_LSI                RB_CLK_INT32K_PON       // �ڲ�32K ʱ����
#define UNIT_SYS_HSE                RB_CLK_XT32M_PON        // �ⲿ32M ʱ����
#define UNIT_SYS_PLL                RB_CLK_PLL_PON          // PLL ʱ����


 /**
   * @brief  wakeup mode define
   */
 typedef enum
 {
 	Short_Delay = 0,
	Long_Delay,

 }WakeUP_ModeypeDef;

 /**
   * @brief  wakeup mode define
   */
 typedef enum
 {
	/* ����ȼ���ʹ�ø߾��ȼ�أ�210uA���� */
 	HALevel_1V9 = 0,		// 1.7-1.9
	HALevel_2V1,			// 1.9-2.1
	HALevel_2V3,			// 2.1-2.3
	HALevel_2V5,			// 2.3-2.5

	/* ����ȼ���ʹ�õ͹��ļ�أ�1uA���� */
	LPLevel_1V8 = 0x80,
	LPLevel_1V9,	LPLevel_2V0,	LPLevel_2V1,
	LPLevel_2V2,	LPLevel_2V3,	LPLevel_2V4,	LPLevel_2V5,

 }VolM_LevelypeDef;


void PWR_DCDCCfg( FunctionalState s );	                              /* �ڲ�DC/DC��Դ���� */
void PWR_UnitModCfg( FunctionalState s, UINT8 unit );                   /* �ɿص�Ԫģ��ĵ�Դ���� */
void PWR_PeriphWakeUpCfg( FunctionalState s, UINT8 perph, WakeUP_ModeypeDef mode );    /* �͹��Ļ���Դ���� */

void PowerMonitor( FunctionalState s , VolM_LevelypeDef vl);          /* ��Դ��ѹ��ؿ��� */

void LowPower_Idle( void );                                 /* �͹���-IDLEģʽ */	 
void LowPower_Halt( void );                                 /* �͹���-Haltģʽ */
void LowPower_Sleep( UINT8 rm );                            /* �͹���-Sleepģʽ */
void LowPower_Shutdown( UINT8 rm );                         /* �͹���-Shutdownģʽ */

	 
	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_PWR_H__

