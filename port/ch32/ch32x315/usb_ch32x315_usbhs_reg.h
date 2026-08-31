/*
 * Copyright (c) 2026, CherryUSB contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_CH32X315_USBHS_REG_H
#define USB_CH32X315_USBHS_REG_H

#include <stdint.h>

#ifndef __IO
#define __IO volatile
#endif

/* CH32X315 USBHS device registers. The controller exposes 8 endpoint
 * indices (EP0..EP7) and stores SRAM offsets in its DMA fields. */
typedef struct {
    __IO uint8_t CONTROL;
    __IO uint8_t BASE_MODE;
    __IO uint8_t INT_EN;
    __IO uint8_t DEV_AD;
    __IO uint8_t WAKE_CTRL;
    __IO uint8_t TEST_MODE;
    __IO uint16_t LPM_DATA;
    __IO uint8_t INT_FG;
    __IO uint8_t INT_ST;
    __IO uint8_t MIS_ST;
    uint8_t RESERVED0;
    __IO uint16_t FRAME_NO;
    __IO uint16_t BUS;
    __IO uint16_t UEP_TX_EN;
    __IO uint16_t UEP_RX_EN;
    __IO uint16_t UEP_TX_TOG_AUTO;
    __IO uint16_t UEP_RX_TOG_AUTO;
    __IO uint8_t UEP_TX_BURST;
    __IO uint8_t UEP_TX_BURST_MODE;
    __IO uint8_t UEP_RX_BURST;
    __IO uint8_t UEP_RX_RES_MODE;
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
    __IO uint8_t UEP0_TX_CTRL;
    __IO uint8_t UEP0_RX_CTRL;
    __IO uint16_t UEP1_TX_LEN;
    __IO uint8_t UEP1_TX_CTRL;
    __IO uint8_t UEP1_RX_CTRL;
    __IO uint16_t UEP2_TX_LEN;
    __IO uint8_t UEP2_TX_CTRL;
    __IO uint8_t UEP2_RX_CTRL;
    __IO uint16_t UEP3_TX_LEN;
    __IO uint8_t UEP3_TX_CTRL;
    __IO uint8_t UEP3_RX_CTRL;
    __IO uint16_t UEP4_TX_LEN;
    __IO uint8_t UEP4_TX_CTRL;
    __IO uint8_t UEP4_RX_CTRL;
    __IO uint16_t UEP5_TX_LEN;
    __IO uint8_t UEP5_TX_CTRL;
    __IO uint8_t UEP5_RX_CTRL;
    __IO uint16_t UEP6_TX_LEN;
    __IO uint8_t UEP6_TX_CTRL;
    __IO uint8_t UEP6_RX_CTRL;
    __IO uint16_t UEP7_TX_LEN;
    __IO uint8_t UEP7_TX_CTRL;
    __IO uint8_t UEP7_RX_CTRL;
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

#define USBHS_BASE                  0x40030000UL
#define USBHS_UD_RST_LINK           0x01U
#define USBHS_UD_RST_SIE            0x02U
#define USBHS_UD_PHY_SUSPENDM       0x08U
#define USBHS_UD_DMA_EN             0x10U
#define USBHS_UD_DEV_EN             0x20U
#define USBHS_UD_LPM_EN             0x80U
#define USBHS_UD_DEV_ADDR           0x7FU
#define USBHS_UD_REMOTE_WKUP        0x01U

#define USBHS_UD_SPEED_FULL         0x00U
#define USBHS_UD_SPEED_HIGH         0x01U

#define USBHS_UDIE_LINK_RDY         0x40U
#define USBHS_UDIE_TRANSFER         0x10U
#define USBHS_UDIE_LPM_ACT          0x08U
#define USBHS_UDIE_BUS_SLEEP        0x04U
#define USBHS_UDIE_SUSPEND          0x02U
#define USBHS_UDIE_BUS_RST          0x01U

#define USBHS_UDIF_LINK_RDY         0x40U
#define USBHS_UDIF_TRANSFER         0x10U
#define USBHS_UDIF_LPM_ACT          0x08U
#define USBHS_UDIF_BUS_SLEEP        0x04U
#define USBHS_UDIF_SUSPEND          0x02U
#define USBHS_UDIF_BUS_RST          0x01U

#define USBHS_UDIS_EP_DIR           0x10U
#define USBHS_UDIS_EP_ID_MASK       0x07U
#define USBHS_UDMS_HS_MOD           0x80U
#define USBHS_UDMS_SUSPEND          0x02U

#define USBHS_UEP0_T_EN             0x0001U
#define USBHS_UEP0_R_EN             0x0001U
#define USBHS_UEP_T_DONE            0x80U
#define USBHS_UEP_T_TOG_MASK        0x0CU
#define USBHS_UEP_T_TOG_DATA1       0x04U
#define USBHS_UEP_T_RES_MASK        0x03U
#define USBHS_UEP_T_RES_ACK         0x02U
#define USBHS_UEP_T_RES_STALL       0x01U
#define USBHS_UEP_T_RES_NAK         0x00U
#define USBHS_UEP_T_TOG_DATA0       0x00U
#define USBHS_UEP_R_DONE             0x80U
#define USBHS_UEP_R_TOG_MATCH        0x10U
#define USBHS_UEP_R_SETUP_IS        0x08U
#define USBHS_UEP_R_TOG_MASK         0x0CU
#define USBHS_UEP_R_TOG_DATA1        0x04U
#define USBHS_UEP_R_RES_MASK         0x03U
#define USBHS_UEP_R_RES_ACK          0x02U
#define USBHS_UEP_R_RES_STALL        0x01U
#define USBHS_UEP_R_RES_NAK          0x00U
#define USBHS_UEP_R_TOG_DATA0        0x00U

#endif /* USB_CH32X315_USBHS_REG_H */
