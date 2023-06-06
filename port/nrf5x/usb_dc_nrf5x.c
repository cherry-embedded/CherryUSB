#include <stddef.h>
#include "usb_dc.h"
#include "usbd_core.h"
#include "nrf5x_regs.h"

#define __ISB()               \
    do {                      \
        __schedule_barrier(); \
        __isb(0xF);           \
        __schedule_barrier(); \
    } while (0U)

#define __DSB()               \
    do {                      \
        __schedule_barrier(); \
        __dsb(0xF);           \
        __schedule_barrier(); \
    } while (0U)

#ifndef USBD_CONFIG_ISO_IN_ZLP
#define USBD_CONFIG_ISO_IN_ZLP 0
#endif

/*!< The default platform is NRF52840 */
#define NRF52_SERIES
#define NRF52840_XXAA

/*!< Ep nums */
#define EP_NUMS 9
/*!< Ep mps */
#define EP_MPS 64
/*!< Nrf5x special */
#define EP_ISO_NUM 8

/*!< USBD peripheral address base */
#define NRF_USBD_BASE 0x40027000UL
/*!< Clock peripheral address base */
#define NRF_CLOCK_BASE 0x40000000UL
/*!< NVIC peripheral address base */
#define NVIC_BASE (0xE000E000UL + 0x0100UL)

#define NRF_USBD  ((NRF_USBD_Type *)NRF_USBD_BASE)
#define NRF_CLOCK ((NRF_CLOCK_Type *)NRF_CLOCK_BASE)
#define NVIC      ((NVIC_Type *)NVIC_BASE)

#define USBD_IRQn 39

#ifndef EP_ISO_MPS
#define EP_ISO_MPS 64
#endif

#define CHECK_ADD_IS_RAM(address) ((((uint32_t)address) & 0xE0000000u) == 0x20000000u) ? 1 : 0

/**
 * @brief   Endpoint information structure
 */
typedef struct _usbd_ep_info {
    uint16_t mps;       /*!< Maximum packet length of endpoint */
    uint8_t eptype;     /*!< Endpoint Type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
    uint8_t ep_buffer[EP_MPS];
    /*!< Other endpoint parameters that may be used */
    volatile uint8_t is_using_dma;
    volatile uint8_t add_flag;
} usbd_ep_info;

/*!< nrf52840 usb */
struct _nrf52840_core_prvi {
    uint8_t address;               /*!< address */
    usbd_ep_info ep_in[EP_NUMS];   /*!< ep in */
    usbd_ep_info ep_out[EP_NUMS];  /*!< ep out */
    struct usb_setup_packet setup; /*!< Setup package that may be used in interrupt processing (outside the protocol stack) */
    volatile uint8_t dma_running;
    int8_t in_count;
    volatile uint8_t iso_turn;
    volatile uint8_t iso_tx_is_ready;
    /**
   * For nrf5x, easydma will not move the setup packet into RAM.
   * We use a flag bit to judge whether the host sends setup,
   * and then notify usbd_ep_read to and from the register to read the setup package
   */
    volatile uint8_t is_setup_packet;
} g_nrf5x_udc;

__WEAK void usb_dc_low_level_pre_init(void)
{
}

__WEAK void usb_dc_low_level_post_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

/**
 * @brief            Get setup package
 * @pre              None
 * @param[in]        setup Pointer to the address where the setup package is stored
 * @retval           None
 */
static inline void get_setup_packet(struct usb_setup_packet *setup)
{
    setup->bmRequestType = (uint8_t)(NRF_USBD->BMREQUESTTYPE);
    setup->bRequest = (uint8_t)(NRF_USBD->BREQUEST);
    setup->wIndex = (uint16_t)(NRF_USBD->WINDEXL | ((NRF_USBD->WINDEXH) << 8));
    setup->wLength = (uint16_t)(NRF_USBD->WLENGTHL | ((NRF_USBD->WLENGTHH) << 8));
    setup->wValue = (uint16_t)(NRF_USBD->WVALUEL | ((NRF_USBD->WVALUEH) << 8));
}

/**
 * @brief            Set tx easydma
 * @pre              None
 * @param[in]        ep      End point address
 * @param[in]        ptr     Data ram ptr
 * @param[in]        maxcnt  Max length
 * @retval           None
 */
static void nrf_usbd_ep_easydma_set_tx(uint8_t ep, uint32_t ptr, uint32_t maxcnt)
{
    uint8_t epid = USB_EP_GET_IDX(ep);
    if (epid == EP_ISO_NUM) {
        NRF_USBD->ISOIN.PTR = ptr;
        NRF_USBD->ISOIN.MAXCNT = maxcnt;
        return;
    }
    NRF_USBD->EPIN[epid].PTR = ptr;
    NRF_USBD->EPIN[epid].MAXCNT = maxcnt;
}

/**
 * @brief            Set rx easydma
 * @pre              None
 * @param[in]        ep      End point address
 * @param[in]        ptr     Data ram ptr
 * @param[in]        maxcnt  Max length
 * @retval           None
 */
static void nrf_usbd_ep_easydma_set_rx(uint8_t ep, uint32_t ptr, uint32_t maxcnt)
{
    uint8_t epid = USB_EP_GET_IDX(ep);
    if (epid == EP_ISO_NUM) {
        NRF_USBD->ISOOUT.PTR = ptr;
        NRF_USBD->ISOOUT.MAXCNT = maxcnt;
        return;
    }
    NRF_USBD->EPOUT[epid].PTR = ptr;
    NRF_USBD->EPOUT[epid].MAXCNT = maxcnt;
}

/**
 * @brief            USB initialization
 * @pre              None
 * @param[in]        None
 * @retval           >=0 success otherwise failure
 */
int nrf5x_udc_init(struct usbd_bus *bus)
{
    if (bus->busid != 0) {
        USB_LOG_ERR("nrf5x busid must be 0\r\n");
        return -1;
    }

    USB_LOG_INFO("========== nrf5x udc params =========\r\n");
    USB_LOG_INFO("nrf5x has %d endpoints\r\n", EP_NUMS);
    USB_LOG_INFO("=================================\r\n");

    /*!< dc init */
    usb_dc_low_level_pre_init();

    memset(&g_nrf5x_udc, 0, sizeof(g_nrf5x_udc));
    /*!< Clear USB Event Interrupt */
    NRF_USBD->EVENTS_USBEVENT = 0;
    NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;

    /*!< Reset interrupt */
    NRF_USBD->INTENCLR = NRF_USBD->INTEN;
    NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_EPDATA_Msk |
                         USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk | USBD_INTEN_STARTED_Msk;

    usb_dc_low_level_post_init();
    return 0;
}

int nrf5x_udc_deinit(struct usbd_bus *bus)
{
    return 0;
}

/**
 * @brief            Set address
 * @pre              None
 * @param[in]        address 8-bit valid address
 * @retval           >=0 success otherwise failure
 */
int nrf5x_set_address(struct usbd_bus *bus, const uint8_t address)
{
    if (address == 0) {
        /*!< init 0 address */
    } else {
        /*!< For non-0 addresses, write the address to the register in the state phase of setting the address */
    }

    NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;
    NRF_USBD->EVENTS_USBEVENT = 0;

    NRF_USBD->INTENSET = USBD_INTEN_USBEVENT_Msk;
    /*!< nothing to do, handled by hardware; but don't STALL */
    g_nrf5x_udc.address = address;
    return 0;
}

uint8_t nrf5x_get_port_speed(struct usbd_bus *bus)
{
    return USB_SPEED_FULL;
}

/**
 * @brief            Open endpoint
 * @pre              None
 * @param[in]        ep_cfg : Endpoint configuration structure pointer
 * @retval           >=0 success otherwise failure
 */
int nrf5x_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    if (USB_EP_DIR_IS_IN(ep_desc->bEndpointAddress)) {
        /*!< In */
        g_nrf5x_udc.ep_in[ep_idx].mps = ep_mps;
        g_nrf5x_udc.ep_in[ep_idx].eptype = ep_type;
        g_nrf5x_udc.ep_in[ep_idx].ep_enable = true;
        /*!< Open ep */
        if (ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            /*!< Enable endpoint interrupt */
            NRF_USBD->INTENSET = (1 << (USBD_INTEN_ENDEPIN0_Pos + ep_idx));
            /*!< Enable the in endpoint host to respond when sending in token */
            NRF_USBD->EPINEN |= (1 << (ep_idx));
            __ISB();
            __DSB();
        } else {
            NRF_USBD->EVENTS_ENDISOIN = 0;
            /*!< SPLIT ISO buffer when ISO OUT endpoint is already opened. */
            if (g_nrf5x_udc.ep_out[EP_ISO_NUM].mps)
                NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

            /*!< Clear SOF event in case interrupt was not enabled yet. */
            if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0)
                NRF_USBD->EVENTS_SOF = 0;

            /*!< Enable SOF and ISOIN interrupts, and ISOIN endpoint. */
            NRF_USBD->INTENSET = USBD_INTENSET_ENDISOIN_Msk | USBD_INTENSET_SOF_Msk;
            NRF_USBD->EPINEN |= USBD_EPINEN_ISOIN_Msk;
        }
    } else if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        /*!< Out */
        g_nrf5x_udc.ep_out[ep_idx].mps = ep_mps;
        g_nrf5x_udc.ep_out[ep_idx].eptype = ep_type;
        g_nrf5x_udc.ep_out[ep_idx].ep_enable = true;
        /*!< Open ep */
        if (ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            NRF_USBD->INTENSET = (1 << (USBD_INTEN_ENDEPOUT0_Pos + ep_idx));
            NRF_USBD->EPOUTEN |= (1 << (ep_idx));
            __ISB();
            __DSB();
            /*!< Write any value to SIZE register will allow nRF to ACK/accept data */
            NRF_USBD->SIZE.EPOUT[ep_idx] = 0;
        } else {
            /*!< SPLIT ISO buffer when ISO IN endpoint is already opened. */
            if (g_nrf5x_udc.ep_in[EP_ISO_NUM].mps)
                NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

            /*!< Clear old events */
            NRF_USBD->EVENTS_ENDISOOUT = 0;

            /*!< Clear SOF event in case interrupt was not enabled yet. */
            if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0)
                NRF_USBD->EVENTS_SOF = 0;

            /*!< Enable SOF and ISOOUT interrupts, and ISOOUT endpoint. */
            NRF_USBD->INTENSET = USBD_INTENSET_ENDISOOUT_Msk | USBD_INTENSET_SOF_Msk;
            NRF_USBD->EPOUTEN |= USBD_EPOUTEN_ISOOUT_Msk;
        }
    }

    /*!< Clear stall and reset DataToggle */
    NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | (ep_desc->bEndpointAddress);
    NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | (ep_desc->bEndpointAddress);

    __ISB();
    __DSB();

    return 0;
}

/**
 * @brief            Close endpoint
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @retval           >=0 success otherwise failure
 */
int nrf5x_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    /*!< ep id */
    uint8_t epid = USB_EP_GET_IDX(ep);
    
    if (epid != EP_ISO_NUM) {
        if (USB_EP_DIR_IS_OUT(ep)) {
            g_nrf5x_udc.ep_out[epid].ep_enable = false;
            NRF_USBD->INTENCLR = (1 << (USBD_INTEN_ENDEPOUT0_Pos + epid));
            NRF_USBD->EPOUTEN &= ~(1 << (epid));
        } else {
            g_nrf5x_udc.ep_in[epid].ep_enable = false;
            NRF_USBD->INTENCLR = (1 << (USBD_INTEN_ENDEPIN0_Pos + epid));
            NRF_USBD->EPINEN &= ~(1 << (epid));
        }
    } else {
        /*!< ISO EP */
        if (USB_EP_DIR_IS_OUT(ep)) {
            g_nrf5x_udc.ep_out[epid].ep_enable = false;
            g_nrf5x_udc.ep_out[EP_ISO_NUM].mps = 0;
            NRF_USBD->INTENCLR = USBD_INTENCLR_ENDISOOUT_Msk;
            NRF_USBD->EPOUTEN &= ~USBD_EPOUTEN_ISOOUT_Msk;
            NRF_USBD->EVENTS_ENDISOOUT = 0;
        } else {
            g_nrf5x_udc.ep_in[epid].ep_enable = false;
            g_nrf5x_udc.ep_in[EP_ISO_NUM].mps = 0;
            NRF_USBD->INTENCLR = USBD_INTENCLR_ENDISOIN_Msk;
            NRF_USBD->EPINEN &= ~USBD_EPINEN_ISOIN_Msk;
        }
        /*!< One of the ISO endpoints closed, no need to split buffers any more. */
        NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_OneDir;
        /*!< When both ISO endpoint are close there is no need for SOF any more. */
        if (g_nrf5x_udc.ep_in[EP_ISO_NUM].mps + g_nrf5x_udc.ep_out[EP_ISO_NUM].mps == 0) {
            NRF_USBD->INTENCLR = USBD_INTENCLR_SOF_Msk;
        }
    }
    __ISB();
    __DSB();

    return 0;
}

/**
 * @brief            Endpoint setting pause
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @retval           >=0 success otherwise failure
 */
int nrf5x_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    /*!< ep id */
    uint8_t epid = USB_EP_GET_IDX(ep);
    if (epid == 0) {
        NRF_USBD->TASKS_EP0STALL = 1;
    } else if (epid != EP_ISO_NUM) {
        NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_Stall << USBD_EPSTALL_STALL_Pos) | (ep);
    }
    __ISB();
    __DSB();
    return 0;
}

/**
 * @brief            Endpoint clear pause
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @retval           >=0 success otherwise failure
 */
int nrf5x_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    /*!< ep id */
    uint8_t epid = USB_EP_GET_IDX(ep);

    if (epid != 0 && epid != EP_ISO_NUM) {
        /**
     * reset data toggle to DATA0
     * First write this register with VALUE=Nop to select the endpoint, then either read it to get the status from
     * VALUE, or write it again with VALUE=Data0 or Data1
     */
        NRF_USBD->DTOGGLE = ep;
        NRF_USBD->DTOGGLE = (USBD_DTOGGLE_VALUE_Data0 << USBD_DTOGGLE_VALUE_Pos) | ep;

        /*!< Clear stall */
        NRF_USBD->EPSTALL = (USBD_EPSTALL_STALL_UnStall << USBD_EPSTALL_STALL_Pos) | ep;

        /*!< Write any value to SIZE register will allow nRF to ACK/accept data */
        if (USB_EP_DIR_IS_OUT(ep))
            NRF_USBD->SIZE.EPOUT[epid] = 0;

        __ISB();
        __DSB();
    }

    return 0;
}

/**
 * @brief            Check endpoint status
 * @pre              None
 * @param[in]        ep ： Endpoint address
 * @param[out]       stalled ： Outgoing endpoint status
 * @retval           >=0 success otherwise failure
 */
int nrf5x_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

/**
 * @brief Setup in ep transfer setting and start transfer.
 *
 * This function is asynchronous.
 * This function is similar to uart with tx dma.
 *
 * This function is called to write data to the specified endpoint. The
 * supplied usbd_endpoint_callback function will be called when data is transmitted
 * out.
 *
 * @param[in]  ep        Endpoint address corresponding to the one
 *                       listed in the device configuration table
 * @param[in]  data      Pointer to data to write
 * @param[in]  data_len  Length of the data requested to write. This may
 *                       be zero for a zero length status packet.
 * @return 0 on success, negative errno code on fail.
 */
int nrf5x_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_nrf5x_udc.ep_in[ep_idx].ep_enable) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }

    g_nrf5x_udc.ep_in[ep_idx].xfer_buf = (uint8_t *)data;
    g_nrf5x_udc.ep_in[ep_idx].xfer_len = data_len;
    g_nrf5x_udc.ep_in[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        /*!< write 0 len data */
        nrf_usbd_ep_easydma_set_tx(ep_idx, NULL, 0);
        NRF_USBD->TASKS_STARTEPIN[ep_idx] = 1;
    } else {
        /*!< Not zlp */
        data_len = MIN(data_len, g_nrf5x_udc.ep_in[ep_idx].mps);
        if (!CHECK_ADD_IS_RAM(data)) {
            /*!< Data is not in ram */
            /*!< Memcpy data to ram */
            memcpy(g_nrf5x_udc.ep_in[ep_idx].ep_buffer, data, data_len);
            nrf_usbd_ep_easydma_set_tx(ep_idx, (uint32_t)g_nrf5x_udc.ep_in[ep_idx].ep_buffer, data_len);
        } else {
            nrf_usbd_ep_easydma_set_tx(ep_idx, (uint32_t)data, data_len);
        }
        /**
     * Note that starting DMA transmission is to transmit data to USB peripherals,
     * and then wait for the host to get it
     */
        /*!< Start dma transfer */
        if (ep_idx != EP_ISO_NUM) {
            NRF_USBD->TASKS_STARTEPIN[ep_idx] = 1;
        } else {
            // NRF_USBD->TASKS_STARTISOIN = 1;
            g_nrf5x_udc.iso_tx_is_ready = 1;
        }
    }
    return 0;
}

/**
 * @brief Setup out ep transfer setting and start transfer.
 *
 * This function is asynchronous.
 * This function is similar to uart with rx dma.
 *
 * This function is called to read data to the specified endpoint. The
 * supplied usbd_endpoint_callback function will be called when data is received
 * in.
 *
 * @param[in]  ep        Endpoint address corresponding to the one
 *                       listed in the device configuration table
 * @param[in]  data      Pointer to data to read
 * @param[in]  data_len  Max length of the data requested to read.
 *
 * @return 0 on success, negative errno code on fail.
 */
int nrf5x_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_nrf5x_udc.ep_out[ep_idx].ep_enable) {
        return -2;
    }
    if ((uint32_t)data & 0x03) {
        return -3;
    }

    g_nrf5x_udc.ep_out[ep_idx].xfer_buf = (uint8_t *)data;
    g_nrf5x_udc.ep_out[ep_idx].xfer_len = data_len;
    g_nrf5x_udc.ep_out[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        return 0;
    } else {
        data_len = MIN(data_len, g_nrf5x_udc.ep_out[ep_idx].mps);
        if (!CHECK_ADD_IS_RAM(data)) {
            /*!< Data address is not in ram */
            // TODO:
        } else {
            if (ep_idx == 0) {
                NRF_USBD->TASKS_EP0RCVOUT = 1;
            }
            nrf_usbd_ep_easydma_set_rx(ep_idx, (uint32_t)data, data_len);
        }
    }
    return 0;
}

/**
 * @brief            USB interrupt processing function
 * @pre              None
 * @param[in]        None
 * @retval           None
 */
void nrf5x_udc_irq(struct usbd_bus *bus)
{
    uint32_t const inten = NRF_USBD->INTEN;
    uint32_t int_status = 0;
    volatile uint32_t usb_event = 0;
    volatile uint32_t *regevt = &NRF_USBD->EVENTS_USBRESET;

    /*!< Traverse USB events */
    for (uint8_t i = 0; i < USBD_INTEN_EPDATA_Pos + 1; i++) {
        if ((inten & (1 << i)) && regevt[i]) {
            int_status |= (1 << (i));
            /*!< event clear */
            regevt[i] = 0;
            __ISB();
            __DSB();
        }
    }

    /*!< bit 24 */
    if (int_status & USBD_INTEN_EPDATA_Msk) {
        /*!< out ep */
        for (uint8_t ep_out_ct = 1; ep_out_ct <= 7; ep_out_ct++) {
            if ((NRF_USBD->EPDATASTATUS) & (1 << (16 + ep_out_ct))) {
                NRF_USBD->EPDATASTATUS |= (1 << (16 + ep_out_ct));
                /*!< The data arrives at the usb fifo, starts the dma transmission, and transfers it to the user ram  */
                NRF_USBD->TASKS_STARTEPOUT[ep_out_ct] = 1;
            }
        }
        /*!< in ep */
        for (uint8_t ep_in_ct = 1; ep_in_ct <= 7; ep_in_ct++) {
            if ((NRF_USBD->EPDATASTATUS) & (1 << (0 + ep_in_ct))) {
                /*!< in ep tranfer to host successfully */
                NRF_USBD->EPDATASTATUS |= (1 << (0 + ep_in_ct));
                if (g_nrf5x_udc.ep_in[ep_in_ct].xfer_len > g_nrf5x_udc.ep_in[ep_in_ct].mps) {
                    /*!< Need start in again */
                    g_nrf5x_udc.ep_in[ep_in_ct].xfer_buf += g_nrf5x_udc.ep_in[ep_in_ct].mps;
                    g_nrf5x_udc.ep_in[ep_in_ct].xfer_len -= g_nrf5x_udc.ep_in[ep_in_ct].mps;
                    g_nrf5x_udc.ep_in[ep_in_ct].actual_xfer_len += g_nrf5x_udc.ep_in[ep_in_ct].mps;
                    if (g_nrf5x_udc.ep_in[ep_in_ct].xfer_len > g_nrf5x_udc.ep_in[ep_in_ct].mps) {
                        nrf_usbd_ep_easydma_set_tx(ep_in_ct, (uint32_t)g_nrf5x_udc.ep_in[ep_in_ct].xfer_buf, g_nrf5x_udc.ep_in[ep_in_ct].mps);
                    } else {
                        nrf_usbd_ep_easydma_set_tx(ep_in_ct, (uint32_t)g_nrf5x_udc.ep_in[ep_in_ct].xfer_buf, g_nrf5x_udc.ep_in[ep_in_ct].xfer_len);
                    }
                    NRF_USBD->TASKS_STARTEPIN[ep_in_ct] = 1;
                } else {
                    g_nrf5x_udc.ep_in[ep_in_ct].actual_xfer_len += g_nrf5x_udc.ep_in[ep_in_ct].xfer_len;
                    g_nrf5x_udc.ep_in[ep_in_ct].xfer_len = 0;
                    usbd_event_ep_in_complete_handler(0, ep_in_ct | 0x80, g_nrf5x_udc.ep_in[ep_in_ct].actual_xfer_len);
                }
            }
        }
    }

    /*!< bit 23 */
    if (int_status & USBD_INTEN_EP0SETUP_Msk) {
        /* Setup */
        /*!< Storing this setup package will help the following procedures */
        get_setup_packet(&(g_nrf5x_udc.setup));
        /*!< Nrf52840 will set the address automatically by hardware,
         so the protocol stack of the address setting command sent by the host does not need to be processed */

        if (g_nrf5x_udc.setup.wLength == 0) {
            NRF_USBD->TASKS_EP0STATUS = 1;
        }

        if (g_nrf5x_udc.setup.bRequest != USB_REQUEST_SET_ADDRESS) {
            usbd_event_ep0_setup_complete_handler(0, (uint8_t *)&(g_nrf5x_udc.setup));
        }
    }

    /*!< bit 22 */
    if (int_status & USBD_INTEN_USBEVENT_Msk) {
        usb_event = NRF_USBD->EVENTCAUSE;
        NRF_USBD->EVENTCAUSE = usb_event;
        if (usb_event & USBD_EVENTCAUSE_SUSPEND_Msk) {
            NRF_USBD->LOWPOWER = 1;
            /*!<  */
        }
        if (usb_event & USBD_EVENTCAUSE_RESUME_Msk) {
            /*!<  */
        }
        if (usb_event & USBD_EVENTCAUSE_USBWUALLOWED_Msk) {
            NRF_USBD->DPDMVALUE = USBD_DPDMVALUE_STATE_Resume;
            NRF_USBD->TASKS_DPDMDRIVE = 1;
            /**
       * There is no Resume interrupt for remote wakeup, enable SOF for to report bus ready state
       * Clear SOF event in case interrupt was not enabled yet.
       */
            if ((NRF_USBD->INTEN & USBD_INTEN_SOF_Msk) == 0)
                NRF_USBD->EVENTS_SOF = 0;
            NRF_USBD->INTENSET = USBD_INTENSET_SOF_Msk;
        }
    }

    /*!< bit 21 */
    if (int_status & USBD_INTEN_SOF_Msk) {
        bool iso_enabled = false;
        /*!< ISOOUT: Transfer data gathered in previous frame from buffer to RAM */
        if (NRF_USBD->EPOUTEN & USBD_EPOUTEN_ISOOUT_Msk) {
            iso_enabled = true;
            /*!< If ZERO bit is set, ignore ISOOUT length */
            if ((NRF_USBD->SIZE.ISOOUT) & USBD_SIZE_ISOOUT_ZERO_Msk) {
            } else {
                /*!< Trigger DMA move data from Endpoint -> SRAM */
                NRF_USBD->TASKS_STARTISOOUT = 1;
                /*!< EP_ISO_NUM out using dma */
                g_nrf5x_udc.ep_out[EP_ISO_NUM].is_using_dma = 1;
            }
        }

        /*!< ISOIN: Notify client that data was transferred */
        if (NRF_USBD->EPINEN & USBD_EPINEN_ISOIN_Msk) {
            iso_enabled = true;
            if (g_nrf5x_udc.iso_tx_is_ready == 1) {
                g_nrf5x_udc.iso_tx_is_ready = 0;
                NRF_USBD->TASKS_STARTISOIN = 1;
            }
        }

        if (!iso_enabled) {
            /**
       * ISO endpoint is not used, SOF is only enabled one-time for remote wakeup
       * so we disable it now
       */
            NRF_USBD->INTENCLR = USBD_INTENSET_SOF_Msk;
        }
    }

    /*!< bit 20 */
    if (int_status & USBD_INTEN_ENDISOOUT_Msk) {
        if (g_nrf5x_udc.ep_out[EP_ISO_NUM].is_using_dma == 1) {
            g_nrf5x_udc.ep_out[EP_ISO_NUM].is_using_dma = 0;
            uint32_t read_count = NRF_USBD->SIZE.ISOOUT;
            g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_buf += read_count;
            g_nrf5x_udc.ep_out[EP_ISO_NUM].actual_xfer_len += read_count;
            g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_len -= read_count;

            if ((read_count < g_nrf5x_udc.ep_out[EP_ISO_NUM].mps) || (g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_len == 0)) {
                usbd_event_ep_out_complete_handler(0, ((EP_ISO_NUM)&0x7f), g_nrf5x_udc.ep_out[EP_ISO_NUM].actual_xfer_len);
            } else {
                if (g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_len > g_nrf5x_udc.ep_out[EP_ISO_NUM].mps) {
                    nrf_usbd_ep_easydma_set_rx(((EP_ISO_NUM)&0x7f), (uint32_t)g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_buf, g_nrf5x_udc.ep_out[EP_ISO_NUM].mps);
                } else {
                    nrf_usbd_ep_easydma_set_rx(((EP_ISO_NUM)&0x7f), (uint32_t)g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_buf, g_nrf5x_udc.ep_out[EP_ISO_NUM].xfer_len);
                }
            }
        }
    }

    /**
   * Traverse ordinary out endpoint events, starting from endpoint 1 to endpoint 7,
   * and end 0 for special processing
   */
    for (uint8_t offset = 0; offset < 7; offset++) {
        if (int_status & (USBD_INTEN_ENDEPOUT1_Msk << offset)) {
            /*!< Out 'offset' transfer complete */
            uint32_t read_count = NRF_USBD->SIZE.EPOUT[offset + 1];
            g_nrf5x_udc.ep_out[offset + 1].xfer_buf += read_count;
            g_nrf5x_udc.ep_out[offset + 1].actual_xfer_len += read_count;
            g_nrf5x_udc.ep_out[offset + 1].xfer_len -= read_count;

            if ((read_count < g_nrf5x_udc.ep_out[offset + 1].mps) || (g_nrf5x_udc.ep_out[offset + 1].xfer_len == 0)) {
                usbd_event_ep_out_complete_handler(0, ((offset + 1) & 0x7f), g_nrf5x_udc.ep_out[offset + 1].actual_xfer_len);
            } else {
                if (g_nrf5x_udc.ep_out[offset + 1].xfer_len > g_nrf5x_udc.ep_out[offset + 1].mps) {
                    nrf_usbd_ep_easydma_set_rx(((offset + 1) & 0x7f), (uint32_t)g_nrf5x_udc.ep_out[offset + 1].xfer_buf, g_nrf5x_udc.ep_out[offset + 1].mps);
                } else {
                    nrf_usbd_ep_easydma_set_rx(((offset + 1) & 0x7f), (uint32_t)g_nrf5x_udc.ep_out[offset + 1].xfer_buf, g_nrf5x_udc.ep_out[offset + 1].xfer_len);
                }
            }
        }
    }

    /*!< bit 12 */
    if (int_status & USBD_INTEN_ENDEPOUT0_Msk) {
        uint32_t read_count = NRF_USBD->SIZE.EPOUT[0];
        g_nrf5x_udc.ep_out[0].actual_xfer_len += read_count;
        g_nrf5x_udc.ep_out[0].xfer_len -= read_count;

        if (g_nrf5x_udc.ep_out[0].xfer_len == 0) {
            /*!< Enable the state phase of endpoint 0 */
            NRF_USBD->TASKS_EP0STATUS = 1;
        }

        usbd_event_ep_out_complete_handler(0, 0x00, g_nrf5x_udc.ep_out[0].actual_xfer_len);
    }

    /*!< bit 11 */
    if (int_status & USBD_INTEN_ENDISOIN_Msk) {
    }

    /*!< bit 10 */
    if (int_status & USBD_INTEN_EP0DATADONE_Msk) {
        switch (g_nrf5x_udc.setup.bmRequestType >> USB_REQUEST_DIR_SHIFT) {
            case 1:
                /*!< IN */
                if (g_nrf5x_udc.ep_in[0].xfer_len > g_nrf5x_udc.ep_in[0].mps) {
                    g_nrf5x_udc.ep_in[0].xfer_len -= g_nrf5x_udc.ep_in[0].mps;
                    g_nrf5x_udc.ep_in[0].actual_xfer_len += g_nrf5x_udc.ep_in[0].mps;
                    usbd_event_ep_in_complete_handler(0, 0 | 0x80, g_nrf5x_udc.ep_in[0].actual_xfer_len);
                } else {
                    g_nrf5x_udc.ep_in[0].actual_xfer_len += g_nrf5x_udc.ep_in[0].xfer_len;
                    g_nrf5x_udc.ep_in[0].xfer_len = 0;
                    /*!< Enable the state phase of endpoint 0 */
                    usbd_event_ep_in_complete_handler(0, 0 | 0x80, g_nrf5x_udc.ep_in[0].actual_xfer_len);
                    NRF_USBD->TASKS_EP0STATUS = 1;
                }
                break;
            case 0:
                if (g_nrf5x_udc.setup.bRequest != USB_REQUEST_SET_ADDRESS) {
                    /*!< The data arrives at the usb fifo, starts the dma transmission, and transfers it to the user ram  */
                    NRF_USBD->TASKS_STARTEPOUT[0] = 1;
                }
                break;
        }
    }

    /**
   * Traversing ordinary in endpoint events, starting from endpoint 1 to endpoint 7,
   * endpoint 0 special processing
   */
    for (uint8_t offset = 0; offset < 7; offset++) {
        if (int_status & (USBD_INTEN_ENDEPIN1_Msk << offset)) {
            /*!< DMA move data completed */
        }
    }

    /*!< bit 1 */
    if (int_status & USBD_INTEN_STARTED_Msk) {
        /*!< Easy dma start transfer data */
    }

    /*!< bit 2 */
    if (int_status & USBD_INTEN_ENDEPIN0_Msk) {
        /*!< EP0 IN DMA move data completed */
    }

    /*!< bit 0 */
    if (int_status & USBD_INTEN_USBRESET_Msk) {
        NRF_USBD->EPOUTEN = 1UL;
        NRF_USBD->EPINEN = 1UL;

        for (int i = 0; i < 8; i++) {
            NRF_USBD->TASKS_STARTEPIN[i] = 0;
            NRF_USBD->TASKS_STARTEPOUT[i] = 0;
        }

        NRF_USBD->TASKS_STARTISOIN = 0;
        NRF_USBD->TASKS_STARTISOOUT = 0;

        /*!< Clear USB Event Interrupt */
        NRF_USBD->EVENTS_USBEVENT = 0;
        NRF_USBD->EVENTCAUSE |= NRF_USBD->EVENTCAUSE;

        /*!< Reset interrupt */
        NRF_USBD->INTENCLR = NRF_USBD->INTEN;
        NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk | USBD_INTEN_USBEVENT_Msk | USBD_INTEN_EPDATA_Msk |
                             USBD_INTEN_EP0SETUP_Msk | USBD_INTEN_EP0DATADONE_Msk | USBD_INTEN_ENDEPIN0_Msk | USBD_INTEN_ENDEPOUT0_Msk | USBD_INTEN_STARTED_Msk;

        usbd_event_reset_handler(0);
    }
}

struct usbd_udc_driver nrf5x_udc_driver = {
    .driver_name = "nrf5x udc",
    .udc_init = nrf5x_udc_init,
    .udc_deinit = nrf5x_udc_deinit,
    .udc_set_address = nrf5x_set_address,
    .udc_get_port_speed = nrf5x_get_port_speed,
    .udc_ep_open = nrf5x_ep_open,
    .udc_ep_close = nrf5x_ep_close,
    .udc_ep_set_stall = nrf5x_ep_set_stall,
    .udc_ep_clear_stall = nrf5x_ep_clear_stall,
    .udc_ep_is_stalled = nrf5x_ep_is_stalled,
    .udc_ep_start_write = nrf5x_ep_start_write,
    .udc_ep_start_read = nrf5x_ep_start_read,
    .udc_irq = nrf5x_udc_irq
};

void USBD_IRQHandler(void)
{
    usbd_irq(0);
}

/**
 * Errata: USB cannot be enabled.
 */
static bool chyu_nrf52_errata_187(void)
{
#ifndef NRF52_SERIES
    return false;
#else
#if defined(NRF52820_XXAA) || defined(DEVELOP_IN_NRF52820) || defined(NRF52833_XXAA) || defined(DEVELOP_IN_NRF52833) || defined(NRF52840_XXAA) || defined(DEVELOP_IN_NRF52840)
    uint32_t var1 = *(uint32_t *)0x10000130ul;
    uint32_t var2 = *(uint32_t *)0x10000134ul;
#endif
#if defined(NRF52840_XXAA) || defined(DEVELOP_IN_NRF52840)
    if (var1 == 0x08) {
        switch (var2) {
            case 0x00ul:
                return false;
            case 0x01ul:
                return true;
            case 0x02ul:
                return true;
            case 0x03ul:
                return true;
            default:
                return true;
        }
    }
#endif
#if defined(NRF52833_XXAA) || defined(DEVELOP_IN_NRF52833)
    if (var1 == 0x0D) {
        switch (var2) {
            case 0x00ul:
                return true;
            case 0x01ul:
                return true;
            default:
                return true;
        }
    }
#endif
#if defined(NRF52820_XXAA) || defined(DEVELOP_IN_NRF52820)
    if (var1 == 0x10) {
        switch (var2) {
            case 0x00ul:
                return true;
            case 0x01ul:
                return true;
            case 0x02ul:
                return true;
            default:
                return true;
        }
    }
#endif
    return false;
#endif
}

/**
 * Errata: USBD might not reach its active state.
 */
static bool chyu_nrf52_errata_171(void)
{
#ifndef NRF52_SERIES
    return false;
#else
#if defined(NRF52840_XXAA) || defined(DEVELOP_IN_NRF52840)
    uint32_t var1 = *(uint32_t *)0x10000130ul;
    uint32_t var2 = *(uint32_t *)0x10000134ul;
#endif
#if defined(NRF52840_XXAA) || defined(DEVELOP_IN_NRF52840)
    if (var1 == 0x08) {
        switch (var2) {
            case 0x00ul:
                return true;
            case 0x01ul:
                return true;
            case 0x02ul:
                return true;
            case 0x03ul:
                return true;
            default:
                return true;
        }
    }
#endif
    return false;
#endif
}

/**
 * Errata: ISO double buffering not functional.
 */
static bool chyu_nrf52_errata_166(void)
{
#ifndef NRF52_SERIES
    return false;
#else
#if defined(NRF52840_XXAA) || defined(DEVELOP_IN_NRF52840)
    uint32_t var1 = *(uint32_t *)0x10000130ul;
    uint32_t var2 = *(uint32_t *)0x10000134ul;
#endif
#if defined(NRF52840_XXAA) || defined(DEVELOP_IN_NRF52840)
    if (var1 == 0x08) {
        switch (var2) {
            case 0x00ul:
                return true;
            case 0x01ul:
                return true;
            case 0x02ul:
                return true;
            case 0x03ul:
                return true;
            default:
                return true;
        }
    }
#endif
    return false;
#endif
}

#ifdef SOFTDEVICE_PRESENT

#include "nrf_mbr.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"

#ifndef SD_MAGIC_NUMBER
#define SD_MAGIC_NUMBER 0x51B1E5DB
#endif

static inline bool is_sd_existed(void)
{
    return *((uint32_t *)(SOFTDEVICE_INFO_STRUCT_ADDRESS + 4)) == SD_MAGIC_NUMBER;
}

/**
 * check if SD is existed and enabled
 */
static inline bool is_sd_enabled(void)
{
    if (!is_sd_existed())
        return false;

    uint8_t sd_en = false;
    (void)sd_softdevice_is_enabled(&sd_en);
    return sd_en;
}
#endif

static bool hfclk_running(void)
{
#ifdef SOFTDEVICE_PRESENT
    if (is_sd_enabled()) {
        uint32_t is_running = 0;
        (void)sd_clock_hfclk_is_running(&is_running);
        return (is_running ? true : false);
    }
#endif

#if defined(CLOCK_HFCLKSTAT_SRC_Xtal) || defined(__NRFX_DOXYGEN__)
    return (NRF_CLOCK->HFCLKSTAT & (CLOCK_HFCLKSTAT_STATE_Msk | CLOCK_HFCLKSTAT_SRC_Msk)) ==
           (CLOCK_HFCLKSTAT_STATE_Msk | (CLOCK_HFCLKSTAT_SRC_Xtal << CLOCK_HFCLKSTAT_SRC_Pos));
#else
    return (NRF_CLOCK->HFCLKSTAT & (CLOCK_HFCLKSTAT_STATE_Msk | CLOCK_HFCLKSTAT_SRC_Msk)) ==
           (CLOCK_HFCLKSTAT_STATE_Msk | (CLOCK_HFCLKSTAT_SRC_HFXO << CLOCK_HFCLKSTAT_SRC_Pos));
#endif
}

enum {
    NRF_CLOCK_EVENT_HFCLKSTARTED = offsetof(NRF_CLOCK_Type, EVENTS_HFCLKSTARTED), /*!< HFCLK oscillator started. */
};

enum {
    NRF_CLOCK_TASK_HFCLKSTART = offsetof(NRF_CLOCK_Type, TASKS_HFCLKSTART), /*!< Start HFCLK clock source. */
    NRF_CLOCK_TASK_HFCLKSTOP = offsetof(NRF_CLOCK_Type, TASKS_HFCLKSTOP),   /*!< Stop HFCLK clock source. */
};

static void hfclk_enable(void)
{
    /*!< already running, nothing to do */
    if (hfclk_running())
        return;

#ifdef SOFTDEVICE_PRESENT
    if (is_sd_enabled()) {
        (void)sd_clock_hfclk_request();
        return;
    }
#endif

    *((volatile uint32_t *)((uint8_t *)NRF_CLOCK + NRF_CLOCK_EVENT_HFCLKSTARTED)) = 0x0UL;
    volatile uint32_t dummy = *((volatile uint32_t *)((uint8_t *)NRF_CLOCK + (uint32_t)NRF_CLOCK_EVENT_HFCLKSTARTED));
    (void)dummy;
    *((volatile uint32_t *)((uint8_t *)NRF_CLOCK + NRF_CLOCK_TASK_HFCLKSTART)) = 0x1UL;
}

static void hfclk_disable(void)
{
#ifdef SOFTDEVICE_PRESENT
    if (is_sd_enabled()) {
        (void)sd_clock_hfclk_release();
        return;
    }
#endif

    *((volatile uint32_t *)((uint8_t *)NRF_CLOCK + NRF_CLOCK_TASK_HFCLKSTOP)) = 0x1UL;
}

/**
 * Power & Clock Peripheral on nRF5x to manage USB
 * USB Bus power is managed by Power module, there are 3 VBUS power events:
 * Detected, Ready, Removed. Upon these power events, This function will
 * enable ( or disable ) usb & hfclk peripheral, set the usb pin pull up
 * accordingly to the controller Startup/Standby Sequence in USBD 51.4 specs.
 * Therefore this function must be called to handle USB power event by
 * - nrfx_power_usbevt_init() : if Softdevice is not used or enabled
 * - SoftDevice SOC event : if SD is used and enabled
 */
void cherry_usb_hal_nrf_power_event(uint32_t event)
{
    enum {
        USB_EVT_DETECTED = 0,
        USB_EVT_REMOVED = 1,
        USB_EVT_READY = 2
    };

    switch (event) {
        case USB_EVT_DETECTED:
            if (!NRF_USBD->ENABLE) {
                /*!< Prepare for receiving READY event: disable interrupt since we will blocking wait */
                NRF_USBD->INTENCLR = USBD_INTEN_USBEVENT_Msk;
                NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
                __ISB();
                __DSB();

#ifdef NRF52_SERIES /*!< NRF53 does not need this errata */
                /*!< ERRATA 171, 187, 166 */
                if (chyu_nrf52_errata_187()) {
                    if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000) {
                        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                        *((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
                        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                    } else {
                        *((volatile uint32_t *)(0x4006ED14)) = 0x00000003;
                    }
                }

                if (chyu_nrf52_errata_171()) {
                    if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000) {
                        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                        *((volatile uint32_t *)(0x4006EC14)) = 0x000000C0;
                        *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                    } else {
                        *((volatile uint32_t *)(0x4006EC14)) = 0x000000C0;
                    }
                }
#endif

                /*!< Enable the peripheral (will cause Ready event) */
                NRF_USBD->ENABLE = 1;
                __ISB();
                __DSB();

                /*!< Enable HFCLK */
                hfclk_enable();
            }
            break;

        case USB_EVT_READY:
            /**
     * Skip if pull-up is enabled and HCLK is already running.
     * Application probably call this more than necessary.
     */
            if (NRF_USBD->USBPULLUP && hfclk_running())
                break;

            /*!< Waiting for USBD peripheral enabled */
            while (!(USBD_EVENTCAUSE_READY_Msk & NRF_USBD->EVENTCAUSE)) {
            }

            NRF_USBD->EVENTCAUSE = USBD_EVENTCAUSE_READY_Msk;
            __ISB();
            __DSB();

#ifdef NRF52_SERIES
            if (chyu_nrf52_errata_171()) {
                if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000) {
                    *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                    *((volatile uint32_t *)(0x4006EC14)) = 0x00000000;
                    *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                } else {
                    *((volatile uint32_t *)(0x4006EC14)) = 0x00000000;
                }
            }

            if (chyu_nrf52_errata_187()) {
                if (*((volatile uint32_t *)(0x4006EC00)) == 0x00000000) {
                    *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                    *((volatile uint32_t *)(0x4006ED14)) = 0x00000000;
                    *((volatile uint32_t *)(0x4006EC00)) = 0x00009375;
                } else {
                    *((volatile uint32_t *)(0x4006ED14)) = 0x00000000;
                }
            }

            if (chyu_nrf52_errata_166()) {
                *((volatile uint32_t *)(NRF_USBD_BASE + 0x800)) = 0x7E3;
                *((volatile uint32_t *)(NRF_USBD_BASE + 0x804)) = 0x40;

                __ISB();
                __DSB();
            }
#endif

            /*!< ISO buffer Lower half for IN, upper half for OUT */
            NRF_USBD->ISOSPLIT = USBD_ISOSPLIT_SPLIT_HalfIN;

            /*!< Enable bus-reset interrupt */
            NRF_USBD->INTENSET = USBD_INTEN_USBRESET_Msk;

            /*!< Enable interrupt, priorities should be set by application */
            /*!< clear pending irq */
            NVIC->ICPR[(((uint32_t)USBD_IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)USBD_IRQn) & 0x1FUL));
            /**
     * Don't enable USBD interrupt yet, if dcd_init() did not finish yet
     * Interrupt will be enabled by tud_init(), when USB stack is ready
     * to handle interrupts.
     */
            /*!< Wait for HFCLK */
            while (!hfclk_running()) {
            }

            /*!< Enable pull up */
            NRF_USBD->USBPULLUP = 1;
            __ISB();
            __DSB();
            break;

        case USB_EVT_REMOVED:
            if (NRF_USBD->ENABLE) {
                /*!< Disable pull up */
                NRF_USBD->USBPULLUP = 0;
                __ISB();
                __DSB();

                /*!< Disable Interrupt */
                NVIC->ICER[(((uint32_t)USBD_IRQn) >> 5UL)] = (uint32_t)(1UL << (((uint32_t)USBD_IRQn) & 0x1FUL));
                __DSB();
                __ISB();
                /*!< disable all interrupt */
                NRF_USBD->INTENCLR = NRF_USBD->INTEN;

                NRF_USBD->ENABLE = 0;
                __ISB();
                __DSB();
                hfclk_disable();
            }
            break;

        default:
            break;
    }
}
