#include "stm32f1xx_hal.h" //chanage this header for different soc
#include "usbd_core.h"

extern PCD_HandleTypeDef hpcd_USB_FS;

#ifndef USB_RAM_SIZE
#define USB_RAM_SIZE 512
#endif
#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 8
#endif

/*
 * USB and USB_OTG_FS are defined in STM32Cube HAL and allows to distinguish
 * between two kind of USB DC. STM32 F0, F3, L0 and G4 series support USB device
 * controller. STM32 F4 and F7 series support USB_OTG_FS device controller.
 * STM32 F1 and L4 series support either USB or USB_OTG_FS device controller.
 *
 * WARNING: Don't mix USB defined in STM32Cube HAL and CONFIG_USB from Zephyr
 * Kconfig system.
 */
#ifdef USB

#define EP0_MPS 64U
#define EP_MPS  64U

/*
 * USB BTABLE is stored in the PMA. The size of BTABLE is 4 bytes
 * per endpoint.
 *
 */
#define USB_BTABLE_SIZE (8 * USB_NUM_BIDIR_ENDPOINTS)

#else /* USB_OTG_FS */

#ifndef USB_OTG_MAX_EP0_SIZE
#define USB_OTG_MAX_EP0_SIZE 64
#endif
#define EP0_MPS USB_OTG_MAX_EP0_SIZE

#ifdef CONFIG_USB_HS
#ifndef USB_OTG_HS_MAX_PACKET_SIZE
#define USB_OTG_HS_MAX_PACKET_SIZE 512
#endif
#define EP_MPS USB_OTG_HS_MAX_PACKET_SIZE
#else
#ifndef USB_OTG_FS_MAX_PACKET_SIZE
#define USB_OTG_FS_MAX_PACKET_SIZE 64
#endif
#define EP_MPS USB_OTG_FS_MAX_PACKET_SIZE
#endif

/* We need one RX FIFO and n TX-IN FIFOs */
#define FIFO_NUM      (1 + USB_NUM_BIDIR_ENDPOINTS)

/* 4-byte words FIFO */
#define FIFO_WORDS    (USB_RAM_SIZE / 4)

/* Allocate FIFO memory evenly between the FIFOs */
#define FIFO_EP_WORDS (FIFO_WORDS / FIFO_NUM)

#endif /* USB */

/* Endpoint state */
struct usb_dc_ep_state {
    uint16_t ep_mps; /** Endpoint max packet size */
#ifdef USB
    uint16_t ep_pma_buf_len; /** Previously allocated buffer size */
#endif
    uint8_t ep_type;      /** Endpoint type (STM32 HAL enum) */
    uint8_t ep_stalled;   /** Endpoint stall flag */
    uint32_t read_count;  /** Number of bytes in read buffer  */
    uint32_t read_offset; /** Current offset in read buffer */
};

/* Driver state */
struct usb_dc_pcd_state {
    struct usb_dc_ep_state out_ep_state[USB_NUM_BIDIR_ENDPOINTS];
    struct usb_dc_ep_state in_ep_state[USB_NUM_BIDIR_ENDPOINTS];
    uint8_t ep_buf[USB_NUM_BIDIR_ENDPOINTS][EP_MPS];

#ifdef USB
    uint32_t pma_offset;
#endif /* USB */
};

static struct usb_dc_pcd_state usb_dc_pcd_state;

/* Internal functions */

static struct usb_dc_ep_state *usb_dc_stm32_get_ep_state(uint8_t ep)
{
    struct usb_dc_ep_state *ep_state_base;

    if (USB_EP_GET_IDX(ep) >= USB_NUM_BIDIR_ENDPOINTS) {
        return NULL;
    }

    if (USB_EP_DIR_IS_OUT(ep)) {
        ep_state_base = usb_dc_pcd_state.out_ep_state;
    } else {
        ep_state_base = usb_dc_pcd_state.in_ep_state;
    }

    return ep_state_base + USB_EP_GET_IDX(ep);
}

int usb_dc_init(void)
{
    HAL_StatusTypeDef status;
    unsigned int i;

    /*pcd has init*/
    status = HAL_PCD_Start(&hpcd_USB_FS);
    if (status != HAL_OK) {
        return -2;
    }

    usb_dc_pcd_state.out_ep_state[0].ep_mps = EP0_MPS;
    usb_dc_pcd_state.out_ep_state[0].ep_type = EP_TYPE_CTRL;
    usb_dc_pcd_state.in_ep_state[0].ep_mps = EP0_MPS;
    usb_dc_pcd_state.in_ep_state[0].ep_type = EP_TYPE_CTRL;

#ifdef USB
    /* Start PMA configuration for the endpoints after the BTABLE. */
    usb_dc_pcd_state.pma_offset = USB_BTABLE_SIZE;
#else /* USB_OTG_FS */
    /* TODO: make this dynamic (depending usage) */
    HAL_PCDEx_SetRxFiFo(&hpcd_USB_FS, FIFO_EP_WORDS);
    for (i = 0U; i < USB_NUM_BIDIR_ENDPOINTS; i++) {
        HAL_PCDEx_SetTxFiFo(&hpcd_USB_FS, i,
                            FIFO_EP_WORDS);
    }
#endif /* USB */
    return 0;
}

void usb_dc_deinit(void)
{
}

int usbd_set_address(const uint8_t addr)
{
    HAL_StatusTypeDef status;

    status = HAL_PCD_SetAddress(&hpcd_USB_FS, addr);
    if (status != HAL_OK) {
        return -2;
    }

    return 0;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    HAL_StatusTypeDef status;
    uint8_t ep = ep_cfg->ep_addr;
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

    if (!ep_state) {
        return -1;
    }

#ifdef USB
    if (ep_cfg->ep_mps > ep_state->ep_pma_buf_len) {
        if (USB_RAM_SIZE <=
            (usb_dc_pcd_state.pma_offset + ep_cfg->ep_mps)) {
            return -1;
        }
        HAL_PCDEx_PMAConfig(&hpcd_USB_FS, ep, PCD_SNG_BUF,
                            usb_dc_pcd_state.pma_offset);
        ep_state->ep_pma_buf_len = ep_cfg->ep_mps;
        usb_dc_pcd_state.pma_offset += ep_cfg->ep_mps;
    }
#endif
    ep_state->ep_mps = ep_cfg->ep_mps;

    switch (ep_cfg->ep_type) {
        case USB_DC_EP_CONTROL:
            ep_state->ep_type = EP_TYPE_CTRL;
            break;
        case USB_DC_EP_ISOCHRONOUS:
            ep_state->ep_type = EP_TYPE_ISOC;
            break;
        case USB_DC_EP_BULK:
            ep_state->ep_type = EP_TYPE_BULK;
            break;
        case USB_DC_EP_INTERRUPT:
            ep_state->ep_type = EP_TYPE_INTR;
            break;
        default:
            return -1;
    }

    status = HAL_PCD_EP_Open(&hpcd_USB_FS, ep,
                             ep_state->ep_mps, ep_state->ep_type);
    if (status != HAL_OK) {
        return -1;
    }

    if (USB_EP_DIR_IS_OUT(ep) && ep != USB_CONTROL_OUT_EP0) {
        return HAL_PCD_EP_Receive(&hpcd_USB_FS, ep,
                                  usb_dc_pcd_state.ep_buf[USB_EP_GET_IDX(ep)],
                                  EP_MPS);
    }
    return 0;
}
int usbd_ep_close(const uint8_t ep)
{
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
    HAL_StatusTypeDef status;

    if (!ep_state) {
        return -1;
    }

    status = HAL_PCD_EP_Close(&hpcd_USB_FS, ep);
    if (status != HAL_OK) {
        return -2;
    }

    return 0;
}
int usbd_ep_set_stall(const uint8_t ep)
{
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
    HAL_StatusTypeDef status;

    if (!ep_state) {
        return -1;
    }

    status = HAL_PCD_EP_SetStall(&hpcd_USB_FS, ep);
    if (status != HAL_OK) {
        return -2;
    }

    ep_state->ep_stalled = 1U;

    return 0;
}
int usbd_ep_clear_stall(const uint8_t ep)
{
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
    HAL_StatusTypeDef status;

    if (!ep_state) {
        return -1;
    }

    status = HAL_PCD_EP_ClrStall(&hpcd_USB_FS, ep);
    if (status != HAL_OK) {
        return -2;
    }

    ep_state->ep_stalled = 0U;
    ep_state->read_count = 0U;

    return 0;
}
int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);

    if (!ep_state || !stalled) {
        return -1;
    }

    *stalled = ep_state->ep_stalled;

    return 0;
}

int usbd_ep_write(const uint8_t ep, const uint8_t *data, uint32_t data_len, uint32_t *ret_bytes)
{
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
    HAL_StatusTypeDef status;
    int ret = 0;

    if (!ep_state || !USB_EP_DIR_IS_IN(ep)) {
        return -1;
    }

    if (ep == USB_CONTROL_IN_EP0 && data_len > 64) {
        data_len = 64;
    }

    status = HAL_PCD_EP_Transmit(&hpcd_USB_FS, ep,
                                 (void *)data, data_len);
    if (status != HAL_OK) {
        ret = -2;
    }

    if (!ret && ep == USB_CONTROL_IN_EP0 && data_len > 0) {
        /* Wait for an empty package as from the host.
		 * This also flushes the TX FIFO to the host.
		 */
        status = HAL_PCD_EP_Receive(&hpcd_USB_FS, ep,
                                    usb_dc_pcd_state.ep_buf[USB_EP_GET_IDX(ep)],
                                    0);
        if (status != HAL_OK) {
            return -2;
        }
    }

    if (!ret && ret_bytes) {
        *ret_bytes = data_len;
    }

    return ret;
}

int usbd_ep_read(const uint8_t ep, uint8_t *data, uint32_t max_data_len, uint32_t *read_bytes)
{
    struct usb_dc_ep_state *ep_state = usb_dc_stm32_get_ep_state(ep);
    uint32_t read_count;
    HAL_StatusTypeDef status;

    if (!ep_state) {
        return -1;
    }
    if (!USB_EP_DIR_IS_OUT(ep)) { /* check if OUT ep */

        return -1;
    }
    if (!data && max_data_len) {
        return -1;
    }

    ep_state->read_count = HAL_PCD_EP_GetRxCount(&hpcd_USB_FS, ep);
    ep_state->read_offset = 0U;
    read_count = ep_state->read_count;

    if (max_data_len == 0) {
        /* If no more data in the buffer, start a new read transaction.
	 * DataOutStageCallback will called on transaction complete.
	 */
        if (!ep_state->read_count) {
            status = HAL_PCD_EP_Receive(&hpcd_USB_FS, ep,
                                        usb_dc_pcd_state.ep_buf[USB_EP_GET_IDX(ep)],
                                        EP_MPS);
            if (status != HAL_OK) {
                return -2;
            }
        }
        return 0;
    }
    /* When both buffer and max data to read are zero, just ingore reading
	 * and return available data in buffer. Otherwise, return data
	 * previously stored in the buffer.
	 */
    if (data) {
        read_count = MIN(read_count, max_data_len);
        memcpy(data, usb_dc_pcd_state.ep_buf[USB_EP_GET_IDX(ep)] + ep_state->read_offset, read_count);
        ep_state->read_count -= read_count;
        ep_state->read_offset += read_count;
    }

    /* If no more data in the buffer, start a new read transaction.
	 * DataOutStageCallback will called on transaction complete.
	 */
    if (!ep_state->read_count) {
        status = HAL_PCD_EP_Receive(&hpcd_USB_FS, ep,
                                    usb_dc_pcd_state.ep_buf[USB_EP_GET_IDX(ep)],
                                    EP_MPS);
        if (status != HAL_OK) {
            return -2;
        }
    }
    if (read_bytes) {
        *read_bytes = read_count;
    }

    return 0;
}

/* Callbacks from the STM32 Cube HAL code */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    struct usbd_endpoint_cfg ep0_cfg;
    /* Configure control EP */
    ep0_cfg.ep_mps = EP0_MPS;
    ep0_cfg.ep_type = USB_DC_EP_CONTROL;

    ep0_cfg.ep_addr = USB_CONTROL_OUT_EP0;
    usbd_ep_open(&ep0_cfg);

    ep0_cfg.ep_addr = USB_CONTROL_IN_EP0;
    usbd_ep_open(&ep0_cfg);
    usbd_event_notify_handler(USB_EVENT_RESET, NULL);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    struct usb_setup_packet *setup = (void *)hpcd_USB_FS.Setup;

    memcpy(&usb_dc_pcd_state.ep_buf[0],
           hpcd_USB_FS.Setup, 8);

    usbd_event_notify_handler(USB_EVENT_SETUP_NOTIFY, NULL);
    if (!(setup->wLength == 0U) &&
        !(REQTYPE_GET_DIR(setup->bmRequestType) ==
          USB_REQUEST_DEVICE_TO_HOST)) {
        HAL_PCD_EP_Receive(&hpcd_USB_FS, 0x00,
                           usb_dc_pcd_state.ep_buf[0],
                           setup->wLength);
    }
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    if (epnum == 0) {
        usbd_event_notify_handler(USB_EVENT_EP0_OUT_NOTIFY, NULL);
    } else {
        usbd_event_notify_handler(USB_EVENT_EP_OUT_NOTIFY, (void *)(epnum | USB_EP_DIR_OUT));
    }
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    if (epnum == 0) {
        usbd_event_notify_handler(USB_EVENT_EP0_IN_NOTIFY, NULL);
    } else {
        usbd_event_notify_handler(USB_EVENT_EP_IN_NOTIFY, (void *)(epnum | USB_EP_DIR_IN));
    }
}