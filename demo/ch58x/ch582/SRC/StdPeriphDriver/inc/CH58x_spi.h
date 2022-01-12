


#ifndef __CH58x_SPI_H__
#define __CH58x_SPI_H__

#ifdef __cplusplus
 extern "C" {
#endif


/**
  * @brief  SPI0 interrupt bit define
  */

#define	SPI0_IT_FST_BYTE 		RB_SPI_IE_FST_BYTE				// �ӻ�ģʽ�����ֽ�����ģʽ�£����յ����ֽ��ж�
#define	SPI0_IT_FIFO_OV			RB_SPI_IE_FIFO_OV				// FIFO ���
#define	SPI0_IT_DMA_END 		RB_SPI_IE_DMA_END				// DMA �������
#define	SPI0_IT_FIFO_HF 		RB_SPI_IE_FIFO_HF				// FIFO ʹ�ù���
#define	SPI0_IT_BYTE_END		RB_SPI_IE_BYTE_END				// ���ֽڴ������
#define	SPI0_IT_CNT_END 		RB_SPI_IE_CNT_END				// ȫ���ֽڴ������	 
	 
	 
	 
/**
  * @brief  Configuration data mode
  */
typedef enum
{
	Mode0_LowBitINFront = 0,					// ģʽ0����λ��ǰ
	Mode0_HighBitINFront,						// ģʽ0����λ��ǰ
	Mode3_LowBitINFront,						// ģʽ3����λ��ǰ
	Mode3_HighBitINFront,						// ģʽ3����λ��ǰ
}ModeBitOrderTypeDef;


/**
  * @brief  Configuration SPI0 slave mode
  */
typedef enum
{
	Mode_DataStream = 0,				// ������ģʽ
	Mose_FirstCmd,						// ���ֽ�����ģʽ
}Slave_ModeTypeDef;
	 
	 
/**************** SPI0 */	 
void SPI0_MasterDefInit( void );                            /* ����ģʽĬ�ϳ�ʼ����ģʽ0+3��ȫ˫��+8MHz */
void SPI0_CLKCfg( UINT8 c );                                /* SPI0 ��׼ʱ�����ã�= d*Tsys */ 	 
void SPI0_DataMode( ModeBitOrderTypeDef m );                /* ����������ģʽ */	 

void SPI0_MasterSendByte( UINT8 d );                        /* ���͵��ֽ� (buffer) */
UINT8 SPI0_MasterRecvByte( void );                          /* ���յ��ֽ� (buffer) */

void SPI0_MasterTrans( UINT8 *pbuf, UINT16 len );           /* ʹ��FIFO�������Ͷ��ֽ� */	 
void SPI0_MasterRecv( UINT8 *pbuf, UINT16 len );            /* ʹ��FIFO�������ն��ֽ� */

void SPI0_MasterDMATrans( PUINT8 pbuf, UINT16 len);			/* DMA��ʽ������������   */
void SPI0_MasterDMARecv( PUINT8 pbuf, UINT16 len);			/* DMA��ʽ������������  */

void SPI1_MasterDefInit( void );                            /* ����ģʽĬ�ϳ�ʼ����ģʽ0+3��ȫ˫��+8MHz */
void SPI1_CLKCfg( UINT8 c );                                /* SPI1 ��׼ʱ�����ã�= d*Tsys */
void SPI1_DataMode( ModeBitOrderTypeDef m );                /* ����������ģʽ */

void SPI1_MasterSendByte( UINT8 d );                        /* ���͵��ֽ� (buffer) */
UINT8 SPI1_MasterRecvByte( void );                          /* ���յ��ֽ� (buffer) */

void SPI1_MasterTrans( UINT8 *pbuf, UINT16 len );           /* ʹ��FIFO�������Ͷ��ֽ� */
void SPI1_MasterRecv( UINT8 *pbuf, UINT16 len );            /* ʹ��FIFO�������ն��ֽ� */

void SPI0_SlaveInit( void );			                    /* �豸ģʽĬ�ϳ�ʼ������������MISO��GPIO��ӦΪ����ģʽ */
#define SetFirstData(d)			(R8_SPI0_SLAVE_PRE = d)		/* �������ֽ��������� */
void SPI0_SlaveSendByte( UINT8 d );			                /* �ӻ�ģʽ������һ�ֽ����� */
UINT8 SPI0_SlaveRecvByte( void );			                /* �ӻ�ģʽ������һ�ֽ����� */

void SPI0_SlaveTrans( UINT8 *pbuf, UINT16 len );            /* �ӻ�ģʽ�����Ͷ��ֽ����� */
void SPI0_SlaveRecv( PUINT8 pbuf, UINT16 len );             /* �ӻ�ģʽ�����ն��ֽ�����  */

void SPI0_SlaveDMATrans( PUINT8 pbuf, UINT16 len);          /* �ӻ�ģʽ��DMA��ʽ���Ͷ��ֽ����� */
void SPI0_SlaveDMARecv( PUINT8 pbuf, UINT16 len);           /* �ӻ�ģʽ��DMA��ʽ���ն��ֽ����� */

// refer to SPI0 interrupt bit define
#define SPI0_ITCfg(s,f)			((s)?(R8_SPI0_INTER_EN|=f):(R8_SPI0_INTER_EN&=~f))
#define SPI0_GetITFlag(f)		(R8_SPI0_INT_FLAG&f)		/* ��ȡ�жϱ�־״̬��0-δ��λ��(!0)-���� */
#define SPI0_ClearITFlag(f)		(R8_SPI0_INT_FLAG = f)		/* �����ǰ�жϱ�־ */


	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_SPI_H__

