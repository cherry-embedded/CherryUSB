#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_ch32_usbhs_reg.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USB2_IRQHandler
#endif

#ifndef CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USBHOST_PIPE_NUM 8
#endif

#ifndef USBHS_MAX_PACKET_SIZE
#define USBHS_MAX_PACKET_SIZE 1024
#endif

#define HIGH_SP_HIGH_BAND_Pos  11
#define HIGH_SP_HIGH_BAND_Mask (0x03 << HIGH_SP_HIGH_BAND_Pos)
#define HIGH_SP_HIGH_BAND_1    (0x00 << HIGH_SP_HIGH_BAND_Pos)
#define HIGH_SP_HIGH_BAND_2    (0x01 << HIGH_SP_HIGH_BAND_Pos)
#define HIGH_SP_HIGH_BAND_3    (0x02 << HIGH_SP_HIGH_BAND_Pos)

typedef enum {
    USB_EP0_STATE_SETUP = 0x0, /**< SETUP DATA */
    USB_EP0_STATE_IN_DATA,     /**< IN DATA */
    USB_EP0_STATE_IN_STATUS,   /**< IN status */
    USB_EP0_STATE_OUT_DATA,    /**< OUT DATA */
    USB_EP0_STATE_OUT_STATUS,  /**< OUT status */
} ep0_state_t;

struct chusb_pipe {
    uint8_t dev_addr;
    uint8_t ep_addr;
    uint8_t ep_type;
    uint8_t ep_interval;
    uint8_t speed;
    uint16_t ep_mps;
    bool inuse;
    uint32_t xfrd;
    volatile bool waiter;

    uint32_t xferlen;
    uint8_t *buffer;
    uint8_t data_pid;
    uint8_t trans_num;

    usb_osal_sem_t waitsem;
    struct usbh_hubport *hport;
    struct usbh_urb *urb;
};

struct chusb_hcd {
    volatile bool port_csc;
    volatile bool port_pec;
    volatile bool port_pe;
    volatile uint8_t current_token;
    volatile uint8_t ep0_state;
    volatile bool prv_get_zero;
    volatile bool prv_set_zero;
    volatile bool main_pipe_using;
    // uint32_t current_pipe_timeout;
    uint8_t dev_speed;
    struct chusb_pipe *current_pipe;
    struct chusb_pipe pipe_pool[CONFIG_USBHOST_PIPE_NUM][2]; /* Support Bidirectional ep */
} g_chusb_hcd;

static inline void SET_UH_RX_CTRL_BIT(uint8_t bit)
{
    if ((USBHS_HOST->HOST_RX_CTRL & USBHS_UH_R_TOG_AUTO) != 0) {
        /**
         * USBHS_UH_R_TOG canot write
         */
        if ((bit & (USBHS_UH_R_TOG_1 | USBHS_UH_R_TOG_2 | USBHS_UH_R_TOG_3)) != 0) {
            /**
             * bit contains USBHS_UH_R_TOG
             */
            USBHS_HOST->HOST_RX_CTRL &= ~(USBHS_UH_R_TOG_AUTO);
            USBHS_HOST->HOST_RX_CTRL |= (USBHS_UH_R_TOG_AUTO | bit);
        } else {
            USBHS_HOST->HOST_RX_CTRL |= bit;
        }
    } else {
        USBHS_HOST->HOST_RX_CTRL |= bit;
    }
}

static inline void SET_UH_TX_CTRL_BIT(uint8_t bit)
{
    if ((USBHS_HOST->HOST_TX_CTRL & USBHS_UH_T_TOG_AUTO) != 0) {
        /**
         * USBFS_UH_T_TOG canot write
         */
        if ((bit & (USBHS_UH_T_TOG_1 | USBHS_UH_T_TOG_2 | USBHS_UH_T_TOG_3)) != 0) {
            /**
             * bit contains USBFS_UH_T_TOG
             */
            USBHS_HOST->HOST_TX_CTRL &= ~(USBHS_UH_T_TOG_AUTO);
            USBHS_HOST->HOST_TX_CTRL |= (USBHS_UH_T_TOG_AUTO | bit);
        } else {
            USBHS_HOST->HOST_TX_CTRL |= bit;
        }
    } else {
        USBHS_HOST->HOST_TX_CTRL |= bit;
    }
}

static inline void CLEAR_UH_TX_CTRL_BIT(uint8_t bit)
{
    if ((USBHS_HOST->HOST_TX_CTRL & USBHS_UH_T_TOG_AUTO) != 0) {
        /**
         * USBFS_UH_T_TOG canot write
         */
        if ((bit & (USBHS_UH_T_TOG_1 | USBHS_UH_T_TOG_2 | USBHS_UH_T_TOG_3)) != 0) {
            /**
             * bit contains USBFS_UH_T_TOG
             */
            USBHS_HOST->HOST_TX_CTRL &= ~(USBHS_UH_T_TOG_AUTO);
            USBHS_HOST->HOST_TX_CTRL &= ~(bit);
            USBHS_HOST->HOST_TX_CTRL |= USBHS_UH_T_TOG_AUTO;
        } else {
            USBHS_HOST->HOST_TX_CTRL &= ~(bit);
        }
    } else {
        USBHS_HOST->HOST_TX_CTRL &= ~(bit);
    }
}

static inline void CLEAR_UH_RX_CTRL_BIT(uint8_t bit)
{
    if ((USBHS_HOST->HOST_RX_CTRL & USBHS_UH_R_TOG_AUTO) != 0) {
        /**
         * USBFS_UH_R_TOG canot write
         */
        if ((bit & (USBHS_UH_R_TOG_1 | USBHS_UH_R_TOG_2 | USBHS_UH_R_TOG_3)) != 0) {
            /**
             * bit contains USBFS_UH_R_TOG
             */
            USBHS_HOST->HOST_RX_CTRL &= ~(USBHS_UH_R_TOG_AUTO);
            USBHS_HOST->HOST_RX_CTRL &= ~(bit);
            USBHS_HOST->HOST_RX_CTRL |= USBHS_UH_R_TOG_AUTO;
        } else {
            USBHS_HOST->HOST_RX_CTRL &= ~(bit);
        }
    } else {
        USBHS_HOST->HOST_RX_CTRL &= ~(bit);
    }
}

static inline void SET_UH_RX_CTRL(uint8_t value)
{
    USBHS_HOST->HOST_RX_CTRL = 0;
    USBHS_HOST->HOST_RX_CTRL = value;
    USB_LOG_DBG("USBHS_HOST->HOST_RX_CTRL:%02x\r\n", USBHS_HOST->HOST_RX_CTRL);
}

static inline void SET_UH_TX_CTRL(uint8_t value)
{
    USBHS_HOST->HOST_TX_CTRL = 0;
    USBHS_HOST->HOST_TX_CTRL = value;
    USB_LOG_DBG("USBHS_HOST->HOST_TX_CTRL:%02x\r\n", USBHS_HOST->HOST_TX_CTRL);
}

static inline void INT_PRE_HANDLER(void)
{
#if defined(CH581) || defined(CH582) || defined(CH583) || defined(CH571) || defined(CH572) || defined(CH573)
#else
    asm("csrrw sp,mscratch,sp");
    extern void rt_interrupt_enter(void);
    rt_interrupt_enter();
#endif
}

static inline void INT_POST_HANDLER(void)
{
#if defined(CH581) || defined(CH582) || defined(CH583) || defined(CH571) || defined(CH572) || defined(CH573)
#else
    extern void rt_interrupt_leave(void);
    rt_interrupt_leave();
    asm("csrrw sp,mscratch,sp");
#endif
}

static int8_t chusb_host_pipe_transfer(struct chusb_pipe *pipe, uint8_t pid, uint8_t *data, uint32_t len)
{
    /*!< Updata current transfer pid */
    g_chusb_hcd.current_token = pid;
    /*!< Updata curretn pipe */
    g_chusb_hcd.current_pipe = pipe;
    /*!< Updata curretn pipe timeout */
    // g_chusb_hcd.current_pipe_timeout = pipe->urb->timeout;
    /*!< Updata main pipe using flag */
    // g_chusb_hcd.main_pipe_using = true;

    if (data == NULL && len > 0) {
        USB_LOG_ERR("Please give correct data and len parameters\r\n");
        return -1;
    }

    if (pid == USB_PID_SETUP) {
        /*!< Record the data len */
        g_chusb_hcd.current_pipe->xferlen = len;

        if ((uint32_t)data & 0x03) {
            USB_LOG_INFO("SETUP DMA address is not align \r\n");
            return -3;
        }

        USBHS_HOST->HOST_TX_DMA = (uint32_t)data;

        /*!< Record the data buffer address */
        pipe->buffer = data;

        if (len > pipe->ep_mps) {
            len = pipe->ep_mps;
        }

        USBHS_HOST->HOST_TX_LEN = len;
        USBHS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);

    } else if (pid == USB_PID_OUT) {
        /*!< Record the data len */
        g_chusb_hcd.current_pipe->xferlen = len;

        if (len == 0) {
            USBHS_HOST->HOST_TX_LEN = len;
            USBHS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);
            return 0;
        }

        if ((uint32_t)data & 0x03) {
            USB_LOG_WRN("OUT DMA address is not align \r\n");
            return -3;
        }

        USBHS_HOST->HOST_TX_DMA = (uint32_t)data;

        /*!< Record the data buffer address */
        pipe->buffer = data;

        if (len > pipe->ep_mps) {
            len = pipe->ep_mps;
        }

        USBHS_HOST->HOST_TX_LEN = len;
        USBHS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);

    } else if (pid == USB_PID_IN) {
        /*!< Record the data len */
        g_chusb_hcd.current_pipe->xferlen = len;

        if (len == 0) {
            /*!< Want get 0 length data */
        } else {
            if ((uint32_t)data & 0x03) {
                USB_LOG_INFO("IN DMA address is not align \r\n");
                return -3;
            }
            USBHS_HOST->HOST_RX_DMA = (uint32_t)data;
            pipe->buffer = data;
        }

        USBHS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);
    }
    return 0;
}

static void chusb_control_pipe_init(struct chusb_pipe *pipe, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    if (g_chusb_hcd.ep0_state == USB_EP0_STATE_SETUP) {
        /**
         * Setup is distributed as DATA0
         */
        SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO);
        pipe->data_pid = 0;
        chusb_host_pipe_transfer(pipe, USB_PID_SETUP, (uint8_t *)setup, 8);
    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_IN_DATA) {
        if (pipe->data_pid != 1) {
            USB_LOG_ERR("IN_DATA PID Error\r\n");
        }
        SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_TOG_1);
        chusb_host_pipe_transfer(pipe, USB_PID_IN, buffer, buflen);
    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_OUT_DATA) {
        if (pipe->data_pid != 1) {
            USB_LOG_ERR("OUT_DATA PID Error\r\n");
        }
        chusb_host_pipe_transfer(pipe, USB_PID_OUT, buffer, buflen);
    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_IN_STATUS) {
        /**
         * Status stage must be DATA1
         */
        // SET_UH_RX_CTRL_BIT(USBFS_UH_R_TOG);
        pipe->data_pid = 1;
        SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_TOG_1);
        chusb_host_pipe_transfer(pipe, USB_PID_IN, NULL, 0);
    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_OUT_STATUS) {
        /**
         * Status stage must be DATA1
         */
        // SET_UH_TX_CTRL_BIT(USBFS_UH_T_TOG);
        pipe->data_pid = 1;
        SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO | USBHS_UH_T_TOG_1);
        chusb_host_pipe_transfer(pipe, USB_PID_OUT, NULL, 0);
    }
}

static void chusb_bulk_pipe_init(struct chusb_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    if (pipe->ep_type != USB_ENDPOINT_TYPE_BULK) {
        USB_LOG_ERR("Endpoint type is not BULK\r\n");
        return;
    }

    if (pipe->ep_addr & 0x80) {
        /*!< IN */
        g_chusb_hcd.current_token = USB_PID_IN;
        if (pipe->data_pid == 1) {
            SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_TOG_1);
        } else {
            SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO);
        }
    } else {
        /*!< OUT */
        g_chusb_hcd.current_token = USB_PID_OUT;
        if (pipe->data_pid == 1) {
            SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO | USBHS_UH_T_TOG_1);
        } else {
            SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO);
        }
    }

    chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
}

static void chusb_intr_pipe_init(struct chusb_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    if (pipe->ep_type != USB_ENDPOINT_TYPE_INTERRUPT) {
        USB_LOG_ERR("Endpoint type is not INTERRUPT\r\n");
        return;
    }

    if (pipe->ep_addr & 0x80) {
        /*!< IN */
        g_chusb_hcd.current_token = USB_PID_IN;

        if (pipe->trans_num > 1) {
            USB_LOG_WRN("Not supported temporarily\r\n");
            if (pipe->trans_num == 2) {
            } else if (pipe->trans_num == 3) {
            }
        } else {
            if (pipe->data_pid == 1) {
                SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_TOG_1);
            } else {
                SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO);
            }
        }
    } else {
        /*!< OUT */
        g_chusb_hcd.current_token = USB_PID_OUT;
        if (pipe->trans_num > 1) {
            USB_LOG_WRN("Not supported temporarily\r\n");
            if (pipe->trans_num == 2) {
            } else if (pipe->trans_num == 3) {
            }
        } else {
            if (pipe->data_pid == 1) {
                SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO | USBHS_UH_T_TOG_1);
            } else {
                SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO);
            }
        }
    }

    chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
}

static void chusb_iso_pipe_init(struct chusb_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    if (pipe->ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
        USB_LOG_ERR("Endpoint type is not ISOCHRONOUS\r\n");
        return;
    }

    if (pipe->ep_addr & 0x80) {
        /*!< IN */
        g_chusb_hcd.current_token = USB_PID_IN;
        if (pipe->trans_num > 1) {
            /**
             * High speed and high bandwidth isochronous endpoint
             */
            USB_LOG_WRN("Not supported temporarily\r\n");
#if 0
            if (pipe->trans_num == 2) {
                SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_TOG_1 | USBHS_UH_R_RES_NO);
            } else if (pipe->trans_num == 3) {
                SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_TOG_2 | USBHS_UH_R_RES_NO);
            }
#endif
        } else {
            SET_UH_RX_CTRL(USBHS_UH_R_TOG_AUTO | USBHS_UH_R_RES_NO);
            chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
            return;
        }
    } else {
        /*!< OUT */
        g_chusb_hcd.current_token = USB_PID_OUT;
        if (pipe->trans_num > 1) {
            /**
             * High speed and high bandwidth isochronous endpoint
             */
            USB_LOG_WRN("Not supported temporarily\r\n");
#if 0
            if (pipe->trans_num == 2) {
                if (buflen < pipe->ep_mps) {
                    SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO | USBHS_UH_T_RES_NO);
                } else {
                    uint32_t send_len = buflen;
                    while (send_len > pipe->ep_mps) {
                        /**
                         * First send a MDATA
                         */
                        SET_UH_TX_CTRL(USBHS_UH_T_TOG_3 | USBHS_UH_T_RES_NO);
                        chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, send_len);
                        send_len -= pipe->ep_mps;
                        SET_UH_TX_CTRL(USBHS_UH_T_TOG_1 | USBHS_UH_T_RES_NO);
                        chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, (buffer + pipe->ep_mps), send_len);
                        if (send_len > pipe->ep_mps) {
                            send_len -= pipe->ep_mps;
                        }
                    }
                }
                return;
            } else if (pipe->trans_num == 3) {
                if (buflen < pipe->ep_mps) {
                    SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO | USBHS_UH_T_RES_NO);
                } else if (buflen < (2 * pipe->ep_mps)) {
                    SET_UH_TX_CTRL(USBHS_UH_T_TOG_3 | USBHS_UH_T_RES_NO);
                    chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
                    SET_UH_TX_CTRL(USBHS_UH_T_TOG_1 | USBHS_UH_T_RES_NO);
                    chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, (buffer + pipe->ep_mps), (buflen - pipe->ep_mps));
                    return;
                } else {
                    uint32_t send_len = buflen;
                    while (send_len > pipe->ep_mps) {
                        /**
                         * First send a MDATA
                         */
                        SET_UH_TX_CTRL(USBHS_UH_T_TOG_3 | USBHS_UH_T_RES_NO);
                        chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, send_len);
                        send_len -= pipe->ep_mps;
                        SET_UH_TX_CTRL(USBHS_UH_T_TOG_3 | USBHS_UH_T_RES_NO);
                        chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, (buffer + pipe->ep_mps), send_len);
                        send_len -= pipe->ep_mps;
                        SET_UH_TX_CTRL(USBHS_UH_T_TOG_2 | USBHS_UH_T_RES_NO);
                        chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, (buffer + pipe->ep_mps), send_len);
                        if (send_len > pipe->ep_mps) {
                            send_len -= pipe->ep_mps;
                        }
                    }
                }
            }
#endif
        } else {
            SET_UH_TX_CTRL(USBHS_UH_T_TOG_AUTO | USBHS_UH_T_RES_NO);
            chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
            return;
        }
    }
}

static void chusbh_set_self_speed(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
        USBHS_HOST->CONTROL = (USBHS_HOST->CONTROL & ~USBHS_SPEED_MASK) | USBHS_HIGH_SPEED;
    } else if (speed == USB_SPEED_FULL) {
        USBHS_HOST->CONTROL = (USBHS_HOST->CONTROL & ~USBHS_SPEED_MASK) | USBHS_FULL_SPEED;
    } else {
        USBHS_HOST->CONTROL = (USBHS_HOST->CONTROL & ~USBHS_SPEED_MASK) | USBHS_LOW_SPEED;
    }
}

static int usbh_reset_port(const uint8_t port)
{
    USB_LOG_INFO("Reset port \r\n");
    /*!< Clear flags that may not have been cleared */
    // g_chusb_hcd.main_pipe_using = false;

    g_chusb_hcd.port_pe = 0;
    /*!< Set dev add 0 */
    USBHS_HOST->DEV_AD = (0x00 & 0x7F);
    chusbh_set_self_speed(USB_SPEED_HIGH);

    /*!< Start reset */
    USBHS_HOST->HOST_CTRL |= USBHS_SEND_BUS_RESET;
    usb_osal_msleep(11);
    /*!< Stop reset */
    USBHS_HOST->HOST_CTRL &= ~USBHS_SEND_BUS_RESET;
    usb_osal_msleep(2);
    g_chusb_hcd.port_pe = 1;

    if (USBHS_HOST->MIS_ST & USBHS_ATTACH) {
        volatile uint8_t speed = USBHS_HOST->SPEED_TYPE & USBSPEED_MASK;
        if (speed == 0x02) {
            USB_LOG_INFO("Dev USB_SPEED_LOW \r\n");
            g_chusb_hcd.dev_speed = USB_SPEED_LOW;
            chusbh_set_self_speed(USB_SPEED_LOW);
        } else if (speed == 0x00) {
            USB_LOG_INFO("Dev USB_SPEED_FULL \r\n");
            g_chusb_hcd.dev_speed = USB_SPEED_FULL;
            chusbh_set_self_speed(USB_SPEED_FULL);
        } else if (speed == 0x01) {
            USB_LOG_INFO("Dev USB_SPEED_HIGH \r\n");
            g_chusb_hcd.dev_speed = USB_SPEED_HIGH;
            chusbh_set_self_speed(USB_SPEED_HIGH);
        }
    }

    USBHS_HOST->INT_EN |= USBHS_DETECT_EN;
    return 0;
}

static uint8_t usbh_get_port_speed(const uint8_t port)
{
    (void)port;
    return g_chusb_hcd.dev_speed;
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_init(void)
{
    memset(&g_chusb_hcd, 0, sizeof(struct chusb_hcd));

    for (uint8_t i = 0; i < CONFIG_USBHOST_PIPE_NUM; i++) {
        g_chusb_hcd.pipe_pool[i][0].waitsem = usb_osal_sem_create(0);
        g_chusb_hcd.pipe_pool[i][1].waitsem = usb_osal_sem_create(0);
    }

    usb_hc_low_level_init();

    /* Reset USB module */
    USBHS_HOST->CONTROL = USBHS_ALL_CLR | USBHS_FORCE_RST;
    static volatile uint32_t wait_ct = 50000;
    while (wait_ct--) {
    }
    USBHS_HOST->CONTROL = 0;

    /* Initialize USB host configuration */
    USBHS_HOST->CONTROL = USBHS_HOST_MODE | USBHS_HIGH_SPEED | USBHS_INT_BUSY_EN | USBHS_DMA_EN;
    USBHS_HOST->HOST_EP_CONFIG = USBHS_HOST_TX_EN | USBHS_HOST_RX_EN;
    USBHS_HOST->HOST_CTRL = USBHS_PHY_SUSPENDM;
    USBHS_HOST->HOST_RX_MAX_LEN = USBHS_MAX_PACKET_SIZE;

    USBHS_HOST->HOST_RX_CTRL = 0;
    USBHS_HOST->HOST_TX_CTRL = 0;
    USBHS_HOST->INT_FG = 0xFF;
    USBHS_HOST->INT_EN = USBHS_TRANSFER_EN | USBHS_DETECT_EN;
    return 0;
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
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
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        g_chusb_hcd.port_csc = 0;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        g_chusb_hcd.port_pec = 0;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
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

                status = 0;
                if (g_chusb_hcd.port_csc) {
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                }
                if (g_chusb_hcd.port_pec) {
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }

                if (g_chusb_hcd.port_pe) {
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);
                    if (usbh_get_port_speed(port) == USB_SPEED_LOW) {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    } else if (usbh_get_port_speed(port) == USB_SPEED_HIGH) {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                }

                memcpy(buf, &status, 4);
                break;
            default:
                break;
        }
    }
    return 0;
}

int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    struct chusb_pipe *ppipe = (struct chusb_pipe *)pipe;

    ppipe->dev_addr = dev_addr;
    ppipe->ep_mps = ep_mps;

    USBHS_HOST->DEV_AD = dev_addr & 0x7f;
    return 0;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct chusb_pipe *ppipe;
    uint8_t ep_idx;
    usb_osal_sem_t waitsem;

    ep_idx = ep_cfg->ep_addr & 0x7f;

    if (ep_idx > CONFIG_USBHOST_PIPE_NUM) {
        return -ENOMEM;
    }

    if (ep_cfg->ep_addr & 0x80) {
        ppipe = &g_chusb_hcd.pipe_pool[ep_idx][1];
    } else {
        ppipe = &g_chusb_hcd.pipe_pool[ep_idx][0];
    }

    /* store variables */
    waitsem = ppipe->waitsem;

    memset(ppipe, 0, sizeof(struct chusb_pipe));

    ppipe->ep_addr = ep_cfg->ep_addr;
    ppipe->ep_type = ep_cfg->ep_type;
    ppipe->ep_mps = ep_cfg->ep_mps;
    ppipe->ep_interval = ep_cfg->ep_interval;
    ppipe->speed = ep_cfg->hport->speed;
    ppipe->dev_addr = ep_cfg->hport->dev_addr;
    ppipe->hport = ep_cfg->hport;

    if (ep_cfg->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
    } else {
        if (ppipe->speed == USB_SPEED_HIGH) {
            if ((ep_cfg->ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS) ||
                (ep_cfg->ep_type == USB_ENDPOINT_TYPE_INTERRUPT)) {
                if ((ppipe->ep_mps & HIGH_SP_HIGH_BAND_Mask) != 0) {
                    /**
                     * High speed and high bandwidth isochronous endpoint
                     */
                    if ((ppipe->ep_mps & HIGH_SP_HIGH_BAND_Mask) == HIGH_SP_HIGH_BAND_2) {
                        ppipe->trans_num = 2;
                    } else if ((ppipe->ep_mps & HIGH_SP_HIGH_BAND_Mask) == HIGH_SP_HIGH_BAND_3) {
                        ppipe->trans_num = 3;
                    }
                } else {
                    ppipe->trans_num = 1;
                }
            }
        } else if (ppipe->speed == USB_SPEED_FULL) {
        } else if (ppipe->speed == USB_SPEED_LOW) {
        }
    }
    /* restore variable */
    ppipe->inuse = true;
    ppipe->waitsem = waitsem;

    *pipe = (usbh_pipe_t)ppipe;
    return 0;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    return 0;
}

static inline void chusb_pipe_waitup(struct chusb_pipe *pipe, bool callback)
{
    struct usbh_urb *urb;

    urb = pipe->urb;
    pipe->urb = NULL;
    // g_chusb_hcd.main_pipe_using = false;

    if (pipe->waiter) {
        pipe->waiter = false;
        usb_osal_sem_give(pipe->waitsem);
    }

    if (callback == true) {
        if (urb->complete) {
            if (urb->errorcode < 0) {
                urb->complete(urb->arg, urb->errorcode);
            } else {
                urb->complete(urb->arg, urb->actual_length);
            }
        }
    }
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    struct chusb_pipe *pipe;
    size_t flags;
    int ret = 0;

    if (!urb) {
        USB_LOG_INFO("urb is null \r\n");
        return -EINVAL;
    }

    pipe = urb->pipe;

    if (!pipe) {
        USB_LOG_INFO("pipe is null \r\n");
        return -EINVAL;
    }

    if (!pipe->hport->connected) {
        USB_LOG_INFO("!pipe->hport->connected \r\n");
        return -ENODEV;
    }

    if (pipe->urb) {
        USB_LOG_INFO("pipe->urb is not null\r\n");
        return -EBUSY;
    }

#if 0
    if (g_chusb_hcd.main_pipe_using == true) {
        USB_LOG_INFO("usbh_submit_urb//main pipe is using\r\n");
        return -EBUSY;
    }
#endif

    flags = usb_osal_enter_critical_section();

    pipe->waiter = false;
    pipe->xfrd = 0;
    pipe->urb = urb;
    urb->errorcode = -EBUSY;
    urb->actual_length = 0;

    if (urb->timeout > 0) {
        pipe->waiter = true;
    }
    usb_osal_leave_critical_section(flags);

    switch (pipe->ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
            g_chusb_hcd.ep0_state = USB_EP0_STATE_SETUP;
            chusb_control_pipe_init(pipe, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_BULK:
            chusb_bulk_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            chusb_intr_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
            chusb_iso_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
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
    g_chusb_hcd.current_token = 0;
    // g_chusb_hcd.main_pipe_using = false;
    chusb_pipe_waitup(pipe, false);
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    return 0;
}

static int8_t chusb_outpipe_irq_handler(uint8_t res_state)
{
    uint16_t current_tx_length = USBHS_HOST->HOST_TX_LEN;
    struct usbh_urb *urb;
    urb = (g_chusb_hcd.current_pipe->urb);

    if (g_chusb_hcd.current_pipe->ep_type != USB_ENDPOINT_TYPE_CONTROL) {
        if ((g_chusb_hcd.current_pipe->ep_addr & 0x80) != 0) {
            /* Error */
            USB_LOG_ERR("ep_addr is not out add \r\n");
            USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
            return -1;
        }
    }

    switch (res_state) {
        case USB_PID_STALL:
            urb->errorcode = -EPERM;
            if (g_chusb_hcd.current_token == USB_PID_SETUP) {
                USB_LOG_ERR("USB_PID_SETUP STALL \r\n");
                USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                return -2;
            } else {
                if (g_chusb_hcd.current_pipe->waiter == true) {
                    USB_LOG_WRN("USB_PID_OUT STALL \r\n");
                    chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT,
                                             g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                } else {
                    USB_LOG_ERR("USB_PID_OUT STALL \r\n");
                    USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                    return -2;
                }
            }
        case USB_PID_NAK:
            urb->errorcode = -EAGAIN;
            if (g_chusb_hcd.current_pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                if (g_chusb_hcd.current_token == USB_PID_SETUP) {
                    /**
                     * Device must ack for setup package
                     */
                    USB_LOG_ERR("Setup NAK \r\n");
                    g_chusb_hcd.current_token = 0;
                    USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                    return -3;
                } else {
                    if (g_chusb_hcd.current_pipe->waiter == true) {
                        USB_LOG_DBG("Control endpoint out nak and retry, length:%d\r\n",g_chusb_hcd.current_pipe->xferlen);
                        chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT,
                                                 g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                    }
                }
            } else {
                urb->errorcode = 0;
                if (g_chusb_hcd.prv_set_zero == true) {
                    /**
                     * It is unlikely to run here,
                     * because the device can probably receive 0 length byte packets
                     */
                    g_chusb_hcd.prv_set_zero = false;
                    g_chusb_hcd.current_token = 0;
                    urb->actual_length = g_chusb_hcd.current_pipe->xfrd;
                    chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                } else {
                    if (g_chusb_hcd.current_pipe->waiter == true) {
                        USB_LOG_DBG("Normal endpoint out nak and retry\r\n");
                        chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT,
                                                 g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                    } else {
                        g_chusb_hcd.current_token = 0;
                        if (g_chusb_hcd.current_pipe->xfrd > 0) {
                            /**
                             * The device received data but did not receive it completely
                             */
                            urb->errorcode = 0;
                            USB_LOG_WRN("The data is not sent completely, but the timeout is 0\r\n");
                            urb->actual_length = g_chusb_hcd.current_pipe->xfrd;
                            chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                        } else {
                            /**
                             * The device may not be ready, make sure your device can receive data
                             */
                            USB_LOG_WRN("The device may not be ready, make sure your device can receive data\r\n");
                            chusb_pipe_waitup(g_chusb_hcd.current_pipe, false);
                        }
                    }
                }
            }
            break;
        case USB_PID_ACK:
        case USB_PID_NYET:
            /**
             * The last out or setup package was sent
             */
            urb->errorcode = 0;

            if (g_chusb_hcd.current_pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                /*!< Ctrol endpoint */
                if (g_chusb_hcd.current_token == USB_PID_SETUP) {
                    /*!< Setup package send successfully */
                    urb->actual_length += 8;
                    g_chusb_hcd.current_pipe->data_pid ^= 1;
                    if (g_chusb_hcd.ep0_state == USB_EP0_STATE_SETUP) {
                        if (urb->setup->wLength) {
                            if (urb->setup->bmRequestType & 0x80) {
                                g_chusb_hcd.ep0_state = USB_EP0_STATE_IN_DATA;
                            } else {
                                g_chusb_hcd.ep0_state = USB_EP0_STATE_OUT_DATA;
                            }
                        } else {
                            g_chusb_hcd.ep0_state = USB_EP0_STATE_IN_STATUS;
                        }
                        chusb_control_pipe_init(g_chusb_hcd.current_pipe, urb->setup,
                                                urb->transfer_buffer, urb->transfer_buffer_length);
                    }
                } else if (g_chusb_hcd.current_token == USB_PID_OUT) {
                    if (g_chusb_hcd.ep0_state == USB_EP0_STATE_OUT_DATA) {
                        USB_LOG_DBG("ep0 tx_len:%d\r\n", current_tx_length);
                        g_chusb_hcd.current_pipe->data_pid ^= 1;
                        g_chusb_hcd.current_pipe->xfrd += current_tx_length;
                        g_chusb_hcd.current_pipe->buffer += current_tx_length;
                        g_chusb_hcd.current_pipe->xferlen -= current_tx_length;
                        if (g_chusb_hcd.current_pipe->xferlen == 0) {
                            if (current_tx_length == g_chusb_hcd.current_pipe->ep_mps) {
                                /**
                                 * Need send 0 length data
                                 */
                                chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT, NULL, 0);
                            } else {
                                /*!< Out package send successfully */
                                g_chusb_hcd.ep0_state = USB_EP0_STATE_IN_STATUS;
                                chusb_control_pipe_init(g_chusb_hcd.current_pipe, urb->setup,
                                                        urb->transfer_buffer, urb->transfer_buffer_length);
                            }
                        } else {
                            /*!< Start send next out package */
                            chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT,
                                                     g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                        }
                    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_OUT_STATUS) {
                        g_chusb_hcd.ep0_state = USB_EP0_STATE_SETUP;
                        urb->actual_length += g_chusb_hcd.current_pipe->xfrd;
                        USB_LOG_DBG("S-> I-> O-status stage: In:%d\r\n", urb->actual_length - 8);
                        if (g_chusb_hcd.current_pipe->data_pid != 1) {
                            USB_LOG_ERR("S-> I-> O-status stage DATA PID Error\r\n");
                        } else {
                            g_chusb_hcd.current_pipe->data_pid ^= 1;
                        }
                        chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                    }
                }
            } else {
                if (g_chusb_hcd.current_token == USB_PID_OUT) {
                    USB_LOG_DBG("ep%d tx_len:%d\r\n", (g_chusb_hcd.current_pipe->ep_addr & 0x0f), current_tx_length);
                    g_chusb_hcd.current_pipe->data_pid ^= 1;
                    g_chusb_hcd.current_pipe->xfrd += current_tx_length;
                    g_chusb_hcd.current_pipe->buffer += current_tx_length;
                    g_chusb_hcd.current_pipe->xferlen -= current_tx_length;
                    if (g_chusb_hcd.current_pipe->xferlen == 0) {
                        if (current_tx_length == g_chusb_hcd.current_pipe->ep_mps) {
                            /**
                             * Need send 0 length data
                             */
                            if (g_chusb_hcd.prv_set_zero == true) {
                                USB_LOG_ERR("g_chusb_hcd.prv_set_zero is always true\r\n");
                            }
                            g_chusb_hcd.prv_set_zero = true;
                            chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT, NULL, 0);
                        } else {
                            /*!< Out package send successfully */
                            g_chusb_hcd.current_token = 0;
                            if (g_chusb_hcd.prv_set_zero == true) {
                                g_chusb_hcd.prv_set_zero = false;
                            }
                            urb->actual_length = g_chusb_hcd.current_pipe->xfrd;
                            chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                        }
                    } else {
                        /*!< Start send next out package */
                        chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT,
                                                 g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                    }
                } else {
                    USB_LOG_ERR("Error token is not match \r\n");
                    USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                    return -4;
                }
            }
            break;
        case 0:
            if (g_chusb_hcd.current_pipe->ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS) {
                if (g_chusb_hcd.current_token == USB_PID_SETUP) {
                    if ((g_chusb_hcd.ep0_state == USB_EP0_STATE_SETUP) && (g_chusb_hcd.current_pipe->waiter == true)) {
                        USB_LOG_WRN("Setup Timeout and retry\r\n");
                        chusb_control_pipe_init(g_chusb_hcd.current_pipe, urb->setup,
                                                urb->transfer_buffer, urb->transfer_buffer_length);
                    } else {
                        USB_LOG_ERR("Setup Timeout\r\n");
                        urb->errorcode = -EIO;
                        USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                        return -5;
                    }
                } else {
                    USB_LOG_ERR("Out Timeout \r\n");
                    if (g_chusb_hcd.current_pipe->ep_type != USB_ENDPOINT_TYPE_CONTROL) {
                        /**
                         * Reset data pid
                         */
                        g_chusb_hcd.current_pipe->data_pid = 0;
                    }
                }
                urb->errorcode = -EIO;
            } else {
                /**
                 * No response from isochronous endpoint out
                 */
                urb->errorcode = 0;
                chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
            }
        default:
            break;
    }

    USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
    return 0;
}

static int8_t chusb_inpipe_irq_handler(uint8_t res_state)
{
    uint16_t rx_len = 0;
    struct usbh_urb *urb;
    urb = (g_chusb_hcd.current_pipe->urb);

    if (g_chusb_hcd.current_pipe->ep_type != USB_ENDPOINT_TYPE_CONTROL) {
        if ((g_chusb_hcd.current_pipe->ep_addr & 0x80) == 0) {
            /* Error */
            USB_LOG_ERR("ep_addr is not in add\r\n");
            USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
            return -1;
        }
    }

    switch (res_state) {
        case USB_PID_STALL:
            USB_LOG_ERR("USB_PID_IN USB_PID_STALL\r\n");
            g_chusb_hcd.current_token = 0;
            urb->errorcode = -EPERM;
            USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
            return -2;
        case USB_PID_NAK:
            urb->errorcode = -EAGAIN;
            g_chusb_hcd.current_token = 0;
            if (g_chusb_hcd.current_pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                if (g_chusb_hcd.current_pipe->waiter == true) {
                    chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_IN,
                                             g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                }
            } else {
                urb->errorcode = 0;
                if (g_chusb_hcd.prv_get_zero == true) {
                    g_chusb_hcd.prv_get_zero = false;
                    g_chusb_hcd.current_token = 0;
                    urb->actual_length = g_chusb_hcd.current_pipe->xfrd;
                    USB_LOG_DBG("Normal endpoint get zero length package\r\n");
                    chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                } else {
                    USB_LOG_DBG("Normal endpoint in nak\r\n");
                    if (g_chusb_hcd.current_pipe->xferlen > 0) {
                        /**
                         * The data has not been transmitted completely
                         */
                        if (g_chusb_hcd.current_pipe->xfrd > 0) {
                            /**
                             * Data was transmitted last time, but this time NAK
                             */
                            if (g_chusb_hcd.current_pipe->waiter == true) {
                                chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_IN,
                                                         g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                            } else {
                                /**
                                 * Some data has been transferred
                                 */
                                USB_LOG_WRN("The device has not finished sending all data, but the timeout is 0\r\n");
                                g_chusb_hcd.current_token = 0;
                                /**
                                 * Update the actual send length
                                 */
                                urb->actual_length = g_chusb_hcd.current_pipe->xfrd;
                                urb->errorcode = 0;
                                chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                            }
                        } else {
                            /**
                             * The device did not send any data. 
                             */
                            if (g_chusb_hcd.current_pipe->waiter == true) {
                                /**
                                 * Try again
                                 */
                                USB_LOG_DBG("The device does not transmit data, try again\r\n");
                                chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_IN,
                                                         g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                            } else {
                                /**
                                 * g_chusb_hcd.current_pipe->waiter = false
                                 * We do not need to call a callback
                                 */
                                USB_LOG_DBG("Do not need try again\r\n");
                                urb->errorcode = -EBUSY;
                                g_chusb_hcd.current_token = 0;
                                urb->actual_length = 0;
                                chusb_pipe_waitup(g_chusb_hcd.current_pipe, false);
                            }
                        }
                    } else {
                        USB_LOG_ERR("xferlen == 0//should get zero package\r\n");
                    }
                }
            }
            break;
        case USB_PID_ACK:
            urb->errorcode = 0;
            break;
        case USB_PID_DATA0:
        case USB_PID_DATA1:
        case USB_PID_DATA2:
        case USB_PID_MDATA:
            if ((USBHS_HOST->INT_ST) & USBHS_DEV_UIS_TOG_OK) {
                /*!< Data is OK */
                rx_len = USBHS_HOST->RX_LEN;

                urb->errorcode = 0;

                g_chusb_hcd.current_pipe->xfrd += rx_len;

                if (g_chusb_hcd.current_pipe->xferlen < rx_len) {
                    g_chusb_hcd.current_pipe->data_pid ^= 1;
                    USB_LOG_ERR("Please provide the correct data length parameter xferlen:%d rxlen:%d\r\n",g_chusb_hcd.current_pipe->xferlen, rx_len);
                    if (g_chusb_hcd.prv_get_zero == true) {
                        g_chusb_hcd.prv_get_zero = false;
                    }
                    USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                    return -6;
                }

                g_chusb_hcd.current_pipe->xferlen -= rx_len;

                if (g_chusb_hcd.current_pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                    /*!< Ctrol endpoint */
                    /**
                     * Status stage
                     *
                     * Setup ---> out data ---> in status stage
                     */
                    if ((g_chusb_hcd.ep0_state == USB_EP0_STATE_IN_STATUS) && (rx_len == 0)) {
                        g_chusb_hcd.ep0_state = USB_EP0_STATE_SETUP;
                        urb->actual_length += rx_len;
                        USB_LOG_DBG("S-> O-> In-status stage: Out:%d\r\n", urb->actual_length - 8);

                        if (g_chusb_hcd.current_pipe->data_pid != 1) {
                            USB_LOG_ERR("S-> O-> In-status stage DATA PID Error//PID:%d\r\n", g_chusb_hcd.current_pipe->data_pid);
                        } else {
                            g_chusb_hcd.current_pipe->data_pid ^= 1;
                        }

                        chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                        USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                        return 0;
                    }

                    /**
                     * Setup ---> in data ---> need generate next out status stage
                     */
                    if (g_chusb_hcd.ep0_state == USB_EP0_STATE_IN_DATA) {
                        USB_LOG_DBG("ep0 rx_len:%d\r\n", rx_len);
                        g_chusb_hcd.current_pipe->data_pid ^= 1;
                        if ((g_chusb_hcd.current_pipe->xfrd) > (urb->setup->wLength)) {
                            /**
                             * Error
                             */
                            USB_LOG_ERR("xfrd > setup->wLength\r\n");
                            USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                            return -3;
                        }

                        if (rx_len == 0 || (rx_len & (g_chusb_hcd.current_pipe->ep_mps - 1))) {
                            /**
                             * Receive a short package, in data has transfer completed
                             */
                            g_chusb_hcd.current_token = 0;
                            g_chusb_hcd.ep0_state = USB_EP0_STATE_OUT_STATUS;
                            /**
                             * generate next out status stage
                             */
                            chusb_control_pipe_init(g_chusb_hcd.current_pipe, urb->setup,
                                                    urb->transfer_buffer, urb->transfer_buffer_length);
                        } else {
                            /**
                             * Retry in
                             */
                            g_chusb_hcd.current_pipe->buffer += rx_len;
                            chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_IN,
                                                     g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                        }
                    }
                } else {
                    USB_LOG_DBG("ep%d rx_len:%d\r\n", (g_chusb_hcd.current_pipe->ep_addr & 0x0f), rx_len);
                    g_chusb_hcd.current_pipe->data_pid ^= 1;
                    if (rx_len == 0 || (rx_len & (g_chusb_hcd.current_pipe->ep_mps - 1)) || (g_chusb_hcd.current_pipe->xfrd == g_chusb_hcd.current_pipe->urb->transfer_buffer_length)) {
                        /**
                         * Receive a short package, in data has transfer completed
                         */
                        g_chusb_hcd.current_token = 0;
                        urb->actual_length = g_chusb_hcd.current_pipe->xfrd;

                        if (g_chusb_hcd.prv_get_zero == true) {
                            g_chusb_hcd.prv_get_zero = false;
                        }

                        chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                    } else {
                        /**
                         * Initiate In again
                         */
                        g_chusb_hcd.current_pipe->buffer += rx_len;

                        if (g_chusb_hcd.current_pipe->xferlen == 0) {
                            if (g_chusb_hcd.prv_get_zero == true) {
                                USB_LOG_ERR("g_chusb_hcd.prv_get_zero is always true\r\n");
                                g_chusb_hcd.prv_get_zero = false;
                            }
                            g_chusb_hcd.prv_get_zero = true;
                        }

                        chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_IN,
                                                 g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                    }
                }
            } else {
                /*!< Discard data out of synchronization */
                USB_LOG_ERR("EP%dData out of sync %d\r\n",
                            (g_chusb_hcd.current_pipe->ep_addr & 0x0f), g_chusb_hcd.current_pipe->data_pid);
                urb->errorcode = -EIO;
                USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                return -3;
            }
            break;
        default:
            break;
    }

    USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
    return 0;
}

__attribute__((interrupt("WCH-Interrupt-fast"))) void USBH_IRQHandler(void)
{
    INT_PRE_HANDLER();
    volatile uint8_t intflag = 0;
    volatile uint8_t res = 0;
    intflag = USBHS_HOST->INT_FG;

    if (intflag & USBHS_TRANSFER_FLAG) {
        /*!< Check the equipment response status */
        res = (USBHS_HOST->INT_ST) & MASK_UIS_H_RES;
        USBHS_HOST->HOST_EP_PID = 0x00;
        switch (g_chusb_hcd.current_token) {
            case USB_PID_SETUP:
            case USB_PID_OUT:
                if (chusb_outpipe_irq_handler(res) < 0) {
                    goto pipe_wait;
                }
                break;
            case USB_PID_IN:
                if (chusb_inpipe_irq_handler(res) < 0) {
                    goto pipe_wait;
                }
                break;
            default:
                USBHS_HOST->INT_FG = USBHS_TRANSFER_FLAG;
                break;
        }
    } else if (intflag & USBHS_DETECT_FLAG) {
        if (USBHS_HOST->MIS_ST & USBHS_ATTACH) {
            USB_LOG_INFO("Dev connect \r\n");
            g_chusb_hcd.port_csc = 1;
            g_chusb_hcd.port_pec = 1;
            g_chusb_hcd.port_pe = 1;
            USBHS_HOST->HOST_CTRL |= USBHS_SEND_SOF_EN;
            usbh_roothub_thread_wakeup(1);
        } else {
            USB_LOG_INFO("Dev remove \r\n");
            USBHS_HOST->HOST_EP_PID = 0x00;
#if 0
            if (g_chusb_hcd.main_pipe_using) {
                g_chusb_hcd.main_pipe_using = false;
            }
#endif
            g_chusb_hcd.port_csc = 1;
            g_chusb_hcd.port_pec = 1;
            g_chusb_hcd.port_pe = 0;
            for (uint8_t index = 0; index < CONFIG_USBHOST_PIPE_NUM; index++) {
                for (uint8_t j = 0; j < 2; j++) {
                    struct chusb_pipe *pipe = &g_chusb_hcd.pipe_pool[index][j];
                    struct usbh_urb *urb = pipe->urb;
                    if (pipe->waiter) {
                        pipe->waiter = false;
                        urb->errorcode = -ESHUTDOWN;
                        usb_osal_sem_give(pipe->waitsem);
                    }
                }
            }
            usbh_roothub_thread_wakeup(1);
        }
        USBHS_HOST->INT_FG = USBHS_DETECT_FLAG;
    } else {
        USB_LOG_INFO("Unkonwn \r\n");
        USBHS_HOST->INT_FG = intflag;
    }
    INT_POST_HANDLER();
    return;
pipe_wait:
    chusb_pipe_waitup(g_chusb_hcd.current_pipe, false);
    INT_POST_HANDLER();
}
