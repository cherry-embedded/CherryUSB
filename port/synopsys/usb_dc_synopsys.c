#include "usbd_core.h"
#include "usb_synopsys_reg.h"

/*!< USB registers base address */
#define USB_OTG_HS_PERIPH_BASE 0x40040000UL
#ifndef STM32H7
#define USB_OTG_FS_PERIPH_BASE 0x50000000UL
#else
#define USB_OTG_FS_PERIPH_BASE 0x40080000UL
#endif

#define USB_OTG_FS ((USB_OTG_GlobalTypeDef *)USB_OTG_FS_PERIPH_BASE)
#define USB_OTG_HS ((USB_OTG_GlobalTypeDef *)USB_OTG_HS_PERIPH_BASE)

#if defined(CONFIG_USB_HS) || defined(CONFIG_USB_HS_IN_FULL)
#ifndef USBD_IRQHandler
#define USBD_IRQHandler OTG_HS_IRQHandler
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 6 /* define with minimum value*/
#endif

#ifndef USB_RAM_SIZE
#define USB_RAM_SIZE 4096 /* define with minimum value*/
#endif

#else
#ifndef USBD_IRQHandler
#define USBD_IRQHandler OTG_FS_IRQHandler
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 4 /* define with minimum value*/
#endif

#ifndef USB_RAM_SIZE
#define USB_RAM_SIZE 1280 /* define with minimum value*/
#endif

#endif

#if defined(CONFIG_USB_HS) || defined(CONFIG_USB_HS_IN_FULL)
/*FIFO sizes in bytes (total available memory for FIFOs is 4 kB)*/
#ifndef CONFIG_USB_RX_FIFO_SIZE
#define CONFIG_USB_RX_FIFO_SIZE (1024U)
#endif
#ifndef CONFIG_USB_TX0_FIFO_SIZE
#define CONFIG_USB_TX0_FIFO_SIZE (64U)
#endif
#ifndef CONFIG_USB_TX1_FIFO_SIZE
#define CONFIG_USB_TX1_FIFO_SIZE (1024U)
#endif
#ifndef CONFIG_USB_TX2_FIFO_SIZE
#define CONFIG_USB_TX2_FIFO_SIZE (512U)
#endif
#ifndef CONFIG_USB_TX3_FIFO_SIZE
#define CONFIG_USB_TX3_FIFO_SIZE (256U)
#endif
#ifndef CONFIG_USB_TX4_FIFO_SIZE
#define CONFIG_USB_TX4_FIFO_SIZE (256U)
#endif
#ifndef CONFIG_USB_TX5_FIFO_SIZE
#define CONFIG_USB_TX5_FIFO_SIZE (256U)
#endif
#else
/*FIFO sizes in bytes (total available memory for FIFOs is 1.25kB)*/
#ifndef CONFIG_USB_RX_FIFO_SIZE
#define CONFIG_USB_RX_FIFO_SIZE (640U)
#endif
#ifndef CONFIG_USB_TX0_FIFO_SIZE
#define CONFIG_USB_TX0_FIFO_SIZE (64U)
#endif
#ifndef CONFIG_USB_TX1_FIFO_SIZE
#define CONFIG_USB_TX1_FIFO_SIZE (256U)
#endif
#ifndef CONFIG_USB_TX2_FIFO_SIZE
#define CONFIG_USB_TX2_FIFO_SIZE (160U)
#endif
#ifndef CONFIG_USB_TX3_FIFO_SIZE
#define CONFIG_USB_TX3_FIFO_SIZE (160U)
#endif
#endif

#ifndef CONFIG_USB_TURNAROUND_TIME
#define CONFIG_USB_TURNAROUND_TIME 6
#endif

/* Endpoint state */
struct usb_dc_ep_state {
    /** Endpoint max packet size */
    uint16_t ep_mps;
    /** Endpoint Transfer Type.
     * May be Bulk, Interrupt, Control or Isochronous
     */
    uint8_t ep_type;
    uint8_t ep_stalled; /** Endpoint stall flag */
};

/* Driver state */
struct usb_dc_config_priv {
    USB_OTG_GlobalTypeDef *Instance;                        /*!< Register base address */
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
    volatile uint32_t grxstsp;
} usb_dc_cfg;

static int usb_flush_rxfifo(USB_OTG_GlobalTypeDef *USBx);
static int usb_flush_txfifo(USB_OTG_GlobalTypeDef *USBx, uint32_t num);
static void usb_set_txfifo(USB_OTG_GlobalTypeDef *USBx, uint8_t fifo, uint16_t size);

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    uint32_t USBx_BASE;
    uint32_t count = 0U;

    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));

#if defined(CONFIG_USB_HS) || defined(CONFIG_USB_HS_IN_FULL)
    usb_dc_cfg.Instance = USB_OTG_HS;
#else
    usb_dc_cfg.Instance = USB_OTG_FS;
#endif
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;

    usb_dc_low_level_init();

    USBx_BASE = (uint32_t)USBx;

    USBx_DEVICE->DCTL |= USB_OTG_DCTL_SDIS;

    USBx->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;
#if defined(CONFIG_USB_HS)
    USBx->GCCFG &= ~(USB_OTG_GCCFG_PWRDWN);

    /* Init The ULPI Interface */
    USBx->GUSBCFG &= ~(USB_OTG_GUSBCFG_TSDPS | USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_PHYSEL);

    /* Select vbus source */
    USBx->GUSBCFG &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);

    //USBx->GUSBCFG |= USB_OTG_GUSBCFG_ULPIEVBUSD;

#else
    /* Select FS Embedded PHY */
    USBx->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;
    /* Activate the USB Transceiver */
    USBx->GCCFG |= USB_OTG_GCCFG_PWRDWN;

#endif

    /* Reset after a PHY select and set Host mode */
    /* Wait for AHB master IDLE state. */
    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);

    /* Core Soft Reset */
    count = 0U;
    USBx->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_CSRST) == USB_OTG_GRSTCTL_CSRST);

    /* Force Device Mode*/
    USBx->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    USBx->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
    USBx->GUSBCFG |= (uint32_t)((CONFIG_USB_TURNAROUND_TIME << 10) & USB_OTG_GUSBCFG_TRDT);

    for (uint8_t i = 0U; i < 15U; i++) {
        USBx->DIEPTXF[i] = 0U;
    }

#ifdef CONFIG_USB_SYNOPSYS_NOVBUSSEN
    /* Deactivate VBUS Sensing B */
    USBx->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    /* B-peripheral session valid override enable */
    USBx->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN;
    USBx->GOTGCTL |= USB_OTG_GOTGCTL_BVALOVAL;
#else
    USBx->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
    USBx->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USBx->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
#endif
    /* Restart the Phy Clock */
    USBx_PCGCCTL = 0U;

    /* Device mode configuration */
    USBx_DEVICE->DCFG |= DCFG_FRAME_INTERVAL_80;
#if defined(CONFIG_USB_HS)
    /* Set Core speed to High speed mode */
    USBx_DEVICE->DCFG |= USB_OTG_SPEED_HIGH;
#elif defined(CONFIG_USB_HS_IN_FULL)
    USBx_DEVICE->DCFG |= USB_OTG_SPEED_HIGH_IN_FULL;
#else
    USBx_DEVICE->DCFG |= USB_OTG_SPEED_FULL;
#endif

    usb_flush_txfifo(USBx, 0x10U);
    usb_flush_rxfifo(USBx);

    /* Clear all pending Device Interrupts */
    USBx_DEVICE->DIEPMSK = 0U;
    USBx_DEVICE->DOEPMSK = 0U;
    USBx_DEVICE->DAINTMSK = 0U;

    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if ((USBx_INEP(i)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) == USB_OTG_DIEPCTL_EPENA) {
            if (i == 0U) {
                USBx_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_SNAK;
            } else {
                USBx_INEP(i)->DIEPCTL = USB_OTG_DIEPCTL_EPDIS | USB_OTG_DIEPCTL_SNAK;
            }
        } else {
            USBx_INEP(i)->DIEPCTL = 0U;
        }

        USBx_INEP(i)->DIEPTSIZ = 0U;
        USBx_INEP(i)->DIEPINT = 0xFB7FU;
    }
    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if ((USBx_OUTEP(i)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) == USB_OTG_DOEPCTL_EPENA) {
            if (i == 0U) {
                USBx_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_SNAK;
            } else {
                USBx_OUTEP(i)->DOEPCTL = USB_OTG_DOEPCTL_EPDIS | USB_OTG_DOEPCTL_SNAK;
            }
        } else {
            USBx_OUTEP(i)->DOEPCTL = 0U;
        }

        USBx_OUTEP(i)->DOEPTSIZ = 0U;
        USBx_OUTEP(i)->DOEPINT = 0xFB7FU;
    }

    /* Disable all interrupts. */
    USBx->GINTMSK = 0U;

    /* Clear any pending interrupts */
    USBx->GINTSTS = 0xBFFFFFFFU;

    /* Enable interrupts matching to the Device mode ONLY */
    USBx->GINTMSK |= USB_OTG_GINTMSK_USBSUSPM | USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |
                     USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_IEPINT | USB_OTG_GINTMSK_RXFLVLM |
                     USB_OTG_GINTMSK_WUIM;
#if 0
    USBx->GINTMSK |= USB_OTG_GINTMSK_SOFM;
#endif
    USBx_DEVICE->DOEPMSK = USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM;

    USBx_DEVICE->DIEPMSK = USB_OTG_DIEPMSK_XFRCM;

    USBx->GRXFSIZ = (CONFIG_USB_RX_FIFO_SIZE / 4);
#if defined(CONFIG_USB_HS) || defined(CONFIG_USB_HS_IN_FULL)
    usb_set_txfifo(USBx, 0, CONFIG_USB_TX0_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 1, CONFIG_USB_TX1_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 2, CONFIG_USB_TX2_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 3, CONFIG_USB_TX3_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 4, CONFIG_USB_TX4_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 5, CONFIG_USB_TX5_FIFO_SIZE / 4);
#else
    usb_set_txfifo(USBx, 0, CONFIG_USB_TX0_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 1, CONFIG_USB_TX1_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 2, CONFIG_USB_TX2_FIFO_SIZE / 4);
    usb_set_txfifo(USBx, 3, CONFIG_USB_TX3_FIFO_SIZE / 4);
#endif
    USBx->GAHBCFG |= USB_OTG_GAHBCFG_GINT;
    USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_SDIS;

    return 0;
}

void usb_dc_deinit(void)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;

    usb_dc_low_level_deinit();
    /* Clear Pending interrupt */
    for (uint8_t i = 0U; i < 15U; i++) {
        USBx_INEP(i)->DIEPINT = 0xFB7FU;
        USBx_OUTEP(i)->DOEPINT = 0xFB7FU;
    }

    /* Clear interrupt masks */
    USBx_DEVICE->DIEPMSK = 0U;
    USBx_DEVICE->DOEPMSK = 0U;
    USBx_DEVICE->DAINTMSK = 0U;

    /* Flush the FIFO */
    usb_flush_txfifo(USBx, 0x10U);
    usb_flush_rxfifo(USBx);

    USBx_DEVICE->DCTL |= USB_OTG_DCTL_SDIS;
}

int usbd_set_address(const uint8_t addr)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;
    USBx_DEVICE->DCFG &= ~(USB_OTG_DCFG_DAD);
    USBx_DEVICE->DCFG |= ((uint32_t)addr << 4) & USB_OTG_DCFG_DAD;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (!ep_cfg) {
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & (uint32_t)(1UL << (16 + ep_idx));

        USBx_OUTEP(ep_idx)->DOEPCTL |= (ep_cfg->ep_mps & USB_OTG_DOEPCTL_MPSIZ) |
                                       ((uint32_t)ep_cfg->ep_type << 18) |
                                       USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                       USB_OTG_DOEPCTL_USBAEP;
        /* EP enable */
        USBx_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << ep_idx);

        USBx_INEP(ep_idx)->DIEPCTL |= (ep_cfg->ep_mps & USB_OTG_DIEPCTL_MPSIZ) |
                                      ((uint32_t)ep_cfg->ep_type << 18) | (ep_idx << 22) |
                                      USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                      USB_OTG_DIEPCTL_USBAEP;
    }
    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
    } else {
        USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
    }

    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USBx_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
    } else {
        USBx_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
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

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t len32b;

    if (!data && data_len) {
        return -1;
    }

    if (!data_len) {
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        /* EP enable, IN data in FIFO */
        USBx_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }

    /* Program the transfer size and packet count
      * as follows: xfersize = N * maxpacket +
      * short_packet pktcnt = N + (short_packet
      * exist ? 1 : 0)
      */
    USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
    USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);
    USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
    //USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (((data_len + usb_dc_cfg.in_ep[ep_idx].ep_mps - 1U) / usb_dc_cfg.in_ep[ep_idx].ep_mps) << 19));
    USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);
    /* EP enable, IN data in FIFO */
    USBx_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

    if (usb_dc_cfg.in_ep[ep_idx].ep_type == EP_TYPE_ISOC) {
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_MULCNT);
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_MULCNT & (1U << 29));

        if ((USBx_DEVICE->DSTS & (1U << 8)) == 0U) {
            USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
        } else {
            USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
        }
    }

    len32b = (data_len + 3U) / 4U;

    while (USBx_INEP(ep_idx)->DTXFSTS < len32b) {
    }
    for (uint8_t i = 0U; i < len32b; i++) {
        USBx_DFIFO(ep_idx) = ((uint32_t *)data)[i];
    }

    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t USBx_BASE = (uint32_t)USBx;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t *pdest = (uint32_t *)data;
    uint32_t len32b;
    uint32_t read_count;
    uint32_t pktcnt;
    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
        if (ep_idx != 0) {
            /* Program the transfer size and packet count as follows:
            * pktcnt = N
            * xfersize = N * maxpacket
            */
            pktcnt = (uint16_t)((max_data_len + usb_dc_cfg.out_ep[ep_idx].ep_mps - 1U) / usb_dc_cfg.out_ep[ep_idx].ep_mps);
            USBx_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);
            USBx_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
            USBx_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (pktcnt << 19));
            USBx_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & usb_dc_cfg.out_ep[ep_idx].ep_mps * pktcnt);
            /* EP enable */
            USBx_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);
        }

        return 0;
    }

    if (max_data_len > usb_dc_cfg.out_ep[ep_idx].ep_mps) {
        max_data_len = usb_dc_cfg.out_ep[ep_idx].ep_mps;
    }

    read_count = (usb_dc_cfg.grxstsp & USB_OTG_GRXSTSP_BCNT) >> 4;
    read_count = MIN(read_count, max_data_len);

    len32b = ((uint32_t)read_count + 3U) / 4U;

    for (uint8_t i = 0U; i < len32b; i++) {
        *pdest = USBx_DFIFO(0U);
        pdest++;
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

    usb_dc_cfg.grxstsp = 0;
    return 0;
}

/**
  * @brief  This function handles PCD interrupt request.
  * @param  hpcd PCD handle
  * @retval HAL status
  */
void USBD_IRQHandler(void)
{
    USB_OTG_GlobalTypeDef *USBx = usb_dc_cfg.Instance;
    uint32_t gint_status, temp, epnum, ep_intr, epint;
    uint32_t USBx_BASE = (uint32_t)USBx;
    gint_status = USBx->GINTSTS;

    if ((gint_status & 0x1U) == USB_OTG_MODE_DEVICE) {
        if (gint_status == 0) {
            return;
        }
        /* Handle RxQLevel Interrupt */
        if (gint_status & USB_OTG_GINTSTS_RXFLVL) {
            USB_MASK_INTERRUPT(USBx, USB_OTG_GINTSTS_RXFLVL);
            temp = USBx->GRXSTSP;
            usb_dc_cfg.grxstsp = temp;
            epnum = temp & USB_OTG_GRXSTSP_EPNUM;
            if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_DATA_UPDT) {
                if ((temp & USB_OTG_GRXSTSP_BCNT) != 0U) {
                    if (epnum == 0) {
                        usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                    } else {
                        usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(epnum | USB_EP_DIR_OUT));
                    }
                }
            } else if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> USB_OTG_GRXSTSP_PKTSTS_Pos) == STS_SETUP_UPDT) {
                uint8_t len = (temp & USB_OTG_GRXSTSP_BCNT) >> 4;
                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
            } else {
                /* ... */
            }
            USB_UNMASK_INTERRUPT(USBx, USB_OTG_GINTSTS_RXFLVL);
        }

        if (gint_status & USB_OTG_GINTSTS_OEPINT) {
            epnum = 0;
            /* Read in the device interrupt bits */
            ep_intr = USBx_DEVICE->DAINT;
            ep_intr &= USBx_DEVICE->DAINTMSK;
            ep_intr >>= 16;

            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = USBx_OUTEP((uint32_t)epnum)->DOEPINT;

                    epint &= USBx_DEVICE->DOEPMSK;
                    if ((epint & USB_OTG_DOEPINT_STUP) == USB_OTG_DOEPINT_STUP) {
                        USBx_OUTEP(epnum)->DOEPINT = (USB_OTG_DOEPINT_STUP);

                        USBx_OUTEP(epnum)->DOEPTSIZ = 1U << USB_OTG_DOEPTSIZ_PKTCNT_Pos |
                                                      (USBx_OUTEP(epnum)->DOEPCTL & USB_OTG_DOEPCTL_MPSIZ) << USB_OTG_DOEPTSIZ_XFRSIZ_Pos;
                        USBx_OUTEP(epnum)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
                    }
                    if ((epint & USB_OTG_DOEPINT_XFRC) == USB_OTG_DOEPINT_XFRC) {
                        USBx_OUTEP(epnum)->DOEPINT = (USB_OTG_DOEPINT_XFRC);

                        if (epnum == 0) {
                            USBx_OUTEP(epnum)->DOEPTSIZ = 1U << USB_OTG_DOEPTSIZ_PKTCNT_Pos |
                                                          (USBx_OUTEP(epnum)->DOEPCTL & USB_OTG_DOEPCTL_MPSIZ) << USB_OTG_DOEPTSIZ_XFRSIZ_Pos;
                            USBx_OUTEP(epnum)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
                        }
                    }
                }
                ep_intr >>= 1U;
                epnum++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_IEPINT) {
            epnum = 0U;
            ep_intr = USBx_DEVICE->DAINT & 0xFFFF;
            ep_intr &= USBx_DEVICE->DAINTMSK;
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = USBx_INEP((uint32_t)epnum)->DIEPINT;
                    epint &= USBx_DEVICE->DIEPMSK;

                    if ((epint & USB_OTG_DIEPINT_XFRC) == USB_OTG_DIEPINT_XFRC) {
                        USBx_INEP(epnum)->DIEPINT = USB_OTG_DIEPINT_XFRC;
                        if (epnum == 0) {
                            usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                        } else {
                            usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(epnum | USB_EP_DIR_IN));
                        }
                    }
                }
                ep_intr >>= 1U;
                epnum++;
            }
        }
        if (gint_status & USB_OTG_GINTSTS_USBRST) {
            USBx->GINTSTS |= USB_OTG_GINTSTS_USBRST;
            USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_RWUSIG;

            usb_flush_txfifo(USBx, 0x10U);
            usb_flush_rxfifo(USBx);
            for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
                USBx_INEP(i)->DIEPINT = 0xFB7FU;
                USBx_INEP(i)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
                USBx_INEP(i)->DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
                USBx_OUTEP(i)->DOEPINT = 0xFB7FU;
                USBx_OUTEP(i)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
                USBx_OUTEP(i)->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
            }
            USBx_DEVICE->DAINTMSK |= 0x10001U;

            USBx_OUTEP(0U)->DOEPTSIZ = 0U;
            USBx_OUTEP(0U)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
            USBx_OUTEP(0U)->DOEPTSIZ |= (3U * 8U);
            USBx_OUTEP(0U)->DOEPTSIZ |= USB_OTG_DOEPTSIZ_STUPCNT;
            USBx_OUTEP(0)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;

            usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
        }
        if (gint_status & USB_OTG_GINTSTS_ENUMDNE) {
            USBx->GINTSTS |= USB_OTG_GINTSTS_ENUMDNE;
            //uint8_t speed = USBx_DEVICE->DSTS & USB_OTG_DSTS_ENUMSPD;
            /* Set the MPS of the IN EP0 to 64 bytes */
            USBx_INEP(0U)->DIEPCTL &= ~USB_OTG_DIEPCTL_MPSIZ;

            USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGINAK;
        }
        if (gint_status & USB_OTG_GINTSTS_SOF) {
            USBx->GINTSTS |= USB_OTG_GINTSTS_SOF;
        }
        if (gint_status & USB_OTG_GINTSTS_USBSUSP) {
            USBx->GINTSTS |= USB_OTG_GINTSTS_USBSUSP;
        }
    }
}

static int usb_flush_rxfifo(USB_OTG_GlobalTypeDef *USBx)
{
    uint32_t count = 0;

    USBx->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH);

    return 0;
}

static int usb_flush_txfifo(USB_OTG_GlobalTypeDef *USBx, uint32_t num)
{
    uint32_t count = 0U;

    USBx->GRSTCTL = (USB_OTG_GRSTCTL_TXFFLSH | (num << 6));

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);

    return 0;
}

static void usb_set_txfifo(USB_OTG_GlobalTypeDef *USBx, uint8_t fifo, uint16_t size)
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

    Tx_Offset = USBx->GRXFSIZ;

    if (fifo == 0U) {
        USBx->DIEPTXF0_HNPTXFSIZ = ((uint32_t)size << 16) | Tx_Offset;
    } else {
        Tx_Offset += (USBx->DIEPTXF0_HNPTXFSIZ) >> 16;
        for (i = 0U; i < (fifo - 1U); i++) {
            Tx_Offset += (USBx->DIEPTXF[i] >> 16);
        }

        /* Multiply Tx_Size by 2 to get higher performance */
        USBx->DIEPTXF[fifo - 1U] = ((uint32_t)size << 16) | Tx_Offset;
    }
}
