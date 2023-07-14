/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_dwc2_reg.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler OTG_HS_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE (0x40040000UL)
#endif

#ifndef CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USBHOST_PIPE_NUM 12
#endif

#if defined(STM32F7) || defined(STM32H7)
#warning please check your buf addr is not in tcm and use nocache ram.
#endif

#define USB_OTG_GLB     ((USB_OTG_GlobalTypeDef *)(USB_BASE))
#define USB_OTG_PCGCCTL *(__IO uint32_t *)((uint32_t)USB_BASE + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_HPRT    *(__IO uint32_t *)((uint32_t)USB_BASE + USB_OTG_HOST_PORT_BASE)
#define USB_OTG_HOST    ((USB_OTG_HostTypeDef *)(USB_BASE + USB_OTG_HOST_BASE))
#define USB_OTG_HC(i)   ((USB_OTG_HostChannelTypeDef *)(USB_BASE + USB_OTG_HOST_CHANNEL_BASE + ((i)*USB_OTG_HOST_CHANNEL_SIZE)))
#define USB_OTG_FIFO(i) *(__IO uint32_t *)(USB_BASE + USB_OTG_FIFO_BASE + ((i)*USB_OTG_FIFO_SIZE))

struct dwc2_pipe {
    uint8_t dev_addr;
    uint8_t ep_addr;
    uint8_t ep_type;
    uint8_t ep_interval;
    uint8_t speed;
    uint16_t ep_mps;
    uint8_t data_pid;
    uint8_t chidx;
    volatile uint8_t ep0_state;
    uint16_t num_packets;
    uint32_t xferlen;
    bool inuse;
    uint32_t xfrd;
    int errorcode;
    volatile bool waiter;
    usb_osal_sem_t waitsem;
    struct usbh_hubport *hport;
    struct usbh_urb *urb;
    uint32_t iso_frame_idx;
};

struct dwc2_hcd {
    volatile bool port_csc;
    volatile bool port_pec;
    volatile bool port_occ;
    struct dwc2_pipe pipe_pool[CONFIG_USBHOST_PIPE_NUM];
} g_dwc2_hcd;

#define DWC2_EP0_STATE_SETUP     0
#define DWC2_EP0_STATE_INDATA    1
#define DWC2_EP0_STATE_OUTDATA   2
#define DWC2_EP0_STATE_INSTATUS  3
#define DWC2_EP0_STATE_OUTSTATUS 4

static inline int dwc2_reset(void)
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
    volatile uint32_t count = 0;

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
    volatile uint32_t count = 0U;

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

static void dwc2_pipe_init(uint8_t ch_num, uint8_t devaddr, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps, uint8_t speed)
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
            regval |= USB_OTG_HCINTMSK_NAKM;
        case USB_ENDPOINT_TYPE_BULK:
            //regval |= USB_OTG_HCINTMSK_NAKM;
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            regval |= USB_OTG_HCINTMSK_NAKM;
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
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

    /* Set host channel enable */
    tmpreg = USB_OTG_HC(ch_num)->HCCHAR;
    tmpreg &= ~USB_OTG_HCCHAR_CHDIS;
    tmpreg |= USB_OTG_HCCHAR_CHENA;
    USB_OTG_HC(ch_num)->HCCHAR = tmpreg;
}

static void dwc2_halt(uint8_t ch_num)
{
    volatile uint32_t count = 0U;
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

static int usbh_reset_port(const uint8_t port)
{
    __IO uint32_t hprt0 = 0U;

    hprt0 = USB_OTG_HPRT;

    hprt0 &= ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
               USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);

    USB_OTG_HPRT = (USB_OTG_HPRT_PRST | hprt0);
    usb_osal_msleep(100U); /* See Note #1 */
    USB_OTG_HPRT = ((~USB_OTG_HPRT_PRST) & hprt0);
    usb_osal_msleep(10U);

    while (!(USB_OTG_HPRT & USB_OTG_HPRT_PENA)) {
        usb_osal_msleep(10U);
    }
    return 0;
}

static uint8_t usbh_get_port_speed(const uint8_t port)
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

static int dwc2_pipe_alloc(void)
{
    int chidx;

    for (chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        if (!g_dwc2_hcd.pipe_pool[chidx].inuse) {
            g_dwc2_hcd.pipe_pool[chidx].inuse = true;
            return chidx;
        }
    }

    return -1;
}

static void dwc2_pipe_free(struct dwc2_pipe *pipe)
{
    pipe->inuse = false;
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

static void dwc2_control_pipe_init(struct dwc2_pipe *chan, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    if (chan->ep0_state == DWC2_EP0_STATE_SETUP) /* fill setup */
    {
        chan->num_packets = dwc2_calculate_packet_num(8, 0x00, chan->ep_mps, &chan->xferlen);
        dwc2_pipe_init(chan->chidx, chan->dev_addr, 0x00, USB_ENDPOINT_TYPE_CONTROL, chan->ep_mps, chan->speed);
        dwc2_pipe_transfer(chan->chidx, 0x00, (uint32_t *)setup, chan->xferlen, chan->num_packets, HC_PID_SETUP);
    } else if (chan->ep0_state == DWC2_EP0_STATE_INDATA) /* fill in data */
    {
        chan->num_packets = dwc2_calculate_packet_num(setup->wLength, 0x80, chan->ep_mps, &chan->xferlen);
        dwc2_pipe_init(chan->chidx, chan->dev_addr, 0x80, USB_ENDPOINT_TYPE_CONTROL, chan->ep_mps, chan->speed);
        dwc2_pipe_transfer(chan->chidx, 0x80, (uint32_t *)buffer, chan->xferlen, chan->num_packets, HC_PID_DATA1);
    } else if (chan->ep0_state == DWC2_EP0_STATE_OUTDATA) /* fill out data */
    {
        chan->num_packets = dwc2_calculate_packet_num(setup->wLength, 0x00, chan->ep_mps, &chan->xferlen);
        dwc2_pipe_init(chan->chidx, chan->dev_addr, 0x00, USB_ENDPOINT_TYPE_CONTROL, chan->ep_mps, chan->speed);
        dwc2_pipe_transfer(chan->chidx, 0x00, (uint32_t *)buffer, chan->xferlen, chan->num_packets, HC_PID_DATA1);
    } else if (chan->ep0_state == DWC2_EP0_STATE_INSTATUS) /* fill in status */
    {
        chan->num_packets = dwc2_calculate_packet_num(0, 0x80, chan->ep_mps, &chan->xferlen);
        dwc2_pipe_init(chan->chidx, chan->dev_addr, 0x80, USB_ENDPOINT_TYPE_CONTROL, chan->ep_mps, chan->speed);
        dwc2_pipe_transfer(chan->chidx, 0x80, NULL, chan->xferlen, chan->num_packets, HC_PID_DATA1);
    } else if (chan->ep0_state == DWC2_EP0_STATE_OUTSTATUS) /* fill out status */
    {
        chan->num_packets = dwc2_calculate_packet_num(0, 0x00, chan->ep_mps, &chan->xferlen);
        dwc2_pipe_init(chan->chidx, chan->dev_addr, 0x00, USB_ENDPOINT_TYPE_CONTROL, chan->ep_mps, chan->speed);
        dwc2_pipe_transfer(chan->chidx, 0x00, NULL, chan->xferlen, chan->num_packets, HC_PID_DATA1);
    }
}

static void dwc2_bulk_intr_pipe_init(struct dwc2_pipe *chan, uint8_t *buffer, uint32_t buflen)
{
    chan->num_packets = dwc2_calculate_packet_num(buflen, chan->ep_addr, chan->ep_mps, &chan->xferlen);
    dwc2_pipe_transfer(chan->chidx, chan->ep_addr, (uint32_t *)buffer, chan->xferlen, chan->num_packets, chan->data_pid);
}

static void dwc2_iso_pipe_init(struct dwc2_pipe *chan, struct usbh_iso_frame_packet *iso_packet)
{
    chan->num_packets = dwc2_calculate_packet_num(iso_packet->transfer_buffer_length, chan->ep_addr, chan->ep_mps, &chan->xferlen);
    dwc2_pipe_transfer(chan->chidx, chan->ep_addr, (uint32_t *)iso_packet->transfer_buffer, chan->xferlen, chan->num_packets, HC_PID_DATA0);
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_init(void)
{
    int ret;

    memset(&g_dwc2_hcd, 0, sizeof(struct dwc2_hcd));

    for (uint8_t chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        g_dwc2_hcd.pipe_pool[chidx].waitsem = usb_osal_sem_create(0);
    }

    usb_hc_low_level_init();

    USB_LOG_INFO("========== dwc2 hcd params ==========\r\n");
    USB_LOG_INFO("CID:%08x\r\n", USB_OTG_GLB->CID);
    USB_LOG_INFO("GSNPSID:%08x\r\n", USB_OTG_GLB->GSNPSID);
    USB_LOG_INFO("GHWCFG1:%08x\r\n", USB_OTG_GLB->GHWCFG1);
    USB_LOG_INFO("GHWCFG2:%08x\r\n", USB_OTG_GLB->GHWCFG2);
    USB_LOG_INFO("GHWCFG3:%08x\r\n", USB_OTG_GLB->GHWCFG3);
    USB_LOG_INFO("GHWCFG4:%08x\r\n", USB_OTG_GLB->GHWCFG4);

    USB_LOG_INFO("dwc2 has %d channels\r\n", ((USB_OTG_GLB->GHWCFG2 & (0x0f << 14)) >> 14) + 1);

    if ((USB_OTG_GLB->GHWCFG2 & (0x3U << 3)) == 0U) {
        USB_LOG_ERR("This dwc2 version does not support dma, so stop working\r\n");
        while (1) {
        }
    }

    USB_OTG_GLB->GAHBCFG &= ~USB_OTG_GAHBCFG_GINT;

    ret = dwc2_core_init();
    /* Force Host Mode*/
    dwc2_set_mode(USB_OTG_MODE_HOST);
    usb_osal_msleep(50);

    /* Restart the Phy Clock */
    USB_OTG_PCGCCTL = 0U;

#if defined(STM32F7) || defined(STM32H7)
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

    /* Set default Max speed support */
    USB_OTG_HOST->HCFG &= ~(USB_OTG_HCFG_FSLSS);

    ret = dwc2_flush_txfifo(0x10U);
    ret = dwc2_flush_rxfifo();

    /* Clear all pending HC Interrupts */
    for (uint8_t i = 0U; i < CONFIG_USBHOST_PIPE_NUM; i++) {
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
                             USB_OTG_GINTSTS_DISCINT);

    USB_OTG_GLB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;

    return 0;
}

uint16_t usbh_get_frame_number(void)
{
    return (USB_OTG_HOST->HFNUM & USB_OTG_HFNUM_FRNUM);
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
    __IO uint32_t hprt0;
    uint8_t nports;
    uint8_t port;
    uint32_t status;

    nports = CONFIG_USBHOST_MAX_RHPORTS;
    port = setup->wIndex;
    if (setup->bmRequestType & USB_REQUEST_RECIPIENT_DEVICE) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_DESCRIPTOR:
                break;
            case HUB_REQUEST_GET_STATUS:
                memset(buf, 0, 4);
                break;
            default:
                break;
        }
    } else if (setup->bmRequestType & USB_REQUEST_RECIPIENT_OTHER) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                if (!port || port > nports) {
                    return -EPIPE;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_ENABLE:
                        USB_OTG_HPRT &= ~USB_OTG_HPRT_PENA;
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        g_dwc2_hcd.port_csc = 0;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        g_dwc2_hcd.port_pec = 0;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        g_dwc2_hcd.port_occ = 0;
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                if (!port || port > nports) {
                    return -EPIPE;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        USB_OTG_HPRT &= ~USB_OTG_HPRT_PPWR;
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        usbh_reset_port(port);
                        break;

                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -EPIPE;
                }
                hprt0 = USB_OTG_HPRT;

                status = 0;
                if (g_dwc2_hcd.port_csc) {
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                }
                if (g_dwc2_hcd.port_pec) {
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }
                if (g_dwc2_hcd.port_occ) {
                    status |= (1 << HUB_PORT_FEATURE_C_OVER_CURREN);
                }

                if (hprt0 & USB_OTG_HPRT_PCSTS) {
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                }
                if (hprt0 & USB_OTG_HPRT_PENA) {
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);
                    if (usbh_get_port_speed(port) == USB_SPEED_LOW) {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    } else if (usbh_get_port_speed(port) == USB_SPEED_HIGH) {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                }
                if (hprt0 & USB_OTG_HPRT_POCA) {
                    status |= (1 << HUB_PORT_FEATURE_OVERCURRENT);
                }
                if (hprt0 & USB_OTG_HPRT_PRST) {
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                }
                if (hprt0 & USB_OTG_HPRT_PPWR) {
                    status |= (1 << HUB_PORT_FEATURE_POWER);
                }

                memcpy(buf, &status, 4);
                break;
            default:
                break;
        }
    }
    return 0;
}

int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t mult)
{
    struct dwc2_pipe *chan;

    chan = (struct dwc2_pipe *)pipe;

    chan->dev_addr = dev_addr;
    chan->ep_mps = ep_mps;

    return 0;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct dwc2_pipe *chan;
    int chidx;
    usb_osal_sem_t waitsem;

    chidx = dwc2_pipe_alloc();
    if (chidx == -1) {
        return -ENOMEM;
    }

    chan = &g_dwc2_hcd.pipe_pool[chidx];

    /* store variables */
    waitsem = chan->waitsem;

    memset(chan, 0, sizeof(struct dwc2_pipe));

    chan->chidx = chidx;
    chan->ep_addr = ep_cfg->ep_addr;
    chan->ep_type = ep_cfg->ep_type;
    chan->ep_mps = ep_cfg->ep_mps;
    chan->ep_interval = ep_cfg->ep_interval;
    chan->speed = ep_cfg->hport->speed;
    chan->dev_addr = ep_cfg->hport->dev_addr;
    chan->hport = ep_cfg->hport;

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        chan->data_pid = HC_PID_DATA1;
    } else {
        dwc2_pipe_init(chidx, chan->dev_addr, ep_cfg->ep_addr, ep_cfg->ep_type, ep_cfg->ep_mps, chan->speed);
        chan->data_pid = HC_PID_DATA0;
    }

    if (chan->speed == USB_SPEED_HIGH) {
        chan->ep_interval = (1 << (chan->ep_interval - 1));
    }

    /* restore variables */
    chan->inuse = true;
    chan->waitsem = waitsem;

    *pipe = (usbh_pipe_t)chan;

    return 0;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    struct dwc2_pipe *chan;
    struct usbh_urb *urb;

    chan = (struct dwc2_pipe *)pipe;

    if (!chan) {
        return -EINVAL;
    }

    urb = chan->urb;

    if (urb) {
        usbh_kill_urb(urb);
    }

    dwc2_pipe_free(chan);
    return 0;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    struct dwc2_pipe *pipe;
    size_t flags;
    int ret = 0;

    if (!urb || !urb->pipe) {
        return -EINVAL;
    }

    pipe = urb->pipe;

    /* dma addr must be aligned 4 bytes */
    if ((((uint32_t)urb->setup) & 0x03) || (((uint32_t)urb->transfer_buffer) & 0x03)) {
        return -EINVAL;
    }

    if (!(USB_OTG_HPRT & USB_OTG_HPRT_PCSTS) || !pipe->hport->connected) {
        return -ENODEV;
    }

    if (pipe->urb) {
        return -EBUSY;
    }

    flags = usb_osal_enter_critical_section();

    pipe->waiter = false;
    pipe->xfrd = 0;
    pipe->urb = urb;
    pipe->errorcode = -EBUSY;
    urb->errorcode = -EBUSY;
    urb->actual_length = 0;

    if (urb->timeout > 0) {
        pipe->waiter = true;
    }
    usb_osal_leave_critical_section(flags);

    switch (pipe->ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
            pipe->ep0_state = DWC2_EP0_STATE_SETUP;
            dwc2_control_pipe_init(pipe, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_BULK:
        case USB_ENDPOINT_TYPE_INTERRUPT:
            dwc2_bulk_intr_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            pipe->iso_frame_idx = 0;
            dwc2_iso_pipe_init(pipe, &urb->iso_packet[pipe->iso_frame_idx]);
            break;
        default:
            break;
    }

    if (urb->timeout > 0) {
        /* wait until timeout or sem give */
        ret = usb_osal_sem_take(pipe->waitsem, urb->timeout);
        if (ret < 0) {
            goto errout_timeout;
        }

        ret = urb->errorcode;
    }
    return ret;
errout_timeout:
    pipe->waiter = false;
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    struct dwc2_pipe *pipe;
    size_t flags;

    pipe = urb->pipe;

    if (!urb || !pipe) {
        return -EINVAL;
    }

    flags = usb_osal_enter_critical_section();

    dwc2_halt(pipe->chidx);
    CLEAR_HC_INT(pipe->chidx, USB_OTG_HCINT_CHH);
    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        urb->errorcode = -ESHUTDOWN;
        usb_osal_sem_give(pipe->waitsem);
    }

    usb_osal_leave_critical_section(flags);

    return 0;
}

static inline void dwc2_pipe_waitup(struct dwc2_pipe *pipe)
{
    struct usbh_urb *urb;

    urb = pipe->urb;
    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        usb_osal_sem_give(pipe->waitsem);
    }

    if (urb->complete) {
        if (urb->errorcode < 0) {
            urb->complete(urb->arg, urb->errorcode);
        } else {
            urb->complete(urb->arg, urb->actual_length);
        }
    }
}

static void dwc2_inchan_irq_handler(uint8_t ch_num)
{
    uint32_t chan_intstatus;
    struct dwc2_pipe *chan;
    struct usbh_urb *urb;

    chan_intstatus = (USB_OTG_HC(ch_num)->HCINT) & (USB_OTG_HC((uint32_t)ch_num)->HCINTMSK);

    chan = &g_dwc2_hcd.pipe_pool[ch_num];
    urb = chan->urb;
    //printf("s1:%08x\r\n", chan_intstatus);

    if ((chan_intstatus & USB_OTG_HCINT_XFRC) == USB_OTG_HCINT_XFRC) {
        chan->errorcode = 0;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_XFRC);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_AHBERR) == USB_OTG_HCINT_AHBERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_AHBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_STALL) == USB_OTG_HCINT_STALL) {
        chan->errorcode = -EPERM;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_STALL);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_NAK) == USB_OTG_HCINT_NAK) {
        chan->errorcode = -EAGAIN;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_ACK) == USB_OTG_HCINT_ACK) {
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_ACK);
    } else if ((chan_intstatus & USB_OTG_HCINT_NYET) == USB_OTG_HCINT_NYET) {
        chan->errorcode = -EAGAIN;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NYET);
    } else if ((chan_intstatus & USB_OTG_HCINT_TXERR) == USB_OTG_HCINT_TXERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_TXERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_BBERR) == USB_OTG_HCINT_BBERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_BBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_FRMOR) == USB_OTG_HCINT_FRMOR) {
        chan->errorcode = -EPIPE;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_FRMOR);
    } else if ((chan_intstatus & USB_OTG_HCINT_DTERR) == USB_OTG_HCINT_DTERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_DTERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_CHH) == USB_OTG_HCINT_CHH) {
        USB_MASK_HALT_HC_INT(ch_num);

        if (urb == NULL) {
            goto chhout;
        }

        urb->errorcode = chan->errorcode;

        if (urb->errorcode == 0) {
            uint32_t count = chan->xferlen - (USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_HCTSIZ_XFRSIZ);                          /* how many size has received */
            uint32_t has_used_packets = chan->num_packets - ((USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_DIEPTSIZ_PKTCNT) >> 19); /* how many packets have used */

            chan->xfrd += count;

            if ((has_used_packets % 2) == 1) /* toggle in odd numbers */
            {
                if (chan->data_pid == HC_PID_DATA0) {
                    chan->data_pid = HC_PID_DATA1;
                } else {
                    chan->data_pid = HC_PID_DATA0;
                }
            }

            if (chan->ep_type == 0x00) {
                if (chan->ep0_state == DWC2_EP0_STATE_INDATA) {
                    chan->ep0_state = DWC2_EP0_STATE_OUTSTATUS;
                    dwc2_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                } else if (chan->ep0_state == DWC2_EP0_STATE_INSTATUS) {
                    chan->ep0_state = DWC2_EP0_STATE_SETUP;
                    urb->actual_length = chan->xfrd;
                    dwc2_pipe_waitup(chan);
                }
            } else if (chan->ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                urb->iso_packet[chan->iso_frame_idx].actual_length = chan->xfrd;
                urb->iso_packet[chan->iso_frame_idx].errorcode = urb->errorcode;
                chan->iso_frame_idx++;

                if (chan->iso_frame_idx == urb->num_of_iso_packets) {
                    dwc2_pipe_waitup(chan);
                } else {
                    dwc2_iso_pipe_init(chan, &urb->iso_packet[chan->iso_frame_idx]);
                }
            } else {
                urb->actual_length = chan->xfrd;
                dwc2_pipe_waitup(chan);
            }
        } else if (urb->errorcode == -EAGAIN) {
            /* re-activate the channel */
            if (chan->ep_type == 0x00) {
                dwc2_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            } else if ((chan->ep_type == 0x03) || (chan->ep_type == 0x02)) {
                dwc2_bulk_intr_pipe_init(chan, urb->transfer_buffer, urb->transfer_buffer_length);
            } else {
            }
        } else {
            dwc2_pipe_waitup(chan);
        }
    chhout:
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_CHH);
    }
}

static void dwc2_outchan_irq_handler(uint8_t ch_num)
{
    uint32_t chan_intstatus;
    struct dwc2_pipe *chan;
    struct usbh_urb *urb;
    uint16_t buflen;

    chan_intstatus = (USB_OTG_HC(ch_num)->HCINT) & (USB_OTG_HC((uint32_t)ch_num)->HCINTMSK);

    chan = &g_dwc2_hcd.pipe_pool[ch_num];
    urb = chan->urb;
    //printf("s2:%08x\r\n", chan_intstatus);

    if ((chan_intstatus & USB_OTG_HCINT_XFRC) == USB_OTG_HCINT_XFRC) {
        chan->errorcode = 0;
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_XFRC);
        dwc2_halt(ch_num);
        USB_UNMASK_HALT_HC_INT(ch_num);
    } else if ((chan_intstatus & USB_OTG_HCINT_AHBERR) == USB_OTG_HCINT_AHBERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_AHBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_STALL) == USB_OTG_HCINT_STALL) {
        chan->errorcode = -EPERM;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_STALL);
    } else if ((chan_intstatus & USB_OTG_HCINT_NAK) == USB_OTG_HCINT_NAK) {
        chan->errorcode = -EAGAIN;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_ACK) == USB_OTG_HCINT_ACK) {
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_ACK);
    } else if ((chan_intstatus & USB_OTG_HCINT_NYET) == USB_OTG_HCINT_NYET) {
        chan->errorcode = -EAGAIN;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NYET);
    } else if ((chan_intstatus & USB_OTG_HCINT_TXERR) == USB_OTG_HCINT_TXERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_TXERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_BBERR) == USB_OTG_HCINT_BBERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_BBERR);
    } else if ((chan_intstatus & USB_OTG_HCINT_FRMOR) == USB_OTG_HCINT_FRMOR) {
        chan->errorcode = -EPIPE;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_FRMOR);
    } else if ((chan_intstatus & USB_OTG_HCINT_DTERR) == USB_OTG_HCINT_DTERR) {
        chan->errorcode = -EIO;
        USB_UNMASK_HALT_HC_INT(ch_num);
        dwc2_halt(ch_num);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_DTERR);
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_NAK);
    } else if ((chan_intstatus & USB_OTG_HCINT_CHH) == USB_OTG_HCINT_CHH) {
        USB_MASK_HALT_HC_INT(ch_num);

        if (urb == NULL) {
            goto chhout;
        }

        urb->errorcode = chan->errorcode;

        if (urb->errorcode == 0) {
            uint32_t count = USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_HCTSIZ_XFRSIZ;                                            /* how many size has sent */
            uint32_t has_used_packets = chan->num_packets - ((USB_OTG_HC(ch_num)->HCTSIZ & USB_OTG_DIEPTSIZ_PKTCNT) >> 19); /* how many packets have used */

            chan->xfrd += count;

            if (has_used_packets % 2) /* toggle in odd numbers */
            {
                if (chan->data_pid == HC_PID_DATA0) {
                    chan->data_pid = HC_PID_DATA1;
                } else {
                    chan->data_pid = HC_PID_DATA0;
                }
            }

            if (chan->ep_type == 0x00) {
                if (chan->ep0_state == DWC2_EP0_STATE_SETUP) {
                    if (urb->setup->wLength) {
                        if (urb->setup->bmRequestType & 0x80) {
                            chan->ep0_state = DWC2_EP0_STATE_INDATA;
                        } else {
                            chan->ep0_state = DWC2_EP0_STATE_OUTDATA;
                        }
                    } else {
                        chan->ep0_state = DWC2_EP0_STATE_INSTATUS;
                    }
                    dwc2_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                } else if (chan->ep0_state == DWC2_EP0_STATE_OUTDATA) {
                    chan->ep0_state = DWC2_EP0_STATE_INSTATUS;
                    dwc2_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                } else if (chan->ep0_state == DWC2_EP0_STATE_OUTSTATUS) {
                    chan->ep0_state = DWC2_EP0_STATE_SETUP;
                    urb->actual_length = chan->xfrd;
                    dwc2_pipe_waitup(chan);
                }
            } else if (chan->ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                urb->iso_packet[chan->iso_frame_idx].actual_length = chan->xfrd;
                urb->iso_packet[chan->iso_frame_idx].errorcode = urb->errorcode;
                chan->iso_frame_idx++;

                if (chan->iso_frame_idx == urb->num_of_iso_packets) {
                    dwc2_pipe_waitup(chan);
                } else {
                    dwc2_iso_pipe_init(chan, &urb->iso_packet[chan->iso_frame_idx]);
                }
            } else {
                urb->actual_length = chan->xfrd;
                dwc2_pipe_waitup(chan);
            }
        } else if (urb->errorcode == -EAGAIN) {
            /* re-activate the channel */
            if (chan->ep_type == 0x00) {
                dwc2_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            } else if ((chan->ep_type == 0x03) || (chan->ep_type == 0x02)) {
                dwc2_bulk_intr_pipe_init(chan, urb->transfer_buffer, urb->transfer_buffer_length);
            } else {
            }
        } else {
            dwc2_pipe_waitup(chan);
        }
    chhout:
        CLEAR_HC_INT(ch_num, USB_OTG_HCINT_CHH);
    }
}

static void dwc2_port_irq_handler(void)
{
    __IO uint32_t hprt0, hprt0_dup, regval;

    /* Handle Host Port Interrupts */
    hprt0 = USB_OTG_HPRT;
    hprt0_dup = USB_OTG_HPRT;

    hprt0_dup &= ~(USB_OTG_HPRT_PENA | USB_OTG_HPRT_PCDET |
                   USB_OTG_HPRT_PENCHNG | USB_OTG_HPRT_POCCHNG);

    /* Check whether Port Connect detected */
    if ((hprt0 & USB_OTG_HPRT_PCDET) == USB_OTG_HPRT_PCDET) {
        if ((hprt0 & USB_OTG_HPRT_PCSTS) == USB_OTG_HPRT_PCSTS) {
            usbh_roothub_thread_wakeup(1);
        }
        hprt0_dup |= USB_OTG_HPRT_PCDET;
        g_dwc2_hcd.port_csc = 1;
    }

    /* Check whether Port Enable Changed */
    if ((hprt0 & USB_OTG_HPRT_PENCHNG) == USB_OTG_HPRT_PENCHNG) {
        hprt0_dup |= USB_OTG_HPRT_PENCHNG;
        g_dwc2_hcd.port_pec = 1;

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
                }
            } else {
                USB_OTG_HOST->HFIR = 48000U;
                if ((USB_OTG_HOST->HCFG & USB_OTG_HCFG_FSLSPCS) != USB_OTG_HCFG_FSLSPCS_0) {
                    regval = USB_OTG_HOST->HCFG;
                    regval &= ~USB_OTG_HCFG_FSLSPCS;
                    regval |= USB_OTG_HCFG_FSLSPCS_0;
                    USB_OTG_HOST->HCFG = regval;
                }
            }
#endif
        } else {
        }
    }

    /* Check for an overcurrent */
    if ((hprt0 & USB_OTG_HPRT_POCCHNG) == USB_OTG_HPRT_POCCHNG) {
        hprt0_dup |= USB_OTG_HPRT_POCCHNG;
        g_dwc2_hcd.port_occ = 1;
    }
    /* Clear Port Interrupts */
    USB_OTG_HPRT = hprt0_dup;
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
            g_dwc2_hcd.port_csc = 1;
            usbh_roothub_thread_wakeup(1);

            USB_OTG_GLB->GINTSTS = USB_OTG_GINTSTS_DISCINT;
        }
        if (gint_status & USB_OTG_GINTSTS_HCINT) {
            chan_int = (USB_OTG_HOST->HAINT & USB_OTG_HOST->HAINTMSK) & 0xFFFFU;
            for (uint8_t i = 0U; i < CONFIG_USBHOST_PIPE_NUM; i++) {
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
    }
}