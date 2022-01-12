


#ifndef __CH58x_TIMER_H__
#define __CH58x_TIMER_H__

#ifdef __cplusplus
 extern "C" {
#endif


#define	DataBit_25	(1<<25)	 


/**
  * @brief  TMR0 interrupt bit define
  */

#define	TMR0_3_IT_CYC_END 		0x01				// ���ڽ�����־����׽-��ʱ����ʱ-���ڽ�����PWM-���ڽ���
#define	TMR0_3_IT_DATA_ACT		0x02				// ������Ч��־����׽-�����ݣ�PWM-��Ч��ƽ����
#define	TMR0_3_IT_FIFO_HF 		0x04				// FIFO ʹ�ù��룺��׽- FIFO>=4�� PWM- FIFO<4
#define	TMR1_2_IT_DMA_END 		0x08				// DMA ������֧��TMR1��TMR2
#define	TMR0_3_IT_FIFO_OV 		0x10				// FIFO �������׽- FIFO���� PWM- FIFO��
	 
	 
/**
  * @brief  Configuration PWM effective level repeat times
  */
typedef enum
{
	PWM_Times_1 = 0,					// PWM ��Ч����ظ�1����
	PWM_Times_4,						// PWM ��Ч����ظ�4����
	PWM_Times_8,						// PWM ��Ч����ظ�8����
	PWM_Times_16,						// PWM ��Ч����ظ�16����
}PWM_RepeatTsTypeDef;	


/**
  * @brief  Configuration Cap mode
  */
typedef enum
{
	CAP_NULL = 0,						// ����׽ & ������
	Edge_To_Edge,						// �������֮��  &  �����������
	FallEdge_To_FallEdge,				// �½��ص��½���  & �����½���
	RiseEdge_To_RiseEdge,				// �����ص�������  &  ����������
}CapModeTypeDef;

/**
  * @brief  Configuration DMA mode
  */
typedef enum
{
	Mode_Single = 0,				// ����ģʽ
	Mode_LOOP,						// ѭ��ģʽ
}DMAModeTypeDef;


/****************** TMR0 */
// ��ʱ����
void TMR0_TimerInit( UINT32 t );									/* ��ʱ���ܳ�ʼ�� */	 
#define  TMR0_GetCurrentTimer()		R32_TMR0_COUNT	 				/* ��ȡ��ǰ��ʱ��ֵ�����67108864 */

//��������
void TMR0_EXTSingleCounterInit( CapModeTypeDef cap );				/* �ⲿ�źű��ؼ�����ʼ�� */
#define TMR0_CountOverflowCfg( cyc )   (R32_TMR0_CNT_END=(cyc+2))	/* ����ͳ�������С�����67108862 */
#define  TMR0_GetCurrentCount()		R32_TMR0_COUNT	 				/* ��ȡ��ǰ����ֵ�����67108862 */

// PWM����
#define TMR0_PWMCycleCfg( cyc )	    (R32_TMR0_CNT_END=cyc)			/* PWM0 ͨ�����������������, ���67108864 */
void TMR0_PWMInit( PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts );	/* PWM �����ʼ�� */	 
#define TMR0_PWMActDataWidth( d )   (R32_TMR0_FIFO = d)			/* PWM0 ��Ч��������, ���67108864 */
	 
// ��׽����
#define TMR0_CAPTimeoutCfg( cyc )   (R32_TMR0_CNT_END=cyc)			/* CAP0 ��׽��ƽ��ʱ����, ���33554432 */
void TMR0_CapInit( CapModeTypeDef cap );							/* �ⲿ�źŲ�׽���ܳ�ʼ�� */
#define TMR0_CAPGetData()			R32_TMR0_FIFO					/* ��ȡ�������� */
#define TMR0_CAPDataCounter()		R8_TMR0_FIFO_COUNT				/* ��ȡ��ǰ�Ѳ������ݸ��� */


#define TMR0_Disable()				(R8_TMR0_CTRL_MOD &= ~RB_TMR_COUNT_EN)		/* �ر� TMR0 */
#define TMR0_Enable()         (R8_TMR0_CTRL_MOD |= RB_TMR_COUNT_EN)   /* ���� TMR0 */
// refer to TMR0 interrupt bit define
#define	TMR0_ITCfg(s,f)				((s)?(R8_TMR0_INTER_EN|=f):(R8_TMR0_INTER_EN&=~f))		/* TMR0 ��Ӧ�ж�λ������ر� */
// refer to TMR0 interrupt bit define
#define TMR0_ClearITFlag(f)         (R8_TMR0_INT_FLAG = f)			/* ����жϱ�־ */
#define TMR0_GetITFlag(f)           (R8_TMR0_INT_FLAG&f)			/* ��ѯ�жϱ�־״̬ */


/****************** TMR1 */
// ��ʱ�ͼ���
void TMR1_TimerInit( UINT32 t );									/* ��ʱ���ܳ�ʼ�� */	 
#define  TMR1_GetCurrentTimer()		R32_TMR1_COUNT	 				/* ��ȡ��ǰ��ʱ��ֵ�����67108864 */

//��������
void TMR1_EXTSingleCounterInit( CapModeTypeDef cap );				/* �ⲿ�źű��ؼ�����ʼ�� */
#define TMR1_CountOverflowCfg( cyc )   (R32_TMR1_CNT_END=(cyc+2))	/* ����ͳ�������С�����67108862 */
#define  TMR1_GetCurrentCount()		R32_TMR1_COUNT	 				/* ��ȡ��ǰ����ֵ�����67108862 */

// PWM����
#define TMR1_PWMCycleCfg( cyc )	    (R32_TMR1_CNT_END=cyc)			/* PWM1 ͨ�����������������, ���67108864 */
void TMR1_PWMInit( PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts );	/* PWM1 �����ʼ�� */	 
#define TMR1_PWMActDataWidth( d )   (R32_TMR1_FIFO = d)			/* PWM1 ��Ч��������, ���67108864 */
	 
// ��׽����
#define TMR1_CAPTimeoutCfg( cyc )   (R32_TMR1_CNT_END=cyc)			/* CAP1 ��׽��ƽ��ʱ����, ���33554432 */
void TMR1_CapInit( CapModeTypeDef cap );							/* �ⲿ�źŲ�׽���ܳ�ʼ�� */
#define TMR1_CAPGetData()			R32_TMR1_FIFO					/* ��ȡ�������� */
#define TMR1_CAPDataCounter()		R8_TMR1_FIFO_COUNT				/* ��ȡ��ǰ�Ѳ������ݸ��� */

void TMR1_DMACfg( UINT8 s, UINT16 startAddr, UINT16 endAddr, DMAModeTypeDef m );    /* DMA����  */

#define TMR1_Disable()				(R8_TMR1_CTRL_MOD &= ~RB_TMR_COUNT_EN)		/* �ر� TMR1 */
#define TMR1_Enable()         (R8_TMR1_CTRL_MOD |= RB_TMR_COUNT_EN)   /* ���� TMR1 */
// refer to TMR1 interrupt bit define
#define	TMR1_ITCfg(s,f)				((s)?(R8_TMR1_INTER_EN|=f):(R8_TMR1_INTER_EN&=~f))		/* TMR1 ��Ӧ�ж�λ������ر� */
// refer to TMR1 interrupt bit define
#define TMR1_ClearITFlag(f)         (R8_TMR1_INT_FLAG = f)			/* ����жϱ�־ */
#define TMR1_GetITFlag(f)           (R8_TMR1_INT_FLAG&f)			/* ��ѯ�жϱ�־״̬ */


/****************** TMR2 */
// ��ʱ�ͼ���
void TMR2_TimerInit( UINT32 t );									/* ��ʱ���ܳ�ʼ�� */	 
#define  TMR2_GetCurrentTimer()		R32_TMR2_COUNT	 				/* ��ȡ��ǰ��ʱ��ֵ�����67108864 */

//��������
void TMR2_EXTSingleCounterInit( CapModeTypeDef cap );				/* �ⲿ�źű��ؼ�����ʼ�� */
#define TMR2_CountOverflowCfg( cyc )   (R32_TMR2_CNT_END=(cyc+2))	/* ����ͳ�������С�����67108862 */
#define  TMR2_GetCurrentCount()		R32_TMR2_COUNT	 				/* ��ȡ��ǰ����ֵ�����67108862 */

// PWM����
#define TMR2_PWMCycleCfg( cyc )	    (R32_TMR2_CNT_END=cyc)			/* PWM2 ͨ�����������������, ���67108864 */
void TMR2_PWMInit( PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts );	/* PWM2 �����ʼ�� */	 
#define TMR2_PWMActDataWidth( d )   (R32_TMR2_FIFO = d)			/* PWM2 ��Ч��������, ���67108864 */
	 
// ��׽����
#define TMR2_CAPTimeoutCfg( cyc )   (R32_TMR2_CNT_END=cyc)			/* CAP2 ��׽��ƽ��ʱ����, ���33554432 */
void TMR2_CapInit( CapModeTypeDef cap );							/* �ⲿ�źŲ�׽���ܳ�ʼ�� */
#define TMR2_CAPGetData()			R32_TMR2_FIFO					/* ��ȡ�������� */
#define TMR2_CAPDataCounter()		R8_TMR2_FIFO_COUNT				/* ��ȡ��ǰ�Ѳ������ݸ��� */

void TMR2_DMACfg( UINT8 s, UINT16 startAddr, UINT16 endAddr, DMAModeTypeDef m );    /* DMA����  */

#define TMR2_Disable()				(R8_TMR2_CTRL_MOD &= ~RB_TMR_COUNT_EN)		/* �ر� TMR2 */
#define TMR2_Enable()         (R8_TMR2_CTRL_MOD |= RB_TMR_COUNT_EN)   /* ���� TMR2 */
// refer to TMR2 interrupt bit define
#define	TMR2_ITCfg(s,f)				((s)?(R8_TMR2_INTER_EN|=f):(R8_TMR2_INTER_EN&=~f))		/* TMR2 ��Ӧ�ж�λ������ر� */
// refer to TMR2 interrupt bit define
#define TMR2_ClearITFlag(f)         (R8_TMR2_INT_FLAG = f)			/* ����жϱ�־ */
#define TMR2_GetITFlag(f)           (R8_TMR2_INT_FLAG&f)			/* ��ѯ�жϱ�־״̬ */


/****************** TMR3 */
// ��ʱ�ͼ���
void TMR3_TimerInit( UINT32 t );									/* ��ʱ���ܳ�ʼ�� */	 
#define  TMR3_GetCurrentTimer()		R32_TMR3_COUNT	 				/* ��ȡ��ǰ��ʱ��ֵ�����67108864 */

//��������
void TMR3_EXTSingleCounterInit( CapModeTypeDef cap );				/* �ⲿ�źű��ؼ�����ʼ�� */
#define TMR3_CountOverflowCfg( cyc )   (R32_TMR3_CNT_END=(cyc+2))	/* ����ͳ�������С�����67108862 */
#define  TMR3_GetCurrentCount()		R32_TMR3_COUNT	 				/* ��ȡ��ǰ����ֵ�����67108862 */

// PWM����
#define TMR3_PWMCycleCfg( cyc )	    (R32_TMR3_CNT_END=cyc)			/* PWM3 ͨ�����������������, ���67108864 */
void TMR3_PWMInit( PWMX_PolarTypeDef pr, PWM_RepeatTsTypeDef ts );	/* PWM3 �����ʼ�� */	 
#define TMR3_PWMActDataWidth( d )   (R32_TMR3_FIFO = d)			/* PWM3 ��Ч��������, ���67108864 */
	 
// ��׽����
#define TMR3_CAPTimeoutCfg( cyc )   (R32_TMR3_CNT_END=cyc)			/* CAP3 ��׽��ƽ��ʱ����, ���33554432 */
void TMR3_CapInit( CapModeTypeDef cap );							/* �ⲿ�źŲ�׽���ܳ�ʼ�� */
#define TMR3_CAPGetData()			R32_TMR3_FIFO					/* ��ȡ�������� */
#define TMR3_CAPDataCounter()		R8_TMR3_FIFO_COUNT				/* ��ȡ��ǰ�Ѳ������ݸ��� */


#define TMR3_Disable()				(R8_TMR3_CTRL_MOD &= ~RB_TMR_COUNT_EN)		/* �ر� TMR3 */
#define TMR3_Enable()         (R8_TMR3_CTRL_MOD |= RB_TMR_COUNT_EN)   /* ���� TMR3 */
// refer to TMR3 interrupt bit define
#define	TMR3_ITCfg(s,f)				((s)?(R8_TMR3_INTER_EN|=f):(R8_TMR3_INTER_EN&=~f))		/* TMR3 ��Ӧ�ж�λ������ر� */
// refer to TMR3 interrupt bit define
#define TMR3_ClearITFlag(f)         (R8_TMR3_INT_FLAG = f)			/* ����жϱ�־ */
#define TMR3_GetITFlag(f)           (R8_TMR3_INT_FLAG&f)			/* ��ѯ�жϱ�־״̬ */


	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_TIMER_H__

