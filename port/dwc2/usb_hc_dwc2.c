#include "usbh_core.h"
#include "usb_dwc2_reg.h"

#ifndef CONFIG_USBHOST_HIGH_WORKQ
#error "dwc2 host must use high workq"
#endif

#ifndef USBH_IRQHandler
#define USBH_IRQHandler OTG_HS_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE (0x40040000UL)
#endif

#ifndef CONFIG_USB_DWC2_PIPE_NUM
#define CONFIG_USB_DWC2_PIPE_NUM 12 /* Number of host channels */
#endif

#define USB_OTG_GLB     ((USB_OTG_GlobalTypeDef *)(USB_BASE))
#define USB_OTG_PCGCCTL *(__IO uint32_t *)((uint32_t)USB_BASE + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_HPRT    *(__IO uint32_t *)((uint32_t)USB_BASE + USB_OTG_HOST_PORT_BASE)
#define USB_OTG_HOST    ((USB_OTG_HostTypeDef *)(USB_BASE + USB_OTG_HOST_BASE))
#define USB_OTG_HC(i)   ((USB_OTG_HostChannelTypeDef *)(USB_BASE + USB_OTG_HOST_CHANNEL_BASE + ((i)*USB_OTG_HOST_CHANNEL_SIZE)))
#define USB_OTG_FIFO(i) *(__IO uint32_t *)(USB_BASE + USB_OTG_FIFO_BASE + ((i)*USB_OTG_FIFO_SIZE))

/* This structure retains the state of one host channel.  NOTE: Since there
 * is only one channel operation active at a time, some of the fields in
 * in the structure could be moved in struct stm32_ubhost_s to achieve
 * some memory savings.
 */

struct dwc2_pipe {
    uint8_t ep_addr;
    uint8_t ep_type;
    uint16_t ep_mps;
    uint8_t speed;
    uint8_t dev_addr;
    uint8_t data_pid;
    uint8_t chidx;
    bool inuse;               /* True: This channel is "in use" */
    uint8_t ep_interval;      /* Interrupt/isochronous EP polling interval */
    uint16_t num_packets;     /* for HCTSIZx*/
    uint32_t xferlen;         /* for HCTSIZx*/
    volatile int result;      /* The result of the transfer */
    volatile uint32_t xfrd;   /* Bytes transferred (at end of transfer) */
    volatile bool waiter;     /* True: Thread is waiting for a channel event */
    usb_osal_sem_t waitsem;   /* Channel wait semaphore */
    usb_osal_mutex_t exclsem; /* Support mutually exclusive access */
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback; /* Transfer complete callback */
    void *arg;                       /* Argument that accompanies the callback */
#endif
};

struct dwc2_hcd {
    volatile bool connected;
    struct usb_work work;
    struct dwc2_pipe chan[CONFIG_USB_DWC2_PIPE_NUM];
} g_dwc2_hcd;

#ifdef CONFIG_USB_DCACHE_ENABLE
void usb_dwc2_dcache_clean(uintptr_t addr, uint32_t len);
void usb_dwc2_dcache_invalidate(uintptr_t addr, uint32_t len);
#else
#define usb_dwc2_dcache_clean(addr, len)
#define usb_dwc2_dcache_invalidate(addr, len)
#endif

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
#if defined(CONFIG_USB_DWC2_ULPI_PHY)
    USB_OTG_GLB->GCCFG &= ~(USB_OTG_GCCFG_PWRDWN);
    /* Init The ULPI Interface */
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_TSDPS | USB_OTG_GUSBCFG_ULPIFSLS | USB_OTG_GUSBCFG_PHYSEL);

    /* Select vbus source */
    USB_OTG_GLB->GUSBCFG &= ~(USB_OTG_GUSBCFG_ULPIEVBUSD | USB_OTG_GUSBCFG_ULPIEVBUSI);

    //USB_OTG_GLB->GUSBCFG |= USB_OTG_GUSBCFG_ULPIEVBUSD;
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

static inline void dwc2_drivebus(uint8_t state)
{
    __IO uint32_t hprt0 = 0U;

    hprt0 = USB_OTG_HPRT;

    hprt0 &= ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
               USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);

    if (((hprt0 & USB_OTG_HPRT_PPWR) == 0U) && (state == 1U)) {
        USB_OTG_HPRT = (USB_OTG_HPRT_PPWR | hprt0);
    }
    if (((hprt0 & USB_OTG_HPRT_PPWR) == USB_OTG_HPRT_PPWR) && (state == 0U)) {
        USB_OTG_HPRT = ((~USB_OTG_HPRT_PPWR) & hprt0);
    }
}

static void dwc2_pipe_init(uint8_t ch_num, uint8_t devaddr, uint8_t ep_addr, uint8_t ep_type, uint8_t ep_mps, uint8_t speed)
{
    uint32_t regval;

    /* Clear old interrupt conditions for this host channel. */
    USB_OTG_HC((uint32_t)ch_num)->HCINT = 0xFFFFFFFFU;

    /* Enable channel interrupts required for this transfer. */
    regval = USB_OTG_HCINTMSK_XFRCM |
             USB_OTG_HCINTMSK_STALLM |
             USB_OTG_HCINTMSK_TXERRM |
             USB_OTG_HCINTMSK_DTERRM |
             USB_OTG_HCINTMSK_AHBERR;

    if ((ep_addr & 0x80U) == 0x80U) {
        regval |= USB_OTG_HCINTMSK_BBERRM;
    }

    switch (ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
        case USB_ENDPOINT_TYPE_BULK:
            //            if ((ep_addr & 0x80U) == 0x00U) {
            //                regval |= USB_OTG_HCINTMSK_NYET;
            //            }
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            regval |= USB_OTG_HCINTMSK_FRMORM;
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            regval |= USB_OTG_HCINTMSK_FRMORM;
            break;
    }

    USB_OTG_HC((uint32_t)ch_num)->HCINTMSK = regval;

    /* Enable the top level host channel interrupt. */
    USB_OTG_HOST->HAINTMSK |= 1UL << (ch_num & 0xFU);

    /* Make sure host channel interrupts are enabled. */
    USB_OTG_GLB->GINTMSK |= USB_OTG_GINTMSK_HCIM;

    /* Program the HCCHAR register */

    regval = (((uint32_t)ep_mps << USB_OTG_HCCHAR_MPSIZ_Pos) & USB_OTG_HCCHAR_MPSIZ) |
             ((((uint32_t)ep_addr & 0x7FU) << USB_OTG_HCCHAR_EPNUM_Pos) & USB_OTG_HCCHAR_EPNUM) |
             (((uint32_t)ep_type << USB_OTG_HCCHAR_EPTYP_Pos) & USB_OTG_HCCHAR_EPTYP) |
             (((uint32_t)devaddr << USB_OTG_HCCHAR_DAD_Pos) & USB_OTG_HCCHAR_DAD);

    if ((ep_addr & 0x80U) == 0x80U) {
        regval |= USB_OTG_HCCHAR_EPDIR;
    }

    /* LS device plugged to HUB */
    if (speed == USB_SPEED_LOW) {
        regval |= USB_OTG_HCCHAR_LSDEV;
    }

    if (ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
        regval |= USB_OTG_HCCHAR_ODDFRM;
    }

    USB_OTG_HC((uint32_t)ch_num)->HCCHAR = regval;
}

static uint8_t dwc2_calculate_packet_num(uint32_t input_size, uint8_t ep_addr, uint16_t ep_mps, uint32_t *output_size)
{
    uint16_t num_packets;

    num_packets = (uint16_t)((input_size + ep_mps - 1U) / ep_mps);

    if (num_packets > 256) {
        num_packets = 256;
    }

    if (input_size == 0) {
        num_packets = 1;
    }

    if (ep_addr & 0x80) {
        input_size = num_packets * ep_mps;
    } else {
    }

    *output_size = input_size;
    return num_packets;
}

/* For IN channel HCTSIZ.XferSize is expected to be an integer multiple of ep_mps size.*/
static inline void dwc2_pipe_transfer(uint8_t ch_num, uint8_t ep_addr, uint32_t *buf, uint32_t size, uint8_t num_packets, uint8_t pid)
{
    __IO uint32_t tmpreg;
    uint8_t is_oddframe;

    /* Initialize the HCTSIZn register */
    USB_OTG_HC(ch_num)->HCTSIZ = (size & USB_OTG_HCTSIZ_XFRSIZ) |
                                 (((uint32_t)num_packets << 19) & USB_OTG_HCTSIZ_PKTCNT) |
                                 (((uint32_t)pid << 29) & USB_OTG_HCTSIZ_DPID);

    /* xfer_buff MUST be 32-bits aligned */
    USB_OTG_HC(ch_num)->HCDMA = (uint32_t)buf;

    is_oddframe = (((uint32_t)USB_OTG_HOST->HFNUM & 0x01U) != 0U) ? 0U : 1U;
    USB_OTG_HC(ch_num)->HCCHAR &= ~USB_OTG_HCCHAR_ODDFRM;
    USB_OTG_HC(ch_num)->HCCHAR |= (uint32_t)is_oddframe << 29;

    /* make sure to set the correct ep direction */
    if (ep_addr & 0x80) {
        tmpreg |= USB_OTG_HCCHAR_EPDIR;
    } else {
        tmpreg &= ~USB_OTG_HCCHAR_EPDIR;
    }

    /* Set host channel enable */
    tmpreg = USB_OTG_HC(ch_num)->HCCHAR;
    tmpreg &= ~USB_OTG_HCCHAR_CHDIS;
    tmpreg |= USB_OTG_HCCHAR_CHENA;
    USB_OTG_HC(ch_num)->HCCHAR = tmpreg;
}

static void dwc2_halt(uint8_t ch_num)
{
    uint32_t count = 0U;
    uint32_t HcEpType = (USB_OTG_HC(ch_num)->HCCHAR & USB_OTG_HCCHAR_EPTYP) >> 18;
    uint32_t ChannelEna = (USB_OTG_HC(ch_num)->HCCHAR & USB_OTG_HCCHAR_CHENA) >> 31;

    if (((USB_OTG_GLB->GAHBCFG & USB_OTG_GAHBCFG_DMAEN) == USB_OTG_GAHBCFG_DMAEN) &&
        (ChannelEna == 0U)) {
        return;
    }

    /* Check for space in the request queue to issue the halt. */
    if ((HcEpType == HCCHAR_CTRL) || (HcEpType == HCCHAR_BULK)) {
        USB_OTG_HC(ch_num)->HCCHAR |= USB_OTG_HCCHAR_CHDIS;

        if ((USB_OTG_GLB->GAHBCFG & USB_OTG_GAHBCFG_DMAEN) == 0U) {
            if ((USB_OTG_GLB->HNPTXSTS & (0xFFU << 16)) == 0U) {
                USB_OTG_HC(ch_num)->HCCHAR &= ~USB_OTG_HCCHAR_CHENA;
                USB_OTG_HC(ch_num)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
                USB_OTG_HC(ch_num)->HCCHAR &= ~USB_OTG_HCCHAR_EPDIR;
                do {
                    if (++count > 1000U) {
                        break;
                    }
                } while ((USB_OTG_HC(ch_num)->HCCHAR & USB_OTG_HCCHAR_CHENA) == USB_OTG_HCCHAR_CHENA);
            } else {
                USB_OTG_HC(ch_num)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
            }
        }
    } else {
        USB_OTG_HC(ch_num)->HCCHAR |= USB_OTG_HCCHAR_CHDIS;

        if ((USB_OTG_HOST->HPTXSTS & (0xFFU << 16)) == 0U) {
            USB_OTG_HC(ch_num)->HCCHAR &= ~USB_OTG_HCCHAR_CHENA;
            USB_OTG_HC(ch_num)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
            USB_OTG_HC(ch_num)->HCCHAR &= ~USB_OTG_HCCHAR_EPDIR;
            do {
                if (++count > 1000U) {
                    break;
                }
            } while ((USB_OTG_HC(ch_num)->HCCHAR & USB_OTG_HCCHAR_CHENA) == USB_OTG_HCCHAR_CHENA);
        } else {
            USB_OTG_HC(ch_num)->HCCHAR |= USB_OTG_HCCHAR_CHENA;
        }
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

static inline uint32_t dwc2_get_current_frame(void)
{
    return (USB_OTG_HOST->HFNUM & USB_OTG_HFNUM_FRNUM);
}

/****************************************************************************
 * Name: dwc2_pipe_alloc
 *
 * Description:
 *   Allocate a channel.
 *
 ****************************************************************************/

static int dwc2_pipe_alloc(void)
{
    int chidx;

    /* Search the table of channels */

    for (chidx = 0; chidx < CONFIG_USB_DWC2_PIPE_NUM; chidx++) {
        /* Is this channel available? */
        if (!g_dwc2_hcd.chan[chidx].inuse) {
            /* Yes... make it "in use" and return the index */

            g_dwc2_hcd.chan[chidx].inuse = true;
            return chidx;
        }
    }

    /* All of the channels are "in-use" */

    return -EBUSY;
}

/****************************************************************************
 * Name: dwc2_pipe_free
 *
 * Description:
 *   Free a previoiusly allocated channel.
 *
 ****************************************************************************/

static void dwc2_pipe_free(struct dwc2_pipe *chan)
{
    /* Mark the channel available */

    chan->inuse = false;
}

/****************************************************************************
 * Name: dwc2_pipe_waitsetup
 *
 * Description:
 *   Set the request for the transfer complete event well BEFORE enabling
 *   the transfer (as soon as we are absolutely committed to the transfer).
 *   We do this to minimize race conditions.  This logic would have to be
 *   expanded if we want to have more than one packet in flight at a time!
 *
 * Assumptions:
 *   Called from a normal thread context BEFORE the transfer has been
 *   started.
 *
 ****************************************************************************/

static int dwc2_pipe_waitsetup(struct dwc2_pipe *chan)
{
    size_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();

    /* Is the device still connected? */

    if (usbh_get_port_connect_status(1)) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */
        chan->waiter = true;
        chan->result = -EBUSY;
        chan->xfrd = 0;
#ifdef CONFIG_USBHOST_ASYNCH
        chan->callback = NULL;
        chan->arg = NULL;
#endif
        ret = 0;
    }
    usb_osal_leave_critical_section(flags);
    return ret;
}

/****************************************************************************
 * Name: dwc2_pipe_asynchsetup
 *
 * Description:
 *   Set the request for the transfer complete event well BEFORE enabling
 *   the transfer (as soon as we are absolutely committed to the to avoid
 *   transfer).  We do this to minimize race conditions.  This logic would
 *   have to be expanded if we want to have more than one packet in flight
 *   at a time!
 *
 * Assumptions:
 *   Might be called from the level of an interrupt handler
 *
 ****************************************************************************/

#ifdef CONFIG_USBHOST_ASYNCH
static int dwc2_pipe_asynchsetup(struct dwc2_pipe *chan, usbh_asynch_callback_t callback, void *arg)
{
    size_t flags;
    int ret = -ENODEV;

    flags = usb_osal_enter_critical_section();
    /* Is the device still connected? */

    if (usbh_get_port_connect_status(1)) {
        /* Yes.. then set waiter to indicate that we expect to be informed
       * when either (1) the device is disconnected, or (2) the transfer
       * completed.
       */

        chan->waiter = false;
        chan->result = -EBUSY;
        chan->xfrd = 0;
        chan->callback = callback;
        chan->arg = arg;

        ret = 0;
    }

    usb_osal_leave_critical_section(flags);
    return ret;
}
#endif

/****************************************************************************
 * Name: dwc2_pipe_wait
 *
 * Description:
 *   Wait for a transfer on a channel to complete.
 *
 * Assumptions:
 *   Called from a normal thread context
 *
 ****************************************************************************/

static int dwc2_pipe_wait(struct dwc2_pipe *chan, uint32_t timeout)
{
    int ret;

    /* Loop, testing for an end of transfer condition.  The channel 'result'
   * was set to EBUSY and 'waiter' was set to true before the transfer;
   * 'waiter' will be set to false and 'result' will be set appropriately
   * when the transfer is completed.
   */

    if (chan->waiter) {
        ret = usb_osal_sem_take(chan->waitsem, timeout);
        if (ret < 0) {
            return ret;
        }
    }

    /* The transfer is complete re-enable interrupts and return the result */
    ret = chan->result;

    if (ret < 0) {
        return ret;
    }
    return chan->xfrd;
}

/****************************************************************************
 * Name: dwc2_pipe_wakeup
 *
 * Description:
 *   A channel transfer has completed... wakeup any threads waiting for the
 *   transfer to complete.
 *
 * Assumptions:
 *   This function is called from the transfer complete interrupt handler for
 *   the channel.  Interrupts are disabled.
 *
 ****************************************************************************/

static void dwc2_pipe_wakeup(struct dwc2_pipe *chan)
{
    usbh_asynch_callback_t callback;
    void *arg;
    int nbytes;

    /* Is the transfer complete? */

    if (chan->result != -EBUSY) {
        /* Is there a thread waiting for this transfer to complete? */

        if (chan->waiter) {
            /* Wake'em up! */
            chan->waiter = false;
            usb_osal_sem_give(chan->waitsem);
        }
#ifdef CONFIG_USBHOST_ASYNCH
        /* No.. is an asynchronous callback expected when the transfer
       * completes?
       */
        else if (chan->callback) {
            callback = chan->callback;
            arg = chan->arg;
            nbytes = chan->xfrd;
            chan->callback = NULL;
            chan->arg = NULL;
            if (chan->result < 0) {
                nbytes = chan->result;
            }

            callback(arg, nbytes);
        }
#endif
    }
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_sw_init(void)
{
    memset(&g_dwc2_hcd, 0, sizeof(struct dwc2_hcd));

    for (uint8_t chidx = 0; chidx < CONFIG_USB_DWC2_PIPE_NUM; chidx++) {
        g_dwc2_hcd.chan[chidx].exclsem = usb_osal_mutex_create();
        g_dwc2_hcd.chan[chidx].waitsem = usb_osal_sem_create(0);
    }

    return 0;
}

int usb_hc_hw_init(void)
{
    int ret;

    usb_hc_low_level_init();

    USB_OTG_GLB->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;

    ret = dwc2_core_init();
    /* Force Host Mode*/
    dwc2_set_mode(USB_OTG_MODE_HOST);
    usb_osal_msleep(50);

    /* Restart the Phy Clock */
    USB_OTG_PCGCCTL = 0U;

#if defined(STM32F446xx) || defined(STM32F469xx) || defined(STM32F479xx) || defined(STM32F412Zx) || defined(STM32F412Vx) || defined(STM32F412Rx) || defined(STM32F412Cx) || defined(STM32F413xx) || defined(STM32F423xx) || \
    defined(STM32F7) || defined(STM32H7)
    /* Disable HW VBUS sensing */
    USB_OTG_GLB->GCCFG &= ~(USB_OTG_GCCFG_VBDEN);
    /* Disable Battery chargin detector */
    USB_OTG_GLB->GCCFG &= ~(USB_OTG_GCCFG_BCDEN);
#else
    /*
     * Disable HW VBUS sensing. VBUS is internally considered to be always
     * at VBUS-Valid level (5V).
     */
    USB_OTG_GLB->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
    USB_OTG_GLB->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
    USB_OTG_GLB->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
#endif
    USB_OTG_HOST->HCFG |= USB_OTG_HCFG_FSLSPCS_0;
    usbh_reset_port(1);

    /* Set default Max speed support */
    USB_OTG_HOST->HCFG &= ~(USB_OTG_HCFG_FSLSS);

    ret = dwc2_flush_txfifo(0x10U);
    ret = dwc2_flush_rxfifo();

    /* Clear all pending HC Interrupts */
    for (uint8_t i = 0U; i < CONFIG_USB_DWC2_PIPE_NUM; i++) {
        USB_OTG_HC(i)->HCINT = 0xFFFFFFFFU;
        USB_OTG_HC(i)->HCINTMSK = 0U;
    }

    dwc2_drivebus(1);
    usb_osal_msleep(200);

    /* Disable all interrupts. */
    USB_OTG_GLB->GINTMSK = 0U;

    /* Clear any pending interrupts */
    USB_OTG_GLB->GINTSTS = 0xFFFFFFFFU;

    /* set Rx FIFO size */
    USB_OTG_GLB->GRXFSIZ = 0x200U;
    USB_OTG_GLB->DIEPTXF0_HNPTXFSIZ = (uint32_t)(((0x100U << 16) & USB_OTG_NPTXFD) | 0x200U);
    USB_OTG_GLB->HPTXFSIZ = (uint32_t)(((0xE0U << 16) & USB_OTG_HPTXFSIZ_PTXFD) | 0x300U);

    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_HBSTLEN_2;
    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_DMAEN;

    /* Enable interrupts matching to the Host mode ONLY */
    USB_OTG_GLB->GINTMSK |= (USB_OTG_GINTMSK_PRTIM | USB_OTG_GINTMSK_HCIM |
                             USB_OTG_GINTSTS_DISCINT |
                             USB_OTG_GINTMSK_PXFRM_IISOOXFRM | USB_OTG_GINTMSK_WUIM);

    //    USB_OTG_GLB->GINTMSK |=USB_OTG_GINTMSK_SOFM;

    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;

    return 0;
}

bool usbh_get_port_connect_status(const uint8_t port)
{
    __IO uint32_t hprt0;

    hprt0 = USB_OTG_HPRT;

    if ((hprt0 & USB_OTG_HPRT_PCSTS) == USB_OTG_HPRT_PCSTS) {
        return true;
    } else {
        return false;
    }
}

int usbh_reset_port(const uint8_t port)
{
    __IO uint32_t hprt0 = 0U;

    hprt0 = USB_OTG_HPRT;

    hprt0 &= ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
               USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);

    USB_OTG_HPRT = (USB_OTG_HPRT_PRST | hprt0);
    usb_osal_msleep(100U); /* See Note #1 */
    USB_OTG_HPRT = ((~USB_OTG_HPRT_PRST) & hprt0);
    usb_osal_msleep(10U);
    return 0;
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    __IO uint32_t hprt0 = 0U;
    uint8_t speed;

    hprt0 = USB_OTG_HPRT;

    speed = (hprt0 & USB_OTG_HPRT_PSPD) >> 17;

    if (speed == HPRT0_PRTSPD_HIGH_SPEED) {
        return USB_SPEED_HIGH;
    } else if (speed == HPRT0_PRTSPD_FULL_SPEED) {
        return USB_SPEED_FULL;
    } else if (speed == HPRT0_PRTSPD_LOW_SPEED) {
        return USB_SPEED_LOW;
    } else {
        return USB_SPEED_UNKNOWN;
    }
}

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    struct dwc2_pipe *chan;
    int ret;

    chan = (struct dwc2_pipe *)ep;

    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }
    chan->dev_addr = dev_addr;
    chan->ep_mps = ep_mps;
    chan->speed = speed;
    usb_osal_mutex_give(chan->exclsem);
    return 0;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct dwc2_pipe *chan;
    struct usbh_hubport *hport;
    int chidx;
    uint8_t speed;
    usb_osal_sem_t waitsem;
    usb_osal_mutex_t exclsem;

    hport = ep_cfg->hport;

    chidx = dwc2_pipe_alloc();

    chan = &g_dwc2_hcd.chan[chidx];

    waitsem = chan->waitsem;
    exclsem = chan->exclsem;

    memset(chan, 0, sizeof(struct dwc2_pipe));

    chan->chidx = chidx;
    chan->ep_addr = ep_cfg->ep_addr;
    chan->ep_mps = ep_cfg->ep_mps;
    chan->ep_type = ep_cfg->ep_type;
    chan->ep_interval = ep_cfg->ep_interval;
    chan->dev_addr = hport->dev_addr;
    chan->speed = hport->speed;

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        chan->data_pid = HC_PID_DATA1;
    } else {
        dwc2_pipe_init(chidx, chan->dev_addr, ep_cfg->ep_addr, ep_cfg->ep_type, chan->ep_mps, chan->speed);
        chan->data_pid = HC_PID_DATA0;
    }

    chan->inuse = true;
    chan->waitsem = waitsem;
    chan->exclsem = exclsem;

    *ep = (usbh_epinfo_t)chan;

    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    struct dwc2_pipe *chan;

    chan = (struct dwc2_pipe *)ep;

    dwc2_pipe_free(chan);
    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    struct dwc2_pipe *chan;
    int chidx;
    int ret;

    chan = (struct dwc2_pipe *)ep;

#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((((uint32_t)setup) & 0x1f) || (buffer && (((uint32_t)buffer) & 0x1f))) {
        return -EINVAL;
    }
#endif
#if defined(STM32F7) || defined(STM32H7)
    if (((((uint32_t)setup) & 0x24000000) != 0x24000000) ||
        ((buffer && (((uint32_t)buffer) & 0x24000000) != 0x24000000))) {
        return -EINVAL;
    }
#endif
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = dwc2_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }

    chidx = chan->chidx;

    chan->waiter = true;
    chan->result = -EBUSY;
    chan->num_packets = dwc2_calculate_packet_num(8, chan->ep_addr, chan->ep_mps, &chan->xferlen);
    usb_dwc2_dcache_clean((uintptr_t)setup, 8);
    dwc2_pipe_init(chidx, chan->dev_addr, 0x00, 0x00, chan->ep_mps, chan->speed);
    dwc2_pipe_transfer(chidx, 0x00, (uint32_t *)setup, chan->xferlen, chan->num_packets, HC_PID_SETUP);
    ret = dwc2_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
    if (ret < 0) {
        goto error_out;
    }

    if (setup->wLength && buffer) {
        if (setup->bmRequestType & 0x80) {
            chan->waiter = true;
            chan->result = -EBUSY;
            chan->num_packets = dwc2_calculate_packet_num(setup->wLength, 0x80, chan->ep_mps, &chan->xferlen);
            dwc2_pipe_init(chidx, chan->dev_addr, 0x80, 0x00, chan->ep_mps, chan->speed);
            dwc2_pipe_transfer(chidx, 0x80, (uint32_t *)buffer, chan->xferlen, chan->num_packets, HC_PID_DATA1);
            ret = dwc2_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
            usb_dwc2_dcache_invalidate((uintptr_t)buffer, setup->wLength);
            chan->waiter = true;
            chan->result = -EBUSY;
            chan->num_packets = dwc2_calculate_packet_num(0, 0x00, chan->ep_mps, &chan->xferlen);
            dwc2_pipe_init(chidx, chan->dev_addr, 0x00, 0x00, chan->ep_mps, chan->speed);
            dwc2_pipe_transfer(chidx, 0x00, NULL, chan->xferlen, chan->num_packets, HC_PID_DATA1);
            ret = dwc2_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
        } else {
            chan->waiter = true;
            chan->result = -EBUSY;
            chan->num_packets = dwc2_calculate_packet_num(setup->wLength, 0x00, chan->ep_mps, &chan->xferlen);
            usb_dwc2_dcache_clean((uintptr_t)buffer, setup->wLength);
            dwc2_pipe_init(chidx, chan->dev_addr, 0x00, 0x00, chan->ep_mps, chan->speed);
            dwc2_pipe_transfer(chidx, 0x00, (uint32_t *)buffer, chan->xferlen, chan->num_packets, HC_PID_DATA1);
            ret = dwc2_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
            chan->waiter = true;
            chan->result = -EBUSY;
            chan->num_packets = dwc2_calculate_packet_num(0, 0x80, chan->ep_mps, &chan->xferlen);
            dwc2_pipe_init(chidx, chan->dev_addr, 0x80, 0x00, chan->ep_mps, chan->speed);
            dwc2_pipe_transfer(chidx, 0x80, NULL, chan->xferlen, chan->num_packets, HC_PID_DATA1);
            ret = dwc2_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
            if (ret < 0) {
                goto error_out;
            }
        }
    } else {
        chan->waiter = true;
        chan->result = -EBUSY;
        chan->num_packets = dwc2_calculate_packet_num(0, 0x80, chan->ep_mps, &chan->xferlen);
        dwc2_pipe_init(chidx, chan->dev_addr, 0x80, 0x00, chan->ep_mps, chan->speed);
        dwc2_pipe_transfer(chidx, 0x80, NULL, chan->xferlen, chan->num_packets, HC_PID_DATA1);
        ret = dwc2_pipe_wait(chan, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
        if (ret < 0) {
            goto error_out;
        }
    }
    usb_osal_mutex_give(chan->exclsem);
    return ret;
error_out:
    chan->waiter = false;
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    struct dwc2_pipe *chan;
    int chidx;
    int ret;

    chan = (struct dwc2_pipe *)ep;
#ifdef CONFIG_USB_DCACHE_ENABLE
    if (buffer && (((uint32_t)buffer) & 0x1f)) {
        return -EINVAL;
    }
#endif
#if defined(STM32F7) || defined(STM32H7)
    if ((buffer && (((uint32_t)buffer) & 0x24000000) != 0x24000000)) {
        return -EINVAL;
    }
#endif
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = dwc2_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }

    chidx = chan->chidx;
    chan->num_packets = dwc2_calculate_packet_num(buflen, chan->ep_addr, chan->ep_mps, &chan->xferlen);
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x00) {
        usb_dwc2_dcache_clean((uintptr_t)buffer, buflen);
    }
#endif
    dwc2_pipe_transfer(chidx, chan->ep_addr, (uint32_t *)buffer, chan->xferlen, chan->num_packets, chan->data_pid);
    ret = dwc2_pipe_wait(chan, timeout);
    if (ret < 0) {
        goto error_out;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x80) {
        usb_dwc2_dcache_invalidate((uintptr_t)buffer, buflen);
    }
#endif
    usb_osal_mutex_give(chan->exclsem);
    return ret;
error_out:
    chan->waiter = false;
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    struct dwc2_pipe *chan;
    int chidx;
    int ret;
    uint32_t wait_ms_count = 0;

    chan = (struct dwc2_pipe *)ep;
#ifdef CONFIG_USB_DCACHE_ENABLE
    if (buffer && (((uint32_t)buffer) & 0x1f)) {
        return -EINVAL;
    }
#endif
#if defined(STM32F7) || defined(STM32H7)
    if ((buffer && (((uint32_t)buffer) & 0x24000000) != 0x24000000)) {
        return -EINVAL;
    }
#endif
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = dwc2_pipe_waitsetup(chan);
    if (ret < 0) {
        goto error_out;
    }

    chidx = chan->chidx;
    chan->num_packets = dwc2_calculate_packet_num(buflen, chan->ep_addr, chan->ep_mps, &chan->xferlen);
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x00) {
        usb_dwc2_dcache_clean((uintptr_t)buffer, buflen);
    }
#endif
    while (1) {
        wait_ms_count++;
        dwc2_pipe_transfer(chidx, chan->ep_addr, (uint32_t *)buffer, chan->xferlen, chan->num_packets, chan->data_pid);
        usb_osal_msleep(chan->ep_interval);
        if (chan->result != -EBUSY) {
            break;
        }
        if ((wait_ms_count * chan->ep_interval) > timeout) {
            ret = -ETIMEDOUT;
            goto error_out;
        }
    }

    ret = dwc2_pipe_wait(chan, 0);
    if (ret < 0) {
        goto error_out;
    }
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x80) {
        usb_dwc2_dcache_invalidate((uintptr_t)buffer, buflen);
    }
#endif
    usb_osal_mutex_give(chan->exclsem);
    return ret;
error_out:
    chan->waiter = false;
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    struct dwc2_pipe *chan;
    int chidx;
    int ret;

    chan = (struct dwc2_pipe *)ep;
#ifdef CONFIG_USB_DCACHE_ENABLE
    if (buffer && (((uint32_t)buffer) & 0x1f)) {
        return -EINVAL;
    }
#endif
#if defined(STM32F7) || defined(STM32H7)
    if ((buffer && (((uint32_t)buffer) & 0x24000000) != 0x24000000)) {
        return -EINVAL;
    }
#endif
    ret = usb_osal_mutex_take(chan->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = dwc2_pipe_asynchsetup(chan, callback, arg);
    if (ret < 0) {
        goto error_out;
    }

    chidx = chan->chidx;
    chan->num_packets = dwc2_calculate_packet_num(buflen, chan->ep_addr, chan->ep_mps, &chan->xferlen);
#ifdef CONFIG_USB_DCACHE_ENABLE
    if ((chan->ep_addr & 0x80) == 0x00) {
        usb_dwc2_dcache_clean((uintptr_t)buffer, buflen);
    }
#endif
    dwc2_pipe_transfer(chidx, chan->ep_addr, (uint32_t *)buffer, chan->xferlen, chan->num_packets, chan->data_pid);

    return 0;
error_out:
    usb_osal_mutex_give(chan->exclsem);
    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    return -1;
}

int usb_ep_cancel(usbh_epinfo_t ep)
{
    struct dwc2_pipe *chan;
    int ret;
    size_t flags;
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback;
    void *arg;
#endif

    chan = (struct dwc2_pipe *)ep;

    flags = usb_osal_enter_critical_section();

    chan->result = -ESHUTDOWN;
#ifdef CONFIG_USBHOST_ASYNCH
    /* Extract the callback information */
    callback = chan->callback;
    arg = chan->arg;
    chan->callback = NULL;
    chan->arg = NULL;
    chan->xfrd = 0;
#endif
    usb_osal_leave_critical_section(flags);

    /* Is there a thread waiting for this transfer to complete? */

    if (chan->waiter) {
        /* Wake'em up! */
        chan->waiter = false;
        usb_osal_sem_give(chan->waitsem);
    }
#ifdef CONFIG_USBHOST_ASYNCH
    /* No.. is an asynchronous callback expected when the transfer completes? */
    else if (callback) {
        /* Then perform the callback */
        callback(arg, -ESHUTDOWN);
    }
#endif
    return 0;
}

//static void usb_dwc2_rxqlvl_irq_handler(void)
//{
//    uint32_t pktsts;
//    uint32_t pktcnt;
//    uint32_t GrxstspReg;
//    uint32_t xferSizePktCnt;
//    uint32_t tmpreg;
//    uint32_t ch_num;
//    uint32_t len32b;
//    uint32_t *pdest;
//    struct dwc2_pipe *chan;

//    GrxstspReg = USB_OTG_GLB->GRXSTSP;
//    ch_num = GrxstspReg & USB_OTG_GRXSTSP_EPNUM;
//    pktsts = (GrxstspReg & USB_OTG_GRXSTSP_PKTSTS) >> 17;
//    pktcnt = (GrxstspReg & USB_OTG_GRXSTSP_BCNT) >> 4;

//    chan = &g_dwc2_hcd.chan[ch_num];
//    switch (pktsts) {
//        case GRXSTS_PKTSTS_IN:
//            /* Read the data into the host buffer. */
//            if ((pktcnt > 0U) && (chan->buffer != NULL)) {
//                len32b = ((uint32_t)pktcnt + 3U) / 4U;

//                pdest = (uint32_t *)chan->buffer;

//                for (uint8_t i = 0U; i < len32b; i++) {
//                    *pdest = USB_OTG_FIFO(0U);
//                    pdest++;
//                }

//                chan->buffer += pktcnt;
//                chan->xfrd += pktcnt;
//                chan->buflen -= pktcnt;

//                if (chan->buflen == 0) {
//                }
//            }
//            break;

//        case GRXSTS_PKTSTS_DATA_TOGGLE_ERR:
//            break;

//        case GRXSTS_PKTSTS_IN_XFER_COMP:
//        case GRXSTS_PKTSTS_CH_HALTED:
//        default:
//            break;
//    }
//}

static void dwc2_inchan_irq_handler(uint8_t ch_num)
{
    uint32_t chan_intstatus;
    struct dwc2_pipe *chan;

    chan_intstatus = (USB_OTG_HC(ch_num)->HCINT) & (USB_OTG_HC((uint32_t)ch_num)->HCINTMSK);

    chan = &g_dwc2_hcd.chan[ch_num];

    if ((chan_intstatus & USB_OTG_HCINT_XFRC) == USB_OTG_HCINT_XFRC) {
        chan->result = 0;

        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_XFRC);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_AHBERR) == USB_OTG_HCINT_AHBERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_AHBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_STALL) == USB_OTG_HCINT_STALL) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EPERM;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_STALL);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_NAK) == USB_OTG_HCINT_NAK) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EAGAIN;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_ACK) == USB_OTG_HCINT_ACK) {
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_ACK);
    } else if ((chan_intstatus & USB_OTG_HCINT_NYET) == USB_OTG_HCINT_NYET) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EAGAIN;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NYET);
    } else if ((chan_intstatus & USB_OTG_HCINT_TXERR) == USB_OTG_HCINT_TXERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_TXERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_BBERR) == USB_OTG_HCINT_BBERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_BBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_FRMOR) == USB_OTG_HCINT_FRMOR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EPIPE;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_FRMOR);
    } else if ((chan_intstatus & USB_OTG_HCINT_DTERR) == USB_OTG_HCINT_DTERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_DTERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_CHH) == USB_OTG_HCINT_CHH) {
        USB_MASK_HALT_HC_INT(ch_num);

        if (chan->result == 0) {
            uint32_t count = chan->xferlen - (USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_HCTSIZ_XFRSIZ);                          /* size that has received */
            uint32_t has_sent_packets = chan->num_packets - ((USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_DIEPTSIZ_PKTCNT) >> 19); /*how many packets has sent*/

            chan->xfrd += count;

            if ((has_sent_packets % 2) == 1) /* Flip in odd numbers */
            {
                if (chan->data_pid == HC_PID_DATA0) {
                    chan->data_pid = HC_PID_DATA1;
                } else {
                    chan->data_pid = HC_PID_DATA0;
                }
            }
            chan->result = 0;
            dwc2_pipe_wakeup(chan);
        } else if (chan->result == -EAGAIN) {
            /* re-activate the channel */
            uint32_t tmpreg = USB_OTG_HC(ch_num)->HCCHAR;
            tmpreg &= ~USB_OTG_HCCHAR_CHDIS;
            tmpreg |= USB_OTG_HCCHAR_CHENA;
            USB_OTG_HC(ch_num)->HCCHAR = tmpreg;
        } else {
            dwc2_pipe_wakeup(chan);
        }

        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_CHH);
    }
}

static void dwc2_outchan_irq_handler(uint8_t ch_num)
{
    uint32_t chan_intstatus;
    struct dwc2_pipe *chan;
    uint16_t buflen;

    chan_intstatus = (USB_OTG_HC(ch_num)->HCINT) & (USB_OTG_HC((uint32_t)ch_num)->HCINTMSK);

    chan = &g_dwc2_hcd.chan[ch_num];

    if ((chan_intstatus & USB_OTG_HCINT_XFRC) == USB_OTG_HCINT_XFRC) {
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_XFRC);
        dwc2_halt(ch_num);
        chan->result = 0;
        USB_UNMASK_HALT_HC_INT(ch_num);
    } else if ((chan_intstatus & USB_OTG_HCINT_AHBERR) == USB_OTG_HCINT_AHBERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_AHBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_STALL) == USB_OTG_HCINT_STALL) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EPERM;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_STALL);
    } else if ((chan_intstatus & USB_OTG_HCINT_NAK) == USB_OTG_HCINT_NAK) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EAGAIN;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_ACK) == USB_OTG_HCINT_ACK) {
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_ACK);
    } else if ((chan_intstatus & USB_OTG_HCINT_NYET) == USB_OTG_HCINT_NYET) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EAGAIN;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NYET);
    } else if ((chan_intstatus & USB_OTG_HCINT_TXERR) == USB_OTG_HCINT_TXERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_TXERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_BBERR) == USB_OTG_HCINT_BBERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_BBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_FRMOR) == USB_OTG_HCINT_FRMOR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EPIPE;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_FRMOR);
    } else if ((chan_intstatus & USB_OTG_HCINT_DTERR) == USB_OTG_HCINT_DTERR) {
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        chan->result = -EIO;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_DTERR);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_CHH) == USB_OTG_HCINT_CHH) {
        USB_MASK_HALT_HC_INT(ch_num);

        if (chan->result == 0) {
            uint32_t count = (USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_HCTSIZ_XFRSIZ); /* last send size */

            if (count == chan->ep_mps) {
                chan->xfrd += chan->num_packets * chan->ep_mps;
            } else {
                chan->xfrd += (chan->num_packets - 1) * chan->ep_mps + count;
            }

            if ((chan->num_packets % 2) == 1) /* Flip in odd numbers */
            {
                if (chan->data_pid == HC_PID_DATA0) {
                    chan->data_pid = HC_PID_DATA1;
                } else {
                    chan->data_pid = HC_PID_DATA0;
                }
            }
            chan->result = 0;
            dwc2_pipe_wakeup(chan);
        } else if (chan->result == -EAGAIN) {
            /* re-activate the channel */
            uint32_t tmpreg = USB_OTG_HC(ch_num)->HCCHAR;
            tmpreg &= ~USB_OTG_HCCHAR_CHDIS;
            tmpreg |= USB_OTG_HCCHAR_CHENA;
            USB_OTG_HC(ch_num)->HCCHAR = tmpreg;
        } else {
            dwc2_pipe_wakeup(chan);
        }

        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_CHH);
    }
}

void dwc2_reset_handler(void *arg)
{
    usbh_reset_port(1);
}

static void dwc2_port_irq_handler(void)
{
    __IO uint32_t hprt0, hprt0_dup, regval;
    bool reset = false;

    /* Handle Host Port Interrupts */
    hprt0 = USB_OTG_HPRT;
    hprt0_dup = USB_OTG_HPRT;

    hprt0_dup &= ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
                   USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);

    /* Check whether Port Connect detected */
    if ((hprt0 & USB_OTG_HPRT_PCDET) == USB_OTG_HPRT_PCDET) {
        if ((hprt0 & USB_OTG_HPRT_PCSTS) == USB_OTG_HPRT_PCSTS) {
            reset = true;
        }
        hprt0_dup |= USB_OTG_HPRT_PCDET;
    }

    /* Check whether Port Enable Changed */
    if ((hprt0 & USB_OTG_HPRT_PENCHNG) == USB_OTG_HPRT_PENCHNG) {
        hprt0_dup |= USB_OTG_HPRT_PENCHNG;

        if ((hprt0 & USB_OTG_HPRT_PENA) == USB_OTG_HPRT_PENA) {
#if defined(CONFIG_USB_DWC2_ULPI_PHY)
#else
            if ((hprt0 & USB_OTG_HPRT_PSPD) == (HPRT0_PRTSPD_LOW_SPEED << 17)) {
                USB_OTG_HOST->HFIR = 6000U;
                if ((USB_OTG_HOST->HCFG & USB_OTG_HCFG_FSLSPCS) != USB_OTG_HCFG_FSLSPCS_1) {
                    regval = USB_OTG_HOST->HCFG;
                    regval &= ~USB_OTG_HCFG_FSLSPCS;
                    regval |= USB_OTG_HCFG_FSLSPCS_1;
                    USB_OTG_HOST->HCFG = regval;
                    reset = true;
                }
            } else {
                USB_OTG_HOST->HFIR = 48000U;
                if ((USB_OTG_HOST->HCFG & USB_OTG_HCFG_FSLSPCS) != USB_OTG_HCFG_FSLSPCS_0) {
                    regval = USB_OTG_HOST->HCFG;
                    regval &= ~USB_OTG_HCFG_FSLSPCS;
                    regval |= USB_OTG_HCFG_FSLSPCS_0;
                    USB_OTG_HOST->HCFG = regval;
                    reset = true;
                }
            }
#endif
            usbh_event_notify_handler(USBH_EVENT_CONNECTED, 1);
        } else {
            for (int chidx = 0; chidx < CONFIG_USB_DWC2_PIPE_NUM; chidx++) {
                struct dwc2_pipe *chan;
                chan = &g_dwc2_hcd.chan[chidx];
                if (chan->waiter) {
                    chan->waiter = false;
                    usb_osal_sem_give(chan->waitsem);
                }
            }
            usbh_event_notify_handler(USBH_EVENT_DISCONNECTED, 1);
        }
    }

    /* Check for an overcurrent */
    if ((hprt0 & USB_OTG_HPRT_POCCHNG) == USB_OTG_HPRT_POCCHNG) {
        hprt0_dup |= USB_OTG_HPRT_POCCHNG;
    }
    /* Clear Port Interrupts */
    USB_OTG_HPRT = hprt0_dup;
    if (reset) {
        usb_workqueue_submit(&g_hpworkq, &g_dwc2_hcd.work, dwc2_reset_handler, NULL, 0);
    }
}

void USBH_IRQHandler(void)
{
    uint32_t gint_status, chan_int;
    gint_status = dwc2_get_glb_intstatus();
    if ((USB_OTG_GLB->GINTSTS & 0x1U) == USB_OTG_MODE_HOST) {
        /* Avoid spurious interrupt */
        if (gint_status == 0) {
            return;
        }

        if (gint_status & USB_OTG_GINTSTS_HPRTINT) {
            dwc2_port_irq_handler();
        }
        if (gint_status & USB_OTG_GINTSTS_DISCINT) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_DISCINT;
        }
        //        if (gint_status & USB_OTG_GINTSTS_RXFLVL) {
        //            USB_MASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);
        //            usb_dwc2_rxqlvl_irq_handler();
        //            USB_UNMASK_INTERRUPT(USB_OTG_GLB, USB_OTG_GINTSTS_RXFLVL);
        //        }
        if (gint_status & USB_OTG_GINTSTS_HCINT) {
            chan_int = (USB_OTG_HOST->HAINT & USB_OTG_HOST->HAINTMSK) & 0xFFFFU;
            for (uint8_t i = 0U; i < CONFIG_USB_DWC2_PIPE_NUM; i++) {
                if ((chan_int & (1UL << (i & 0xFU))) != 0U) {
                    if ((USB_OTG_HC(i)->HCCHAR & USB_OTG_HCCHAR_EPDIR) == USB_OTG_HCCHAR_EPDIR) {
                        dwc2_inchan_irq_handler(i);
                    } else {
                        dwc2_outchan_irq_handler(i);
                    }
                }
            }
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_HCINT;
        }
        if (gint_status & USB_OTG_GINTSTS_SOF) {
            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_SOF;
        }
    }
}