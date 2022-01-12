/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_uart0.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"

/*******************************************************************************
* Function Name  : UART0_DefInit
* Description    : ����Ĭ�ϳ�ʼ������
* Input          : None
* Return         : None
*******************************************************************************/
void UART0_DefInit( void )
{	
    UART0_BaudRateCfg( 115200 );
    R8_UART0_FCR = (2<<6) | RB_FCR_TX_FIFO_CLR | RB_FCR_RX_FIFO_CLR | RB_FCR_FIFO_EN;		// FIFO�򿪣�������4�ֽ�
    R8_UART0_LCR = RB_LCR_WORD_SZ;	
    R8_UART0_IER = RB_IER_TXD_EN;
    R8_UART0_DIV = 1;	
}

/*******************************************************************************
* Function Name  : UART0_BaudRateCfg
* Description    : ���ڲ���������
* Input          : 
* Return         : 
*******************************************************************************/
void UART0_BaudRateCfg( UINT32 baudrate )
{
    UINT32	x;

    x = 10 * GetSysClock() / 8 / baudrate;
    x = ( x + 5 ) / 10;
    R16_UART0_DL = (UINT16)x;
}

/*******************************************************************************
* Function Name  : UART0_ByteTrigCfg
* Description    : �����ֽڴ����ж�����
* Input          : b: �����ֽ���
                    refer to UARTByteTRIGTypeDef
* Return         : 
*******************************************************************************/
void UART0_ByteTrigCfg( UARTByteTRIGTypeDef b )
{
    R8_UART0_FCR = (R8_UART0_FCR&~RB_FCR_FIFO_TRIG)|(b<<6);
}

/*******************************************************************************
* Function Name  : UART0_INTCfg
* Description    : �����ж�����
* Input          : s:  �жϿ���״̬
					ENABLE  - ʹ����Ӧ�ж�    
					DISABLE - �ر���Ӧ�ж�
				   i:  �ж�����
					RB_IER_MODEM_CHG  - ���ƽ��������״̬�仯�ж�ʹ��λ���� UART0 ֧�֣�
					RB_IER_LINE_STAT  - ������·״̬�ж�
					RB_IER_THR_EMPTY  - ���ͱ��ּĴ������ж�
					RB_IER_RECV_RDY   - ���������ж�
* Return         : None
*******************************************************************************/
void UART0_INTCfg( FunctionalState s,  UINT8 i )
{
    if( s )
    {
        R8_UART0_IER |= i;
        R8_UART0_MCR |= RB_MCR_INT_OE;
    }
    else
    {
        R8_UART0_IER &= ~i;
    }
}

/*******************************************************************************
* Function Name  : UART0_Reset
* Description    : ���������λ
* Input          : None
* Return         : None
*******************************************************************************/
void UART0_Reset( void )
{
    R8_UART0_IER = RB_IER_RESET;
}

/*******************************************************************************
* Function Name  : UART0_SendString
* Description    : ���ڶ��ֽڷ���
* Input          : buf - �����͵����������׵�ַ
                     l - �����͵����ݳ���
* Return         : None
*******************************************************************************/
void UART0_SendString( PUINT8 buf, UINT16 l )
{
    UINT16 len = l;

    while(len)
    {
        if(R8_UART0_TFC != UART_FIFO_SIZE)
        {
            R8_UART0_THR = *buf++;
            len--;
        }		
    }
}

/*******************************************************************************
* Function Name  : UART0_RecvString
* Description    : ���ڶ�ȡ���ֽ�
* Input          : buf - ��ȡ���ݴ�Ż������׵�ַ
* Return         : ��ȡ���ݳ���
*******************************************************************************/
UINT16 UART0_RecvString( PUINT8 buf )
{
    UINT16 len = 0;

    while( R8_UART0_RFC )
    {
        *buf++ = R8_UART0_RBR;
        len ++;
    }

    return (len);
}


