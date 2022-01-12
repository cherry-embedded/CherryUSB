


#ifndef __CH58x_I2C_H__
#define __CH58x_I2C_H__

#ifdef __cplusplus
 extern "C" {
#endif


/* I2C_transfer_direction */
#define  I2C_Direction_Transmitter      ((uint8_t)0x00)
#define  I2C_Direction_Receiver         ((uint8_t)0x01)

/* I2C ADD0 mask */
#define OADDR1_ADD0_Set                 ((uint16_t)0x0001)
#define OADDR1_ADD0_Reset               ((uint16_t)0xFFFE)

/* I2C_NACK_position */
#define I2C_NACKPosition_Next           ((uint16_t)RB_I2C_POS)
#define I2C_NACKPosition_Current        ((uint16_t)~RB_I2C_POS)

/* I2C_PEC_position */
#define I2C_PECPosition_Next            ((uint16_t)RB_I2C_POS)
#define I2C_PECPosition_Current         ((uint16_t)~RB_I2C_POS)

/* I2C_SMBus_alert_pin_level */
#define I2C_SMBusAlert_Low              ((uint16_t)RB_I2C_ALERT)
#define I2C_SMBusAlert_High             ((uint16_t)~RB_I2C_ALERT)

/* I2C FLAG mask */
#define FLAG_Mask                       ((uint32_t)0x00FFFFFF)

/* I2C Interrupt Enable mask */
#define ITEN_Mask                       ((uint32_t)0x07000000)

/* I2C_mode */
typedef enum
{
  I2C_Mode_I2C          = 0x0000,
  I2C_Mode_SMBusDevice  = 0x0002,
  I2C_Mode_SMBusHost    = 0x000A,
}I2C_ModeTypeDef;

/* I2C_duty_cycle_in_fast_mode */
typedef enum
{
  I2C_DutyCycle_16_9    =  RB_I2C_DUTY, /* I2C fast mode Tlow/Thigh = 16/9 */
  I2C_DutyCycle_2       =  0x0000, /* I2C fast mode Tlow/Thigh = 2 */
}I2C_DutyTypeDef;

/* I2C_acknowledgement - Enables or disables the acknowledgement.*/
typedef enum
{
  I2C_Ack_Enable        =  RB_I2C_ACK,
  I2C_Ack_Disable       =  0x0000,
}I2C_AckTypeDef;

/* I2C_acknowledged_address - Specifies if 7-bit or 10-bit address is acknowledged. */
typedef enum
{
  I2C_AckAddr_7bit      =  0x4000,
  I2C_AckAddr_10bit     =  0xC000,
}I2C_AckAddrTypeDef;

/* I2C_interrupts_definition */
typedef enum
{
  I2C_IT_BUF            =  0x0400, /* Buffer interrupt mask. */
  I2C_IT_EVT            =  0x0200, /* Event interrupt mask. */
  I2C_IT_ERR            =  0x0100, /* Error interrupt mask. */
}I2C_ITTypeDef;



/* I2C_interrupts_definition */
#define I2C_IT_SMBALERT                 ((uint32_t)0x01008000)
#define I2C_IT_TIMEOUT                  ((uint32_t)0x01004000)
#define I2C_IT_PECERR                   ((uint32_t)0x01001000)
#define I2C_IT_OVR                      ((uint32_t)0x01000800)
#define I2C_IT_AF                       ((uint32_t)0x01000400)
#define I2C_IT_ARLO                     ((uint32_t)0x01000200)
#define I2C_IT_BERR                     ((uint32_t)0x01000100)
#define I2C_IT_TXE                      ((uint32_t)0x06000080)
#define I2C_IT_RXNE                     ((uint32_t)0x06000040)
#define I2C_IT_STOPF                    ((uint32_t)0x02000010)
#define I2C_IT_ADD10                    ((uint32_t)0x02000008)
#define I2C_IT_BTF                      ((uint32_t)0x02000004)
#define I2C_IT_ADDR                     ((uint32_t)0x02000002)
#define I2C_IT_SB                       ((uint32_t)0x02000001)

/* SR2 register flags  */
#define I2C_FLAG_DUALF                  ((uint32_t)0x00800000)
#define I2C_FLAG_SMBHOST                ((uint32_t)0x00400000)
#define I2C_FLAG_SMBDEFAULT             ((uint32_t)0x00200000)
#define I2C_FLAG_GENCALL                ((uint32_t)0x00100000)
#define I2C_FLAG_TRA                    ((uint32_t)0x00040000)
#define I2C_FLAG_BUSY                   ((uint32_t)0x00020000)
#define I2C_FLAG_MSL                    ((uint32_t)0x00010000)

/* SR1 register flags */
#define I2C_FLAG_SMBALERT               ((uint32_t)0x10008000)
#define I2C_FLAG_TIMEOUT                ((uint32_t)0x10004000)
#define I2C_FLAG_PECERR                 ((uint32_t)0x10001000)
#define I2C_FLAG_OVR                    ((uint32_t)0x10000800)
#define I2C_FLAG_AF                     ((uint32_t)0x10000400)
#define I2C_FLAG_ARLO                   ((uint32_t)0x10000200)
#define I2C_FLAG_BERR                   ((uint32_t)0x10000100)
#define I2C_FLAG_TXE                    ((uint32_t)0x10000080)
#define I2C_FLAG_RXNE                   ((uint32_t)0x10000040)
#define I2C_FLAG_STOPF                  ((uint32_t)0x10000010)
#define I2C_FLAG_ADD10                  ((uint32_t)0x10000008)
#define I2C_FLAG_BTF                    ((uint32_t)0x10000004)
#define I2C_FLAG_ADDR                   ((uint32_t)0x10000002)
#define I2C_FLAG_SB                     ((uint32_t)0x10000001)

/****************I2C Master Events (Events grouped in order of communication)********************/

#define  I2C_EVENT_MASTER_MODE_SELECT                      ((uint32_t)0x00030001)  /* BUSY, MSL and SB flag */
#define  I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED        ((uint32_t)0x00070082)  /* BUSY, MSL, ADDR, TXE and TRA flags */
#define  I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED           ((uint32_t)0x00030002)  /* BUSY, MSL and ADDR flags */
#define  I2C_EVENT_MASTER_MODE_ADDRESS10                   ((uint32_t)0x00030008)  /* BUSY, MSL and ADD10 flags */
#define  I2C_EVENT_MASTER_BYTE_RECEIVED                    ((uint32_t)0x00030040)  /* BUSY, MSL and RXNE flags */
#define  I2C_EVENT_MASTER_BYTE_TRANSMITTING                ((uint32_t)0x00070080)  /* TRA, BUSY, MSL, TXE flags */
#define  I2C_EVENT_MASTER_BYTE_TRANSMITTED                 ((uint32_t)0x00070084)  /* TRA, BUSY, MSL, TXE and BTF flags */

/******************I2C Slave Events (Events grouped in order of communication)******************/

#define  I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED          ((uint32_t)0x00020002)  /* BUSY and ADDR flags */
#define  I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED       ((uint32_t)0x00060082)  /* TRA, BUSY, TXE and ADDR flags */
#define  I2C_EVENT_SLAVE_RECEIVER_SECONDADDRESS_MATCHED    ((uint32_t)0x00820000)  /* DUALF and BUSY flags */
#define  I2C_EVENT_SLAVE_TRANSMITTER_SECONDADDRESS_MATCHED ((uint32_t)0x00860080)  /* DUALF, TRA, BUSY and TXE flags */
#define  I2C_EVENT_SLAVE_GENERALCALLADDRESS_MATCHED        ((uint32_t)0x00120000)  /* GENCALL and BUSY flags */
#define  I2C_EVENT_SLAVE_BYTE_RECEIVED                     ((uint32_t)0x00020040)  /* BUSY and RXNE flags */
#define  I2C_EVENT_SLAVE_STOP_DETECTED                     ((uint32_t)0x00000010)  /* STOPF flag */
#define  I2C_EVENT_SLAVE_BYTE_TRANSMITTED                  ((uint32_t)0x00060084)  /* TRA, BUSY, TXE and BTF flags */
#define  I2C_EVENT_SLAVE_BYTE_TRANSMITTING                 ((uint32_t)0x00060080)  /* TRA, BUSY and TXE flags */
#define  I2C_EVENT_SLAVE_ACK_FAILURE                       ((uint32_t)0x00000400)  /* AF flag */

void I2C_Init( I2C_ModeTypeDef I2C_Mode, UINT32 I2C_ClockSpeed, I2C_DutyTypeDef I2C_DutyCycle,
               I2C_AckTypeDef I2C_Ack , I2C_AckAddrTypeDef I2C_AckAddr, UINT16 I2C_OwnAddress1 );
void I2C_Cmd( FunctionalState NewState);
void I2C_GenerateSTART( FunctionalState NewState);
void I2C_GenerateSTOP( FunctionalState NewState);
void I2C_AcknowledgeConfig( FunctionalState NewState);
void I2C_OwnAddress2Config( uint8_t Address);
void I2C_DualAddressCmd( FunctionalState NewState);
void I2C_GeneralCallCmd( FunctionalState NewState);
void I2C_ITConfig( I2C_ITTypeDef I2C_IT, FunctionalState NewState);
void I2C_SendData( uint8_t Data);
uint8_t I2C_ReceiveData( void );
void I2C_Send7bitAddress( uint8_t Address, uint8_t I2C_Direction);
void I2C_SoftwareResetCmd( FunctionalState NewState);
void I2C_NACKPositionConfig( uint16_t I2C_NACKPosition);
void I2C_SMBusAlertConfig( uint16_t I2C_SMBusAlert);
void I2C_TransmitPEC( FunctionalState NewState);
void I2C_PECPositionConfig( uint16_t I2C_PECPosition);
void I2C_CalculatePEC( FunctionalState NewState);
uint8_t I2C_GetPEC( void );
void I2C_ARPCmd( FunctionalState NewState);
void I2C_StretchClockCmd( FunctionalState NewState);
void I2C_FastModeDutyCycleConfig( uint16_t I2C_DutyCycle);

/****************************************************************************************
*                         I2C State Monitoring Functions
****************************************************************************************/
uint8_t I2C_CheckEvent( uint32_t I2C_EVENT);
uint32_t I2C_GetLastEvent( void );
FlagStatus I2C_GetFlagStatus( uint32_t I2C_FLAG);

void I2C_ClearFlag( uint32_t I2C_FLAG);
ITStatus I2C_GetITStatus( uint32_t I2C_IT);
void I2C_ClearITPendingBit( uint32_t I2C_IT);


#ifdef __cplusplus
}
#endif

#endif  // __CH58x_I2C_H__

