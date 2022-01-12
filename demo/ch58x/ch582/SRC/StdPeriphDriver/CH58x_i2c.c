/********************************** (C) COPYRIGHT *******************************
* File Name          : CH58x_i2c.c
* Author             : WCH
* Version            : V1.0
* Date               : 2021/03/15
* Description
*******************************************************************************/

#include "CH58x_common.h"


/*******************************************************************************
* Function Name  : I2C_Init
* Description    : Initializes the I2Cx peripheral according to the specified
*                  parameters in the I2C_InitStruct.
* Input          : I2C_Mode: refer to I2C_ModeTypeDef
*                  I2C_ClockSpeed:  Specifies the clock frequency(Hz).
*                    This parameter must be set to a value lower than 400kHz
*                  I2C_DutyCycle: Specifies the I2C fast mode duty cycle.
*                    refer to I2C_DutyTypeDef
*                  I2C_Ack : Enables or disables the acknowledgement.
*                    refer to I2C_AckTypeDef
*                  I2C_AckAddr: Specifies if 7-bit or 10-bit address is acknowledged.
*                    refer to I2C_AckAddrTypeDef
*                  I2C_OwnAddress1: Specifies the first device own address.
*                    This parameter can be a 7-bit or 10-bit address.
* Return         : None
*******************************************************************************/
void I2C_Init( I2C_ModeTypeDef I2C_Mode, UINT32 I2C_ClockSpeed, I2C_DutyTypeDef I2C_DutyCycle,
               I2C_AckTypeDef I2C_Ack , I2C_AckAddrTypeDef I2C_AckAddr, UINT16 I2C_OwnAddress1 )
{
  uint32_t sysClock;
  uint16_t tmpreg;

  sysClock = GetSysClock();

  R16_I2C_CTRL2 &= ~RB_I2C_FREQ;
  R16_I2C_CTRL2 |= (sysClock/1000000);

  R16_I2C_CTRL1 &= ~RB_I2C_PE;

  if (I2C_ClockSpeed <= 100000)
  {
    tmpreg = (sysClock / (I2C_ClockSpeed << 1)) & RB_I2C_CCR;

    if (tmpreg < 0x04)  tmpreg = 0x04;

    R16_I2C_RTR = (((sysClock/1000000) + 1) > 0x3F) ? 0x3F : ((sysClock/1000000) + 1);
  }
  else
  {
    if ( I2C_DutyCycle == I2C_DutyCycle_2 )
    {
      tmpreg = (sysClock / (I2C_ClockSpeed * 3)) & RB_I2C_CCR;
    }
    else
    {
      tmpreg = (sysClock / (I2C_ClockSpeed * 25)) & RB_I2C_CCR;
      tmpreg |= I2C_DutyCycle_16_9;
    }

    if ( tmpreg == 0 )
    {
      tmpreg |= (uint16_t)0x0001;
    }

    tmpreg |=  RB_I2C_F_S;
    R16_I2C_RTR = (uint16_t)((((sysClock/1000000) * (uint16_t)300) / (uint16_t)1000) + (uint16_t)1);

  }
  R16_I2C_CKCFGR = tmpreg;

  R16_I2C_CTRL1 |= RB_I2C_PE;

  R16_I2C_CTRL1 &= ~(RB_I2C_SMBUS|RB_I2C_SMBTYPE|RB_I2C_ACK);
  R16_I2C_CTRL1 |= I2C_Mode|I2C_Ack;

  R16_I2C_OADDR1 &= ~0xFFFF;
  R16_I2C_OADDR1 |= I2C_AckAddr|I2C_OwnAddress1;

}

/*******************************************************************************
* Function Name  : I2C_Cmd
* Description    : Enables or disables the specified I2C peripheral.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_Cmd( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL1 |= RB_I2C_PE;
  else                      R16_I2C_CTRL1 &= ~RB_I2C_PE;

}

/*******************************************************************************
* Function Name  : I2C_GenerateSTART
* Description    : Generates I2Cx communication START condition.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_GenerateSTART( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL1 |= RB_I2C_START;
  else                      R16_I2C_CTRL1 &= ~RB_I2C_START;
}

/*******************************************************************************
* Function Name  : I2C_GenerateSTOP
* Description    : Generates I2Cx communication STOP condition.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_GenerateSTOP( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL1 |= RB_I2C_STOP;
  else                      R16_I2C_CTRL1 &= RB_I2C_STOP;
}

/*******************************************************************************
* Function Name  : I2C_AcknowledgeConfig
* Description    : Enables or disables the specified I2C acknowledge feature.
* Input          : I2Cx: where x can be 1 or 2 to select the I2C peripheral.
*                  NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_AcknowledgeConfig( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL1 |= RB_I2C_ACK;
  else                      R16_I2C_CTRL1 &= RB_I2C_ACK;
}

/*******************************************************************************
* Function Name  : I2C_OwnAddress2Config
* Description    : Configures the specified I2C own address2.
* Input          : Address: specifies the 7bit I2C own address2.
* Return         : None
*******************************************************************************/
void I2C_OwnAddress2Config( uint8_t Address)
{
  R16_I2C_OADDR2 &= ~RB_I2C_ADD2;
  R16_I2C_OADDR2 |= (uint16_t)(Address & RB_I2C_ADD2);
}

/*******************************************************************************
* Function Name  : I2C_DualAddressCmd
* Description    : Enables or disables the specified I2C dual addressing mode.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_DualAddressCmd( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_OADDR2 |= RB_I2C_ENDUAL;
  else                      R16_I2C_OADDR2 &= ~RB_I2C_ENDUAL;
}

/*******************************************************************************
* Function Name  : I2C_GeneralCallCmd
* Description    : Enables or disables the specified I2C general call feature.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_GeneralCallCmd( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL1 |= RB_I2C_ENGC;
  else                      R16_I2C_CTRL1 &= ~RB_I2C_ENGC;
}

/*******************************************************************************
* Function Name  : I2C_ITConfig
* Description    : Enables or disables the specified I2C interrupts.
* Input          : I2C_IT: specifies the I2C interrupts sources to be enabled or disabled.
*                    I2C_IT_BUF: Buffer interrupt mask.
*                    I2C_IT_EVT: Event interrupt mask.
*                    I2C_IT_ERR: Error interrupt mask.
*                  NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_ITConfig( I2C_ITTypeDef I2C_IT, FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL2 |= I2C_IT;
  else                      R16_I2C_CTRL2 &= (uint16_t)~I2C_IT;
}

/*******************************************************************************
* Function Name  : I2C_SendData
* Description    : Sends a data byte through the I2Cx peripheral.
* Input          : Data: Byte to be transmitted.
* Return         : None
*******************************************************************************/
void I2C_SendData( uint8_t Data )
{
  R16_I2C_DATAR = Data;
}

/*******************************************************************************
* Function Name  : I2C_ReceiveData
* Description    : Returns the most recent received data by the I2Cx peripheral.
* Input          : None.
* Return         : The value of the received data.
*******************************************************************************/
uint8_t I2C_ReceiveData( void )
{
  return (uint8_t)R16_I2C_DATAR;
}

/*******************************************************************************
* Function Name  : I2C_Send7bitAddress
* Description    : Transmits the address byte to select the slave device.
* Input          : Address: specifies the slave address which will be transmitted.
*                  I2C_Direction: specifies whether the I2C device will be a
*      Transmitter or a Receiver.
*                    I2C_Direction_Transmitter: Transmitter mode.
*                    I2C_Direction_Receiver: Receiver mode.
* Return         : None
*******************************************************************************/
void I2C_Send7bitAddress( uint8_t Address, uint8_t I2C_Direction)
{
  if (I2C_Direction != I2C_Direction_Transmitter)     Address |= OADDR1_ADD0_Set;
  else                                                Address &= OADDR1_ADD0_Reset;

  R16_I2C_DATAR = Address;
}

/*******************************************************************************
* Function Name  : I2C_SoftwareResetCmd
* Description    : Enables or disables the specified I2C software reset.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_SoftwareResetCmd( FunctionalState NewState)
{
  if (NewState != DISABLE)  R16_I2C_CTRL1 |= RB_I2C_SWRST;
  else                      R16_I2C_CTRL1 &= ~RB_I2C_SWRST;
}

/*******************************************************************************
* Function Name  : I2C_NACKPositionConfig
* Description    : Selects the specified I2C NACK position in master receiver mode.
* Input          : I2C_NACKPosition: specifies the NACK position.
*                    I2C_NACKPosition_Next: indicates that the next byte will be
*      the last received byte.
*                    I2C_NACKPosition_Current: indicates that current byte is the
*      last received byte.
* Return         : None
*******************************************************************************/
void I2C_NACKPositionConfig( uint16_t I2C_NACKPosition)
{
  if (I2C_NACKPosition == I2C_NACKPosition_Next)  R16_I2C_CTRL1 |= I2C_NACKPosition_Next;
  else                                            R16_I2C_CTRL1 &= I2C_NACKPosition_Current;
}

/*******************************************************************************
* Function Name  : I2C_SMBusAlertConfig
* Description    : Drives the SMBusAlert pin high or low for the specified I2C.
* Input          : I2C_SMBusAlert: specifies SMBAlert pin level.
*                    I2C_SMBusAlert_Low: SMBAlert pin driven low.
*                    I2C_SMBusAlert_High: SMBAlert pin driven high.
* Return         : None
*******************************************************************************/
void I2C_SMBusAlertConfig( uint16_t I2C_SMBusAlert)
{
  if (I2C_SMBusAlert == I2C_SMBusAlert_Low)     R16_I2C_CTRL1 |= I2C_SMBusAlert_Low;
  else                                          R16_I2C_CTRL1 &= I2C_SMBusAlert_High;
}

/*******************************************************************************
* Function Name  : I2C_TransmitPEC
* Description    : Enables or disables the specified I2C PEC transfer.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_TransmitPEC( FunctionalState NewState)
{
  if (NewState != DISABLE)     R16_I2C_CTRL1 |= RB_I2C_PEC;
  else                         R16_I2C_CTRL1 &= ~RB_I2C_PEC;
}

/*******************************************************************************
* Function Name  : I2C_PECPositionConfig
* Description    : Selects the specified I2C PEC position.
* Input          : I2C_PECPosition: specifies the PEC position.
*                    I2C_PECPosition_Next: indicates that the next byte is PEC.
*                    I2C_PECPosition_Current: indicates that current byte is PEC.
* Return         : None
*******************************************************************************/
void I2C_PECPositionConfig( uint16_t I2C_PECPosition )
{
  if (I2C_PECPosition == I2C_PECPosition_Next)  R16_I2C_CTRL1 |= I2C_PECPosition_Next;
  else                                          R16_I2C_CTRL1 &= I2C_PECPosition_Current;
}

/*******************************************************************************
* Function Name  : I2C_CalculatePEC
* Description    : Enables or disables the PEC value calculation of the transferred bytes.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_CalculatePEC( FunctionalState NewState )
{
  if (NewState != DISABLE)    R16_I2C_CTRL1 |= RB_I2C_ENPEC;
  else                        R16_I2C_CTRL1 &= ~RB_I2C_ENPEC;
}

/*******************************************************************************
* Function Name  : I2C_GetPEC
* Description    : Returns the PEC value for the specified I2C.
* Input          : None.
* Return         : The PEC value.
*******************************************************************************/
uint8_t I2C_GetPEC( void )
{
  return (R16_I2C_STAR2 >> 8);
}

/*******************************************************************************
* Function Name  : I2C_ARPCmd
* Description    : Enables or disables the specified I2C ARP.
* Input          : NewState: ENABLE or DISABLE.
* Return         : The PEC value.
*******************************************************************************/
void I2C_ARPCmd( FunctionalState NewState)
{
  if (NewState != DISABLE)      R16_I2C_CTRL1 |= RB_I2C_EBARP;
  else                          R16_I2C_CTRL1 &= ~RB_I2C_EBARP;
}

/*******************************************************************************
* Function Name  : I2C_StretchClockCmd
* Description    : Enables or disables the specified I2C Clock stretching.
* Input          : NewState: ENABLE or DISABLE.
* Return         : None
*******************************************************************************/
void I2C_StretchClockCmd( FunctionalState NewState )
{
  if (NewState == DISABLE)      R16_I2C_CTRL1 |= RB_I2C_NOSTRETCH;
  else                          R16_I2C_CTRL1 &= ~RB_I2C_NOSTRETCH;
}

/*******************************************************************************
* Function Name  : I2C_FastModeDutyCycleConfig
* Description    : Selects the specified I2C fast mode duty cycle.
* Input          : I2C_DutyCycle: specifies the fast mode duty cycle.
*                    I2C_DutyCycle_2: I2C fast mode Tlow/Thigh = 2.
*                    I2C_DutyCycle_16_9: I2C fast mode Tlow/Thigh = 16/9.
* Return         : None
*******************************************************************************/
void I2C_FastModeDutyCycleConfig( uint16_t I2C_DutyCycle)
{
  if (I2C_DutyCycle != I2C_DutyCycle_16_9)    R16_I2C_CKCFGR &= ~I2C_DutyCycle_16_9;
  else                                        R16_I2C_CKCFGR |= I2C_DutyCycle_16_9;
}

/*******************************************************************************
* Function Name  : I2C_CheckEvent
* Description    : Checks whether the last I2Cx Event is equal to the one passed
*      as parameter.
* Input          : I2C_EVENT: specifies the event to be checked.
*                    I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED           : EV1.
*                    I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED              : EV1.
*                    I2C_EVENT_SLAVE_TRANSMITTER_SECONDADDRESS_MATCHED     : EV1.
*                    I2C_EVENT_SLAVE_RECEIVER_SECONDADDRESS_MATCHED        : EV1.
*                    I2C_EVENT_SLAVE_GENERALCALLADDRESS_MATCHED            : EV1.
*                    I2C_EVENT_SLAVE_BYTE_RECEIVED                         : EV2.
*                    (I2C_EVENT_SLAVE_BYTE_RECEIVED | I2C_FLAG_DUALF)      : EV2.
*                    (I2C_EVENT_SLAVE_BYTE_RECEIVED | I2C_FLAG_GENCALL)    : EV2.
*                    I2C_EVENT_SLAVE_BYTE_TRANSMITTED                      : EV3.
*                    (I2C_EVENT_SLAVE_BYTE_TRANSMITTED | I2C_FLAG_DUALF)   : EV3.
*                    (I2C_EVENT_SLAVE_BYTE_TRANSMITTED | I2C_FLAG_GENCALL) : EV3.
*                    I2C_EVENT_SLAVE_ACK_FAILURE                           : EV3_2.
*                    I2C_EVENT_SLAVE_STOP_DETECTED                         : EV4.
*                    I2C_EVENT_MASTER_MODE_SELECT                          : EV5.
*                    I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED            : EV6.
*                    I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED               : EV6.
*                    I2C_EVENT_MASTER_BYTE_RECEIVED                        : EV7.
*                    I2C_EVENT_MASTER_BYTE_TRANSMITTING                    : EV8.
*                    I2C_EVENT_MASTER_BYTE_TRANSMITTED                     : EV8_2.
*                    I2C_EVENT_MASTER_MODE_ADDRESS10                       : EV9.
* Return         : 1 - SUCCESS or 0 - ERROR.
*******************************************************************************/
uint8_t I2C_CheckEvent( uint32_t I2C_EVENT )
{
  uint32_t lastevent = 0;
  uint32_t flag1 = 0, flag2 = 0;
  uint8_t status = 0;

  flag1 = R16_I2C_STAR1;
  flag2 = R16_I2C_STAR2;
  flag2 = flag2 << 16;

  lastevent = (flag1 | flag2) & FLAG_Mask;

  if ((lastevent & I2C_EVENT) == I2C_EVENT)
  {
    status = !0;
  }
  else
  {
    status = 0;
  }

  return status;
}

/*******************************************************************************
* Function Name  : I2C_GetLastEvent
* Description    : Returns the last I2Cx Event.
* Input          : None.
* Return         : The last event.
*******************************************************************************/
uint32_t I2C_GetLastEvent( void )
{
  uint32_t lastevent = 0;
  uint32_t flag1 = 0, flag2 = 0;

  flag1 = R16_I2C_STAR1;
  flag2 = R16_I2C_STAR2;
  flag2 = flag2 << 16;
  lastevent = (flag1 | flag2) & FLAG_Mask;

  return lastevent;
}

/*******************************************************************************
* Function Name  : I2C_GetFlagStatus
* Description    : Checks whether the last I2Cx Event is equal to the one passed
*      as parameter.
* Input          : I2C_FLAG: specifies the flag to check.
*                    I2C_FLAG_DUALF: Dual flag (Slave mode).
*                    I2C_FLAG_SMBHOST: SMBus host header (Slave mode).
*                    I2C_FLAG_SMBDEFAULT: SMBus default header (Slave mode).
*                    I2C_FLAG_GENCALL: General call header flag (Slave mode).
*                    I2C_FLAG_TRA: Transmitter/Receiver flag.
*                    I2C_FLAG_BUSY: Bus busy flag.
*                    I2C_FLAG_MSL: Master/Slave flag.
*                    I2C_FLAG_SMBALERT: SMBus Alert flag.
*                    I2C_FLAG_TIMEOUT: Timeout or Tlow error flag.
*                    I2C_FLAG_PECERR: PEC error in reception flag.
*                    I2C_FLAG_OVR: Overrun/Underrun flag (Slave mode).
*                    I2C_FLAG_AF: Acknowledge failure flag.
*                    I2C_FLAG_ARLO: Arbitration lost flag (Master mode).
*                    I2C_FLAG_BERR: Bus error flag.
*                    I2C_FLAG_TXE: Data register empty flag (Transmitter).
*                    I2C_FLAG_RXNE: Data register not empty (Receiver) flag.
*                    I2C_FLAG_STOPF: Stop detection flag (Slave mode).
*                    I2C_FLAG_ADD10: 10-bit header sent flag (Master mode).
*                    I2C_FLAG_BTF: Byte transfer finished flag.
*                    I2C_FLAG_ADDR: Address sent flag (Master mode) "ADSL"
*      Address matched flag (Slave mode)"ENDA".
*                    I2C_FLAG_SB: Start bit flag (Master mode).
* Return         : FlagStatus: SET or RESET.
*******************************************************************************/
FlagStatus I2C_GetFlagStatus( uint32_t I2C_FLAG)
{
  FlagStatus bitstatus = RESET;
  __IO uint32_t i2creg = 0, i2cxbase = 0;

  i2cxbase = (uint32_t)BA_I2C;
  i2creg = I2C_FLAG >> 28;
  I2C_FLAG &= FLAG_Mask;

  if(i2creg != 0)
  {
    i2cxbase += 0x14;
  }
  else
  {
    I2C_FLAG = (uint32_t)(I2C_FLAG >> 16);
    i2cxbase += 0x18;
  }

  if(((*(__IO uint32_t *)i2cxbase) & I2C_FLAG) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }

  return  bitstatus;
}

/*******************************************************************************
* Function Name  : I2C_ClearFlag
* Description    : Clears the I2Cx's pending flags.
* Input          : I2C_FLAG: specifies the flag to clear.
*                    I2C_FLAG_SMBALERT: SMBus Alert flag.
*                    I2C_FLAG_TIMEOUT: Timeout or Tlow error flag.
*                    I2C_FLAG_PECERR: PEC error in reception flag.
*                    I2C_FLAG_OVR: Overrun/Underrun flag (Slave mode).
*                    I2C_FLAG_AF: Acknowledge failure flag.
*                    I2C_FLAG_ARLO: Arbitration lost flag (Master mode).
*                    I2C_FLAG_BERR: Bus error flag.
* Return         : None
*******************************************************************************/
void I2C_ClearFlag( uint32_t I2C_FLAG )
{
  uint32_t flagpos = 0;

  flagpos = I2C_FLAG & FLAG_Mask;
  R16_I2C_STAR1 = (uint16_t)~flagpos;
}

/*******************************************************************************
* Function Name  : I2C_GetITStatus
* Description    : Checks whether the specified I2C interrupt has occurred or not.
* Input          : II2C_IT: specifies the interrupt source to check.
*                    I2C_IT_SMBALERT: SMBus Alert flag.
*                    I2C_IT_TIMEOUT: Timeout or Tlow error flag.
*                    I2C_IT_PECERR: PEC error in reception flag.
*                    I2C_IT_OVR: Overrun/Underrun flag (Slave mode).
*                    I2C_IT_AF: Acknowledge failure flag.
*                    I2C_IT_ARLO: Arbitration lost flag (Master mode).
*                    I2C_IT_BERR: Bus error flag.
*                    I2C_IT_TXE: Data register empty flag (Transmitter).
*                    I2C_IT_RXNE: Data register not empty (Receiver) flag.
*                    I2C_IT_STOPF: Stop detection flag (Slave mode).
*                    I2C_IT_ADD10: 10-bit header sent flag (Master mode).
*                    I2C_IT_BTF: Byte transfer finished flag.
*                    I2C_IT_ADDR: Address sent flag (Master mode) "ADSL"  Address matched
*      flag (Slave mode)"ENDAD".
*                    I2C_IT_SB: Start bit flag (Master mode).
* Return         : None
*******************************************************************************/
ITStatus I2C_GetITStatus( uint32_t I2C_IT)
{
  ITStatus bitstatus = RESET;
  uint32_t enablestatus = 0;

  enablestatus = (uint32_t)(((I2C_IT & ITEN_Mask) >> 16) & (R16_I2C_CTRL2)) ;
  I2C_IT &= FLAG_Mask;

  if (((R16_I2C_STAR1 & I2C_IT) != (uint32_t)RESET) && enablestatus)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }

  return  bitstatus;
}

/*******************************************************************************
* Function Name  : I2C_ClearITPendingBit
* Description    : Clears the I2Cx interrupt pending bits.
* Input          : I2C_IT: specifies the interrupt pending bit to clear.
*                    I2C_IT_SMBALERT: SMBus Alert interrupt.
*                    I2C_IT_TIMEOUT: Timeout or Tlow error interrupt.
*                    I2C_IT_PECERR: PEC error in reception  interrupt.
*                    I2C_IT_OVR: Overrun/Underrun interrupt (Slave mode).
*                    I2C_IT_AF: Acknowledge failure interrupt.
*                    I2C_IT_ARLO: Arbitration lost interrupt (Master mode).
*                    I2C_IT_BERR: Bus error interrupt.
* Return         : None
*******************************************************************************/
void I2C_ClearITPendingBit( uint32_t I2C_IT )
{
  uint32_t flagpos = 0;

  flagpos = I2C_IT & FLAG_Mask;
  R16_I2C_STAR1 = (uint16_t)~flagpos;
}


