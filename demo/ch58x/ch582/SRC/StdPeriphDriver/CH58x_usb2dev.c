/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_usb2dev.c
* Author             : WCH
* Version            : V1.0
* Date               : 2021/03/10
* Description 
*******************************************************************************/

#include "CH58x_common.h"

PUINT8  pU2EP0_RAM_Addr;
PUINT8  pU2EP1_RAM_Addr;
PUINT8  pU2EP2_RAM_Addr;
PUINT8  pU2EP3_RAM_Addr;

/*******************************************************************************
* Function Name  : USB2_DeviceInit
* Description    : USB2设备功能初始化，4个端点，8个通道。
* Input          : None
* Return         : None
*******************************************************************************/
void USB2_DeviceInit( void )
{
    R8_USB2_CTRL = 0x00;         // 先设定模式,取消 RB_UC_CLR_ALL

    R8_U2EP4_1_MOD = RB_UEP4_RX_EN|RB_UEP4_TX_EN|RB_UEP1_RX_EN|RB_UEP1_TX_EN;    // 端点4 OUT+IN,端点1 OUT+IN
    R8_U2EP2_3_MOD = RB_UEP2_RX_EN|RB_UEP2_TX_EN|RB_UEP3_RX_EN|RB_UEP3_TX_EN;    // 端点2 OUT+IN,端点3 OUT+IN

    R16_U2EP0_DMA = (UINT16)(UINT32)pU2EP0_RAM_Addr;
    R16_U2EP1_DMA = (UINT16)(UINT32)pU2EP1_RAM_Addr;
    R16_U2EP2_DMA = (UINT16)(UINT32)pU2EP2_RAM_Addr;
    R16_U2EP3_DMA = (UINT16)(UINT32)pU2EP3_RAM_Addr;

    R8_U2EP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_U2EP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_U2EP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_U2EP3_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
    R8_U2EP4_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;

    R8_USB2_DEV_AD = 0x00;
    R8_USB2_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;  // 启动USB设备及DMA，在中断期间中断标志未清除前自动返回NAK
    R16_PIN_ANALOG_IE |= RB_PIN_USB2_IE|RB_PIN_USB2_DP_PU;    // 防止USB端口浮空及上拉电阻
    R8_USB2_INT_FG = 0xFF;                                           // 清中断标志
    R8_U2DEV_CTRL = RB_UD_PD_DIS|RB_UD_PORT_EN;            // 允许USB端口
    R8_USB2_INT_EN = RB_UIE_SUSPEND | RB_UIE_BUS_RST | RB_UIE_TRANSFER;
}

/*******************************************************************************
* Function Name  : U2DevEP1_IN_Deal
* Description    : U2端点1数据上传
* Input          : l: 上传数据长度(<64B)
* Return         : None
*******************************************************************************/
void U2DevEP1_IN_Deal( UINT8 l )
{
    R8_U2EP1_T_LEN = l;
    R8_U2EP1_CTRL = (R8_U2EP1_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

/*******************************************************************************
* Function Name  : U2DevEP2_IN_Deal
* Description    : U2端点2数据上传
* Input          : l: 上传数据长度(<64B)
* Return         : None
*******************************************************************************/
void U2DevEP2_IN_Deal( UINT8 l )
{
    R8_U2EP2_T_LEN = l;
    R8_U2EP2_CTRL = (R8_U2EP2_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

/*******************************************************************************
* Function Name  : U2DevEP3_IN_Deal
* Description    : U2端点3数据上传
* Input          : l: 上传数据长度(<64B)
* Return         : None
*******************************************************************************/
void U2DevEP3_IN_Deal( UINT8 l )
{
    R8_U2EP3_T_LEN = l;
    R8_U2EP3_CTRL = (R8_U2EP3_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

/*******************************************************************************
* Function Name  : U2DevEP4_IN_Deal
* Description    : U2端点4数据上传
* Input          : l: 上传数据长度(<64B)
* Return         : None
*******************************************************************************/
void U2DevEP4_IN_Deal( UINT8 l )
{
    R8_U2EP4_T_LEN = l;
    R8_U2EP4_CTRL = (R8_U2EP4_CTRL & ~MASK_UEP_T_RES)| UEP_T_RES_ACK;
}

