#include "usbd_core.h"
#ifdef STM32F1
#include "stm32f1xx_hal.h" //chanage this header for different soc
#elif defined(STM32F4)
#include "stm32f4xx_hal.h" //chanage this header for different soc
#elif defined(STM32H7)
#include "stm32h7xx_hal.h" //chanage this header for different soc
#endif

#ifndef USB_RAM_SIZE
#define USB_RAM_SIZE 512
#endif
#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
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
#ifdef USB
    uint16_t ep_pma_buf_len; /** Previously allocated buffer size */
    uint16_t ep_pma_addr;    /**ep pmd allocated addr*/
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#endif
};

/* Driver state */
struct usb_dc_config_priv {
    PCD_TypeDef *Instance;                                  /*!< Register base address */
    PCD_InitTypeDef Init;                                   /*!< PCD required parameters */
    __IO uint8_t USB_Address;                               /*!< USB Address */
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
#ifdef USB
    uint32_t pma_offset;
#endif
} usb_dc_cfg;

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));
#ifdef USB
    usb_dc_cfg.Instance = USB;
    usb_dc_cfg.pma_offset = 64;
    usb_dc_cfg.Init.speed = PCD_SPEED_FULL;
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
#ifdef CONFIG_USB_HS
    usb_dc_cfg.Instance = USB_OTG_HS;
    usb_dc_cfg.Init.speed = PCD_SPEED_FULL;
    usb_dc_cfg.Init.dma_enable = DISABLE;
    usb_dc_cfg.Init.phy_itface = USB_OTG_ULPI_PHY;
    usb_dc_cfg.Init.vbus_sensing_enable = DISABLE;
    usb_dc_cfg.Init.use_dedicated_ep1 = DISABLE;
    usb_dc_cfg.Init.use_external_vbus = DISABLE;
#else
    //usb_dc_cfg.Instance = USB_OTG_HS;  //run in full speed
    usb_dc_cfg.Instance = USB_OTG_FS;
    usb_dc_cfg.Init.speed = PCD_SPEED_FULL;
    usb_dc_cfg.Init.dma_enable = DISABLE;
    usb_dc_cfg.Init.phy_itface = USB_OTG_EMBEDDED_PHY;
    usb_dc_cfg.Init.vbus_sensing_enable = DISABLE;
    usb_dc_cfg.Init.use_dedicated_ep1 = DISABLE;
    usb_dc_cfg.Init.use_external_vbus = DISABLE;
#endif
#endif
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;

    usb_dc_cfg.Init.dev_endpoints = USB_NUM_BIDIR_ENDPOINTS;
    usb_dc_cfg.Init.Sof_enable = DISABLE;
    usb_dc_cfg.Init.low_power_enable = DISABLE;
    usb_dc_cfg.Init.lpm_enable = DISABLE;

    HAL_PCD_MspInit((PCD_HandleTypeDef *)&usb_dc_cfg);

#ifdef USB
    USB_DisableGlobalInt(USBx);
    USB_DevInit(USBx, usb_dc_cfg.Init);
    USB_EnableGlobalInt(USBx);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
    /* Disable DMA mode for FS instance */
#ifndef CONFIG_USB_HS
    if ((USBx->CID & (0x1U << 8)) == 0U) {
        usb_dc_cfg.Init.dma_enable = 0U;
    }
#endif
    /* Disable the Interrupts */
    USB_DisableGlobalInt(USBx);

    /*Init the Core (common init.) */
    if (USB_CoreInit(usb_dc_cfg.Instance, usb_dc_cfg.Init) != HAL_OK) {
        return -1;
    }

    /* Force Device Mode*/
    (void)USB_SetCurrentMode(USBx, USB_DEVICE_MODE);

    /* Init Device */
    if (USB_DevInit(USBx, usb_dc_cfg.Init) != HAL_OK) {
        return -1;
    }

    USB_DevDisconnect(USBx);

    if ((usb_dc_cfg.Init.battery_charging_enable == 1U) &&
        (usb_dc_cfg.Init.phy_itface != USB_OTG_ULPI_PHY)) {
        /* Enable USB Transceiver */
        USBx->GCCFG |= USB_OTG_GCCFG_PWRDWN;
    }
    (void)USB_DevConnect(USBx);
    /* Enable the Interrupts */
    USB_EnableGlobalInt(USBx);
#endif
    return 0;
}

void usb_dc_deinit(void)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    USB_StopDevice(USBx);
    HAL_PCD_MspDeInit((PCD_HandleTypeDef *)&usb_dc_cfg);
}

int usbd_set_address(const uint8_t addr)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;

    USB_SetDevAddress(USBx, addr);
    usb_dc_cfg.USB_Address = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif
    if (!ep_cfg) {
        return -1;
    }

#ifdef USB
    uint16_t wEpRegVal;

    wEpRegVal = PCD_GET_ENDPOINT(USBx, ep_idx) & USB_EP_T_MASK;
    /* initialize Endpoint */
    switch (ep_cfg->ep_type) {
        case EP_TYPE_CTRL:
            wEpRegVal |= USB_EP_CONTROL;
            break;

        case EP_TYPE_BULK:
            wEpRegVal |= USB_EP_BULK;
            break;

        case EP_TYPE_INTR:
            wEpRegVal |= USB_EP_INTERRUPT;
            break;

        case EP_TYPE_ISOC:
            wEpRegVal |= USB_EP_ISOCHRONOUS;
            break;

        default:
            break;
    }
    PCD_SET_ENDPOINT(USBx, ep_idx, (wEpRegVal | USB_EP_CTR_RX | USB_EP_CTR_TX));

    PCD_SET_EP_ADDRESS(USBx, ep_idx, ep_idx);
    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        if (usb_dc_cfg.out_ep[ep_idx].ep_mps > usb_dc_cfg.out_ep[ep_idx].ep_pma_buf_len) {
            if (usb_dc_cfg.pma_offset + usb_dc_cfg.out_ep[ep_idx].ep_mps >= 512) {
                return -1;
            }
            usb_dc_cfg.out_ep[ep_idx].ep_pma_buf_len = ep_cfg->ep_mps;
            usb_dc_cfg.out_ep[ep_idx].ep_pma_addr = usb_dc_cfg.pma_offset;
            /*Set the endpoint Receive buffer address */
            PCD_SET_EP_RX_ADDRESS(USBx, ep_idx, usb_dc_cfg.pma_offset);
            usb_dc_cfg.pma_offset += ep_cfg->ep_mps;
        }
        /*Set the endpoint Receive buffer counter*/
        PCD_SET_EP_RX_CNT(USBx, ep_idx, ep_cfg->ep_mps);
        PCD_CLEAR_RX_DTOG(USBx, ep_idx);

        /* Configure VALID status for the Endpoint*/
        PCD_SET_EP_RX_STATUS(USBx, ep_idx, USB_EP_RX_VALID);
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        if (usb_dc_cfg.in_ep[ep_idx].ep_mps > usb_dc_cfg.in_ep[ep_idx].ep_pma_buf_len) {
            if (usb_dc_cfg.pma_offset + usb_dc_cfg.in_ep[ep_idx].ep_mps >= USB_RAM_SIZE) {
                return -1;
            }
            usb_dc_cfg.in_ep[ep_idx].ep_pma_buf_len = ep_cfg->ep_mps;
            usb_dc_cfg.in_ep[ep_idx].ep_pma_addr = usb_dc_cfg.pma_offset;
            /*Set the endpoint Transmit buffer address */
            PCD_SET_EP_TX_ADDRESS(USBx, ep_idx, usb_dc_cfg.pma_offset);
            usb_dc_cfg.pma_offset += ep_cfg->ep_mps;
        }

        PCD_CLEAR_TX_DTOG(USBx, ep_idx);
        if (ep_cfg->ep_type != EP_TYPE_ISOC) {
            /* Configure NAK status for the Endpoint */
            PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_NAK);
        } else {
            /* Configure TX Endpoint to disabled state */
            PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_DIS);
        }
    }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & EP_ADDR_MSK)) << 16);

        if (((USBx_OUTEP(ep_idx)->DOEPCTL) & USB_OTG_DOEPCTL_USBAEP) == 0U) {
            USBx_OUTEP(ep_idx)->DOEPCTL |= (ep_cfg->ep_mps & USB_OTG_DOEPCTL_MPSIZ) |
                                           ((uint32_t)ep_cfg->ep_type << 18) |
                                           USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                           USB_OTG_DOEPCTL_USBAEP;
        }

    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (ep_idx & EP_ADDR_MSK));

        if ((USBx_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP) == 0U) {
            USBx_INEP(ep_idx)->DIEPCTL |= (ep_cfg->ep_mps & USB_OTG_DIEPCTL_MPSIZ) |
                                          ((uint32_t)ep_cfg->ep_type << 18) | (ep_idx << 22) |
                                          USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                          USB_OTG_DIEPCTL_USBAEP;
        }
    }

#endif

    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif
    if (USB_EP_DIR_IS_OUT(ep)) {
#ifdef USB
        PCD_CLEAR_RX_DTOG(USBx, ep_idx);

        /* Configure DISABLE status for the Endpoint*/
        PCD_SET_EP_RX_STATUS(USBx, ep_idx, USB_EP_RX_DIS);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        if (((USBx_OUTEP(ep_idx)->DOEPCTL) & USB_OTG_DOEPCTL_USBAEP) == 0U) {
            USBx_OUTEP(ep_idx)->DOEPCTL |= (usb_dc_cfg.out_ep[ep_idx].ep_mps & USB_OTG_DOEPCTL_MPSIZ) |
                                           ((uint32_t)usb_dc_cfg.out_ep[ep_idx].ep_type << 18) | (ep_idx << 22) |
                                           USB_OTG_DOEPCTL_USBAEP;
        }

        USBx_DEVICE->DEACHMSK |= USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (ep_idx & EP_ADDR_MSK)) << 16);
#endif
    } else {
#ifdef USB
        PCD_CLEAR_TX_DTOG(USBx, ep_idx);

        /* Configure DISABLE status for the Endpoint*/
        PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_DIS);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        if (((USBx_INEP(ep_idx)->DIEPCTL) & USB_OTG_DIEPCTL_USBAEP) == 0U) {
            USBx_INEP(ep_idx)->DIEPCTL |= (usb_dc_cfg.in_ep[ep_idx].ep_mps & USB_OTG_DIEPCTL_MPSIZ) |
                                          ((uint32_t)usb_dc_cfg.in_ep[ep_idx].ep_type << 18) | (ep_idx << 22) |
                                          USB_OTG_DIEPCTL_SD0PID_SEVNFRM |
                                          USB_OTG_DIEPCTL_USBAEP;
        }

        USBx_DEVICE->DEACHMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (ep_idx & EP_ADDR_MSK));
#endif
    }
    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif
    if (USB_EP_DIR_IS_OUT(ep)) {
#ifdef USB
        PCD_SET_EP_RX_STATUS(USBx, ep_idx, USB_EP_RX_STALL);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        if (((USBx_OUTEP(ep_idx)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            USBx_OUTEP(ep_idx)->DOEPCTL &= ~(USB_OTG_DOEPCTL_EPDIS);
        }
        USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
#endif
    } else {
#ifdef USB
        PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_STALL);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        if (((USBx_INEP(ep_idx)->DIEPCTL & USB_OTG_DIEPCTL_EPENA) == 0U) && (ep_idx != 0U)) {
            USBx_INEP(ep_idx)->DIEPCTL &= ~(USB_OTG_DIEPCTL_EPDIS);
        }
        USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
#endif
    }
    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif
    if (USB_EP_DIR_IS_OUT(ep)) {
#ifdef USB
        PCD_CLEAR_TX_DTOG(USBx, ep_idx);

        if (usb_dc_cfg.in_ep[ep_idx].ep_type != EP_TYPE_ISOC) {
            /* Configure NAK status for the Endpoint */
            PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_NAK);
        }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        USBx_OUTEP(ep_idx)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
        if ((usb_dc_cfg.out_ep[ep_idx].ep_type == EP_TYPE_INTR) || (usb_dc_cfg.out_ep[ep_idx].ep_type == EP_TYPE_BULK)) {
            USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
#endif
    } else {
#ifdef USB
        PCD_CLEAR_RX_DTOG(USBx, ep_idx);
        /* Configure VALID status for the Endpoint */
        PCD_SET_EP_RX_STATUS(USBx, ep_idx, USB_EP_RX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        USBx_INEP(ep_idx)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
        if ((usb_dc_cfg.in_ep[ep_idx].ep_type == EP_TYPE_INTR) || (usb_dc_cfg.in_ep[ep_idx].ep_type == EP_TYPE_BULK)) {
            USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM; /* DATA0 */
        }
#endif
    }
    return 0;
}
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif
    if (USB_EP_DIR_IS_OUT(ep)) {
    } else {
    }
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif

    if (!data && data_len) {
        return -1;
    }

    if (!data_len) {
#ifdef USB
        PCD_SET_EP_TX_CNT(USBx, ep_idx, (uint16_t)0);
        PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);

        /* EP enable, IN data in FIFO */
        USBx_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

#endif
        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;
    }
#ifdef USB
    USB_WritePMA(USBx, (uint8_t *)data, usb_dc_cfg.in_ep[ep_idx].ep_pma_addr, (uint16_t)data_len);
    PCD_SET_EP_TX_CNT(USBx, ep_idx, (uint16_t)data_len);
    PCD_SET_EP_TX_STATUS(USBx, ep_idx, USB_EP_TX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)

    if (ep_idx == 0x00) {
        /* Program the transfer size and packet count
      * as follows: xfersize = N * maxpacket +
      * short_packet pktcnt = N + (short_packet
      * exist ? 1 : 0)
      */
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (1U << 19));
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);

        /* EP enable, IN data in FIFO */
        USBx_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

        /* Enable the Tx FIFO Empty Interrupt for this EP */
        USBx_DEVICE->DIEPEMPMSK |= 1UL << (ep_idx & EP_ADDR_MSK);

    } else {
        /* Program the transfer size and packet count
      * as follows: xfersize = N * maxpacket +
      * short_packet pktcnt = N + (short_packet
      * exist ? 1 : 0)
      */
        uint32_t len32b = (data_len + 3) / 4;
        while ((USBx_INEP(ep_idx)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= len32b) {
        }

        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);
        USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (((data_len + usb_dc_cfg.in_ep[ep_idx].ep_mps - 1U) / usb_dc_cfg.in_ep[ep_idx].ep_mps) << 19));
        USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & data_len);

        if (usb_dc_cfg.in_ep[ep_idx].ep_type == EP_TYPE_ISOC) {
            USBx_INEP(ep_idx)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_MULCNT);
            USBx_INEP(ep_idx)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_MULCNT & (1U << 29));
        }

        (void)USB_WritePacket(USBx, (uint8_t *)data, ep_idx, (uint16_t)data_len, 0);

        /* EP enable, IN data in FIFO */
        USBx_INEP(ep_idx)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

        if (usb_dc_cfg.in_ep[ep_idx].ep_type != EP_TYPE_ISOC) {
            /* Enable the Tx FIFO Empty Interrupt for this EP */
            //USBx_DEVICE->DIEPEMPMSK |= 1UL << (ep_idx & EP_ADDR_MSK);
        } else {
            if ((USBx_DEVICE->DSTS & (1U << 8)) == 0U) {
                USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SODDFRM;
            } else {
                USBx_INEP(ep_idx)->DIEPCTL |= USB_OTG_DIEPCTL_SD0PID_SEVNFRM;
            }
        }
    }

#endif
    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
#if defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
#endif

    uint32_t read_count;
    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
#ifdef USB
        if (ep_idx != 0x00)
            PCD_SET_EP_RX_STATUS(USBx, ep_idx, USB_EP_RX_VALID);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
        /* Program the transfer size and packet count as follows:
    * pktcnt = N
    * xfersize = N * maxpacket
    */
        USBx_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
        USBx_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);
        USBx_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
        USBx_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & (usb_dc_cfg.out_ep[ep_idx].ep_mps));

        if (usb_dc_cfg.out_ep[ep_idx].ep_type == EP_TYPE_ISOC) {
            if ((USBx_DEVICE->DSTS & (1U << 8)) == 0U) {
                USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SODDFRM;
            } else {
                USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
            }
        }
        /* EP enable */
        USBx_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);

#endif
        return 0;
    }
#ifdef USB
    read_count = PCD_GET_EP_RX_CNT(USBx, ep_idx);
    read_count = MIN(read_count, max_data_len);
    USB_ReadPMA(USBx, (uint8_t *)data,
                usb_dc_cfg.out_ep[ep_idx].ep_pma_addr, (uint16_t)read_count);
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)

    read_count = (USBx->GRXSTSP & USB_OTG_GRXSTSP_BCNT) >> 4;
    read_count = MIN(read_count, max_data_len);
    (void)USB_ReadPacket(USBx, data, read_count);

    USBx_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
    USBx_OUTEP(ep_idx)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);
    USBx_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (1U << 19));
    USBx_OUTEP(ep_idx)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & (usb_dc_cfg.out_ep[ep_idx].ep_mps));

    if (usb_dc_cfg.out_ep[ep_idx].ep_type == EP_TYPE_ISOC) {
        if ((USBx_DEVICE->DSTS & (1U << 8)) == 0U) {
            USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SODDFRM;
        } else {
            USBx_OUTEP(ep_idx)->DOEPCTL |= USB_OTG_DOEPCTL_SD0PID_SEVNFRM;
        }
    }
    /* EP enable */
    USBx_OUTEP(ep_idx)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);

#endif
    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

/**
  * @brief  This function handles PCD interrupt request.
  * @param  hpcd PCD handle
  * @retval HAL status
  */
void USBD_IRQHandler(void)
{
    PCD_TypeDef *USBx = usb_dc_cfg.Instance;
#ifdef USB
    uint16_t wIstr, wEPVal;
    uint8_t epindex;
    wIstr = USBx->ISTR;

    uint16_t store_ep[8];
    if (wIstr & USB_ISTR_CTR) {
        while ((USBx->ISTR & USB_ISTR_CTR) != 0U) {
            wIstr = USBx->ISTR;

            /* extract highest priority endpoint number */
            epindex = (uint8_t)(wIstr & USB_ISTR_EP_ID);

            if (epindex == 0U) {
                /* Decode and service control endpoint interrupt */

                /* DIR bit = origin of the interrupt */
                if ((wIstr & USB_ISTR_DIR) == 0U) {
                    /* DIR = 0 */

                    /* DIR = 0 => IN  int */
                    /* DIR = 0 implies that (EP_CTR_TX = 1) always */
                    PCD_CLEAR_TX_EP_CTR(USBx, PCD_ENDP0);
                    usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                    if ((usb_dc_cfg.USB_Address > 0U) && (PCD_GET_EP_TX_CNT(USBx, PCD_ENDP0) == 0U)) {
                        USBx->DADDR = ((uint16_t)usb_dc_cfg.USB_Address | USB_DADDR_EF);
                        usb_dc_cfg.USB_Address = 0U;
                    }
                }
                /* DIR = 1 */

                /* DIR = 1 & CTR_RX => SETUP or OUT int */
                /* DIR = 1 & (CTR_TX | CTR_RX) => 2 int pending */

                wEPVal = PCD_GET_ENDPOINT(USBx, PCD_ENDP0);

                if ((wEPVal & USB_EP_SETUP) != 0U) {
                    /* SETUP bit kept frozen while CTR_RX = 1 */
                    PCD_CLEAR_RX_EP_CTR(USBx, PCD_ENDP0);

                    /* Process SETUP Packet*/
                    usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                    PCD_SET_EP_RX_STATUS(USBx, 0, USB_EP_RX_VALID);
                } else if ((wEPVal & USB_EP_CTR_RX) != 0U) {
                    PCD_CLEAR_RX_EP_CTR(USBx, PCD_ENDP0);
                    /* Process Control Data OUT Packet */
                    usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                    PCD_SET_EP_RX_STATUS(USBx, 0, USB_EP_RX_VALID);
                }
            } else {
                /* Decode and service non control endpoints interrupt */
                /* process related endpoint register */
                wEPVal = PCD_GET_ENDPOINT(USBx, epindex);

                if ((wEPVal & USB_EP_CTR_RX) != 0U) {
                    /* clear int flag */
                    PCD_CLEAR_RX_EP_CTR(USBx, epindex);
                    usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(epindex & 0x7f));
                }

                if ((wEPVal & USB_EP_CTR_TX) != 0U) {
                    /* clear int flag */
                    PCD_CLEAR_TX_EP_CTR(USBx, epindex);
                    usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(epindex | 0x80));
                }
            }
        }
    }
    if (wIstr & USB_ISTR_RESET) {
        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);

        USBx->ISTR &= (uint16_t)(~USB_ISTR_RESET);
    }
    if (wIstr & USB_ISTR_PMAOVR) {
        USBx->ISTR &= (uint16_t)(~USB_ISTR_PMAOVR);
    }
    if (wIstr & USB_ISTR_ERR) {
        USBx->ISTR &= (uint16_t)(~USB_ISTR_ERR);
    }
    if (wIstr & USB_ISTR_WKUP) {
        USBx->CNTR &= (uint16_t) ~(USB_CNTR_LP_MODE);
        USBx->CNTR &= (uint16_t) ~(USB_CNTR_FSUSP);

        USBx->ISTR &= (uint16_t)(~USB_ISTR_WKUP);
    }
    if (wIstr & USB_ISTR_SUSP) {
        /* WA: To Clear Wakeup flag if raised with suspend signal */

        /* Store Endpoint register */
        for (uint8_t i = 0U; i < 8U; i++) {
            store_ep[i] = PCD_GET_ENDPOINT(USBx, i);
        }

        /* FORCE RESET */
        USBx->CNTR |= (uint16_t)(USB_CNTR_FRES);

        /* CLEAR RESET */
        USBx->CNTR &= (uint16_t)(~USB_CNTR_FRES);

        /* wait for reset flag in ISTR */
        while ((USBx->ISTR & USB_ISTR_RESET) == 0U) {
        }

        /* Clear Reset Flag */
        USBx->ISTR &= (uint16_t)(~USB_ISTR_RESET);
        /* Restore Registre */
        for (uint8_t i = 0U; i < 8U; i++) {
            PCD_SET_ENDPOINT(USBx, i, store_ep[i]);
        }

        /* Force low-power mode in the macrocell */
        USBx->CNTR |= (uint16_t)USB_CNTR_FSUSP;

        /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
        USBx->ISTR &= (uint16_t)(~USB_ISTR_SUSP);

        USBx->CNTR |= (uint16_t)USB_CNTR_LP_MODE;
    }
    if (wIstr & USB_ISTR_SOF) {
        USBx->ISTR &= (uint16_t)(~USB_ISTR_SOF);
    }
    if (wIstr & USB_ISTR_ESOF) {
        USBx->ISTR &= (uint16_t)(~USB_ISTR_ESOF);
    }
#elif defined(USB_OTG_FS) || defined(USB_OTG_HS)
    uint32_t USBx_BASE = (uint32_t)USBx;
    uint32_t int_status;
    uint32_t temp, epindex, ep_intr, ep_int_status;
    /* ensure that we are in device mode */
    if (USB_GetMode(USBx) == USB_OTG_MODE_DEVICE) {
        while (int_status == USB_ReadInterrupts(USBx)) {
            if (int_status & USB_OTG_GINTSTS_USBRST) {
                usbd_event_notify_handler(USBD_EVENT_RESET, NULL);
                USBx->GINTSTS &= USB_OTG_GINTSTS_USBRST;
            }
            if (int_status & USB_OTG_GINTSTS_ENUMDNE) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_ENUMDNE;
            }
            if (int_status & USB_OTG_GINTSTS_RXFLVL) {
                USB_MASK_INTERRUPT(USBx, USB_OTG_GINTSTS_RXFLVL);
                temp = USBx->GRXSTSP;
                epindex = temp & USB_OTG_GRXSTSP_EPNUM;
                if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> 17) == STS_DATA_UPDT) {
                    if (epindex == 0)
                        usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                    else {
                        usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(epindex & 0x7f));
                    }
                } else if (((temp & USB_OTG_GRXSTSP_PKTSTS) >> 17) == STS_SETUP_UPDT) {
                    usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);
                } else {
                    /* ... */
                }
                USB_UNMASK_INTERRUPT(USBx, USB_OTG_GINTSTS_RXFLVL);
            }
            if (int_status & USB_OTG_GINTSTS_IEPINT) {
                epindex = 0U;
                /* Read in the device interrupt bits */
                ep_intr = USB_ReadDevAllInEpInterrupt(USBx);

                while (ep_intr != 0U) {
                    if ((ep_intr & 0x1U) != 0U) {
                        /* Read IN EP interrupt status */
                        ep_int_status = USB_ReadDevInEPInterrupt(USBx, (uint8_t)epindex);
                        /* Clear IN EP interrupts */
                        CLEAR_IN_EP_INTR(epindex, ep_int_status);
                        if (ep_int_status & USB_OTG_DIEPINT_XFRC) {
                            usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(epindex | 0x80));
                        }
                    }
                    epindex++;
                    ep_intr >>= 1U;
                }
            }
            if (int_status & USB_OTG_GINTSTS_OEPINT) {
                epindex = 0;
                /* Read in the device interrupt bits */
                ep_intr = USB_ReadDevAllOutEpInterrupt(USBx);

                while (ep_intr != 0U) {
                    if ((ep_intr & 0x1U) != 0U) {
                        /* Read OUT EP interrupt status */
                        ep_int_status = USB_ReadDevOutEPInterrupt(USBx, (uint8_t)epindex);
                        /* Clear OUT EP interrupts */
                        CLEAR_OUT_EP_INTR(epindex, ep_int_status);
                    }
                    epindex++;
                    ep_intr >>= 1U;
                }
            }
            if (int_status & USB_OTG_GINTSTS_MMIS) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_MMIS;
            }
            if (int_status & USB_OTG_GINTSTS_WKUINT) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_WKUINT;
            }
            if (int_status & USB_OTG_GINTSTS_USBSUSP) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_USBSUSP;
            }
            //            if (int_status & USB_OTG_GINTSTS_LPMINT) {
            //                USBx->GINTSTS &= USB_OTG_GINTSTS_LPMINT;
            //            }
            if (int_status & USB_OTG_GINTSTS_SOF) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_SOF;
            }
            if (int_status & USB_OTG_GINTSTS_SRQINT) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_SRQINT;
            }
            if (int_status & USB_OTG_GINTSTS_OTGINT) {
                USBx->GINTSTS &= USB_OTG_GINTSTS_OTGINT;
            }
        }
    }
#endif
}
