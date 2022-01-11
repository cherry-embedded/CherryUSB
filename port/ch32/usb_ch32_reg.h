#ifndef _USB_CH32_REG_H
#define _USB_CH32_REG_H

#define     __IO    volatile                  /* defines 'read / write' permissions */

/* USBHS Registers */
typedef struct
{
  __IO uint8_t  CONTROL;
  __IO uint8_t  HOST_CTRL;
  __IO uint8_t  INT_EN;
  __IO uint8_t  DEV_AD;
  __IO uint16_t FRAME_NO;
  __IO uint8_t  SUSPEND;
  __IO uint8_t  RESERVED0;
  __IO uint8_t  SPEED_TYPE;
  __IO uint8_t  MIS_ST;
  __IO uint8_t  INT_FG;
  __IO uint8_t  INT_ST;
  __IO uint16_t RX_LEN;
  __IO uint16_t RESERVED1;
  __IO uint32_t ENDP_CONFIG;
  __IO uint32_t ENDP_TYPE;
  __IO uint32_t BUF_MODE;
  __IO uint32_t UEP0_DMA;
  __IO uint32_t UEP1_RX_DMA;
  __IO uint32_t UEP2_RX_DMA;
  __IO uint32_t UEP3_RX_DMA;
  __IO uint32_t UEP4_RX_DMA;
  __IO uint32_t UEP5_RX_DMA;
  __IO uint32_t UEP6_RX_DMA;
  __IO uint32_t UEP7_RX_DMA;
  __IO uint32_t UEP8_RX_DMA;
  __IO uint32_t UEP9_RX_DMA;
  __IO uint32_t UEP10_RX_DMA;
  __IO uint32_t UEP11_RX_DMA;
  __IO uint32_t UEP12_RX_DMA;
  __IO uint32_t UEP13_RX_DMA;
  __IO uint32_t UEP14_RX_DMA;
  __IO uint32_t UEP15_RX_DMA;
  __IO uint32_t UEP1_TX_DMA;
  __IO uint32_t UEP2_TX_DMA;
  __IO uint32_t UEP3_TX_DMA;
  __IO uint32_t UEP4_TX_DMA;
  __IO uint32_t UEP5_TX_DMA;
  __IO uint32_t UEP6_TX_DMA;
  __IO uint32_t UEP7_TX_DMA;
  __IO uint32_t UEP8_TX_DMA;
  __IO uint32_t UEP9_TX_DMA;
  __IO uint32_t UEP10_TX_DMA;
  __IO uint32_t UEP11_TX_DMA;
  __IO uint32_t UEP12_TX_DMA;
  __IO uint32_t UEP13_TX_DMA;
  __IO uint32_t UEP14_TX_DMA;
  __IO uint32_t UEP15_TX_DMA;
  __IO uint16_t UEP0_MAX_LEN;
  __IO uint16_t RESERVED2;
  __IO uint16_t UEP1_MAX_LEN;
  __IO uint16_t RESERVED3;
  __IO uint16_t UEP2_MAX_LEN;
  __IO uint16_t RESERVED4;
  __IO uint16_t UEP3_MAX_LEN;
  __IO uint16_t RESERVED5;
  __IO uint16_t UEP4_MAX_LEN;
  __IO uint16_t RESERVED6;
  __IO uint16_t UEP5_MAX_LEN;
  __IO uint16_t RESERVED7;
  __IO uint16_t UEP6_MAX_LEN;
  __IO uint16_t RESERVED8;
  __IO uint16_t UEP7_MAX_LEN;
  __IO uint16_t RESERVED9;
  __IO uint16_t UEP8_MAX_LEN;
  __IO uint16_t RESERVED10;
  __IO uint16_t UEP9_MAX_LEN;
  __IO uint16_t RESERVED11;
  __IO uint16_t UEP10_MAX_LEN;
  __IO uint16_t RESERVED12;
  __IO uint16_t UEP11_MAX_LEN;
  __IO uint16_t RESERVED13;
  __IO uint16_t UEP12_MAX_LEN;
  __IO uint16_t RESERVED14;
  __IO uint16_t UEP13_MAX_LEN;
  __IO uint16_t RESERVED15;
  __IO uint16_t UEP14_MAX_LEN;
  __IO uint16_t RESERVED16;
  __IO uint16_t UEP15_MAX_LEN;
  __IO uint16_t RESERVED17;
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
  __IO uint16_t UEP8_TX_LEN;
  __IO uint8_t  UEP8_TX_CTRL;
  __IO uint8_t  UEP8_RX_CTRL;
  __IO uint16_t UEP9_TX_LEN;
  __IO uint8_t  UEP9_TX_CTRL;
  __IO uint8_t  UEP9_RX_CTRL;
  __IO uint16_t UEP10_TX_LEN;
  __IO uint8_t  UEP10_TX_CTRL;
  __IO uint8_t  UEP10_RX_CTRL;
  __IO uint16_t UEP11_TX_LEN;
  __IO uint8_t  UEP11_TX_CTRL;
  __IO uint8_t  UEP11_RX_CTRL;
  __IO uint16_t UEP12_TX_LEN;
  __IO uint8_t  UEP12_TX_CTRL;
  __IO uint8_t  UEP12_RX_CTRL;
  __IO uint16_t UEP13_TX_LEN;
  __IO uint8_t  UEP13_TX_CTRL;
  __IO uint8_t  UEP13_RX_CTRL;
  __IO uint16_t UEP14_TX_LEN;
  __IO uint8_t  UEP14_TX_CTRL;
  __IO uint8_t  UEP14_RX_CTRL;
  __IO uint16_t UEP15_TX_LEN;
  __IO uint8_t  UEP15_TX_CTRL;
  __IO uint8_t  UEP15_RX_CTRL;
} USBHSD_TypeDef;

typedef struct  __attribute__((packed))
{
    __IO uint8_t  CONTROL;
    __IO uint8_t  HOST_CTRL;
    __IO uint8_t  INT_EN;
    __IO uint8_t  DEV_AD;
    __IO uint16_t FRAME_NO;
    __IO uint8_t  SUSPEND;
    __IO uint8_t  RESERVED0;
    __IO uint8_t  SPEED_TYPE;
    __IO uint8_t  MIS_ST;
    __IO uint8_t  INT_FG;
    __IO uint8_t  INT_ST;
    __IO uint16_t RX_LEN;
    __IO uint16_t RESERVED1;
    __IO uint32_t HOST_EP_CONFIG;
    __IO uint32_t HOST_EP_TYPE;
    __IO uint32_t RESERVED2;
    __IO uint32_t RESERVED3;
    __IO uint32_t RESERVED4;
    __IO uint32_t HOST_RX_DMA;
    __IO uint32_t RESERVED5;
    __IO uint32_t RESERVED6;
    __IO uint32_t RESERVED7;
    __IO uint32_t RESERVED8;
    __IO uint32_t RESERVED9;
    __IO uint32_t RESERVED10;
    __IO uint32_t RESERVED11;
    __IO uint32_t RESERVED12;
    __IO uint32_t RESERVED13;
    __IO uint32_t RESERVED14;
    __IO uint32_t RESERVED15;
    __IO uint32_t RESERVED16;
    __IO uint32_t RESERVED17;
    __IO uint32_t RESERVED18;
    __IO uint32_t RESERVED19;
    __IO uint32_t HOST_TX_DMA;
    __IO uint32_t RESERVED20;
    __IO uint32_t RESERVED21;
    __IO uint32_t RESERVED22;
    __IO uint32_t RESERVED23;
    __IO uint32_t RESERVED24;
    __IO uint32_t RESERVED25;
    __IO uint32_t RESERVED26;
    __IO uint32_t RESERVED27;
    __IO uint32_t RESERVED28;
    __IO uint32_t RESERVED29;
    __IO uint32_t RESERVED30;
    __IO uint32_t RESERVED31;
    __IO uint32_t RESERVED32;
    __IO uint32_t RESERVED33;
    __IO uint16_t HOST_RX_MAX_LEN;
    __IO uint16_t RESERVED34;
    __IO uint32_t RESERVED35;
    __IO uint32_t RESERVED36;
    __IO uint32_t RESERVED37;
    __IO uint32_t RESERVED38;
    __IO uint32_t RESERVED39;
    __IO uint32_t RESERVED40;
    __IO uint32_t RESERVED41;
    __IO uint32_t RESERVED42;
    __IO uint32_t RESERVED43;
    __IO uint32_t RESERVED44;
    __IO uint32_t RESERVED45;
    __IO uint32_t RESERVED46;
    __IO uint32_t RESERVED47;
    __IO uint32_t RESERVED48;
    __IO uint32_t RESERVED49;
    __IO uint8_t  HOST_EP_PID;
    __IO uint8_t  RESERVED50;
    __IO uint8_t  RESERVED51;
    __IO uint8_t  HOST_RX_CTRL;
    __IO uint16_t HOST_TX_LEN;
    __IO uint8_t  HOST_TX_CTRL;
    __IO uint8_t  RESERVED52;
    __IO uint16_t HOST_SPLIT_DATA;
} USBHSH_TypeDef;


/* USBOTG_FS Registers */
typedef struct
{
   __IO uint8_t  BASE_CTRL;
   __IO uint8_t  UDEV_CTRL;
   __IO uint8_t  INT_EN;
   __IO uint8_t  DEV_ADDR;
   __IO uint8_t  Reserve0;
   __IO uint8_t  MIS_ST;
   __IO uint8_t  INT_FG;
   __IO uint8_t  INT_ST;
   __IO uint16_t RX_LEN;
   __IO uint16_t Reserve1;
   __IO uint8_t  UEP4_1_MOD;
   __IO uint8_t  UEP2_3_MOD;
   __IO uint8_t  UEP5_6_MOD;
   __IO uint8_t  UEP7_MOD;
   __IO uint32_t UEP0_DMA;
   __IO uint32_t UEP1_DMA;
   __IO uint32_t UEP2_DMA;
   __IO uint32_t UEP3_DMA;
   __IO uint32_t UEP4_DMA;
   __IO uint32_t UEP5_DMA;
   __IO uint32_t UEP6_DMA;
   __IO uint32_t UEP7_DMA;
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
   __IO uint32_t Reserve2;
   __IO uint32_t OTG_CR;
   __IO uint32_t OTG_SR;
}USBOTG_FS_TypeDef;

typedef struct  __attribute__((packed))
{
   __IO uint8_t   BASE_CTRL;
   __IO uint8_t   HOST_CTRL;
   __IO uint8_t   INT_EN;
   __IO uint8_t   DEV_ADDR;
   __IO uint8_t   Reserve0;
   __IO uint8_t   MIS_ST;
   __IO uint8_t   INT_FG;
   __IO uint8_t   INT_ST;
   __IO uint16_t  RX_LEN;
   __IO uint16_t  Reserve1;
   __IO uint8_t   Reserve2;
   __IO uint8_t   HOST_EP_MOD;
   __IO uint16_t  Reserve3;
   __IO uint32_t  Reserve4;
   __IO uint32_t  Reserve5;
   __IO uint32_t  HOST_RX_DMA;
   __IO uint32_t  HOST_TX_DMA;
   __IO uint32_t  Reserve6;
   __IO uint32_t  Reserve7;
   __IO uint32_t  Reserve8;
   __IO uint32_t  Reserve9;
   __IO uint32_t  Reserve10;
   __IO uint16_t  Reserve11;
   __IO uint16_t  HOST_SETUP;
   __IO uint8_t   HOST_EP_PID;
   __IO uint8_t   Reserve12;
   __IO uint8_t   Reserve13;
   __IO uint8_t   HOST_RX_CTRL;
   __IO uint16_t  HOST_TX_LEN;
   __IO uint8_t   HOST_TX_CTRL;
   __IO uint8_t   Reserve14;
   __IO uint32_t  Reserve15;
   __IO uint32_t  Reserve16;
   __IO uint32_t  Reserve17;
   __IO uint32_t  Reserve18;
   __IO uint32_t  Reserve19;
   __IO uint32_t  OTG_CR;
   __IO uint32_t  OTG_SR;
}USBOTGH_FS_TypeDef;

#define USBFS_BASE            ((uint32_t)0x50000000)
#define USBHS_BASE            (((uint32_t))(0x40000000+0x23400))

#define USBHSD              ((USBHSD_TypeDef *) USBHS_BASE)
#define USBHSH              ((USBHSH_TypeDef *) USBHS_BASE)
#define USBOTG_FS           ((USBOTG_FS_TypeDef *)USBFS_BASE)
#define USBOTG_H_FS         ((USBOTGH_FS_TypeDef *)USBFS_BASE)

/******************************************************************************/
/* USBOTG_FS DEVICE USB_CONTROL */
/* BASE USB_CTRL */
#define   USBHD_BASE_CTRL       (USBOTG_FS->BASE_CTRL)  // USB base control
#define     USBHD_UC_HOST_MODE     0x80      // enable USB host mode: 0=device mode, 1=host mode
#define     USBHD_UC_LOW_SPEED     0x40      // enable USB low speed: 0=12Mbps, 1=1.5Mbps
#define     USBHD_UC_DEV_PU_EN     0x20      // USB device enable and internal pullup resistance enable
#define     USBHD_UC_SYS_CTRL1     0x20      // USB system control high bit
#define     USBHD_UC_SYS_CTRL0     0x10      // USB system control low bit
#define     USBHD_UC_SYS_CTRL_MASK 0x30      // bit mask of USB system control
// UC_HOST_MODE & UC_SYS_CTRL1 & UC_SYS_CTRL0: USB system control
//   0 00: disable USB device and disable internal pullup resistance
//   0 01: enable USB device and disable internal pullup resistance, need external pullup resistance
//   0 1x: enable USB device and enable internal pullup resistance
//   1 00: enable USB host and normal status
//   1 01: enable USB host and force UDP/UDM output SE0 state
//   1 10: enable USB host and force UDP/UDM output J state
//   1 11: enable USB host and force UDP/UDM output resume or K state
#define     USBHD_UC_INT_BUSY      0x08      // enable automatic responding busy for device mode or automatic pause for host mode during interrupt flag UIF_TRANSFER valid
#define     USBHD_UC_RESET_SIE     0x04      // force reset USB SIE, need software clear
#define     USBHD_UC_CLR_ALL       0x02      // force clear FIFO and count of USB
#define     USBHD_UC_DMA_EN        0x01      // DMA enable and DMA interrupt enable for USB
/* DEVICE USB_CTRL */
#define   USBHD_UDEV_CTRL        (USBOTG_FS->UDEV_CTRL)  // USB device physical prot control
#define     USBHD_UD_PD_DIS        0x80      // disable USB UDP/UDM pulldown resistance: 0=enable pulldown, 1=disable
#define     USBHD_UD_DP_PIN        0x20      // ReadOnly: indicate current UDP pin level
#define     USBHD_UD_DM_PIN        0x10      // ReadOnly: indicate current UDM pin level
#define     USBHD_UD_LOW_SPEED     0x04      // enable USB physical port low speed: 0=full speed, 1=low speed
#define     USBHD_UD_GP_BIT        0x02      // general purpose bit
#define     USBHD_UD_PORT_EN       0x01      // enable USB physical port I/O: 0=disable, 1=enable
/* USB INT EN */
#define   USBHD_INT_EN           (USBOTG_FS->INT_EN)    // USB interrupt enable
#define     USBHD_UIE_DEV_SOF      0x80      // enable interrupt for SOF received for USB device mode
#define     USBHD_UIE_DEV_NAK      0x40      // enable interrupt for NAK responded for USB device mode
#define     USBHD_UIE_FIFO_OV      0x10      // enable interrupt for FIFO overflow
#define     USBHD_UIE_HST_SOF      0x08      // enable interrupt for host SOF timer action for USB host mode
#define     USBHD_UIE_SUSPEND      0x04      // enable interrupt for USB suspend or resume event
#define     USBHD_UIE_TRANSFER     0x02      // enable interrupt for USB transfer completion
#define     USBHD_UIE_DETECT       0x01      // enable interrupt for USB device detected event for USB host mode
#define     USBHD_UIE_BUS_RST      0x01      // enable interrupt for USB bus reset event for USB device mode
/* USB_DEV_ADDR */
#define   USBHD_DEV_ADDR         (USBOTG_FS->DEV_ADDR)   // USB device address
#define     USBHD_UDA_GP_BIT       0x80      // general purpose bit
#define     USBHD_USB_ADDR_MASK    0x7F      // bit mask for USB device address
/* USBOTG_FS DEVICE USB_STATUS */
/* USB_MIS_ST */
#define   USBHD_MIS_ST           (USBOTG_FS->MIS_ST)     // USB miscellaneous status
#define     USBHD_UMS_SOF_PRES     0x80      // RO, indicate host SOF timer presage status
#define     USBHD_UMS_SOF_ACT      0x40      // RO, indicate host SOF timer action status for USB host
#define     USBHD_UMS_SIE_FREE     0x20      // RO, indicate USB SIE free status
#define     USBHD_UMS_R_FIFO_RDY   0x10      // RO, indicate USB receiving FIFO ready status (not empty)
#define     USBHD_UMS_BUS_RESET    0x08      // RO, indicate USB bus reset status
#define     USBHD_UMS_SUSPEND      0x04      // RO, indicate USB suspend status
#define     USBHD_UMS_DM_LEVEL     0x02      // RO, indicate UDM level saved at device attached to USB host
#define     USBHD_UMS_DEV_ATTACH   0x01      // RO, indicate device attached status on USB host
/* USB_INT_FG */
#define   USBHD_INT_FG           (USBOTG_FS->INT_FG)    // USB interrupt flag
#define     USBHD_U_IS_NAK         0x80    // RO, indicate current USB transfer is NAK received
#define     USBHD_U_TOG_OK         0x40    // RO, indicate current USB transfer toggle is OK
#define     USBHD_U_SIE_FREE       0x20    // RO, indicate USB SIE free status
#define     USBHD_UIF_FIFO_OV      0x10    // FIFO overflow interrupt flag for USB, direct bit address clear or write 1 to clear
#define     USBHD_UIF_HST_SOF      0x08    // host SOF timer interrupt flag for USB host, direct bit address clear or write 1 to clear
#define     USBHD_UIF_SUSPEND      0x04    // USB suspend or resume event interrupt flag, direct bit address clear or write 1 to clear
#define     USBHD_UIF_TRANSFER     0x02    // USB transfer completion interrupt flag, direct bit address clear or write 1 to clear
#define     USBHD_UIF_DETECT       0x01    // device detected event interrupt flag for USB host mode, direct bit address clear or write 1 to clear
#define     USBHD_UIF_BUS_RST      0x01    // bus reset event interrupt flag for USB device mode, direct bit address clear or write 1 to clear
/* USB_INT_ST */
#define   USBHD_INT_ST           (USBOTG_FS->INT_ST)    // USB interrupt flag
#define     USBHD_UIS_IS_SETUP     0x80      // RO, indicate current USB transfer is setup received for USB device mode
#define     USBHD_UIS_IS_NAK       0x80      // RO, indicate current USB transfer is NAK received for USB device mode
#define     USBHD_UIS_TOG_OK       0x40      // RO, indicate current USB transfer toggle is OK
#define     USBHD_UIS_TOKEN1       0x20      // RO, current token PID code bit 1 received for USB device mode
#define     USBHD_UIS_TOKEN0       0x10      // RO, current token PID code bit 0 received for USB device mode
#define     USBHD_UIS_TOKEN_MASK   0x30      // RO, bit mask of current token PID code received for USB device mode
#define     USBHD_UIS_TOKEN_OUT    0x00
#define     USBHD_UIS_TOKEN_SOF    0x10
#define     USBHD_UIS_TOKEN_IN     0x20
#define     USBHD_UIS_TOKEN_SETUP  0x30
// UIS_TOKEN1 & UIS_TOKEN0: current token PID code received for USB device mode
//   00: OUT token PID received
//   01: SOF token PID received
//   10: IN token PID received
//   11: SETUP token PID received
#define     USBHD_UIS_ENDP_MASK    0x0F      // RO, bit mask of current transfer endpoint number for USB device mode
/* USB_RX_LEN */
#define   USBHD_RX_LEN        (USBOTG_FS->Rx_Len)      // USB receiving length
/* USB_BUF_MOD */
#define   USBHD_UEP4_1_MOD    (USBOTG_FS->UEP4_1_MOD)  // endpoint 4/1 mode
#define     USBHD_UEP1_RX_EN       0x80      // enable USB endpoint 1 receiving (OUT)
#define     USBHD_UEP1_TX_EN       0x40      // enable USB endpoint 1 transmittal (IN)
#define     USBHD_UEP1_BUF_MOD     0x10      // buffer mode of USB endpoint 1
// UEPn_RX_EN & UEPn_TX_EN & UEPn_BUF_MOD: USB endpoint 1/2/3 buffer mode, buffer start address is UEPn_DMA
//   0 0 x:  disable endpoint and disable buffer
//   1 0 0:  64 bytes buffer for receiving (OUT endpoint)
//   1 0 1:  dual 64 bytes buffer by toggle bit bUEP_R_TOG selection for receiving (OUT endpoint), total=128bytes
//   0 1 0:  64 bytes buffer for transmittal (IN endpoint)
//   0 1 1:  dual 64 bytes buffer by toggle bit bUEP_T_TOG selection for transmittal (IN endpoint), total=128bytes
//   1 1 0:  64 bytes buffer for receiving (OUT endpoint) + 64 bytes buffer for transmittal (IN endpoint), total=128bytes
//   1 1 1:  dual 64 bytes buffer by bUEP_R_TOG selection for receiving (OUT endpoint) + dual 64 bytes buffer by bUEP_T_TOG selection for transmittal (IN endpoint), total=256bytes
#define     USBHD_UEP4_RX_EN       0x08      // enable USB endpoint 4 receiving (OUT)
#define     USBHD_UEP4_TX_EN       0x04      // enable USB endpoint 4 transmittal (IN)
// UEP4_RX_EN & UEP4_TX_EN: USB endpoint 4 buffer mode, buffer start address is UEP0_DMA
//   0 0:  single 64 bytes buffer for endpoint 0 receiving & transmittal (OUT & IN endpoint)
//   1 0:  single 64 bytes buffer for endpoint 0 receiving & transmittal (OUT & IN endpoint) + 64 bytes buffer for endpoint 4 receiving (OUT endpoint), total=128bytes
//   0 1:  single 64 bytes buffer for endpoint 0 receiving & transmittal (OUT & IN endpoint) + 64 bytes buffer for endpoint 4 transmittal (IN endpoint), total=128bytes
//   1 1:  single 64 bytes buffer for endpoint 0 receiving & transmittal (OUT & IN endpoint)
//           + 64 bytes buffer for endpoint 4 receiving (OUT endpoint) + 64 bytes buffer for endpoint 4 transmittal (IN endpoint), total=192bytes

#define  USBHD_UEP2_3_MOD     (USBOTG_FS->UEP2_3_MOD)  // endpoint 2/3 mode
#define     USBHD_UEP3_RX_EN       0x80      // enable USB endpoint 3 receiving (OUT)
#define     USBHD_UEP3_TX_EN       0x40      // enable USB endpoint 3 transmittal (IN)
#define     USBHD_UEP3_BUF_MOD     0x10      // buffer mode of USB endpoint 3
#define     USBHD_UEP2_RX_EN       0x08      // enable USB endpoint 2 receiving (OUT)
#define     USBHD_UEP2_TX_EN       0x04      // enable USB endpoint 2 transmittal (IN)
#define     USBHD_UEP2_BUF_MOD     0x01      // buffer mode of USB endpoint 2

#define  USBHD_UEP5_6_MOD     (USBOTG_FS->UEP5_6_MOD)  // endpoint 5/6 mode
#define     USBHD_UEP6_RX_EN       0x80      // enable USB endpoint 6 receiving (OUT)
#define     USBHD_UEP6_TX_EN       0x40      // enable USB endpoint 6 transmittal (IN)
#define     USBHD_UEP6_BUF_MOD     0x10      // buffer mode of USB endpoint 6
#define     USBHD_UEP5_RX_EN       0x08      // enable USB endpoint 5 receiving (OUT)
#define     USBHD_UEP5_TX_EN       0x04      // enable USB endpoint 5 transmittal (IN)
#define     USBHD_UEP5_BUF_MOD     0x01      // buffer mode of USB endpoint 5

#define  USBHD_UEP7_MOD       (USBOTG_FS->UEP7_MOD)  // endpoint 7 mode
#define     USBHD_UEP7_RX_EN       0x08      // enable USB endpoint 7 receiving (OUT)
#define     USBHD_UEP7_TX_EN       0x04      // enable USB endpoint 7 transmittal (IN)
#define     USBHD_UEP7_BUF_MOD     0x01      // buffer mode of USB endpoint 7
/* USB_DMA */
#define  USBHD_UEP0_DMA       (USBOTG_FS->UEP0_DMA) // endpoint 0 DMA buffer address
#define  USBHD_UEP1_DMA       (USBOTG_FS->UEP1_DMA) // endpoint 1 DMA buffer address
#define  USBHD_UEP2_DMA       (USBOTG_FS->UEP2_DMA) // endpoint 2 DMA buffer address
#define  USBHD_UEP3_DMA       (USBOTG_FS->UEP3_DMA) // endpoint 3 DMA buffer address
#define  USBHD_UEP4_DMA       (USBOTG_FS->UEP4_DMA) // endpoint 4 DMA buffer address
#define  USBHD_UEP5_DMA       (USBOTG_FS->UEP5_DMA) // endpoint 5 DMA buffer address
#define  USBHD_UEP6_DMA       (USBOTG_FS->UEP6_DMA) // endpoint 6 DMA buffer address
#define  USBHD_UEP7_DMA       (USBOTG_FS->UEP7_DMA) // endpoint 7 DMA buffer address
/* USB_EP_CTRL */
#define  USBHD_UEP0_T_LEN     (USBOTG_FS->UEP0_TX_LEN)   // endpoint 0 transmittal length
#define  USBHD_UEP0_TX_CTRL   (USBOTG_FS->UEP0_TX_CTRL)  // endpoint 0 control
#define  USBHD_UEP0_RX_CTRL   (USBOTG_FS->UEP0_RX_CTRL)  // endpoint 0 control

#define  USBHD_UEP1_T_LEN     (USBOTG_FS->UEP1_TX_LEN)   // endpoint 1 transmittal length
#define  USBHD_UEP1_TX_CTRL   (USBOTG_FS->UEP1_TX_CTRL)  // endpoint 1 control
#define  USBHD_UEP1_RX_CTRL   (USBOTG_FS->UEP1_RX_CTRL)  // endpoint 1 control

#define  USBHD_UEP2_T_LEN     (USBOTG_FS->UEP2_TX_LEN)   // endpoint 2 transmittal length
#define  USBHD_UEP2_TX_CTRL   (USBOTG_FS->UEP2_TX_CTRL)  // endpoint 2 control
#define  USBHD_UEP2_RX_CTRL   (USBOTG_FS->UEP2_RX_CTRL)  // endpoint 2 control

#define  USBHD_UEP3_T_LEN     (USBOTG_FS->UEP3_TX_LEN)   // endpoint 3 transmittal length
#define  USBHD_UEP3_TX_CTRL   (USBOTG_FS->UEP3_TX_CTRL)  // endpoint 3 control
#define  USBHD_UEP3_RX_CTRL   (USBOTG_FS->UEP3_RX_CTRL)  // endpoint 3 control

#define  USBHD_UEP4_T_LEN     (USBOTG_FS->UEP4_TX_LEN)   // endpoint 4 transmittal length
#define  USBHD_UEP4_TX_CTRL   (USBOTG_FS->UEP4_TX_CTRL)  // endpoint 4 control
#define  USBHD_UEP4_RX_CTRL   (USBOTG_FS->UEP4_RX_CTRL)  // endpoint 4 control

#define  USBHD_UEP5_T_LEN     (USBOTG_FS->UEP5_TX_LEN)   // endpoint 5 transmittal length
#define  USBHD_UEP5_TX_CTRL   (USBOTG_FS->UEP5_TX_CTRL)  // endpoint 5 control
#define  USBHD_UEP5_RX_CTRL   (USBOTG_FS->UEP5_RX_CTRL)  // endpoint 5 control

#define  USBHD_UEP6_T_LEN     (USBOTG_FS->UEP6_TX_LEN)   // endpoint 6 transmittal length
#define  USBHD_UEP6_TX_CTRL   (USBOTG_FS->UEP6_TX_CTRL)  // endpoint 6 control
#define  USBHD_UEP6_RX_CTRL   (USBOTG_FS->UEP6_RX_CTRL)  // endpoint 6 control

#define  USBHD_UEP7_T_LEN     (USBOTG_FS->UEP7_TX_LEN)   // endpoint 7 transmittal length
#define  USBHD_UEP7_TX_CTRL   (USBOTG_FS->UEP7_TX_CTRL)  // endpoint 7 control
#define  USBHD_UEP7_RX_CTRL   (USBOTG_FS->UEP7_RX_CTRL)  // endpoint 7 control

#define     USBHD_UEP_AUTO_TOG     0x08      // enable automatic toggle after successful transfer completion on endpoint 1/2/3: 0=manual toggle, 1=automatic toggle
#define     USBHD_UEP_R_TOG        0x04      // expected data toggle flag of USB endpoint X receiving (OUT): 0=DATA0, 1=DATA1
#define     USBHD_UEP_T_TOG        0x04      // prepared data toggle flag of USB endpoint X transmittal (IN): 0=DATA0, 1=DATA1

#define     USBHD_UEP_R_RES1       0x02      // handshake response type high bit for USB endpoint X receiving (OUT)
#define     USBHD_UEP_R_RES0       0x01      // handshake response type low bit for USB endpoint X receiving (OUT)
#define     USBHD_UEP_R_RES_MASK   0x03      // bit mask of handshake response type for USB endpoint X receiving (OUT)
#define     USBHD_UEP_R_RES_ACK    0x00
#define     USBHD_UEP_R_RES_TOUT   0x01
#define     USBHD_UEP_R_RES_NAK    0x02
#define     USBHD_UEP_R_RES_STALL  0x03
// RB_UEP_R_RES1 & RB_UEP_R_RES0: handshake response type for USB endpoint X receiving (OUT)
//   00: ACK (ready)
//   01: no response, time out to host, for non-zero endpoint isochronous transactions
//   10: NAK (busy)
//   11: STALL (error)
#define     USBHD_UEP_T_RES1       0x02      // handshake response type high bit for USB endpoint X transmittal (IN)
#define     USBHD_UEP_T_RES0       0x01      // handshake response type low bit for USB endpoint X transmittal (IN)
#define     USBHD_UEP_T_RES_MASK   0x03      // bit mask of handshake response type for USB endpoint X transmittal (IN)
#define     USBHD_UEP_T_RES_ACK    0x00
#define     USBHD_UEP_T_RES_TOUT   0x01
#define     USBHD_UEP_T_RES_NAK    0x02
#define     USBHD_UEP_T_RES_STALL  0x03
// bUEP_T_RES1 & bUEP_T_RES0: handshake response type for USB endpoint X transmittal (IN)
//   00: DATA0 or DATA1 then expecting ACK (ready)
//   01: DATA0 or DATA1 then expecting no response, time out from host, for non-zero endpoint isochronous transactions
//   10: NAK (busy)
//   11: TALL (error)

#endif
