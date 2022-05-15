#ifndef __USB_MM32_REG_H__
#define __USB_MM32_REG_H__

#define     __IO    volatile             /*!< Defines 'read / write' permissions */

/**
* @brief USB
*/
typedef struct
{
    __IO uint32_t  rTOP;        		/*!           Address offset: 0x00 */
    __IO uint32_t  rINT_STATE;        	/*!           Address offset: 0x04 */
    __IO uint32_t  rEP_INT_STATE;  		/*!           Address offset: 0x08 */
    __IO uint32_t  rEP0_INT_STATE; 		/*!           Address offset: 0x0C */
    __IO uint32_t  rINT_EN;        		/*!           Address offset: 0x10 */
    __IO uint32_t  rEP_INT_EN;        	/*!           Address offset: 0x14 */
    __IO uint32_t  rEP0_INT_EN;      	/*!           Address offset: 0x18 */

    __IO uint32_t  RESERVED0;

    __IO uint32_t  rEP1_INT_STATE;      /*!           Address offset: 0x20 */
    __IO uint32_t  rEP2_INT_STATE;      /*!           Address offset: 0x24 */
    __IO uint32_t  rEP3_INT_STATE;      /*!           Address offset: 0x28 */
    __IO uint32_t  rEP4_INT_STATE;      /*!           Address offset: 0x2C */

    __IO uint32_t  RESERVED1; 			/*!           Address offset: 0x30 */
    __IO uint32_t  RESERVED2; 			/*!           Address offset: 0x34 */
    __IO uint32_t  RESERVED3; 			/*!           Address offset: 0x38 */
    __IO uint32_t  RESERVED4; 			/*!           Address offset: 0x3C */

    __IO uint32_t  rEP1_INT_EN;        	/*!           Address offset: 0x40 */
    __IO uint32_t  rEP2_INT_EN;        	/*!           Address offset: 0x44 */
    __IO uint32_t  rEP3_INT_EN;        	/*!           Address offset: 0x48 */
    __IO uint32_t  rEP4_INT_EN;        	/*!           Address offset: 0x4C */

    __IO uint32_t  RESERVED5; 			/*!           Address offset: 0x50 */
    __IO uint32_t  RESERVED6; 			/*!           Address offset: 0x54 */
    __IO uint32_t  RESERVED7; 			/*!           Address offset: 0x58 */
    __IO uint32_t  RESERVED8; 			/*!           Address offset: 0x5C */

    __IO uint32_t  rADDR;        		/*!           Address offset: 0x60 */
    __IO uint32_t  rEP_EN;        		/*!           Address offset: 0x64 */

    __IO uint32_t  RESERVED9; 			/*!           Address offset: 0x68 */
    __IO uint32_t  RESERVED10; 			/*!           Address offset: 0x6C */
    __IO uint32_t  RESERVED11; 			/*!           Address offset: 0x70 */
    __IO uint32_t  RESERVED12; 			/*!           Address offset: 0x74 */

    __IO uint32_t  rTOG_CTRL1_4;        /*!           Address offset: 0x78 */

    __IO uint32_t  RESERVED13; 			/*!           Address offset: 0x7C */

    __IO uint32_t  rSETUP[8];        		/*!           Address offset: 0x80 */
    //__IO uint32_t  rSETUP0;        		/*!           Address offset: 0x80 */
    //__IO uint32_t  rSETUP1;        		/*!           Address offset: 0x84 */
    //__IO uint32_t  rSETUP2;        		/*!           Address offset: 0x88 */
    //__IO uint32_t  rSETUP3;        		/*!           Address offset: 0x8C */
    //__IO uint32_t  rSETUP4;        		/*!           Address offset: 0x90 */
    //__IO uint32_t  rSETUP5;        		/*!           Address offset: 0x94 */
    //__IO uint32_t  rSETUP6;        		/*!           Address offset: 0x98 */
    //__IO uint32_t  rSETUP7;        		/*!           Address offset: 0x9C */
    __IO uint32_t  rPAKET_SIZE0;    	/*!           Address offset: 0xA0 */
    __IO uint32_t  rPAKET_SIZE1;    	/*!           Address offset: 0xA4 */

    __IO uint32_t  RESERVED14; 			/*!           Address offset: 0xA8 */
    __IO uint32_t  RESERVED15; 			/*!           Address offset: 0xAC */

    __IO uint32_t  RESERVED16; 			/*!           Address offset: 0xB0 */
    __IO uint32_t  RESERVED17; 			/*!           Address offset: 0xB4 */
    __IO uint32_t  RESERVED18; 			/*!           Address offset: 0xB8 */
    __IO uint32_t  RESERVED19; 			/*!           Address offset: 0xBC */

    __IO uint32_t  RESERVED20; 			/*!           Address offset: 0xC0 */
    __IO uint32_t  RESERVED21; 			/*!           Address offset: 0xC4 */
    __IO uint32_t  RESERVED22; 			/*!           Address offset: 0xC8 */
    __IO uint32_t  RESERVED23; 			/*!           Address offset: 0xCC */

    __IO uint32_t  RESERVED24; 			/*!           Address offset: 0xD0 */
    __IO uint32_t  RESERVED25; 			/*!           Address offset: 0xD4 */
    __IO uint32_t  RESERVED26; 			/*!           Address offset: 0xD8 */
    __IO uint32_t  RESERVED27; 			/*!           Address offset: 0xDC */

    __IO uint32_t  RESERVED28; 			/*!           Address offset: 0xE0 */
    __IO uint32_t  RESERVED29; 			/*!           Address offset: 0xE4 */
    __IO uint32_t  RESERVED30; 			/*!           Address offset: 0xE8 */
    __IO uint32_t  RESERVED31; 			/*!           Address offset: 0xEC */

    __IO uint32_t  RESERVED32; 			/*!           Address offset: 0xF0 */
    __IO uint32_t  RESERVED33; 			/*!           Address offset: 0xF4 */
    __IO uint32_t  RESERVED34; 			/*!           Address offset: 0xF8 */
    __IO uint32_t  RESERVED35; 			/*!           Address offset: 0xFC */

    __IO uint32_t  rEP0_AVIL;		 	/*!           Address offset: 0x100 */
    __IO uint32_t  rEP1_AVIL;			/*!           Address offset: 0x104 */
    __IO uint32_t  rEP2_AVIL;			/*!           Address offset: 0x108 */
    __IO uint32_t  rEP3_AVIL;			/*!           Address offset: 0x10C */
    __IO uint32_t  rEP4_AVIL;			/*!           Address offset: 0x110 */

    __IO uint32_t  RESERVED36; 			/*!           Address offset: 0x114 */
    __IO uint32_t  RESERVED37; 			/*!           Address offset: 0x118 */
    __IO uint32_t  RESERVED38; 			/*!           Address offset: 0x11C */
    __IO uint32_t  RESERVED39; 			/*!           Address offset: 0x120 */

    __IO uint32_t  RESERVED40; 			/*!           Address offset: 0x124 */
    __IO uint32_t  RESERVED41; 			/*!           Address offset: 0x128 */
    __IO uint32_t  RESERVED42; 			/*!           Address offset: 0x12C */
    __IO uint32_t  RESERVED43; 			/*!           Address offset: 0x130 */

    __IO uint32_t  RESERVED44; 			/*!           Address offset: 0x134 */
    __IO uint32_t  RESERVED45; 			/*!           Address offset: 0x138 */
    __IO uint32_t  RESERVED46; 			/*!           Address offset: 0x13C */

    __IO uint32_t  rEP0_CTRL;			/*!           Address offset: 0x140 */
    __IO uint32_t  rEP1_CTRL;			/*!           Address offset: 0x144 */
    __IO uint32_t  rEP2_CTRL;			/*!           Address offset: 0x148 */
    __IO uint32_t  rEP3_CTRL;			/*!           Address offset: 0x14C */
    __IO uint32_t  rEP4_CTRL;			/*!           Address offset: 0x150 */

    __IO uint32_t  RESERVED47; 			/*!           Address offset: 0x154 */
    __IO uint32_t  RESERVED48; 			/*!           Address offset: 0x158 */
    __IO uint32_t  RESERVED49; 			/*!           Address offset: 0x15C */
    //__IO uint32_t  RESERVED50; 			/*!           Address offset: 0x15C */

    //__IO uint32_t  rEPn_FIFO[5];			/*!           Address offset: 0x160 */

    __IO uint32_t  rEP0_FIFO;			/*!           Address offset: 0x160 */
    __IO uint32_t  rEP1_FIFO;			/*!           Address offset: 0x164 */
    __IO uint32_t  rEP2_FIFO;			/*!           Address offset: 0x168 */
    __IO uint32_t  rEP3_FIFO;			/*!           Address offset: 0x16C */
    __IO uint32_t  rEP4_FIFO;			/*!           Address offset: 0x170 */

    __IO uint32_t  RESERVED51; 			/*!           Address offset: 0x174 */
    __IO uint32_t  RESERVED52; 			/*!           Address offset: 0x178 */
    __IO uint32_t  RESERVED53; 			/*!           Address offset: 0x17C */

    __IO uint32_t  RESERVED54; 			/*!           Address offset: 0x180 */

    __IO uint32_t  rEP_DMA;				/*!           Address offset: 0x184 */
    __IO uint32_t  rEP_HALT;			/*!           Address offset: 0x188 */
    __IO uint32_t  RESERVED55; 			/*!           Address offset: 0x18C */

    __IO uint32_t  RESERVED56; 			/*!           Address offset: 0x190 */
    __IO uint32_t  RESERVED57; 			/*!           Address offset: 0x194 */
    __IO uint32_t  RESERVED58; 			/*!           Address offset: 0x198 */
    __IO uint32_t  RESERVED59; 			/*!           Address offset: 0x19C */

    __IO uint32_t  RESERVED60; 			/*!           Address offset: 0x1A0 */
    __IO uint32_t  RESERVED61; 			/*!           Address offset: 0x1A4 */
    __IO uint32_t  RESERVED62; 			/*!           Address offset: 0x1A8 */
    __IO uint32_t  RESERVED63; 			/*!           Address offset: 0x1AC */

    __IO uint32_t  RESERVED64; 			/*!           Address offset: 0x1B0 */
    __IO uint32_t  RESERVED65; 			/*!           Address offset: 0x1B4 */
    __IO uint32_t  RESERVED66; 			/*!           Address offset: 0x1B8 */
    __IO uint32_t  RESERVED67; 			/*!           Address offset: 0x1BC */
    __IO uint32_t  rPOWER;				/*!           Address offset: 0x1C0 */
} USB_TypeDef;

/******************************************************************************/
/*                                                                            */
/*                                   USB                                      */
/*                                                                            */
/******************************************************************************/

/*******************  Bit definition for USB_TOP register  *******************/
#define  USB_TOP_SPEED                         ((uint16_t)0x0001)
#define  USB_TOP_CONNECT                       ((uint16_t)0x0002)
#define  USB_TOP_RESET                         ((uint16_t)0x0008)
#define  USB_TOP_SUSPEND                       ((uint16_t)0x0010)
#define  USB_TOP_ACTIVE                        ((uint16_t)0x0080)

#define  USB_TOP_STATE                         ((uint16_t)0x0060)
#define  USB_TOP_STATE_0                       ((uint16_t)0x0020)
#define  USB_TOP_STATE_1                       ((uint16_t)0x0040)

/*******************  Bit definition for USB_INT_STATE register  *******************/
#define  USB_INT_STATE_RSTF                    ((uint16_t)0x0001)
#define  USB_INT_STATE_SUSPENDF                ((uint16_t)0x0002)
#define  USB_INT_STATE_RESUMF                  ((uint16_t)0x0004)
#define  USB_INT_STATE_SOFF                    ((uint16_t)0x0008)
#define  USB_INT_STATE_EPINTF                  ((uint16_t)0x0010)

/*******************  Bit definition for EP_INT_STATE register  *******************/
#define  EP_INT_STATE_EP0F                    ((uint16_t)0x0001)
#define  EP_INT_STATE_EP1F                    ((uint16_t)0x0002)
#define  EP_INT_STATE_EP2F                    ((uint16_t)0x0004)
#define  EP_INT_STATE_EP3F                    ((uint16_t)0x0008)
#define  EP_INT_STATE_EP4F                    ((uint16_t)0x0010)

/*******************  Bit definition for EP0_INT_STATE register  *******************/
#define  EPn_INT_STATE_SETUP                  ((uint16_t)0x0001)
#define  EPn_INT_STATE_END                    ((uint16_t)0x0002)
#define  EPn_INT_STATE_INNACK                 ((uint16_t)0x0004)
#define  EPn_INT_STATE_INACK                  ((uint16_t)0x0008)
#define  EPn_INT_STATE_INSTALL                ((uint16_t)0x0010)
#define  EPn_INT_STATE_OUTNACK                ((uint16_t)0x0020)
#define  EPn_INT_STATE_OUTACK                 ((uint16_t)0x0040)
#define  EPn_INT_STATE_OUTSTALL               ((uint16_t)0x0080)

/*******************  Bit definition for USB_INT_EN register  *******************/
#define  USB_INT_EN_RSTIE                     ((uint16_t)0x0001)
#define  USB_INT_EN_SUSPENDIE                 ((uint16_t)0x0002)
#define  USB_INT_EN_RESUMIE                   ((uint16_t)0x0004)
#define  USB_INT_EN_SOFIE                     ((uint16_t)0x0008)
#define  USB_INT_EN_EPINTIE                   ((uint16_t)0x0010)

/*******************  Bit definition for EP_INT_EN register  *******************/
#define  EP_INT_EN_EP0IE                      ((uint16_t)0x0001)
#define  EP_INT_EN_EP1IE                      ((uint16_t)0x0002)
#define  EP_INT_EN_EP2IE                      ((uint16_t)0x0004)
#define  EP_INT_EN_EP3IE                      ((uint16_t)0x0008)
#define  EP_INT_EN_EP4IE                      ((uint16_t)0x0010)

/*******************  Bit definition for EP0_INT_EN register  *******************/
#define  EPn_INT_EN_SETUPIE                   ((uint16_t)0x0001)
#define  EPn_INT_EN_ENDIE                     ((uint16_t)0x0002)
#define  EPn_INT_EN_INNACKIE                  ((uint16_t)0x0004)
#define  EPn_INT_EN_INACKIE                   ((uint16_t)0x0008)
#define  EPn_INT_EN_INSTALLIE                 ((uint16_t)0x0010)
#define  EPn_INT_EN_OUTNACKIE                 ((uint16_t)0x0020)
#define  EPn_INT_EN_OUTACKIE                  ((uint16_t)0x0040)
#define  EPn_INT_EN_OUTSTALLIE                ((uint16_t)0x0080)

///*******************  Bit definition for EP1_INT_STATE register  *******************/
//#define  EP1_INT_STATE_END                    ((uint16_t)0x0002)
//#define  EP1_INT_STATE_INNACK                 ((uint16_t)0x0004)
//#define  EP1_INT_STATE_INACK                  ((uint16_t)0x0008)
//#define  EP1_INT_STATE_INSTALL                ((uint16_t)0x0010)
//#define  EP1_INT_STATE_OUTNACK                ((uint16_t)0x0020)
//#define  EP1_INT_STATE_OUTACK                 ((uint16_t)0x0040)
//#define  EP1_INT_STATE_OUTSTALL               ((uint16_t)0x0080)

///*******************  Bit definition for EP2_INT_STATE register  *******************/
//#define  EP2_INT_STATE_END                    ((uint16_t)0x0002)
//#define  EP2_INT_STATE_INNACK                 ((uint16_t)0x0004)
//#define  EP2_INT_STATE_INACK                  ((uint16_t)0x0008)
//#define  EP2_INT_STATE_INSTALL                ((uint16_t)0x0010)
//#define  EP2_INT_STATE_OUTNACK                ((uint16_t)0x0020)
//#define  EP2_INT_STATE_OUTACK                 ((uint16_t)0x0040)
//#define  EP2_INT_STATE_OUTSTALL               ((uint16_t)0x0080)

///*******************  Bit definition for EP3_INT_STATE register  *******************/
//#define  EP3_INT_STATE_END                    ((uint16_t)0x0002)
//#define  EP3_INT_STATE_INNACK                 ((uint16_t)0x0004)
//#define  EP3_INT_STATE_INACK                  ((uint16_t)0x0008)
//#define  EP3_INT_STATE_INSTALL                ((uint16_t)0x0010)
//#define  EP3_INT_STATE_OUTNACK                ((uint16_t)0x0020)
//#define  EP3_INT_STATE_OUTACK                 ((uint16_t)0x0040)
//#define  EP3_INT_STATE_OUTSTALL               ((uint16_t)0x0080)

///*******************  Bit definition for EP4_INT_STATE register  *******************/
//#define  EP4_INT_STATE_END                    ((uint16_t)0x0002)
//#define  EP4_INT_STATE_INNACK                 ((uint16_t)0x0004)
//#define  EP4_INT_STATE_INACK                  ((uint16_t)0x0008)
//#define  EP4_INT_STATE_INSTALL                ((uint16_t)0x0010)
//#define  EP4_INT_STATE_OUTNACK                ((uint16_t)0x0020)
//#define  EP4_INT_STATE_OUTACK                 ((uint16_t)0x0040)
//#define  EP4_INT_STATE_OUTSTALL               ((uint16_t)0x0080)

///*******************  Bit definition for EP1_INT_EN register  *******************/
//#define  EPn_INT_EN_ENDIE                     ((uint16_t)0x0002)
//#define  EPn_INT_EN_INNACKIE                  ((uint16_t)0x0004)
//#define  EPn_INT_EN_INACKIE                   ((uint16_t)0x0008)
//#define  EPn_INT_EN_INSTALLIE                 ((uint16_t)0x0010)
//#define  EPn_INT_EN_OUTNACKIE                 ((uint16_t)0x0020)
//#define  EPn_INT_EN_OUTACKIE                  ((uint16_t)0x0040)
//#define  EPn_INT_EN_OUTSTALLIE                ((uint16_t)0x0080)

/*******************  Bit definition for USB_ADDR register  *******************/
#define  USB_ADDR_ADDR                        ((uint16_t)0x007F)

/*******************  Bit definition for EP_EN register  *******************/
#define  EP_EN_EP0EN                        ((uint16_t)0x0001)
#define  EP_EN_EP1EN                        ((uint16_t)0x0002)
#define  EP_EN_EP2EN                        ((uint16_t)0x0004)
#define  EP_EN_EP3EN                        ((uint16_t)0x0008)
#define  EP_EN_EP4EN                        ((uint16_t)0x0010)

/*******************  Bit definition for TOG_CTRL1_4 register  *******************/
#define  TOG_CTRL1_4_DTOG1                  ((uint16_t)0x0001)
#define  TOG_CTRL1_4_DTOG1EN                ((uint16_t)0x0002)
#define  TOG_CTRL1_4_DTOG2                  ((uint16_t)0x0004)
#define  TOG_CTRL1_4_DTOG2EN                ((uint16_t)0x0008)
#define  TOG_CTRL1_4_DTOG3                  ((uint16_t)0x0010)
#define  TOG_CTRL1_4_DTOG3EN                ((uint16_t)0x0020)
#define  TOG_CTRL1_4_DTOG4                  ((uint16_t)0x0040)
#define  TOG_CTRL1_4_DTOG4EN                ((uint16_t)0x0080)

/*******************  Bit definition for SETUP0 register  *******************/
#define  SETUP0                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP1 register  *******************/
#define  SETUP1                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP2 register  *******************/
#define  SETUP2                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP3 register  *******************/
#define  SETUP3                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP4 register  *******************/
#define  SETUP4                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP5 register  *******************/
#define  SETUP5                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP6 register  *******************/
#define  SETUP6                             ((uint16_t)0x00FF)

/*******************  Bit definition for SETUP7 register  *******************/
#define  SETUP7                             ((uint16_t)0x00FF)

/*******************  Bit definition for PACKET_SIZE1 register  *******************/
#define  PACKET_SIZE1                        ((uint16_t)0x00FF)

/*******************  Bit definition for PACKET_SIZE2 register  *******************/
#define  PACKET_SIZE2                        ((uint16_t)0x00FF)

/*******************  Bit definition for EP0_AVIL register  *******************/
#define  EP0_AVIL_EPXAVIL                   ((uint16_t)0x00FF)

/*******************  Bit definition for EP1_AVIL register  *******************/
#define  EP1_AVIL_EPXAVIL                   ((uint16_t)0x00FF)

/*******************  Bit definition for EP2_AVIL register  *******************/
#define  EP2_AVIL_EPXAVIL                   ((uint16_t)0x00FF)

/*******************  Bit definition for EP3_AVIL register  *******************/
#define  EP3_AVIL_EPXAVIL                   ((uint16_t)0x00FF)

/*******************  Bit definition for EP4_AVIL register  *******************/
#define  EP4_AVIL_EPXAVIL                   ((uint16_t)0x00FF)

/*******************  Bit definition for EP0_CTRL register  *******************/
#define  EP0_CTRL_TRANEN                    ((uint16_t)0x0080)

#define  EP0_CTRL_TRANCOUNT                 ((uint16_t)0x007F)
#define  EP0_CTRL_TRANCOUNT_0               ((uint16_t)0x0001)
#define  EP0_CTRL_TRANCOUNT_1               ((uint16_t)0x0002)
#define  EP0_CTRL_TRANCOUNT_2               ((uint16_t)0x0004)
#define  EP0_CTRL_TRANCOUNT_3               ((uint16_t)0x0008)
#define  EP0_CTRL_TRANCOUNT_4               ((uint16_t)0x0010)
#define  EP0_CTRL_TRANCOUNT_5               ((uint16_t)0x0020)
#define  EP0_CTRL_TRANCOUNT_6               ((uint16_t)0x0040)

/*******************  Bit definition for EP1_CTRL register  *******************/
#define  EP1_CTRL_TRANEN                    ((uint16_t)0x0080)

#define  EP1_CTRL_TRANCOUNT                 ((uint16_t)0x007F)
#define  EP1_CTRL_TRANCOUNT_0               ((uint16_t)0x0001)
#define  EP1_CTRL_TRANCOUNT_1               ((uint16_t)0x0002)
#define  EP1_CTRL_TRANCOUNT_2               ((uint16_t)0x0004)
#define  EP1_CTRL_TRANCOUNT_3               ((uint16_t)0x0008)
#define  EP1_CTRL_TRANCOUNT_4               ((uint16_t)0x0010)
#define  EP1_CTRL_TRANCOUNT_5               ((uint16_t)0x0020)
#define  EP1_CTRL_TRANCOUNT_6               ((uint16_t)0x0040)

/*******************  Bit definition for EP2_CTRL register  *******************/
#define  EP2_CTRL_TRANEN                    ((uint16_t)0x0080)

#define  EP2_CTRL_TRANCOUNT                 ((uint16_t)0x007F)
#define  EP2_CTRL_TRANCOUNT_0               ((uint16_t)0x0001)
#define  EP2_CTRL_TRANCOUNT_1               ((uint16_t)0x0002)
#define  EP2_CTRL_TRANCOUNT_2               ((uint16_t)0x0004)
#define  EP2_CTRL_TRANCOUNT_3               ((uint16_t)0x0008)
#define  EP2_CTRL_TRANCOUNT_4               ((uint16_t)0x0010)
#define  EP2_CTRL_TRANCOUNT_5               ((uint16_t)0x0020)
#define  EP2_CTRL_TRANCOUNT_6               ((uint16_t)0x0040)

/*******************  Bit definition for EP3_CTRL register  *******************/
#define  EP3_CTRL_TRANEN                    ((uint16_t)0x0080)

#define  EP3_CTRL_TRANCOUNT                 ((uint16_t)0x007F)
#define  EP3_CTRL_TRANCOUNT_0               ((uint16_t)0x0001)
#define  EP3_CTRL_TRANCOUNT_1               ((uint16_t)0x0002)
#define  EP3_CTRL_TRANCOUNT_2               ((uint16_t)0x0004)
#define  EP3_CTRL_TRANCOUNT_3               ((uint16_t)0x0008)
#define  EP3_CTRL_TRANCOUNT_4               ((uint16_t)0x0010)
#define  EP3_CTRL_TRANCOUNT_5               ((uint16_t)0x0020)
#define  EP3_CTRL_TRANCOUNT_6               ((uint16_t)0x0040)

/*******************  Bit definition for EP4_CTRL register  *******************/
#define  EP4_CTRL_TRANEN                    ((uint16_t)0x0080)

#define  EP4_CTRL_TRANCOUNT                 ((uint16_t)0x007F)
#define  EP4_CTRL_TRANCOUNT_0               ((uint16_t)0x0001)
#define  EP4_CTRL_TRANCOUNT_1               ((uint16_t)0x0002)
#define  EP4_CTRL_TRANCOUNT_2               ((uint16_t)0x0004)
#define  EP4_CTRL_TRANCOUNT_3               ((uint16_t)0x0008)
#define  EP4_CTRL_TRANCOUNT_4               ((uint16_t)0x0010)
#define  EP4_CTRL_TRANCOUNT_5               ((uint16_t)0x0020)
#define  EP4_CTRL_TRANCOUNT_6               ((uint16_t)0x0040)

/*******************  Bit definition for EP_DMA register  *******************/
#define  EP_DMA_DMA1EN                      ((uint16_t)0x0001)
#define  EP_DMA_DMA2EN                      ((uint16_t)0x0002)

/*******************  Bit definition for EP_HALT register  *******************/
#define  EP_HALT_HALT0                      ((uint16_t)0x0001)
#define  EP_HALT_HALT1                      ((uint16_t)0x0002)
#define  EP_HALT_HALT2                      ((uint16_t)0x0004)
#define  EP_HALT_HALT3                      ((uint16_t)0x0008)
#define  EP_HALT_HALT4                      ((uint16_t)0x0010)

/*******************  Bit definition for USB_POWER register  *******************/
#define  USB_POWER_SUSPEN                   ((uint16_t)0x0001)
#define  USB_POWER_SUSP                     ((uint16_t)0x0002)
#define  USB_POWER_WKUP                     ((uint16_t)0x0008)

#endif
