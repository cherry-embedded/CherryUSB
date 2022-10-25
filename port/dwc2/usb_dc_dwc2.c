/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usb_dwc2_reg.h"

// clang-format off
#if   defined ( __CC_ARM )
#ifndef   __UNALIGNED_UINT32_WRITE
  #define __UNALIGNED_UINT32_WRITE(addr, val)    ((*((__packed uint32_t *)(addr))) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #define __UNALIGNED_UINT32_READ(addr)          (*((const __packed uint32_t *)(addr)))
#endif
#elif defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#ifndef   __UNALIGNED_UINT32_WRITE
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
/*lint -esym(9058, T_UINT32_WRITE)*/ /* disable MISRA 2012 Rule 2.4 for T_UINT32_WRITE */
  __PACKED_STRUCT T_UINT32_WRITE { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32_WRITE(addr, val)    (void)((((struct T_UINT32_WRITE *)(void *)(addr))->v) = (val))
#endif
#ifndef   __UNALIGNED_UINT32_READ
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wpacked"
/*lint -esym(9058, T_UINT32_READ)*/ /* disable MISRA 2012 Rule 2.4 for T_UINT32_READ */
  __PACKED_STRUCT T_UINT32_READ { uint32_t v; };
  #pragma clang diagnostic pop
  #define __UNALIGNED_UINT32_READ(addr)          (((const struct T_UINT32_READ *)(const void *)(addr))->v)
#endif
#elif defined ( __GNUC__ )
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
#endif
// clang-format on

#define FS_PORT 0
#define HS_PORT 1

#ifndef CONFIG_USB_DWC2_PORT
#error "please select CONFIG_USB_DWC2_PORT with FS_PORT or HS_PORT"
#endif

#if CONFIG_USB_DWC2_PORT == FS_PORT
#ifndef USBD_IRQHandler
#define USBD_IRQHandler OTG_FS_IRQHandler
#endif

#ifndef USB_BASE
#ifdef STM32H7
#define USB_BASE (0x40080000UL)
#else
#define USB_BASE (0x50000000UL)
#endif
#endif

#define USB_RAM_SIZE 1280 /* define with minimum value*/

/*FIFO sizes in bytes (total available memory for FIFOs is 1.25KB )*/
#ifndef CONFIG_USB_DWC2_RX_FIFO_SIZE
#define CONFIG_USB_DWC2_RX_FIFO_SIZE (512)
#endif

#ifndef CONFIG_USB_DWC2_TX0_FIFO_SIZE
#define CONFIG_USB_DWC2_TX0_FIFO_SIZE (64)
#endif

#ifndef CONFIG_USB_DWC2_TX1_FIFO_SIZE
#define CONFIG_USB_DWC2_TX1_FIFO_SIZE (128)
#endif

#ifndef CONFIG_USB_DWC2_TX2_FIFO_SIZE
#define CONFIG_USB_DWC2_TX2_FIFO_SIZE (128)
#endif

#ifndef CONFIG_USB_DWC2_TX3_FIFO_SIZE
#define CONFIG_USB_DWC2_TX3_FIFO_SIZE (128)
#endif

#ifndef CONFIG_USB_DWC2_TX4_FIFO_SIZE
#define CONFIG_USB_DWC2_TX4_FIFO_SIZE (128)
#endif

#ifndef CONFIG_USB_DWC2_TX5_FIFO_SIZE
#define CONFIG_USB_DWC2_TX5_FIFO_SIZE (128)
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 4 /* define with minimum value*/
#endif

#else

#ifndef USBD_IRQHandler
#define USBD_IRQHandler OTG_HS_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE (0x40040000UL)
#endif

#define USB_RAM_SIZE 4096 /* define with minimum value*/

#define CONFIG_USB_DWC2_DMA_ENABLE

#ifdef CONFIG_USB_DWC2_DMA_ENABLE
#if defined(STM32F7) || defined(STM32H7)
#warning "if you enable dcache,please add .nocacheble section in your sct or ld or icf"
#endif
#endif

/*FIFO sizes in bytes (total available memory for FIFOs is 4KB )*/
#ifndef CONFIG_USB_DWC2_RX_FIFO_SIZE
#define CONFIG_USB_DWC2_RX_FIFO_SIZE (1024)
#endif

#ifndef CONFIG_USB_DWC2_TX0_FIFO_SIZE
#define CONFIG_USB_DWC2_TX0_FIFO_SIZE (256)
#endif

#ifndef CONFIG_USB_DWC2_TX1_FIFO_SIZE
#define CONFIG_USB_DWC2_TX1_FIFO_SIZE (1024)
#endif

#ifndef CONFIG_USB_DWC2_TX2_FIFO_SIZE
#define CONFIG_USB_DWC2_TX2_FIFO_SIZE (512)
#endif

#ifndef CONFIG_USB_DWC2_TX3_FIFO_SIZE
#define CONFIG_USB_DWC2_TX3_FIFO_SIZE (512)
#endif

#ifndef CONFIG_USB_DWC2_TX4_FIFO_SIZE
#define CONFIG_USB_DWC2_TX4_FIFO_SIZE (512)
#endif

#ifndef CONFIG_USB_DWC2_TX5_FIFO_SIZE
#define CONFIG_USB_DWC2_TX5_FIFO_SIZE (256)
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 6 /* define with minimum value*/
#endif

#endif

#define USB_OTG_GLB      ((USB_OTG_GlobalTypeDef *)(USB_BASE))
#define USB_OTG_DEV      ((USB_OTG_DeviceTypeDef *)(USB_BASE + USB_OTG_DEVICE_BASE))
#define USB_OTG_PCGCCTL  *(__IO uint32_t *)((uint32_t)USB_BASE + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_INEP(i)  ((USB_OTG_INEndpointTypeDef *)(USB_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i)*USB_OTG_EP_REG_SIZE)))
#define USB_OTG_OUTEP(i) ((USB_OTG_OUTEndpointTypeDef *)(USB_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i)*USB_OTG_EP_REG_SIZE)))
#define USB_OTG_FIFO(i)  *(__IO uint32_t *)(USB_BASE + USB_OTG_FIFO_BASE + ((i)*USB_OTG_FIFO_SIZE))

extern uint32_t SystemCoreClock;

/* Endpoint state */
struct dwc2_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t ep_busy;    /* Endpoint busy */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct dwc2_udc {
    __attribute__((aligned(32))) struct usb_setup_packet setup;
    struct dwc2_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct dwc2_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} g_dwc2_udc;

static inline int dwc2_reset(void)
{
    uint32_t count = 0U;

    /* Wait for AHB master IDLE state. */
    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);

    /* Core Soft Reset */
    count = 0U;
    USB_OTG_GLB->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_CSRST) == USB_OTG_GRSTCTL_CSRST);

    return 0;
}

static inline int dwc2_core_init(void)
{
    int ret;
#if defined(CONFIG_USB_HS)
    USB_OTG_GLB->GCCFG &= ~(USB_OTG_GCCFG_PWRDWN);

    /* Init The ULPI Interface */
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_TSDPS | USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_PHYSEL);

    /* Select vbus source */
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);

    /* Reset after a PHY select */
    ret = dwc2_reset();
#else
    /* Select FS Embedded PHY */
    USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;

    /* Reset after a PHY select */
    ret = dwc2_reset();
    /* Activate the USB Transceiver */
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_PWRDWN;
#endif
    return ret;
}

static inline void dwc2_set_mode(uint8_t mode)
{
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_FHMOD | USB_OTG_GUSBCFG_FDMOD);

    if (mode == USB_OTG_MODE_HOST) {
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
    } else if (mode == USB_OTG_MODE_DEVICE) {
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;
    }
}

static inline int dwc2_flush_rxfifo(void)
{
    uint32_t count = 0;

    USB_OTG_GLB->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH);

    return 0;
}

static inline int dwc2_flush_txfifo(uint32_t num)
{
    uint32_t count = 0U;

    USB_OTG_GLB->GRSTCTL = (USB_OTG_GRSTCTL_TXFFLSH | (num << 6));

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);

    return 0;
}

static void dwc2_set_turnaroundtime(uint32_t hclk, uint8_t speed)
{
    uint32_t UsbTrd;

    /* The USBTRD is configured according to the tables below, depending on AHB frequency
  used by application. In the low AHB frequency range it is used to stretch enough the USB response
  time to IN tokens, the USB turnaround time, so to compensate for the longer AHB read access
  latency to the Data FIFO */
    if (speed == USB_OTG_SPEED_FULL) {
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
        } else /* if(hclk >= 32000000) */
        {
            /* hclk Clock Range between 32-200 MHz */
            UsbTrd = 0x6U;
        }
    } else if (speed == USB_OTG_SPEED_HIGH) {
        UsbTrd = USBD_HS_TRDT_VALUE;
    } else {
        UsbTrd = USBD_DEFAULT_TRDT_VALUE;
    }

    USB_OTG_GLB->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
    USB_OTG_GLB->GUSBCFG |= (uint32_t)((UsbTrd << 10) & USB_OTG_GUSBCFG_TRDT);
}

static void dwc2_set_txfifo(uint8_t fifo, uint16_t size)
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

    Tx_Offset = USB_OTG_GLB->GRXFSIZ;

    if (fifo == 0U) {
        USB_OTG_GLB->DIEPTXF0_HNPTXFSIZ = ((uint32_t)size << 16) | Tx_Offset;
    } else {
        Tx_Offset += (USB_OTG_GLB->DIEPTXF0_HNPTXFSIZ) >> 16;
        for (i = 0U; i < (fifo - 1U); i++) {
            Tx_Offset += (USB_OTG_GLB->DIEPTXF[i] >> 16);
        }

        /* Multiply Tx_Size by 2 to get higher performance */
        USB_OTG_GLB->DIEPTXF[fifo - 1U] = ((uint32_t)size << 16) | Tx_Offset;
    }
}

static uint8_t dwc2_get_devspeed(void)
{
    uint8_t speed;
    uint32_t DevEnumSpeed = USB_OTG_DEV->DSTS & USB_OTG_DSTS_ENUMSPD;

    if (DevEnumSpeed == DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ) {
        speed = USB_OTG_SPEED_HIGH;
    } else if ((DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ) ||
               (DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_48MHZ)) {
        speed = USB_OTG_SPEED_FULL;
    } else {
        speed = 0xFU;
    }

    return speed;
}

static void dwc2_ep0_start_read_setup(uint8_t *psetup)
{
    USB_OTG_OUTEP(0U)->DOEPTSIZ = 0U;
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= (3U * 8U);
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= USB_OTG_DOEPTSIZ_STUPCNT;

#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    USB_OTG_OUTEP(0U)->DOEPDMA = (uint32_t)psetup;
    /* EP enable */
    USB_OTG_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_USBAEP;
#endif
}

void dwc2_ep_write(uint8_t ep_idx, uint8_t *src, uint16_t len)
{
    uint32_t *pSrc = (uint32_t *)src;
    uint32_t count32b, i;

    count32b = ((uint32_t)len + 3U) / 4U;
    for (i = 0U; i < count32b; i++) {
        USB_OTG_FIFO((uint32_t)ep_idx) = __UNALIGNED_UINT32_READ(pSrc);
        pSrc++;
    }
}

void dwc2_ep_read(uint8_t *dest, uint16_t len)
{
    uint32_t *pDest = (uint32_t *)dest;
    uint32_t i;
    uint32_t count32b = ((uint32_t)len + 3U) / 4U;

    for (i = 0U; i < count32b; i++) {
        __UNALIGNED_UINT32_WRITE(pDest, USB_OTG_FIFO(0U));
        pDest++;
    }
}

static void dwc2_tx_fifo_empty_procecss(uint8_t ep_idx)
{
    uint32_t len;
    uint32_t len32b;
    uint32_t fifoemptymsk;

    len = g_dwc2_udc.in_ep[ep_idx].xfer_len - g_dwc2_udc.in_ep[ep_idx].actual_xfer_len;
    if (len > g_dwc2_udc.in_ep[ep_idx].ep_mps) {
        len = g_dwc2_udc.in_ep[ep_idx].ep_mps;
    }

    len32b = (len + 3U) / 4U;

    while (((USB_OTG_INEP(ep_idx)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= len32b) &&
           (g_dwc2_udc.in_ep[ep_idx].actual_xfer_len < g_dwc2_udc.in_ep[ep_idx].xfer_len) && (g_dwc2_udc.in_ep[ep_idx].xfer_len != 0U)) {
        /* Write the FIFO */
        len = g_dwc2_udc.in_ep[ep_idx].xfer_len - g_dwc2_udc.in_ep[ep_idx].actual_xfer_len;
        if (len > g_dwc2_udc.in_ep[ep_idx].ep_mps) {
            len = g_dwc2_udc.in_ep[ep_idx].ep_mps;
        }

        dwc2_ep_write(ep_idx, g_dwc2_udc.in_ep[ep_idx].xfer_buf, len);
        g_dwc2_udc.in_ep[ep_idx].xfer_buf += len;
        g_dwc2_udc.in_ep[ep_idx].actual_xfer_len += len;
    }

    if (g_dwc2_udc.in_ep[ep_idx].xfer_len <= g_dwc2_udc.in_ep[ep_idx].actual_xfer_len) {
        fifoemptymsk = (uint32_t)(0x1UL << (ep_idx & 0x0f));
        USB_OTG_DEV->DIEPEMPMSK &= ~fifoemptymsk;
    }
}

/**
  * @brief  dwc2_get_glb_intstatus: return the global USB interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_glb_intstatus(void)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_GLB->GINTSTS;
    tmpreg &= USB_OTG_GLB->GINTMSK;

    return tmpreg;
}

/**
  * @brief  dwc2_get_outeps_intstatus: return the USB device OUT endpoints interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_outeps_intstatus(void)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_DEV->DAINT;
    tmpreg &= USB_OTG_DEV->DAINTMSK;

    return ((tmpreg & 0xffff0000U) >> 16);
}

/**
  * @brief  dwc2_get_ineps_intstatus: return the USB device IN endpoints interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_ineps_intstatus(void)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_DEV->DAINT;
    tmpreg &= USB_OTG_DEV->DAINTMSK;

    return ((tmpreg & 0xFFFFU));
}

/**
  * @brief  Returns Device OUT EP Interrupt register
  * @param  epnum  endpoint number
  *          This parameter can be a value from 0 to 15
  * @retval Device OUT EP Interrupt register
  */
static inline uint32_t dwc2_get_outep_intstatus(uint8_t epnum)
{
    uint32_t tmpreg;

    tmpreg = USB_OTG_OUTEP((uint32_t)epnum)->DOEPINT;
    tmpreg &= USB_OTG_DEV->DOEPMSK;

    return tmpreg;
}

/**
  * @brief  Returns Device IN EP Interrupt register
  * @param  epnum  endpoint number
  *          This parameter can be a value from 0 to 15
  * @retval Device IN EP Interrupt register
  */
static inline uint32_t dwc2_get_inep_intstatus(uint8_t epnum)
{
    uint32_t tmpreg, msk, emp;

    msk = USB_OTG_DEV->DIEPMSK;
    emp = USB_OTG_DEV->DIEPEMPMSK;
    msk |= ((emp >> (epnum & 0x07)) & 0x1U) << 7;
    tmpreg = USB_OTG_INEP((uint32_t)epnum)->DIEPINT & msk;

    return tmpreg;
}

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    int ret;

    memset(&g_dwc2_udc, 0, sizeof(struct dwc2_udc));

    usb_dc_low_level_init();

    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_SDIS;

    USB_OTG_GLB->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;

    ret = dwc2_core_init();

    /* Force Device Mode*/
    dwc2_set_mode(USB_OTG_MODE_DEVICE);

    for (uint8_t i = 0U; i < 15U; i++) {
        USB_OTG_GLB->DIEPTXF[i] = 0U;
    }

#if defined(STM32F7) || defined(STM32H7)
#ifdef CONFIG_DWC2_VBUS_SENSING_ENABLE
    /* Enable HW VBUS sensing */
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_VBDEN;
#else
    /* Deactivate VBUS Sensing B */
    USB_OTG_GLB->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    /* B-peripheral session valid override enable */
    USB_OTG_GLB->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USB_OTG_GLB->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#endif
#else
#ifdef CONFIG_DWC2_VBUS_SENSING_ENABLE
    /* Enable HW VBUS sensing */
    USB_OTG_GLB->GCCFG &= ~USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_VBUSBSEN;
#else
    /*
     * Disable HW VBUS sensing. VBUS is internally considered to be always
     * at VBUS-Valid level (5V).
     */
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG_GLB->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USB_OTG_GLB->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
#endif
#endif
    /* Restart the Phy Clock */
    USB_OTG_PCGCCTL = 0U;

    /* Device mode configuration */
    USB_OTG_DEV->DCFG |= DCFG_FRAME_INTERVAL_80;

#if CONFIG_USB_DWC2_PORT == HS_PORT
#if defined(CONFIG_USB_HS)
    /* Set Core speed to High speed mode */
    USB_OTG_DEV->DCFG |= USB_OTG_SPEED_HIGH;
#else

    USB_OTG_DEV->DCFG |= USB_OTG_SPEED_HIGH_IN_FULL;
#endif
#else
    USB_OTG_DEV->DCFG |= USB_OTG_SPEED_FULL;
#endif

    ret = dwc2_flush_txfifo(0x10U);
    ret = dwc2_flush_rxfifo();

    /* Clear all pending Device Interrupts */
    USB_OTG_DEV->DIEPMSK = 0U;
    USB_OTG_DEV->DOEPMSK = 0U;
    USB_OTG_DEV->DAINTMSK = 0U;

    /* Disable all interrupts. */
    USB_OTG_GLB->GINTMSK = 0U;
    /* Clear any pending interrupts */
    USB_OTG_GLB->GINTSTS = 0xBFFFFFFFU;

    /* Enable interrupts matching to the Device mode ONLY */
    USB_OTG_GLB->GINTMSK = USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |
                           USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_IEPINT;
#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_HBSTLEN_2;
    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_DMAEN;
#else
    USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
#endif
#if CONFIG_DWC2_VBUS_SENSING
    USB_OTG_GLB->GINTMSK |= (USB_OTG_GINTMSK_OTGINT | USB_OTG_GINTMSK_SRQIM);
#endif
#if 0
    USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_SOFM;
#endif

    USB_OTG_GLB->GRXFSIZ = (CONFIG_USB_DWC2_RX_FIFO_SIZE / 4);

    dwc2_set_txfifo(0, CONFIG_USB_DWC2_TX0_FIFO_SIZE / 4);
    dwc2_set_txfifo(1, CONFIG_USB_DWC2_TX1_FIFO_SIZE / 4);
    dwc2_set_txfifo(2, CONFIG_USB_DWC2_TX2_FIFO_SIZE / 4);
    dwc2_set_txfifo(3, CONFIG_USB_DWC2_TX3_FIFO_SIZE / 4);
#if USB_NUM_BIDIR_ENDPOINTS > 4
    dwc2_set_txfifo(4, CONFIG_USB_DWC2_TX4_FIFO_SIZE / 4);
#endif
#if USB_NUM_BIDIR_ENDPOINTS > 5
    dwc2_set_txfifo(5, CONFIG_USB_DWC2_TX5_FIFO_SIZE / 4);
#endif
    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
    USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;

    return ret;
}

int usb_dc_deinit(void)
{
    usb_dc_low_level_deinit();
    /* Clear Pending interrupt */
    for (uint8_t i = 0U; i < 15U; i++) {
        USB_OTG_INEP(i)->DIEPINT = 0xFB7FU;
        USB_OTG_OUTEP(i)->DOEPINT = 0xFB7FU;
    }

    /* Clear interrupt masks */
    USB_OTG_DEV->DIEPMSK = 0U;
    USB_OTG_DEV->DOEPMSK = 0U;
    USB_OTG_DEV->DAINTMSK = 0U;

    /* Flush the FIFO */
    dwc2_flush_txfifo(0x10U);
    dwc2_flush_rxfifo();

    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_SDIS;

    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    USB_OTG_DEV->DCFG &= ~(USB_OTG_DCFG_DAD);
    USB_OTG_DEV->DCFG |= ((uint32_t)addr << 4) & USB_OTG_DCFG_DAD;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);
    uint16_t ep_mps;

    if (ep_idx > (USB_NUM_BIDIR_ENDPOINTS - 1)) {
        USB_LOG_ERR("Ep addr %d overflow\r\n", ep_cfg->ep_addr);
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        g_dwc2_udc.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_dwc2_udc.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        g_dwc2_udc.out_ep[ep_idx].ep_enable = true;

        USB_OTG_DEV->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & (uint32_t)(1UL << (16 + ep_idx));

        ep_mps = ep_cfg->ep_mps;
        if (ep_idx == 0) {
            switch (ep_cfg->ep_mps) {
                case 8:
                    ep_mps = EP_MPS_8;
                    break;
                case 16:
                    ep_mps = EP_MPS_16;
                    break;
                case 32:
                    ep_mps = EP_MPS_32;
                    break;
                case 64:
                    ep_mps = EP_MPS_64;
                    break;
            }
        }
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (ep_mps & USB_OTG_DOEPCTL_MPSIZ) |
                                          ((uint32_t)ep_cfg->ep_type << 18) |
                                          USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                          USB_OTG_DOEPCTL_USBAEP;
    } else {
        g_dwc2_udc.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_dwc2_udc.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        g_dwc2_udc.in_ep[ep_idx].ep_enable = true;

        USB_OTG_DEV->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << ep_idx);

        USB_OTG_INEP(ep_idx)->DIEPCTL |= (ep_cfg->ep_mps & USB_OTG_DIEPCTL_MPSIZ) |
                                         ((uint32_t)ep_cfg->ep_type << 18) | (ep_idx << 22) |
                                         USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                         USB_OTG_DIEPCTL_USBAEP;
    }
    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_EPDIS;

        USB_OTG_DEV->DEACHMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USB_OTG_DEV->DAINTMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~(USB_OTG_DOEPCTL_USBAEP |
                                            USB_OTG_DOEPCTL_MPSIZ |
                                            USB_OTG_DOEPCTL_SD0PID_SEVNFRM |
                                            USB_OTG_DOEPCTL_EPTYP);
    } else {
        USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
        USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;

        USB_OTG_DEV->DEACHMSK &= ~(USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (ep_idx & 0x07)));
        USB_OTG_DEV->DAINTMSK &= ~(USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (ep_idx & 0x07)));
        USB_OTG_INEP(ep_idx)->DIEPCTL &= ~(USB_OTG_DIEPCTL_USBAEP |
                                           USB_OTG_DIEPCTL_MPSIZ |
                                           USB_OTG_DIEPCTL_TXFNUM |
                                           USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                           USB_OTG_DIEPCTL_EPTYP);
    }
    return 0;
}

int usbd_ep_set_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (((USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~(USB_OTG_DOEPCTL_EPDIS);
        }
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
    } else {
        if (((USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            USB_OTG_INEP(ep_idx)->DIEPCTL &= ~(USB_OTG_DIEPCTL_EPDIS);
        }
        USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
    }
#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    if (ep_idx == 0) {
        dwc2_ep0_start_read_setup((uint8_t *)&g_dwc2_udc.setup);
    }
#endif
    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
        if ((g_dwc2_udc.out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (g_dwc2_udc.out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
    } else {
        USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
        if ((g_dwc2_udc.in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (g_dwc2_udc.in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
    }
    return 0;
}

int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
    } else {
    }
    return 0;
}

int usbd_ep_start_write(const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;

    if (!data && data_len) {
        return -1;
    }
    if (!g_dwc2_udc.in_ep[ep_idx].ep_enable) {
        return -2;
    }
#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    if ((uint32_t)data & 0x03) {
        return -3;
    }
#endif

    g_dwc2_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_dwc2_udc.in_ep[ep_idx].xfer_len = data_len;
    g_dwc2_udc.in_ep[ep_idx].actual_xfer_len = 0;

    USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
    USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);

    if (data_len == 0) {
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > g_dwc2_udc.in_ep[ep_idx].ep_mps) {
            data_len = g_dwc2_udc.in_ep[ep_idx].ep_mps;
        }
        g_dwc2_udc.in_ep[ep_idx].xfer_len = data_len;
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + g_dwc2_udc.in_ep[ep_idx].ep_mps - 1U) / g_dwc2_udc.in_ep[ep_idx].ep_mps);

        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (pktcnt << 19));
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    }

#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    USB_OTG_INEP(ep_idx)->DIEPDMA = (uint32_t)data;

    if (g_dwc2_udc.in_ep[ep_idx].ep_type == 0x01) {
        if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
        } else {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
        USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_MULCNT);
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_MULCNT & (1U << 29));
    }
    USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
#else
    USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
    /* Enable the Tx FIFO Empty Interrupt for this EP */
    if (data_len > 0U) {
        USB_OTG_DEV->DIEPEMPMSK |= 1UL << (ep_idx & 0x0f);
    }
#endif
    return 0;
}

int usbd_ep_start_read(const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;

    if (!data && data_len) {
        return -1;
    }
    if (!g_dwc2_udc.out_ep[ep_idx].ep_enable) {
        return -2;
    }
#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    if (((uint32_t)data) & 0x03) {
        return -3;
    }
#endif

    g_dwc2_udc.out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_dwc2_udc.out_ep[ep_idx].xfer_len = data_len;
    g_dwc2_udc.out_ep[ep_idx].actual_xfer_len = 0;

    USB_OTG_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);
    USB_OTG_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
    if (data_len == 0) {
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1 << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & g_dwc2_udc.out_ep[ep_idx].ep_mps);
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > g_dwc2_udc.out_ep[ep_idx].ep_mps) {
            data_len = g_dwc2_udc.out_ep[ep_idx].ep_mps;
        }
        g_dwc2_udc.in_ep[ep_idx].xfer_len = data_len;
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + g_dwc2_udc.out_ep[ep_idx].ep_mps - 1U) / g_dwc2_udc.out_ep[ep_idx].ep_mps);

        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (pktcnt << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & data_len);
    }

#ifdef CONFIG_USB_DWC2_DMA_ENABLE
    USB_OTG_OUTEP(ep_idx)->DOEPDMA = (uint32_t)data;

    if (g_dwc2_udc.out_ep[ep_idx].ep_type == 0x01) {
        if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
        } else {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
    }
#endif

    USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
    return 0;
}

void USBD_IRQHandler(void)
{
    uint32_t gint_status, temp, ep_idx, ep_intr, epint, read_count;
    gint_status = dwc2_get_glb_intstatus();

    if ((USB_OTG_GLB->GINTSTS & 0x1U) == USB_OTG_MODE_DEVICE) {
        /* Avoid spurious interrupt */
        if (gint_status == 0) {
            return;
        }

#ifndef CONFIG_USB_DWC2_DMA_ENABLE
        /* Handle RxQLevel Interrupt */
        if (gint_status & USB_OTG_GINTSTS_RXFLVL) {
            USB_MASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);

            temp = USB_OTG_GLB->GRXSTSP;
            ep_idx = temp & USB_OTG_GRXSTSP_EPNUM;

            if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_DATA_UPDT) {
                read_count = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                if (read_count != 0) {
                    dwc2_ep_read(g_dwc2_udc.out_ep[ep_idx].xfer_buf, read_count);
                    g_dwc2_udc.out_ep[ep_idx].xfer_buf += read_count;
                }
            } else if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_SETUP_UPDT) {
                read_count = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                dwc2_ep_read((uint8_t *)&g_dwc2_udc.setup, read_count);
            } else {
                /* ... */
            }
            USB_UNMASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);
        }
#endif
        if (gint_status & USB_OTG_GINTSTS_OEPINT) {
            ep_idx = 0;
            ep_intr = dwc2_get_outeps_intstatus();
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = dwc2_get_outep_intstatus(ep_idx);
                    uint32_t DoepintReg = USB_OTG_OUTEP(ep_idx)->DOEPINT;
                    USB_OTG_OUTEP(ep_idx)->DOEPINT = DoepintReg;

                    if ((epint & USB_OTG_DOEPINT_XFRC) == USB_OTG_DOEPINT_XFRC) {
                        if (ep_idx == 0) {
                            if (g_dwc2_udc.out_ep[ep_idx].xfer_len == 0) {
                                /* Out status, start reading setup */
                                dwc2_ep0_start_read_setup((uint8_t *)&g_dwc2_udc.setup);
                            } else {
                                g_dwc2_udc.out_ep[ep_idx].actual_xfer_len = g_dwc2_udc.out_ep[ep_idx].xfer_len - ((USB_OTG_OUTEP(ep_idx)->DOEPTSIZ) & USB_OTG_DOEPTSIZ_XFRSIZ);
                                g_dwc2_udc.out_ep[ep_idx].xfer_len = 0;
                                usbd_event_ep_out_complete_handler(0x00, g_dwc2_udc.out_ep[ep_idx].actual_xfer_len);
                            }
                        } else {
                            g_dwc2_udc.out_ep[ep_idx].actual_xfer_len = g_dwc2_udc.out_ep[ep_idx].xfer_len - ((USB_OTG_OUTEP(ep_idx)->DOEPTSIZ) & USB_OTG_DOEPTSIZ_XFRSIZ);
                            g_dwc2_udc.out_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_out_complete_handler(ep_idx, g_dwc2_udc.out_ep[ep_idx].actual_xfer_len);
                        }
                    }

                    if ((epint & USB_OTG_DOEPINT_STUP) == USB_OTG_DOEPINT_STUP) {
                        usbd_event_ep0_setup_complete_handler((uint8_t *)&g_dwc2_udc.setup);
                    }
                }
                ep_intr >>= 1U;
                ep_idx++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_IEPINT) {
            ep_idx = 0U;
            ep_intr = dwc2_get_ineps_intstatus();
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = dwc2_get_inep_intstatus(ep_idx);
                    uint32_t DiepintReg = USB_OTG_INEP(ep_idx)->DIEPINT;
                    USB_OTG_INEP(ep_idx)->DIEPINT = DiepintReg;

                    if ((epint & USB_OTG_DIEPINT_XFRC) == USB_OTG_DIEPINT_XFRC) {
                        if (ep_idx == 0) {
                            g_dwc2_udc.in_ep[ep_idx].actual_xfer_len = g_dwc2_udc.in_ep[ep_idx].xfer_len - ((USB_OTG_INEP(ep_idx)->DIEPTSIZ) & USB_OTG_DIEPTSIZ_XFRSIZ);
                            g_dwc2_udc.in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(0x80, g_dwc2_udc.in_ep[ep_idx].actual_xfer_len);

                            if (g_dwc2_udc.setup.wLength && ((g_dwc2_udc.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                                /* In status, start reading setup */
                                dwc2_ep0_start_read_setup((uint8_t *)&g_dwc2_udc.setup);
                            } else if (g_dwc2_udc.setup.wLength == 0) {
                                /* In status, start reading setup */
                                dwc2_ep0_start_read_setup((uint8_t *)&g_dwc2_udc.setup);
                            }
                        } else {
                            g_dwc2_udc.in_ep[ep_idx].actual_xfer_len = g_dwc2_udc.in_ep[ep_idx].xfer_len - ((USB_OTG_INEP(ep_idx)->DIEPTSIZ) & USB_OTG_DIEPTSIZ_XFRSIZ);
                            g_dwc2_udc.in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(ep_idx | 0x80, g_dwc2_udc.in_ep[ep_idx].actual_xfer_len);
                        }
                    }
                    if ((epint & USB_OTG_DIEPINT_TXFE) == USB_OTG_DIEPINT_TXFE) {
                        dwc2_tx_fifo_empty_procecss(ep_idx);
                    }
                }
                ep_intr >>= 1U;
                ep_idx++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_USBRST) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_USBRST;
            USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_RWUSIG;

            dwc2_flush_txfifo(0x10U);
            dwc2_flush_rxfifo();

            for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
                if (i == 0U) {
                    USB_OTG_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_SNAK;
                    USB_OTG_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_SNAK;
                } else {
                    USB_OTG_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_SNAK;
                    USB_OTG_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_SNAK;
                }
                USB_OTG_INEP(i)->DIEPTSIZ = 0U;
                USB_OTG_INEP(i)->DIEPINT = 0xFB7FU;
                USB_OTG_OUTEP(i)->DOEPTSIZ = 0U;
                USB_OTG_OUTEP(i)->DOEPINT = 0xFB7FU;
            }

            USB_OTG_DEV->DAINTMSK |= 0x10001U;

            USB_OTG_DEV->DOEPMSK = USB_OTG_DOEPMSK_STUPM |
                                   USB_OTG_DOEPMSK_XFRCM;

            USB_OTG_DEV->DIEPMSK = USB_OTG_DIEPMSK_XFRCM;

            memset(&g_dwc2_udc, 0, sizeof(struct dwc2_udc));
            usbd_event_reset_handler();
            /* Start reading setup */
            dwc2_ep0_start_read_setup((uint8_t *)&g_dwc2_udc.setup);
        }
        if (gint_status & USB_OTG_GINTSTS_ENUMDNE) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_ENUMDNE;
            dwc2_set_turnaroundtime(SystemCoreClock, dwc2_get_devspeed());

            USB_OTG_DEV->DCTL |= USB_OTG_DCTL_CGINAK;
        }
        if (gint_status & USB_OTG_GINTSTS_SOF) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_SOF;
        }
        if (gint_status & USB_OTG_GINTSTS_USBSUSP) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_USBSUSP;
        }
        if (gint_status & USB_OTG_GINTSTS_WKUINT) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_WKUINT;
        }
        if (gint_status & USB_OTG_GINTSTS_OTGINT) {
            temp = USB_OTG_GLB->GOTGINT;
            if ((temp & USB_OTG_GOTGINT_SEDET) == USB_OTG_GOTGINT_SEDET) {
            } else {
            }
            USB_OTG_GLB->GOTGINT |= temp;
        }
    }
}
