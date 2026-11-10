/*
 * Copyright (c) 2023-2026, ArtInChip Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtconfig.h>
#include "usbd_core.h"
#include "usb_dc_aic_reg.h"

#ifdef RT_USING_PM
#include <rtthread.h>
#endif

#if defined(KERNEL_RTTHREAD) && defined(LPKG_CHERRYUSB_DEVICE_PLUG_CHECK)
#include <rtdevice.h>
#include "aic_hal.h"
#endif

// clang-format off
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  #pragma GCC diagnostic ignored "-Wattributes"
  __PACKED_STRUCT T_UINT32_WRITE { uint32_t v; };
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32_WRITE(addr, val)    (void)((((struct T_UINT32_WRITE *)(void *)(addr))->v) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wpacked"
  #pragma GCC diagnostic ignored "-Wattributes"
  __PACKED_STRUCT T_UINT32_READ { uint32_t v; };
  #pragma GCC diagnostic pop
  #define __UNALIGNED_UINT32_READ(addr)          (((const struct T_UINT32_READ *)(const void *)(addr))->v)
#endif
// clang-format on

#define FS_PORT 0
#define HS_PORT 1

#ifndef CONFIG_USB_AIC_DC_PORT
#error "please select CONFIG_USB_AIC_DC_PORT with FS_PORT or HS_PORT"
#endif

#ifndef USB_BASE
#define USB_BASE CONFIG_USB_AIC_DC_BASE
#endif

#ifdef LPKG_CHERRYUSB_DEVICE_HID_IO_TEMPLATE
#define USB_RAM_SIZE 1024 /* define with maximum value*/
#else
#define USB_RAM_SIZE 512  /* define with maximum value*/
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 5 /* define with minimum value*/
#endif

/* USB_P_TXFIFO_NUM:
 * There are only 2 Periodic_TXFIFO, and each Interrupt/Isochronous In ep
 * needs to request one P_TXFIFO. So, only two Interrupt/Isochronous In ep
 * can be requested for at most.
 */
#define USB_P_TXFIFO_NUM 2

#define AIC_UDC_REG      ((AIC_UDC_RegDef *)(USB_BASE))
#define AIC_EP_FIFO(i)  *(__IO uint32_t *)(USB_BASE + AIC_EP_FIFO_BASE + ((i)*AIC_EP_FIFO_SIZE))

#define AIC_EP_READ_TIMEOUT  1000000U
#define AIC_EP_FIFOCTL_TIMEOUT 200000U
/* Endpoint state */
struct aic_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t *xfer_buf;
#ifdef CONFIG_USB_AIC_DMA_ENABLE
    uint8_t *xfer_align_buf;
    uint32_t xfer_align_len;
#endif
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

extern uint32_t usbd_clk;

/* Driver state */
/*USB_NOCACHE_RAM_SECTION*/ struct aic_udc {
    USB_MEM_ALIGNX struct usb_setup_packet setup;
    USB_MEM_ALIGNX struct aic_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct aic_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
    struct usb_endpoint_descriptor in_ep_desc[USB_NUM_BIDIR_ENDPOINTS];
    uint32_t p_txfifo_map; /* map which periodic_txfifo is used */
    uint32_t np_txfifo_map; /* map no-periodic_txfifo is used */
    uint8_t test_mode;
} g_aic_udc;

#ifndef AIC_USB_DEVICE_DRV_V10
static uint32_t g_tx_fifo_sz_array[USB_NUM_BIDIR_ENDPOINTS] = {0};
#endif

#define AIC_EP0_SETUP           1
#define AIC_EP0_DATA_IN         2
#define AIC_EP0_STATUS_OUT      3
#define AIC_EP0_DATA_OUT        12
#define AIC_EP0_STATUS_IN       13

uint8_t ep0_ctrl_stage = 0; /* 1 = setup stage, 2 = data stage, 3 = status stage */
static int setup_handler_immediately = 0;

#ifdef RT_USING_PM
static int usbd_pm_init(void);
#endif

#ifdef CONFIG_USB_AIC_DMA_ENABLE
static uint8_t g_aic_udc_ibuf[USB_RAM_SIZE] __ALIGNED(CACHE_LINE_SIZE);
static uint8_t g_aic_udc_obuf[USB_RAM_SIZE] __ALIGNED(CACHE_LINE_SIZE);

void aic_udc_dcache_clean(uintptr_t addr, uint32_t len)
{
    aicos_dcache_clean_range((size_t *)addr, len);
}

void aic_udc_dcache_invalidate(uintptr_t addr, uint32_t len)
{
    aicos_dcache_invalid_range((size_t *)addr, len);
}

void aic_udc_dcache_clean_invalidate(uintptr_t addr, uint32_t len)
{
    aicos_dcache_clean_invalid_range((size_t *)addr, len);
}

static int aic_udc_ep_buf_alloc(struct aic_ep_state *ep, uint32_t len,
                                uint8_t *sbuf)
{
    ep->xfer_len = len;
    if (len % CACHE_LINE_SIZE)
        ep->xfer_align_len = ALIGN_UP(len, CACHE_LINE_SIZE);
    else
        ep->xfer_align_len = len;

    if (ep->xfer_align_len > USB_RAM_SIZE) {
        ep->xfer_align_buf = aicos_malloc_align(0, ep->xfer_align_len,
                                                CACHE_LINE_SIZE);
        USB_LOG_DBG("aicos_malloc_align %d bytes.", ep->xfer_align_len);
        if (!ep->xfer_align_buf) {
            USB_LOG_ERR("alloc error.\r\n");
            return -5;
        }
    } else {
        ep->xfer_align_buf = sbuf;
    }

    return 0;
}

static void aic_udc_ep_buf_free(struct aic_ep_state *ep, uint8_t *sbuf)
{
    if (!ep->xfer_align_buf)
        return;

    /* Whether the buf is allocated dynamically */
    if (ep->xfer_align_buf != sbuf) {
        USB_LOG_DBG("aicos_free_align %d bytes.", ep->xfer_align_len);
        aicos_free_align(0, ep->xfer_align_buf);
    }

    ep->xfer_align_buf = NULL;
    ep->xfer_align_len = 0;
}

static int aic_udc_ibuf_alloc(struct aic_ep_state *ep, uint32_t len)
{
    return aic_udc_ep_buf_alloc(ep, len, g_aic_udc_ibuf);
}

static int aic_udc_obuf_alloc(struct aic_ep_state *ep, uint32_t len)
{
    return aic_udc_ep_buf_alloc(ep, len, g_aic_udc_obuf);
}

static void aic_udc_ibuf_free(struct aic_ep_state *ep)
{
    aic_udc_ep_buf_free(ep, g_aic_udc_ibuf);
}

static void aic_udc_obuf_free(struct aic_ep_state *ep)
{
    aic_udc_ep_buf_free(ep, g_aic_udc_obuf);
}
#else
#define aic_udc_dcache_clean(addr, len)
#define aic_udc_dcache_invalidate(addr, len)
#define aic_udc_dcache_clean_invalidate(addr, len)
#endif

static void aic_set_dma_nextep(void)
{
    uint32_t i, j;
    uint32_t inepcfg = 0, next = 0;

#ifndef AIC_USB_DEVICE_DRV_V10
    return;
#endif

    /* dma to set the next-endpoint pointer. */
    for (i = 0; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        for (j = i + 1; j < USB_NUM_BIDIR_ENDPOINTS; j++) {
            if ((g_aic_udc.np_txfifo_map & (1 << j)) == 0U)
                continue;
            next = (j << DEPCTL_NEXT_EP_BIT);
            break;
        }

        inepcfg = readl(&AIC_UDC_REG->inepcfg[i]);
        inepcfg &= ~DEPCTL_NEXT_EP_MASK;
        inepcfg |= next;
        writel(inepcfg, &AIC_UDC_REG->inepcfg[i]);
        next = 0;
    }
}

static inline int aic_reset(void)
{
    uint32_t count = 0U;
    uint32_t tmpreg = 0;

    /* Wait for AHB master IDLE state. */
    do {
        if (++count > AIC_EP_FIFOCTL_TIMEOUT) {
            return -1;
        }
        tmpreg = readl(&AIC_UDC_REG->ahbbasic);
    } while ((tmpreg & AHBBASIC_AHBIDLE) == 0U);

    /* Core Soft Reset */
    count = 0U;
    tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    tmpreg |= USBDEVINIT_CSFTRST;
    writel(tmpreg, &AIC_UDC_REG->usbdevinit);

    do {
        if (++count > AIC_EP_FIFOCTL_TIMEOUT) {
            return -1;
        }
        tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    } while ((tmpreg & USBDEVINIT_CSFTRST) == USBDEVINIT_CSFTRST);

    return 0;
}

static inline int aic_core_init(void)
{
    int ret;
    uint32_t usb_gusbcfg =
        0 << 19     /* ULPI Clock SuspendM */
        | 0 << 18   /* ULPI Phy Auto Resume */
        | 0 << 15   /* PHY Low Power Clock sel */
#ifdef FPGA_BOARD_ARTINCHIP
        | 0x9 << 10 /* USB Turnaround time (0x5 for HS phy) */
#else
        | 0x5 << 10 /* USB Turnaround time (0x5 for HS phy) */
#endif
        | 0 << 7    /* ULPI DDR sel 0:single 8bit, 1:double 4bit */
        /*| 0 << 6   0: high speed utmi+, 1: full speed serial*/
#ifdef FPGA_BOARD_ARTINCHIP
        | 1 << 4    /* 0: utmi+, 1:ulpi*/
#else
        | 0 << 4    /* 0: utmi+, 1:ulpi*/
#endif
        | 0 << 3    /* UTMI+ PHY  0:8bit, 1:16bit (ULPI PHY set 8bit) */
        | 0x7 << 0; /* HS/FS timeout calibration**/

    /* Activate the USB Transceiver */

    /* Reset after a PHY select */
    ret = aic_reset();

    writel(usb_gusbcfg, &AIC_UDC_REG->usbphyif);

    aic_set_dma_nextep();

    return ret;
}

static inline int aic_flush_rxfifo(void)
{
    uint32_t count = 0;
    uint32_t tmpreg = 0;

    tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    tmpreg |= USBDEVINIT_RXFFLSH;
    writel(tmpreg, &AIC_UDC_REG->usbdevinit);

    do {
        if (++count > AIC_EP_FIFOCTL_TIMEOUT) {
            return -1;
        }
        tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    } while ((tmpreg & USBDEVINIT_RXFFLSH) == USBDEVINIT_RXFFLSH);

    return 0;
}

static inline int aic_flush_txfifo(uint32_t num)
{
    uint32_t count = 0U;
    uint32_t tmpreg = 0;

    tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    tmpreg &= ~ USBDEVINIT_TXFNUM(USBDEVINIT_TXFNUM_LIMIT);
    tmpreg |= USBDEVINIT_TXFNUM(num & USBDEVINIT_TXFNUM_LIMIT)
               | USBDEVINIT_TXFFLSH;
    writel(tmpreg, &AIC_UDC_REG->usbdevinit);

    do {
        if (++count > AIC_EP_FIFOCTL_TIMEOUT) {
            return -1;
        }
        tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    } while ((tmpreg & USBDEVINIT_TXFFLSH) == USBDEVINIT_TXFFLSH);

    return 0;
}

static void aic_set_turnaroundtime(uint32_t hclk, uint8_t speed)
{
    uint32_t UsbTrd;
    uint32_t tmpreg = 0;
#ifdef FPGA_BOARD_ARTINCHIP
    return;
#endif
    /* The USBTRD is configured according to the tables below, depending on AHB frequency
  used by application. In the low AHB frequency range it is used to stretch enough the USB response
  time to IN tokens, the USB turnaround time, so to compensate for the longer AHB read access
  latency to the Data FIFO */
    if (speed == USB_ENUM_SPEED_FULL) {
        if ((hclk >= 14200000U) && (hclk < 15000000U)) {
            /* hclk Clock Range between 14.2-15 MHz */
            UsbTrd = 0xFU;
        } else if ((hclk >= 15000000U) && (hclk < 16000000U)) {
            /* hclk Clock Range between 15-16 MHz */
            UsbTrd = 0xEU;
        } else if ((hclk >= 16000000U) && (hclk < 17200000U)) {
            /* hclk Clock Range between 16-17.2 MHz */
            UsbTrd = 0xDU;
        } else if ((hclk >= 17200000U) && (hclk < 18500000U)) {
            /* hclk Clock Range between 17.2-18.5 MHz */
            UsbTrd = 0xCU;
        } else if ((hclk >= 18500000U) && (hclk < 20000000U)) {
            /* hclk Clock Range between 18.5-20 MHz */
            UsbTrd = 0xBU;
        } else if ((hclk >= 20000000U) && (hclk < 21800000U)) {
            /* hclk Clock Range between 20-21.8 MHz */
            UsbTrd = 0xAU;
        } else if ((hclk >= 21800000U) && (hclk < 24000000U)) {
            /* hclk Clock Range between 21.8-24 MHz */
            UsbTrd = 0x9U;
        } else if ((hclk >= 24000000U) && (hclk < 27700000U)) {
            /* hclk Clock Range between 24-27.7 MHz */
            UsbTrd = 0x8U;
        } else if ((hclk >= 27700000U) && (hclk < 32000000U)) {
            /* hclk Clock Range between 27.7-32 MHz */
            UsbTrd = 0x7U;
        } else {/* if(hclk >= 32000000) */
            /* hclk Clock Range between 32-200 MHz */
            UsbTrd = 0x6U;
        }
    } else if (speed == USB_ENUM_SPEED_HIGH) {
        UsbTrd = USBPHYIF_HS_TRDT_VALUE;
    } else {
        UsbTrd = USBPHYIF_DEFAULT_TRDT_VALUE;
    }

    tmpreg = readl(&AIC_UDC_REG->usbphyif);
    tmpreg |= USBPHYIF_TOUTCAL_LIMIT;
    tmpreg &= ~USBPHYIF_USBTRDTIM_MASK;
    tmpreg |= (uint32_t)((UsbTrd << USBPHYIF_USBTRDTIM_SHIFT)
                        & USBPHYIF_USBTRDTIM_MASK);
    writel(tmpreg, &AIC_UDC_REG->usbphyif);
}

#if 0
static void aic_set_txfifo(uint8_t fifo, uint16_t size)
{
    uint8_t i;
    uint32_t Tx_Offset;

    /*  TXn min size = 16 words. (n  : Transmit FIFO index)
      When a TxFIFO is not used, the Configuration should be as follows:
          case 1 :  n > m    and Txn is not used    (n,m  : Transmit FIFO indexes)
         --> Txm can use the space allocated for Txn.
         case2  :  n < m    and Txn is not used    (n,m  : Transmit FIFO indexes)
         --> Txn should be configured with the minimum space of 16 words
     The FIFO is used optimally when used TxFIFOs are allocated in the top
         of the FIFO.Ex: use EP1 and EP2 as IN instead of EP1 and EP3 as IN ones.
     When DMA is used 3n * FIFO locations should be reserved for internal DMA registers */

    Tx_Offset = AIC_UDC_REG->rxfifosiz;

    if (fifo == 0U) {
        AIC_UDC_REG->nptxfifosiz = ((uint32_t)size << 16) | Tx_Offset;
    } else {
        Tx_Offset += (AIC_UDC_REG->nptxfifosiz) >> 16;
        for (i = 0U; i < (fifo - 1U); i++) {
            Tx_Offset += (AIC_UDC_REG->txfifosiz[i] >> 16);
        }

        /* Multiply Tx_Size by 2 to get higher performance */
        AIC_UDC_REG->txfifosiz[fifo - 1U] = ((uint32_t)size << 16) | Tx_Offset;
    }
}
#endif

static uint8_t aic_get_devspeed(void)
{
    uint8_t speed;
    uint32_t DevEnumSpeed = readl(&AIC_UDC_REG->usblinests);

    DevEnumSpeed &= USB_ENUM_SPEED_MASK;
    if (DevEnumSpeed == USB_HIGH_30_60MHZ) {
        speed = USB_ENUM_SPEED_HIGH;
    } else if (DevEnumSpeed == USB_FULL_30_60MHZ) {
        speed = USB_ENUM_SPEED_FULL;
    } else {
        speed = 0xFU;
    }

    return speed;
}

void aic_ep0_start_read_setup(void)
{
    uint32_t outepcfg = 0;
    uint8_t *psetup = (uint8_t *)&g_aic_udc.setup;
    g_aic_udc.out_ep[0].xfer_buf = psetup;
    g_aic_udc.out_ep[0].xfer_len = 8;
    ep0_ctrl_stage = AIC_EP0_SETUP;

    USB_LOG_DBG("\r\n");
    USB_LOG_DBG("[1]: Control transfer start read setup.");

    aic_udc_dcache_invalidate((uintptr_t)psetup, CACHE_LINE_SIZE);

    writel((1<<29) | (DXEPTSIZ_PKT_CNT_MASK & (3U << 19)) | (24U), &AIC_UDC_REG->outeptsfsiz[0]);

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    writel((uint32_t)(uintptr_t)psetup, &AIC_UDC_REG->outepdmaaddr[0]);
#endif
    writel(0xFFFFFFFFU, &AIC_UDC_REG->outepint[0]);

    /* EP enable */
    outepcfg = readl(&AIC_UDC_REG->outepcfg[0]);
    outepcfg |= DEPCTL_CNAK | DEPCTL_EPENA | DEPCTL_USBACTEP;
    writel(outepcfg, &AIC_UDC_REG->outepcfg[0]);
}

void usbd_ep0_start_read_zlp(void)
{
    usbd_ep_start_read(0, USB_CONTROL_OUT_EP0, (uint8_t *)&g_aic_udc.setup, 0);
}

void aic_ep_write(uint8_t ep_idx, uint8_t *src, uint16_t len)
{
    uint32_t *pSrc = (uint32_t *)src;
    uint32_t count32b, i;

    count32b = ((uint32_t)len + 3U) / 4U;
    for (i = 0U; i < count32b; i++) {
        AIC_EP_FIFO(ep_idx) = __UNALIGNED_UINT32_READ(pSrc);
        pSrc++;
    }
}

void aic_ep_read(uint8_t *dest, uint16_t len)
{
    uint32_t *pDest = (uint32_t *)dest;
    uint32_t i;
    uint32_t count32b = ((uint32_t)len + 3U) / 4U;

    for (i = 0U; i < count32b; i++) {
        __UNALIGNED_UINT32_WRITE(pDest, AIC_EP_FIFO(0U));
        pDest++;
    }
}

#ifndef CONFIG_USB_AIC_DMA_ENABLE
static int aic_tx_fifo_is_ready(uint8_t ep_idx)
{
    uint32_t len;
    uint32_t len32b;

    len = g_aic_udc.in_ep[ep_idx].xfer_len - g_aic_udc.in_ep[ep_idx].actual_xfer_len;
    if (len > g_aic_udc.in_ep[ep_idx].ep_mps) {
        len = g_aic_udc.in_ep[ep_idx].ep_mps;
    }

    len32b = (len + 3U) / 4U;

    if (AIC_UDC_REG->inepcfg[ep_idx] & DEPCTL_EPENA
            && (g_aic_udc.in_ep[ep_idx].actual_xfer_len < g_aic_udc.in_ep[ep_idx].xfer_len)
            && (g_aic_udc.in_ep[ep_idx].xfer_len != 0U)
#ifdef AIC_USB_DEVICE_DRV_V10
            &&((AIC_UDC_REG->nptxfifosts & INEPTXSTS_IN_EP_TXFIFO_STS) >= len32b)
            && ((AIC_UDC_REG->nptxfifosts & INEPTXSTS_IN_EP_TXQUEUE_STS) != 0)
#else
            &&((AIC_UDC_REG->ineptxsts[ep_idx] & INEPTXSTS_IN_EP_TXFIFO_STS) >= len32b)
#endif
    ) {
        return 0;
    } else {
        return -1;
    }
}

static void aic_tx_fifo_empty_procecss(uint8_t ep_idx)
{
    uint32_t len;

    len = g_aic_udc.in_ep[ep_idx].xfer_len - g_aic_udc.in_ep[ep_idx].actual_xfer_len;

    while (aic_tx_fifo_is_ready(ep_idx) == 0) {
        /* Write the FIFO */
        len = g_aic_udc.in_ep[ep_idx].xfer_len - g_aic_udc.in_ep[ep_idx].actual_xfer_len;
        if (len > g_aic_udc.in_ep[ep_idx].ep_mps) {
            len = g_aic_udc.in_ep[ep_idx].ep_mps;
        }

        aic_ep_write(ep_idx, g_aic_udc.in_ep[ep_idx].xfer_buf, len);
        g_aic_udc.in_ep[ep_idx].xfer_buf += len;
        g_aic_udc.in_ep[ep_idx].actual_xfer_len += len;
    }
}
#endif

/**
  * @brief  aic_get_glb_intstatus: return the global USB interrupt status
  * @retval status
  */
static inline uint32_t aic_get_glb_intstatus(void)
{
    uint32_t tmpreg;
    uint32_t mask;

    tmpreg = readl(&AIC_UDC_REG->usbintsts);
    mask = readl(&AIC_UDC_REG->usbintmsk);

    return tmpreg & mask;
}

/**
  * @brief  aic_get_outeps_intstatus: return the USB device OUT endpoints interrupt status
  * @retval status
  */
static inline uint32_t aic_get_outeps_intstatus(void)
{
    uint32_t tmpreg;
    uint32_t mask;

    tmpreg = readl(&AIC_UDC_REG->usbepint);
    mask = readl(&AIC_UDC_REG->usbepintmsk);

    return (((tmpreg & mask) & 0xffff0000U) >> 16);
}

/**
  * @brief  aic_get_ineps_intstatus: return the USB device IN endpoints interrupt status
  * @retval status
  */
static inline uint32_t aic_get_ineps_intstatus(void)
{
    uint32_t tmpreg;
    uint32_t mask;

    tmpreg = readl(&AIC_UDC_REG->usbepint);
    mask = readl(&AIC_UDC_REG->usbepintmsk);

    return ((tmpreg & mask) & 0xFFFFU);
}

/**
  * @brief  Returns Device OUT EP Interrupt register
  * @param  epnum  endpoint number
  *          This parameter can be a value from 0 to 15
  * @retval Device OUT EP Interrupt register
  */
static inline uint32_t aic_get_outep_intstatus(uint8_t epnum)
{
    uint32_t tmpreg;
    uint32_t mask;

    tmpreg = readl(&AIC_UDC_REG->outepint[epnum]);
    USB_LOG_DBG("[int]: Out ep%d outepint = 0x%x [%d].", epnum, tmpreg, ep0_ctrl_stage);
    mask = readl(&AIC_UDC_REG->outepintmsk);

    return tmpreg & mask;
}

/**
  * @brief  Returns Device IN EP Interrupt register
  * @param  epnum  endpoint number
  *          This parameter can be a value from 0 to 15
  * @retval Device IN EP Interrupt register
  */
static inline uint32_t aic_get_inep_intstatus(uint8_t epnum)
{
    uint32_t tmpreg;
    uint32_t mask;

    tmpreg = readl(&AIC_UDC_REG->inepint[epnum]);
    USB_LOG_DBG("[int]: In ep%d inepint = 0x%x [%d].", epnum, tmpreg, ep0_ctrl_stage);
    mask = readl(&AIC_UDC_REG->inepintmsk);

    return tmpreg & mask;
}

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_rst(void)
{
    uint32_t outepintmsk = 0;
    uint32_t inepintmsk = 0;

    USB_LOG_DBG("usb_dc_rst.\n");

    usbd_set_address(0, 0);

    aic_flush_txfifo(0x10U);
    aic_flush_rxfifo();

    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if (i == 0U) {
            writel(DEPCTL_SNAK, &AIC_UDC_REG->inepcfg[i]);
            writel(DEPCTL_SNAK, &AIC_UDC_REG->outepcfg[i]);
        } else {
            usbd_ep_close(0, i);
            usbd_ep_close(0, i | 0x80);
        }

        writel(0U, &AIC_UDC_REG->ineptsfsiz[i]);
        writel(0xFFFFFFFFU, &AIC_UDC_REG->inepint[i]);
        writel(0U, &AIC_UDC_REG->outeptsfsiz[i]);
        writel(0xFFFFFFFFU, &AIC_UDC_REG->outepint[i]);
    }

    writel(0, &AIC_UDC_REG->usbepintmsk);
    outepintmsk = TRANSFER_DONE | CTRL_OUT_EP_SETUP_PHASE_DONE | CTRL_OUT_EP_STATUS_PHASE_RCVD;
    inepintmsk = TRANSFER_DONE | NON_ISO_IN_EP_TIMEOUT | INTKNEPMIS;
#ifndef CONFIG_USB_AIC_DMA_ENABLE
    outepintmsk |= CTRL_OUT_EP_SETUP_PHASE_DONE;
    inepintmsk |= INTKN_TXFEMP;
#endif
    writel(outepintmsk, &AIC_UDC_REG->outepintmsk);
    writel(inepintmsk, &AIC_UDC_REG->inepintmsk);

    aic_set_dma_nextep();

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        aic_udc_obuf_free(&g_aic_udc.out_ep[i]);
        aic_udc_ibuf_free(&g_aic_udc.in_ep[i]);
    }
#endif

    memset(&g_aic_udc, 0, sizeof(struct aic_udc));
    usbd_event_reset_handler(0);
    /* Start reading setup */
    aic_ep0_start_read_setup();

    return 0;
}

static int _set_tx_fifo_size(void)
{
    #ifndef AIC_USB_DEVICE_DRV_V10
    g_tx_fifo_sz_array[0] = AIC_DEDICATED_TX_FIFO0_SIZE;
    g_tx_fifo_sz_array[1] = AIC_DEDICATED_TX_FIFO1_SIZE;
    g_tx_fifo_sz_array[2] = AIC_DEDICATED_TX_FIFO2_SIZE;
    g_tx_fifo_sz_array[3] = AIC_DEDICATED_TX_FIFO3_SIZE;
    g_tx_fifo_sz_array[4] = AIC_DEDICATED_TX_FIFO4_SIZE;
    #endif
    return 0;
}

int usb_dc_init(uint8_t busid)
{
    int ret;
    uint32_t base = 0;
    uint32_t tmpreg = 0;

    memset(&g_aic_udc, 0, sizeof(struct aic_udc));

    usb_dc_low_level_init();

    /* Disconnect */
    tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
    tmpreg |= USBDEVFUNC_SFTDISCON;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);
    /* Disable Interrupt */
    tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    tmpreg &= ~USBDEVINIT_GLBL_INTR_EN;
    writel(tmpreg, &AIC_UDC_REG->usbdevinit);

    ret = aic_core_init();

    for (uint8_t i = 0U; i < (USB_NUM_BIDIR_ENDPOINTS - 1); i++) {
        writel(0, &AIC_UDC_REG->txfifosiz[i]);
    }

    _set_tx_fifo_size();

    /* Device mode configuration */
    tmpreg = readl(&AIC_UDC_REG->usbdevconf);
    tmpreg |= PERIOD_FRAME_INTERVAL_80;

#if defined(CONFIG_USB_HS)
    /* Set Core speed to High speed mode */
    tmpreg |= DEV_SPEED_HIGH_SPEED_20;
#else
    tmpreg |= DEV_SPEED_FULL_SPEED_20;
#endif
    writel(tmpreg, &AIC_UDC_REG->usbdevconf);

    ret = aic_flush_txfifo(0x10U);
    ret = aic_flush_rxfifo();

    /* Clear all pending Device Interrupts */
    writel(0, &AIC_UDC_REG->inepintmsk);
    writel(0, &AIC_UDC_REG->outepintmsk);
    writel(0, &AIC_UDC_REG->usbepintmsk);

    /* Disable all interrupts. */
    writel(0, &AIC_UDC_REG->usbintmsk);
    /* Clear any pending interrupts */
    writel(0xBFFFFFFFU, &AIC_UDC_REG->usbintsts);

    /* Enable interrupts matching to the Device mode ONLY */
    tmpreg = INT_RESET | INT_ENUMDONE |
             INT_OUT_EP | INT_IN_EP |
             INT_INCOMP_ISO_IN_INT | INT_INCOMP_ISO_OUT_INT | INT_SUSPEND | INT_RESUME;
    writel(tmpreg, &AIC_UDC_REG->usbintmsk);

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    tmpreg |= (USBDEVINIT_HBSTLEN_INCR4 << USBDEVINIT_HBSTLEN_SHIFT);
    tmpreg |= USBDEVINIT_DMA_EN;
    writel(tmpreg, &AIC_UDC_REG->usbdevinit);
#else
    tmpreg = readl(&AIC_UDC_REG->usbintmsk);
    tmpreg |= INT_RX_FIFO_NOT_EMPTY;
    writel(tmpreg, &AIC_UDC_REG->usbintmsk);
#endif

    /* Assign FIFO */
    base = 0;
    writel(AIC_RX_FIFO_SIZE, &AIC_UDC_REG->rxfifosiz);
    base += AIC_RX_FIFO_SIZE;
#ifdef AIC_USB_DEVICE_DRV_V10
    writel((AIC_NP_TX_FIFO_SIZE << 16) | base, &AIC_UDC_REG->nptxfifosiz);
    base += AIC_NP_TX_FIFO_SIZE;
    writel((AIC_PERIOD_TX_FIFO1_SIZE << 16) | base, &AIC_UDC_REG->txfifosiz[0]);
    base += AIC_PERIOD_TX_FIFO1_SIZE;
    writel((AIC_PERIOD_TX_FIFO2_SIZE << 16) | base, &AIC_UDC_REG->txfifosiz[1]);
#else
    AIC_UDC_REG->ietxfifosiz = (g_tx_fifo_sz_array[0] << 16) | base;
    base += g_tx_fifo_sz_array[0];

    for (int i = 1; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        /* txfifo is configuared separately, starting from txfifo 1 here. */

        writel((g_tx_fifo_sz_array[i] << 16) | base, &AIC_UDC_REG->txfifosiz[i -1]);
        base += g_tx_fifo_sz_array[i];
        if (g_tx_fifo_sz_array[i] == 0)
            writel((g_tx_fifo_sz_array[i] << 16) | 0, &AIC_UDC_REG->txfifosiz[i -1]);
    }
#endif

    usb_dc_rst();

    /* Enable Interrupt */
    tmpreg = readl(&AIC_UDC_REG->usbdevinit);
    tmpreg |= USBDEVINIT_GLBL_INTR_EN;
    writel(tmpreg, &AIC_UDC_REG->usbdevinit);
    /* Connect */
    tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
    tmpreg &= ~USBDEVFUNC_SFTDISCON;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);
#if defined(KERNEL_RTTHREAD) && defined(LPKG_CHERRYUSB_DEVICE_PLUG_CHECK)
    ret = usbd_plug_irq_init();
#endif
#ifdef RT_USING_PM
    ret = usbd_pm_init();
#endif
    return ret;
}

int usb_dc_deinit(uint8_t busid)
{
    uint32_t tmpreg = 0;

    usbd_event_suspend_handler(busid);

    /* Clear Pending interrupt */
    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        writel(0xFB7FU, &AIC_UDC_REG->outepint[i]);
        writel(0xFB7FU, &AIC_UDC_REG->inepint[i]);
    }

    /* Clear interrupt masks */
    writel(0, &AIC_UDC_REG->inepintmsk);
    writel(0, &AIC_UDC_REG->outepintmsk);
    writel(0, &AIC_UDC_REG->usbepintmsk);

    /* Flush the FIFO */
    aic_flush_txfifo(0x10U);
    aic_flush_rxfifo();

    tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
    tmpreg |= USBDEVFUNC_SFTDISCON;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);

    usb_dc_low_level_deinit();
    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    uint32_t tmpreg = 0;

    tmpreg = readl(&AIC_UDC_REG->usbdevconf);
    tmpreg &= ~(DEVICE_ADDRESS_MASK);
    tmpreg |= ((uint32_t)addr << 4) & DEVICE_ADDRESS_MASK;
    writel(tmpreg, &AIC_UDC_REG->usbdevconf);

    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    uint32_t tmpreg = 0;

    tmpreg |= USBDEVFUNC_RMTWKUPSIG;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);
    aicos_mdelay(7);
    tmpreg &= ~USBDEVFUNC_RMTWKUPSIG;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);
    aicos_mdelay(7);

    return 0;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    uint8_t speed;
    uint32_t DevEnumSpeed = readl(&AIC_UDC_REG->usblinests);

    DevEnumSpeed &= USB_ENUM_SPEED_MASK;

    if (DevEnumSpeed == USB_HIGH_30_60MHZ) {
        speed = USB_SPEED_HIGH;
    } else if (DevEnumSpeed == USB_FULL_30_60MHZ) {
        speed = USB_SPEED_FULL;
    } else {
        speed = USB_SPEED_FULL;
    }

    return speed;
}

void usbd_iso_threshold_cfg(void)
{
#ifndef AIC_USB_DEVICE_DRV_V10
    uint32_t reg = readl(&AIC_UDC_REG->thr_ctl);

    reg |= ISO_THR_EN | TX_THR_LEN(128);

    writel(reg, &AIC_UDC_REG->thr_ctl);
#endif
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t tx_fifo_num = 0;
    uint32_t usbepintmsk = 0;
    uint32_t epcfg = 0;

    if (ep_idx > (USB_NUM_BIDIR_ENDPOINTS - 1)) {
        USB_LOG_ERR("Ep addr %d overflow\r\n", ep->bEndpointAddress);
        return -1;
    }

    ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
    if (ep_idx == 0) {
        switch (ep_mps) {
            case 8:
                ep_mps = DEPCTL0_MPS_8;
                break;
            case 16:
                ep_mps = DEPCTL0_MPS_16;
                break;
            case 32:
                ep_mps = DEPCTL0_MPS_32;
                break;
            case 64:
                ep_mps = DEPCTL0_MPS_64;
                break;
        }
    }

    usbepintmsk = readl(&AIC_UDC_REG->usbepintmsk);
    if (USB_EP_DIR_IS_OUT(ep->bEndpointAddress)) {
        g_aic_udc.out_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_aic_udc.out_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

        usbepintmsk |= DAINT_OUT_MASK & (uint32_t)(1UL << (16 + ep_idx));
        writel(usbepintmsk, &AIC_UDC_REG->usbepintmsk);
        epcfg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
        epcfg |= (ep_mps & DEPCTL_MPS_MASK) |
                  ((uint32_t)USB_GET_ENDPOINT_TYPE(ep->bmAttributes) << 18) |
                  DEPCTL_SETD0PID |
                  DEPCTL_USBACTEP;
        writel(epcfg, &AIC_UDC_REG->outepcfg[ep_idx]);
    } else {
        g_aic_udc.in_ep_desc[ep_idx] = *ep;
        g_aic_udc.in_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_aic_udc.in_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);

        usbepintmsk |= DAINT_IN_MASK & (uint32_t)(1UL << ep_idx);
        writel(usbepintmsk, &AIC_UDC_REG->usbepintmsk);
#ifdef AIC_USB_DEVICE_DRV_V10
        /* Period IN EP alloc fifo num */
        if ((USB_GET_ENDPOINT_TYPE(ep->bmAttributes) == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (USB_GET_ENDPOINT_TYPE(ep->bmAttributes) == USB_ENDPOINT_TYPE_ISOCHRONOUS)) {
            for (int i = 1; i <= USB_P_TXFIFO_NUM; i++) {
                if (g_aic_udc.p_txfifo_map & (1 << i))
                    continue;
                g_aic_udc.p_txfifo_map |= (1 << i);
                tx_fifo_num = i;
                break;
            }

            if (tx_fifo_num == 0)
                return -1;

            aic_flush_txfifo(tx_fifo_num);
        } else {
            g_aic_udc.np_txfifo_map |= (1 << ep_idx);
        }
        aic_set_dma_nextep();
#else
        tx_fifo_num = ep_idx;
        aic_flush_txfifo(tx_fifo_num);
        if ((USB_GET_ENDPOINT_TYPE(ep->bmAttributes) == USB_ENDPOINT_TYPE_ISOCHRONOUS)) {
            usbd_iso_threshold_cfg();
        }
#endif

        epcfg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
        epcfg |= (ep_mps & DEPCTL_MPS_MASK) |
                 ((uint32_t)USB_GET_ENDPOINT_TYPE(ep->bmAttributes) << 18) |
                 (tx_fifo_num << 22) |
                 DEPCTL_SETD0PID |
                 DEPCTL_USBACTEP;
        writel(epcfg, &AIC_UDC_REG->inepcfg[ep_idx]);
    }
    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint8_t tx_fifo_num = 0;
    int i = 0;
    uint32_t outepcfg = 0;
    uint32_t outepint = 0;
    uint32_t inepcfg = 0;
    uint32_t inepint = 0;
    uint32_t usbintsts = 0;
    uint32_t usbdevfunc = 0;
    uint32_t usbepintmsk = 0;

    #define DIS_EP_TIMOUT 100

    if (USB_EP_DIR_IS_OUT(ep)) {
        outepcfg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
        if (outepcfg & DEPCTL_EPENA) {
            /* (1) Wait for global nak to take effect */
            usbintsts = readl(&AIC_UDC_REG->usbintsts);
            if (!(usbintsts & INT_GOUTNAKEFF)) {
                usbdevfunc = readl(&AIC_UDC_REG->usbdevfunc);
                usbdevfunc |= USBDEVFUNC_SGOUTNAK;
                writel(usbdevfunc, &AIC_UDC_REG->usbdevfunc);
            }

            for (i = 0; i < DIS_EP_TIMOUT; i++) {
                usbintsts = readl(&AIC_UDC_REG->usbintsts);
                if (usbintsts & INT_GOUTNAKEFF)
                    break;
                aic_udelay(1);
            }

            if (i == DIS_EP_TIMOUT)
                USB_LOG_ERR("%s: timeout USBINTSTS.GOUTNAKEFF\n", __func__);

            /* (2) Disable ep */
            outepcfg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
            outepcfg |= DEPCTL_SNAK;
            outepcfg |= DEPCTL_EPDIS;
            writel(outepcfg, &AIC_UDC_REG->outepcfg[ep_idx]);

            for (i = 0; i < DIS_EP_TIMOUT*10; i++) {
                outepint = readl(&AIC_UDC_REG->outepint[ep_idx]);
                if (outepint & EPDISBLD)
                    break;
                aic_udelay(1);
            }

            if (i == DIS_EP_TIMOUT)
                USB_LOG_ERR("%s: timeout OUTEPCFG.EPDisable\n", __func__);

            /* Clear EPDISBLD interrupt */
            outepint = readl(&AIC_UDC_REG->outepint[ep_idx]);
            outepint |= EPDISBLD;
            writel(outepint, &AIC_UDC_REG->outepint[ep_idx]);

            /* (3) Remove global NAKs */
            usbdevfunc = readl(&AIC_UDC_REG->usbdevfunc);
            usbdevfunc |= USBDEVFUNC_CGOUTNAK;
            writel(usbdevfunc, &AIC_UDC_REG->usbdevfunc);
        }

        usbepintmsk = readl(&AIC_UDC_REG->usbepintmsk);
        usbepintmsk &= ~(DAINT_OUT_MASK & (uint32_t)(1UL << (16 + ep_idx)));
        writel(usbepintmsk, &AIC_UDC_REG->usbepintmsk);
        writel(0, &AIC_UDC_REG->outepcfg[ep_idx]);
    } else {
        inepcfg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
        tx_fifo_num = (inepcfg & DEPCTL_TXFIFONUM_MASK) >> DEPCTL_TXFIFONUM_SHIFT;

        inepcfg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
        if (inepcfg & DEPCTL_EPENA) {
            if (tx_fifo_num) {
                /* (1) Wait for Nak effect */
                inepcfg |= DEPCTL_SNAK;
                writel(inepcfg, &AIC_UDC_REG->inepcfg[ep_idx]);

                for (i = 0; i < DIS_EP_TIMOUT; i++) {
                    inepint = readl(&AIC_UDC_REG->inepint[ep_idx]);
                    if (inepint & INEP_NAKEFF)
                        break;
                    aic_udelay(1);
                }

                if (i == DIS_EP_TIMOUT)
                    USB_LOG_ERR("%s: timeout INEPINT.NAKEFF\n", __func__);
            } else {
                /* (1) Wait for Nak effect */
                usbdevfunc = readl(&AIC_UDC_REG->usbdevfunc);
                usbdevfunc |= USBDEVFUNC_SGNPINNAK;
                writel(usbdevfunc, &AIC_UDC_REG->usbdevfunc);

                for (i = 0; i < DIS_EP_TIMOUT; i++) {
                    usbintsts = readl(&AIC_UDC_REG->usbintsts);
                    if (usbintsts & INT_GINNAKEFF)
                        break;
                    aic_udelay(1);
                }

                if (i == DIS_EP_TIMOUT)
                    USB_LOG_ERR("%s: timeout USBINTSTS.GOUTNAKEFF\n", __func__);
            }

            /* (2) Disable ep */
            inepcfg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
            inepcfg |= DEPCTL_SNAK;
            inepcfg |= DEPCTL_EPDIS;
            writel(inepcfg, &AIC_UDC_REG->inepcfg[ep_idx]);

            for (i = 0; i < DIS_EP_TIMOUT; i++) {
                inepint = readl(&AIC_UDC_REG->inepint[ep_idx]);
                if (inepint & EPDISBLD)
                    break;
                aic_udelay(1);
            }

            if (i == DIS_EP_TIMOUT)
                USB_LOG_ERR("%s: timeout OUTEPCFG.EPDisable\n", __func__);

            /* Clear EPDISBLD interrupt */
            inepint = readl(&AIC_UDC_REG->inepint[ep_idx]);
            inepint |= EPDISBLD;
            writel(inepint, &AIC_UDC_REG->inepint[ep_idx]);

            /* (3) Flush TX FIFO */
            aic_flush_txfifo(tx_fifo_num);

            /* Period IN EP free fifo num */
            #ifdef AIC_USB_DEVICE_DRV_V10
            if (tx_fifo_num > 0)
                g_aic_udc.p_txfifo_map &= ~(1 << tx_fifo_num);
            else
                g_aic_udc.np_txfifo_map &= ~(1 << ep_idx);
            #endif

            /* (4) Clear Global In NP NAK in Shared FIFO for non periodic ep */
            if (!tx_fifo_num) {
                usbdevfunc = readl(&AIC_UDC_REG->usbdevfunc);
                usbdevfunc |= USBDEVFUNC_CGNPINNAK;
                writel(usbdevfunc, &AIC_UDC_REG->usbdevfunc);
            }
        }

        usbepintmsk = readl(&AIC_UDC_REG->usbepintmsk);
        usbepintmsk &= ~(DAINT_OUT_MASK & (uint32_t)(1UL << ep_idx));
        writel(usbepintmsk, &AIC_UDC_REG->usbepintmsk);
        writel(0, &AIC_UDC_REG->inepcfg[ep_idx]);
    }

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    if (USB_EP_DIR_IS_OUT(ep)) {
        aic_udc_obuf_free(&g_aic_udc.out_ep[ep_idx]);
    } else {
        aic_udc_ibuf_free(&g_aic_udc.in_ep[ep_idx]);
    }
#endif

    return 0;
}

int usbd_npinep_rewrite(const uint8_t ep)
{
    int i = 0;
    int j = 0;
    int fail = 0;
    unsigned int pending_map = 0;
    //int data_len = 0;
    uint32_t tmpreg = 0;

    for (i = 0U, j = 0; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        tmpreg = readl(&AIC_UDC_REG->inepcfg[i]);
        if (!((g_aic_udc.np_txfifo_map & (1 << i)) &&
            (tmpreg & DEPCTL_EPENA)))
            continue;

        j++;

        pending_map |= (1 << i);
    }
    if (j <= 1)
        return 0;

    /* (1) close all no-periodic ep */
    /* (1.1) Set Global In NP NAK in Shared FIFO for non periodic ep */
    tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
    tmpreg |= USBDEVFUNC_SGNPINNAK;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);

    for (i = 0; i < DIS_EP_TIMOUT; i++) {
        tmpreg = readl(&AIC_UDC_REG->usbintsts);
        if (tmpreg & INT_GINNAKEFF)
            break;
        aic_udelay(1);
    }

    if (i == DIS_EP_TIMOUT)
        USB_LOG_ERR("%s: timeout USBINTSTS.GOUTNAKEFF\n", __func__);

    /* (1.2) Disable ep */
    for (i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        tmpreg = readl(&AIC_UDC_REG->inepcfg[i]);
        if (!((g_aic_udc.np_txfifo_map & (1 << i)) &&
            (tmpreg & DEPCTL_EPENA)))
            continue;

        tmpreg |= DEPCTL_SNAK;
        tmpreg |= DEPCTL_EPDIS;
        writel(tmpreg, &AIC_UDC_REG->inepcfg[i]);

        /* remain data */
        #if 0
        tmpreg = readl(&AIC_UDC_REG->ineptsfsiz[i]);
        data_len = tmpreg & DXEPTSIZ_XFER_SIZE_MASK;
        g_aic_udc.in_ep[ep].xfer_buf += g_aic_udc.in_ep[ep].xfer_len - data_len;
        g_aic_udc.in_ep[ep].xfer_len = data_len;
        #endif

        //pending_map |= (1 << i);
    }

    for (j = 0; j < DIS_EP_TIMOUT; j++) {
        fail = 0;
        for (i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
            if (!(pending_map & (1 << i)))
                continue;
            tmpreg = readl(&AIC_UDC_REG->inepint[i]);
            if (!(tmpreg & EPDISBLD)) {
                fail = i+1;
                break;
            }
        }
        if (!fail)
            break;
        aic_udelay(1);
    }

    if (j == DIS_EP_TIMOUT)
        USB_LOG_ERR("%s: ep%d timeout OUTEPCFG.EPDisable\n", __func__, fail-1);

    /* Clear EPDISBLD interrupt */
    for (i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if (!(pending_map & (1 << i)))
            continue;
        tmpreg = readl(&AIC_UDC_REG->inepint[i]);
        tmpreg |= EPDISBLD;
        writel(tmpreg, &AIC_UDC_REG->inepint[i]);
    }

    /* (1.3) Flush TX FIFO0 */
    aic_flush_txfifo(0);

    /* (1.4) Clear Global In NP NAK in Shared FIFO for non periodic ep */
    tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
    tmpreg |= USBDEVFUNC_CGNPINNAK;
    writel(tmpreg, &AIC_UDC_REG->usbdevfunc);

    /* (2) reopen current ep */
    usbd_ep_open(0, &g_aic_udc.in_ep_desc[ep]);

    /* (3) rewrite current ep */
    if (pending_map & (1 << ep)) {
        usbd_ep_start_write(0, ep, g_aic_udc.in_ep[ep].xfer_buf,
                                g_aic_udc.in_ep[ep].xfer_len);
    }

    /* (4) reopen & rewrite other ep, let's receive ep mismtach interrupt */
    for (i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if (!(pending_map & (1 << i)) ||
            (i == ep))
            continue;
        usbd_ep_open(0, &g_aic_udc.in_ep_desc[i]);
        usbd_ep_start_write(0, i, g_aic_udc.in_ep[i].xfer_buf,
                            g_aic_udc.in_ep[i].xfer_len);
    }

    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t tmpreg = 0;

    if (USB_EP_DIR_IS_OUT(ep)) {
        tmpreg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
        if (((tmpreg & DEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            tmpreg &= ~(DEPCTL_EPDIS);
        }
        tmpreg |= DEPCTL_STALL;
        writel(tmpreg, &AIC_UDC_REG->outepcfg[ep_idx]);
    } else {
        tmpreg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
        if (((tmpreg & DEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            tmpreg &= ~(DEPCTL_EPDIS);
        }
        tmpreg |= DEPCTL_STALL;
        writel(tmpreg, &AIC_UDC_REG->inepcfg[ep_idx]);
    }

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    if (ep_idx == 0) {
        aic_ep0_start_read_setup();
    }
#endif
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t tmpreg = 0;

    if (USB_EP_DIR_IS_OUT(ep)) {
        tmpreg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
        tmpreg &= ~DEPCTL_STALL;
        if ((g_aic_udc.out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (g_aic_udc.out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            tmpreg |= DEPCTL_SETD0PID; /* DATA0 */
        }
        writel(tmpreg, &AIC_UDC_REG->outepcfg[ep_idx]);
    } else {
        tmpreg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
        tmpreg &= ~DEPCTL_STALL;
        if ((g_aic_udc.in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (g_aic_udc.in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            tmpreg |= DEPCTL_SETD0PID; /* DATA0 */
        }
        writel(tmpreg, &AIC_UDC_REG->inepcfg[ep_idx]);
    }

    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
    } else {
    }
    return 0;
}

int usbd_ep_start_write_raw(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint8_t cache_align)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;
    uint32_t inepcfg = 0;
    uint32_t ineptsfsiz = 0;
    uint32_t usblinests = 0;

    if (!data && data_len) {
        return -1;
    }
    inepcfg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
    if (inepcfg & DEPCTL_EPENA) {
        return -2;
    }
    if (ep_idx && !(inepcfg & DEPCTL_MPS_MASK)) {
        return -3;
    }
    if ((uint32_t)(uintptr_t)data & 0x03) {
        return -4;
    }

    g_aic_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_aic_udc.in_ep[ep_idx].xfer_len = data_len;
    g_aic_udc.in_ep[ep_idx].actual_xfer_len = 0;

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    if (cache_align && (ep_idx != 0) && (data_len > 0) &&
        ((((uint32_t)(uintptr_t)data % CACHE_LINE_SIZE) != 0) ||
         (((uint32_t)(uintptr_t)(data + data_len) % CACHE_LINE_SIZE) != 0))) {

        if (g_aic_udc.in_ep[ep_idx].xfer_align_len != data_len) {
            int ret = 0;

            aic_udc_ibuf_free(&g_aic_udc.in_ep[ep_idx]);
            ret = aic_udc_ibuf_alloc(&g_aic_udc.in_ep[ep_idx], data_len);
            if (ret)
                return ret;
        }
        memcpy(g_aic_udc.in_ep[ep_idx].xfer_align_buf, data, data_len);
        data = g_aic_udc.in_ep[ep_idx].xfer_align_buf;
    } else {
        aic_udc_ibuf_free(&g_aic_udc.in_ep[ep_idx]);
    }

    if (data_len>0)
        aic_udc_dcache_clean((uintptr_t)data, ALIGN_UP(data_len, CACHE_LINE_SIZE));
#endif

    ineptsfsiz = readl(&AIC_UDC_REG->ineptsfsiz[ep_idx]);
    ineptsfsiz &= ~(DXEPTSIZ_PKT_CNT_MASK);
    ineptsfsiz &= ~(DXEPTSIZ_XFER_SIZE_MASK);

    if (data_len == 0) {
#ifdef CONFIG_USB_AIC_DMA_ENABLE
        if (data)
            writel((uint32_t)(uintptr_t)data, &AIC_UDC_REG->inepdmaaddr[ep_idx]);
#endif
        ineptsfsiz |= (DXEPTSIZ_PKT_CNT_MASK & (1U << 19));
        writel(ineptsfsiz, &AIC_UDC_REG->ineptsfsiz[ep_idx]);
        inepcfg |= (DEPCTL_CNAK | DEPCTL_EPENA);
        writel(inepcfg, &AIC_UDC_REG->inepcfg[ep_idx]);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > g_aic_udc.in_ep[ep_idx].ep_mps) {
            data_len = g_aic_udc.in_ep[ep_idx].ep_mps;
        }
        g_aic_udc.in_ep[ep_idx].xfer_len = data_len;
        ineptsfsiz |= (DXEPTSIZ_PKT_CNT_MASK & (1U << 19));
        ineptsfsiz |= (DXEPTSIZ_XFER_SIZE_MASK & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + g_aic_udc.in_ep[ep_idx].ep_mps - 1U) / g_aic_udc.in_ep[ep_idx].ep_mps);

        ineptsfsiz |= (DXEPTSIZ_PKT_CNT_MASK & (pktcnt << 19));
        ineptsfsiz |= (DXEPTSIZ_XFER_SIZE_MASK & data_len);
    }

    if (g_aic_udc.in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        usblinests = readl(&AIC_UDC_REG->usblinests);
        if ((usblinests & (1U << 8)) == 0U) {
            inepcfg &= ~DEPCTL_SETD0PID;
            inepcfg |= DEPCTL_SETD1PID;
        } else {
            inepcfg &= ~DEPCTL_SETD1PID;
            inepcfg |= DEPCTL_SETD0PID;
        }
        ineptsfsiz &= ~(DXEPTSIZ_MULCNT_MASK);
        ineptsfsiz |= ((MIN(3, pktcnt) << DXEPTSIZ_MULCNT_SHIFT) & DXEPTSIZ_MULCNT_MASK);
    } else if (g_aic_udc.in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
        ineptsfsiz &= ~(DXEPTSIZ_MULCNT_MASK);
        ineptsfsiz |= (1U << DXEPTSIZ_MULCNT_SHIFT);
    }
    writel(ineptsfsiz, &AIC_UDC_REG->ineptsfsiz[ep_idx]);

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    writel((uint32_t)(uintptr_t)data, &AIC_UDC_REG->inepdmaaddr[ep_idx]);
#endif

    writel(0xFFFFFFFFU, &AIC_UDC_REG->inepint[ep_idx]);

    inepcfg |= (DEPCTL_CNAK | DEPCTL_EPENA);
    writel(inepcfg, &AIC_UDC_REG->inepcfg[ep_idx]);

    return 0;
}


int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    return usbd_ep_start_write_raw(ep, data, data_len, 1);
}

int usbd_ep_start_read_raw(const uint8_t ep, uint8_t *data, uint32_t data_len, uint8_t cache_align)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;
    uint32_t outepcfg = 0;
    uint32_t outeptsfsiz = 0;
    uint32_t usblinests = 0;

    if (!data && data_len) {
        return -1;
    }
    outepcfg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
    if (outepcfg & DEPCTL_EPENA) {
        uint32_t i = 0;
        for (i = 0; i < AIC_EP_READ_TIMEOUT; i++) {
            outepcfg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
            if (!(outepcfg & DEPCTL_EPENA))
                break;
        }
        if (i == AIC_EP_READ_TIMEOUT) {
            g_aic_udc.out_ep[ep_idx].xfer_len = data_len;
            USB_LOG_ERR("ep%d AIC_UDC_REG->outepcfg[ep_idx] = 0x%x.\n",
                        (unsigned int)ep_idx, (unsigned int)outepcfg);
            return -2;
        }
    }
    if (ep_idx && !(outepcfg & DEPCTL_MPS_MASK)) {
        return -3;
    }
    if (((uint32_t)(uintptr_t)data) & 0x03) {
        return -4;
    }

    g_aic_udc.out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_aic_udc.out_ep[ep_idx].xfer_len = data_len;
    g_aic_udc.out_ep[ep_idx].actual_xfer_len = 0;

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    if (cache_align && (ep_idx != 0) && (data_len > 0) &&
        ((((uint32_t)(uintptr_t)data % CACHE_LINE_SIZE) != 0) ||
         (((uint32_t)(uintptr_t)(data + data_len) % CACHE_LINE_SIZE) != 0))) {

        if (g_aic_udc.out_ep[ep_idx].xfer_align_len != data_len) {
            int ret = 0;

            aic_udc_obuf_free(&g_aic_udc.out_ep[ep_idx]);
            ret = aic_udc_obuf_alloc(&g_aic_udc.out_ep[ep_idx], data_len);
            if (ret)
                return ret;
        }
        data = g_aic_udc.out_ep[ep_idx].xfer_align_buf;
    } else {
        aic_udc_obuf_free(&g_aic_udc.out_ep[ep_idx]);
    }

    if (data_len > 0)
        aic_udc_dcache_invalidate((uintptr_t)data, ALIGN_UP(data_len, CACHE_LINE_SIZE));
#endif

    outeptsfsiz = readl(&AIC_UDC_REG->outeptsfsiz[ep_idx]);
    outeptsfsiz &= ~(DXEPTSIZ_PKT_CNT_MASK);
    outeptsfsiz &= ~(DXEPTSIZ_XFER_SIZE_MASK);
    outeptsfsiz &= ~(DXEPTSIZ_MULCNT_MASK);

    if (data_len == 0) {
        outeptsfsiz |= (DXEPTSIZ_PKT_CNT_MASK & (1 << 19));
        outeptsfsiz |= (DXEPTSIZ_XFER_SIZE_MASK & g_aic_udc.out_ep[ep_idx].ep_mps);
        writel(outeptsfsiz, &AIC_UDC_REG->outeptsfsiz[ep_idx]);
        #ifdef CONFIG_USB_AIC_DMA_ENABLE
        if (data)
            writel((uint32_t)(uintptr_t)data, &AIC_UDC_REG->outepdmaaddr[ep_idx]);
        #endif
        outepcfg |= (DEPCTL_CNAK | DEPCTL_EPENA);
        writel(outepcfg, &AIC_UDC_REG->outepcfg[ep_idx]);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > g_aic_udc.out_ep[ep_idx].ep_mps) {
            data_len = g_aic_udc.out_ep[ep_idx].ep_mps;
        }
        g_aic_udc.out_ep[ep_idx].xfer_len = data_len;
        outeptsfsiz |= (DXEPTSIZ_PKT_CNT_MASK & (1U << 19));
        outeptsfsiz |= (DXEPTSIZ_XFER_SIZE_MASK & data_len);
        writel(outeptsfsiz, &AIC_UDC_REG->outeptsfsiz[ep_idx]);
    } else {
        pktcnt = (uint16_t)((data_len + g_aic_udc.out_ep[ep_idx].ep_mps - 1U) / g_aic_udc.out_ep[ep_idx].ep_mps);

        outeptsfsiz |= (DXEPTSIZ_PKT_CNT_MASK & (pktcnt << 19));
        outeptsfsiz |= (DXEPTSIZ_XFER_SIZE_MASK & data_len);
        writel(outeptsfsiz, &AIC_UDC_REG->outeptsfsiz[ep_idx]);
    }

#ifdef CONFIG_USB_AIC_DMA_ENABLE
    writel((uint32_t)(uintptr_t)data, &AIC_UDC_REG->outepdmaaddr[ep_idx]);
#endif
    if (g_aic_udc.out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        usblinests = readl(&AIC_UDC_REG->usblinests);
        if ((usblinests & (1U << 8)) == 0U) {
            outepcfg &= ~DEPCTL_SETD0PID;
            outepcfg |= DEPCTL_SETD1PID;
        } else {
            outepcfg &= ~DEPCTL_SETD1PID;
            outepcfg |= DEPCTL_SETD0PID;
        }
    }
    writel(0xFFFFFFFFU, &AIC_UDC_REG->outepint[ep_idx]);
    outepcfg |= (DEPCTL_CNAK | DEPCTL_EPENA);
    writel(outepcfg, &AIC_UDC_REG->outepcfg[ep_idx]);

    return 0;
}


int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    return usbd_ep_start_read_raw(ep, data, data_len, 1);
}

#ifdef CONFIG_USBDEV_TEST_MODE

void usbd_req_test_mode(uint8_t test_mode)
{
    g_aic_udc.test_mode = test_mode;
}

void usbd_execute_test_mode(uint8_t test_mode)
{
    uint32_t regval;
    uint32_t test_mode_val;

    switch (test_mode) {
        case TEST_SE0_NAK_MODE:
            test_mode_val = TEST_SE0_NAK_MODE;
            USB_LOG_INFO("TEST_SE0_NAK_MODE\n");
            break;
        case TEST_J_MODE:
            test_mode_val = TEST_J_MODE;
            USB_LOG_INFO("TEST_J_MODE\n");
            break;
        case TEST_K_MODE:
            test_mode_val = TEST_K_MODE;
            USB_LOG_INFO("TEST_K_MODE\n");
            break;
        case TEST_PACKET_MODE:
            test_mode_val = TEST_PACKET_MODE;
            USB_LOG_INFO("TEST_PACKET_MODE\n");
            break;
        case TEST_FORCE_MODE:
            test_mode_val = TEST_FORCE_MODE;
            USB_LOG_INFO("TEST_FORCE_MODE\n");
            break;
        default:
            return;
        }

        regval = readl(&AIC_UDC_REG->usbdevfunc);
        regval |= test_mode_val << USBDEVFUNC_TSTCTL_SHIFT;
        writel(regval, &AIC_UDC_REG->usbdevfunc);
}
#endif

static int usbd_ep0_setup_handler(void)
{
    uint32_t outepdmaaddr = readl(&AIC_UDC_REG->outepdmaaddr[0]);
    struct usb_setup_packet *setup = (struct usb_setup_packet *)(uintptr_t)outepdmaaddr;
    setup--;
    if (setup == NULL) {
        USB_LOG_ERR("outepdmaaddr is null.\n");
        return -1;
    }

    if (g_aic_udc.setup.wLength) {
        if ((g_aic_udc.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT) {
            ep0_ctrl_stage = AIC_EP0_DATA_OUT;
            // USB_LOG_DBG("out cb -> out data start\n");
        } else {
            ep0_ctrl_stage = AIC_EP0_DATA_IN;
        }
    } else {
        ep0_ctrl_stage = AIC_EP0_STATUS_IN;
    }

    usbd_event_ep0_setup_complete_handler(0, (uint8_t *)setup);

    return 0;
}

static int usbd_ep0_setup_handler_immediately(void)
{
    uint32_t count = 0;

    aic_udc_dcache_invalidate((uintptr_t)&g_aic_udc, CACHE_LINE_SIZE);

    USB_LOG_DBG("out cb -> out setup + done %#x %#x %#x ",
                readl(&AIC_UDC_REG->inepcfg[0]),
                readl(&AIC_UDC_REG->outepcfg[0]),
                *(uint16_t *)&g_aic_udc.setup);

    setup_handler_immediately = 1;
    do {
        if (++count > AIC_EP_FIFOCTL_TIMEOUT) {
            USB_LOG_ERR("(E) setup busy timeout. -> %#x %#x\n",
                        readl(&AIC_UDC_REG->inepcfg[0]), readl(&AIC_UDC_REG->outepcfg[0]));
            break;
        }
    } while ((readl(&AIC_UDC_REG->outepcfg[0]) & DEPCTL_NAK_STATUS) == 0);

    return 0;
}


void usbd_irq_handler(void)
{
    uint32_t gint_status, ep_idx, ep_intr, epint, daintmask, tmpreg, tmpreg01;
    gint_status = aic_get_glb_intstatus();

    /* Avoid spurious interrupt */
    if (gint_status == 0) {
        return;
    }

#if defined(RT_USING_PM) && defined(AIC_USING_PM) && defined(AIC_USB_DISP_PM_SUSPEND)
    #define USB_DEV_SUSPEND_DELAY_MS 2000
    rt_pm_module_delay_sleep(PM_USB_DEV_ID, rt_tick_from_millisecond(USB_DEV_SUSPEND_DELAY_MS));
#endif

#ifndef CONFIG_USB_AIC_DMA_ENABLE
    uint32_t temp;
    uint32_t read_count;

    /* Handle RxQLevel Interrupt */
    if (gint_status & INT_RX_FIFO_NOT_EMPTY) {
        tmpreg = readl(&AIC_UDC_REG->usbintmsk);
        tmpreg &= ~(INT_RX_FIFO_NOT_EMPTY);
        writel(tmpreg, &AIC_UDC_REG->usbintmsk);

        temp = readl(&AIC_UDC_REG->rxfifosts_pop);
        ep_idx = temp & RXFIFOSTS_EPNUM_MASK;

        if ((temp & RXFIFOSTS_PKTSTS_MASK)  == PKTSTS_OUT_DATA_PKT_REC) {
            read_count = (temp & RXFIFOSTS_BCNT_MASK) >> 4;
            if (read_count != 0) {
                aic_ep_read(g_aic_udc.out_ep[ep_idx].xfer_buf, read_count);
                g_aic_udc.out_ep[ep_idx].xfer_buf += read_count;
            }
        } else if ((temp & RXFIFOSTS_PKTSTS_MASK) == PKTSTS_SETUP_DATA_PKT_REC) {
            read_count = (temp & RXFIFOSTS_BCNT_MASK) >> 4;
            aic_ep_read((uint8_t *)&g_aic_udc.setup, read_count);
        } else {
            /* ... */
        }
        tmpreg = readl(&AIC_UDC_REG->usbintmsk);
        tmpreg |= INT_RX_FIFO_NOT_EMPTY;
        writel(tmpreg, &AIC_UDC_REG->usbintmsk);
    }
#endif
    if (gint_status & INT_OUT_EP) {
        ep_idx = 0;
        ep_intr = aic_get_outeps_intstatus();
        while (ep_intr != 0U) {
            if ((ep_intr & 0x1U) != 0U) {
                epint = aic_get_outep_intstatus(ep_idx);
                writel(epint, &AIC_UDC_REG->outepint[ep_idx]);

                struct aic_ep_state *ep = &g_aic_udc.out_ep[ep_idx];

                if ((ep_idx == 0)) {
                    #ifdef CONFIG_USB_AIC_DMA_ENABLE
                    extern void usb_dc_sync_dma(void);
                    usb_dc_sync_dma();
                    if (ep->xfer_align_buf)
                        memcpy(ep->xfer_buf, ep->xfer_align_buf, ep->xfer_len);
                    #endif

                    if (ep0_ctrl_stage == AIC_EP0_SETUP) {
                        /* Setup data-out will send STATUS_PHASE_RCVD interrupt when next setup.
                           Attemp to re-read setup data. */
                        if (epint & CTRL_OUT_EP_STATUS_PHASE_RCVD) {
                            if (epint & TRANSFER_DONE)  {
                                /* case E*/
                                aic_ep0_start_read_setup();
                                USB_LOG_DBG("(E)out cb -> out status %d \n", epint);
                            } else {
                                /* case D */
                                usbd_ep_start_read(0, USB_CONTROL_OUT_EP0, (uint8_t *)&g_aic_udc.setup, 0);
                                USB_LOG_DBG("(D)out cb -> out status %d \n", epint);
                            }
                        }

                        /* Normal setup handle */
                        if (epint & CTRL_OUT_EP_SETUP_PHASE_DONE) {
                            USB_LOG_DBG("out cb -> out setup %#x %#x %#x \n",
                                        readl(&AIC_UDC_REG->inepcfg[0]),
                                        readl(&AIC_UDC_REG->outepcfg[0]),
                                        *(uint16_t *)&g_aic_udc.setup);

                            usbd_ep0_setup_handler();
                        }
                    } else {
                        if (epint & CTRL_OUT_EP_SETUP_PHASE_DONE)  {
                            usbd_ep0_setup_handler_immediately();
                        }

                        if ((epint & TRANSFER_DONE) == TRANSFER_DONE) {
                            if (ep->xfer_len == 0) {
                                USB_LOG_DBG("out cb -> out status\n");
                                /* Out status, start reading setup */
                                if (setup_handler_immediately) {
                                    usbd_ep0_setup_handler();
                                    setup_handler_immediately = 0;
                                } else {
                                    aic_ep0_start_read_setup();
                                }
                            } else {
                                USB_LOG_DBG("out cb -> out data %#x %#x \n", readl(&AIC_UDC_REG->inepcfg[0]), readl(&AIC_UDC_REG->outepcfg[0]));
                                // ep->actual_xfer_len = ep->xfer_len - ((AIC_UDC_REG->outeptsfsiz[ep_idx]) & DXEPTSIZ_XFER_SIZE_MASK);
                                ep->actual_xfer_len = ep->xfer_len;
                                ep->xfer_len = 0;
                                usbd_event_ep_out_complete_handler(0, 0x00, ep->actual_xfer_len);
                            }
                        }
                    }
                } else {
                    #ifdef CONFIG_USB_AIC_DMA_ENABLE
                    extern void usb_dc_sync_dma(void);
                    usb_dc_sync_dma();
                    if (ep->xfer_align_buf)
                        memcpy(ep->xfer_buf, ep->xfer_align_buf, ep->xfer_len);
                    #endif
                    tmpreg = readl(&AIC_UDC_REG->outeptsfsiz[ep_idx]);
                    ep->actual_xfer_len = ep->xfer_len - (tmpreg & DXEPTSIZ_XFER_SIZE_MASK);
                    ep->xfer_len = 0;
                    usbd_event_ep_out_complete_handler(0, ep_idx, ep->actual_xfer_len);
                }

#ifndef CONFIG_USB_AIC_DMA_ENABLE
                if ((epint & CTRL_OUT_EP_SETUP_PHASE_DONE) == CTRL_OUT_EP_SETUP_PHASE_DONE) {
                    if (ep0_ctrl_stage == 1) {
                        ep0_ctrl_stage = 2;
                        usbd_event_ep0_setup_complete_handler(0, (uint8_t *)&g_aic_udc.setup);
                    }
                }
#endif
            }
            ep_intr >>= 1U;
            ep_idx++;
        }
    }

#ifndef CONFIG_USB_AIC_DMA_ENABLE
    if (gint_status & INT_NP_TX_FIFO_EMPTY) {
        tmpreg = readl(&AIC_UDC_REG->usbintmsk);
        tmpreg &= ~(INT_NP_TX_FIFO_EMPTY);
        writel(tmpreg, &AIC_UDC_REG->usbintmsk);
        aic_tx_fifo_empty_procecss(0);
    }
#endif

    if (gint_status & INT_IN_EP) {
        ep_idx = 0U;
        ep_intr = aic_get_ineps_intstatus();
        while (ep_intr != 0U) {
            if ((ep_intr & 0x1U) != 0U) {
                epint = aic_get_inep_intstatus(ep_idx);
                writel(epint, &AIC_UDC_REG->inepint[ep_idx]);

                if (epint & INTKNEPMIS) {
                    aic_udelay(100);
                    usbd_npinep_rewrite(ep_idx);
                }

                if ((epint & TRANSFER_DONE) == TRANSFER_DONE) {
                    if (ep_idx == 0) {
                        if (g_aic_udc.setup.wLength && ((g_aic_udc.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                            /* In status, start reading setup */
                            aic_ep0_start_read_setup();
                        } else if (g_aic_udc.setup.wLength == 0) {
                            /* In status, start reading setup */
                            USB_LOG_DBG("in cb -> zlp %d \n", setup_handler_immediately);
                            aic_ep0_start_read_setup();
                        } else {
                            USB_LOG_DBG("in cb -> data %d\n", g_aic_udc.in_ep[ep_idx].xfer_len);
                            g_aic_udc.in_ep[ep_idx].actual_xfer_len = g_aic_udc.in_ep[ep_idx].xfer_len - ((AIC_UDC_REG->ineptsfsiz[ep_idx]) & DXEPTSIZ_XFER_SIZE_MASK);
                            g_aic_udc.in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(0, 0x80, g_aic_udc.in_ep[ep_idx].actual_xfer_len);
                        }
#ifdef CONFIG_USBDEV_TEST_MODE
                        if (g_aic_udc.test_mode) {
                            usbd_execute_test_mode(g_aic_udc.test_mode);
                            g_aic_udc.test_mode = 0;
                            return;
                        }
#endif
                    } else {
                        tmpreg = readl(&AIC_UDC_REG->ineptsfsiz[ep_idx]);
                        g_aic_udc.in_ep[ep_idx].actual_xfer_len = g_aic_udc.in_ep[ep_idx].xfer_len - (tmpreg & DXEPTSIZ_XFER_SIZE_MASK);
                        g_aic_udc.in_ep[ep_idx].xfer_len = 0;
                        usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, g_aic_udc.in_ep[ep_idx].actual_xfer_len);
                    }
                }

                #ifndef CONFIG_USB_AIC_DMA_ENABLE
                if ((epint & INTKN_TXFEMP) == INTKN_TXFEMP) {
                    aic_tx_fifo_empty_procecss(ep_idx);
                }
                #endif
            }
            ep_intr >>= 1U;
            ep_idx++;
        }
    }
    if (gint_status & INT_RESET) {
        writel(INT_RESET, &AIC_UDC_REG->usbintsts);
        tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
        tmpreg &= ~USBDEVFUNC_RMTWKUPSIG;
        writel(tmpreg, &AIC_UDC_REG->usbdevfunc);

        usb_dc_rst();
    }

    if (gint_status & INT_ENUMDONE) {
        writel(INT_ENUMDONE, &AIC_UDC_REG->usbintsts);
        aic_set_turnaroundtime(usbd_clk, aic_get_devspeed());

        tmpreg = readl(&AIC_UDC_REG->usbdevfunc);
        tmpreg |= USBDEVFUNC_CGNPINNAK;
        writel(tmpreg, &AIC_UDC_REG->usbdevfunc);
    }
    if (gint_status & INT_INCOMP_ISO_OUT_INT) {
        daintmask = readl(&AIC_UDC_REG->usbepintmsk);
        daintmask >>= 16;

        for (ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
            if ((BIT(ep_idx) & ~daintmask) || (g_aic_udc.out_ep[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS))
                continue;

            tmpreg = readl(&AIC_UDC_REG->outepcfg[ep_idx]);
            if (!(tmpreg & DEPCTL_USBACTEP))
                continue;
            tmpreg01 = readl(&AIC_UDC_REG->usblinests);
            if ((tmpreg01 & (1U << 8)) != 0U) {
                tmpreg |= DEPCTL_SETD0PID;
                tmpreg &= ~DEPCTL_SETD1PID;
            } else {
                tmpreg &= ~DEPCTL_SETD0PID;
                tmpreg |= DEPCTL_SETD1PID;
            }
            writel(tmpreg, &AIC_UDC_REG->outepcfg[ep_idx]);
        }

        writel(INT_INCOMP_ISO_OUT_INT, &AIC_UDC_REG->usbintsts);
    }

    if (gint_status & INT_INCOMP_ISO_IN_INT) {
        daintmask = readl(&AIC_UDC_REG->usbepintmsk);
        daintmask &= 0xFFFF;

        for (ep_idx = 1; ep_idx < USB_NUM_BIDIR_ENDPOINTS; ep_idx++) {
            if (((BIT(ep_idx) & ~daintmask)) || (g_aic_udc.in_ep[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS))
                continue;

            tmpreg = readl(&AIC_UDC_REG->inepcfg[ep_idx]);
            if (!(tmpreg & DEPCTL_USBACTEP))
                continue;
            tmpreg01 = readl(&AIC_UDC_REG->usblinests);
            if ((tmpreg01 & (1U << 8)) != 0U) {
                tmpreg |= DEPCTL_SETD0PID;
                tmpreg &= ~DEPCTL_SETD1PID;
            } else {
                tmpreg &= ~DEPCTL_SETD0PID;
                tmpreg |= DEPCTL_SETD1PID;
            }
            writel(tmpreg, &AIC_UDC_REG->inepcfg[ep_idx]);
        }

        writel(INT_INCOMP_ISO_IN_INT, &AIC_UDC_REG->usbintsts);
    }

    if (gint_status & INT_SOF) {
        writel(INT_SOF, &AIC_UDC_REG->usbintsts);
    }
    if (gint_status & INT_SUSPEND) {
        writel(INT_SUSPEND, &AIC_UDC_REG->usbintsts);
        usbd_event_suspend_handler(0);
    }
    if (gint_status & INT_RESUME) {
        writel(INT_RESUME, &AIC_UDC_REG->usbintsts);
    }
}

/*add usb plug/unplug check function*/
#if defined(KERNEL_RTTHREAD) && defined(LPKG_CHERRYUSB_DEVICE_PLUG_CHECK)
void usb_plug_irq(void *args)
{
    unsigned int ret;
    int pin = *((int *)(args));

    hal_gpio_get_value(GPIO_GROUP(pin), GPIO_GROUP_PIN(pin), &ret);
    if (ret) {
        usbd_event_connect_handler(0);
    } else {
        usbd_event_disconnect_handler(0);
    }
}
int usbd_plug_irq_init(void)
{
    static rt_base_t pin = 0;
    unsigned int g, p;

    pin = rt_pin_get(LPKG_PLUG_CHECK_GPIO);
    if (pin < 0) {
        USB_LOG_ERR("get id gpio err.\n");
        return -1;
    }

    g = GPIO_GROUP(pin);
    p = GPIO_GROUP_PIN(pin);
    hal_gpio_set_debounce(g, p, 0xFFF);
    rt_pin_mode(pin, PIN_MODE_INPUT);

    rt_pin_attach_irq(pin, PIN_IRQ_MODE_RISING_FALLING, usb_plug_irq, &pin);
    rt_pin_irq_enable(pin, 1);
    return 0;
}
#endif

#ifdef RT_USING_PM
static int aic_usbd_suspend(const struct rt_device *device, rt_uint8_t mode)
{
    switch (mode) {
    case PM_SLEEP_MODE_IDLE:
        break;
    case PM_SLEEP_MODE_LIGHT:
    case PM_SLEEP_MODE_DEEP:
    case PM_SLEEP_MODE_STANDBY:
    case PM_SLEEP_MODE_SHUTDOWN:
        usb_dc_deinit(0);
        break;
    default:
        break;
    }

    return 0;
}

static void aic_usbd_resume(const struct rt_device *device, rt_uint8_t mode)
{
    switch (mode) {
    case PM_SLEEP_MODE_IDLE:
        break;
    case PM_SLEEP_MODE_LIGHT:
    case PM_SLEEP_MODE_DEEP:
    case PM_SLEEP_MODE_STANDBY:
    case PM_SLEEP_MODE_SHUTDOWN:
        usb_dc_init(0);
        break;
    default:
        break;
    }
}

static struct rt_device_pm_ops aic_usbd_pm_ops =
{
    SET_DEVICE_PM_OPS(aic_usbd_suspend, aic_usbd_resume)
    NULL,
};

struct rt_device *usbd_device = RT_NULL;

static int usbd_pm_init(void)
{
    if (usbd_device)
        return 0;

    usbd_device = (struct rt_device *)rt_malloc(sizeof(struct rt_device));
    if (usbd_device) {
        rt_memset(usbd_device, 0, sizeof(struct rt_device));
        usbd_device->user_data = &g_aic_udc;
        if (rt_device_register(usbd_device, "usbd", RT_DEVICE_FLAG_RDWR) != RT_EOK) {
            USB_LOG_WRN("Failed to register usbd device\n");
            rt_free(usbd_device);
            usbd_device = RT_NULL;
            return -1;
        } else {
            rt_pm_device_register(usbd_device, &aic_usbd_pm_ops);
        }
    }

    return 0;
}

#endif
