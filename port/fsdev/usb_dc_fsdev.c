#include "usbd_core.h"

#ifndef CONFIG_FSDEV_PORTS
#define CONFIG_FSDEV_PORTS 1
#endif

#ifndef CONFIG_FSDEV_BIDIR_ENDPOINTS
#define CONFIG_FSDEV_BIDIR_ENDPOINTS 8
#endif

#ifndef CONFIG_FSDEV_PMA_RAM_SIZE
#define CONFIG_FSDEV_PMA_RAM_SIZE 512
#endif

#ifndef CONFIG_FSDEV_PMA_ACCESS
#define CONFIG_FSDEV_PMA_ACCESS 2
#endif

#define PMA_ACCESS CONFIG_FSDEV_PMA_ACCESS
#include "usb_fsdev_reg.h"

#warning please check your CONFIG_FSDEV_PMA_ACCESS is 1 or 2

#define USB ((USB_TypeDef *)bus->reg_base)

#define USB_BTABLE_SIZE (8 * CONFIG_FSDEV_BIDIR_ENDPOINTS)

/* Endpoint state */
struct fsdev_ep {
    uint16_t ep_mps;         /* Endpoint max packet size */
    uint8_t ep_type;         /* Endpoint type */
    uint8_t ep_stalled;      /* Endpoint stall flag */
    uint8_t ep_enable;       /* Endpoint enable */
    uint16_t ep_pma_buf_len; /* Previously allocated buffer size */
    uint16_t ep_pma_addr;    /* Endpoint pma allocated addr */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
};

/* Driver state */
struct fsdev_udc {
    struct usb_setup_packet setup;
    volatile uint8_t dev_addr;
    volatile uint32_t pma_offset;
    struct fsdev_ep in_ep[CONFIG_FSDEV_BIDIR_ENDPOINTS];
    struct fsdev_ep out_ep[CONFIG_FSDEV_BIDIR_ENDPOINTS];
    bool inuse;
} g_fsdev_udc[CONFIG_FSDEV_PORTS];

struct fsdev_udc *fsdev_alloc_udc(void)
{
    for (uint8_t i = 0; i < CONFIG_FSDEV_PORTS; i++) {
        if (g_fsdev_udc[i].inuse == false) {
            g_fsdev_udc[i].inuse = true;
            return &g_fsdev_udc[i];
        }
    }
    return NULL;
}

void fsdev_free_udc(struct fsdev_udc *udc)
{
    udc->inuse = false;
}

static void fsdev_write_pma(USB_TypeDef *USBx, uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
    uint32_t n = ((uint32_t)wNBytes + 1U) >> 1;
    uint32_t BaseAddr = (uint32_t)USBx;
    uint32_t i, temp1, temp2;
    __IO uint16_t *pdwVal;
    uint8_t *pBuf = pbUsrBuf;

    pdwVal = (__IO uint16_t *)(BaseAddr + 0x400U + ((uint32_t)wPMABufAddr * PMA_ACCESS));

    for (i = n; i != 0U; i--) {
        temp1 = *pBuf;
        pBuf++;
        temp2 = temp1 | ((uint16_t)((uint16_t)*pBuf << 8));
        *pdwVal = (uint16_t)temp2;
        pdwVal++;

#if PMA_ACCESS > 1U
        pdwVal++;
#endif

        pBuf++;
    }
}

/**
  * @brief Copy data from packet memory area (PMA) to user memory buffer
  * @param   USBx USB peripheral instance register address.
  * @param   pbUsrBuf pointer to user memory area.
  * @param   wPMABufAddr address into PMA.
  * @param   wNBytes no. of bytes to be copied.
  * @retval None
  */
static void fsdev_read_pma(USB_TypeDef *USBx, uint8_t *pbUsrBuf, uint16_t wPMABufAddr, uint16_t wNBytes)
{
    uint32_t n = (uint32_t)wNBytes >> 1;
    uint32_t BaseAddr = (uint32_t)USBx;
    uint32_t i, temp;
    __IO uint16_t *pdwVal;
    uint8_t *pBuf = pbUsrBuf;

    pdwVal = (__IO uint16_t *)(BaseAddr + 0x400U + ((uint32_t)wPMABufAddr * PMA_ACCESS));

    for (i = n; i != 0U; i--) {
        temp = *(__IO uint16_t *)pdwVal;
        pdwVal++;
        *pBuf = (uint8_t)((temp >> 0) & 0xFFU);
        pBuf++;
        *pBuf = (uint8_t)((temp >> 8) & 0xFFU);
        pBuf++;

#if PMA_ACCESS > 1U
        pdwVal++;
#endif
    }

    if ((wNBytes % 2U) != 0U) {
        temp = *pdwVal;
        *pBuf = (uint8_t)((temp >> 0) & 0xFFU);
    }
}

int fsdev_udc_init(struct usbd_bus *bus)
{
    struct fsdev_udc *udc;

    udc = fsdev_alloc_udc();
    if (udc == NULL) {
        return -1;
    }

    USB_LOG_INFO("========== fsdev udc params =========\r\n");
    USB_LOG_INFO("fsdev pma ram size:%d, pma access:%d\r\n", CONFIG_FSDEV_PMA_RAM_SIZE, CONFIG_FSDEV_PMA_ACCESS);
    USB_LOG_INFO("fsdev has %d endpoints\r\n", CONFIG_FSDEV_BIDIR_ENDPOINTS);
    USB_LOG_INFO("=================================\r\n");

    bus->udc = udc;
    bus->endpoints = CONFIG_FSDEV_BIDIR_ENDPOINTS;

    usbd_udc_low_level_init(bus->busid);

    /* Init Device */
    /* CNTR_FRES = 1 */
    USB->CNTR = (uint16_t)USB_CNTR_FRES;

    /* CNTR_FRES = 0 */
    USB->CNTR = 0U;

    /* Clear pending interrupts */
    USB->ISTR = 0U;

    /*Set Btable Address*/
    USB->BTABLE = BTABLE_ADDRESS;

    uint32_t winterruptmask;

    /* Set winterruptmask variable */
    winterruptmask = USB_CNTR_CTRM | USB_CNTR_WKUPM |
                     USB_CNTR_SUSPM | USB_CNTR_ERRM |
                     USB_CNTR_SOFM | USB_CNTR_ESOFM |
                     USB_CNTR_RESETM;

    /* Set interrupt mask */
    USB->CNTR = (uint16_t)winterruptmask;

    /* Enabling DP Pull-UP bit to Connect internal PU resistor on USB DP line */
    USB->BCDR |= (uint16_t)USB_BCDR_DPPU;

    return 0;
}

int fsdev_udc_deinit(struct usbd_bus *bus)
{
    struct fsdev_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    /* disable all interrupts and force USB reset */
    USB->CNTR = (uint16_t)USB_CNTR_FRES;

    /* clear interrupt status register */
    USB->ISTR = 0U;

    /* switch-off device */
    USB->CNTR = (uint16_t)(USB_CNTR_FRES | USB_CNTR_PDWN);

    fsdev_free_udc(udc);
    bus->udc = NULL;
    usbd_udc_low_level_deinit(bus->busid);
    return 0;
}

int fsdev_set_address(struct usbd_bus *bus, const uint8_t addr)
{
    struct fsdev_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (addr == 0U) {
        /* set device address and enable function */
        USB->DADDR = (uint16_t)USB_DADDR_EF;
    }

    udc->dev_addr = addr;
    return 0;
}

uint8_t fsdev_get_port_speed(struct usbd_bus *bus)
{
    return USB_SPEED_FULL;
}

int fsdev_ep_open(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_desc->bEndpointAddress);
    uint16_t ep_mps;
    uint8_t ep_type;
    struct fsdev_udc *udc = bus->udc;
    uint16_t wEpRegVal;

    if (udc == NULL) {
        return -1;
    }

    if (ep_idx > (bus->endpoints - 1)) {
        USB_LOG_ERR("Ep addr 0x%02x overflow\r\n", ep_desc->bEndpointAddress);
        return -2;
    }

    ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    /* initialize Endpoint */
    switch (ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
            wEpRegVal = USB_EP_CONTROL;
            break;

        case USB_ENDPOINT_TYPE_BULK:
            wEpRegVal = USB_EP_BULK;
            break;

        case USB_ENDPOINT_TYPE_INTERRUPT:
            wEpRegVal = USB_EP_INTERRUPT;
            break;

        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            wEpRegVal = USB_EP_ISOCHRONOUS;
            break;

        default:
            break;
    }

    PCD_SET_EPTYPE(USB, ep_idx, wEpRegVal);

    PCD_SET_EP_ADDRESS(USB, ep_idx, ep_idx);
    if (USB_EP_DIR_IS_OUT(ep_desc->bEndpointAddress)) {
        udc->out_ep[ep_idx].ep_mps = ep_mps;
        udc->out_ep[ep_idx].ep_type = ep_type;
        udc->out_ep[ep_idx].ep_enable = true;
        if (udc->out_ep[ep_idx].ep_mps > udc->out_ep[ep_idx].ep_pma_buf_len) {
            if (udc->pma_offset + udc->out_ep[ep_idx].ep_mps > CONFIG_FSDEV_PMA_RAM_SIZE) {
                USB_LOG_ERR("Ep pma %02x overflow\r\n", ep_desc->bEndpointAddress);
                return -1;
            }
            udc->out_ep[ep_idx].ep_pma_buf_len = ep_mps;
            udc->out_ep[ep_idx].ep_pma_addr = udc->pma_offset;
            /*Set the endpoint Receive buffer address */
            PCD_SET_EP_RX_ADDRESS(USB, ep_idx, udc->pma_offset);
            udc->pma_offset += ep_mps;
        }
        /*Set the endpoint Receive buffer counter*/
        PCD_SET_EP_RX_CNT(USB, ep_idx, ep_mps);
        PCD_CLEAR_RX_DTOG(USB, ep_idx);
    } else {
        udc->in_ep[ep_idx].ep_mps = ep_mps;
        udc->in_ep[ep_idx].ep_type = ep_type;
        udc->in_ep[ep_idx].ep_enable = true;
        if (udc->in_ep[ep_idx].ep_mps > udc->in_ep[ep_idx].ep_pma_buf_len) {
            if (udc->pma_offset + udc->in_ep[ep_idx].ep_mps > CONFIG_FSDEV_PMA_RAM_SIZE) {
                USB_LOG_ERR("Ep pma %02x overflow\r\n", ep_desc->bEndpointAddress);
                return -1;
            }
            udc->in_ep[ep_idx].ep_pma_buf_len = ep_mps;
            udc->in_ep[ep_idx].ep_pma_addr = udc->pma_offset;
            /*Set the endpoint Transmit buffer address */
            PCD_SET_EP_TX_ADDRESS(USB, ep_idx, udc->pma_offset);
            udc->pma_offset += ep_mps;
        }

        PCD_CLEAR_TX_DTOG(USB, ep_idx);
        if (ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            /* Configure NAK status for the Endpoint */
            PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_NAK);
        } else {
            /* Configure TX Endpoint to disabled state */
            PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_DIS);
        }
    }
    return 0;
}

int fsdev_ep_close(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        PCD_CLEAR_RX_DTOG(USB, ep_idx);

        /* Configure DISABLE status for the Endpoint*/
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_DIS);
    } else {
        PCD_CLEAR_TX_DTOG(USB, ep_idx);

        /* Configure DISABLE status for the Endpoint*/
        PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_DIS);
    }
    return 0;
}

int fsdev_ep_set_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_STALL);
    } else {
        PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_STALL);
    }
    return 0;
}

int fsdev_ep_clear_stall(struct usbd_bus *bus, const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct fsdev_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep)) {
        PCD_CLEAR_RX_DTOG(USB, ep_idx);
        /* Configure VALID status for the Endpoint */
        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);
    } else {
        PCD_CLEAR_TX_DTOG(USB, ep_idx);

        if (udc->in_ep[ep_idx].ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
            /* Configure NAK status for the Endpoint */
            PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_NAK);
        }
    }
    return 0;
}

int fsdev_ep_is_stalled(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled)
{
    if (USB_EP_DIR_IS_OUT(ep)) {
    } else {
    }
    return 0;
}

int fsdev_ep_start_write(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct fsdev_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }

    if (!udc->in_ep[ep_idx].ep_enable) {
        return -3;
    }

    udc->in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    udc->in_ep[ep_idx].xfer_len = data_len;
    udc->in_ep[ep_idx].actual_xfer_len = 0;

    data_len = MIN(data_len, udc->in_ep[ep_idx].ep_mps);

    fsdev_write_pma(USB, (uint8_t *)data, udc->in_ep[ep_idx].ep_pma_addr, (uint16_t)data_len);
    PCD_SET_EP_TX_CNT(USB, ep_idx, (uint16_t)data_len);
    PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_VALID);

    return 0;
}

int fsdev_ep_start_read(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    struct fsdev_udc *udc = bus->udc;

    if (udc == NULL) {
        return -1;
    }

    if (!data && data_len) {
        return -2;
    }
    if (!udc->out_ep[ep_idx].ep_enable) {
        return -3;
    }

    udc->out_ep[ep_idx].xfer_buf = data;
    udc->out_ep[ep_idx].xfer_len = data_len;
    udc->out_ep[ep_idx].actual_xfer_len = 0;

    PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);

    return 0;
}

void fsdev_udc_irq(struct usbd_bus *bus)
{
    uint16_t wIstr, wEPVal;
    uint8_t ep_idx;
    uint8_t read_count;
    uint16_t write_count;
    uint16_t store_ep[8];
    struct fsdev_udc *udc = bus->udc;

    if (udc == NULL) {
        return;
    }

    wIstr = USB->ISTR;
    if (wIstr & USB_ISTR_CTR) {
        while ((USB->ISTR & USB_ISTR_CTR) != 0U) {
            wIstr = USB->ISTR;

            /* extract highest priority endpoint number */
            ep_idx = (uint8_t)(wIstr & USB_ISTR_EP_ID);

            if (ep_idx == 0U) {
                if ((wIstr & USB_ISTR_DIR) == 0U) {
                    PCD_CLEAR_TX_EP_CTR(USB, ep_idx);

                    write_count = PCD_GET_EP_TX_CNT(USB, ep_idx);

                    udc->in_ep[ep_idx].xfer_buf += write_count;
                    udc->in_ep[ep_idx].xfer_len -= write_count;
                    udc->in_ep[ep_idx].actual_xfer_len += write_count;

                    usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, udc->in_ep[ep_idx].actual_xfer_len);

                    if (udc->setup.wLength == 0) {
                        /* In status, start reading setup */
                        usbd_ep_start_read(bus->busid, 0x00, NULL, 0);
                    } else if (udc->setup.wLength && ((udc->setup.bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
                        /* In status, start reading setup */
                        usbd_ep_start_read(bus->busid, 0x00, NULL, 0);
                    }

                    if ((udc->dev_addr > 0U) && (write_count == 0U)) {
                        USB->DADDR = ((uint16_t)udc->dev_addr | USB_DADDR_EF);
                        udc->dev_addr = 0U;
                    }

                } else {
                    wEPVal = PCD_GET_ENDPOINT(USB, ep_idx);

                    if ((wEPVal & USB_EP_SETUP) != 0U) {
                        PCD_CLEAR_RX_EP_CTR(USB, ep_idx);

                        read_count = PCD_GET_EP_RX_CNT(USB, ep_idx);
                        fsdev_read_pma(USB, (uint8_t *)&udc->setup, udc->out_ep[ep_idx].ep_pma_addr, (uint16_t)read_count);

                        usbd_event_ep0_setup_complete_handler(bus->busid, (uint8_t *)&udc->setup);

                    } else if ((wEPVal & USB_EP_CTR_RX) != 0U) {
                        PCD_CLEAR_RX_EP_CTR(USB, ep_idx);

                        read_count = PCD_GET_EP_RX_CNT(USB, ep_idx);

                        fsdev_read_pma(USB, udc->out_ep[ep_idx].xfer_buf, udc->out_ep[ep_idx].ep_pma_addr, (uint16_t)read_count);

                        udc->out_ep[ep_idx].xfer_buf += read_count;
                        udc->out_ep[ep_idx].xfer_len -= read_count;
                        udc->out_ep[ep_idx].actual_xfer_len += read_count;

                        usbd_event_ep_out_complete_handler(bus->busid, ep_idx, udc->out_ep[ep_idx].actual_xfer_len);

                        if (read_count == 0) {
                            /* Out status, start reading setup */
                            usbd_ep_start_read(bus->busid, 0x00, NULL, 0);
                        }
                    }
                }
            } else {
                wEPVal = PCD_GET_ENDPOINT(USB, ep_idx);

                if ((wEPVal & USB_EP_CTR_RX) != 0U) {
                    PCD_CLEAR_RX_EP_CTR(USB, ep_idx);
                    read_count = PCD_GET_EP_RX_CNT(USB, ep_idx);
                    fsdev_read_pma(USB, udc->out_ep[ep_idx].xfer_buf, udc->out_ep[ep_idx].ep_pma_addr, (uint16_t)read_count);
                    udc->out_ep[ep_idx].xfer_buf += read_count;
                    udc->out_ep[ep_idx].xfer_len -= read_count;
                    udc->out_ep[ep_idx].actual_xfer_len += read_count;

                    if ((read_count < udc->out_ep[ep_idx].ep_mps) ||
                        (udc->out_ep[ep_idx].xfer_len == 0)) {
                        usbd_event_ep_out_complete_handler(bus->busid, ep_idx, udc->out_ep[ep_idx].actual_xfer_len);
                    } else {
                        PCD_SET_EP_RX_STATUS(USB, ep_idx, USB_EP_RX_VALID);
                    }
                }

                if ((wEPVal & USB_EP_CTR_TX) != 0U) {
                    PCD_CLEAR_TX_EP_CTR(USB, ep_idx);
                    write_count = PCD_GET_EP_TX_CNT(USB, ep_idx);

                    udc->in_ep[ep_idx].xfer_buf += write_count;
                    udc->in_ep[ep_idx].xfer_len -= write_count;
                    udc->in_ep[ep_idx].actual_xfer_len += write_count;

                    if (udc->in_ep[ep_idx].xfer_len == 0) {
                        usbd_event_ep_in_complete_handler(bus->busid, ep_idx | 0x80, udc->in_ep[ep_idx].actual_xfer_len);
                    } else {
                        write_count = MIN(udc->in_ep[ep_idx].xfer_len, udc->in_ep[ep_idx].ep_mps);
                        fsdev_write_pma(USB, udc->in_ep[ep_idx].xfer_buf, udc->in_ep[ep_idx].ep_pma_addr, (uint16_t)write_count);
                        PCD_SET_EP_TX_CNT(USB, ep_idx, write_count);
                        PCD_SET_EP_TX_STATUS(USB, ep_idx, USB_EP_TX_VALID);
                    }
                }
            }
        }
    }
    if (wIstr & USB_ISTR_RESET) {
        memset(udc->in_ep, 0, sizeof(struct fsdev_ep) * bus->endpoints);
        memset(udc->out_ep, 0, sizeof(struct fsdev_ep) * bus->endpoints);
        udc->dev_addr = 0;
        udc->pma_offset = USB_BTABLE_SIZE;
        usbd_event_reset_handler(bus->busid);
        /* start reading setup packet */
        PCD_SET_EP_RX_STATUS(USB, 0, USB_EP_RX_VALID);
        USB->ISTR &= (uint16_t)(~USB_ISTR_RESET);
    }
    if (wIstr & USB_ISTR_PMAOVR) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_PMAOVR);
    }
    if (wIstr & USB_ISTR_ERR) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_ERR);
    }
    if (wIstr & USB_ISTR_WKUP) {
        USB->CNTR &= (uint16_t) ~(USB_CNTR_LP_MODE);
        USB->CNTR &= (uint16_t) ~(USB_CNTR_FSUSP);

        USB->ISTR &= (uint16_t)(~USB_ISTR_WKUP);
    }
    if (wIstr & USB_ISTR_SUSP) {
        /* WA: To Clear Wakeup flag if raised with suspend signal */

        /* Store Endpoint register */
        for (uint8_t i = 0U; i < 8U; i++) {
            store_ep[i] = PCD_GET_ENDPOINT(USB, i);
        }

        /* FORCE RESET */
        USB->CNTR |= (uint16_t)(USB_CNTR_FRES);

        /* CLEAR RESET */
        USB->CNTR &= (uint16_t)(~USB_CNTR_FRES);

        /* wait for reset flag in ISTR */
        while ((USB->ISTR & USB_ISTR_RESET) == 0U) {
        }

        /* Clear Reset Flag */
        USB->ISTR &= (uint16_t)(~USB_ISTR_RESET);
        /* Restore Registre */
        for (uint8_t i = 0U; i < 8U; i++) {
            PCD_SET_ENDPOINT(USB, i, store_ep[i]);
        }

        /* Force low-power mode in the macrocell */
        USB->CNTR |= (uint16_t)USB_CNTR_FSUSP;

        /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
        USB->ISTR &= (uint16_t)(~USB_ISTR_SUSP);

        USB->CNTR |= (uint16_t)USB_CNTR_LP_MODE;
    }
    if (wIstr & USB_ISTR_SOF) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_SOF);
    }
    if (wIstr & USB_ISTR_ESOF) {
        USB->ISTR &= (uint16_t)(~USB_ISTR_ESOF);
    }
}

struct usbd_udc_driver fsdev_udc_driver = {
    .driver_name = "fsdev udc",
    .udc_init = fsdev_udc_init,
    .udc_deinit = fsdev_udc_deinit,
    .udc_set_address = fsdev_set_address,
    .udc_get_port_speed = fsdev_get_port_speed,
    .udc_ep_open = fsdev_ep_open,
    .udc_ep_close = fsdev_ep_close,
    .udc_ep_set_stall = fsdev_ep_set_stall,
    .udc_ep_clear_stall = fsdev_ep_clear_stall,
    .udc_ep_is_stalled = fsdev_ep_is_stalled,
    .udc_ep_start_write = fsdev_ep_start_write,
    .udc_ep_start_read = fsdev_ep_start_read,
    .udc_irq = fsdev_udc_irq
};