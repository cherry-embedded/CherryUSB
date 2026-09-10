/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USB_USBFS_REG_H
#define USB_USBFS_REG_H

#ifdef __cplusplus
extern "C" {
#endif

/* @include */
#include <stdint.h>

/*******************************************************************************/
/* USBFS Related Register Macro Definition */

/* R8_USB_CTRL */
#define USBFS_UC_HOST_MODE     0x80
#define USBFS_UC_LOW_SPEED     0x40
#define USBFS_UC_DEV_PU_EN     0x20
#define USBFS_UC_SYS_CTRL_MASK 0x30
#define USBFS_UC_SYS_CTRL0     0x00
#define USBFS_UC_SYS_CTRL1     0x10
#define USBFS_UC_SYS_CTRL2     0x20
#define USBFS_UC_SYS_CTRL3     0x30
#define USBFS_UC_INT_BUSY      0x08
#define USBFS_UC_RESET_SIE     0x04
#define USBFS_UC_CLR_ALL       0x02
#define USBFS_UC_DMA_EN        0x01

/* R8_USB_INT_EN */
#define USBFS_UIE_DEV_SOF  0x80
#define USBFS_UIE_DEV_NAK  0x40
#define USBFS_U_1WIRE_MODE 0x20
#define USBFS_UIE_FIFO_OV  0x10
#define USBFS_UIE_HST_SOF  0x08
#define USBFS_UIE_SUSPEND  0x04
#define USBFS_UIE_TRANSFER 0x02
#define USBFS_UIE_DETECT   0x01
#define USBFS_UIE_BUS_RST  0x01

/* R8_USB_DEV_AD */
#define USBFS_UDA_GP_BIT    0x80
#define USBFS_USB_ADDR_MASK 0x7F

/* R8_USB_MIS_ST */
#define USBFS_UMS_SOF_PRES   0x80
#define USBFS_UMS_SOF_ACT    0x40
#define USBFS_UMS_SIE_FREE   0x20
#define USBFS_UMS_R_FIFO_RDY 0x10
#define USBFS_UMS_BUS_RESET  0x08
#define USBFS_UMS_SUSPEND    0x04
#define USBFS_UMS_DM_LEVEL   0x02
#define USBFS_UMS_DEV_ATTACH 0x01

/* R8_USB_INT_FG */
#define USBFS_U_IS_NAK     0x80 // RO, indicate current USB transfer is NAK received
#define USBFS_U_TOG_OK     0x40 // RO, indicate current USB transfer toggle is OK
#define USBFS_U_SIE_FREE   0x20 // RO, indicate USB SIE free status
#define USBFS_UIF_FIFO_OV  0x10 // FIFO overflow interrupt flag for USB, direct bit address clear or write 1 to clear
#define USBFS_UIF_HST_SOF  0x08 // host SOF timer interrupt flag for USB host, direct bit address clear or write 1 to clear
#define USBFS_UIF_SUSPEND  0x04 // USB suspend or resume event interrupt flag, direct bit address clear or write 1 to clear
#define USBFS_UIF_TRANSFER 0x02 // USB transfer completion interrupt flag, direct bit address clear or write 1 to clear
#define USBFS_UIF_DETECT   0x01 // device detected event interrupt flag for USB host mode, direct bit address clear or write 1 to clear
#define USBFS_UIF_BUS_RST  0x01 // bus reset event interrupt flag for USB device mode, direct bit address clear or write 1 to clear

/* R8_USB_INT_ST */
#define USBFS_UIS_IS_NAK      0x80 // RO, indicate current USB transfer is NAK received for USB device mode
#define USBFS_UIS_TOG_OK      0x40 // RO, indicate current USB transfer toggle is OK
#define USBFS_UIS_TOKEN_MASK  0x30 // RO, bit mask of current token PID code received for USB device mode
#define USBFS_UIS_TOKEN_OUT   0x00
#define USBFS_UIS_TOKEN_SOF   0x10
#define USBFS_UIS_TOKEN_IN    0x20
#define USBFS_UIS_TOKEN_SETUP 0x30
// bUIS_TOKEN1 & bUIS_TOKEN0: current token PID code received for USB device mode
//   00: OUT token PID received
//   01: SOF token PID received
//   10: IN token PID received
//   11: SETUP token PID received
#define USBFS_UIS_ENDP_MASK  0x0F // RO, bit mask of current transfer endpoint number for USB device mode
#define USBFS_UIS_H_RES_MASK 0x0F // RO, bit mask of current transfer handshake response for USB host mode: 0000=no response, time out from device, others=handshake response PID received

/* R32_USB_OTG_CR */
#define USBFS_CR_SESS_VTH     0x20
#define USBFS_CR_VBUS_VTH     0x10
#define USBFS_CR_OTG_EN       0x08
#define USBFS_CR_IDPU         0x04
#define USBFS_CR_CHARGE_VBUS  0x02
#define USBFS_CR_DISCHAR_VBUS 0x01

/* R32_USB_OTG_SR */
#define USBFS_SR_ID_DIG   0x08
#define USBFS_SR_SESS_END 0x04
#define USBFS_SR_SESS_VLD 0x02
#define USBFS_SR_VBUS_VLD 0x01

/* R8_UDEV_CTRL */
#define USBFS_UD_PD_DIS    0x80 // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define USBFS_UD_DP_PIN    0x20 // ReadOnly: indicate current UDP pin level
#define USBFS_UD_DM_PIN    0x10 // ReadOnly: indicate current UDM pin level
#define USBFS_UD_LOW_SPEED 0x04 // enable USB physical port low speed: 0=full speed, 1=low speed
#define USBFS_UD_GP_BIT    0x02 // general purpose bit
#define USBFS_UD_PORT_EN   0x01 // enable USB physical port I/O: 0=disable, 1=enable

/* R8_UEP4_1_MOD */
#define USBFS_UEP1_RX_EN   0x80 // enable USB endpoint 1 receiving (OUT)
#define USBFS_UEP1_TX_EN   0x40 // enable USB endpoint 1 transmittal (IN)
#define USBFS_UEP1_BUF_MOD 0x10 // buffer mode of USB endpoint 1
#define USBFS_UEP4_RX_EN   0x08 // enable USB endpoint 4 receiving (OUT)
#define USBFS_UEP4_TX_EN   0x04 // enable USB endpoint 4 transmittal (IN)
#define USBFS_UEP4_BUF_MOD 0x01

/* R8_UEP2_3_MOD */
#define USBFS_UEP3_RX_EN   0x80 // enable USB endpoint 3 receiving (OUT)
#define USBFS_UEP3_TX_EN   0x40 // enable USB endpoint 3 transmittal (IN)
#define USBFS_UEP3_BUF_MOD 0x10 // buffer mode of USB endpoint 3
#define USBFS_UEP2_RX_EN   0x08 // enable USB endpoint 2 receiving (OUT)
#define USBFS_UEP2_TX_EN   0x04 // enable USB endpoint 2 transmittal (IN)
#define USBFS_UEP2_BUF_MOD 0x01 // buffer mode of USB endpoint 2

/* R8_UEP5_6_MOD */
#define USBFS_UEP6_RX_EN   0x80 // enable USB endpoint 6 receiving (OUT)
#define USBFS_UEP6_TX_EN   0x40 // enable USB endpoint 6 transmittal (IN)
#define USBFS_UEP6_BUF_MOD 0x10 // buffer mode of USB endpoint 6
#define USBFS_UEP5_RX_EN   0x08 // enable USB endpoint 5 receiving (OUT)
#define USBFS_UEP5_TX_EN   0x04 // enable USB endpoint 5 transmittal (IN)
#define USBFS_UEP5_BUF_MOD 0x01 // buffer mode of USB endpoint 5

/* R8_UEP7_MOD */
#define USBFS_UEP7_RX_EN   0x08 // enable USB endpoint 7 receiving (OUT)
#define USBFS_UEP7_TX_EN   0x04 // enable USB endpoint 7 transmittal (IN)
#define USBFS_UEP7_BUF_MOD 0x01 // buffer mode of USB endpoint 7

/* R8_UEPn_TX_CTRL */
#define USBFS_UEP_T_AUTO_TOG  0x08 // enable automatic toggle after successful transfer completion on endpoint 1/2/3: 0=manual toggle, 1=automatic toggle
#define USBFS_UEP_T_TOG       0x04 // prepared data toggle flag of USB endpoint X transmittal (IN): 0=DATA0, 1=DATA1
#define USBFS_UEP_T_RES_MASK  0x03 // bit mask of handshake response type for USB endpoint X transmittal (IN)
#define USBFS_UEP_T_RES_ACK   0x00
#define USBFS_UEP_T_RES_NONE  0x01
#define USBFS_UEP_T_RES_NAK   0x02
#define USBFS_UEP_T_RES_STALL 0x03
// bUEP_T_RES1 & bUEP_T_RES0: handshake response type for USB endpoint X transmittal (IN)
//   00: DATA0 or DATA1 then expecting ACK (ready)
//   01: DATA0 or DATA1 then expecting no response, time out from host, for non-zero endpoint isochronous transactions
//   10: NAK (busy)
//   11: STALL (error)
// host aux setup

/* R8_UEPn_RX_CTRL, n=0-7 */
#define USBFS_UEP_R_AUTO_TOG  0x08 // enable automatic toggle after successful transfer completion on endpoint 1/2/3: 0=manual toggle, 1=automatic toggle
#define USBFS_UEP_R_TOG       0x04 // expected data toggle flag of USB endpoint X receiving (OUT): 0=DATA0, 1=DATA1
#define USBFS_UEP_R_RES_MASK  0x03 // bit mask of handshake response type for USB endpoint X receiving (OUT)
#define USBFS_UEP_R_RES_ACK   0x00
#define USBFS_UEP_R_RES_NONE  0x01
#define USBFS_UEP_R_RES_NAK   0x02
#define USBFS_UEP_R_RES_STALL 0x03
// RB_UEP_R_RES1 & RB_UEP_R_RES0: handshake response type for USB endpoint X receiving (OUT)
//   00: ACK (ready)
//   01: no response, time out to host, for non-zero endpoint isochronous transactions
//   10: NAK (busy)
//   11: STALL (error)

/* R8_UHOST_CTRL */
#define USBFS_UH_PD_DIS    0x80 // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define USBFS_UH_DP_PIN    0x20 // ReadOnly: indicate current UDP pin level
#define USBFS_UH_DM_PIN    0x10 // ReadOnly: indicate current UDM pin level
#define USBFS_UH_LOW_SPEED 0x04 // enable USB port low speed: 0=full speed, 1=low speed
#define USBFS_UH_BUS_RESET 0x02 // control USB bus reset: 0=normal, 1=force bus reset
#define USBFS_UH_PORT_EN   0x01 // enable USB port: 0=disable, 1=enable port, automatic disabled if USB device detached

/* R32_UH_EP_MOD */
#define USBFS_UH_EP_TX_EN    0x40 // enable USB host OUT endpoint transmittal
#define USBFS_UH_EP_TBUF_MOD 0x10 // buffer mode of USB host OUT endpoint
// bUH_EP_TX_EN & bUH_EP_TBUF_MOD: USB host OUT endpoint buffer mode, buffer start address is UH_TX_DMA
//   0 x:  disable endpoint and disable buffer
//   1 0:  64 bytes buffer for transmittal (OUT endpoint)
//   1 1:  dual 64 bytes buffer by toggle bit bUH_T_TOG selection for transmittal (OUT endpoint), total=128bytes
#define USBFS_UH_EP_RX_EN    0x08 // enable USB host IN endpoint receiving
#define USBFS_UH_EP_RBUF_MOD 0x01 // buffer mode of USB host IN endpoint
// bUH_EP_RX_EN & bUH_EP_RBUF_MOD: USB host IN endpoint buffer mode, buffer start address is UH_RX_DMA
//   0 x:  disable endpoint and disable buffer
//   1 0:  64 bytes buffer for receiving (IN endpoint)
//   1 1:  dual 64 bytes buffer by toggle bit bUH_R_TOG selection for receiving (IN endpoint), total=128bytes

/* R16_UH_SETUP */
#define USBFS_UH_PRE_PID_EN 0x0400 // USB host PRE PID enable for low speed device via hub
#define USBFS_UH_SOF_EN     0x0004 // USB host automatic SOF enable

/* R8_UH_EP_PID */
#define USBFS_UH_TOKEN_MASK 0xF0 // bit mask of token PID for USB host transfer
#define USBFS_UH_ENDP_MASK  0x0F // bit mask of endpoint number for USB host transfer

/* R8_UH_RX_CTRL */
#define USBFS_UH_R_AUTO_TOG 0x08 // enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle
#define USBFS_UH_R_TOG      0x04 // expected data toggle flag of host receiving (IN): 0=DATA0, 1=DATA1
#define USBFS_UH_R_RES      0x01 // prepared handshake response type for host receiving (IN): 0=ACK (ready), 1=no response, time out to device, for isochronous transactions

/* R8_UH_TX_CTRL */
#define USBFS_UH_T_AUTO_TOG 0x08 // enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle
#define USBFS_UH_T_TOG      0x04 // prepared data toggle flag of host transmittal (SETUP/OUT): 0=DATA0, 1=DATA1
#define USBFS_UH_T_RES      0x01 // expected handshake response type for host transmittal (SETUP/OUT): 0=ACK (ready), 1=no response, time out from device, for isochronous transactions

/* IO definitions */
#ifdef __cplusplus
#define __I volatile
#else
#define __I volatile const
#endif

#define __O  volatile
#define __IO volatile

/* USBFS Device Registers */
typedef struct
{
    __IO uint8_t BASE_CTRL;
    __IO uint8_t UDEV_CTRL;
    __IO uint8_t INT_EN;
    __IO uint8_t DEV_ADDR;
    __IO uint8_t Reserve0;
    __IO uint8_t MIS_ST;
    __IO uint8_t INT_FG;
    __IO uint8_t INT_ST;
    __IO uint32_t RX_LEN;
    __IO uint8_t UEP4_1_MOD;
    __IO uint8_t UEP2_3_MOD;
    __IO uint8_t UEP5_6_MOD;
    __IO uint8_t UEP7_MOD;
    __IO uint32_t UEP0_DMA;
    __IO uint32_t UEP1_DMA;
    __IO uint32_t UEP2_DMA;
    __IO uint32_t UEP3_DMA;
    __IO uint32_t UEP4_DMA;
    __IO uint32_t UEP5_DMA;
    __IO uint32_t UEP6_DMA;
    __IO uint32_t UEP7_DMA;
    __IO uint16_t UEP0_TX_LEN;
    union {
        __IO uint16_t UEP0_CTRL;
        struct {
            __IO uint8_t UEP0_TX_CTRL;
            __IO uint8_t UEP0_RX_CTRL;
        };
    };
    __IO uint16_t UEP1_TX_LEN;
    union {
        __IO uint16_t UEP1_CTRL;
        struct {
            __IO uint8_t UEP1_TX_CTRL;
            __IO uint8_t UEP1_RX_CTRL;
        };
    };
    __IO uint16_t UEP2_TX_LEN;
    union {
        __IO uint16_t UEP2_CTRL;
        struct {
            __IO uint8_t UEP2_TX_CTRL;
            __IO uint8_t UEP2_RX_CTRL;
        };
    };
    __IO uint16_t UEP3_TX_LEN;
    union {
        __IO uint16_t UEP3_CTRL;
        struct {
            __IO uint8_t UEP3_TX_CTRL;
            __IO uint8_t UEP3_RX_CTRL;
        };
    };
    __IO uint16_t UEP4_TX_LEN;
    union {
        __IO uint16_t UEP4_CTRL;
        struct {
            __IO uint8_t UEP4_TX_CTRL;
            __IO uint8_t UEP4_RX_CTRL;
        };
    };
    __IO uint16_t UEP5_TX_LEN;
    union {
        __IO uint16_t UEP5_CTRL;
        struct {
            __IO uint8_t UEP5_TX_CTRL;
            __IO uint8_t UEP5_RX_CTRL;
        };
    };
    __IO uint16_t UEP6_TX_LEN;
    union {
        __IO uint16_t UEP6_CTRL;
        struct {
            __IO uint8_t UEP6_TX_CTRL;
            __IO uint8_t UEP6_RX_CTRL;
        };
    };
    __IO uint16_t UEP7_TX_LEN;
    union {
        __IO uint16_t UEP7_CTRL;
        struct {
            __IO uint8_t UEP7_TX_CTRL;
            __IO uint8_t UEP7_RX_CTRL;
        };
    };
    __IO uint32_t Reserve1;
    __IO uint32_t OTG_CR;
    __IO uint32_t OTG_SR;
} USBFSD_TypeDef;

/* USBFS Host Registers */
typedef struct
{
    __IO uint8_t BASE_CTRL;
    __IO uint8_t HOST_CTRL;
    __IO uint8_t INT_EN;
    __IO uint8_t DEV_ADDR;
    __IO uint8_t Reserve0;
    __IO uint8_t MIS_ST;
    __IO uint8_t INT_FG;
    __IO uint8_t INT_ST;
    __IO uint16_t RX_LEN;
    __IO uint16_t Reserve1;
    __IO uint8_t Reserve2;
    __IO uint8_t HOST_EP_MOD;
    __IO uint16_t Reserve3;
    __IO uint32_t Reserve4;
    __IO uint32_t Reserve5;
    __IO uint32_t HOST_RX_DMA;
    __IO uint32_t HOST_TX_DMA;
    __IO uint32_t Reserve6;
    __IO uint32_t Reserve7;
    __IO uint32_t Reserve8;
    __IO uint32_t Reserve9;
    __IO uint32_t Reserve10;
    __IO uint16_t Reserve11;
    __IO uint16_t HOST_SETUP;
    __IO uint8_t HOST_EP_PID;
    __IO uint8_t Reserve12;
    __IO uint8_t Reserve13;
    __IO uint8_t HOST_RX_CTRL;
    __IO uint16_t HOST_TX_LEN;
    __IO uint8_t HOST_TX_CTRL;
    __IO uint8_t Reserve14;
    __IO uint32_t Reserve15;
    __IO uint32_t Reserve16;
    __IO uint32_t Reserve17;
    __IO uint32_t Reserve18;
    __IO uint32_t Reserve19;
    __IO uint32_t OTG_CR;
    __IO uint32_t OTG_SR;
} USBFSH_TypeDef;

#ifdef __cplusplus
}
#endif

#endif
