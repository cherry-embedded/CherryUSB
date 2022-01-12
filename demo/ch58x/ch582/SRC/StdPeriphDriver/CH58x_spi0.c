/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_SPI0.c
* Author             : WCH
* Version            : V1.0
* Date               : 2018/12/15
* Description 
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : SPI0_MasterDefInit
* Description    : ����ģʽĬ�ϳ�ʼ����ģʽ0+3��ȫ˫��+8MHz
* Input          : None			   				
* Return         : None
*******************************************************************************/
void SPI0_MasterDefInit( void )
{
    R8_SPI0_CLOCK_DIV = 4;		// ��Ƶʱ��4��Ƶ
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MOSI_OE | RB_SPI_SCK_OE ;
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;		   // ����BUFFER/FIFO�Զ����IF_BYTE_END��־
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;    // ������DMA��ʽ
}

/*******************************************************************************
* Function Name  : SPI0_CLKCfg
* Description    : SPI0 ��׼ʱ�����ã�= d*Tsys
* Input          : c: ʱ�ӷ�Ƶϵ��			   				
* Return         : None
*******************************************************************************/
void SPI0_CLKCfg( UINT8 c )
{
    if(c==2)
        R8_SPI0_CTRL_CFG |= RB_SPI_MST_DLY_EN;
    else 
        R8_SPI0_CTRL_CFG &= ~RB_SPI_MST_DLY_EN;
    R8_SPI0_CLOCK_DIV = c;
}

/*******************************************************************************
* Function Name  : SPI0_DataMode
* Description    : ����������ģʽ
* Input          : m: ������ģʽ
					refer to ModeBitOrderTypeDef
* Return         : None
*******************************************************************************/
void SPI0_DataMode( ModeBitOrderTypeDef m )
{
    switch( m )
    {
        case Mode0_LowBitINFront:
            R8_SPI0_CTRL_MOD &= ~RB_SPI_MST_SCK_MOD;
            R8_SPI0_CTRL_CFG |= RB_SPI_BIT_ORDER;
            break;
        case Mode0_HighBitINFront:
            R8_SPI0_CTRL_MOD &= ~RB_SPI_MST_SCK_MOD;
            R8_SPI0_CTRL_CFG &= ~RB_SPI_BIT_ORDER;
            break;
        case Mode3_LowBitINFront:
            R8_SPI0_CTRL_MOD |= RB_SPI_MST_SCK_MOD;
            R8_SPI0_CTRL_CFG |= RB_SPI_BIT_ORDER;
            break;
        case Mode3_HighBitINFront:
            R8_SPI0_CTRL_MOD |= RB_SPI_MST_SCK_MOD;
            R8_SPI0_CTRL_CFG &= ~RB_SPI_BIT_ORDER;
            break;
        default:
            break;
    }
}

/*******************************************************************************
* Function Name  : SPI0_MasterSendByte
* Description    : ���͵��ֽ� (buffer)
* Input          : d: �����ֽ�
* Return         : None
*******************************************************************************/
void SPI0_MasterSendByte( UINT8 d )
{
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R8_SPI0_BUFFER = d;
    while( !(R8_SPI0_INT_FLAG & RB_SPI_FREE) );	
}

/*******************************************************************************
* Function Name  : SPI0_MasterRecvByte
* Description    : ���յ��ֽ� (buffer)
* Input          : None
* Return         : ���յ����ֽ�
*******************************************************************************/
UINT8 SPI0_MasterRecvByte( void )
{
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;
    R8_SPI0_BUFFER = 0xFF;           // ��������
    while( !(R8_SPI0_INT_FLAG & RB_SPI_FREE) );
    return ( R8_SPI0_BUFFER );
}


/*******************************************************************************
* Function Name  : SPI0_MasterTrans
* Description    : ʹ��FIFO�������Ͷ��ֽ�
* Input          : pbuf: �����͵����������׵�ַ
                   len: �����͵����ݳ��ȣ����4095				   
* Return         : None
*******************************************************************************/
void SPI0_MasterTrans( UINT8 *pbuf, UINT16 len )
{
    UINT16 sendlen;

    sendlen = len;
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;                     // �������ݷ���Ϊ��� 
    R16_SPI0_TOTAL_CNT = sendlen;                             // ����Ҫ���͵����ݳ���	
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END;
    while( sendlen )
    {                                         
        if( R8_SPI0_FIFO_COUNT < SPI_FIFO_SIZE )
        {                              
            R8_SPI0_FIFO = *pbuf;
            pbuf++;
            sendlen--;
        }
    }
    while( R8_SPI0_FIFO_COUNT != 0 );                         // �ȴ�FIFO�е�����ȫ��������� 
}

/*******************************************************************************
* Function Name  : SPI0_MasterRecv
* Description    : ʹ��FIFO�������ն��ֽ�
* Input          : pbuf: �����͵����������׵�ַ
                   len: �����͵����ݳ��ȣ����4095
* Return         : None
*******************************************************************************/
void SPI0_MasterRecv( UINT8 *pbuf, UINT16 len )
{
    UINT16  readlen;

    readlen = len;
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;                     // �������ݷ���Ϊ���� 
    R16_SPI0_TOTAL_CNT = len;                                // ������Ҫ���յ����ݳ��ȣ�FIFO����Ϊ���볤�Ȳ�Ϊ0����������� */
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END;
    while( readlen )
    {                                                          
        if( R8_SPI0_FIFO_COUNT )
        {                                               
            *pbuf = R8_SPI0_FIFO;                                             
            pbuf++;
            readlen--;
        }
    }
}

/*******************************************************************************
* Function Name  : SPI0_MasterDMATrans
* Description    : DMA��ʽ������������  
* Input          : pbuf: ������������ʼ��ַ
*                  len : ���������ݳ���
* Return         : None
*******************************************************************************/
void SPI0_MasterDMATrans( PUINT8 pbuf, UINT16 len)
{                                    
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;     
    R16_SPI0_DMA_BEG = (UINT32)pbuf;                                           
    R16_SPI0_DMA_END = (UINT32)(pbuf + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END|RB_SPI_IF_DMA_END;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

/*******************************************************************************
* Function Name  : SPI0_MasterDMARecv
* Description    : DMA��ʽ������������  
* Input          : pbuf: ���������ݴ����ʼ��ַ
*                  len : ���������ݳ���
* Return         : None
*******************************************************************************/
void SPI0_MasterDMARecv( PUINT8 pbuf, UINT16 len)
{
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;                                      
    R16_SPI0_DMA_BEG = (UINT32)pbuf;                                           
    R16_SPI0_DMA_END = (UINT32)(pbuf + len);                                 
    R16_SPI0_TOTAL_CNT = len; 
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END|RB_SPI_IF_DMA_END;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}



/*******************************************************************************
* Function Name  : SPI0_SlaveInit
* Description    : �豸ģʽĬ�ϳ�ʼ������������MISO��GPIO��ӦΪ����ģʽ
* Input          : None
* Return         : None
*******************************************************************************/
void SPI0_SlaveInit( void )
{  
    R8_SPI0_CTRL_MOD = RB_SPI_ALL_CLEAR;
    R8_SPI0_CTRL_MOD = RB_SPI_MISO_OE | RB_SPI_MODE_SLAVE;	
    R8_SPI0_CTRL_CFG |= RB_SPI_AUTO_IF;
}

/*******************************************************************************
* Function Name  : SPI0_SlaveRecvByte
* Description    : �ӻ�ģʽ������һ�ֽ�����  
* Input          : None
* Return         : ���յ�����
*******************************************************************************/
UINT8 SPI0_SlaveRecvByte( void )
{
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
    while( R8_SPI0_FIFO_COUNT == 0 );          
    return R8_SPI0_FIFO; 
}

/*******************************************************************************
* Function Name  : SPI0_SlaveSendByte
* Description    : �ӻ�ģʽ������һ�ֽ�����  
* Input          : d -���������� 
* Return         : None
*******************************************************************************/
void SPI0_SlaveSendByte( UINT8 d )
{
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;                                              
    R8_SPI0_FIFO = d;                    
    while( R8_SPI0_FIFO_COUNT != 0 );               // �ȴ�������� 
}

/*******************************************************************************
* Function Name  : SPI0_SlaveRecv
* Description    : �ӻ�ģʽ�����ն��ֽ�����  
* Input          : pbuf: ���������ݴ����ʼ��ַ
*                  len : ����������ݳ���
* Return         : None
*******************************************************************************/
void SPI0_SlaveRecv( PUINT8 pbuf, UINT16 len )
{
    UINT16 revlen;
    
    revlen = len;
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END;
    while( revlen )
    {
        if( R8_SPI0_FIFO_COUNT )
        {            
            *pbuf = R8_SPI0_FIFO;
            pbuf++;
            revlen--;
        }
    }        
}

/*******************************************************************************
* Function Name  : SPI0_SlaveTrans
* Description    : �ӻ�ģʽ�����Ͷ��ֽ�����
* Input          : pbuf: �����͵����������׵�ַ
                   len: �����͵����ݳ��ȣ����4095				   
* Return         : None
*******************************************************************************/
void SPI0_SlaveTrans( UINT8 *pbuf, UINT16 len )
{
    UINT16 sendlen;

    sendlen = len;
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;                     // �������ݷ���Ϊ��� 
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END;
    while( sendlen )
    {                                         
        if( R8_SPI0_FIFO_COUNT < SPI_FIFO_SIZE )
        {                              
            R8_SPI0_FIFO = *pbuf;
            pbuf++;
            sendlen--;
        }
    }
    while( R8_SPI0_FIFO_COUNT != 0 );                         // �ȴ�FIFO�е�����ȫ��������� 
}

/*******************************************************************************
* Function Name  : SPI0_SlaveDMARecv
* Description    : DMA��ʽ������������  
* Input          : pbuf: ���������ݴ����ʼ��ַ
*                  len : ���������ݳ���
* Return         : None
*******************************************************************************/
void SPI0_SlaveDMARecv( PUINT8 pbuf, UINT16 len)
{
    R8_SPI0_CTRL_MOD |= RB_SPI_FIFO_DIR;                                      
    R16_SPI0_DMA_BEG = (UINT32)pbuf;                                           
    R16_SPI0_DMA_END = (UINT32)(pbuf + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END|RB_SPI_IF_DMA_END;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}

/*******************************************************************************
* Function Name  : SPI0_SlaveDMATrans
* Description    : DMA��ʽ������������  
* Input          : pbuf: ������������ʼ��ַ
*                  len : ���������ݳ���
* Return         : None
*******************************************************************************/
void SPI0_SlaveDMATrans( PUINT8 pbuf, UINT16 len)
{                                    
    R8_SPI0_CTRL_MOD &= ~RB_SPI_FIFO_DIR;     
    R16_SPI0_DMA_BEG = (UINT32)pbuf;                                           
    R16_SPI0_DMA_END = (UINT32)(pbuf + len);
    R16_SPI0_TOTAL_CNT = len;
    R8_SPI0_INT_FLAG = RB_SPI_IF_CNT_END|RB_SPI_IF_DMA_END;
    R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    while(!(R8_SPI0_INT_FLAG & RB_SPI_IF_CNT_END));
    R8_SPI0_CTRL_CFG &= ~RB_SPI_DMA_ENABLE;
}



