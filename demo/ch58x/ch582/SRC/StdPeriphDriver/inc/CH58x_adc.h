


#ifndef __CH58x_ADC_H__
#define __CH58x_ADC_H__

#ifdef __cplusplus
 extern "C" {
#endif

     
#define ROM_CFG_TMP_25C   0x7F014

/**
  * @brief  adc single channel define
  */
typedef enum
{
	CH_EXTIN_0 = 0,			// ADC �ⲿģ��ͨ�� 0
	CH_EXTIN_1,				// ADC �ⲿģ��ͨ�� 1
	CH_EXTIN_2,				// ADC �ⲿģ��ͨ�� 2
	CH_EXTIN_3,				// ADC �ⲿģ��ͨ�� 3
	CH_EXTIN_4,				// ADC �ⲿģ��ͨ�� 4
	CH_EXTIN_5,				// ADC �ⲿģ��ͨ�� 5
	CH_EXTIN_8 = 8,			// ADC �ⲿģ��ͨ�� 8
	CH_EXTIN_9,				// ADC �ⲿģ��ͨ�� 9
	CH_EXTIN_12 = 12,		// ADC �ⲿģ��ͨ�� 12
	CH_EXTIN_13,			// ADC �ⲿģ��ͨ�� 13
	
	CH_INTE_VBAT = 14,		// ADC �ڲ���ؼ��ͨ��
	CH_INTE_VTEMP = 15,		// ADC �ڲ��¶ȴ��������ͨ��

}ADC_SingleChannelTypeDef;

/**
  * @brief  adc differential channel define
  */
typedef enum
{
	CH_DIFF_0_2 = 0,			// ADC ���ͨ�� #0-#2
	CH_DIFF_1_3,				// ADC ���ͨ�� #1-#3

}ADC_DiffChannelTypeDef;

/**
  * @brief  adc sampling clock
  */
typedef enum
{
	SampleFreq_3_2 = 0,			// 3.2M ����Ƶ��
	SampleFreq_8,				// 8M ����Ƶ��
	SampleFreq_5_33,			// 5.33M ����Ƶ��
	SampleFreq_4,				// 4M ����Ƶ��
}ADC_SampClkTypeDef;


/**
  * @brief  adc signal PGA
  */
typedef enum
{
	ADC_PGA_1_4 = 0,			// -12dB, 1/4��
	ADC_PGA_1_2,				// -6dB, 1/2��
	ADC_PGA_0,					// 0dB, 1����������
	ADC_PGA_2,					// 6dB, 2��
}ADC_SignalPGATypeDef;


// refer to ADC_SingleChannelTypeDef
#define		ADC_ChannelCfg( d )		(R8_ADC_CHANNEL = d)												/* ���� ADC ����ͨ�� */
// refer to ADC_SampClkTypeDef
#define		ADC_SampClkCfg( d )		(R8_ADC_CFG=R8_ADC_CFG&(~RB_ADC_CLK_DIV)|(d<<6))					/* ���� ADC ����ʱ�� */
// refer to ADC_SignalPGATypeDef
#define		ADC_PGACfg( d )			(R8_ADC_CFG=R8_ADC_CFG&(~RB_ADC_PGA_GAIN)|(d<<4))				/* ���� ADC �ź����� */
#define		ADC_TempCalibCfg( d )	(R8_TEM_SENSOR=R8_TEM_SENSOR&(~RB_TEM_SEN_CALIB)|d)				/* �����ڲ��¶ȴ�����У׼ֵ */

void ADC_ExtSingleChSampInit( ADC_SampClkTypeDef sp, ADC_SignalPGATypeDef ga );						/* �ⲿ�źŵ�ͨ��������ʼ�� */
void ADC_ExtDiffChSampInit( ADC_SampClkTypeDef sp, ADC_SignalPGATypeDef ga );						/* �ⲿ�źŲ��ͨ��������ʼ�� */
void TouchKey_ChSampInit( void );                                                                   /* ��������ͨ��������ʼ�� */
#define TouchKey_DisableTSPower()       (R8_TKEY_CFG &= ~RB_TKEY_PWR_ON)
void ADC_InterTSSampInit( void );																	/* �����¶ȴ�����������ʼ�� */
#define ADC_DisableTSPower()       		(R8_TEM_SENSOR = 0)                                         /* �ر��¶ȴ�������Դ */
void ADC_InterBATSampInit( void );																	/* ���õ�ص�ѹ������ʼ�� */

UINT16 ADC_ExcutSingleConver( void );																/* ADCִ�е���ת�� */
signed short ADC_DataCalib_Rough( void ); 
UINT16 TouchKey_ExcutSingleConver( UINT8 charg, UINT8 disch );										/* TouchKeyת�������� */
int ADC_GetCurrentTS( UINT16 ts_v );                                                              /* ��ȡ��ǰ�������¶�ֵ���棩 */

#define ADC_ReadConverValue()       (R16_ADC_DATA)                                                  /* ��ȡת�������ֵ */
#define ADC_StartUp()               (R8_ADC_CONVERT = RB_ADC_START)									/* ADC����ת�� */
#define ADC_GetITStatus()           ( R8_ADC_INT_FLAG & RB_ADC_IF_EOC )								/* ��ȡADCת����ɱ�־ */
#define ADC_ClearITFlag()           (R8_ADC_CONVERT = 0)											/* ���ADCת����ɱ�־ */

#define TouchKey_GetITStatus()      ( R8_ADC_INT_FLAG & RB_ADC_IF_EOC )							    /* ��ȡTouchKeyת����ɱ�־ */
#define TouchKey_ClearITFlag()      (R8_TKEY_CTRL |= RB_TKEY_PWR_ON)								/* ���TouchKeyת����ɱ�־ */

	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_ADC_H__

