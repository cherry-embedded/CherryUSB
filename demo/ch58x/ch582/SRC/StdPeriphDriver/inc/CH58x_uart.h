


#ifndef __CH58x_UART_H__
#define __CH58x_UART_H__

#ifdef __cplusplus
 extern "C" {
#endif


/** 
  * @brief	LINE error and status define
  */     
#define  STA_ERR_BREAK      RB_LSR_BREAK_ERR       // ���ݼ������     
#define  STA_ERR_FRAME      RB_LSR_FRAME_ERR       // ����֡����     
#define  STA_ERR_PAR        RB_LSR_PAR_ERR         // ��żУ��λ����
#define  STA_ERR_FIFOOV     RB_LSR_OVER_ERR        // �����������  
     
#define  STA_TXFIFO_EMP     RB_LSR_TX_FIFO_EMP     // ��ǰ����FIFO�գ����Լ�����䷢������
#define  STA_TXALL_EMP      RB_LSR_TX_ALL_EMP      // ��ǰ���з������ݶ��������     
#define  STA_RECV_DATA      RB_LSR_DATA_RDY        // ��ǰ�н��յ�����


/**
  * @brief  Configuration UART TrigByte num
  */     
typedef enum
{
	UART_1BYTE_TRIG = 0,        // 1�ֽڴ���
	UART_2BYTE_TRIG,            // 2�ֽڴ���
	UART_4BYTE_TRIG,            // 4�ֽڴ���
	UART_7BYTE_TRIG,            // 7�ֽڴ���
	
}UARTByteTRIGTypeDef;     
 
 
/****************** UART0 */ 
void UART0_DefInit( void );	 							/* ����Ĭ�ϳ�ʼ������ */
void UART0_BaudRateCfg( UINT32 baudrate );	 			/* ���ڲ��������� */
void UART0_ByteTrigCfg( UARTByteTRIGTypeDef b );        /* �����ֽڴ����ж����� */
void UART0_INTCfg( FunctionalState s,  UINT8 i );		            /* �����ж����� */
void UART0_Reset( void );								/* ���������λ */

#define UART0_CLR_RXFIFO()       (R8_UART0_FCR |= RB_FCR_RX_FIFO_CLR)          /* �����ǰ����FIFO */
#define UART0_CLR_TXFIFO()       (R8_UART0_FCR |= RB_FCR_TX_FIFO_CLR)          /* �����ǰ����FIFO */

#define UART0_GetITFlag()       (R8_UART0_IIR&RB_IIR_INT_MASK)          /* ��ȡ��ǰ�жϱ�־ */
// please refer to LINE error and status define
#define UART0_GetLinSTA()       (R8_UART0_LSR)          /* ��ȡ��ǰͨѶ״̬ */

#define	UART0_SendByte(b)		(R8_UART0_THR = b)		/* ���ڵ��ֽڷ��� */
void UART0_SendString( PUINT8 buf, UINT16 l );			/* ���ڶ��ֽڷ��� */
#define	UART0_RecvByte()		( R8_UART0_RBR )        /* ���ڶ�ȡ���ֽ� */
UINT16 UART0_RecvString( PUINT8 buf );					/* ���ڶ�ȡ���ֽ� */

 
     
/****************** UART1 */ 	 
void UART1_DefInit( void );	 							/* ����Ĭ�ϳ�ʼ������ */
void UART1_BaudRateCfg( UINT32 baudrate );	 			/* ���ڲ��������� */
void UART1_ByteTrigCfg( UARTByteTRIGTypeDef b );        /* �����ֽڴ����ж����� */
void UART1_INTCfg( FunctionalState s,  UINT8 i );		            /* �����ж����� */
void UART1_Reset( void );								/* ���������λ */

#define UART1_CLR_RXFIFO()       (R8_UART1_FCR |= RB_FCR_RX_FIFO_CLR)          /* �����ǰ����FIFO */
#define UART1_CLR_TXFIFO()       (R8_UART1_FCR |= RB_FCR_TX_FIFO_CLR)          /* �����ǰ����FIFO */

#define UART1_GetITFlag()       (R8_UART1_IIR&RB_IIR_INT_MASK)          /* ��ȡ��ǰ�жϱ�־ */
// please refer to LINE error and status define
#define UART1_GetLinSTA()       (R8_UART1_LSR)          /* ��ȡ��ǰͨѶ״̬ */

#define	UART1_SendByte(b)		(R8_UART1_THR = b)		/* ���ڵ��ֽڷ��� */
void UART1_SendString( PUINT8 buf, UINT16 l );			/* ���ڶ��ֽڷ��� */
#define	UART1_RecvByte()		( R8_UART1_RBR )        /* ���ڶ�ȡ���ֽ� */
UINT16 UART1_RecvString( PUINT8 buf );					/* ���ڶ�ȡ���ֽ� */



/****************** UART2 */ 
void UART2_DefInit( void );	 							/* ����Ĭ�ϳ�ʼ������ */
void UART2_BaudRateCfg( UINT32 baudrate );	 			/* ���ڲ��������� */
void UART2_ByteTrigCfg( UARTByteTRIGTypeDef b );        /* �����ֽڴ����ж����� */
void UART2_INTCfg( FunctionalState s,  UINT8 i );		            /* �����ж����� */
void UART2_Reset( void );								/* ���������λ */

#define UART2_CLR_RXFIFO()       (R8_UART2_FCR |= RB_FCR_RX_FIFO_CLR)          /* �����ǰ����FIFO */
#define UART2_CLR_TXFIFO()       (R8_UART2_FCR |= RB_FCR_TX_FIFO_CLR)          /* �����ǰ����FIFO */

#define UART2_GetITFlag()       (R8_UART2_IIR&RB_IIR_INT_MASK)          /* ��ȡ��ǰ�жϱ�־ */
// please refer to LINE error and status define
#define UART2_GetLinSTA()       (R8_UART2_LSR)          /* ��ȡ��ǰͨѶ״̬ */

#define	UART2_SendByte(b)		(R8_UART2_THR = b)		/* ���ڵ��ֽڷ��� */
void UART2_SendString( PUINT8 buf, UINT16 l );			/* ���ڶ��ֽڷ��� */
#define	UART2_RecvByte()		( R8_UART2_RBR )        /* ���ڶ�ȡ���ֽ� */
UINT16 UART2_RecvString( PUINT8 buf );					/* ���ڶ�ȡ���ֽ� */




/****************** UART3 */ 
void UART3_DefInit( void );	 							/* ����Ĭ�ϳ�ʼ������ */
void UART3_BaudRateCfg( UINT32 baudrate );	 			/* ���ڲ��������� */
void UART3_ByteTrigCfg( UARTByteTRIGTypeDef b );        /* �����ֽڴ����ж����� */
void UART3_INTCfg( FunctionalState s,  UINT8 i );		            /* �����ж����� */
void UART3_Reset( void );								/* ���������λ */

#define UART3_CLR_RXFIFO()       (R8_UART3_FCR |= RB_FCR_RX_FIFO_CLR)          /* �����ǰ����FIFO */
#define UART3_CLR_TXFIFO()       (R8_UART3_FCR |= RB_FCR_TX_FIFO_CLR)          /* �����ǰ����FIFO */

#define UART3_GetITFlag()       (R8_UART3_IIR&RB_IIR_INT_MASK)          /* ��ȡ��ǰ�жϱ�־ */
// please refer to LINE error and status define
#define UART3_GetLinSTA()       (R8_UART3_LSR)          /* ��ȡ��ǰͨѶ״̬ */

#define	UART3_SendByte(b)		(R8_UART3_THR = b)		/* ���ڵ��ֽڷ��� */
void UART3_SendString( PUINT8 buf, UINT16 l );			/* ���ڶ��ֽڷ��� */
#define	UART3_RecvByte()		( R8_UART3_RBR )        /* ���ڶ�ȡ���ֽ� */
UINT16 UART3_RecvString( PUINT8 buf );					/* ���ڶ�ȡ���ֽ� */

	 
	 
#ifdef __cplusplus
}
#endif

#endif  // __CH58x_UART_H__

