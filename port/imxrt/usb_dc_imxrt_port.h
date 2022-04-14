#ifndef _USB_DC_IMXRT_PORT
#define _USB_DC_IMXRT_PORT

/* USB Device condfiguration */
#define USB_DEVICE_USE_PORT     (1U)
#define USB_DEVICE_ENDPOINTS    (8U)
#define USB_DEVICE_MAX_DTD      (USB_DEVICE_ENDPOINTS * 2)

/*! @brief Whether the transfer buffer is cache-enabled or not. */
#ifndef USB_DEVICE_CONFIG_BUFFER_PROPERTY_CACHEABLE
#define USB_DEVICE_CONFIG_BUFFER_PROPERTY_CACHEABLE (0U)
#endif
/*! @brief Whether the low power mode is enabled or not. */
#define USB_DEVICE_CONFIG_LOW_POWER_MODE (0U)
/*! @brief Whether the device detached feature is enabled or not. */
#define USB_DEVICE_CONFIG_DETACH_ENABLE (0U)
/*! @brief Whether handle the USB bus error. */
#define USB_DEVICE_CONFIG_ERROR_HANDLING (0U)

/*! @brief Define big endian */
#define USB_BIG_ENDIAN (0U)
/*! @brief Define little endian */
#define USB_LITTLE_ENDIAN (1U)

/* USB PHY condfiguration */
#define BOARD_USB_PHY_D_CAL     (0x0CU)
#define BOARD_USB_PHY_TXCAL45DP (0x06U)
#define BOARD_USB_PHY_TXCAL45DM (0x06U)
typedef struct
{
    uint8_t sta;
    uint8_t type;
    uint16_t mps;
    uint8_t *pbuf;
}usb_dtd_buffer_t;

#if CONFIG_USB_HS
void usbd_desc_hs_register(uint8_t *descriptor);
#endif
uint8_t usb_dtd_buf_node_register(uint8_t ep, uint32_t mps);
int usb_transfer_data(uint8_t ep, uint8_t *data, uint16_t len);
#endif // !_USB_DC_IMXRT_PORT