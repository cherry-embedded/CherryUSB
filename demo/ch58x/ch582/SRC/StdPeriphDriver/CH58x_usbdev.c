/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_usbdev.c
* Author             : WCH
* Version            : V1.0
* Date               : 2021/03/10
* Description 
*******************************************************************************/

#include "CH58x_common.h"

PUINT8  pEP0_RAM_Addr;
PUINT8  pEP1_RAM_Addr;
PUINT8  pEP2_RAM_Addr;
PUINT8  pEP3_RAM_Addr;

/*******************************************************************************
* Function Name  : USB_DeviceInit
* Description    : USB�豸���ܳ�ʼ����4���˵㣬8��ͨ����
* Input          : None			   				
* Return         : None
*******************************************************************************/
void USB_DeviceInit( void )                                    
{	
    R8_USB_CTRL = 0x00;					// ���趨ģʽ,ȡ�� RB_UC_CLR_ALL

    R8_UEP4_1_MOD = RB_UEP4_RX_EN|RB_UEP4_TX_EN|RB_UEP1_RX_EN|RB_UEP1_TX_EN;    // �˵�4 OUT+IN,�˵�1 OUT+IN
    R8_UEP2_3_MOD = RB_UEP2_RX_EN|RB_UEP2_TX_EN|RB_UEP3_RX_EN|RB_UEP3_TX_EN;    // �˵�2 OUT+IN,�˵�3 OUT+IN

    R16_UEP0_DMA = (UINT16)(UINT32)pEP0_RAM_Addr;
    R16_UEP1_DMA = (UINT16)(UINT32)pEP1_RAM_Addr;
    R16_UEP2_DMA = (UINT16)(UINT32)pEP2_RAM_Addr;
    R16_UEP3_DMA = (UINT16)(UINT32)pEP3_RAM_Addr;

    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_UEP4_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                                
    R8_USB_DEV_AD = 0x00;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;  // ����USB�豸��DMA�����ж��ڼ��жϱ�־δ���ǰ�Զ�����NAK
    R16_PIN_ANALOG_IE |= RB_PIN_USB_IE|RB_PIN_USB_DP_PU;		// ��ֹUSB�˿ڸ��ռ���������
    R8_USB_INT_FG = 0xFF;                                           // ���жϱ�־
    R8_UDEV_CTRL = RB_UD_PD_DIS|RB_UD_PORT_EN; 						// ����USB�˿�	
    R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;
}

/*******************************************************************************
* Function Name  : DevEP1_IN_Deal
* Description    : �˵�1�����ϴ�
* Input          : l: �ϴ����ݳ���(<64B)			   				
* Return         : None
*******************************************************************************/
void DevEP1_IN_Deal( UINT8 l )
{
    R8_UEP1_T_LEN = l;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

/*******************************************************************************
* Function Name  : DevEP2_IN_Deal
* Description    : �˵�2�����ϴ�
* Input          : l: �ϴ����ݳ���(<64B)			   				
* Return         : None
*******************************************************************************/
void DevEP2_IN_Deal( UINT8 l )
{
    R8_UEP2_T_LEN = l;
    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

/*******************************************************************************
* Function Name  : DevEP3_IN_Deal
* Description    : �˵�3�����ϴ�
* Input          : l: �ϴ����ݳ���(<64B)			   				
* Return         : None
*******************************************************************************/
void DevEP3_IN_Deal( UINT8 l )
{
    R8_UEP3_T_LEN = l;
    R8_UEP3_CTRL = (R8_UEP3_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

/*******************************************************************************
* Function Name  : DevEP4_IN_Deal
* Description    : �˵�4�����ϴ�
* Input          : l: �ϴ����ݳ���(<64B)			   				
* Return         : None
*******************************************************************************/
void DevEP4_IN_Deal( UINT8 l )
{
    R8_UEP4_T_LEN = l;
    R8_UEP4_CTRL = (R8_UEP4_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

