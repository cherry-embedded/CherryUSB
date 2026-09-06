/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef USB_USBHS_REG_H
#define USB_USBHS_REG_H

#ifdef __cplusplus
extern "C" {
#endif

/* @include */
#include <stdint.h>

/*******************************************************************************/
/* USBHS Related Register Macro Definition */

/* R8_USB_CTRL */
#define  USBHS_UD_LPM_EN            0x80
#define  USBHS_UD_DEV_EN            0x20
#define  USBHS_UD_DMA_EN            0x10
#define  USBHS_UD_PHY_SUSPENDM      0x08
#define  USBHS_UD_CLR_ALL           0x04
#define  USBHS_UD_RST_SIE           0x02
#define  USBHS_UD_RST_LINK          0x01

/* R8_USB_BASE_MODE */
#define  USBHS_UD_SPEED_FULL        0x00
#define  USBHS_UD_SPEED_HIGH        0x01
#define  USBHS_UD_SPEED_LOW         0x02
#define  USBHS_UD_SPEED_TYPE        0x03

/* R8_USB_INT_EN */
#define  USBHS_UDIE_FIFO_OVER       0x80
#define  USBHS_UDIE_LINK_RDY        0x40
#define  USBHS_UDIE_SOF_ACT         0x20
#define  USBHS_UDIE_TRANSFER        0x10
#define  USBHS_UDIE_LPM_ACT         0x08
#define  USBHS_UDIE_BUS_SLEEP       0x04
#define  USBHS_UDIE_SUSPEND         0x02
#define  USBHS_UDIE_BUS_RST         0x01

/* R8_USB_DEV_AD */
#define  USBHS_UD_DEV_ADDR          0x7F

/* R8_USB_WAKE_CTRL */
#define  USBHS_UD_REMOTE_WKUP       0x01

/* R8_USB_TEST_MODE */
#define  USBHS_UD_TEST_EN           0x80
#define  USBHS_UD_TEST_SE0NAK       0x08
#define  USBHS_UD_TEST_PKT          0x04
#define  USBHS_UD_TEST_K            0x02
#define  USBHS_UD_TEST_J            0x01

/* R16_USB_LPM_DATA */
#define  USBHS_UD_LPM_BUSY          0x8000
#define  USBHS_UD_LPM_DATA          0x07FF

/* R8_USB_INT_FG */
#define  USBHS_UDIF_FIFO_OV         0x80
#define  USBHS_UDIF_LINK_RDY        0x40
#define  USBHS_UDIF_RX_SOF          0x20
#define  USBHS_UDIF_TRANSFER        0x10
#define  USBHS_UDIF_RTX_ACT         0x10
#define  USBHS_UDIF_LPM_ACT         0x08
#define  USBHS_UDIF_BUS_SLEEP       0x04
#define  USBHS_UDIF_SUSPEND         0x02
#define  USBHS_UDIF_BUS_RST         0x01

/* R8_USB_INT_ST */
#define  USBHS_UDIS_EP_DIR          0x10
#define  USBHS_UDIS_EP_ID_MASK      0x07

/* R8_USB_MIS_ST */
#define  USBHS_UDMS_HS_MOD          0x80
#define  USBHS_UDMS_SUSP_REQ        0x10
#define  USBHS_UDMS_SIE_FREE        0x08
#define  USBHS_UDMS_SLEEP           0x04
#define  USBHS_UDMS_SUSPEND         0x02
#define  USBHS_UDMS_READY           0x01

/* R16_USB_FRAME_NO */
#define  USBHS_UD_MFRAME_NO         0xE000
#define  USBHS_UD_FRAME_NO          0x07FF

/* R16_USB_BUS */
#define  USBHS_USB_DM_ST            0x08
#define  USBHS_USB_DP_ST            0x04
#define  USB_WAKEUP                 0x01

/* R16_UEP_TX_EN */
#define USBHS_UEP0_T_EN             0x0001
#define USBHS_UEP1_T_EN             0x0002
#define USBHS_UEP2_T_EN             0x0004
#define USBHS_UEP3_T_EN             0x0008
#define USBHS_UEP4_T_EN             0x0010
#define USBHS_UEP5_T_EN             0x0020
#define USBHS_UEP6_T_EN             0x0040
#define USBHS_UEP7_T_EN             0x0080
#define USBHS_UEP8_T_EN             0x0100
#define USBHS_UEP9_T_EN             0x0200
#define USBHS_UEP10_T_EN            0x0400
#define USBHS_UEP11_T_EN            0x0800
#define USBHS_UEP12_T_EN            0x1000
#define USBHS_UEP13_T_EN            0x2000
#define USBHS_UEP14_T_EN            0x4000
#define USBHS_UEP15_T_EN            0x8000

/* R16_UEP_RX_EN */
#define USBHS_UEP0_R_EN             0x0001
#define USBHS_UEP1_R_EN             0x0002
#define USBHS_UEP2_R_EN             0x0004
#define USBHS_UEP3_R_EN             0x0008
#define USBHS_UEP4_R_EN             0x0010
#define USBHS_UEP5_R_EN             0x0020
#define USBHS_UEP6_R_EN             0x0040
#define USBHS_UEP7_R_EN             0x0080
#define USBHS_UEP8_R_EN             0x0100
#define USBHS_UEP9_R_EN             0x0200
#define USBHS_UEP10_R_EN            0x0400
#define USBHS_UEP11_R_EN            0x0800
#define USBHS_UEP12_R_EN            0x1000
#define USBHS_UEP13_R_EN            0x2000
#define USBHS_UEP14_R_EN            0x4000
#define USBHS_UEP15_R_EN            0x8000

/* R16_UEP_T_TOG_AUTO */
#define USBHS_UEP_T_TOG_AUTO        0xFF

/* R16_UEP_R_TOG_AUTO */
#define USBHS_UEP_R_TOG_AUTO        0xFF

/* R8_UEP_T_BURST */
#define USBHS_UEP_T_BURST_EN        0xFF

/* R8_UEP_T_BURST_MODE */
#define USBHS_UEP_T_BURST_MODE      0xFF

/* R8_UEP_R_BURST */
#define USBHS_UEP_R_BURST_EN        0xFF

/* R8_UEP_R_RES_MODE */
#define USBHS_UEP_R_RES_MODE        0xFF

/* R32_UEP_AF_MODE */
#define USBHS_UEP_T_AF              0xFE

/* R32_UEP0_DMA */
#define UEPn_DMA                    0xFFFFFF

/* R32_UEPn_RX_DMA */
#define UEPn_RX_DMA                 0xFFFFFF

/* R32_UEPn_TX_DMA */
#define UEPn_TX_DMA                 0xFFFFFF

/* R32_UEPn_MAX_LEN */
#define USBHS_UEP0_MAX_LEN          0x007F
#define USBHS_UEPn_MAX_LEN          0x07FF

/* R16_UEPn_RX_LEN */
#define USBHS_UEP0_RX_LEN           0x007F

/* R16_UEPn_RX_LEN */
#define USBHS_UEPn_RX_LEN           0xFFFF

/* R16_UEPn_R_SIZE */
#define USBHS_UEPn_R_SIZE           0xFFFF

/* R16_UEP0_T_LEN */
#define USBHS_UEP0_T_LEN            0x7F

/**R16_UEPn_T_LEN**/
#define USBHS_UEPn_T_LEN            0xFFFF

/**R8_UEPn_TX_CTRL**/
#define USBHS_UEP_T_DONE            0x80
#define USBHS_UEP_T_NAK_ACT         0x40
#define USBHS_UEP_T_TOG_MASK        0x0C
#define USBHS_UEP_T_TOG_MDATA       0x0C
#define USBHS_UEP_T_TOG_DATA2       0x08
#define USBHS_UEP_T_TOG_DATA1       0x04
#define USBHS_UEP_T_TOG_DATA0       0x00
#define USBHS_UEP_T_RES_MASK        0x03
#define USBHS_UEP_T_RES_ACK         0x02
#define USBHS_UEP_T_RES_STALL       0x01
#define USBHS_UEP_T_RES_NAK         0x00

/**R8_UEP0_RX_CTRL**/

/**R8_UEPn_RX_CTRL**/
#define USBHS_UEP_R_DONE            0x80
#define USBHS_UEP_R_NAK_ACT         0x40
#define USBHS_UEP_R_NAK_TOG         0x20
#define USBHS_UEP_R_TOG_MATCH       0x10
#define USBHS_UEP_R_SETUP_IS        0x08
#define USBHS_UEP_R_TOG_MASK        0x0C
#define USBHS_UEP_R_TOG_MDATA       0x0C
#define USBHS_UEP_R_TOG_DATA2       0x08
#define USBHS_UEP_R_TOG_DATA1       0x04
#define USBHS_UEP_R_TOG_DATA0       0x00
#define USBHS_UEP_R_RES_MASK        0x03
#define USBHS_UEP_R_RES_ACK         0x02
#define USBHS_UEP_R_RES_STALL       0x01
#define USBHS_UEP_R_RES_NAK         0x00

/* R16_UEP_T_ISO */
#define  USBHS_UEP1_T_FIFO_EN       0x0200
#define  USBHS_UEP2_T_FIFO_EN       0x0400
#define  USBHS_UEP3_T_FIFO_EN       0x0800
#define  USBHS_UEP4_T_FIFO_EN       0x1000
#define  USBHS_UEP5_T_FIFO_EN       0x2000
#define  USBHS_UEP6_T_FIFO_EN       0x4000
#define  USBHS_UEP7_T_FIFO_EN       0x8000
#define  USBHS_UEP1_T_ISO_EN        0x0002
#define  USBHS_UEP2_T_ISO_EN        0x0004
#define  USBHS_UEP3_T_ISO_EN        0x0008
#define  USBHS_UEP4_T_ISO_EN        0x0010
#define  USBHS_UEP5_T_ISO_EN        0x0020
#define  USBHS_UEP6_T_ISO_EN        0x0040
#define  USBHS_UEP7_T_ISO_EN        0x0080

/* R16_UEP_R_ISO */
#define  USBHS_UEP1_R_FIFO_EN       0x0200
#define  USBHS_UEP2_R_FIFO_EN       0x0400
#define  USBHS_UEP3_R_FIFO_EN       0x0800
#define  USBHS_UEP4_R_FIFO_EN       0x1000
#define  USBHS_UEP5_R_FIFO_EN       0x2000
#define  USBHS_UEP6_R_FIFO_EN       0x4000
#define  USBHS_UEP7_R_FIFO_EN       0x8000
#define  USBHS_UEP1_R_ISO_EN        0x0002
#define  USBHS_UEP2_R_ISO_EN        0x0004
#define  USBHS_UEP3_R_ISO_EN        0x0008
#define  USBHS_UEP4_R_ISO_EN        0x0010
#define  USBHS_UEP5_R_ISO_EN        0x0020
#define  USBHS_UEP6_R_ISO_EN        0x0040
#define  USBHS_UEP7_R_ISO_EN        0x0080

/* R32_UEPn_RX_FIFO */
#define  USBHS_UEP_RX_FIFO_E        0xFF00
#define  USBHS_UEP_RX_FIFO_S        0x00FF

/* R32_UEPn_TX_FIFO */
#define  USBHS_UEP_TX_FIFO_E        0xFF00
#define  USBHS_UEP_TX_FIFO_S        0x00FF

/* USB high speed host register  */
/* R8_UH_CFG */
#define  USBHS_UH_LPM_EN            0x80
#define  USBHS_UH_FORCE_FS          0x40
#define  USBHS_UH_SOF_EN            0x20
#define  USBHS_UH_DMA_EN            0x10
#define  USBHS_UH_PHY_SUSPENDM      0x08
#define  USBHS_UH_CLR_ALL           0x04
#define  USBHS_RST_SIE              0x02
#define  USBHS_RST_LINK             0x01

/* R8_UH_INT_EN */
#define  USBHS_UHIE_FIFO_OVER       0x80
#define  USBHS_UHIE_TX_HALT         0x40
#define  USBHS_UHIE_SOF_ACT         0x20
#define  USBHS_UHIE_TRANSFER        0x10
#define  USBHS_UHIE_RESUME_ACT      0x08
#define  USBHS_UHIE_WKUP_ACT        0x04

/* R8_UH_DEV_AD */
#define  USBHS_UH_DEV_ADDR          0x7F

/* R32_UH_CONTROL */
#define  USBHS_UH_RX_NO_RES         0x800000
#define  USBHS_UH_TX_NO_RES         0x400000
#define  USBHS_UH_RX_NO_DATA        0x200000
#define  USBHS_UH_TX_NO_DATA        0x100000
#define  USBHS_UH_PRE_PID_EN        0x080000
#define  USBHS_UH_SPLIT_VALID       0x040000
#define  USBHS_UH_LPM_VALID         0x020000
#define  USBHS_UH_HOST_ACTION       0x010000
#define  USBHS_UH_BUF_MODE          0x0400
#define  USBHS_UH_T_TOG_MASK        0x0300
#define  USBHS_UH_T_TOG_MDATA       0x0300
#define  USBHS_UH_T_TOG_DATA2       0x0200
#define  USBHS_UH_T_TOG_DATA1       0x0100
#define  USBHS_UH_T_TOG_DATA0       0x0000
#define  USBHS_UH_T_ENDP_MASK       0xF0
#define  USBHS_UH_T_TOKEN_MASK      0x0F

/* R8_UH_INT_FLAG */
#define  USBHS_UHIF_FIFO_OVER       0x80
#define  USBHS_UHIF_TX_HALT         0x40
#define  USBHS_UHIF_SOF_ACT         0x20
#define  USBHS_UHIF_TRANSFER        0x10
#define  USBHS_UHIF_RESUME_ACT      0x08
#define  USBHS_UHIF_WKUP_ACT        0x04

/* R8_UH_INT_ST */
#define  USBHS_UHIF_PORT_RX_RESUME  0x10
#define  USBHS_UH_R_TOKEN_MASK      0x0F

/* R8_UH_MIS_ST */
#define  USBHS_UHMS_BUS_SE0         0x80
#define  USBHS_UHMS_BUS_J           0x40
#define  USBHS_UHMS_LINESTATE       0x30
#define  USBHS_UHMS_USB_WAKEUP      0x08
#define  USBHS_UHMS_SOF_ACT         0x04
#define  USBHS_UHMS_SOF_PRE         0x02
#define  USBHS_UHMS_SOF_FREE        0x01

/* R32_UH_LPM_DATA */
#define  USBHS_UH_LPM_DATA          0x07FF

/* R32_UH_SPLIT_DATA */
#define  USBHS_UH_SPLIT_DATA        0x07FFFF

/* R32_UH_FRAME */
#define  USBHS_UH_SOF_CNT_CLR       0x02000000
#define  USBHS_UH_SOF_CNT_EN        0x01000000
#define  USBHS_UH_MFRAME_NO         0x070000
#define  USBHS_UH_FRAME_NO          0x07FF

/* R32_UH_TX_LEN */
#define  USBHS_UH_TX_LEN            0x07FF

/* R32_UH_RX_LEN */
#define  USBHS_UH_RX_LEN            0x07FF

/* R32_UH_RX_MAX_LEN */
#define  USBHS_UH_RX_MAX_LEN        0x07FF

/* R32_UH_RX_DMA */
#define  USBHS_R32_UH_RX_DMA        0x01FFFF

/* R32_UH_TX_DMA */
#define  USBHS_R32_UH_TX_DMA        0x01FFFF

/* R32_UH_PORT_CTRL */
#define  USBHS_UH_BUS_RST_LONG      0x010000
#define  USBHS_UH_PORT_SLEEP_BESL   0xF000
#define  USBHS_UH_CLR_PORT_SLEEP    0x0100
#define  USBHS_UH_CLR_PORT_CONNECT  0x20
#define  USBHS_UH_CLR_PORT_EN       0x10
#define  USBHS_UH_SET_PORT_SLEEP    0x08
#define  USBHS_UH_CLR_PORT_SUSP     0x04
#define  USBHS_UH_SET_PORT_SUSP     0x02
#define  USBHS_UH_SET_PORT_RESET    0x01

/* R8_UH_PORT_CFG */
#define  USBHS_UH_PD_EN             0x80
#define  USBHS_UH_HOST_EN           0x01

/* R8_UH_PORT_INT_EN */
#define  USBHS_UHIE_PORT_SLP        0x20
#define  USBHS_UHIE_PORT_RESET      0x10
#define  USBHS_UHIE_PORT_SUSP       0x04
#define  USBHS_UHIE_PORT_EN         0x02
#define  USBHS_UHIE_PORT_CONNECT    0x01

/* R8_UH_PORT_TEST_CT */
#define  USBHS_UH_TEST_SE0_NAK      0x10
#define  USBHS_UH_TEST_PACKET       0x08
#define  USBHS_UH_TEST_FORCE_EN     0x04
#define  USBHS_UH_TEST_K            0x02
#define  USBHS_UH_TEST_J            0x01

/* R16_UH_PORT_ST */
#define  USBHS_UHIS_PORT_TEST       0x0800
#define  USBHS_UHIS_PORT_SPEED_MASK 0x0600
#define  USBHS_UHIS_PORT_HS         0x0400
#define  USBHS_UHIS_PORT_LS         0x0200
#define  USBHS_UHIS_PORT_FS         0x0000
#define  USBHS_UHIS_PORT_SLP        0x20
#define  USBHS_UHIS_PORT_RST        0x10
#define  USBHS_UHIS_PORT_SUSP       0x04
#define  USBHS_UHIS_PORT_EN         0x02
#define  USBHS_UHIS_PORT_CONNECT    0x01

/* R8_UH_PORT_CHG */
#define  USBHS_UHIF_PORT_SLP        0x20
#define  USBHS_UHIF_PORT_RESET      0x10
#define  USBHS_UHIF_PORT_SUSP       0x04
#define  USBHS_UHIF_PORT_EN         0x02
#define  USBHS_UHIF_PORT_CONNECT    0x01

/* R32_UH_BC_CTRL */
#define  UDM_VSRC_ACT               0x0400
#define  UDM_BC_VSRC                0x0200
#define  UDP_BC_VSRC                0x0100
#define  BC_AUTO_MODE               0x40
#define  UDM_BC_CMPE                0x20
#define  UDP_BC_CMPE                0x10
#define  UDM_BC_CMPO                0x02
#define  UDP_BC_CMPO                0x01

/* IO definitions */
#ifdef __cplusplus
#define __I                         volatile
#else
#define __I                         volatile const
#endif

#define __O                         volatile
#define __IO                        volatile

/* USBHS Device Registers */
typedef struct
{
    __IO uint8_t  CONTROL;
    __IO uint8_t  BASE_MODE;
    __IO uint8_t  INT_EN;
    __IO uint8_t  DEV_AD;
    __IO uint8_t  WAKE_CTRL;
    __IO uint8_t  TEST_MODE;
    __IO uint16_t LPM_DATA;

    __IO uint8_t  INT_FG;
    __IO uint8_t  INT_ST;
    __IO uint8_t  MIS_ST;
    uint8_t  RESERVED0;

    __IO uint16_t FRAME_NO;
    __IO uint16_t BUS;

    __IO uint16_t UEP_TX_EN;
    __IO uint16_t UEP_RX_EN;
    __IO uint16_t UEP_TX_TOG_AUTO;
    __IO uint16_t UEP_RX_TOG_AUTO;

    __IO uint8_t  UEP_TX_BURST;
    __IO uint8_t  UEP_TX_BURST_MODE;
    __IO uint8_t  UEP_RX_BURST;
    __IO uint8_t  UEP_RX_RES_MODE;

    __IO uint32_t UEP_AF_MODE;
    __IO uint32_t UEP0_DMA;
    __IO uint32_t UEP1_RX_DMA;
    __IO uint32_t UEP2_RX_DMA;
    __IO uint32_t UEP3_RX_DMA;
    __IO uint32_t UEP4_RX_DMA;
    __IO uint32_t UEP5_RX_DMA;
    __IO uint32_t UEP6_RX_DMA;
    __IO uint32_t UEP7_RX_DMA;
    __IO uint32_t UEP1_TX_DMA;
    __IO uint32_t UEP2_TX_DMA;
    __IO uint32_t UEP3_TX_DMA;
    __IO uint32_t UEP4_TX_DMA;
    __IO uint32_t UEP5_TX_DMA;
    __IO uint32_t UEP6_TX_DMA;
    __IO uint32_t UEP7_TX_DMA;
    __IO uint32_t UEP0_MAX_LEN;
    __IO uint32_t UEP1_MAX_LEN;
    __IO uint32_t UEP2_MAX_LEN;
    __IO uint32_t UEP3_MAX_LEN;
    __IO uint32_t UEP4_MAX_LEN;
    __IO uint32_t UEP5_MAX_LEN;
    __IO uint32_t UEP6_MAX_LEN;
    __IO uint32_t UEP7_MAX_LEN;

    __IO uint16_t UEP0_RX_LEN;
    uint16_t RESERVED1;
    __IO uint16_t UEP1_RX_LEN;
    __IO uint16_t UEP1_RX_SIZE;
    __IO uint16_t UEP2_RX_LEN;
    __IO uint16_t UEP2_RX_SIZE;
    __IO uint16_t UEP3_RX_LEN;
    __IO uint16_t UEP3_RX_SIZE;
    __IO uint16_t UEP4_RX_LEN;
    __IO uint16_t UEP4_RX_SIZE;
    __IO uint16_t UEP5_RX_LEN;
    __IO uint16_t UEP5_RX_SIZE;
    __IO uint16_t UEP6_RX_LEN;
    __IO uint16_t UEP6_RX_SIZE;
    __IO uint16_t UEP7_RX_LEN;
    __IO uint16_t UEP7_RX_SIZE;
    __IO uint16_t UEP0_TX_LEN;
    __IO uint8_t  UEP0_TX_CTRL;
    __IO uint8_t  UEP0_RX_CTRL;

    __IO uint16_t UEP1_TX_LEN;
    __IO uint8_t  UEP1_TX_CTRL;
    __IO uint8_t  UEP1_RX_CTRL;
    __IO uint16_t UEP2_TX_LEN;
    __IO uint8_t  UEP2_TX_CTRL;
    __IO uint8_t  UEP2_RX_CTRL;
    __IO uint16_t UEP3_TX_LEN;
    __IO uint8_t  UEP3_TX_CTRL;
    __IO uint8_t  UEP3_RX_CTRL;
    __IO uint16_t UEP4_TX_LEN;
    __IO uint8_t  UEP4_TX_CTRL;
    __IO uint8_t  UEP4_RX_CTRL;
    __IO uint16_t UEP5_TX_LEN;
    __IO uint8_t  UEP5_TX_CTRL;
    __IO uint8_t  UEP5_RX_CTRL;
    __IO uint16_t UEP6_TX_LEN;
    __IO uint8_t  UEP6_TX_CTRL;
    __IO uint8_t  UEP6_RX_CTRL;
    __IO uint16_t UEP7_TX_LEN;
    __IO uint8_t  UEP7_TX_CTRL;
    __IO uint8_t  UEP7_RX_CTRL;

    __IO uint16_t UEP_TX_ISO;
    __IO uint16_t UEP_RX_ISO;

    __IO uint32_t UEP1_RX_FIFO;
    __IO uint32_t UEP2_RX_FIFO;
    __IO uint32_t UEP3_RX_FIFO;
    __IO uint32_t UEP4_RX_FIFO;
    __IO uint32_t UEP5_RX_FIFO;
    __IO uint32_t UEP6_RX_FIFO;
    __IO uint32_t UEP7_RX_FIFO;
    __IO uint32_t UEP1_TX_FIFO;
    __IO uint32_t UEP2_TX_FIFO;
    __IO uint32_t UEP3_TX_FIFO;
    __IO uint32_t UEP4_TX_FIFO;
    __IO uint32_t UEP5_TX_FIFO;
    __IO uint32_t UEP6_TX_FIFO;
    __IO uint32_t UEP7_TX_FIFO;
} USBHSD_TypeDef;

/* USBHS Host Registers */
typedef struct  __attribute__((packed))
{
    __IO uint8_t  CFG;
    uint8_t  RESERVED0;
    __IO uint8_t  INT_EN;
    __IO uint8_t  DEV_ADDR;
    __IO uint32_t CONTROL;

    __IO uint8_t  INT_FLAG;
    __IO uint8_t  INT_ST;
    __IO uint8_t  MIS_ST;
    uint8_t  RESERVED1;

    __IO uint32_t LPM;
    __IO uint32_t SPLIT;
    __IO uint32_t FRAME;
    __IO uint32_t TX_LEN;
    __IO uint32_t RX_LEN;
    __IO uint32_t RX_MAX_LEN;
    __IO uint32_t RX_DMA;
    __IO uint32_t TX_DMA;
    __IO uint32_t PORT_CTRL;
    __IO uint8_t  PORT_CFG;
    uint8_t  RESERVED2;
    __IO uint8_t  PORT_INT_EN;
    __IO uint8_t  PORT_TEST_CT;

    __IO uint16_t PORT_STATUS;
    __IO uint8_t  PORT_STATUS_CHG;
    uint8_t  RESERVED3[5];
    __IO uint32_t ROOT_BC_CTRL;
} USBHSH_TypeDef;

#ifdef __cplusplus
}
#endif

#endif
