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

#ifndef CONFIG_DWC2_PORTS
#define CONFIG_DWC2_PORTS 2
#endif

#ifndef CONFIG_DWC2_BIDIR_ENDPOINTS
#define CONFIG_DWC2_BIDIR_ENDPOINTS 6
#endif

/*
    rx fifo:512 byte
    tx0 fifo:64 byte
    tx1 fifo:128 byte
    tx2 fifo:128 byte
    tx3 fifo:128 byte
    tx4 fifo:128 byte
    tx5 fifo:128 byte

    fifo max :1280 byte
*/
const uint16_t dwc2_fs_fifo_config[] = {
    512, 64, 128, 128, 128, 128, 128
};

/*
    rx fifo:1024 byte
    tx0 fifo:128 byte
    tx1 fifo:1024 byte
    tx2 fifo:512 byte
    tx3 fifo:512 byte
    tx4 fifo:512 byte
    tx5 fifo:256 byte

    fifo max :4096 byte
*/
const uint16_t dwc2_hs_fifo_config[] = {
    1024, 128, 1024, 512, 512, 512, 256
};

#define USB_OTG_GLB      ((USB_OTG_GlobalTypeDef *)(bus->reg_base))
#define USB_OTG_DEV      ((USB_OTG_DeviceTypeDef *)(bus->reg_base + USB_OTG_DEVICE_BASE))
#define USB_OTG_PCGCCTL  *(__IO uint32_t *)((uint32_t)bus->reg_base + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_INEP(i)  ((USB_OTG_INEndpointTypeDef *)(bus->reg_base + USB_OTG_IN_ENDPOINT_BASE + ((i)*USB_OTG_EP_REG_SIZE)))
#define USB_OTG_OUTEP(i) ((USB_OTG_OUTEndpointTypeDef *)(bus->reg_base + USB_OTG_OUT_ENDPOINT_BASE + ((i)*USB_OTG_EP_REG_SIZE)))
#define USB_OTG_FIFO(i)  *(__IO uint32_t *)(bus->reg_base + USB_OTG_FIFO_BASE + ((i)*USB_OTG_FIFO_SIZE))

extern uint32_t SystemCoreClock;

struct dwc2_ep {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

USB_NOCACHE_RAM_SECTION struct dwc2_udc {
    __attribute__((aligned(32))) struct usb_setup_packet setup;
    struct dwc2_ep in_ep[CONFIG_DWC2_BIDIR_ENDPOINTS];
    struct dwc2_ep out_ep[CONFIG_DWC2_BIDIR_ENDPOINTS];
    bool inuse;
    bool dma_enable;
} g_dwc2_udc[CONFIG_DWC2_PORTS];

struct dwc2_udc *dwc2_alloc_udc(void)
{
    for (uint8_t i = 0; i < 2; i++) {
        if (g_dwc2_udc[i].inuse == false) {
            g_dwc2_udc[i].inuse = true;
            return &g_dwc2_udc[i];
        }
    }
    return NULL;
}

void dwc2_free_udc(struct dwc2_udc *udc)
{
    udc->inuse = false;
}

static inline int dwc2_reset(struct usbd_bus *bus)
{
    volatile uint32_t count = 0U;

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

static inline int dwc2_core_init(struct usbd_bus *bus)
{
    int ret;
#if defined(CONFIG_USB_HS)
    USB_OTG_GLB->GCCFG &= ~(USB_OTG_GCCFG_PWRDWN);

    /* Init The ULPI Interface */
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_TSDPS | USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_PHYSEL);

    /* Select vbus source */
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);

    /* Reset after a PHY select */
    ret = dwc2_reset(bus);
#else
    /* Select FS Embedded PHY */
    USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;

    /* Reset after a PHY select */
    ret = dwc2_reset(bus);
    /* Activate the USB Transceiver */
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_PWRDWN;
#endif
    return ret;
}

static inline void dwc2_set_mode(struct usbd_bus *bus, uint8_t mode)
{
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_FHMOD | USB_OTG_GUSBCFG_FDMOD);

    if (mode == USB_OTG_MODE_HOST) {
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
    } else if (mode == USB_OTG_MODE_DEVICE) {
        USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;
    }
}

static inline int dwc2_flush_rxfifo(struct usbd_bus *bus)
{
    volatile uint32_t count = 0;

    USB_OTG_GLB->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH);

    return 0;
}

static inline int dwc2_flush_txfifo(struct usbd_bus *bus, uint32_t num)
{
    volatile uint32_t count = 0U;

    USB_OTG_GLB->GRSTCTL = (USB_OTG_GRSTCTL_TXFFLSH | (num << 6));

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USB_OTG_GLB->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);

    return 0;
}

static void dwc2_set_turnaroundtime(struct usbd_bus *bus, uint32_t hclk, uint8_t speed)
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

static void dwc2_set_txfifo(struct usbd_bus *bus, uint8_t fifo, uint16_t size)
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

static uint8_t dwc2_get_devspeed(struct usbd_bus *bus)
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

static void dwc2_ep0_start_read_setup(struct usbd_bus *bus, uint8_t *psetup)
{
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    USB_OTG_OUTEP(0U)->DOEPTSIZ = 0U;
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= (3U * 8U);
    USB_OTG_OUTEP(0U)->DOEPTSIZ |= USB_OTG_DOEPTSIZ_STUPCNT;

    if (udc->dma_enable) {
        USB_OTG_OUTEP(0U)->DOEPDMA = (uint32_t)psetup;
        /* EP enable */
        USB_OTG_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_USBAEP;
    }
}

void dwc2_ep_write(struct usbd_bus *bus, uint8_t ep_idx, uint8_t *src, uint16_t len)
{
    uint32_t *pSrc = (uint32_t *)src;
    uint32_t count32b, i;

    count32b = ((uint32_t)len + 3U) / 4U;
    for (i = 0U; i < count32b; i++) {
        USB_OTG_FIFO((uint32_t)ep_idx) = __UNALIGNED_UINT32_READ(pSrc);
        pSrc++;
    }
}

void dwc2_ep_read(struct usbd_bus *bus, uint8_t *dest, uint16_t len)
{
    uint32_t *pDest = (uint32_t *)dest;
    uint32_t i;
    uint32_t count32b = ((uint32_t)len + 3U) / 4U;

    for (i = 0U; i < count32b; i++) {
        __UNALIGNED_UINT32_WRITE(pDest, USB_OTG_FIFO(0U));
        pDest++;
    }
}

static void dwc2_tx_fifo_empty_procecss(struct usbd_bus *bus, uint8_t ep_idx)
{
    uint32_t len;
    uint32_t len32b;
    uint32_t fifoemptymsk;
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    len = udc->in_ep[ep_idx].xfer_len - udc->in_ep[ep_idx].actual_xfer_len;
    if (len > udc->in_ep[ep_idx].ep_mps) {
        len = udc->in_ep[ep_idx].ep_mps;
    }

    len32b = (len + 3U) / 4U;

    while (((USB_OTG_INEP(ep_idx)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= len32b) &&
           (udc->in_ep[ep_idx].actual_xfer_len < udc->in_ep[ep_idx].xfer_len) && (udc->in_ep[ep_idx].xfer_len != 0U)) {
        /* Write the FIFO */
        len = udc->in_ep[ep_idx].xfer_len - udc->in_ep[ep_idx].actual_xfer_len;
        if (len > udc->in_ep[ep_idx].ep_mps) {
            len = udc->in_ep[ep_idx].ep_mps;
        }

        dwc2_ep_write(bus, ep_idx, udc->in_ep[ep_idx].xfer_buf, len);
        udc->in_ep[ep_idx].xfer_buf += len;
        udc->in_ep[ep_idx].actual_xfer_len += len;
    }

    if (udc->in_ep[ep_idx].xfer_len <= udc->in_ep[ep_idx].actual_xfer_len) {
        fifoemptymsk = (uint32_t)(0x1UL << (ep_idx & 0x0f));
        USB_OTG_DEV->DIEPEMPMSK &= ~fifoemptymsk;
    }
}

/**
  * @brief  dwc2_get_glb_intstatus: return the global USB interrupt status
  * @retval status
  */
static inline uint32_t dwc2_get_glb_intstatus(struct usbd_bus *bus)
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
static inline uint32_t dwc2_get_outeps_intstatus(struct usbd_bus *bus)
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
static inline uint32_t dwc2_get_ineps_intstatus(struct usbd_bus *bus)
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
static inline uint32_t dwc2_get_outep_intstatus(struct usbd_bus *bus, uint8_t epnum)
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
static inline uint32_t dwc2_get_inep_intstatus(struct usbd_bus *bus, uint8_t epnum)
{
    uint32_t tmpreg, msk, emp;

    msk = USB_OTG_DEV->DIEPMSK;
    emp = USB_OTG_DEV->DIEPEMPMSK;
    msk |= ((emp >> (epnum & 0x07)) & 0x1U) << 7;
    tmpreg = USB_OTG_INEP((uint32_t)epnum)->DIEPINT & msk;

    return tmpreg;
}

int dwc2_udc_init(struct usbd_bus *bus)
{
    int ret;
    struct dwc2_udc *udc;
    uint8_t fsphy_type;
    uint8_t hsphy_type;
    uint8_t dma_support;
    uint8_t endpoints;

    udc = dwc2_alloc_udc();
    if (udc == NULL) {
        return -1;
    }

    bus->udc = udc;

    usbd_udc_low_level_init(bus->busid);

    /*
        Full-Speed PHY Interface Type (FSPhyType)
        2'b00: Full-speed interface not supported
        2'b01: Dedicated full-speed interface
        2'b10: FS pins shared with UTMI+ pins
        2'b11: FS pins shared with ULPI pins

        High-Speed PHY Interface Type (HSPhyType)
        2'b00: High-Speed interface not supported
        2'b01: UTMI+
        2'b10: ULPI
        2'b11: UTMI+ and ULPI

        Architecture (OtgArch)
        2'b00: Slave-Only
        2'b01: External DMA
        2'b10: Internal DMA
        Others: Reserved
    */
    fsphy_type = ((USB_OTG_GLB->GHWCFG2 & (0x03 << 8)) >> 8);
    hsphy_type = ((USB_OTG_GLB->GHWCFG2 & (0x03 << 6)) >> 6);
    dma_support = ((USB_OTG_GLB->GHWCFG2 & (0x03 << 3)) >> 3);
    endpoints = ((USB_OTG_GLB->GHWCFG2 & (0x0f << 10)) >> 10) + 1;

    USB_LOG_INFO("========== dwc2 udc params ==========\r\n");
    USB_LOG_INFO("CID:%08x\r\n", USB_OTG_GLB->CID);
    USB_LOG_INFO("GSNPSID:%08x\r\n", USB_OTG_GLB->GSNPSID);
    USB_LOG_INFO("GHWCFG1:%08x\r\n", USB_OTG_GLB->GHWCFG1);
    USB_LOG_INFO("GHWCFG2:%08x\r\n", USB_OTG_GLB->GHWCFG2);
    USB_LOG_INFO("GHWCFG3:%08x\r\n", USB_OTG_GLB->GHWCFG3);
    USB_LOG_INFO("GHWCFG4:%08x\r\n", USB_OTG_GLB->GHWCFG4);

    USB_LOG_INFO("dwc2 fsphy type:%d, hsphy type:%d, dma support:%d\r\n", fsphy_type, hsphy_type, dma_support);
    USB_LOG_INFO("dwc2 has %d endpoints, default config: %d endpoints\r\n", endpoints, CONFIG_DWC2_BIDIR_ENDPOINTS);
    USB_LOG_INFO("=================================\r\n");

    bus->endpoints = MIN(endpoints, CONFIG_DWC2_BIDIR_ENDPOINTS);
    udc->dma_enable = dma_support ? true : false;

    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_SDIS;

    USB_OTG_GLB->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;

    ret = dwc2_core_init(bus);

    /* Force Device Mode*/
    dwc2_set_mode(bus, USB_OTG_MODE_DEVICE);

    for (uint8_t i = 0U; i < 15U; i++) {
        USB_OTG_GLB->DIEPTXF[i] = 0U;
    }

#if defined(STM32F7) || defined(STM32H7) || defined(STM32L4)
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
#ifdef CONFIG_DWC2_GD32
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_VBUSBSEN | USB_OTG_GCCFG_VBUSASEN;
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
#endif
    /* Restart the Phy Clock */
    USB_OTG_PCGCCTL = 0U;

    /* Device mode configuration */
    USB_OTG_DEV->DCFG |= DCFG_FRAME_INTERVAL_80;

    if (hsphy_type) {
#if defined(CONFIG_USB_HS)
        /* Set Core speed to High speed mode */
        USB_OTG_DEV->DCFG |= USB_OTG_SPEED_HIGH;
#else

        USB_OTG_DEV->DCFG |= USB_OTG_SPEED_HIGH_IN_FULL;
#endif
    } else {
        USB_OTG_DEV->DCFG |= USB_OTG_SPEED_FULL;
    }

    ret = dwc2_flush_txfifo(bus, 0x10U);
    ret = dwc2_flush_rxfifo(bus);

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
                           USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_IEPINT |
                           USB_OTG_GINTMSK_IISOIXFRM | USB_OTG_GINTMSK_PXFRM_IISOOXFRM;

    if (udc->dma_enable) {
        USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_HBSTLEN_2;
        USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_DMAEN;
    } else {
        USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM;
    }

#if CONFIG_DWC2_VBUS_SENSING
    USB_OTG_GLB->GINTMSK |= (USB_OTG_GINTMSK_OTGINT | USB_OTG_GINTMSK_SRQIM);
#endif
#if 0
    USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_SOFM;
#endif

    if (hsphy_type) {
        for (uint8_t i = 0; i < sizeof(dwc2_hs_fifo_config); i++) {
            if (i == 0) {
                USB_OTG_GLB->GRXFSIZ = (dwc2_hs_fifo_config[i] / 4);
            } else {
                dwc2_set_txfifo(bus, i - 1, dwc2_hs_fifo_config[i] / 4);
            }
        }
    } else {
        for (uint8_t i = 0; i < sizeof(dwc2_fs_fifo_config); i++) {
            if (i == 0) {
                USB_OTG_GLB->GRXFSIZ = (dwc2_fs_fifo_config[i] / 4);
            } else {
                dwc2_set_txfifo(bus, i - 1, dwc2_fs_fifo_config[i] / 4);
            }
        }
    }

    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
    USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;

    return ret;
}

int dwc2_udc_deinit(struct usbd_bus *bus)
{
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

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
    dwc2_flush_txfifo(bus, 0x10U);
    dwc2_flush_rxfifo(bus);

    USB_OTG_DEV->DCTL |= USB_OTG_DCTL_SDIS;

    dwc2_free_udc(udc);
    bus->udc = NULL;
    usbd_udc_low_level_deinit(bus->busid);
    return 0;
}

int dwc2_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    USB_OTG_DEV->DCFG &= ~(USB_OTG_DCFG_DAD);
    USB_OTG_DEV->DCFG |= ((uint32_t)addr << 4) & USB_OTG_DCFG_DAD;
    return 0;
}

uint8_t dwc2_get_port_speed(struct usbd_bus *bus)
{
    uint8_t speed;
    uint32_t DevEnumSpeed = USB_OTG_DEV->DSTS & USB_OTG_DSTS_ENUMSPD;

    if (DevEnumSpeed == DSTS_ENUMSPD_HS_PHY_30MHZ_OR_60MHZ) {
        speed = USB_SPEED_HIGH;
    } else if ((DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_30MHZ_OR_60MHZ) ||
               (DevEnumSpeed == DSTS_ENUMSPD_FS_PHY_48MHZ)) {
        speed = USB_SPEED_FULL;
    } else {
        speed = USB_SPEED_FULL;
    }

    return speed;
}

int dwc2_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (ep_idx > (bus->endpoints - 1)) {
        USB_LOG_ERR("Ep addr 0x%02x overflow\r\n", ep_desc->bEndpointAddress);
        return -2;
    }

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        udc->out_ep[ep_idx].ep_mps = ep_mps;
        udc->out_ep[ep_idx].ep_type = ep_type;
        udc->out_ep[ep_idx].ep_enable = true;

        USB_OTG_DEV->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & (uint32_t)(1UL << (16 + ep_idx));

        if ((USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_USBAEP) == 0) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (ep_mps & USB_OTG_DOEPCTL_MPSIZ) |
                                              ((uint32_t)ep_type << 18) |
                                              USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                              USB_OTG_DOEPCTL_USBAEP;
        }
    } else {
        udc->in_ep[ep_idx].ep_mps = ep_mps;
        udc->in_ep[ep_idx].ep_type = ep_type;
        udc->in_ep[ep_idx].ep_enable = true;

        USB_OTG_DEV->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << ep_idx);

        if ((USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP) == 0) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= (ep_mps & USB_OTG_DIEPCTL_MPSIZ) |
                                             ((uint32_t)ep_type << 18) | (ep_idx << 22) |
                                             USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                             USB_OTG_DIEPCTL_USBAEP;
        }
    }
    return 0;
}

int dwc2_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    volatile uint32_t count = 0U;

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_EPDIS;

            /* Wait for endpoint disabled interrupt */
            count = 0;
            do {
                if (++count > 50000) {
                    break;
                }
            } while ((USB_OTG_OUTEP(ep_idx)->DOEPINT & USB_OTG_DOEPINT_EPDISD) != USB_OTG_DOEPINT_EPDISD);

            /* Clear and unmask endpoint disabled interrupt */
            USB_OTG_OUTEP(ep_idx)->DOEPINT |= USB_OTG_DOEPINT_EPDISD;
        }

        USB_OTG_DEV->DEACHMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USB_OTG_DEV->DAINTMSK &= ~(USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~(USB_OTG_DOEPCTL_USBAEP |
                                            USB_OTG_DOEPCTL_MPSIZ |
                                            USB_OTG_DOEPCTL_SD0PID_SEVNFRM |
                                            USB_OTG_DOEPCTL_EPTYP);
    } else {
        if (USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_EPDIS;

            /* Wait for endpoint disabled interrupt */
            count = 0;
            do {
                if (++count > 50000) {
                    break;
                }
            } while ((USB_OTG_INEP(ep_idx)->DIEPINT & USB_OTG_DIEPINT_EPDISD) != USB_OTG_DIEPINT_EPDISD);

            /* Clear and unmask endpoint disabled interrupt */
            USB_OTG_INEP(ep_idx)->DIEPINT |= USB_OTG_DIEPINT_EPDISD;
        }

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

int dwc2_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

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

    if ((ep_idx == 0) && udc->dma_enable) {
        dwc2_ep0_start_read_setup(bus, (uint8_t *)&udc->setup);
    }

    return 0;
}

int dwc2_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep)) {
        USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
        if ((udc->out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (udc->out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
    } else {
        USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
        if ((udc->in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_INTERRUPT) ||
            (udc->in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_BULK)) {
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
    }
    return 0;
}

int dwc2_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
    } else {
    }
    return 0;
}

int dwc2_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }

    if (!udc->in_ep[ep_idx].ep_enable) {
        return -3;
    }

    if (udc->dma_enable) {
        if ((uint32_t)data & 0x03) {
            return -4;
        }
    }

    udc->in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    udc->in_ep[ep_idx].xfer_len = data_len;
    udc->in_ep[ep_idx].actual_xfer_len = 0;

    USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
    USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);

    if (data_len == 0) {
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > udc->in_ep[ep_idx].ep_mps) {
            data_len = udc->in_ep[ep_idx].ep_mps;
        }
        udc->in_ep[ep_idx].xfer_len = data_len;
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + udc->in_ep[ep_idx].ep_mps - 1U) / udc->in_ep[ep_idx].ep_mps);

        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (pktcnt << 19));
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    }

    if (udc->in_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
            USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
        } else {
            USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SODDFRM;
            USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
        USB_OTG_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_MULCNT);
        USB_OTG_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_MULCNT & (1U << 29));
    }

    if (udc->dma_enable) {
        USB_OTG_INEP(ep_idx)->DIEPDMA = (uint32_t)data;

        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
    } else {
        USB_OTG_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);
        /* Enable the Tx FIFO Empty Interrupt for this EP */
        if (data_len > 0U) {
            USB_OTG_DEV->DIEPEMPMSK |= 1UL << (ep_idx & 0x0f);
        }
    }
    return 0;
}

int dwc2_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt = 0;
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }

    if (!udc->out_ep[ep_idx].ep_enable) {
        return -3;
    }

    if (udc->dma_enable) {
        if ((uint32_t)data & 0x03) {
            return -4;
        }
    }

    udc->out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    udc->out_ep[ep_idx].xfer_len = data_len;
    udc->out_ep[ep_idx].actual_xfer_len = 0;

    USB_OTG_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);
    USB_OTG_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
    if (data_len == 0) {
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1 << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & udc->out_ep[ep_idx].ep_mps);
        USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > udc->out_ep[ep_idx].ep_mps) {
            data_len = udc->out_ep[ep_idx].ep_mps;
        }
        udc->out_ep[ep_idx].xfer_len = data_len;
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + udc->out_ep[ep_idx].ep_mps - 1U) / udc->out_ep[ep_idx].ep_mps);

        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (pktcnt << 19));
        USB_OTG_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & data_len);
    }

    if (udc->dma_enable) {
        USB_OTG_OUTEP(ep_idx)->DOEPDMA = (uint32_t)data;
    }
    if (udc->out_ep[ep_idx].ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        if ((USB_OTG_DEV->DSTS & (1U << 8)) == 0U) {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SODDFRM;
        } else {
            USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_SODDFRM;
            USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
        }
    }
    USB_OTG_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
    return 0;
}

void dwc2_udc_irq(struct usbd_bus *bus)
{
    uint32_t gint_status, temp, ep_idx, ep_intr, epint, read_count, daintmask;
    gint_status = dwc2_get_glb_intstatus(bus);
    struct dwc2_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    if ((USB_OTG_GLB->GINTSTS & 0x1U) == USB_OTG_MODE_DEVICE) {
        /* Avoid spurious interrupt */
        if (gint_status == 0) {
            return;
        }
        if (udc->dma_enable == 0) {
            /* Handle RxQLevel Interrupt */
            if (gint_status & USB_OTG_GINTSTS_RXFLVL) {
                USB_MASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);

                temp = USB_OTG_GLB->GRXSTSP;
                ep_idx = temp & USB_OTG_GRXSTSP_EPNUM;

                if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_DATA_UPDT) {
                    read_count = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                    if (read_count != 0) {
                        dwc2_ep_read(bus, udc->out_ep[ep_idx].xfer_buf, read_count);
                        udc->out_ep[ep_idx].xfer_buf += read_count;
                    }
                } else if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_SETUP_UPDT) {
                    read_count = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                    dwc2_ep_read(bus, (uint8_t *)&udc->setup, read_count);
                } else {
                    /* ... */
                }
                USB_UNMASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);
            }
        }
        if (gint_status & USB_OTG_GINTSTS_OEPINT) {
            ep_idx = 0;
            ep_intr = dwc2_get_outeps_intstatus(bus);
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = dwc2_get_outep_intstatus(bus, ep_idx);
                    uint32_t DoepintReg = USB_OTG_OUTEP(ep_idx)->DOEPINT;
                    USB_OTG_OUTEP(ep_idx)->DOEPINT = DoepintReg;

                    if ((epint & USB_OTG_DOEPINT_XFRC) == USB_OTG_DOEPINT_XFRC) {
                        if (ep_idx == 0) {
                            if (udc->out_ep[ep_idx].xfer_len == 0) {
                                /* Out status, start reading setup */
                                dwc2_ep0_start_read_setup(bus, (uint8_t *)&udc->setup);
                            } else {
                                udc->out_ep[ep_idx].actual_xfer_len = udc->out_ep[ep_idx].xfer_len - ((USB_OTG_OUTEP(ep_idx)->DOEPTSIZ) & USB_OTG_DOEPTSIZ_XFRSIZ);
                                udc->out_ep[ep_idx].xfer_len = 0;
                                usbd_event_ep_out_complete_handler(bus->busid, 0x00, udc->out_ep[ep_idx].actual_xfer_len);
                            }
                        } else {
                            udc->out_ep[ep_idx].actual_xfer_len = udc->out_ep[ep_idx].xfer_len - ((USB_OTG_OUTEP(ep_idx)->DOEPTSIZ) & USB_OTG_DOEPTSIZ_XFRSIZ);
                            udc->out_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_out_complete_handler(bus->busid, ep_idx, udc->out_ep[ep_idx].actual_xfer_len);
                        }
                    }

                    if ((epint & USB_OTG_DOEPINT_STUP) == USB_OTG_DOEPINT_STUP) {
                        usbd_event_ep0_setup_complete_handler(bus->busid, (uint8_t *)&udc->setup);
                    }
                }
                ep_intr >>= 1U;
                ep_idx++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_IEPINT) {
            ep_idx = 0U;
            ep_intr = dwc2_get_ineps_intstatus(bus);
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = dwc2_get_inep_intstatus(bus, ep_idx);
                    uint32_t DiepintReg = USB_OTG_INEP(ep_idx)->DIEPINT;
                    USB_OTG_INEP(ep_idx)->DIEPINT = DiepintReg;

                    if ((epint & USB_OTG_DIEPINT_XFRC) == USB_OTG_DIEPINT_XFRC) {
                        if (ep_idx == 0) {
                            udc->in_ep[ep_idx].actual_xfer_len = udc->in_ep[ep_idx].xfer_len - ((USB_OTG_INEP(ep_idx)->DIEPTSIZ) & USB_OTG_DIEPTSIZ_XFRSIZ);
                            udc->in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(bus->busid, 0x80, udc->in_ep[ep_idx].actual_xfer_len);

                            if (udc->setup.wLength && ((udc->setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                                /* In status, start reading setup */
                                dwc2_ep0_start_read_setup(bus, (uint8_t *)&udc->setup);
                            } else if (udc->setup.wLength == 0) {
                                /* In status, start reading setup */
                                dwc2_ep0_start_read_setup(bus, (uint8_t *)&udc->setup);
                            }
                        } else {
                            udc->in_ep[ep_idx].actual_xfer_len = udc->in_ep[ep_idx].xfer_len - ((USB_OTG_INEP(ep_idx)->DIEPTSIZ) & USB_OTG_DIEPTSIZ_XFRSIZ);
                            udc->in_ep[ep_idx].xfer_len = 0;
                            usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, udc->in_ep[ep_idx].actual_xfer_len);
                        }
                    }
                    if ((epint & USB_OTG_DIEPINT_TXFE) == USB_OTG_DIEPINT_TXFE) {
                        dwc2_tx_fifo_empty_procecss(bus, ep_idx);
                    }
                }
                ep_intr >>= 1U;
                ep_idx++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_USBRST) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_USBRST;
            USB_OTG_DEV->DCTL &= ~USB_OTG_DCTL_RWUSIG;

            dwc2_flush_txfifo(bus, 0x10U);
            dwc2_flush_rxfifo(bus);

            for (uint8_t i = 0U; i < bus->endpoints; i++) {
                if (i == 0U) {
                    USB_OTG_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_SNAK;
                    USB_OTG_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_SNAK;
                } else {
                    if (USB_OTG_INEP(i)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) {
                        USB_OTG_INEP(i)->DIEPCTL = (USB_OTG_DIEPCTL_EPDIS | USB_OTG_DIEPCTL_SNAK);
                    } else {
                        USB_OTG_INEP(i)->DIEPCTL = 0;
                    }
                    if (USB_OTG_OUTEP(i)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) {
                        USB_OTG_OUTEP(i)->DOEPCTL = (USB_OTG_DOEPCTL_EPDIS | USB_OTG_DOEPCTL_SNAK);
                    } else {
                        USB_OTG_OUTEP(i)->DOEPCTL = 0;
                    }
                }
                USB_OTG_INEP(i)->DIEPTSIZ = 0U;
                USB_OTG_INEP(i)->DIEPINT = 0xFBFFU;
                USB_OTG_OUTEP(i)->DOEPTSIZ = 0U;
                USB_OTG_OUTEP(i)->DOEPINT = 0xFBFFU;
            }

            USB_OTG_DEV->DAINTMSK |= 0x10001U;

            USB_OTG_DEV->DOEPMSK = USB_OTG_DOEPMSK_STUPM |
                                   USB_OTG_DOEPMSK_XFRCM;

            USB_OTG_DEV->DIEPMSK = USB_OTG_DIEPMSK_XFRCM;

            memset(udc->in_ep, 0, sizeof(struct dwc2_ep) * bus->endpoints);
            memset(udc->out_ep, 0, sizeof(struct dwc2_ep) * bus->endpoints);
            usbd_event_reset_handler(bus->busid);
            /* Start reading setup */
            dwc2_ep0_start_read_setup(bus, (uint8_t *)&udc->setup);
        }
        if (gint_status & USB_OTG_GINTSTS_ENUMDNE) {
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_ENUMDNE;
            dwc2_set_turnaroundtime(bus, SystemCoreClock, dwc2_get_devspeed(bus));

            USB_OTG_DEV->DCTL |= USB_OTG_DCTL_CGINAK;
        }
        if (gint_status & USB_OTG_GINTSTS_PXFR_INCOMPISOOUT) {
            daintmask = USB_OTG_DEV->DAINTMSK;
            daintmask >>= 16;

            for (ep_idx = 1; ep_idx < bus->endpoints; ep_idx++) {
                if ((BIT(ep_idx) & ~daintmask) || (udc->out_ep[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS))
                    continue;
                if (!(USB_OTG_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_USBAEP))
                    continue;

                if ((USB_OTG_DEV->DSTS & (1U << 8)) != 0U) {
                    USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
                    USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_SODDFRM;
                } else {
                    USB_OTG_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
                    USB_OTG_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SODDFRM;
                }
            }

            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_PXFR_INCOMPISOOUT;
        }

        if (gint_status & USB_OTG_GINTSTS_IISOIXFR) {
            daintmask = USB_OTG_DEV->DAINTMSK;
            daintmask >>= 16;

            for (ep_idx = 1; ep_idx < bus->endpoints; ep_idx++) {
                if (((BIT(ep_idx) & ~daintmask)) || (udc->in_ep[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS))
                    continue;

                if (!(USB_OTG_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP))
                    continue;

                if ((USB_OTG_DEV->DSTS & (1U << 8)) != 0U) {
                    USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
                    USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SODDFRM;
                } else {
                    USB_OTG_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
                    USB_OTG_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
                }
            }
            USB_OTG_GLB->GINTSTS |= USB_OTG_GINTSTS_IISOIXFR;
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

struct usbd_udc_driver dwc2_udc_driver = {
    .driver_name = "dwc2 udc",
    .udc_init = dwc2_udc_init,
    .udc_deinit = dwc2_udc_deinit,
    .udc_set_address = dwc2_set_address,
    .udc_get_port_speed = dwc2_get_port_speed,
    .udc_ep_open = dwc2_ep_open,
    .udc_ep_close = dwc2_ep_close,
    .udc_ep_set_stall = dwc2_ep_set_stall,
    .udc_ep_clear_stall = dwc2_ep_clear_stall,
    .udc_ep_is_stalled = dwc2_ep_is_stalled,
    .udc_ep_start_write = dwc2_ep_start_write,
    .udc_ep_start_read = dwc2_ep_start_read,
    .udc_irq = dwc2_udc_irq
};