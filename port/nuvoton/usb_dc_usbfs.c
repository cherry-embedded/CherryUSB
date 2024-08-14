#include <stdint.h>
#include "NuMicro.h"
#include "usbd_core.h"

#ifndef USBD_EPNUM
#define USBD_EPNUM 8
#endif
#define USB_NUM_BIDIR_ENDPOINTS (USBD_EPNUM >> 1)

#define USBD_EP_GET_CONFIG(ep) (*((__IO uint32_t *)((uint32_t)&USBD->EP[0].CFG + (uint32_t)((ep) << 4))))

/* Define EP maximum packet size */
#define DEFAULT_EP_MAX_PKT_SIZE 64

#define EP0_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP1_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP2_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP3_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP4_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP5_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP6_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP7_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP8_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP9_MAX_PKT_SIZE  DEFAULT_EP_MAX_PKT_SIZE
#define EP10_MAX_PKT_SIZE DEFAULT_EP_MAX_PKT_SIZE
#define EP11_MAX_PKT_SIZE DEFAULT_EP_MAX_PKT_SIZE

#define SETUP_BUF_BASE 0
#define SETUP_BUF_LEN  8
#define EP0_BUF_BASE   (SETUP_BUF_BASE + SETUP_BUF_LEN)
#define EP0_BUF_LEN    EP0_MAX_PKT_SIZE
#define EP1_BUF_BASE   (SETUP_BUF_BASE + SETUP_BUF_LEN)
#define EP1_BUF_LEN    EP1_MAX_PKT_SIZE
#define EP2_BUF_BASE   (EP1_BUF_BASE + EP1_BUF_LEN)
#define EP2_BUF_LEN    EP2_MAX_PKT_SIZE
#define EP3_BUF_BASE   (EP2_BUF_BASE + EP2_BUF_LEN)
#define EP3_BUF_LEN    EP3_MAX_PKT_SIZE
#define EP4_BUF_BASE   (EP3_BUF_BASE + EP3_BUF_LEN)
#define EP4_BUF_LEN    EP4_MAX_PKT_SIZE
#define EP5_BUF_BASE   (EP4_BUF_BASE + EP4_BUF_LEN)
#define EP5_BUF_LEN    EP5_MAX_PKT_SIZE
#if USBD_EPNUM >= 8
#define EP6_BUF_BASE (EP5_BUF_BASE + EP5_BUF_LEN)
#define EP6_BUF_LEN  EP6_MAX_PKT_SIZE
#define EP7_BUF_BASE (EP6_BUF_BASE + EP6_BUF_LEN)
#define EP7_BUF_LEN  EP7_MAX_PKT_SIZE
#if USBD_EPNUM >= 10
#define EP8_BUF_BASE (EP7_BUF_BASE + EP7_BUF_LEN)
#define EP8_BUF_LEN  EP8_MAX_PKT_SIZE
#define EP9_BUF_BASE (EP8_BUF_BASE + EP8_BUF_LEN)
#define EP9_BUF_LEN  EP9_MAX_PKT_SIZE
#if USBD_EPNUM >= 12
#define EP10_BUF_BASE (EP9_BUF_BASE + EP9_BUF_LEN)
#define EP10_BUF_LEN  EP10_MAX_PKT_SIZE
#define EP11_BUF_BASE (EP10_BUF_BASE + EP10_BUF_LEN)
#define EP11_BUF_LEN  EP11_MAX_PKT_SIZE
#endif
#endif
#endif

#define USBD_EPNUM_FROM_IN_EPIDX(ep_idx)  ((ep_idx) << 1)
#define USBD_EPNUM_FROM_OUT_EPIDX(ep_idx) (((ep_idx) << 1) + 1)
#define USBD_EPNUM_FROM_EPADDR(ep_addr) \
    (USB_EP_DIR_IS_IN(ep_addr) ? USBD_EPNUM_FROM_IN_EPIDX(USB_EP_GET_IDX(ep_addr)) : USBD_EPNUM_FROM_OUT_EPIDX(USB_EP_GET_IDX(ep_addr)))

/* Endpoint state */
struct usb_dc_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
    uint32_t mps_xfer_len;
};

/* Driver state */
struct usb_dc_config_priv {
    volatile uint8_t dev_addr;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
} g_nuvoton_udc;

static uint8_t usbd_set_address_flag = 0;
static uint8_t usbd_out_toggle[USB_NUM_BIDIR_ENDPOINTS] = { 0 };

__WEAK void usb_dc_low_level_init(void)
{
}

__WEAK void usb_dc_low_level_deinit(void)
{
}

int usb_dc_init(uint8_t busid)
{
    memset(&g_nuvoton_udc, 0, sizeof(g_nuvoton_udc));

    usb_dc_low_level_init();

    /*****************************************************/
    /* Initial USB engine */
    USBD->ATTR = 0x6D0ul;
    /* Force SE0 */
    USBD_SET_SE0();

    /*****************************************************/
    /* Init setup packet buffer */
    /* Buffer for setup packet -> [0 ~ 0x7] */
    USBD->STBUFSEG = SETUP_BUF_BASE;

    USBD_SET_EP_BUF_ADDR(EP0, EP0_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP1, EP1_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP2, EP2_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP3, EP3_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP4, EP4_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP5, EP5_BUF_BASE);
#if USBD_EPNUM >= 8
    USBD_SET_EP_BUF_ADDR(EP6, EP6_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP7, EP7_BUF_BASE);
#if USBD_EPNUM >= 10
    USBD_SET_EP_BUF_ADDR(EP8, EP8_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP9, EP9_BUF_BASE);
    USBD_SET_EP_BUF_ADDR(EP10, EP10_BUF_BASE);
#if USBD_EPNUM >= 12
    USBD_SET_EP_BUF_ADDR(EP11, EP11_BUF_BASE);
#endif
#endif
#endif

    /*****************************************************/
    /* USB start */
    /* Disable software-disconnect function */
    USBD_CLR_SE0();
    USBD->ATTR = 0x7D0ul;

    /* Clear USB-related interrupts before enable interrupt */
    USBD_CLR_INT_FLAG(USBD_INT_BUS | USBD_INT_USB | USBD_INT_FLDET | USBD_INT_WAKEUP);

    /* Enable USB-related interrupts. */
    USBD_ENABLE_INT(USBD_INT_BUS | USBD_INT_USB | USBD_INT_FLDET | USBD_INT_WAKEUP);
    /*****************************************************/

    return 0;
}

int usb_dc_deinit(uint8_t busid)
{
    USBD->ATTR = 0x00000040;
    /* Force SE0 */
    USBD_SET_SE0();

    return 0;
}

int usbd_set_address(uint8_t busid, const uint8_t addr)
{
    uint8_t usbd_addr = USBD_GET_ADDR();
    if ((usbd_addr == 0) && (usbd_addr != addr)) {
        g_nuvoton_udc.dev_addr = addr;
        usbd_set_address_flag = 1;
    }

    return 0;
}

int usbd_set_remote_wakeup(uint8_t busid)
{
    return -1;
}

uint8_t usbd_get_port_speed(uint8_t busid)
{
    return USB_SPEED_FULL;
}

int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep->bEndpointAddress);

    if (USB_EP_DIR_IS_IN(ep->bEndpointAddress)) {
        uint8_t epnum = USBD_EPNUM_FROM_IN_EPIDX(ep_idx);

        g_nuvoton_udc.in_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_nuvoton_udc.in_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        g_nuvoton_udc.in_ep[ep_idx].ep_enable = true;
        if (ep_idx == 0) {
            /* EP0 ==> control IN endpoint, address 0 */
            USBD_CONFIG_EP(EP0, USBD_CFG_CSTALL | USBD_CFG_EPMODE_IN | 0);
        } else {
            USBD_CONFIG_EP(epnum, USBD_CFG_EPMODE_IN | ep_idx);
            USBD_STOP_TRANSACTION(epnum);
        }
    } else {
        uint8_t epnum = USBD_EPNUM_FROM_OUT_EPIDX(ep_idx);

        g_nuvoton_udc.out_ep[ep_idx].ep_mps = USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize);
        g_nuvoton_udc.out_ep[ep_idx].ep_type = USB_GET_ENDPOINT_TYPE(ep->bmAttributes);
        g_nuvoton_udc.out_ep[ep_idx].ep_enable = true;
        if (ep_idx == 0) {
            /* EP1 ==> control OUT endpoint, address 0 */
            USBD_CONFIG_EP(EP1, USBD_CFG_CSTALL | USBD_CFG_EPMODE_OUT | 0);
        } else {
            USBD_CONFIG_EP(epnum, USBD_CFG_EPMODE_OUT | ep_idx);
            USBD_STOP_TRANSACTION(epnum);
        }
    }

    return 0;
}

int usbd_ep_close(uint8_t busid, const uint8_t ep)
{
    USBD->EP[USBD_EPNUM_FROM_EPADDR(ep)].CFG &= ~USBD_CFG_STATE_Msk; // disable endpoint
    return 0;
}

int usbd_ep_set_stall(uint8_t busid, const uint8_t ep)
{
    USBD_SET_EP_STALL(USBD_EPNUM_FROM_EPADDR(ep));
    return 0;
}

int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep)
{
    USBD_CLR_EP_STALL(USBD_EPNUM_FROM_EPADDR(ep));
    return 0;
}

int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled)
{
    *stalled = USBD_GET_EP_STALL(USBD_EPNUM_FROM_EPADDR(ep)) > 0 ? 1 : 0;
    return 0;
}

int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_nuvoton_udc.in_ep[ep_idx].ep_enable) {
        return -2;
    }

    uint8_t epnum = USBD_EPNUM_FROM_IN_EPIDX(ep);

    g_nuvoton_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_nuvoton_udc.in_ep[ep_idx].xfer_len = data_len;
    g_nuvoton_udc.in_ep[ep_idx].actual_xfer_len = 0;

    if (data_len > g_nuvoton_udc.in_ep[ep_idx].ep_mps) {
        data_len = g_nuvoton_udc.in_ep[ep_idx].ep_mps;
    }

    if (epnum == 0) {
        USBD_SET_DATA1(epnum);
    }

    USBD_MemCopy((uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(epnum)), (uint8_t *)data, data_len);
    USBD_SET_PAYLOAD_LEN(epnum, data_len);

    return 0;
}

int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_nuvoton_udc.out_ep[ep_idx].ep_enable) {
        return -2;
    }

    uint8_t epnum = USBD_EPNUM_FROM_OUT_EPIDX(ep);

    g_nuvoton_udc.out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_nuvoton_udc.out_ep[ep_idx].xfer_len = data_len;
    g_nuvoton_udc.out_ep[ep_idx].actual_xfer_len = 0;

    if (g_nuvoton_udc.out_ep[ep_idx].xfer_len < g_nuvoton_udc.out_ep[ep_idx].ep_mps) {
        g_nuvoton_udc.out_ep[ep_idx].mps_xfer_len = g_nuvoton_udc.out_ep[ep_idx].xfer_len;
    } else {
        g_nuvoton_udc.out_ep[ep_idx].mps_xfer_len = g_nuvoton_udc.out_ep[ep_idx].ep_mps;
    }

    USBD_SET_PAYLOAD_LEN(epnum, g_nuvoton_udc.out_ep[ep_idx].mps_xfer_len);

    return 0;
}

void USBD_IRQHandler(uint8_t busid)
{
    uint32_t int_flag = USBD_GET_INT_FLAG();
    uint32_t bus_state = USBD_GET_BUS_STATE();

    if (int_flag & USBD_INTSTS_FLDET) {
        // Floating detect
        USBD_CLR_INT_FLAG(USBD_INTSTS_FLDET);

        if (USBD_IS_ATTACHED()) {
            /* USB Plug In */
            USBD_ENABLE_USB();
            usbd_event_connect_handler(0);
        } else {
            /* USB Un-plug */
            USBD_DISABLE_USB();
            usbd_event_disconnect_handler(0);
        }
    }

    //------------------------------------------------------------------
    if (int_flag & USBD_INTSTS_BUS) {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_BUS);

        if (bus_state & USBD_STATE_USBRST) {
            /* Bus reset */
            USBD_ENABLE_USB();

            memset((usbd_out_toggle + 1), 0, (USB_NUM_BIDIR_ENDPOINTS - 1));

            for (uint8_t i = 0; i < USBD_MAX_EP; i++) {
                USBD->EP[i].CFG = 0; // default value
            }

            USBD_SET_ADDR(0ul);
            g_nuvoton_udc.dev_addr = 0;

            usbd_event_reset_handler(0);
        }

        if (bus_state & USBD_STATE_SUSPEND) {
            /* Enable USB but disable PHY */
            USBD_DISABLE_PHY();
            usbd_event_suspend_handler(0);
        }
        if (bus_state & USBD_STATE_RESUME) {
            /* Enable USB and enable PHY */
            USBD_ENABLE_USB();
            usbd_event_resume_handler(0);
        }
    }

    //------------------------------------------------------------------
    if (int_flag & USBD_INTSTS_WAKEUP) {
        /* Clear event flag */
        USBD_CLR_INT_FLAG(USBD_INTSTS_WAKEUP);
    }

    if (int_flag & USBD_INTSTS_USB) {
        // USB event
        if (int_flag & USBD_INTSTS_SETUP) {
            // Setup packet
            /* Clear event flag */
            USBD_CLR_INT_FLAG(USBD_INTSTS_SETUP);

            /* Clear the data IN/OUT ready flag of control end-points */
            USBD_STOP_TRANSACTION(EP0);
            USBD_STOP_TRANSACTION(EP1);

            usbd_out_toggle[0] = 0;
            usbd_event_ep0_setup_complete_handler(0, (uint8_t *)(USBD_BUF_BASE));
        }

        if (int_flag & USBD_INTSTS_EP0) {
            /* In ACK for Set address */
            if (usbd_set_address_flag == 1) {
                USBD_SET_ADDR(g_nuvoton_udc.dev_addr);
                usbd_set_address_flag = 0;
            }
        }

        // if (int_flag & USBD_INTSTS_EP1)
        // {
        // }

        for (uint8_t epnum = 0; epnum < USBD_EPNUM; epnum++) {
            if (int_flag & (USBD_INTSTS_EP0 << epnum)) {
                USBD_CLR_INT_FLAG((USBD_INTSTS_EP0 << epnum));

                uint8_t ep_cfg = USBD_EP_GET_CONFIG(epnum);

                uint8_t ep_state = 0;
#if USBD_EPNUM >= 10
                if (epnum > 7) {
                    ep_state = (USBD->EPSTS1 >> ((epnum - 8) * 4)) & 0x0f;
                } else
#endif
                {
                    ep_state = (USBD->EPSTS0 >> (epnum * 4)) & 0x0f;
                }

                uint8_t ep_cfg_state = (ep_cfg & USBD_CFG_STATE_Msk) >> USBD_CFG_STATE_Pos;

                if (ep_cfg_state == 0x01) {
                    // OUT
                    uint8_t ep_idx = ep_cfg & USBD_CFG_EPNUM_Msk;

                    if (ep_state == usbd_out_toggle[ep_idx]) {
                        USBD_SET_PAYLOAD_LEN(epnum, g_nuvoton_udc.out_ep[ep_idx].ep_mps);
                    } else {
                        uint32_t recv_count = USBD_GET_PAYLOAD_LEN(epnum);

                        USBD_MemCopy((uint8_t *)g_nuvoton_udc.out_ep[ep_idx].xfer_buf,
                                     (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(epnum)), recv_count);

                        g_nuvoton_udc.out_ep[ep_idx].xfer_buf += recv_count;
                        g_nuvoton_udc.out_ep[ep_idx].xfer_len -= recv_count;
                        g_nuvoton_udc.out_ep[ep_idx].actual_xfer_len += recv_count;

                        usbd_out_toggle[ep_idx] = ep_state;

                        if ((recv_count < g_nuvoton_udc.out_ep[ep_idx].ep_mps) ||
                            (g_nuvoton_udc.out_ep[ep_idx].xfer_len == 0)) {
                            usbd_event_ep_out_complete_handler(0, ep_idx, g_nuvoton_udc.out_ep[ep_idx].actual_xfer_len);
                        } else {
                            if (g_nuvoton_udc.out_ep[ep_idx].xfer_len < g_nuvoton_udc.out_ep[ep_idx].ep_mps) {
                                g_nuvoton_udc.out_ep[ep_idx].mps_xfer_len = g_nuvoton_udc.out_ep[ep_idx].xfer_len;
                            } else {
                                g_nuvoton_udc.out_ep[ep_idx].mps_xfer_len = g_nuvoton_udc.out_ep[ep_idx].ep_mps;
                            }

                            USBD_SET_PAYLOAD_LEN(epnum, g_nuvoton_udc.out_ep[ep_idx].mps_xfer_len);
                        }
                    }
                } else if (ep_cfg_state == 0x02) {
                    uint8_t ep_idx = ep_cfg & USBD_CFG_EPNUM_Msk;
                    // In Ack

                    if (g_nuvoton_udc.in_ep[ep_idx].xfer_len > g_nuvoton_udc.in_ep[ep_idx].ep_mps) {
                        g_nuvoton_udc.in_ep[ep_idx].xfer_buf += g_nuvoton_udc.in_ep[ep_idx].ep_mps;
                        g_nuvoton_udc.in_ep[ep_idx].xfer_len -= g_nuvoton_udc.in_ep[ep_idx].ep_mps;
                        g_nuvoton_udc.in_ep[ep_idx].actual_xfer_len += g_nuvoton_udc.in_ep[ep_idx].ep_mps;

                        uint16_t min_len = MIN(g_nuvoton_udc.in_ep[ep_idx].xfer_len, g_nuvoton_udc.in_ep[ep_idx].ep_mps);

                        uint8_t *usbd_ep_buf_addr = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(epnum));
                        USBD_MemCopy(usbd_ep_buf_addr, g_nuvoton_udc.in_ep[ep_idx].xfer_buf, min_len);
                        USBD_SET_PAYLOAD_LEN(epnum, min_len);
                    } else {
                        g_nuvoton_udc.in_ep[ep_idx].actual_xfer_len += g_nuvoton_udc.in_ep[ep_idx].xfer_len;
                        g_nuvoton_udc.in_ep[ep_idx].xfer_len = 0;

                        usbd_event_ep_in_complete_handler(0, ep_idx | 0x80, g_nuvoton_udc.in_ep[ep_idx].actual_xfer_len);
                    }
                }
            }
        }
    }
}
