#include "usbd_core.h"
#include "usb_hs_reg.h"

#include "drv_usb_hw.h"

#if defined(CONFIG_USB_HS0)
#ifndef USBD_IRQHandler
#define USBD_IRQHandler USBHS0_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE USBHS0
#endif

#endif /* CONFIG_USB_HS0 */

#if defined(CONFIG_USB_HS1)
#ifndef USBD_IRQHandler
#define USBD_IRQHandler USBHS1_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE USBHS1
#endif

#endif /* CONFIG_USB_HS1 */

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8 /* define with minimum value*/
#endif

#ifndef USB_RAM_SIZE
#define USB_RAM_SIZE 4096 /* define with minimum value*/
#endif


/*FIFO sizes in bytes (total available memory for FIFOs is 4 kB)*/
#ifndef CONFIG_USB_RX_FIFO_SIZE
#define CONFIG_USB_RX_FIFO_SIZE (1024U)
#endif

#ifndef CONFIG_USB_TX0_FIFO_SIZE
#define CONFIG_USB_TX0_FIFO_SIZE (512U)
#endif

#ifndef CONFIG_USB_TX1_FIFO_SIZE
#define CONFIG_USB_TX1_FIFO_SIZE (512U)
#endif

#ifndef CONFIG_USB_TX2_FIFO_SIZE
#define CONFIG_USB_TX2_FIFO_SIZE (512U)
#endif

#ifndef CONFIG_USB_TX3_FIFO_SIZE
#define CONFIG_USB_TX3_FIFO_SIZE (512U)
#endif

#ifndef CONFIG_USB_TX4_FIFO_SIZE
#define CONFIG_USB_TX4_FIFO_SIZE (256U)
#endif

#ifndef CONFIG_USB_TX5_FIFO_SIZE
#define CONFIG_USB_TX5_FIFO_SIZE (256U)
#endif

#ifndef CONFIG_USB_TX6_FIFO_SIZE
#define CONFIG_USB_TX6_FIFO_SIZE (256U)
#endif

#ifndef CONFIG_USB_TX7_FIFO_SIZE
#define CONFIG_USB_TX7_FIFO_SIZE (256U)
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
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
USB_NOCACHE_RAM_SECTION struct usb_dc_config_priv {
    usb_core_regs *Instance; /*!< Register base address */
    volatile uint32_t grxstsp;
    __attribute__((aligned(32))) struct usb_setup_packet setup;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

static usb_core_regs *USBx = NULL;

static int usb_flush_rxfifo(void);
static int usb_flush_txfifo(uint32_t num);
static void usb_set_txfifo(uint8_t fifo, uint16_t size);
static int usb_txfifo_write(uint8_t *src_buf, uint8_t  fifo_num, uint16_t byte_count);
static void *usb_rxfifo_read(uint8_t *dest_buf, uint16_t byte_count);
static uint32_t usbd_emptytxfifo_write(uint32_t ep_num);
static void usb_ctlep_startout (uint8_t *psetup);

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

/*!
    \brief      configure USB core to soft reset
    \param[in]  usb_regs: pointer to USB core registers
    \param[out] none
    \retval     none
*/
static inline void usb_core_reset (void)
{
    uint32_t count = 0U;

    /* Core Soft Reset */
    USBx->gr->GRSTCTL |= GRSTCTL_CSRST;

    do {
        if (++count > 200000U) {
            return;
        }
    } while ((USBx->gr->GRSTCTL & GRSTCTL_CSRST) == GRSTCTL_CSRST);
}

/*!
    \brief      read device IN endpoint interrupt flag register
    \param[in]  udev: pointer to USB device
    \param[in]  ep_num: endpoint number
    \param[out] none
    \retval     interrupt value
*/
static inline uint32_t usb_iepintr_read (uint8_t ep_num)
{
    __IO uint32_t value = 0U;
    uint32_t fifoemptymask, commonintmask;

    commonintmask = USBx->dr->DIEPINTEN;
    fifoemptymask = USBx->dr->DIEPFEINTEN;

    /* check FIFO empty interrupt enable bit */
    commonintmask |= ((fifoemptymask >> ep_num) & 0x1U) << 7U;

    value = USBx->er_in[ep_num]->DIEPINTF & commonintmask;

    return value;
}


int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));

    usb_dc_cfg.Instance = (usb_core_regs *)malloc(sizeof(usb_core_regs));

    USBx = usb_dc_cfg.Instance;

    USBx->gr = (usb_gr *)(USB_BASE + USB_REG_OFFSET_CORE);
    USBx->dr = (usb_dr *)(USB_BASE + USB_REG_OFFSET_DEV);

    USBx->PWRCLKCTL = (uint32_t *)(USB_BASE + USB_REG_OFFSET_PWRCLKCTL);

    /* assign device endpoint registers address */
    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        USBx->er_in[i] = (usb_erin *)(USB_BASE + USB_REG_OFFSET_EP_IN + (i * USB_REG_OFFSET_EP));
        USBx->er_out[i] = (usb_erout *)(USB_BASE + USB_REG_OFFSET_EP_OUT + (i * USB_REG_OFFSET_EP));

        USBx->DFIFO[i] = (uint32_t *)(USB_BASE + USB_DATA_FIFO_OFFSET + (i * USB_DATA_FIFO_SIZE));
    }

    usb_dc_low_level_init();

    USBx->dr->DCTL |= DCTL_SD;

    USBx->gr->GAHBCS &= ~GAHBCS_GINTEN;

#if defined(USE_USB_HS)
    USBx->gr->GUSBCS |= GUSBCS_EMBPHY_HS;
#else
    /* Select FS Embedded PHY */
    USBx->gr->GUSBCS |= GUSBCS_EMBPHY_FS;
#endif /* USE_USB_HS */

    /* soft reset the core */
    usb_core_reset ();

    USBx->gr->GCCFG = 0U;

#ifdef VBUS_SENSING_ENABLED
    /* active the transceiver and enable VBUS sensing */
    USBx->gr->GCCFG |=  GCCFG_VDEN | GCCFG_PWRON;
#else
    USBx->gr->GOTGCS |= GOTGCS_BVOV | GOTGCS_BVOE;

    /* active the transceiver */
    USBx->gr->GCCFG |= GCCFG_PWRON;
#endif /* VBUS_SENSING_ENABLED */

#if (USB_SOF_OUTPUT == 1)
    USBx->gr->GCCFG |= GCCFG_SOFOEN;
#endif /* USB_SOF_OUTPUT */

    usb_mdelay(20U);

#ifdef USB_INTERNAL_DMA_ENABLED
    USBx->gr->GAHBCS &= ~GAHBCS_BURST;
    USBx->gr->GAHBCS |= DMA_INCR8 | GAHBCS_DMAEN;
#endif /* USB_INTERNAL_DMA_ENABLED */

    /* Force Device Mode*/
    USBx->gr->GUSBCS &= ~(GUSBCS_FDM | GUSBCS_FHM);
    USBx->gr->GUSBCS |= GUSBCS_FDM;

    for (uint8_t i = 0U; i < 16U; i++) {
        USBx->gr->DIEPTFLEN[i] = 0U;
    }

    /* Restart the Phy Clock */
    *USBx->PWRCLKCTL = 0U;

    /* Device mode configuration */
    USBx->dr->DCFG &= ~DCFG_EOPFT;
    USBx->dr->DCFG |= FRAME_INTERVAL_80;

#if defined(USE_USB_HS)
    /* Set Core speed to High speed mode */
    USBx->dr->DCFG |= USB_SPEED_INP_HIGH;
#else
    USBx->dr->DCFG |= USB_SPEED_INP_FULL;
#endif

    /* Clear all pending Device Interrupts */
    USBx->dr->DIEPINTEN = 0U;
    USBx->dr->DOEPINTEN = 0U;
    USBx->dr->DAEPINT = 0xFFFFFFFFU;
    USBx->dr->DAEPINTEN = 0U;

    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if ((USBx->er_in[i]->DIEPCTL & DEPCTL_EPEN) == DEPCTL_EPEN) {
            USBx->er_in[i]->DIEPCTL = DEPCTL_EPD | DEPCTL_SNAK;
        } else {
            USBx->er_in[i]->DIEPCTL = 0U;
        }

        USBx->er_in[i]->DIEPLEN = 0U;
        USBx->er_in[i]->DIEPINTF = 0xFFU;
    }
    for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        if ((USBx->er_out[i]->DOEPCTL & DEPCTL_EPEN) == DEPCTL_EPEN) {
            USBx->er_out[i]->DOEPCTL = DEPCTL_EPD | DEPCTL_SNAK;
        } else {
            USBx->er_out[i]->DOEPCTL = 0U;
        }

        USBx->er_out[i]->DOEPLEN = 0U;
        USBx->er_out[i]->DOEPINTF = 0xFFU;
    }

    USBx->dr->DIEPINTEN |= DIEPINTEN_EPTXFUDEN;

    /* Disable all interrupts. */
    USBx->gr->GINTEN = 0U;

    USBx->gr->GOTGINTF = 0xFFFFFFFFU;

    /* Clear any pending interrupts */
    USBx->gr->GINTF = 0xBFFFFFFFU;

#ifndef USB_INTERNAL_DMA_ENABLED
    USBx->gr->GINTEN |= GINTEN_RXFNEIE;
#endif /* !USB_INTERNAL_DMA_ENABLED */

    /* Enable interrupts matching to the Device mode ONLY */
    USBx->gr->GINTEN |= GINTEN_SPIE | GINTEN_RSTIE | GINTEN_ENUMFIE |
                     GINTEN_OEPIE | GINTEN_IEPIE | GINTEN_WKUPIE;

    USBx->gr->GRFLEN = (CONFIG_USB_RX_FIFO_SIZE / 4);

    usb_set_txfifo(0, CONFIG_USB_TX0_FIFO_SIZE / 4);
    usb_set_txfifo(1, CONFIG_USB_TX1_FIFO_SIZE / 4);
    usb_set_txfifo(2, CONFIG_USB_TX2_FIFO_SIZE / 4);
    usb_set_txfifo(3, CONFIG_USB_TX3_FIFO_SIZE / 4);
    usb_set_txfifo(4, CONFIG_USB_TX4_FIFO_SIZE / 4);
    usb_set_txfifo(5, CONFIG_USB_TX5_FIFO_SIZE / 4);
    usb_set_txfifo(6, CONFIG_USB_TX6_FIFO_SIZE / 4);
    usb_set_txfifo(7, CONFIG_USB_TX7_FIFO_SIZE / 4);

    usb_flush_txfifo(0x10U);
    usb_flush_rxfifo();

    USBx->gr->GAHBCS |= GAHBCS_GINTEN;
    USBx->dr->DCTL &= ~DCTL_SD;

    return 0;
}

int usb_dc_deinit(void)
{
    USBx = usb_dc_cfg.Instance;

    usb_dc_low_level_deinit();
    /* Clear Pending interrupt */
    for (uint8_t i = 0U; i < 15U; i++) {
        USBx->er_in[i]->DIEPINTF = 0xFB7FU;
        USBx->er_out[i]->DOEPINTF = 0xFB7FU;
    }

    /* Clear interrupt masks */
    USBx->dr->DIEPINTEN = 0U;
    USBx->dr->DOEPINTEN = 0U;
    USBx->dr->DAEPINTEN = 0U;

    /* Flush the FIFO */
    usb_flush_txfifo(0x10U);
    usb_flush_rxfifo();

    USBx->dr->DCTL |= DCTL_SD;

    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    USBx = usb_dc_cfg.Instance;
    USBx->dr->DCFG &= ~(DCFG_DAR);
    USBx->dr->DCFG |= ((uint32_t)addr << 4) & DCFG_DAR;
    return 0;
}

uint8_t usbd_get_port_speed(const uint8_t port)
{
    uint8_t speed;
    USBx = usb_dc_cfg.Instance;
    uint32_t DevEnumSpeed = (USBx->dr->DSTAT & DSTAT_ES) >> 1;

    if (DevEnumSpeed == 0U) {
        speed = USB_SPEED_HIGH;
    } else if (DevEnumSpeed == 1U) {
        speed = USB_SPEED_FULL;
    } else {
        speed = USB_SPEED_FULL;
    }

    return speed;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (!ep_cfg) {
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USBx->dr->DAEPINTEN |= DAEPINT_OEPITB & (uint32_t)(1UL << (16 + ep_idx));

        USBx->er_out[ep_idx]->DOEPCTL |= (ep_cfg->ep_mps & DEPCTL_MPL) |
                                       ((uint32_t)ep_cfg->ep_type << 18) |
                                       DEPCTL_SD0PID | DEPCTL_EPACT;
        /* EP enable */
        USBx->er_out[ep_idx]->DOEPCTL |= (DEPCTL_CNAK | DEPCTL_EPEN);
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;

        USBx->dr->DAEPINTEN |= DAEPINT_IEPITB & (uint32_t)(1UL << ep_idx);

        USBx->er_in[ep_idx]->DIEPCTL |= (ep_cfg->ep_mps & DEPCTL_MPL) |
                                      ((uint32_t)ep_cfg->ep_type << 18) | (ep_idx << 22) |
                                      DEPCTL_SD0PID | DEPCTL_EPACT;
    }
    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    USBx = usb_dc_cfg.Instance;
    volatile uint32_t count = 0U;

    if (USB_EP_DIR_IS_OUT(ep)) {
        if (USBx->er_out[ep_idx]->DOEPCTL & DEPCTL_EPEN) {
            USBx->er_out[ep_idx]->DOEPCTL |= DEPCTL_SNAK;
            USBx->er_out[ep_idx]->DOEPCTL |= DEPCTL_EPD;

            /* Wait for endpoint disabled interrupt */
            count = 0;
            do {
                if (++count > 50000) {
                    break;
                }
            } while ((USBx->er_out[ep_idx]->DOEPINTF & DOEPINTF_EPDIS) != DOEPINTF_EPDIS);

            /* Clear and unmask endpoint disabled interrupt */
            USBx->er_out[ep_idx]->DOEPINTF |= DOEPINTF_EPDIS;
        }

        USBx->dr->DEP1INTEN &= ~(DAEPINTEN_OEPIE & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USBx->dr->DAEPINTEN &= ~(DAEPINTEN_OEPIE & ((uint32_t)(1UL << (ep_idx & 0x07)) << 16));
        USBx->er_out[ep_idx]->DOEPCTL = 0;
    } else {
        if (USBx->er_in[ep_idx]->DIEPCTL & DEPCTL_EPEN) {
            USBx->er_in[ep_idx]->DIEPCTL |= DEPCTL_SNAK;
            USBx->er_in[ep_idx]->DIEPCTL |= DEPCTL_EPD;

            /* Wait for endpoint disabled interrupt */
            count = 0;
            do {
                if (++count > 50000) {
                    break;
                }
            } while ((USBx->er_in[ep_idx]->DIEPINTF & DIEPINTF_EPDIS) != DIEPINTF_EPDIS);

            /* Clear and unmask endpoint disabled interrupt */
            USBx->er_in[ep_idx]->DIEPINTF |= DIEPINTF_EPDIS;
        }
        
        USBx->dr->DEP1INTEN &= ~(DAEPINTEN_IEPIE & (uint32_t)(1UL << (ep_idx & 0x07)));
        USBx->dr->DAEPINTEN &= ~(DAEPINTEN_IEPIE & (uint32_t)(1UL << (ep_idx & 0x07)));
        USBx->er_in[ep_idx]->DIEPCTL = 0;
    }

    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USBx->er_out[ep_idx]->DOEPCTL |= DEPCTL_STALL;
    } else {
        USBx->er_in[ep_idx]->DIEPCTL |= DEPCTL_STALL;
    }

#ifdef USB_INTERNAL_DMA_ENABLED
    if(ep_idx == 0U) {
        usb_ctlep_startout((uint8_t *)&usb_dc_cfg.setup);
    }
#endif /* USB_INTERNAL_DMA_ENABLED */

    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
    USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        USBx->er_out[ep_idx]->DOEPCTL &= ~DEPCTL_STALL;
    } else {
        USBx->er_in[ep_idx]->DIEPCTL &= ~DEPCTL_STALL;
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
    USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t packet_cnt = 0U;

    if (!data && data_len) {
        return -1;
    }

    usb_dc_cfg.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    usb_dc_cfg.in_ep[ep_idx].xfer_len = data_len;
    usb_dc_cfg.in_ep[ep_idx].actual_xfer_len = 0;

    USBx->er_in[ep_idx]->DIEPLEN &= ~(DEPLEN_PCNT);
    USBx->er_in[ep_idx]->DIEPLEN &= ~(DEPLEN_TLEN);

    if (!data_len) {
        USBx->er_in[ep_idx]->DIEPLEN |= (DEPLEN_PCNT & (1U << 19));
        /* EP enable, IN data in FIFO */
        USBx->er_in[ep_idx]->DIEPCTL |= (DEPCTL_CNAK | DEPCTL_EPEN);

        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
            data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
        }

        usb_dc_cfg.in_ep[ep_idx].xfer_len = data_len;
        USBx->er_in[ep_idx]->DIEPLEN |= (DEPLEN_PCNT & (1U << 19));
        USBx->er_in[ep_idx]->DIEPLEN |= (DEPLEN_TLEN & data_len);
    } else {
        packet_cnt = (data_len + usb_dc_cfg.in_ep[ep_idx].ep_mps - 1U) / usb_dc_cfg.in_ep[ep_idx].ep_mps;

        USBx->er_in[ep_idx]->DIEPLEN |= (DEPLEN_PCNT & (packet_cnt << 19));
        USBx->er_in[ep_idx]->DIEPLEN |= (DEPLEN_TLEN & data_len);
    }

    if (usb_dc_cfg.in_ep[ep_idx].ep_type == USB_EPTYPE_ISOC) {
        USBx->er_in[ep_idx]->DIEPLEN &= ~(DIEPLEN_MCNT);
        USBx->er_in[ep_idx]->DIEPLEN |= (DIEPLEN_MCNT & (1U << 29));

        if ((USBx->dr->DSTAT & (1U << 8)) == 0U) {
            USBx->er_in[ep_idx]->DIEPCTL |= DEPCTL_SODDFRM;
        } else {
            USBx->er_in[ep_idx]->DIEPCTL |= DEPCTL_SEVNFRM;
        }
    } 

#ifdef USB_INTERNAL_DMA_ENABLED
    USBx->er_in[ep_idx]->DIEPDMAADDR = (uint32_t)data;

    USBx->er_in[ep_idx]->DIEPCTL |= (DEPCTL_CNAK | DEPCTL_EPEN);
#else
    USBx->er_in[ep_idx]->DIEPCTL |= (DEPCTL_CNAK | DEPCTL_EPEN);

    if (data_len > 0U) {
        USBx->dr->DIEPFEINTEN |= 1UL << (ep_idx & 0x0F);
    }
#endif /* USB_INTERNAL_DMA_ENABLED */

    return 0;
}

int usbd_ep_start_read(const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t pktcnt;

    if (!data && data_len) {
        return -1;
    }

    usb_dc_cfg.out_ep[ep_idx].xfer_buf = data;
    usb_dc_cfg.out_ep[ep_idx].xfer_len = data_len;
    usb_dc_cfg.out_ep[ep_idx].actual_xfer_len = 0U;

    USBx->er_out[ep_idx]->DOEPLEN &= ~(DEPLEN_PCNT);
    USBx->er_out[ep_idx]->DOEPLEN &= ~(DEPLEN_TLEN);

    if (!data_len) {
        USBx->er_out[ep_idx]->DOEPLEN |= (DEPLEN_PCNT & (1 << 19));
        USBx->er_out[ep_idx]->DOEPLEN |= (DEPLEN_TLEN & usb_dc_cfg.out_ep[ep_idx].ep_mps);

        /* EP enable */
        USBx->er_out[ep_idx]->DOEPCTL |= (DEPCTL_CNAK | DEPCTL_EPEN);

        return 0;
    }

    if (ep_idx == 0) {
        if (data_len > usb_dc_cfg.out_ep[ep_idx].ep_mps) {
            data_len = usb_dc_cfg.out_ep[ep_idx].ep_mps;
        }

        usb_dc_cfg.out_ep[ep_idx].xfer_len = data_len;

        USBx->er_out[ep_idx]->DOEPLEN |= (DEPLEN_PCNT & (1 << 19));
        USBx->er_out[ep_idx]->DOEPLEN |= (DEPLEN_TLEN & data_len);
    } else {
        pktcnt = (uint16_t)((data_len + usb_dc_cfg.out_ep[ep_idx].ep_mps - 1U) / usb_dc_cfg.out_ep[ep_idx].ep_mps);

        USBx->er_out[ep_idx]->DOEPLEN |= (DEPLEN_PCNT & (pktcnt << 19));
        USBx->er_out[ep_idx]->DOEPLEN |= (DEPLEN_TLEN & data_len);
    }

#ifdef USB_INTERNAL_DMA_ENABLED
    USBx->er_out[ep_idx]->DOEPDMAADDR = (uint32_t)data;
#endif /* USB_INTERNAL_DMA_ENABLED */

    /* EP enable */
    USBx->er_out[ep_idx]->DOEPCTL |= (DEPCTL_CNAK | DEPCTL_EPEN);

    return 0;
}

void USBD_IRQHandler(void)
{
    USBx = usb_dc_cfg.Instance;
    __IO uint32_t gint_status, temp, epnum, ep_intr, epint, read_count;
    gint_status = USBx->gr->GINTF;

    if ((gint_status & 0x1U) == DEVICE_MODE) {
        if (gint_status == 0) {
            return;
        }

#ifndef USB_INTERNAL_DMA_ENABLED
        /* Handle RxQLevel Interrupt */
        if (gint_status & GINTF_RXFNEIF) {

            USBx->gr->GINTEN &= ~GINTF_RXFNEIF;
            temp = USBx->gr->GRSTATP;
            usb_dc_cfg.grxstsp = temp;
            epnum = temp & GRSTATRP_EPNUM;
            if (((temp & GRSTATRP_RPCKST) >> 17) == RSTAT_DATA_UPDT) {
                read_count = (temp & GRSTATRP_BCOUNT) >> 4;
                if (read_count != 0U) {
                    usb_rxfifo_read(usb_dc_cfg.out_ep[epnum].xfer_buf, read_count);
                    usb_dc_cfg.out_ep[epnum].xfer_buf += read_count;
                }
            } else if (((temp & GRSTATRP_RPCKST) >> 17) == RSTAT_SETUP_UPDT) {
                read_count = (temp & GRSTATRP_BCOUNT) >> 4;
                usb_rxfifo_read((uint8_t *)&usb_dc_cfg.setup, read_count);
            } else {
                /* ... */
            }

            USBx->gr->GINTEN |= GINTF_RXFNEIF;
        }
#endif

        if (gint_status & GINTF_OEPIF) {
            epnum = 0;
            /* Read in the device interrupt bits */
            ep_intr = USBx->dr->DAEPINT;
            ep_intr &= USBx->dr->DAEPINTEN;
            ep_intr >>= 16;

            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = USBx->er_out[(uint32_t)epnum]->DOEPINTF;

                    epint &= USBx->dr->DOEPINTEN;

                    if ((epint & DOEPINTF_TF) == DOEPINTF_TF) {
                        USBx->er_out[epnum]->DOEPINTF = DOEPINTF_TF;

                        if (epnum == 0) {
                            if (usb_dc_cfg.out_ep[epnum].xfer_len == 0U) {
                                usb_ctlep_startout((uint8_t *)&usb_dc_cfg.setup);
                            } else {
                                if ((usb_dc_cfg.out_ep[epnum].xfer_len & DEPLEN_PCNT) == 0U) {
                                    usb_dc_cfg.out_ep[epnum].actual_xfer_len = usb_dc_cfg.out_ep[epnum].xfer_len;
                                } else {
                                    usb_dc_cfg.out_ep[epnum].actual_xfer_len = usb_dc_cfg.out_ep[epnum].xfer_len - ((USBx->er_out[epnum]->DOEPLEN) & DEPLEN_TLEN);
                                }
                                usb_dc_cfg.out_ep[epnum].xfer_len = 0;
                                usbd_event_ep_out_complete_handler(0x00, usb_dc_cfg.out_ep[epnum].actual_xfer_len);
                            }
                        } else {
//                            if ((usb_dc_cfg.out_ep[epnum].xfer_len & DEPLEN_PCNT) == 0U) {
//                                usb_dc_cfg.out_ep[epnum].actual_xfer_len = usb_dc_cfg.out_ep[epnum].xfer_len;
//                            } else {
                                usb_dc_cfg.out_ep[epnum].actual_xfer_len = usb_dc_cfg.out_ep[epnum].xfer_len - ((USBx->er_out[epnum]->DOEPLEN) & DEPLEN_TLEN);
//                            }
                            usb_dc_cfg.out_ep[epnum].xfer_len = 0;
                            usbd_event_ep_out_complete_handler(epnum, usb_dc_cfg.out_ep[epnum].actual_xfer_len);
                        }
                    }

                    if ((epint & DOEPINTF_STPF) == DOEPINTF_STPF) {
                        USBx->er_out[epnum]->DOEPINTF = DOEPINTF_STPF;

                        usbd_event_ep0_setup_complete_handler((uint8_t *)&usb_dc_cfg.setup);
                    }
                }
                ep_intr >>= 1U;
                epnum++;
            }
        }
        if (gint_status & GINTF_IEPIF) {
            epnum = 0U;
            ep_intr = USBx->dr->DAEPINT & 0xFFFF;
            ep_intr &= USBx->dr->DAEPINTEN;
            while (ep_intr != 0U) {
                if ((ep_intr & 0x1U) != 0U) {
                    epint = usb_iepintr_read(epnum);

                    if ((epint & DIEPINTF_TF) == DIEPINTF_TF) {
                        USBx->er_in[epnum]->DIEPINTF = DIEPINTF_TF;

                        if ((USBx->er_in[epnum]->DIEPLEN & DEPLEN_PCNT) == 0) {
                            usb_dc_cfg.in_ep[epnum].actual_xfer_len = usb_dc_cfg.in_ep[epnum].xfer_len;
                        } else {
                            usb_dc_cfg.in_ep[epnum].actual_xfer_len = usb_dc_cfg.in_ep[epnum].xfer_len - (USBx->er_in[epnum]->DIEPLEN & DEPLEN_TLEN);
                        }
                        usb_dc_cfg.in_ep[epnum].xfer_len = 0;

                        if (epnum == 0) {
                            usbd_event_ep_in_complete_handler(0x80, usb_dc_cfg.in_ep[epnum].actual_xfer_len);

                            if (usb_dc_cfg.setup.wLength && ((usb_dc_cfg.setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                                /* In status, start reading setup */
                                usb_ctlep_startout((uint8_t *)&usb_dc_cfg.setup);
                            } else if (usb_dc_cfg.setup.wLength == 0) {
                                /* In status, start reading setup */
                                usb_ctlep_startout((uint8_t *)&usb_dc_cfg.setup);
                            }
                        } else {
                            usbd_event_ep_in_complete_handler(epnum | 0x80, usb_dc_cfg.in_ep[epnum].actual_xfer_len);
                        }
                    }

                    if ((epint & DIEPINTF_TXFE) == DIEPINTF_TXFE) {
                        usbd_emptytxfifo_write ((uint32_t)epnum);

                        USBx->er_in[epnum]->DIEPINTF = DIEPINTF_TXFE;
                    }
                }
                ep_intr >>= 1U;
                epnum++;
            }
        }
        if (gint_status & GINTF_RST) {
            USBx->gr->GINTF |= GINTF_RST;
            USBx->dr->DCTL &= ~DCTL_RWKUP;

            usb_flush_txfifo(0U);

            for (uint8_t i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
                USBx->er_in[i]->DIEPINTF = 0xFFU;
                USBx->er_out[i]->DOEPINTF = 0xFFU;
            }

            /* clear all pending device endpoint interrupts */
            USBx->dr->DAEPINT = 0xFFFFFFFFU;
            USBx->dr->DAEPINTEN |= 0x10001U;
            USBx->dr->DOEPINTEN = DOEPINTEN_STPFEN | DOEPINTEN_TFEN;
            USBx->dr->DIEPINTEN = DIEPINTEN_TFEN;

            usbd_event_reset_handler();

            usb_ctlep_startout((uint8_t *)&usb_dc_cfg.setup);
        }
        if (gint_status & GINTF_ENUMFIF) {
            USBx->gr->GINTF |= GINTF_ENUMFIF;

            /* Set the MPS of the IN EP0 to 64 bytes */
            USBx->er_in[0U]->DIEPCTL &= ~DEPCTL_MPL;

            USBx->dr->DCTL &= ~DCTL_CGINAK;
            USBx->dr->DCTL |= DCTL_CGINAK;

            USBx->gr->GUSBCS &= ~GUSBCS_UTT;

            uint8_t enum_speed = usbd_get_port_speed(0);

            if (USB_SPEED_HIGH == enum_speed) {
                USBx->gr->GUSBCS |= 0x09U << 10U;
            } else {
                USBx->gr->GUSBCS |= 0x05U << 10U;
            }
        }
        if (gint_status & GINTF_SOF) {
            USBx->gr->GINTF |= GINTF_SOF;
        }
        if (gint_status & GINTF_SP) {
            USBx->gr->GINTF |= GINTF_SP;
        }
    }
}

#ifdef USB_DEDICATED_EP1_ENABLED

/*!
    \brief      USB dedicated OUT endpoint 1 interrupt service routine handler
    \param[in]  udev: pointer to USB device instance
    \param[out] none
    \retval     operation status
*/
uint32_t usbd_int_dedicated_ep1out (void)
{
    uint32_t oepintr = 0U;

    USBx = usb_dc_cfg.Instance;

    oepintr = USBx->er_out[1]->DOEPINTF;
    oepintr &= USBx->dr->DOEP1INTEN;

    /* transfer complete */
    if (oepintr & DOEPINTF_TF) {
        /* clear the bit in DOEPINTn for this interrupt */
        USBx->er_out[1]->DOEPINTF = DOEPINTF_TF;

        /* ToDo : handle more than one single MPS size packet */
        usb_dc_cfg.out_ep[1].actual_xfer_len = usb_dc_cfg.out_ep[1].xfer_len - ((USBx->er_out[1]->DOEPLEN) & DEPLEN_TLEN);
        usb_dc_cfg.out_ep[1].xfer_len = 0;
        usbd_event_ep_out_complete_handler(EP1_OUT, usb_dc_cfg.out_ep[1].actual_xfer_len);
    }

    return 1U;
}

/*!
    \brief      USB dedicated IN endpoint 1 interrupt service routine handler
    \param[in]  udev: pointer to USB device instance
    \param[out] none
    \retval     operation status
*/
uint32_t usbd_int_dedicated_ep1in (void)
{
    uint32_t inten, intr, emptyen;

    USBx = usb_dc_cfg.Instance;

    inten = USBx->dr->DIEP1INTEN;
    emptyen = USBx->dr->DIEPFEINTEN;

    inten |= ((emptyen >> 1U ) & 0x1U) << 7U;

    intr = USBx->er_in[1]->DIEPINTF & inten;

    if (intr & DIEPINTF_TF) {
        USBx->dr->DIEPFEINTEN &= ~(0x1U << 1U);
        USBx->er_in[1]->DIEPINTF = DIEPINTF_TF;

        usb_dc_cfg.in_ep[1].actual_xfer_len = usb_dc_cfg.in_ep[1].xfer_len - (USBx->er_in[1]->DIEPLEN & DEPLEN_TLEN);
        usb_dc_cfg.in_ep[1].xfer_len = 0;
        usbd_event_ep_in_complete_handler(EP1_IN, usb_dc_cfg.in_ep[1].actual_xfer_len);
    }

    if (intr & DIEPINTF_TXFE) {
        usbd_emptytxfifo_write ((uint32_t)0x01UL);

        USBx->er_in[1]->DIEPINTF = DIEPINTF_TXFE;
    }

    return 1U;
}

#endif

static int usb_flush_rxfifo(void)
{
    uint32_t count = 0;

    USBx->gr->GRSTCTL = GRSTCTL_RXFF;

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USBx->gr->GRSTCTL & GRSTCTL_RXFF) == GRSTCTL_RXFF);

    return 0;
}

static int usb_flush_txfifo(uint32_t num)
{
    uint32_t count = 0U;

    USBx->gr->GRSTCTL = (GRSTCTL_TXFF | (num << 6));

    do {
        if (++count > 200000U) {
            return -1;
        }
    } while ((USBx->gr->GRSTCTL & GRSTCTL_TXFF) == GRSTCTL_TXFF);

    return 0;
}

static void usb_set_txfifo(uint8_t fifo, uint16_t size)
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

    Tx_Offset = USBx->gr->GRFLEN;

    if (fifo == 0U) {
        USBx->gr->DIEP0TFLEN_HNPTFLEN = ((uint32_t)size << 16) | Tx_Offset;
    } else {
        Tx_Offset += (USBx->gr->DIEP0TFLEN_HNPTFLEN) >> 16;
        for (i = 0U; i < (fifo - 1U); i++) {
            Tx_Offset += (USBx->gr->DIEPTFLEN[i] >> 16);
        }

        /* Multiply Tx_Size by 2 to get higher performance */
        USBx->gr->DIEPTFLEN[fifo - 1U] = ((uint32_t)size << 16) | Tx_Offset;
    }
}

/*!
    \brief      configures OUT endpoint 0 to receive SETUP packets
    \param[in]  udev: pointer to USB device
    \param[out] none
    \retval     none
*/
static void usb_ctlep_startout (uint8_t *psetup)
{
    /* set OUT endpoint 0 receive length to 24 bytes, 1 packet and 3 setup packets */
    USBx->er_out[0]->DOEPLEN = DOEP0_TLEN(8U * 3U) | DOEP0_PCNT(1U) | DOEP0_STPCNT(3U);

#ifdef USB_INTERNAL_DMA_ENABLED
    USBx->er_out[0]->DOEPDMAADDR = (uint32_t)psetup;

    /* endpoint enable */
    USBx->er_out[0]->DOEPCTL |= DEPCTL_EPACT | DEPCTL_EPEN;
#endif
}

/*!
    \brief      write a packet into the TX FIFO associated with the endpoint
    \param[in]  usb_regs: pointer to USB core registers
    \param[in]  src_buf: pointer to source buffer
    \param[in]  fifo_num: FIFO number which is in (0..5)
    \param[in]  byte_count: packet byte count
    \param[out] none
    \retval     operation status
*/
static int usb_txfifo_write (uint8_t *src_buf, uint8_t  fifo_num, uint16_t byte_count)
{
    uint32_t word_count = (byte_count + 3U) / 4U;
    __IO uint32_t *fifo = USBx->DFIFO[fifo_num];

    while (word_count-- > 0U) {
        *fifo = *((__IO uint32_t *)src_buf);

        src_buf += 4U;
    }

    return 0;
}

/*!
    \brief      read a packet from the Rx FIFO associated with the endpoint
    \param[in]  usb_regs: pointer to USB core registers
    \param[in]  dest_buf: pointer to destination buffer
    \param[in]  byte_count: packet byte count
    \param[out] none
    \retval     void type pointer
*/
static void *usb_rxfifo_read (uint8_t *dest_buf, uint16_t byte_count)
{
    __IO uint32_t word_count = (byte_count + 3U) / 4U;
    __IO uint32_t *fifo = USBx->DFIFO[0];

    while (word_count-- > 0U) {
        *(__IO uint32_t *)dest_buf = *fifo;

        dest_buf += 4U;
    }

    return ((void *)dest_buf);
}

/*!
    \brief      check FIFO for the next packet to be loaded
    \param[in]  udev: pointer to USB device instance
    \param[in]  ep_num: endpoint identifier which is in (0..5)
    \param[out] none
    \retval     status
*/
static uint32_t usbd_emptytxfifo_write (uint32_t ep_num)
{
    uint32_t len;
    uint32_t word_count;

    len = usb_dc_cfg.in_ep[ep_num].xfer_len - usb_dc_cfg.in_ep[ep_num].actual_xfer_len;

    /* get the data length to write */
    if (len > usb_dc_cfg.in_ep[ep_num].ep_mps) {
        len = usb_dc_cfg.in_ep[ep_num].ep_mps;
    }

    word_count = (len + 3U) / 4U;

    while (((USBx->er_in[ep_num]->DIEPTFSTAT & DIEPTFSTAT_IEPTFS) >= word_count) && \
              (usb_dc_cfg.in_ep[ep_num].actual_xfer_len < usb_dc_cfg.in_ep[ep_num].xfer_len) && (usb_dc_cfg.in_ep[ep_num].xfer_len != 0)) {
        len = usb_dc_cfg.in_ep[ep_num].xfer_len - usb_dc_cfg.in_ep[ep_num].actual_xfer_len;

        if (len > usb_dc_cfg.in_ep[ep_num].ep_mps) {
            len = usb_dc_cfg.in_ep[ep_num].ep_mps;
        }

        /* write FIFO in word(4bytes) */
        word_count = (len + 3U) / 4U;

        /* write the FIFO */
        (void)usb_txfifo_write (usb_dc_cfg.in_ep[ep_num].xfer_buf, (uint8_t)ep_num, (uint16_t)len);

        usb_dc_cfg.in_ep[ep_num].xfer_buf += len;
        usb_dc_cfg.in_ep[ep_num].actual_xfer_len += len;

        if (usb_dc_cfg.in_ep[ep_num].actual_xfer_len == usb_dc_cfg.in_ep[ep_num].xfer_len) {
            /* disable the device endpoint FIFO empty interrupt */
            USBx->dr->DIEPFEINTEN &= ~(0x01U << ep_num);
        }
    }

    return 1U;
}
