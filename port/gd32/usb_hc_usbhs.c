

#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_hs_reg.h"

#if defined(CONFIG_USB_HS0)
#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBHS0_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE USBHS0
#endif

#endif /* CONFIG_USB_HS0 */

#if defined(CONFIG_USB_HS1)
#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBHS1_IRQHandler
#endif

#ifndef USB_BASE
#define USB_BASE USBHS1
#endif

#endif /* CONFIG_USB_HS1 */

#ifndef CONFIG_USBHOST_CHANNELS
#define CONFIG_USBHOST_CHANNELS     16 /* Number of host channels */
#endif

#define USB_RX_FIFO_SIZE            512
#define USB_HTX_NPFIFO_SIZE         256
#define USB_HTX_PFIFO_SIZE          256

#define CONFIG_CONTROL_RETRY_COUNT  10

#define USBH_PID_SETUP              0U
#define USBH_PID_DATA               1U

typedef enum _usb_ch_mode
{
    CH_PERIOD     = 0U,
    CH_NON_PERIOD = 1U
} usb_ch_mode;

enum usb_synopsys_transfer_state {
    TRANSFER_IDLE = 0,
    TRANSFER_BUSY,
};

enum usb_ctl_status {
    USB_EP0_STATE_SETUP = 0U,
    USB_EP0_STATE_INDATA,
    USB_EP0_STATE_OUTDATA,
    USB_EP0_STATE_INSTATUS,
    USB_EP0_STATE_OUTSTATUS,
};

/* this structure retains the state of one host channel */
struct usb_ch {
    uint8_t              in_used;
    uint8_t              dev_addr;
    uint32_t             dev_speed;

    struct {
        uint8_t          num;
        uint8_t          dir;
        uint8_t          type;
        uint8_t          interval;
        uint16_t         mps;
    } ep;

    uint8_t              ch_index;
    volatile uint8_t     ep0_state;
    int                  err_code;
    uint32_t             DPID;

    uint8_t             *xfer_buf;
    uint16_t             packet_num;
    uint32_t             xfer_len;
    uint32_t             xfer_count;
    uint32_t             iso_frame_idx;

    volatile bool        waiter;
    usb_osal_sem_t       waitsem;
    struct usbh_hubport *hport;
    struct usbh_urb     *urb;
};

struct usb_hcd {
    volatile bool port_csc;
    volatile bool port_pec;
    volatile bool port_occ;
    struct usb_ch pipe_pool[CONFIG_USBHOST_PIPE_NUM];
} g_usb_hcd;

/* Driver state */
USB_NOCACHE_RAM_SECTION struct usb_hc_config_priv {
    usb_core_regs *Instance; /*!< Register base address */
    __attribute__((aligned(32))) struct usb_setup_packet setup;
} usb_hc_cfg;

static usb_core_regs *USBx = NULL;

static int usb_flush_rxfifo(void);
static int usb_flush_txfifo(uint32_t num);
static void usb_set_txfifo(uint8_t fifo, uint16_t size);
static int usb_txfifo_write(uint8_t *src_buf, uint8_t  fifo_num, uint16_t byte_count);
static void *usb_rxfifo_read(uint8_t *dest_buf, uint16_t byte_count);

static void usb_channel_init(uint8_t ch_num, uint8_t devaddr, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps, uint8_t speed);

extern void usb_vbus_drive (uint8_t state);

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

static inline void usb_drivebus (uint8_t state)
{
    uint32_t port = 0U;

    /* enable or disable the external charge pump */
    usb_vbus_drive (state);

    /* turn on the host port power. */
    port = *USBx->HPCS & ~(HPCS_PE | HPCS_PCD | HPCS_PEDC);

    if ((!(port & HPCS_PP)) && (1U == state)) {
        port |= HPCS_PP;
    }

    if ((port & HPCS_PP) && (0U == state)) {
        port &= ~HPCS_PP;
    }

    *USBx->HPCS = port;

    usb_osal_msleep(200U);
}

static inline void usb_channel_transfer(uint8_t ch_num, uint8_t ep_addr, uint32_t *buf, uint32_t size, uint8_t num_packets, uint32_t pid)
{
    __IO uint32_t ch_reg;
    uint8_t is_oddframe;
    uint16_t dword_len = 0U;
    uint32_t cheptype = (USBx->pr[ch_num]->HCHCTL & HCHCTL_EPTYPE) >> 18;

    USBx->pr[ch_num]->HCHLEN = ((size & HCHLEN_TLEN) | (((uint32_t)num_packets << 19) & HCHLEN_PCNT) | pid);

#ifdef USB_INTERNAL_DMA_ENABLED
    USBx->pr[ch_num]->HCHDMAADDR = (unsigned int)buf;
#endif

    is_oddframe = (((uint32_t)USBx->hr->HFINFR & 0x01U) != 0U) ? 0U : 1U;
    USBx->pr[ch_num]->HCHCTL &= ~HCHCTL_ODDFRM;
    USBx->pr[ch_num]->HCHCTL |= (uint32_t)is_oddframe << 29;

    ch_reg = USBx->pr[ch_num]->HCHCTL;
    ch_reg &= ~HCHCTL_CDIS;
    ch_reg |= HCHCTL_CEN;
    USBx->pr[ch_num]->HCHCTL = ch_reg;

#ifndef USB_INTERNAL_DMA_ENABLED
        if ((0U == (ep_addr & 0x80)) && (size > 0U)) {
            switch (cheptype) {
            /* non-periodic transfer */
            case USB_EPTYPE_CTRL:
            case USB_EPTYPE_BULK:
                dword_len = (uint16_t)((size + 3U) / 4U);

                /* check if there is enough space in FIFO space */
                if (dword_len > (USBx->gr->HNPTFQSTAT & HNPTFQSTAT_NPTXFS)) {
                    /* need to process data in nptxfempty interrupt */
                    USBx->gr->GINTEN |= GINTEN_NPTXFEIE;
                }
                break;

            /* periodic transfer */
            case USB_EPTYPE_INTR:
            case USB_EPTYPE_ISOC:
                dword_len = (uint16_t)((size + 3U) / 4U);

                /* check if there is enough space in FIFO space */
                if (dword_len > (USBx->hr->HPTFQSTAT & HPTFQSTAT_PTXFS)) {
                    /* need to process data in ptxfempty interrupt */
                    USBx->gr->GINTEN |= GINTEN_PTXFEIE;
                }
                break;

            default:
                break;
            }

            /* write packet into the TX FIFO. */
            usb_txfifo_write ((uint8_t *)buf, ch_num, (uint16_t)size);
        }
#endif
}

static void usb_channel_halt(uint8_t ch_num)
{
    volatile uint32_t count = 0U;
    uint32_t cheptype = (USBx->pr[ch_num]->HCHCTL & HCHCTL_EPTYPE) >> 18;
    uint32_t chena = (USBx->pr[ch_num]->HCHCTL & HCHCTL_CEN) >> 31;

    if (((USBx->gr->GAHBCS & GAHBCS_DMAEN) == GAHBCS_DMAEN) && (chena == 0U)) {
        return;
    }

    __IO uint32_t ch_ctl = USBx->pr[ch_num]->HCHCTL | (HCHCTL_CEN | HCHCTL_CDIS);

    switch (cheptype) {
    case USB_EPTYPE_CTRL:
    case USB_EPTYPE_BULK:
        if (0U == (USBx->gr->HNPTFQSTAT & HNPTFQSTAT_NPTXFS)) {
            ch_ctl &= ~HCHCTL_CEN;
        }
        break;

    case USB_EPTYPE_INTR:
    case USB_EPTYPE_ISOC:
        if (0U == (USBx->hr->HPTFQSTAT & HPTFQSTAT_PTXFS)) {
            ch_ctl &= ~HCHCTL_CEN;
        }
        break;

    default:
        break;
    }

    USBx->pr[ch_num]->HCHCTL = ch_ctl;
}

static void usbh_port_reset (const uint8_t port)
{
    __IO uint32_t port_value = *USBx->HPCS & ~(HPCS_PE | HPCS_PCD | HPCS_PEDC);

    *USBx->HPCS = port_value | HPCS_PRST;

    usb_osal_msleep(20U); /* see note */

    *USBx->HPCS = port_value & ~HPCS_PRST;

    usb_osal_msleep(20U);
}

uint8_t usbh_get_port_speed(const uint8_t port)
{
    uint32_t port_speed = *USBx->HPCS & HPCS_PS;

    if (PORT_SPEED_LOW == port_speed) {
        return USB_SPEED_LOW;
    } else if (PORT_SPEED_FULL == port_speed) {
        return USB_SPEED_FULL;
    } else if (PORT_SPEED_HIGH == port_speed) {
        return USB_SPEED_HIGH;
    } else {
        return USB_SPEED_UNKNOWN;
    }
}

static int usb_pipe_alloc(void)
{
    int ch_num;

    for (ch_num = 0; ch_num < CONFIG_USBHOST_PIPE_NUM; ch_num++) {
        if (!g_usb_hcd.pipe_pool[ch_num].in_used) {
            g_usb_hcd.pipe_pool[ch_num].in_used = true;
            return ch_num;
        }
    }

    return -1;
}

static void usb_pipe_free(struct usb_ch *pipe)
{
    pipe->in_used = false;
}


__WEAK void usb_hc_low_level_init(void)
{
}

static uint8_t usb_packet_num_calculate(uint32_t input_size, uint8_t ep_addr, uint16_t ep_mps, uint32_t *output_size)
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

static void usb_control_pipe_init(struct usb_ch *chan, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    uint8_t ep_addr = 0U;
    uint32_t size = 0U, data_pid = 0U;

    switch (chan->ep0_state) {
        case USB_EP0_STATE_SETUP:
            size = 8;
            chan->xfer_buf = (uint8_t *)setup;
            data_pid = PIPE_DPID_SETUP;
            break;

        case USB_EP0_STATE_INDATA:
            size = setup->wLength;
            ep_addr = 0x80U;
            chan->xfer_buf = buffer;
            data_pid = PIPE_DPID_DATA1;
            break;

        case USB_EP0_STATE_OUTDATA:
            size = setup->wLength;
            chan->xfer_buf = buffer;
            data_pid = PIPE_DPID_DATA1;
            break;

        case USB_EP0_STATE_INSTATUS:
            chan->xfer_buf = NULL;
            ep_addr = 0x80U;
            data_pid = PIPE_DPID_DATA1;
            break;

        case USB_EP0_STATE_OUTSTATUS:
            chan->xfer_buf = NULL;
            data_pid = PIPE_DPID_DATA1;
            break;

        default:
            break;
    }

    chan->packet_num = usb_packet_num_calculate(size, ep_addr, chan->ep.mps, &chan->xfer_len);
    usb_channel_init(chan->ch_index, chan->dev_addr, ep_addr, USB_ENDPOINT_TYPE_CONTROL, chan->ep.mps, chan->dev_speed);
    usb_channel_transfer(chan->ch_index, ep_addr, (uint32_t *)chan->xfer_buf, chan->xfer_len, chan->packet_num, data_pid);
}

static void usb_bulk_intr_pipe_init(struct usb_ch *chan, uint8_t *buffer, uint32_t buflen)
{
    chan->xfer_buf = buffer;
    chan->packet_num = usb_packet_num_calculate(buflen, chan->ep.num, chan->ep.mps, &chan->xfer_len);
    usb_channel_transfer(chan->ch_index, chan->ep.num, (uint32_t *)buffer, chan->xfer_len, chan->packet_num, chan->DPID);
}

static void usb_iso_pipe_init(struct usb_ch *chan, struct usbh_iso_frame_packet *iso_packet)
{
    chan->packet_num = usb_packet_num_calculate(iso_packet->transfer_buffer_length, chan->ep.num, chan->ep.mps, &chan->xfer_len);
    usb_channel_transfer(chan->ch_index, chan->ep.num, (uint32_t *)iso_packet->transfer_buffer, chan->xfer_len, chan->packet_num, PIPE_DPID_DATA0);
}

int usb_hc_init(void)
{
    uint32_t inten = 0U;
    uint32_t nptxfifolen = 0U;
    uint32_t ptxfifolen = 0U;

    memset(&g_usb_hcd, 0, sizeof(struct usb_hcd));
    memset(&usb_hc_cfg, 0, sizeof(struct usb_hc_config_priv));

    usb_hc_cfg.Instance = (usb_core_regs *)malloc(sizeof(usb_core_regs));

    USBx = usb_hc_cfg.Instance;

    USBx->gr = (usb_gr *)(USB_BASE + USB_REG_OFFSET_CORE);
    USBx->hr = (usb_hr *)(USB_BASE + USB_REG_OFFSET_HOST);

    USBx->HPCS = (uint32_t*) (USB_BASE + (uint32_t)USB_REG_OFFSET_PORT),
    USBx->PWRCLKCTL = (uint32_t *)(USB_BASE + USB_REG_OFFSET_PWRCLKCTL);

    /* assign device endpoint registers address */
    for (uint8_t i = 0U; i < CONFIG_USBHOST_CHANNELS; i++) {
        USBx->pr[i] = (usb_pr *)(USB_BASE + (uint32_t)USB_REG_OFFSET_CH_INOUT + (i * (uint32_t)USB_REG_OFFSET_CH));

        USBx->DFIFO[i] = (uint32_t *)(USB_BASE + USB_DATA_FIFO_OFFSET + (i * USB_DATA_FIFO_SIZE));
    }

    for (uint8_t ch_idx = 0; ch_idx < CONFIG_USBHOST_PIPE_NUM; ch_idx++) {
        g_usb_hcd.pipe_pool[ch_idx].waitsem = usb_osal_sem_create(0);
    }

    usb_hc_low_level_init();

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
    USBx->gr->GOTGCS |= GOTGCS_AVOV | GOTGCS_AVOE;

    /* active the transceiver */
    USBx->gr->GCCFG |= GCCFG_PWRON;
#endif /* VBUS_SENSING_ENABLED */

#if (USB_SOF_OUTPUT == 1)
    USBx->gr->GCCFG |= GCCFG_SOFOEN;
#endif /* USB_SOF_OUTPUT */

    usb_osal_msleep(20U);

#ifdef USB_INTERNAL_DMA_ENABLED
    USBx->gr->GAHBCS &= ~GAHBCS_BURST;
    USBx->gr->GAHBCS |= DMA_INCR8 | GAHBCS_DMAEN;
#endif /* USB_INTERNAL_DMA_ENABLED */

    /* force host mode */
    USBx->gr->GUSBCS &= ~(GUSBCS_FDM | GUSBCS_FHM);
    USBx->gr->GUSBCS |= GUSBCS_FHM;

    /* restart the PHY Clock */
    *USBx->PWRCLKCTL = 0U;

    /* support HS, FS and LS */
    USBx->hr->HCTL &= ~HCTL_SPDFSLS;

    /* set RX FIFO size */
    USBx->gr->GRFLEN = USB_RX_FIFO_SIZE;

    /* set non-periodic TX FIFO size and address */
    nptxfifolen |= USB_RX_FIFO_SIZE;
    nptxfifolen |= USB_HTX_NPFIFO_SIZE << 16U;
    USBx->gr->DIEP0TFLEN_HNPTFLEN = nptxfifolen;

    /* set periodic TX FIFO size and address */
    ptxfifolen |= USB_RX_FIFO_SIZE + USB_HTX_PFIFO_SIZE;
    ptxfifolen |= USB_HTX_PFIFO_SIZE << 16U;
    USBx->gr->HPTFLEN = ptxfifolen;


#ifdef USE_OTG_MODE
    /* clear host set HNP enable in the usb_otg control register */
    udev->regs.gr->GOTGCS &= ~GOTGCS_HHNPEN;
#endif /* USE_OTG_MODE */

    /* make sure the FIFOs are flushed */

    /* flush all TX FIFOs in device or host mode */
    usb_flush_txfifo(0x10U);

    /* flush the entire Rx FIFO */
    usb_flush_rxfifo();

    /* disable all interrupts */
    USBx->gr->GINTEN = 0U;

    /* clear any pending USB OTG interrupts */
    USBx->gr->GOTGINTF = 0xFFFFFFFFU;

    /* enable the USB wakeup and suspend interrupts */
    USBx->gr->GINTF = 0xBFFFFFFFU;

    /* clear all pending host channel interrupts */
    for (uint8_t i = 0U; i < CONFIG_USBHOST_PIPE_NUM; i++) {
        USBx->pr[i]->HCHINTF = 0xFFFFFFFFU;
        USBx->pr[i]->HCHINTEN = 0U;
    }

#ifndef USE_OTG_MODE
    usb_drivebus(1U);
#endif /* USE_OTG_MODE */

    /* enable host_mode-related interrupts */
#ifndef USB_INTERNAL_DMA_ENABLED
    inten = GINTEN_RXFNEIE;
#endif

    /* configure USBHS interrupts */
    inten |= GINTEN_HPIE | GINTEN_HCIE | GINTEN_DISCIE;
    USBx->gr->GINTEN |= inten;

    USBx->gr->GAHBCS |= GAHBCS_GINTEN;

    return 0;
}

uint16_t usbh_get_frame_number(void)
{
    return USBx->hr->HFINFR & 0xFFFFU;
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
                        *USBx->HPCS &= ~HPCS_PE;
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        g_usb_hcd.port_csc = 0;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        g_usb_hcd.port_pec = 0;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        g_usb_hcd.port_occ = 0;
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
                        *USBx->HPCS &= ~HPCS_PP;
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        usbh_port_reset(port);
                        break;

                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -EPIPE;
                }
                hprt0 = *USBx->HPCS;

                status = 0;
                if (g_usb_hcd.port_csc) {
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                }
                if (g_usb_hcd.port_pec) {
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }
                if (g_usb_hcd.port_occ) {
                    status |= (1 << HUB_PORT_FEATURE_C_OVER_CURREN);
                }

                if (hprt0 & HPCS_PCST) {
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                }
                if (hprt0 & HPCS_PE) {
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);
                    if (usbh_get_port_speed(port) == USB_SPEED_LOW) {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    } else if (usbh_get_port_speed(port) == USB_SPEED_HIGH) {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                }
//                if (hprt0 & USB_OTG_HPRT_POCA) {
//                    status |= (1 << HUB_PORT_FEATURE_OVERCURRENT);
//                }
                if (hprt0 & HPCS_PRST) {
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                }
                if (hprt0 & HPCS_PP) {
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
    struct usb_ch *chan;

    chan = (struct usb_ch *)pipe;

    chan->dev_addr = dev_addr;
    chan->ep.mps = ep_mps;

    return 0;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct usb_ch *chan;
    int ch_idx;
    usb_osal_sem_t waitsem;

    ch_idx = usb_pipe_alloc();
    if (ch_idx == -1) {
        return -ENOMEM;
    }

    chan = &g_usb_hcd.pipe_pool[ch_idx];

    /* store variables */
    waitsem = chan->waitsem;

    memset(chan, 0, sizeof(struct usb_ch));

    chan->ch_index = ch_idx;
    chan->ep.num = ep_cfg->ep_addr;
    chan->ep.type = ep_cfg->ep_type;
    chan->ep.mps = ep_cfg->ep_mps;
    chan->ep.interval = ep_cfg->ep_interval;
    chan->dev_speed = ep_cfg->hport->speed;
    chan->dev_addr = ep_cfg->hport->dev_addr;
    chan->hport = ep_cfg->hport;

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        chan->DPID = PIPE_DPID_DATA1;
    } else {
        usb_channel_init(ch_idx, chan->dev_addr, ep_cfg->ep_addr, ep_cfg->ep_type, ep_cfg->ep_mps, chan->dev_speed);
        chan->DPID = PIPE_DPID_DATA0;
    }

    if (chan->dev_speed == USB_SPEED_HIGH) {
        chan->ep.interval = (1 << (chan->ep.interval - 1));
    }

    /* restore variables */
    chan->in_used = true;
    chan->waitsem = waitsem;

    *pipe = (usbh_pipe_t)chan;

    return 0;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    struct usb_ch *chan;
    struct usbh_urb *urb;

    chan = (struct usb_ch *)pipe;

    if (!chan) {
        return -EINVAL;
    }

    urb = chan->urb;

    if (urb) {
        usbh_kill_urb(urb);
    }

    usb_pipe_free(chan);
    return 0;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    struct usb_ch *pipe;
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

    if (!(*USBx->HPCS & HPCS_PCST) || !pipe->hport->connected) {
        return -ENODEV;
    }

    if (pipe->urb) {
        return -EBUSY;
    }

    flags = usb_osal_enter_critical_section();

    pipe->waiter = false;
    pipe->xfer_buf = NULL;
    pipe->xfer_len = 0;
    pipe->xfer_count = 0;
    pipe->urb = urb;
    pipe->err_code = -EBUSY;
    urb->errorcode = -EBUSY;
    urb->actual_length = 0;

    if (urb->timeout > 0) {
        pipe->waiter = true;
    }
    usb_osal_leave_critical_section(flags);

    switch (pipe->ep.type) {
        case USB_ENDPOINT_TYPE_CONTROL:
            pipe->ep0_state = USB_EP0_STATE_SETUP;
            usb_control_pipe_init(pipe, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_BULK:
        case USB_ENDPOINT_TYPE_INTERRUPT:
            usb_bulk_intr_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            pipe->iso_frame_idx = 0;
            usb_iso_pipe_init(pipe, &urb->iso_packet[pipe->iso_frame_idx]);
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
    struct usb_ch *pipe;
    size_t flags;

    pipe = urb->pipe;

    if (!urb || !pipe) {
        return -EINVAL;
    }

    flags = usb_osal_enter_critical_section();

    usb_channel_halt(pipe->ch_index);
    USBx->pr[pipe->ch_index]->HCHINTF = HCHINTF_CH;
    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        urb->errorcode = -ESHUTDOWN;
        usb_osal_sem_give(pipe->waitsem);
    }

    usb_osal_leave_critical_section(flags);

    return 0;
}

static inline void usb_pipe_waitup(struct usb_ch *pipe)
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

static void usbh_int_txfifoempty (usb_ch_mode ch_mode)
{
    uint8_t ch_num = 0U;
    uint16_t word_count = 0U, len = 0U;
    __IO uint32_t *txfiforeg = 0U, txfifostate = 0U;

    if (CH_NON_PERIOD == ch_mode) {
        txfiforeg = &USBx->gr->HNPTFQSTAT;
    } else if (CH_PERIOD == ch_mode) {
        txfiforeg = &USBx->hr->HPTFQSTAT;
    } else {
        return;
    }

    txfifostate = *txfiforeg;

    ch_num = (uint8_t)((txfifostate & TFQSTAT_CNUM) >> 27U);

    word_count = (uint16_t)(g_usb_hcd.pipe_pool[ch_num].xfer_len + 3U) / 4U;

    while (((txfifostate & TFQSTAT_TXFS) >= word_count) && (0U != g_usb_hcd.pipe_pool[ch_num].xfer_len)) {
        len = (uint16_t)(txfifostate & TFQSTAT_TXFS) * 4U;

        if (len > g_usb_hcd.pipe_pool[ch_num].xfer_len) {
            /* last packet */
            len = (uint16_t)g_usb_hcd.pipe_pool[ch_num].xfer_len;

            if (CH_NON_PERIOD == ch_mode) {
                USBx->gr->GINTEN &= ~GINTEN_NPTXFEIE;
            } else {
                USBx->gr->GINTEN &= ~GINTEN_PTXFEIE;
            }
        }

        word_count = (uint16_t)((g_usb_hcd.pipe_pool[ch_num].xfer_len + 3U) / 4U);
        usb_txfifo_write (g_usb_hcd.pipe_pool[ch_num].xfer_buf, ch_num, len);

        g_usb_hcd.pipe_pool[ch_num].xfer_buf += len;
        g_usb_hcd.pipe_pool[ch_num].xfer_len -= len;
//        g_usb_hcd.pipe_pool[ch_num].xfer_count += len;

        txfifostate = *txfiforeg;
    }
}

static void usbh_int_rxfifonoempty(void)
{
    uint32_t count = 0U;

    __IO uint8_t ch_num = 0U;
    __IO uint32_t rx_stat = 0U;

    /* disable the RX status queue level interrupt */
    USBx->gr->GINTEN &= ~GINTEN_RXFNEIE;

    rx_stat = USBx->gr->GRSTATP;
    ch_num = (uint8_t)(rx_stat & GRSTATRP_CNUM);

    switch ((rx_stat & GRSTATRP_RPCKST) >> 17U) {
        case GRXSTS_PKTSTS_IN:
            count = (rx_stat & GRSTATRP_BCOUNT) >> 4U;

            /* read the data into the host buffer. */
            if ((NULL != g_usb_hcd.pipe_pool[ch_num].xfer_buf) && (count > 0U)) {
                (void)usb_rxfifo_read (g_usb_hcd.pipe_pool[ch_num].xfer_buf, (uint16_t)count);

                /* manage multiple transfer packet */
                g_usb_hcd.pipe_pool[ch_num].xfer_buf += count;

                if (USBx->pr[ch_num]->HCHLEN & HCHLEN_PCNT) {
                    /* re-activate the channel when more packets are expected */
                    uint32_t ch_ctl = USBx->pr[ch_num]->HCHCTL;

                    ch_ctl |= HCHCTL_CEN;
                    ch_ctl &= ~HCHCTL_CDIS;

                    USBx->pr[ch_num]->HCHCTL = ch_ctl;
                }
            }
            break;

        case GRXSTS_PKTSTS_IN_XFER_COMP:
            break;

        case GRXSTS_PKTSTS_DATA_TOGGLE_ERR:
            count = (rx_stat & GRSTATRP_BCOUNT) >> 4U;

            while (count > 0U) {
                rx_stat = USBx->gr->GRSTATP;
                count--;
            }
            break;

        case GRXSTS_PKTSTS_CH_HALTED:
            break;

        default:
            break;
    }

    /* enable the RX status queue level interrupt */
    USBx->gr->GINTEN |= GINTEN_RXFNEIE;
}

static void usb_ch_in_irq_handler(uint8_t ch_num)
{
    uint32_t chan_intstatus;
    struct usb_ch *chan;
    struct usbh_urb *urb;

    chan_intstatus = (USBx->pr[ch_num]->HCHINTF) & (USBx->pr[ch_num]->HCHINTEN);

    chan = &g_usb_hcd.pipe_pool[ch_num];
    urb = chan->urb;
    //printf("s1:%08x\r\n", chan_intstatus);

    if ((chan_intstatus & HCHINTF_TF) == HCHINTF_TF) {
        chan->err_code = 0;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_TF;
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NAK;
    } else if ((chan_intstatus & HCHINTF_STALL) == HCHINTF_STALL) {
        chan->err_code = -EPERM;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_STALL;
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NAK;
    } else if ((chan_intstatus & HCHINTF_NAK) == HCHINTF_NAK) {
        chan->err_code = -EAGAIN;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NAK;
    } else if ((chan_intstatus & HCHINTF_ACK) == HCHINTF_ACK) {
        USBx->pr[ch_num]->HCHINTF = HCHINTF_ACK;
    } else if ((chan_intstatus & HCHINTF_NYET) == HCHINTF_NYET) {
        chan->err_code = -EAGAIN;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NYET;
    } else if ((chan_intstatus & HCHINTF_USBER) == HCHINTF_USBER) {
        chan->err_code = -EIO;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_USBER;
    } else if ((chan_intstatus & HCHINTF_BBER) == HCHINTF_BBER) {
        chan->err_code = -EIO;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_BBER;
    } else if ((chan_intstatus & HCHINTF_REQOVR) == HCHINTF_REQOVR) {
        chan->err_code = -EPIPE;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_REQOVR;
    } else if ((chan_intstatus & HCHINTF_DTER) == HCHINTF_DTER) {
        chan->err_code = -EIO;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NAK;
        USBx->pr[ch_num]->HCHINTF = HCHINTF_DTER;
    } else if ((chan_intstatus & HCHINTF_CH) == HCHINTF_CH) {
        USBx->pr[ch_num]->HCHINTEN &= ~HCHINTEN_CHIE;

        if (urb == NULL) {
            goto chhout;
        }

        urb->errorcode = chan->err_code;

        if (urb->errorcode == 0) {
            uint32_t count = chan->xfer_len - (USBx->pr[ch_num]->HCHLEN & HCHLEN_TLEN);                          /* how many size has received */
            uint32_t has_used_packets = chan->packet_num - ((USBx->pr[ch_num]->HCHLEN & HCHLEN_PCNT) >> 19); /* how many packets have used */

            chan->xfer_count += count;

            if ((has_used_packets % 2) == 1) /* toggle in odd numbers */
            {
                if (chan->DPID == PIPE_DPID_DATA0) {
                    chan->DPID = PIPE_DPID_DATA1;
                } else {
                    chan->DPID = PIPE_DPID_DATA0;
                }
            }

            if (chan->ep.type == 0x00) {
                if (chan->ep0_state == USB_EP0_STATE_INDATA) {
//                    if (has_used_packets == chan->packet_num) {
                        chan->ep0_state = USB_EP0_STATE_OUTSTATUS;
//                    }
                    usb_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                } else if (chan->ep0_state == USB_EP0_STATE_INSTATUS) {
                    chan->ep0_state = USB_EP0_STATE_SETUP;
                    urb->actual_length = chan->xfer_count;
                    usb_pipe_waitup(chan);
                }
            } else if (chan->ep.type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                urb->iso_packet[chan->iso_frame_idx].actual_length = chan->xfer_count;
                urb->iso_packet[chan->iso_frame_idx].errorcode = urb->errorcode;
                chan->iso_frame_idx++;

                if (chan->iso_frame_idx == urb->num_of_iso_packets) {
                    usb_pipe_waitup(chan);
                } else {
                    usb_iso_pipe_init(chan, &urb->iso_packet[chan->iso_frame_idx]);
                }
            } else {
                urb->actual_length = chan->xfer_count;
                usb_pipe_waitup(chan);
            }
        } else if (urb->errorcode == -EAGAIN) {
            /* re-activate the channel */
            if (chan->ep.type == 0x00) {
//                usb_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                /* re-activate the channel */
                USBx->pr[chan->ch_index]->HCHCTL = (USBx->pr[chan->ch_index]->HCHCTL | HCHCTL_CEN) & ~HCHCTL_CDIS;
            } else if ((chan->ep.type == 0x03) || (chan->ep.type == 0x02)) {
                usb_bulk_intr_pipe_init(chan, urb->transfer_buffer, urb->transfer_buffer_length);
            } else {
            }
        } else {
            usb_pipe_waitup(chan);
        }
    chhout:
        USBx->pr[ch_num]->HCHINTF = HCHINTF_CH;
    }
}

static void usb_ch_out_irq_handler(uint8_t ch_num)
{
    uint32_t chan_intstatus;
    struct usb_ch *chan;
    struct usbh_urb *urb;
    uint16_t buflen;

    chan_intstatus = (USBx->pr[ch_num]->HCHINTF) & (USBx->pr[ch_num]->HCHINTEN);

    chan = &g_usb_hcd.pipe_pool[ch_num];
    urb = chan->urb;
    //printf("s2:%08x\r\n", chan_intstatus);

    if ((chan_intstatus & HCHINTF_TF) == HCHINTF_TF) {
        chan->err_code = 0;
        USBx->pr[ch_num]->HCHINTF = HCHINTF_TF;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
    } else if ((chan_intstatus & HCHINTF_STALL) == HCHINTF_STALL) {
        chan->err_code = -EPERM;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_STALL;
    } else if ((chan_intstatus & HCHINTF_NAK) == HCHINTF_NAK) {
        chan->err_code = -EAGAIN;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NAK;
    } else if ((chan_intstatus & HCHINTF_ACK) == HCHINTF_ACK) {
        USBx->pr[ch_num]->HCHINTF = HCHINTF_ACK;
    } else if ((chan_intstatus & HCHINTF_NYET) == HCHINTF_NYET) {
        chan->err_code = -EAGAIN;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NYET;
    } else if ((chan_intstatus & HCHINTF_USBER) == HCHINTF_USBER) {
        chan->err_code = -EIO;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_USBER;
    } else if ((chan_intstatus & HCHINTF_BBER) == HCHINTF_BBER) {
        chan->err_code = -EIO;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_BBER;
    } else if ((chan_intstatus & HCHINTF_REQOVR) == HCHINTF_REQOVR) {
        chan->err_code = -EPIPE;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_REQOVR;
    } else if ((chan_intstatus & HCHINTF_DTER) == HCHINTF_DTER) {
        chan->err_code = -EIO;
        USBx->pr[ch_num]->HCHINTEN |= HCHINTEN_CHIE;
        usb_channel_halt(ch_num);
        USBx->pr[ch_num]->HCHINTF = HCHINTF_DTER;
        USBx->pr[ch_num]->HCHINTF = HCHINTF_NAK;
    } else if ((chan_intstatus & HCHINTF_CH) == HCHINTF_CH) {
        USBx->pr[ch_num]->HCHINTEN &= ~HCHINTEN_CHIE;

        if (urb == NULL) {
            goto chhout;
        }

        urb->errorcode = chan->err_code;

        if (urb->errorcode == 0) {
            uint32_t count = USBx->pr[ch_num]->HCHLEN & HCHLEN_TLEN;                          /* how many size has received */
            uint32_t has_used_packets = chan->packet_num - ((USBx->pr[ch_num]->HCHLEN & HCHLEN_PCNT) >> 19); /* how many packets have used */

            chan->xfer_count += count;

            if (has_used_packets % 2) /* toggle in odd numbers */
            {
                if (chan->DPID == PIPE_DPID_DATA0) {
                    chan->DPID = PIPE_DPID_DATA1;
                } else {
                    chan->DPID = PIPE_DPID_DATA0;
                }
            }

            if (chan->ep.type == 0x00) {
                if (chan->ep0_state == USB_EP0_STATE_SETUP) {
                    if (urb->setup->wLength) {
                        if (urb->setup->bmRequestType & 0x80) {
                            chan->ep0_state = USB_EP0_STATE_INDATA;
                        } else {
                            chan->ep0_state = USB_EP0_STATE_OUTDATA;
                        }
                    } else {
                        chan->ep0_state = USB_EP0_STATE_INSTATUS;
                    }
                    usb_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                } else if (chan->ep0_state == USB_EP0_STATE_OUTDATA) {
                    chan->ep0_state = USB_EP0_STATE_INSTATUS;
                    usb_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
                } else if (chan->ep0_state == USB_EP0_STATE_OUTSTATUS) {
                    chan->ep0_state = USB_EP0_STATE_SETUP;
                    urb->actual_length = chan->xfer_count;
                    usb_pipe_waitup(chan);
                }
            } else if (chan->ep.type == USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                urb->iso_packet[chan->iso_frame_idx].actual_length = chan->xfer_count;
                urb->iso_packet[chan->iso_frame_idx].errorcode = urb->errorcode;
                chan->iso_frame_idx++;

                if (chan->iso_frame_idx == urb->num_of_iso_packets) {
                    usb_pipe_waitup(chan);
                } else {
                    usb_iso_pipe_init(chan, &urb->iso_packet[chan->iso_frame_idx]);
                }
            } else {
                urb->actual_length = chan->xfer_count;
                usb_pipe_waitup(chan);
            }
        } else if (urb->errorcode == -EAGAIN) {
            /* re-activate the channel */
            if (chan->ep.type == 0x00) {
                usb_control_pipe_init(chan, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            } else if ((chan->ep.type == 0x03) || (chan->ep.type == 0x02)) {
                usb_bulk_intr_pipe_init(chan, urb->transfer_buffer, urb->transfer_buffer_length);
            } else {
            }
        } else {
            usb_pipe_waitup(chan);
        }
    chhout:
        USBx->pr[ch_num]->HCHINTF = HCHINTF_CH;
    }
}

static void usb_port_irq_handler(void)
{
    __IO uint32_t port_state = *USBx->HPCS;
    __IO uint32_t port_value = *USBx->HPCS;
    __IO uint32_t port_reset = 0U;

    /* clear the interrupt bits in GINTSTS */
    port_state &= ~(HPCS_PE | HPCS_PCD | HPCS_PEDC);

    /* port connect detected */
    if ((port_value & HPCS_PCD) == HPCS_PCD) {
        if ((port_value & HPCS_PCST) == HPCS_PCST) {
            usbh_roothub_thread_wakeup(1);
        }

        port_state |= HPCS_PCD;
        g_usb_hcd.port_csc = 1;
    }

    /* port enable changed */
    if ((port_value & HPCS_PEDC) == HPCS_PEDC) {
        port_state |= HPCS_PEDC;

        g_usb_hcd.port_pec = 1;

        if ((port_value & HPCS_PE) == HPCS_PE) {
            uint32_t port_speed = port_value & HPCS_PS;

            if (PORT_SPEED_LOW == port_speed) {
                USBx->hr->HFT = 6000U;
                port_reset = 1U;
            } else if (PORT_SPEED_FULL == port_speed) {
                USBx->hr->HFT = 48000U;
            } else {
                /* no operation */
            }
        } else {
            /* no operation */
        }
    }

    if (1U == port_reset) {
        usbh_port_reset(0);
    }

    /* clear port interrupts */
    *USBx->HPCS = port_state;
}

void USBH_IRQHandler(void)
{
    uint32_t gint_status, chan_int;
    gint_status = USBx->gr->GINTEN & USBx->gr->GINTF;
    if ((USBx->gr->GINTF & GINTF_COPM) == HOST_MODE) {
        /* Avoid spurious interrupt */
        if (gint_status == 0) {
            return;
        }

        if (gint_status & GINTF_RXFNEIF) {
            usbh_int_rxfifonoempty();
        }

        if (gint_status & GINTF_NPTXFEIF) {
            usbh_int_txfifoempty(CH_NON_PERIOD);
        }

        if (gint_status & GINTF_PTXFEIF) {
            usbh_int_txfifoempty(CH_PERIOD);
        }

        if (gint_status & GINTF_HPIF) {
            usb_port_irq_handler();
        }

        if (gint_status & GINTF_DISCIF) {
            g_usb_hcd.port_csc = 1;
            usbh_roothub_thread_wakeup(1);

            USBx->gr->GINTF = GINTF_DISCIF;
        }

        if (gint_status & GINTF_HCIF) {
            chan_int = (USBx->hr->HACHINT & HACHINT_HACHINT) & 0xFFFFU;
            for (uint8_t i = 0U; i < CONFIG_USBHOST_PIPE_NUM; i++) {
                if ((chan_int & (1UL << (i & 0xFU))) != 0U) {
                    if ((USBx->pr[i]->HCHCTL & HCHCTL_EPDIR) == HCHCTL_EPDIR) {
                        usb_ch_in_irq_handler(i);
                    } else {
                        usb_ch_out_irq_handler(i);
                    }
                }
            }
            USBx->gr->GINTF = GINTF_HCIF;
        }
    }
}



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

static void usb_channel_init(uint8_t ch_num, uint8_t devaddr, uint8_t ep_addr, uint8_t ep_type, uint16_t ep_mps, uint8_t speed)
{
    __IO uint32_t pp_ctl = 0U;
    __IO uint32_t pp_inten = 0U;

    /* clear old interrupt conditions for this host channel */
    USBx->pr[ch_num]->HCHINTF = 0xFFFFFFFFU;

#ifdef USB_INTERNAL_DMA_ENABLED
    pp_inten |= HCHINTEN_DMAERIE;
#endif /* USB_INTERNAL_DMA_ENABLED */

    pp_inten = HCHINTEN_TFIE | 
               HCHINTEN_STALLIE | 
               HCHINTEN_USBERIE | 
               HCHINTEN_DTERIE;

    if ((ep_addr & 0x80U) == 0x80U) {
        pp_inten |= HCHINTEN_BBERIE;
    }

    /* enable channel interrupts required for this transfer */
    switch (ep_type) {
    case USB_EPTYPE_CTRL:
    case USB_EPTYPE_BULK:
        pp_inten |=  HCHINTEN_NAKIE;
        break;

    case USB_EPTYPE_INTR:
        pp_inten |= HCHINTEN_NAKIE | HCHINTEN_REQOVRIE;
        break;

    case USB_EPTYPE_ISOC:
        pp_inten |= HCHINTEN_REQOVRIE | HCHINTEN_ACKIE;
        break;

    default:
        break;
    }

    USBx->pr[ch_num]->HCHINTEN = pp_inten;

    /* enable the top level host channel interrupt */
    USBx->hr->HACHINTEN |= 1U << ch_num;

    /* make sure host channel interrupts are enabled */
    USBx->gr->GINTEN |= GINTEN_HCIE;

    /* program the host channel control register */
    pp_ctl |= PIPE_CTL_DAR(devaddr);
    pp_ctl |= PIPE_CTL_EPNUM(ep_addr & 0x7FU);
    pp_ctl |= PIPE_CTL_EPTYPE(ep_type);

    if ((ep_addr & 0x80U) == 0x80) {
        pp_ctl |= HCHCTL_EPDIR;
    } else {
        pp_ctl &= ~HCHCTL_EPDIR;
    }

    /* LS device plugged to HUB */
    if (speed == USB_SPEED_LOW) {
        pp_ctl |= HCHCTL_LSD;
    }

    pp_ctl |= ep_mps;
    pp_ctl |= ((uint32_t)(ep_type == USB_EPTYPE_INTR) << 29U) & HCHCTL_ODDFRM;

    USBx->pr[ch_num]->HCHCTL = pp_ctl;
}
