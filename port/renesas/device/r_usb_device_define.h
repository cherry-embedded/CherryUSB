/*
 * Copyright (c) 2020 - 2024 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef R_USB_DEVICE_DEFINE_H
#define R_USB_DEVICE_DEFINE_H

#include "r_usb_device.h"

#define USB_NOT_SUPPORT    (-1)
#define USB_FS_MODULE      (0U)
#define USB_HS_MODULE      (1U)

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#if BSP_FEATURE_USB_HAS_USBHS == 1U

 #define USB_HIGH_SPEED_MODULE
 #define USB_IP0_MODULE    USB_FS_MODULE
 #define USB_IP1_MODULE    USB_HS_MODULE
 #define USB_NUM_USBIP     2

 #define USB_IS_USBHS(usbip)    ((usbip) == USB_HS_MODULE)

#else

 #define USB_IP0_MODULE    USB_FS_MODULE
 #define USB_IP1_MODULE    USB_NOT_SUPPORT
 #define USB_NUM_USBIP     1

#endif

/* USBFS & USBHS Register definition */

/* PIPE_TR E */
#define R_USB_PIPE_TR_E_TRENB_Pos                 (9UL)        /* TRENB (Bit 9) */
#define R_USB_PIPE_TR_E_TRENB_Msk                 (0x200UL)    /* TRENB (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_TR_E_TRCLR_Pos                 (8UL)        /* TRCLR (Bit 8) */
#define R_USB_PIPE_TR_E_TRCLR_Msk                 (0x100UL)    /* TRCLR (Bitfield-Mask: 0x01) */

/* PIPE_TR N */
#define R_USB_PIPE_TR_N_TRNCNT_Pos                (0UL)        /* TRNCNT (Bit 0) */
#define R_USB_PIPE_TR_N_TRNCNT_Msk                (0xffffUL)   /* TRNCNT (Bitfield-Mask: 0xffff) */

/* SYSCFG */
#define R_USB_SYSCFG_SCKE_Pos                     (10UL)       /* SCKE (Bit 10) */
#define R_USB_SYSCFG_SCKE_Msk                     (0x400UL)    /* SCKE (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_CNEN_Pos                     (8UL)        /* CNEN (Bit 8) */
#define R_USB_SYSCFG_CNEN_Msk                     (0x100UL)    /* CNEN (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_HSE_Pos                      (7UL)        /* HSE (Bit 7) */
#define R_USB_SYSCFG_HSE_Msk                      (0x80UL)     /* HSE (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_DCFM_Pos                     (6UL)        /* DCFM (Bit 6) */
#define R_USB_SYSCFG_DCFM_Msk                     (0x40UL)     /* DCFM (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_DRPD_Pos                     (5UL)        /* DRPD (Bit 5) */
#define R_USB_SYSCFG_DRPD_Msk                     (0x20UL)     /* DRPD (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_DPRPU_Pos                    (4UL)        /* DPRPU (Bit 4) */
#define R_USB_SYSCFG_DPRPU_Msk                    (0x10UL)     /* DPRPU (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_DMRPU_Pos                    (3UL)        /* DMRPU (Bit 3) */
#define R_USB_SYSCFG_DMRPU_Msk                    (0x8UL)      /* DMRPU (Bitfield-Mask: 0x01) */
#define R_USB_SYSCFG_USBE_Pos                     (0UL)        /* USBE (Bit 0) */
#define R_USB_SYSCFG_USBE_Msk                     (0x1UL)      /* USBE (Bitfield-Mask: 0x01) */

/* BUSWAIT */
#define R_USB_BUSWAIT_BWAIT_Pos                   (0UL)        /* BWAIT (Bit 0) */
#define R_USB_BUSWAIT_BWAIT_Msk                   (0xfUL)      /* BWAIT (Bitfield-Mask: 0x0f) */

/* SYSSTS0 */
#define R_USB_SYSSTS0_OVCMON_Pos                  (14UL)       /* OVCMON (Bit 14) */
#define R_USB_SYSSTS0_OVCMON_Msk                  (0xc000UL)   /* OVCMON (Bitfield-Mask: 0x03) */
#define R_USB_SYSSTS0_HTACT_Pos                   (6UL)        /* HTACT (Bit 6) */
#define R_USB_SYSSTS0_HTACT_Msk                   (0x40UL)     /* HTACT (Bitfield-Mask: 0x01) */
#define R_USB_SYSSTS0_SOFEA_Pos                   (5UL)        /* SOFEA (Bit 5) */
#define R_USB_SYSSTS0_SOFEA_Msk                   (0x20UL)     /* SOFEA (Bitfield-Mask: 0x01) */
#define R_USB_SYSSTS0_IDMON_Pos                   (2UL)        /* IDMON (Bit 2) */
#define R_USB_SYSSTS0_IDMON_Msk                   (0x4UL)      /* IDMON (Bitfield-Mask: 0x01) */
#define R_USB_SYSSTS0_LNST_Pos                    (0UL)        /* LNST (Bit 0) */
#define R_USB_SYSSTS0_LNST_Msk                    (0x3UL)      /* LNST (Bitfield-Mask: 0x03) */

/* PLLSTA */
#define R_USB_PLLSTA_PLLLOCK_Pos                  (0UL)        /* PLLLOCK (Bit 0) */
#define R_USB_PLLSTA_PLLLOCK_Msk                  (0x1UL)      /* PLLLOCK (Bitfield-Mask: 0x01) */

/* DVSTCTR0 */
#define R_USB_DVSTCTR0_HNPBTOA_Pos                (11UL)       /* HNPBTOA (Bit 11) */
#define R_USB_DVSTCTR0_HNPBTOA_Msk                (0x800UL)    /* HNPBTOA (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_EXICEN_Pos                 (10UL)       /* EXICEN (Bit 10) */
#define R_USB_DVSTCTR0_EXICEN_Msk                 (0x400UL)    /* EXICEN (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_VBUSEN_Pos                 (9UL)        /* VBUSEN (Bit 9) */
#define R_USB_DVSTCTR0_VBUSEN_Msk                 (0x200UL)    /* VBUSEN (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_WKUP_Pos                   (8UL)        /* WKUP (Bit 8) */
#define R_USB_DVSTCTR0_WKUP_Msk                   (0x100UL)    /* WKUP (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_RWUPE_Pos                  (7UL)        /* RWUPE (Bit 7) */
#define R_USB_DVSTCTR0_RWUPE_Msk                  (0x80UL)     /* RWUPE (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_USBRST_Pos                 (6UL)        /* USBRST (Bit 6) */
#define R_USB_DVSTCTR0_USBRST_Msk                 (0x40UL)     /* USBRST (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_RESUME_Pos                 (5UL)        /* RESUME (Bit 5) */
#define R_USB_DVSTCTR0_RESUME_Msk                 (0x20UL)     /* RESUME (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_UACT_Pos                   (4UL)        /* UACT (Bit 4) */
#define R_USB_DVSTCTR0_UACT_Msk                   (0x10UL)     /* UACT (Bitfield-Mask: 0x01) */
#define R_USB_DVSTCTR0_RHST_Pos                   (0UL)        /* RHST (Bit 0) */
#define R_USB_DVSTCTR0_RHST_Msk                   (0x7UL)      /* RHST (Bitfield-Mask: 0x07) */

/* TESTMODE */
#define R_USB_TESTMODE_UTST_Pos                   (0UL)        /* UTST (Bit 0) */
#define R_USB_TESTMODE_UTST_Msk                   (0xfUL)      /* UTST (Bitfield-Mask: 0x0f) */

/* CFIFOSEL */
#define R_USB_CFIFOSEL_RCNT_Pos                   (15UL)       /* RCNT (Bit 15) */
#define R_USB_CFIFOSEL_RCNT_Msk                   (0x8000UL)   /* RCNT (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOSEL_REW_Pos                    (14UL)       /* REW (Bit 14) */
#define R_USB_CFIFOSEL_REW_Msk                    (0x4000UL)   /* REW (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOSEL_MBW_Pos                    (10UL)       /* MBW (Bit 10) */
#define R_USB_CFIFOSEL_MBW_Msk                    (0xc00UL)    /* MBW (Bitfield-Mask: 0x03) */
#define R_USB_CFIFOSEL_BIGEND_Pos                 (8UL)        /* BIGEND (Bit 8) */
#define R_USB_CFIFOSEL_BIGEND_Msk                 (0x100UL)    /* BIGEND (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOSEL_ISEL_Pos                   (5UL)        /* ISEL (Bit 5) */
#define R_USB_CFIFOSEL_ISEL_Msk                   (0x20UL)     /* ISEL (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOSEL_CURPIPE_Pos                (0UL)        /* CURPIPE (Bit 0) */
#define R_USB_CFIFOSEL_CURPIPE_Msk                (0xfUL)      /* CURPIPE (Bitfield-Mask: 0x0f) */

/* CFIFOCTR */
#define R_USB_CFIFOCTR_BVAL_Pos                   (15UL)       /* BVAL (Bit 15) */
#define R_USB_CFIFOCTR_BVAL_Msk                   (0x8000UL)   /* BVAL (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOCTR_BCLR_Pos                   (14UL)       /* BCLR (Bit 14) */
#define R_USB_CFIFOCTR_BCLR_Msk                   (0x4000UL)   /* BCLR (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOCTR_FRDY_Pos                   (13UL)       /* FRDY (Bit 13) */
#define R_USB_CFIFOCTR_FRDY_Msk                   (0x2000UL)   /* FRDY (Bitfield-Mask: 0x01) */
#define R_USB_CFIFOCTR_DTLN_Pos                   (0UL)        /* DTLN (Bit 0) */
#define R_USB_CFIFOCTR_DTLN_Msk                   (0xfffUL)    /* DTLN (Bitfield-Mask: 0xfff) */

/* D0FIFOSEL */
#define R_USB_D0FIFOSEL_RCNT_Pos                  (15UL)       /* RCNT (Bit 15) */
#define R_USB_D0FIFOSEL_RCNT_Msk                  (0x8000UL)   /* RCNT (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOSEL_REW_Pos                   (14UL)       /* REW (Bit 14) */
#define R_USB_D0FIFOSEL_REW_Msk                   (0x4000UL)   /* REW (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOSEL_DCLRM_Pos                 (13UL)       /* DCLRM (Bit 13) */
#define R_USB_D0FIFOSEL_DCLRM_Msk                 (0x2000UL)   /* DCLRM (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOSEL_DREQE_Pos                 (12UL)       /* DREQE (Bit 12) */
#define R_USB_D0FIFOSEL_DREQE_Msk                 (0x1000UL)   /* DREQE (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOSEL_MBW_Pos                   (10UL)       /* MBW (Bit 10) */
#define R_USB_D0FIFOSEL_MBW_Msk                   (0xc00UL)    /* MBW (Bitfield-Mask: 0x03) */
#define R_USB_D0FIFOSEL_BIGEND_Pos                (8UL)        /* BIGEND (Bit 8) */
#define R_USB_D0FIFOSEL_BIGEND_Msk                (0x100UL)    /* BIGEND (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOSEL_CURPIPE_Pos               (0UL)        /* CURPIPE (Bit 0) */
#define R_USB_D0FIFOSEL_CURPIPE_Msk               (0xfUL)      /* CURPIPE (Bitfield-Mask: 0x0f) */

/* D0FIFOCTR */
#define R_USB_D0FIFOCTR_BVAL_Pos                  (15UL)       /* BVAL (Bit 15) */
#define R_USB_D0FIFOCTR_BVAL_Msk                  (0x8000UL)   /* BVAL (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOCTR_BCLR_Pos                  (14UL)       /* BCLR (Bit 14) */
#define R_USB_D0FIFOCTR_BCLR_Msk                  (0x4000UL)   /* BCLR (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOCTR_FRDY_Pos                  (13UL)       /* FRDY (Bit 13) */
#define R_USB_D0FIFOCTR_FRDY_Msk                  (0x2000UL)   /* FRDY (Bitfield-Mask: 0x01) */
#define R_USB_D0FIFOCTR_DTLN_Pos                  (0UL)        /* DTLN (Bit 0) */
#define R_USB_D0FIFOCTR_DTLN_Msk                  (0xfffUL)    /* DTLN (Bitfield-Mask: 0xfff) */

/* D1FIFOSEL */
#define R_USB_D1FIFOSEL_RCNT_Pos                  (15UL)       /* RCNT (Bit 15) */
#define R_USB_D1FIFOSEL_RCNT_Msk                  (0x8000UL)   /* RCNT (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOSEL_REW_Pos                   (14UL)       /* REW (Bit 14) */
#define R_USB_D1FIFOSEL_REW_Msk                   (0x4000UL)   /* REW (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOSEL_DCLRM_Pos                 (13UL)       /* DCLRM (Bit 13) */
#define R_USB_D1FIFOSEL_DCLRM_Msk                 (0x2000UL)   /* DCLRM (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOSEL_DREQE_Pos                 (12UL)       /* DREQE (Bit 12) */
#define R_USB_D1FIFOSEL_DREQE_Msk                 (0x1000UL)   /* DREQE (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOSEL_MBW_Pos                   (10UL)       /* MBW (Bit 10) */
#define R_USB_D1FIFOSEL_MBW_Msk                   (0xc00UL)    /* MBW (Bitfield-Mask: 0x03) */
#define R_USB_D1FIFOSEL_BIGEND_Pos                (8UL)        /* BIGEND (Bit 8) */
#define R_USB_D1FIFOSEL_BIGEND_Msk                (0x100UL)    /* BIGEND (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOSEL_CURPIPE_Pos               (0UL)        /* CURPIPE (Bit 0) */
#define R_USB_D1FIFOSEL_CURPIPE_Msk               (0xfUL)      /* CURPIPE (Bitfield-Mask: 0x0f) */

/* D1FIFOCTR */
#define R_USB_D1FIFOCTR_BVAL_Pos                  (15UL)       /* BVAL (Bit 15) */
#define R_USB_D1FIFOCTR_BVAL_Msk                  (0x8000UL)   /* BVAL (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOCTR_BCLR_Pos                  (14UL)       /* BCLR (Bit 14) */
#define R_USB_D1FIFOCTR_BCLR_Msk                  (0x4000UL)   /* BCLR (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOCTR_FRDY_Pos                  (13UL)       /* FRDY (Bit 13) */
#define R_USB_D1FIFOCTR_FRDY_Msk                  (0x2000UL)   /* FRDY (Bitfield-Mask: 0x01) */
#define R_USB_D1FIFOCTR_DTLN_Pos                  (0UL)        /* DTLN (Bit 0) */
#define R_USB_D1FIFOCTR_DTLN_Msk                  (0xfffUL)    /* DTLN (Bitfield-Mask: 0xfff) */

/* INTENB0 */
#define R_USB_INTENB0_VBSE_Pos                    (15UL)       /* VBSE (Bit 15) */
#define R_USB_INTENB0_VBSE_Msk                    (0x8000UL)   /* VBSE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_RSME_Pos                    (14UL)       /* RSME (Bit 14) */
#define R_USB_INTENB0_RSME_Msk                    (0x4000UL)   /* RSME (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_SOFE_Pos                    (13UL)       /* SOFE (Bit 13) */
#define R_USB_INTENB0_SOFE_Msk                    (0x2000UL)   /* SOFE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_DVSE_Pos                    (12UL)       /* DVSE (Bit 12) */
#define R_USB_INTENB0_DVSE_Msk                    (0x1000UL)   /* DVSE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_CTRE_Pos                    (11UL)       /* CTRE (Bit 11) */
#define R_USB_INTENB0_CTRE_Msk                    (0x800UL)    /* CTRE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_BEMPE_Pos                   (10UL)       /* BEMPE (Bit 10) */
#define R_USB_INTENB0_BEMPE_Msk                   (0x400UL)    /* BEMPE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_NRDYE_Pos                   (9UL)        /* NRDYE (Bit 9) */
#define R_USB_INTENB0_NRDYE_Msk                   (0x200UL)    /* NRDYE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB0_BRDYE_Pos                   (8UL)        /* BRDYE (Bit 8) */
#define R_USB_INTENB0_BRDYE_Msk                   (0x100UL)    /* BRDYE (Bitfield-Mask: 0x01) */

/* INTENB1 */
#define R_USB_INTENB1_OVRCRE_Pos                  (15UL)       /* OVRCRE (Bit 15) */
#define R_USB_INTENB1_OVRCRE_Msk                  (0x8000UL)   /* OVRCRE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_BCHGE_Pos                   (14UL)       /* BCHGE (Bit 14) */
#define R_USB_INTENB1_BCHGE_Msk                   (0x4000UL)   /* BCHGE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_DTCHE_Pos                   (12UL)       /* DTCHE (Bit 12) */
#define R_USB_INTENB1_DTCHE_Msk                   (0x1000UL)   /* DTCHE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_ATTCHE_Pos                  (11UL)       /* ATTCHE (Bit 11) */
#define R_USB_INTENB1_ATTCHE_Msk                  (0x800UL)    /* ATTCHE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_L1RSMENDE_Pos               (9UL)        /*!< L1RSMENDE (Bit 9)                                     */
#define R_USB_INTENB1_L1RSMENDE_Msk               (0x200UL)    /*!< L1RSMENDE (Bitfield-Mask: 0x01)                       */
#define R_USB_INTENB1_LPMENDE_Pos                 (8UL)        /*!< LPMENDE (Bit 8)                                       */
#define R_USB_INTENB1_LPMENDE_Msk                 (0x100UL)    /*!< LPMENDE (Bitfield-Mask: 0x01)                         */
#define R_USB_INTENB1_EOFERRE_Pos                 (6UL)        /* EOFERRE (Bit 6) */
#define R_USB_INTENB1_EOFERRE_Msk                 (0x40UL)     /* EOFERRE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_SIGNE_Pos                   (5UL)        /* SIGNE (Bit 5) */
#define R_USB_INTENB1_SIGNE_Msk                   (0x20UL)     /* SIGNE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_SACKE_Pos                   (4UL)        /* SACKE (Bit 4) */
#define R_USB_INTENB1_SACKE_Msk                   (0x10UL)     /* SACKE (Bitfield-Mask: 0x01) */
#define R_USB_INTENB1_PDDETINTE0_Pos              (0UL)        /* PDDETINTE0 (Bit 0) */
#define R_USB_INTENB1_PDDETINTE0_Msk              (0x1UL)      /* PDDETINTE0 (Bitfield-Mask: 0x01) */

/* BRDYENB */
#define R_USB_BRDYENB_PIPEBRDYE_Pos               (0UL)        /* PIPEBRDYE (Bit 0) */
#define R_USB_BRDYENB_PIPEBRDYE_Msk               (0x1UL)      /* PIPEBRDYE (Bitfield-Mask: 0x01) */

/* NRDYENB */
#define R_USB_NRDYENB_PIPENRDYE_Pos               (0UL)        /* PIPENRDYE (Bit 0) */
#define R_USB_NRDYENB_PIPENRDYE_Msk               (0x1UL)      /* PIPENRDYE (Bitfield-Mask: 0x01) */

/* BEMPENB */
#define R_USB_BEMPENB_PIPEBEMPE_Pos               (0UL)        /* PIPEBEMPE (Bit 0) */
#define R_USB_BEMPENB_PIPEBEMPE_Msk               (0x1UL)      /* PIPEBEMPE (Bitfield-Mask: 0x01) */

/* SOFCFG */
#define R_USB_SOFCFG_TRNENSEL_Pos                 (8UL)        /* TRNENSEL (Bit 8) */
#define R_USB_SOFCFG_TRNENSEL_Msk                 (0x100UL)    /* TRNENSEL (Bitfield-Mask: 0x01) */
#define R_USB_SOFCFG_BRDYM_Pos                    (6UL)        /* BRDYM (Bit 6) */
#define R_USB_SOFCFG_BRDYM_Msk                    (0x40UL)     /* BRDYM (Bitfield-Mask: 0x01) */
#define R_USB_SOFCFG_INTL_Pos                     (5UL)        /* INTL (Bit 5) */
#define R_USB_SOFCFG_INTL_Msk                     (0x20UL)     /* INTL (Bitfield-Mask: 0x01) */
#define R_USB_SOFCFG_EDGESTS_Pos                  (4UL)        /* EDGESTS (Bit 4) */
#define R_USB_SOFCFG_EDGESTS_Msk                  (0x10UL)     /* EDGESTS (Bitfield-Mask: 0x01) */

/* PHYSET */
#define R_USB_PHYSET_HSEB_Pos                     (15UL)       /* HSEB (Bit 15) */
#define R_USB_PHYSET_HSEB_Msk                     (0x8000UL)   /* HSEB (Bitfield-Mask: 0x01) */
#define R_USB_PHYSET_REPSTART_Pos                 (11UL)       /* REPSTART (Bit 11) */
#define R_USB_PHYSET_REPSTART_Msk                 (0x800UL)    /* REPSTART (Bitfield-Mask: 0x01) */
#define R_USB_PHYSET_REPSEL_Pos                   (8UL)        /* REPSEL (Bit 8) */
#define R_USB_PHYSET_REPSEL_Msk                   (0x300UL)    /* REPSEL (Bitfield-Mask: 0x03) */
#define R_USB_PHYSET_CLKSEL_Pos                   (4UL)        /* CLKSEL (Bit 4) */
#define R_USB_PHYSET_CLKSEL_Msk                   (0x30UL)     /* CLKSEL (Bitfield-Mask: 0x03) */
#define R_USB_PHYSET_CDPEN_Pos                    (3UL)        /* CDPEN (Bit 3) */
#define R_USB_PHYSET_CDPEN_Msk                    (0x8UL)      /* CDPEN (Bitfield-Mask: 0x01) */
#define R_USB_PHYSET_PLLRESET_Pos                 (1UL)        /* PLLRESET (Bit 1) */
#define R_USB_PHYSET_PLLRESET_Msk                 (0x2UL)      /* PLLRESET (Bitfield-Mask: 0x01) */
#define R_USB_PHYSET_DIRPD_Pos                    (0UL)        /* DIRPD (Bit 0) */
#define R_USB_PHYSET_DIRPD_Msk                    (0x1UL)      /* DIRPD (Bitfield-Mask: 0x01) */

/* INTSTS0 */
#define R_USB_INTSTS0_VBINT_Pos                   (15UL)       /* VBINT (Bit 15) */
#define R_USB_INTSTS0_VBINT_Msk                   (0x8000UL)   /* VBINT (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_RESM_Pos                    (14UL)       /* RESM (Bit 14) */
#define R_USB_INTSTS0_RESM_Msk                    (0x4000UL)   /* RESM (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_SOFR_Pos                    (13UL)       /* SOFR (Bit 13) */
#define R_USB_INTSTS0_SOFR_Msk                    (0x2000UL)   /* SOFR (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_DVST_Pos                    (12UL)       /* DVST (Bit 12) */
#define R_USB_INTSTS0_DVST_Msk                    (0x1000UL)   /* DVST (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_CTRT_Pos                    (11UL)       /* CTRT (Bit 11) */
#define R_USB_INTSTS0_CTRT_Msk                    (0x800UL)    /* CTRT (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_BEMP_Pos                    (10UL)       /* BEMP (Bit 10) */
#define R_USB_INTSTS0_BEMP_Msk                    (0x400UL)    /* BEMP (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_NRDY_Pos                    (9UL)        /* NRDY (Bit 9) */
#define R_USB_INTSTS0_NRDY_Msk                    (0x200UL)    /* NRDY (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_BRDY_Pos                    (8UL)        /* BRDY (Bit 8) */
#define R_USB_INTSTS0_BRDY_Msk                    (0x100UL)    /* BRDY (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_VBSTS_Pos                   (7UL)        /* VBSTS (Bit 7) */
#define R_USB_INTSTS0_VBSTS_Msk                   (0x80UL)     /* VBSTS (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_DVSQ_Pos                    (4UL)        /* DVSQ (Bit 4) */
#define R_USB_INTSTS0_DVSQ_Msk                    (0x70UL)     /* DVSQ (Bitfield-Mask: 0x07) */
#define R_USB_INTSTS0_VALID_Pos                   (3UL)        /* VALID (Bit 3) */
#define R_USB_INTSTS0_VALID_Msk                   (0x8UL)      /* VALID (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS0_CTSQ_Pos                    (0UL)        /* CTSQ (Bit 0) */
#define R_USB_INTSTS0_CTSQ_Msk                    (0x7UL)      /* CTSQ (Bitfield-Mask: 0x07) */

/* INTSTS1 */
#define R_USB_INTSTS1_OVRCR_Pos                   (15UL)       /* OVRCR (Bit 15) */
#define R_USB_INTSTS1_OVRCR_Msk                   (0x8000UL)   /* OVRCR (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_BCHG_Pos                    (14UL)       /* BCHG (Bit 14) */
#define R_USB_INTSTS1_BCHG_Msk                    (0x4000UL)   /* BCHG (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_DTCH_Pos                    (12UL)       /* DTCH (Bit 12) */
#define R_USB_INTSTS1_DTCH_Msk                    (0x1000UL)   /* DTCH (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_ATTCH_Pos                   (11UL)       /* ATTCH (Bit 11) */
#define R_USB_INTSTS1_ATTCH_Msk                   (0x800UL)    /* ATTCH (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_L1RSMEND_Pos                (9UL)        /* L1RSMEND (Bit 9) */
#define R_USB_INTSTS1_L1RSMEND_Msk                (0x200UL)    /* L1RSMEND (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_LPMEND_Pos                  (8UL)        /* LPMEND (Bit 8) */
#define R_USB_INTSTS1_LPMEND_Msk                  (0x100UL)    /* LPMEND (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_EOFERR_Pos                  (6UL)        /* EOFERR (Bit 6) */
#define R_USB_INTSTS1_EOFERR_Msk                  (0x40UL)     /* EOFERR (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_SIGN_Pos                    (5UL)        /* SIGN (Bit 5) */
#define R_USB_INTSTS1_SIGN_Msk                    (0x20UL)     /* SIGN (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_SACK_Pos                    (4UL)        /* SACK (Bit 4) */
#define R_USB_INTSTS1_SACK_Msk                    (0x10UL)     /* SACK (Bitfield-Mask: 0x01) */
#define R_USB_INTSTS1_PDDETINT0_Pos               (0UL)        /* PDDETINT0 (Bit 0) */
#define R_USB_INTSTS1_PDDETINT0_Msk               (0x1UL)      /* PDDETINT0 (Bitfield-Mask: 0x01) */

/* BRDYSTS */
#define R_USB_BRDYSTS_PIPEBRDY_Pos                (0UL)        /* PIPEBRDY (Bit 0) */
#define R_USB_BRDYSTS_PIPEBRDY_Msk                (0x3ffUL)    /* PIPEBRDY (Bitfield-Mask: 0x01) */

/* NRDYSTS */
#define R_USB_NRDYSTS_PIPENRDY_Pos                (0UL)        /* PIPENRDY (Bit 0) */
#define R_USB_NRDYSTS_PIPENRDY_Msk                (0x1UL)      /* PIPENRDY (Bitfield-Mask: 0x01) */

/* BEMPSTS */
#define R_USB_BEMPSTS_PIPEBEMP_Pos                (0UL)        /* PIPEBEMP (Bit 0) */
#define R_USB_BEMPSTS_PIPEBEMP_Msk                (0x1UL)      /* PIPEBEMP (Bitfield-Mask: 0x01) */

/* FRMNUM */
#define R_USB_FRMNUM_OVRN_Pos                     (15UL)       /* OVRN (Bit 15) */
#define R_USB_FRMNUM_OVRN_Msk                     (0x8000UL)   /* OVRN (Bitfield-Mask: 0x01) */
#define R_USB_FRMNUM_CRCE_Pos                     (14UL)       /* CRCE (Bit 14) */
#define R_USB_FRMNUM_CRCE_Msk                     (0x4000UL)   /* CRCE (Bitfield-Mask: 0x01) */
#define R_USB_FRMNUM_FRNM_Pos                     (0UL)        /* FRNM (Bit 0) */
#define R_USB_FRMNUM_FRNM_Msk                     (0x7ffUL)    /* FRNM (Bitfield-Mask: 0x7ff) */

/* UFRMNUM */
#define R_USB_UFRMNUM_DVCHG_Pos                   (15UL)       /* DVCHG (Bit 15) */
#define R_USB_UFRMNUM_DVCHG_Msk                   (0x8000UL)   /* DVCHG (Bitfield-Mask: 0x01) */
#define R_USB_UFRMNUM_UFRNM_Pos                   (0UL)        /* UFRNM (Bit 0) */
#define R_USB_UFRMNUM_UFRNM_Msk                   (0x7UL)      /* UFRNM (Bitfield-Mask: 0x07) */

/* USBADDR */
#define R_USB_USBADDR_STSRECOV0_Pos               (8UL)        /* STSRECOV0 (Bit 8) */
#define R_USB_USBADDR_STSRECOV0_Msk               (0x700UL)    /* STSRECOV0 (Bitfield-Mask: 0x07) */
#define R_USB_USBADDR_USBADDR_Pos                 (0UL)        /* USBADDR (Bit 0) */
#define R_USB_USBADDR_USBADDR_Msk                 (0x7fUL)     /* USBADDR (Bitfield-Mask: 0x7f) */

/* USBREQ */
#define R_USB_USBREQ_BREQUEST_Pos                 (8UL)        /* BREQUEST (Bit 8) */
#define R_USB_USBREQ_BREQUEST_Msk                 (0xff00UL)   /* BREQUEST (Bitfield-Mask: 0xff) */
#define R_USB_USBREQ_BMREQUESTTYPE_Pos            (0UL)        /* BMREQUESTTYPE (Bit 0) */
#define R_USB_USBREQ_BMREQUESTTYPE_Msk            (0xffUL)     /* BMREQUESTTYPE (Bitfield-Mask: 0xff) */

/* USBVAL */
#define R_USB_USBVAL_WVALUE_Pos                   (0UL)        /* WVALUE (Bit 0) */
#define R_USB_USBVAL_WVALUE_Msk                   (0xffffUL)   /* WVALUE (Bitfield-Mask: 0xffff) */

/* USBINDX */
#define R_USB_USBINDX_WINDEX_Pos                  (0UL)        /* WINDEX (Bit 0) */
#define R_USB_USBINDX_WINDEX_Msk                  (0xffffUL)   /* WINDEX (Bitfield-Mask: 0xffff) */

/* USBLENG */
#define R_USB_USBLENG_WLENGTH_Pos                 (0UL)        /* WLENGTH (Bit 0) */
#define R_USB_USBLENG_WLENGTH_Msk                 (0xffffUL)   /* WLENGTH (Bitfield-Mask: 0xffff) */

/* DCPCFG */
#define R_USB_DCPCFG_CNTMD_Pos                    (8UL)        /* CNTMD (Bit 8) */
#define R_USB_DCPCFG_CNTMD_Msk                    (0x100UL)    /* CNTMD (Bitfield-Mask: 0x01) */
#define R_USB_DCPCFG_SHTNAK_Pos                   (7UL)        /* SHTNAK (Bit 7) */
#define R_USB_DCPCFG_SHTNAK_Msk                   (0x80UL)     /* SHTNAK (Bitfield-Mask: 0x01) */
#define R_USB_DCPCFG_DIR_Pos                      (4UL)        /* DIR (Bit 4) */
#define R_USB_DCPCFG_DIR_Msk                      (0x10UL)     /* DIR (Bitfield-Mask: 0x01) */

/* DCPMAXP */
#define R_USB_DCPMAXP_DEVSEL_Pos                  (12UL)       /* DEVSEL (Bit 12) */
#define R_USB_DCPMAXP_DEVSEL_Msk                  (0xf000UL)   /* DEVSEL (Bitfield-Mask: 0x0f) */
#define R_USB_DCPMAXP_MXPS_Pos                    (0UL)        /* MXPS (Bit 0) */
#define R_USB_DCPMAXP_MXPS_Msk                    (0x7fUL)     /* MXPS (Bitfield-Mask: 0x7f) */

/* DCPCTR */
#define R_USB_DCPCTR_BSTS_Pos                     (15UL)       /* BSTS (Bit 15) */
#define R_USB_DCPCTR_BSTS_Msk                     (0x8000UL)   /* BSTS (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_SUREQ_Pos                    (14UL)       /* SUREQ (Bit 14) */
#define R_USB_DCPCTR_SUREQ_Msk                    (0x4000UL)   /* SUREQ (Bitfield-Mask: 0x01) */
#define R_USB_HS0_DCPCTR_CSCLR_Pos                (13UL)       /*!< CSCLR (Bit 13)                                        */
#define R_USB_DCPCTR_CSCLR_Msk                    (0x2000UL)   /*!< CSCLR (Bitfield-Mask: 0x01)                           */
#define R_USB_DCPCTR_CSSTS_Pos                    (12UL)       /*!< CSSTS (Bit 12)                                        */
#define R_USB_DCPCTR_CSSTS_Msk                    (0x1000UL)   /*!< CSSTS (Bitfield-Mask: 0x01)                           */
#define R_USB_DCPCTR_SUREQCLR_Pos                 (11UL)       /* SUREQCLR (Bit 11) */
#define R_USB_DCPCTR_SUREQCLR_Msk                 (0x800UL)    /* SUREQCLR (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_SQCLR_Pos                    (8UL)        /* SQCLR (Bit 8) */
#define R_USB_DCPCTR_SQCLR_Msk                    (0x100UL)    /* SQCLR (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_SQSET_Pos                    (7UL)        /* SQSET (Bit 7) */
#define R_USB_DCPCTR_SQSET_Msk                    (0x80UL)     /* SQSET (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_SQMON_Pos                    (6UL)        /* SQMON (Bit 6) */
#define R_USB_DCPCTR_SQMON_Msk                    (0x40UL)     /* SQMON (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_PBUSY_Pos                    (5UL)        /* PBUSY (Bit 5) */
#define R_USB_DCPCTR_PBUSY_Msk                    (0x20UL)     /* PBUSY (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_CCPL_Pos                     (2UL)        /* CCPL (Bit 2) */
#define R_USB_DCPCTR_CCPL_Msk                     (0x4UL)      /* CCPL (Bitfield-Mask: 0x01) */
#define R_USB_DCPCTR_PID_Pos                      (0UL)        /* PID (Bit 0) */
#define R_USB_DCPCTR_PID_Msk                      (0x3UL)      /* PID (Bitfield-Mask: 0x03) */

/* PIPESEL */
#define R_USB_PIPESEL_PIPESEL_Pos                 (0UL)        /* PIPESEL (Bit 0) */
#define R_USB_PIPESEL_PIPESEL_Msk                 (0xfUL)      /* PIPESEL (Bitfield-Mask: 0x0f) */

/* PIPECFG */
#define R_USB_PIPECFG_TYPE_Pos                    (14UL)       /* TYPE (Bit 14) */
#define R_USB_PIPECFG_TYPE_Msk                    (0xc000UL)   /* TYPE (Bitfield-Mask: 0x03) */
#define R_USB_PIPECFG_BFRE_Pos                    (10UL)       /* BFRE (Bit 10) */
#define R_USB_PIPECFG_BFRE_Msk                    (0x400UL)    /* BFRE (Bitfield-Mask: 0x01) */
#define R_USB_PIPECFG_DBLB_Pos                    (9UL)        /* DBLB (Bit 9) */
#define R_USB_PIPECFG_DBLB_Msk                    (0x200UL)    /* DBLB (Bitfield-Mask: 0x01) */
#define R_USB_PIPECFG_CNTMD_Pos                   (8UL)        /*!< CNTMD (Bit 8)                                         */
#define R_USB_PIPECFG_CNTMD_Msk                   (0x100UL)    /*!< CNTMD (Bitfield-Mask: 0x01)                           */
#define R_USB_PIPECFG_SHTNAK_Pos                  (7UL)        /* SHTNAK (Bit 7) */
#define R_USB_PIPECFG_SHTNAK_Msk                  (0x80UL)     /* SHTNAK (Bitfield-Mask: 0x01) */
#define R_USB_PIPECFG_DIR_Pos                     (4UL)        /* DIR (Bit 4) */
#define R_USB_PIPECFG_DIR_Msk                     (0x10UL)     /* DIR (Bitfield-Mask: 0x01) */
#define R_USB_PIPECFG_EPNUM_Pos                   (0UL)        /* EPNUM (Bit 0) */
#define R_USB_PIPECFG_EPNUM_Msk                   (0xfUL)      /* EPNUM (Bitfield-Mask: 0x0f) */

/* PIPEBUF */
#define R_USB_PIPEBUF_BUFSIZE_Pos                 (10UL)       /*!< BUFSIZE (Bit 10)                                      */
#define R_USB_PIPEBUF_BUFSIZE_Msk                 (0x7c00UL)   /*!< BUFSIZE (Bitfield-Mask: 0x1f)                         */
#define R_USB_PIPEBUF_BUFNMB_Pos                  (0UL)        /*!< BUFNMB (Bit 0)                                        */
#define R_USB_PIPEBUF_BUFNMB_Msk                  (0xffUL)     /*!< BUFNMB (Bitfield-Mask: 0xff)                          */

/* PIPEMAXP */
#define R_USB_PIPEMAXP_DEVSEL_Pos                 (12UL)       /* DEVSEL (Bit 12) */
#define R_USB_PIPEMAXP_DEVSEL_Msk                 (0xf000UL)   /* DEVSEL (Bitfield-Mask: 0x0f) */
#define R_USB_PIPEMAXP_MXPS_Pos                   (0UL)        /* MXPS (Bit 0) */
#define R_USB_PIPEMAXP_MXPS_Msk                   (0x1ffUL)    /* MXPS (Bitfield-Mask: 0x1ff) */

/* PIPEPERI */
#define R_USB_PIPEPERI_IFIS_Pos                   (12UL)       /* IFIS (Bit 12) */
#define R_USB_PIPEPERI_IFIS_Msk                   (0x1000UL)   /* IFIS (Bitfield-Mask: 0x01) */
#define R_USB_PIPEPERI_IITV_Pos                   (0UL)        /* IITV (Bit 0) */
#define R_USB_PIPEPERI_IITV_Msk                   (0x7UL)      /* IITV (Bitfield-Mask: 0x07) */

/* PIPE_CTR */
#define R_USB_PIPE_CTR_BSTS_Pos                   (15UL)       /* BSTS (Bit 15) */
#define R_USB_PIPE_CTR_BSTS_Msk                   (0x8000UL)   /* BSTS (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_INBUFM_Pos                 (14UL)       /* INBUFM (Bit 14) */
#define R_USB_PIPE_CTR_INBUFM_Msk                 (0x4000UL)   /* INBUFM (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_CSCLR_Pos                  (13UL)       /* CSCLR (Bit 13) */
#define R_USB_PIPE_CTR_CSCLR_Msk                  (0x2000UL)   /* CSCLR (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_CSSTS_Pos                  (12UL)       /* CSSTS (Bit 12) */
#define R_USB_PIPE_CTR_CSSTS_Msk                  (0x1000UL)   /* CSSTS (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_ATREPM_Pos                 (10UL)       /* ATREPM (Bit 10) */
#define R_USB_PIPE_CTR_ATREPM_Msk                 (0x400UL)    /* ATREPM (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_ACLRM_Pos                  (9UL)        /* ACLRM (Bit 9) */
#define R_USB_PIPE_CTR_ACLRM_Msk                  (0x200UL)    /* ACLRM (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_SQCLR_Pos                  (8UL)        /* SQCLR (Bit 8) */
#define R_USB_PIPE_CTR_SQCLR_Msk                  (0x100UL)    /* SQCLR (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_SQSET_Pos                  (7UL)        /* SQSET (Bit 7) */
#define R_USB_PIPE_CTR_SQSET_Msk                  (0x80UL)     /* SQSET (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_SQMON_Pos                  (6UL)        /* SQMON (Bit 6) */
#define R_USB_PIPE_CTR_SQMON_Msk                  (0x40UL)     /* SQMON (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_PBUSY_Pos                  (5UL)        /* PBUSY (Bit 5) */
#define R_USB_PIPE_CTR_PBUSY_Msk                  (0x20UL)     /* PBUSY (Bitfield-Mask: 0x01) */
#define R_USB_PIPE_CTR_PID_Pos                    (0UL)        /* PID (Bit 0) */
#define R_USB_PIPE_CTR_PID_Msk                    (0x3UL)      /* PID (Bitfield-Mask: 0x03) */

/* DEVADD */
#define R_USB_DEVADD_UPPHUB_Pos                   (11UL)       /* UPPHUB (Bit 11) */
#define R_USB_DEVADD_UPPHUB_Msk                   (0x7800UL)   /* UPPHUB (Bitfield-Mask: 0x0f) */
#define R_USB_DEVADD_HUBPORT_Pos                  (8UL)        /* HUBPORT (Bit 8) */
#define R_USB_DEVADD_HUBPORT_Msk                  (0x700UL)    /* HUBPORT (Bitfield-Mask: 0x07) */
#define R_USB_DEVADD_USBSPD_Pos                   (6UL)        /* USBSPD (Bit 6) */
#define R_USB_DEVADD_USBSPD_Msk                   (0xc0UL)     /* USBSPD (Bitfield-Mask: 0x03) */

/* USBBCCTRL0 */
#define R_USB_USBBCCTRL0_PDDETSTS0_Pos            (9UL)        /* PDDETSTS0 (Bit 9) */
#define R_USB_USBBCCTRL0_PDDETSTS0_Msk            (0x200UL)    /* PDDETSTS0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_CHGDETSTS0_Pos           (8UL)        /* CHGDETSTS0 (Bit 8) */
#define R_USB_USBBCCTRL0_CHGDETSTS0_Msk           (0x100UL)    /* CHGDETSTS0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_BATCHGE0_Pos             (7UL)        /* BATCHGE0 (Bit 7) */
#define R_USB_USBBCCTRL0_BATCHGE0_Msk             (0x80UL)     /* BATCHGE0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_VDMSRCE0_Pos             (5UL)        /* VDMSRCE0 (Bit 5) */
#define R_USB_USBBCCTRL0_VDMSRCE0_Msk             (0x20UL)     /* VDMSRCE0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_IDPSINKE0_Pos            (4UL)        /* IDPSINKE0 (Bit 4) */
#define R_USB_USBBCCTRL0_IDPSINKE0_Msk            (0x10UL)     /* IDPSINKE0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_VDPSRCE0_Pos             (3UL)        /* VDPSRCE0 (Bit 3) */
#define R_USB_USBBCCTRL0_VDPSRCE0_Msk             (0x8UL)      /* VDPSRCE0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_IDMSINKE0_Pos            (2UL)        /* IDMSINKE0 (Bit 2) */
#define R_USB_USBBCCTRL0_IDMSINKE0_Msk            (0x4UL)      /* IDMSINKE0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_IDPSRCE0_Pos             (1UL)        /* IDPSRCE0 (Bit 1) */
#define R_USB_USBBCCTRL0_IDPSRCE0_Msk             (0x2UL)      /* IDPSRCE0 (Bitfield-Mask: 0x01) */
#define R_USB_USBBCCTRL0_RPDME0_Pos               (0UL)        /* RPDME0 (Bit 0) */
#define R_USB_USBBCCTRL0_RPDME0_Msk               (0x1UL)      /* RPDME0 (Bitfield-Mask: 0x01) */

/* UCKSEL */
#define R_USB_UCKSEL_UCKSELC_Pos                  (0UL)        /* UCKSELC (Bit 0) */
#define R_USB_UCKSEL_UCKSELC_Msk                  (0x1UL)      /* UCKSELC (Bitfield-Mask: 0x01) */

/* USBMC */
#define R_USB_USBMC_VDCEN_Pos                     (7UL)        /* VDCEN (Bit 7) */
#define R_USB_USBMC_VDCEN_Msk                     (0x80UL)     /* VDCEN (Bitfield-Mask: 0x01) */
#define R_USB_USBMC_VDDUSBE_Pos                   (0UL)        /* VDDUSBE (Bit 0) */
#define R_USB_USBMC_VDDUSBE_Msk                   (0x1UL)      /* VDDUSBE (Bitfield-Mask: 0x01) */

/* PHYSLEW */
#define R_USB_PHYSLEW_SLEWF01_Pos                 (3UL)        /* SLEWF01 (Bit 3) */
#define R_USB_PHYSLEW_SLEWF01_Msk                 (0x8UL)      /* SLEWF01 (Bitfield-Mask: 0x01) */
#define R_USB_PHYSLEW_SLEWF00_Pos                 (2UL)        /* SLEWF00 (Bit 2) */
#define R_USB_PHYSLEW_SLEWF00_Msk                 (0x4UL)      /* SLEWF00 (Bitfield-Mask: 0x01) */
#define R_USB_PHYSLEW_SLEWR01_Pos                 (1UL)        /* SLEWR01 (Bit 1) */
#define R_USB_PHYSLEW_SLEWR01_Msk                 (0x2UL)      /* SLEWR01 (Bitfield-Mask: 0x01) */
#define R_USB_PHYSLEW_SLEWR00_Pos                 (0UL)        /* SLEWR00 (Bit 0) */
#define R_USB_PHYSLEW_SLEWR00_Msk                 (0x1UL)      /* SLEWR00 (Bitfield-Mask: 0x01) */

/* LPCTRL */
#define R_USB_LPCTRL_HWUPM_Pos                    (7UL)        /* HWUPM (Bit 7) */
#define R_USB_LPCTRL_HWUPM_Msk                    (0x80UL)     /* HWUPM (Bitfield-Mask: 0x01) */

/* LPSTS */
#define R_USB_LPSTS_SUSPENDM_Pos                  (14UL)       /* SUSPENDM (Bit 14) */
#define R_USB_LPSTS_SUSPENDM_Msk                  (0x4000UL)   /* SUSPENDM (Bitfield-Mask: 0x01) */

/* BCCTRL */
#define R_USB_BCCTRL_PDDETSTS_Pos                 (9UL)        /* PDDETSTS (Bit 9) */
#define R_USB_BCCTRL_PDDETSTS_Msk                 (0x200UL)    /* PDDETSTS (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_CHGDETSTS_Pos                (8UL)        /* CHGDETSTS (Bit 8) */
#define R_USB_BCCTRL_CHGDETSTS_Msk                (0x100UL)    /* CHGDETSTS (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_DCPMODE_Pos                  (5UL)        /* DCPMODE (Bit 5) */
#define R_USB_BCCTRL_DCPMODE_Msk                  (0x20UL)     /* DCPMODE (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_VDMSRCE_Pos                  (4UL)        /* VDMSRCE (Bit 4) */
#define R_USB_BCCTRL_VDMSRCE_Msk                  (0x10UL)     /* VDMSRCE (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_IDPSINKE_Pos                 (3UL)        /* IDPSINKE (Bit 3) */
#define R_USB_BCCTRL_IDPSINKE_Msk                 (0x8UL)      /* IDPSINKE (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_VDPSRCE_Pos                  (2UL)        /* VDPSRCE (Bit 2) */
#define R_USB_BCCTRL_VDPSRCE_Msk                  (0x4UL)      /* VDPSRCE (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_IDMSINKE_Pos                 (1UL)        /* IDMSINKE (Bit 1) */
#define R_USB_BCCTRL_IDMSINKE_Msk                 (0x2UL)      /* IDMSINKE (Bitfield-Mask: 0x01) */
#define R_USB_BCCTRL_IDPSRCE_Pos                  (0UL)        /* IDPSRCE (Bit 0) */
#define R_USB_BCCTRL_IDPSRCE_Msk                  (0x1UL)      /* IDPSRCE (Bitfield-Mask: 0x01) */

/* PL1CTRL1 */
#define R_USB_PL1CTRL1_L1EXTMD_Pos                (14UL)       /* L1EXTMD (Bit 14) */
#define R_USB_PL1CTRL1_L1EXTMD_Msk                (0x4000UL)   /* L1EXTMD (Bitfield-Mask: 0x01) */
#define R_USB_PL1CTRL1_HIRDTHR_Pos                (8UL)        /* HIRDTHR (Bit 8) */
#define R_USB_PL1CTRL1_HIRDTHR_Msk                (0xf00UL)    /* HIRDTHR (Bitfield-Mask: 0x0f) */
#define R_USB_PL1CTRL1_DVSQ_Pos                   (4UL)        /* DVSQ (Bit 4) */
#define R_USB_PL1CTRL1_DVSQ_Msk                   (0xf0UL)     /* DVSQ (Bitfield-Mask: 0x0f) */
#define R_USB_PL1CTRL1_L1NEGOMD_Pos               (3UL)        /* L1NEGOMD (Bit 3) */
#define R_USB_PL1CTRL1_L1NEGOMD_Msk               (0x8UL)      /* L1NEGOMD (Bitfield-Mask: 0x01) */
#define R_USB_PL1CTRL1_L1RESPMD_Pos               (1UL)        /* L1RESPMD (Bit 1) */
#define R_USB_PL1CTRL1_L1RESPMD_Msk               (0x6UL)      /* L1RESPMD (Bitfield-Mask: 0x03) */
#define R_USB_PL1CTRL1_L1RESPEN_Pos               (0UL)        /* L1RESPEN (Bit 0) */
#define R_USB_PL1CTRL1_L1RESPEN_Msk               (0x1UL)      /* L1RESPEN (Bitfield-Mask: 0x01) */

/* PL1CTRL2 */
#define R_USB_PL1CTRL2_RWEMON_Pos                 (12UL)       /* RWEMON (Bit 12) */
#define R_USB_PL1CTRL2_RWEMON_Msk                 (0x1000UL)   /* RWEMON (Bitfield-Mask: 0x01) */
#define R_USB_PL1CTRL2_HIRDMON_Pos                (8UL)        /* HIRDMON (Bit 8) */
#define R_USB_PL1CTRL2_HIRDMON_Msk                (0xf00UL)    /* HIRDMON (Bitfield-Mask: 0x0f) */

/* HL1CTRL1 */
#define R_USB_HL1CTRL1_L1STATUS_Pos               (1UL)        /* L1STATUS (Bit 1) */
#define R_USB_HL1CTRL1_L1STATUS_Msk               (0x6UL)      /* L1STATUS (Bitfield-Mask: 0x03) */
#define R_USB_HL1CTRL1_L1REQ_Pos                  (0UL)        /* L1REQ (Bit 0) */
#define R_USB_HL1CTRL1_L1REQ_Msk                  (0x1UL)      /* L1REQ (Bitfield-Mask: 0x01) */

/* HL1CTRL2 */
#define R_USB_HL1CTRL2_BESL_Pos                   (15UL)       /* BESL (Bit 15) */
#define R_USB_HL1CTRL2_BESL_Msk                   (0x8000UL)   /* BESL (Bitfield-Mask: 0x01) */
#define R_USB_HL1CTRL2_L1RWE_Pos                  (12UL)       /* L1RWE (Bit 12) */
#define R_USB_HL1CTRL2_L1RWE_Msk                  (0x1000UL)   /* L1RWE (Bitfield-Mask: 0x01) */
#define R_USB_HL1CTRL2_HIRD_Pos                   (8UL)        /* HIRD (Bit 8) */
#define R_USB_HL1CTRL2_HIRD_Msk                   (0xf00UL)    /* HIRD (Bitfield-Mask: 0x0f) */
#define R_USB_HL1CTRL2_L1ADDR_Pos                 (0UL)        /* L1ADDR (Bit 0) */
#define R_USB_HL1CTRL2_L1ADDR_Msk                 (0xfUL)      /* L1ADDR (Bitfield-Mask: 0x0f) */

/* PHYTRIM1 */
#define R_USB_PHYTRIM1_IMPOFFSET_Pos              (12UL)       /*!< IMPOFFSET (Bit 12)                                    */
#define R_USB_PHYTRIM1_IMPOFFSET_Msk              (0x7000UL)   /*!< IMPOFFSET (Bitfield-Mask: 0x07)                       */
#define R_USB_PHYTRIM1_HSIUP_Pos                  (8UL)        /*!< HSIUP (Bit 8)                                         */
#define R_USB_PHYTRIM1_HSIUP_Msk                  (0xf00UL)    /*!< HSIUP (Bitfield-Mask: 0x0f)                           */
#define R_USB_PHYTRIM1_PCOMPENB_Pos               (7UL)        /*!< PCOMPENB (Bit 7)                                      */
#define R_USB_PHYTRIM1_PCOMPENB_Msk               (0x80UL)     /*!< PCOMPENB (Bitfield-Mask: 0x01)                        */
#define R_USB_PHYTRIM1_DFALL_Pos                  (2UL)        /*!< DFALL (Bit 2)                                         */
#define R_USB_PHYTRIM1_DFALL_Msk                  (0xcUL)      /*!< DFALL (Bitfield-Mask: 0x03)                           */
#define R_USB_PHYTRIM1_DRISE_Pos                  (0UL)        /*!< DRISE (Bit 0)                                         */
#define R_USB_PHYTRIM1_DRISE_Msk                  (0x3UL)      /*!< DRISE (Bitfield-Mask: 0x03)                           */

/* PHYTRIM2 */
#define R_USB_PHYTRIM2_DIS_Pos                    (12UL)       /*!< DIS (Bit 12)                                          */
#define R_USB_PHYTRIM2_DIS_Msk                    (0x7000UL)   /*!< DIS (Bitfield-Mask: 0x07)                             */
#define R_USB_PHYTRIM2_PDR_Pos                    (8UL)        /*!< PDR (Bit 8)                                           */
#define R_USB_PHYTRIM2_PDR_Msk                    (0x300UL)    /*!< PDR (Bitfield-Mask: 0x03)                             */
#define R_USB_PHYTRIM2_HSRXENMO_Pos               (7UL)        /*!< HSRXENMO (Bit 7)                                      */
#define R_USB_PHYTRIM2_HSRXENMO_Msk               (0x80UL)     /*!< HSRXENMO (Bitfield-Mask: 0x01)                        */
#define R_USB_PHYTRIM2_SQU_Pos                    (0UL)        /*!< SQU (Bit 0)                                           */
#define R_USB_PHYTRIM2_SQU_Msk                    (0xfUL)      /*!< SQU (Bitfield-Mask: 0x0f)                             */

/* DPUSR0R */
#define R_USB_DPUSR0R_DVBSTSHM_Pos                (23UL)       /* DVBSTSHM (Bit 23) */
#define R_USB_DPUSR0R_DVBSTSHM_Msk                (0x800000UL) /* DVBSTSHM (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_DOVCBHM_Pos                 (21UL)       /* DOVCBHM (Bit 21) */
#define R_USB_DPUSR0R_DOVCBHM_Msk                 (0x200000UL) /* DOVCBHM (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_DOVCAHM_Pos                 (20UL)       /* DOVCAHM (Bit 20) */
#define R_USB_DPUSR0R_DOVCAHM_Msk                 (0x100000UL) /* DOVCAHM (Bitfield-Mask: 0x01) */

/* DPUSR1R */
#define R_USB_DPUSR1R_DVBSTSH_Pos                 (23UL)       /* DVBSTSH (Bit 23) */
#define R_USB_DPUSR1R_DVBSTSH_Msk                 (0x800000UL) /* DVBSTSH (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_DOVCBH_Pos                  (21UL)       /* DOVCBH (Bit 21) */
#define R_USB_DPUSR1R_DOVCBH_Msk                  (0x200000UL) /* DOVCBH (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_DOVCAH_Pos                  (20UL)       /* DOVCAH (Bit 20) */
#define R_USB_DPUSR1R_DOVCAH_Msk                  (0x100000UL) /* DOVCAH (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_DVBSTSHE_Pos                (7UL)        /* DVBSTSHE (Bit 7) */
#define R_USB_DPUSR1R_DVBSTSHE_Msk                (0x80UL)     /* DVBSTSHE (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_DOVCBHE_Pos                 (5UL)        /* DOVCBHE (Bit 5) */
#define R_USB_DPUSR1R_DOVCBHE_Msk                 (0x20UL)     /* DOVCBHE (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_DOVCAHE_Pos                 (4UL)        /* DOVCAHE (Bit 4) */
#define R_USB_DPUSR1R_DOVCAHE_Msk                 (0x10UL)     /* DOVCAHE (Bitfield-Mask: 0x01) */

/* DPUSR2R */
#define R_USB_DPUSR2R_DMINTE_Pos                  (9UL)        /* DMINTE (Bit 9) */
#define R_USB_DPUSR2R_DMINTE_Msk                  (0x200UL)    /* DMINTE (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR2R_DPINTE_Pos                  (8UL)        /* DPINTE (Bit 8) */
#define R_USB_DPUSR2R_DPINTE_Msk                  (0x100UL)    /* DPINTE (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR2R_DMVAL_Pos                   (5UL)        /* DMVAL (Bit 5) */
#define R_USB_DPUSR2R_DMVAL_Msk                   (0x20UL)     /* DMVAL (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR2R_DPVAL_Pos                   (4UL)        /* DPVAL (Bit 4) */
#define R_USB_DPUSR2R_DPVAL_Msk                   (0x10UL)     /* DPVAL (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR2R_DMINT_Pos                   (1UL)        /* DMINT (Bit 1) */
#define R_USB_DPUSR2R_DMINT_Msk                   (0x2UL)      /* DMINT (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR2R_DPINT_Pos                   (0UL)        /* DPINT (Bit 0) */
#define R_USB_DPUSR2R_DPINT_Msk                   (0x1UL)      /* DPINT (Bitfield-Mask: 0x01) */

/* DPUSRCR */
#define R_USB_DPUSRCR_FIXPHYPD_Pos                (1UL)        /* FIXPHYPD (Bit 1) */
#define R_USB_DPUSRCR_FIXPHYPD_Msk                (0x2UL)      /* FIXPHYPD (Bitfield-Mask: 0x01) */
#define R_USB_DPUSRCR_FIXPHY_Pos                  (0UL)        /* FIXPHY (Bit 0) */
#define R_USB_DPUSRCR_FIXPHY_Msk                  (0x1UL)      /* FIXPHY (Bitfield-Mask: 0x01) */

/* DPUSR0R_FS */
#define R_USB_DPUSR0R_FS_DVBSTS0_Pos              (23UL)       /* DVBSTS0 (Bit 23) */
#define R_USB_DPUSR0R_FS_DVBSTS0_Msk              (0x800000UL) /* DVBSTS0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_DOVCB0_Pos               (21UL)       /* DOVCB0 (Bit 21) */
#define R_USB_DPUSR0R_FS_DOVCB0_Msk               (0x200000UL) /* DOVCB0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_DOVCA0_Pos               (20UL)       /* DOVCA0 (Bit 20) */
#define R_USB_DPUSR0R_FS_DOVCA0_Msk               (0x100000UL) /* DOVCA0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_DM0_Pos                  (17UL)       /* DM0 (Bit 17) */
#define R_USB_DPUSR0R_FS_DM0_Msk                  (0x20000UL)  /* DM0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_DP0_Pos                  (16UL)       /* DP0 (Bit 16) */
#define R_USB_DPUSR0R_FS_DP0_Msk                  (0x10000UL)  /* DP0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_FIXPHY0_Pos              (4UL)        /* FIXPHY0 (Bit 4) */
#define R_USB_DPUSR0R_FS_FIXPHY0_Msk              (0x10UL)     /* FIXPHY0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_DRPD0_Pos                (3UL)        /* DRPD0 (Bit 3) */
#define R_USB_DPUSR0R_FS_DRPD0_Msk                (0x8UL)      /* DRPD0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_RPUE0_Pos                (1UL)        /* RPUE0 (Bit 1) */
#define R_USB_DPUSR0R_FS_RPUE0_Msk                (0x2UL)      /* RPUE0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR0R_FS_SRPC0_Pos                (0UL)        /* SRPC0 (Bit 0) */
#define R_USB_DPUSR0R_FS_SRPC0_Msk                (0x1UL)      /* SRPC0 (Bitfield-Mask: 0x01) */

/* DPUSR1R_FS */
#define R_USB_DPUSR1R_FS_DVBINT0_Pos              (23UL)       /* DVBINT0 (Bit 23) */
#define R_USB_DPUSR1R_FS_DVBINT0_Msk              (0x800000UL) /* DVBINT0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DOVRCRB0_Pos             (21UL)       /* DOVRCRB0 (Bit 21) */
#define R_USB_DPUSR1R_FS_DOVRCRB0_Msk             (0x200000UL) /* DOVRCRB0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DOVRCRA0_Pos             (20UL)       /* DOVRCRA0 (Bit 20) */
#define R_USB_DPUSR1R_FS_DOVRCRA0_Msk             (0x100000UL) /* DOVRCRA0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DMINT0_Pos               (17UL)       /* DMINT0 (Bit 17) */
#define R_USB_DPUSR1R_FS_DMINT0_Msk               (0x20000UL)  /* DMINT0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DPINT0_Pos               (16UL)       /* DPINT0 (Bit 16) */
#define R_USB_DPUSR1R_FS_DPINT0_Msk               (0x10000UL)  /* DPINT0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DVBSE0_Pos               (7UL)        /* DVBSE0 (Bit 7) */
#define R_USB_DPUSR1R_FS_DVBSE0_Msk               (0x80UL)     /* DVBSE0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DOVRCRBE0_Pos            (5UL)        /* DOVRCRBE0 (Bit 5) */
#define R_USB_DPUSR1R_FS_DOVRCRBE0_Msk            (0x20UL)     /* DOVRCRBE0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DOVRCRAE0_Pos            (4UL)        /* DOVRCRAE0 (Bit 4) */
#define R_USB_DPUSR1R_FS_DOVRCRAE0_Msk            (0x10UL)     /* DOVRCRAE0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DMINTE0_Pos              (1UL)        /* DMINTE0 (Bit 1) */
#define R_USB_DPUSR1R_FS_DMINTE0_Msk              (0x2UL)      /* DMINTE0 (Bitfield-Mask: 0x01) */
#define R_USB_DPUSR1R_FS_DPINTE0_Pos              (0UL)        /* DPINTE0 (Bit 0) */
#define R_USB_DPUSR1R_FS_DPINTE0_Msk              (0x1UL)      /* DPINTE0 (Bitfield-Mask: 0x01) */

/*--------------------------------------------------------------------*/
/* Register Bit Utils                                           */
/*--------------------------------------------------------------------*/
#define R_USB_PIPE_CTR_PID_NAK                    (0U << R_USB_PIPE_CTR_PID_Pos)    /* NAK response */
#define R_USB_PIPE_CTR_PID_BUF                    (1U << R_USB_PIPE_CTR_PID_Pos)    /* BUF response (depends buffer state) */
#define R_USB_PIPE_CTR_PID_STALL                  (2U << R_USB_PIPE_CTR_PID_Pos)    /* STALL response */
#define R_USB_PIPE_CTR_PID_STALL2                 (3U << R_USB_PIPE_CTR_PID_Pos)    /* Also STALL response */

#define R_USB_DVSTCTR0_RHST_LS                    (1U << R_USB_DVSTCTR0_RHST_Pos)   /*  Low-speed connection */
#define R_USB_DVSTCTR0_RHST_FS                    (2U << R_USB_DVSTCTR0_RHST_Pos)   /*  Full-speed connection */
#define R_USB_DVSTCTR0_RHST_HS                    (3U << R_USB_DVSTCTR0_RHST_Pos)   /*  Full-speed connection */

#define R_USB_DEVADD_USBSPD_LS                    (1U << R_USB_DEVADD_USBSPD_Pos)   /* Target Device Low-speed */
#define R_USB_DEVADD_USBSPD_FS                    (2U << R_USB_DEVADD_USBSPD_Pos)   /* Target Device Full-speed */

#define R_USB_CFIFOSEL_ISEL_WRITE                 (1U << R_USB_CFIFOSEL_ISEL_Pos)   /* FIFO write AKA TX*/

#define R_USB_FIFOSEL_BIGEND                      (1U << R_USB_CFIFOSEL_BIGEND_Pos) /* FIFO Big Endian */
#define R_USB_FIFOSEL_MBW_8BIT                    (0U << R_USB_CFIFOSEL_MBW_Pos)    /* 8-bit width */
#define R_USB_FIFOSEL_MBW_16BIT                   (1U << R_USB_CFIFOSEL_MBW_Pos)    /* 16-bit width */
#define R_USB_FIFOSEL_MBW_32BIT                   (2U << R_USB_CFIFOSEL_MBW_Pos)    /* 32-bit width */

#define R_USB_INTSTS0_CTSQ_CTRL_IDLE              (0U << R_USB_INTSTS0_CTSQ_Pos)    /* Control setup/idle stage */
#define R_USB_INTSTS0_CTSQ_CTRL_RDATA             (1U << R_USB_INTSTS0_CTSQ_Pos)    /* Control read data stage */
#define R_USB_INTSTS0_CTSQ_CTRL_RSTATUS           (2U << R_USB_INTSTS0_CTSQ_Pos)    /* Control read status stage */
#define R_USB_INTSTS0_CTSQ_CTRL_WDATA             (3U << R_USB_INTSTS0_CTSQ_Pos)    /* Control write data stage */
#define R_USB_INTSTS0_CTSQ_CTRL_WSTATUS           (4U << R_USB_INTSTS0_CTSQ_Pos)    /* Control write status stage */
#define R_USB_INTSTS0_CTSQ_CTRL_WSTATUS_NODATA    (5U << R_USB_INTSTS0_CTSQ_Pos)    /* Control write status no data stage */
#define R_USB_INTSTS0_CTSQ_CTRL_ERROR             (6U << R_USB_INTSTS0_CTSQ_Pos)    /* Control error stage */

#define R_USB_INTSTS0_DVSQ_STATE_DEF              (1U << R_USB_INTSTS0_DVSQ_Pos)    /* Default state */
#define R_USB_INTSTS0_DVSQ_STATE_ADDR             (2U << R_USB_INTSTS0_DVSQ_Pos)    /* Address state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP0            (4U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP1            (5U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP2            (6U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP3            (7U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */

#define R_USB_PIPECFG_TYPE_BULK                   (1U << R_USB_PIPECFG_TYPE_Pos)
#define R_USB_PIPECFG_TYPE_INT                    (2U << R_USB_PIPECFG_TYPE_Pos)
#define R_USB_PIPECFG_TYPE_ISO                    (3U << R_USB_PIPECFG_TYPE_Pos)

typedef struct
{
    union
    {
        volatile uint16_t E;           /* (@ 0x00000000) Pipe Transaction Counter Enable Register */

        struct __PACKED
        {
            uint16_t                : 8;
            volatile uint16_t TRCLR : 1; /* [8..8] Transaction Counter Clear */
            volatile uint16_t TRENB : 1; /* [9..9] Transaction Counter Enable */
            uint16_t                : 6;
        } E_b;
    };

    union
    {
        volatile uint16_t N;               /* (@ 0x00000002) Pipe Transaction Counter Register */

        struct __PACKED
        {
            volatile uint16_t TRNCNT : 16; /* [15..0] Transaction Counter */
        } N_b;
    };
} R_USB_PIPE_TR_t;                         /* Size = 4 (0x4) */

/*--------------------------------------------------------------------*/
/* Register Bit Utils                                           */
/*--------------------------------------------------------------------*/
#define R_USB_PIPE_CTR_PID_NAK            (0U << R_USB_PIPE_CTR_PID_Pos)    /* NAK response */
#define R_USB_PIPE_CTR_PID_BUF            (1U << R_USB_PIPE_CTR_PID_Pos)    /* BUF response (depends buffer state) */
#define R_USB_PIPE_CTR_PID_STALL          (2U << R_USB_PIPE_CTR_PID_Pos)    /* STALL response */
#define R_USB_PIPE_CTR_PID_STALL2         (3U << R_USB_PIPE_CTR_PID_Pos)    /* Also STALL response */

#define R_USB_DVSTCTR0_RHST_LS            (1U << R_USB_DVSTCTR0_RHST_Pos)   /*  Low-speed connection */
#define R_USB_DVSTCTR0_RHST_FS            (2U << R_USB_DVSTCTR0_RHST_Pos)   /*  Full-speed connection */
#define R_USB_DVSTCTR0_RHST_HS            (3U << R_USB_DVSTCTR0_RHST_Pos)   /*  Full-speed connection */

#define R_USB_DEVADD_USBSPD_LS            (1U << R_USB_DEVADD_USBSPD_Pos)   /* Target Device Low-speed */
#define R_USB_DEVADD_USBSPD_FS            (2U << R_USB_DEVADD_USBSPD_Pos)   /* Target Device Full-speed */

#define R_USB_CFIFOSEL_ISEL_WRITE         (1U << R_USB_CFIFOSEL_ISEL_Pos)   /* FIFO write AKA TX*/

#define R_USB_FIFOSEL_BIGEND              (1U << R_USB_CFIFOSEL_BIGEND_Pos) /* FIFO Big Endian */
#define R_USB_FIFOSEL_MBW_8BIT            (0U << R_USB_CFIFOSEL_MBW_Pos)    /* 8-bit width */
#define R_USB_FIFOSEL_MBW_16BIT           (1U << R_USB_CFIFOSEL_MBW_Pos)    /* 16-bit width */
#define R_USB_FIFOSEL_MBW_32BIT           (2U << R_USB_CFIFOSEL_MBW_Pos)    /* 32-bit width */

#define R_USB_INTSTS0_CTSQ_CTRL_RDATA     (1U << R_USB_INTSTS0_CTSQ_Pos)

#define R_USB_INTSTS0_DVSQ_STATE_DEF      (1U << R_USB_INTSTS0_DVSQ_Pos)    /* Default state */
#define R_USB_INTSTS0_DVSQ_STATE_ADDR     (2U << R_USB_INTSTS0_DVSQ_Pos)    /* Address state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP0    (4U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP1    (5U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP2    (6U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */
#define R_USB_INTSTS0_DVSQ_STATE_SUSP3    (7U << R_USB_INTSTS0_DVSQ_Pos)    /* Suspend state */

#define R_USB_PIPECFG_TYPE_BULK           (1U << R_USB_PIPECFG_TYPE_Pos)
#define R_USB_PIPECFG_TYPE_INT            (2U << R_USB_PIPECFG_TYPE_Pos)
#define R_USB_PIPECFG_TYPE_ISO            (3U << R_USB_PIPECFG_TYPE_Pos)

#endif /* R_USB_DEVICE_DEFINE_H */