#include "usbd_core.h"
#include "usb_ch32_usbfs_reg.h"

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
};

/* Driver state */
struct usb_dc_config_priv {
    uint8_t dev_addr;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} usb_dc_cfg;

// clang-format off
/* Endpoint Buffer */
__attribute__ ((aligned(4))) uint8_t EP0_DatabufHD[64]; //ep0(64)
__attribute__ ((aligned(4))) uint8_t EP1_DatabufHD[64+64];  //ep1_out(64)+ep1_in(64)
__attribute__ ((aligned(4))) uint8_t EP2_DatabufHD[64+64];  //ep2_out(64)+ep2_in(64)
__attribute__ ((aligned(4))) uint8_t EP3_DatabufHD[64+64];  //ep3_out(64)+ep3_in(64)
__attribute__ ((aligned(4))) uint8_t EP4_DatabufHD[64+64];  //ep4_out(64)+ep4_in(64)
__attribute__ ((aligned(4))) uint8_t EP5_DatabufHD[64+64];  //ep5_out(64)+ep5_in(64)
__attribute__ ((aligned(4))) uint8_t EP6_DatabufHD[64+64];  //ep6_out(64)+ep6_in(64)
__attribute__ ((aligned(4))) uint8_t EP7_DatabufHD[64+64];  //ep7_out(64)+ep7_in(64)
// clang-format on

void OTG_FS_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

volatile uint8_t mps_over_flag = 0;

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(void)
{
    memset(&usb_dc_cfg, 0, sizeof(struct usb_dc_config_priv));

    usb_dc_low_level_init();

    USBFS_DEVICE->BASE_CTRL = 0x00;

    USBFS_DEVICE->UEP4_1_MOD = USBFS_UEP4_RX_EN | USBFS_UEP4_TX_EN | USBFS_UEP1_RX_EN | USBFS_UEP1_TX_EN;
    USBFS_DEVICE->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP2_TX_EN | USBFS_UEP3_RX_EN | USBFS_UEP3_TX_EN;
    USBFS_DEVICE->UEP5_6_MOD = USBFS_UEP5_RX_EN | USBFS_UEP5_TX_EN | USBFS_UEP6_RX_EN | USBFS_UEP6_TX_EN;
    USBFS_DEVICE->UEP7_MOD = USBFS_UEP7_RX_EN | USBFS_UEP7_TX_EN;

    USBFS_DEVICE->UEP0_DMA = (uint32_t)EP0_DatabufHD;
    USBFS_DEVICE->UEP1_DMA = (uint32_t)EP1_DatabufHD;
    USBFS_DEVICE->UEP2_DMA = (uint32_t)EP2_DatabufHD;
    USBFS_DEVICE->UEP3_DMA = (uint32_t)EP3_DatabufHD;
    USBFS_DEVICE->UEP4_DMA = (uint32_t)EP4_DatabufHD;
    USBFS_DEVICE->UEP5_DMA = (uint32_t)EP5_DatabufHD;
    USBFS_DEVICE->UEP6_DMA = (uint32_t)EP6_DatabufHD;
    USBFS_DEVICE->UEP7_DMA = (uint32_t)EP7_DatabufHD;

    USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP1_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP2_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP3_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP4_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP5_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP6_RX_CTRL = USBFS_UEP_R_RES_ACK;
    USBFS_DEVICE->UEP7_RX_CTRL = USBFS_UEP_R_RES_ACK;

    USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    USBFS_DEVICE->UEP1_TX_LEN = 0;
    USBFS_DEVICE->UEP2_TX_LEN = 0;
    USBFS_DEVICE->UEP3_TX_LEN = 0;
    USBFS_DEVICE->UEP4_TX_LEN = 0;
    USBFS_DEVICE->UEP5_TX_LEN = 0;
    USBFS_DEVICE->UEP6_TX_LEN = 0;
    USBFS_DEVICE->UEP7_TX_LEN = 0;

    USBFS_DEVICE->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;
    USBFS_DEVICE->UEP2_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;
    USBFS_DEVICE->UEP3_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;
    USBFS_DEVICE->UEP4_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;
    USBFS_DEVICE->UEP5_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;
    USBFS_DEVICE->UEP6_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;
    USBFS_DEVICE->UEP7_TX_CTRL = USBFS_UEP_T_RES_NAK | USBFS_UEP_AUTO_TOG;

    USBFS_DEVICE->INT_FG = 0xFF;
    USBFS_DEVICE->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
    USBFS_DEVICE->DEV_ADDR = 0x00;

    USBFS_DEVICE->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    USBFS_DEVICE->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
    return 0;
}

void usb_dc_deinit(void)
{
}

int usbd_set_address(const uint8_t addr)
{
    if (addr == 0) {
        USBFS_DEVICE->DEV_ADDR = (USBFS_DEVICE->DEV_ADDR & USBFS_UDA_GP_BIT) | 0;
    }
    usb_dc_cfg.dev_addr = addr;
    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        usb_dc_cfg.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
    } else {
        usb_dc_cfg.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        usb_dc_cfg.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
    }
    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        switch (ep_idx) {
            case 0:
                USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_STALL;
                break;
            case 1:
                USBFS_DEVICE->UEP1_RX_CTRL = (USBFS_DEVICE->UEP1_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            case 2:
                USBFS_DEVICE->UEP2_RX_CTRL = (USBFS_DEVICE->UEP2_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            case 3:
                USBFS_DEVICE->UEP3_RX_CTRL = (USBFS_DEVICE->UEP3_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            case 4:
                USBFS_DEVICE->UEP4_RX_CTRL = (USBFS_DEVICE->UEP4_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            case 5:
                USBFS_DEVICE->UEP5_RX_CTRL = (USBFS_DEVICE->UEP5_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            case 6:
                USBFS_DEVICE->UEP6_RX_CTRL = (USBFS_DEVICE->UEP6_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            case 7:
                USBFS_DEVICE->UEP7_RX_CTRL = (USBFS_DEVICE->UEP7_RX_CTRL &= ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_STALL;
                break;
            default:
                break;
        }

    } else {
        switch (ep_idx) {
            case 0:
                USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_STALL;
                break;
            case 1:
                USBFS_DEVICE->UEP1_TX_CTRL = (USBFS_DEVICE->UEP1_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            case 2:
                USBFS_DEVICE->UEP2_TX_CTRL = (USBFS_DEVICE->UEP2_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            case 3:
                USBFS_DEVICE->UEP3_TX_CTRL = (USBFS_DEVICE->UEP3_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            case 4:
                USBFS_DEVICE->UEP4_TX_CTRL = (USBFS_DEVICE->UEP4_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            case 5:
                USBFS_DEVICE->UEP5_TX_CTRL = (USBFS_DEVICE->UEP5_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            case 6:
                USBFS_DEVICE->UEP6_TX_CTRL = (USBFS_DEVICE->UEP6_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            case 7:
                USBFS_DEVICE->UEP7_TX_CTRL = (USBFS_DEVICE->UEP7_TX_CTRL &= ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_STALL;
                break;
            default:
                break;
        }
    }

    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (USB_EP_DIR_IS_OUT(ep)) {
        switch (ep_idx) {
            case 0:

                break;
            case 1:
                USBFS_DEVICE->UEP1_RX_CTRL = (USBFS_DEVICE->UEP1_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            case 2:
                USBFS_DEVICE->UEP2_RX_CTRL = (USBFS_DEVICE->UEP2_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            case 3:
                USBFS_DEVICE->UEP3_RX_CTRL = (USBFS_DEVICE->UEP3_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            case 4:
                USBFS_DEVICE->UEP4_RX_CTRL = (USBFS_DEVICE->UEP4_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            case 5:
                USBFS_DEVICE->UEP5_RX_CTRL = (USBFS_DEVICE->UEP5_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            case 6:
                USBFS_DEVICE->UEP6_RX_CTRL = (USBFS_DEVICE->UEP6_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            case 7:
                USBFS_DEVICE->UEP7_RX_CTRL = (USBFS_DEVICE->UEP7_RX_CTRL & ~(USBFS_UEP_R_TOG | USBFS_UEP_R_RES_MASK)) | USBFS_UEP_R_RES_ACK;
                break;
            default:
                break;
        }

    } else {
        switch (ep_idx) {
            case 0:

                break;
            case 1:
                USBFS_DEVICE->UEP1_TX_CTRL = (USBFS_DEVICE->UEP1_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            case 2:
                USBFS_DEVICE->UEP2_TX_CTRL = (USBFS_DEVICE->UEP2_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            case 3:
                USBFS_DEVICE->UEP3_TX_CTRL = (USBFS_DEVICE->UEP3_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            case 4:
                USBFS_DEVICE->UEP4_TX_CTRL = (USBFS_DEVICE->UEP4_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            case 5:
                USBFS_DEVICE->UEP5_TX_CTRL = (USBFS_DEVICE->UEP5_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            case 6:
                USBFS_DEVICE->UEP6_TX_CTRL = (USBFS_DEVICE->UEP6_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            case 7:
                USBFS_DEVICE->UEP7_TX_CTRL = (USBFS_DEVICE->UEP7_TX_CTRL & ~(USBFS_UEP_T_TOG | USBFS_UEP_T_RES_MASK)) | USBFS_UEP_T_RES_NAK;
                break;
            default:
                break;
        }
    }
    return 0;
}
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }

    if (!data_len) {
        switch (ep_idx) {
            case 0:
                USBFS_DEVICE->UEP0_TX_LEN = 0;
                break;
            case 1:
                USBFS_DEVICE->UEP1_TX_LEN = 0;
                USBFS_DEVICE->UEP1_TX_CTRL = (USBFS_DEVICE->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            case 2:
                USBFS_DEVICE->UEP2_TX_LEN = 0;
                USBFS_DEVICE->UEP2_TX_CTRL = (USBFS_DEVICE->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            case 3:
                USBFS_DEVICE->UEP3_TX_LEN = 0;
                USBFS_DEVICE->UEP3_TX_CTRL = (USBFS_DEVICE->UEP3_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            case 4:
                USBFS_DEVICE->UEP4_TX_LEN = 0;
                USBFS_DEVICE->UEP4_TX_CTRL = (USBFS_DEVICE->UEP4_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            case 5:
                USBFS_DEVICE->UEP5_TX_LEN = 0;
                USBFS_DEVICE->UEP5_TX_CTRL = (USBFS_DEVICE->UEP5_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            case 6:
                USBFS_DEVICE->UEP6_TX_LEN = 0;
                USBFS_DEVICE->UEP6_TX_CTRL = (USBFS_DEVICE->UEP6_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            case 7:
                USBFS_DEVICE->UEP7_TX_LEN = 0;
                USBFS_DEVICE->UEP7_TX_CTRL = (USBFS_DEVICE->UEP7_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
                break;
            default:
                break;
        }

        return 0;
    }

    if (data_len > usb_dc_cfg.in_ep[ep_idx].ep_mps) {
        data_len = usb_dc_cfg.in_ep[ep_idx].ep_mps;

        if (ep_idx == 0) {
            mps_over_flag = 1;
        }
    }

    switch (ep_idx) {
        case 0:
            memcpy(&EP0_DatabufHD[0], data, data_len);
            USBFS_DEVICE->UEP0_TX_LEN = data_len;
            break;
        case 1:
            memcpy(&EP1_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP1_TX_LEN = data_len;
            USBFS_DEVICE->UEP1_TX_CTRL = (USBFS_DEVICE->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        case 2:
            memcpy(&EP2_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP2_TX_LEN = data_len;
            USBFS_DEVICE->UEP2_TX_CTRL = (USBFS_DEVICE->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        case 3:
            memcpy(&EP3_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP3_TX_LEN = data_len;
            USBFS_DEVICE->UEP3_TX_CTRL = (USBFS_DEVICE->UEP3_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        case 4:
            memcpy(&EP4_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP4_TX_LEN = data_len;
            USBFS_DEVICE->UEP4_TX_CTRL = (USBFS_DEVICE->UEP4_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        case 5:
            memcpy(&EP5_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP5_TX_LEN = data_len;
            USBFS_DEVICE->UEP5_TX_CTRL = (USBFS_DEVICE->UEP5_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        case 6:
            memcpy(&EP6_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP6_TX_LEN = data_len;
            USBFS_DEVICE->UEP6_TX_CTRL = (USBFS_DEVICE->UEP6_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        case 7:
            memcpy(&EP7_DatabufHD[64], data, data_len);
            USBFS_DEVICE->UEP7_TX_LEN = data_len;
            USBFS_DEVICE->UEP7_TX_CTRL = (USBFS_DEVICE->UEP7_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_ACK;
            break;
        default:
            break;
    }

    if (ret_bytes) {
        *ret_bytes = data_len;
    }

    return 0;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);
    uint32_t read_count;

    if (!data && max_data_len) {
        return -1;
    }

    if (!max_data_len) {
        return 0;
    }

    read_count = USBFS_DEVICE->RX_LEN;
    read_count = MIN(read_count, max_data_len);

    switch (ep_idx) {
        case 0:
            if ((max_data_len == 8) && !read_bytes) {
                read_count = 8;
                memcpy(data, &EP0_DatabufHD[0], 8);
            } else {
                memcpy(data, &EP0_DatabufHD[0], read_count);
            }
            break;
        case 1:
            memcpy(data, &EP1_DatabufHD[0], read_count);
            break;
        case 2:
            memcpy(data, &EP2_DatabufHD[0], read_count);
            break;
        case 3:
            memcpy(data, &EP3_DatabufHD[0], read_count);
            break;
        case 4:
            memcpy(data, &EP4_DatabufHD[0], read_count);
            break;
        case 5:
            memcpy(data, &EP5_DatabufHD[0], read_count);
            break;
        case 6:
            memcpy(data, &EP6_DatabufHD[0], read_count);
            break;
        case 7:
            memcpy(data, &EP7_DatabufHD[0], read_count);
            break;
        default:
            break;
    }

    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

/*********************************************************************
 * @fn      OTG_FS_IRQHandler
 *
 * @brief   This function handles OTG_FS exception.
 *
 * @return  none
 */
void OTG_FS_IRQHandler(void)
{
    uint8_t intflag = 0;

    intflag = USBFS_DEVICE->INT_FG;

    if (intflag & USBFS_UIF_TRANSFER) {
        switch (USBFS_DEVICE->INT_ST & USBFS_UIS_TOKEN_MASK) {
            case USBFS_UIS_TOKEN_SETUP:
                USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
                USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;

                usbd_event_notify_handler(USBD_EVENT_SETUP_NOTIFY, NULL);

                USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_ACK;
                USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
                break;

            case USBFS_UIS_TOKEN_IN:
                switch (USBFS_DEVICE->INT_ST & (USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK)) {
                    case USBFS_UIS_TOKEN_IN:

                        usbd_event_notify_handler(USBD_EVENT_EP0_IN_NOTIFY, NULL);
                        if (usb_dc_cfg.dev_addr > 0) {
                            USBFS_DEVICE->DEV_ADDR = (USBFS_DEVICE->DEV_ADDR & USBFS_UDA_GP_BIT) | usb_dc_cfg.dev_addr;
                            usb_dc_cfg.dev_addr = 0;
                        }

                        if (mps_over_flag) {
                            mps_over_flag = 0;
                            USBFS_DEVICE->UEP0_TX_CTRL ^= USBFS_UEP_T_TOG;
                        } else {
                            USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
                            USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
                        }
                        break;

                    case USBFS_UIS_TOKEN_IN | 1:
                        USBFS_DEVICE->UEP1_TX_CTRL = (USBFS_DEVICE->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP1_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(1 | 0x80));
                        break;

                    case USBFS_UIS_TOKEN_IN | 2:
                        USBFS_DEVICE->UEP2_TX_CTRL = (USBFS_DEVICE->UEP2_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP2_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(2 | 0x80));
                        break;

                    case USBFS_UIS_TOKEN_IN | 3:
                        USBFS_DEVICE->UEP3_TX_CTRL = (USBFS_DEVICE->UEP3_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP3_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(3 | 0x80));
                        break;

                    case USBFS_UIS_TOKEN_IN | 4:
                        USBFS_DEVICE->UEP4_TX_CTRL = (USBFS_DEVICE->UEP4_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP4_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(4 | 0x80));
                        break;

                    case USBFS_UIS_TOKEN_IN | 5:
                        USBFS_DEVICE->UEP5_TX_CTRL = (USBFS_DEVICE->UEP5_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP5_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(5 | 0x80));
                        break;

                    case USBFS_UIS_TOKEN_IN | 6:
                        USBFS_DEVICE->UEP6_TX_CTRL = (USBFS_DEVICE->UEP6_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP6_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(6 | 0x80));
                        break;

                    case USBFS_UIS_TOKEN_IN | 7:
                        USBFS_DEVICE->UEP7_TX_CTRL = (USBFS_DEVICE->UEP7_TX_CTRL & ~USBFS_UEP_T_RES_MASK) | USBFS_UEP_T_RES_NAK;
                        USBFS_DEVICE->UEP7_TX_CTRL ^= USBFS_UEP_T_TOG;
                        usbd_event_notify_handler(USBD_EVENT_EP_IN_NOTIFY, (void *)(7 | 0x80));
                        break;

                    default:
                        break;
                }
                break;

            case USBFS_UIS_TOKEN_OUT:
                switch (USBFS_DEVICE->INT_ST & (USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK)) {
                    case USBFS_UIS_TOKEN_OUT:
                        usbd_event_notify_handler(USBD_EVENT_EP0_OUT_NOTIFY, NULL);
                        break;

                    case USBFS_UIS_TOKEN_OUT | 1:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP1_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(1 & 0x7f));
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | 2:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP2_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(2 & 0x7f));
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | 3:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP3_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(3 & 0x7f));
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | 4:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP4_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(4 & 0x7f));
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | 5:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP5_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(5 & 0x7f));
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | 6:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP6_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(6 & 0x7f));
                        }
                        break;

                    case USBFS_UIS_TOKEN_OUT | 7:
                        if (USBFS_DEVICE->INT_ST & USBFS_UIS_TOG_OK) {
                            USBFS_DEVICE->UEP7_RX_CTRL ^= USBFS_UEP_R_TOG;
                            usbd_event_notify_handler(USBD_EVENT_EP_OUT_NOTIFY, (void *)(7 & 0x7f));
                        }
                        break;
                }

                break;

            case USBFS_UIS_TOKEN_SOF:

                break;

            default:
                break;
        }

        USBFS_DEVICE->INT_FG = USBFS_UIF_TRANSFER;
    } else if (intflag & USBFS_UIF_BUS_RST) {
        USBFS_DEVICE->DEV_ADDR = 0;

        USBFS_DEVICE->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP1_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP2_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP3_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP4_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP5_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP6_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFS_DEVICE->UEP7_RX_CTRL = USBFS_UEP_R_RES_ACK;

        USBFS_DEVICE->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP1_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP2_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP3_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP4_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP5_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP6_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFS_DEVICE->UEP7_TX_CTRL = USBFS_UEP_T_RES_NAK;

        usbd_event_notify_handler(USBD_EVENT_RESET, NULL);

        USBFS_DEVICE->INT_FG |= USBFS_UIF_BUS_RST;
    } else if (intflag & USBFS_UIF_SUSPEND) {
        if (USBFS_DEVICE->MIS_ST & USBFS_UMS_SUSPEND) {
            ;
        } else {
            ;
        }
        USBFS_DEVICE->INT_FG = USBFS_UIF_SUSPEND;
    } else {
        USBFS_DEVICE->INT_FG = intflag;
    }
}
