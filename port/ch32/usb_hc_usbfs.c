#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_ch32_usbfs_reg.h"

#if defined(CH581) || defined(CH582) || defined(CH583) || defined(CH571) || defined(CH572) || defined(CH573)
#undef USBFS_BASE
#undef USBFS_HOST

#ifndef USBFS_BASE
#pragma message "USB2 is used by default"
#define USBFS_BASE ((uint32_t)0x40008400u)
#endif

#define USBFS_HOST ((USB_FS_TypeDef *)USBFS_BASE)

#undef USBFS_UH_PRE_PID_EN
#undef USBFS_UH_SOF_EN
#define USBFS_UH_PRE_PID_EN 0x80 /*!< USB host PRE PID enable for low speed device via hub */
#define USBFS_UH_SOF_EN     0x40 /*!< USB host automatic SOF enable */

#undef USBFS_UH_R_AUTO_TOG
#undef USBFS_UH_R_TOG
#undef USBFS_UH_R_RES
#define USBFS_UH_R_AUTO_TOG 0x10 /*!< enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle */
#define USBFS_UH_R_TOG      0x80 /*!< expected data toggle flag of host receiving (IN): 0=DATA0, 1=DATA1 */
#define USBFS_UH_R_RES      0x04 /*!< prepared handshake response type for host receiving (IN): 0=ACK (ready), 1=no response, time out to device, for isochronous transactions */

#undef USBFS_UH_T_AUTO_TOG
#undef USBFS_UH_T_TOG
#undef USBFS_UH_T_RES
#define USBFS_UH_T_AUTO_TOG 0x10 /*!< enable automatic toggle after successful transfer completion: 0=manual toggle, 1=automatic toggle */
#define USBFS_UH_T_TOG      0x40 /*!< prepared data toggle flag of host transmittal (SETUP/OUT): 0=DATA0, 1=DATA1 */
#define USBFS_UH_T_RES      0x01 /*!< expected handshake response type for host transmittal (SETUP/OUT): 0=ACK (ready), 1=no response, time out from device, for isochronous transactions */

void USBH_IRQHandler(void) __attribute__((section(".highcode")));
#elif defined(CH32F203) || defined(CH32F205) || defined(CH32F207) || defined(CH32F208)
void USBH_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
#elif defined(CH32V203) || defined(CH32V303) || defined(CH32V305) || defined(CH32V307) || defined(CH32V208)
void USBH_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
#else
#error "Do not support"
#endif

#ifndef USBH_IRQHandler
#pragma message "Please make sure your platform interrupt name"
#define USBH_IRQHandler USB2_IRQHandler
#endif

#ifndef CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USBHOST_PIPE_NUM 8
#endif

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
    if ((USBFS_HOST->HOST_RX_CTRL & USBFS_UH_R_AUTO_TOG) != 0) {
        /**
         * USBFS_UH_R_TOG canot write
         */
        if ((bit & USBFS_UH_R_TOG) != 0) {
            /**
             * bit contains USBFS_UH_R_TOG
             */
            USBFS_HOST->HOST_RX_CTRL &= ~(USBFS_UH_R_AUTO_TOG);
            USBFS_HOST->HOST_RX_CTRL |= (USBFS_UH_R_AUTO_TOG | bit);
        } else {
            USBFS_HOST->HOST_RX_CTRL |= bit;
        }
    } else {
        USBFS_HOST->HOST_RX_CTRL |= bit;
    }
}

static inline void SET_UH_TX_CTRL_BIT(uint8_t bit)
{
    if ((USBFS_HOST->HOST_TX_CTRL & USBFS_UH_T_AUTO_TOG) != 0) {
        /**
         * USBFS_UH_T_TOG canot write
         */
        if ((bit & USBFS_UH_T_TOG) != 0) {
            /**
             * bit contains USBFS_UH_T_TOG
             */
            USBFS_HOST->HOST_TX_CTRL &= ~(USBFS_UH_T_AUTO_TOG);
            USBFS_HOST->HOST_TX_CTRL |= (USBFS_UH_T_AUTO_TOG | bit);
        } else {
            USBFS_HOST->HOST_TX_CTRL |= bit;
        }
    } else {
        USBFS_HOST->HOST_TX_CTRL |= bit;
    }
}

static inline void CLEAR_UH_TX_CTRL_BIT(uint8_t bit)
{
    if ((USBFS_HOST->HOST_TX_CTRL & USBFS_UH_T_AUTO_TOG) != 0) {
        /**
         * USBFS_UH_T_TOG canot write
         */
        if ((bit & USBFS_UH_T_TOG) != 0) {
            /**
             * bit contains USBFS_UH_T_TOG
             */
            USBFS_HOST->HOST_TX_CTRL &= ~(USBFS_UH_T_AUTO_TOG);
            USBFS_HOST->HOST_TX_CTRL &= ~(bit);
            USBFS_HOST->HOST_TX_CTRL |= USBFS_UH_T_AUTO_TOG;
        } else {
            USBFS_HOST->HOST_TX_CTRL &= ~(bit);
        }
    } else {
        USBFS_HOST->HOST_TX_CTRL &= ~(bit);
    }
}

static inline void CLEAR_UH_RX_CTRL_BIT(uint8_t bit)
{
    if ((USBFS_HOST->HOST_RX_CTRL & USBFS_UH_R_AUTO_TOG) != 0) {
        /**
         * USBFS_UH_R_TOG can't write
         */
        if ((bit & USBFS_UH_R_TOG) != 0) {
            /**
             * bit contains USBFS_UH_R_TOG
             */
            USBFS_HOST->HOST_RX_CTRL &= ~(USBFS_UH_R_AUTO_TOG);
            USBFS_HOST->HOST_RX_CTRL &= ~(bit);
            USBFS_HOST->HOST_RX_CTRL |= USBFS_UH_R_AUTO_TOG;
        } else {
            USBFS_HOST->HOST_RX_CTRL &= ~(bit);
        }
    } else {
        USBFS_HOST->HOST_RX_CTRL &= ~(bit);
    }
}

static inline void SET_UH_RX_CTRL(uint8_t value)
{
    USBFS_HOST->HOST_RX_CTRL = 0;
    USBFS_HOST->HOST_RX_CTRL = value;
    USB_LOG_DBG("USBFS_HOST->HOST_RX_CTRL:%02x\r\n", USBFS_HOST->HOST_RX_CTRL);
}

static inline void SET_UH_TX_CTRL(uint8_t value)
{
    USBFS_HOST->HOST_TX_CTRL = 0;
    USBFS_HOST->HOST_TX_CTRL = value;
    USB_LOG_DBG("USBFS_HOST->HOST_TX_CTRL:%02x\r\n", USBFS_HOST->HOST_TX_CTRL);
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
        return -1;
    }

    if (pid == USB_PID_SETUP) {
        /*!< Record the data len */
        g_chusb_hcd.current_pipe->xferlen = len;

        if ((uint32_t)data & 0x03) {
            USB_LOG_INFO("SETUP DMA address is not align \r\n");
            return -3;
        }

        USBFS_HOST->HOST_TX_DMA = (uint16_t)(uint32_t)data;

        /*!< Record the data buffer address */
        pipe->buffer = data;

        if (len > pipe->ep_mps) {
            len = pipe->ep_mps;
        }

        USBFS_HOST->HOST_TX_LEN = len;
        USBFS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);

    } else if (pid == USB_PID_OUT) {
        /*!< Record the data len */
        g_chusb_hcd.current_pipe->xferlen = len;

        if (len == 0) {
            USBFS_HOST->HOST_TX_LEN = len;
            USBFS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);
            return 0;
        }

        if ((uint32_t)data & 0x03) {
            USB_LOG_INFO("OUT DMA address is not align \r\n");
            return -3;
        }

        USBFS_HOST->HOST_TX_DMA = (uint16_t)(uint32_t)data;

        /*!< Record the data buffer address */
        pipe->buffer = data;

        if (len > pipe->ep_mps) {
            len = pipe->ep_mps;
        }

        USBFS_HOST->HOST_TX_LEN = len;
        USBFS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);

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
            USBFS_HOST->HOST_RX_DMA = (uint16_t)(uint32_t)data;
            pipe->buffer = data;
        }

        USBFS_HOST->HOST_EP_PID = pid << 4 | (pipe->ep_addr & 0x0f);
    }

    return 0;
}

static void chusb_control_pipe_init(struct chusb_pipe *pipe, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    if (g_chusb_hcd.ep0_state == USB_EP0_STATE_SETUP) {
        /**
         * Setup is distributed as DATA0
         */
        SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG);
        pipe->data_pid = 0;
        chusb_host_pipe_transfer(pipe, USB_PID_SETUP, (uint8_t *)setup, 8);
    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_IN_DATA) {
        if (pipe->data_pid != 1) {
            USB_LOG_ERR("IN_DATA PID Error\r\n");
        }
        SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG | USBFS_UH_R_TOG);
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
        SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG | USBFS_UH_R_TOG);
        chusb_host_pipe_transfer(pipe, USB_PID_IN, NULL, 0);
    } else if (g_chusb_hcd.ep0_state == USB_EP0_STATE_OUT_STATUS) {
        /**
         * Status stage must be DATA1
         */
        // SET_UH_TX_CTRL_BIT(USBFS_UH_T_TOG);
        pipe->data_pid = 1;
        SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG | USBFS_UH_T_TOG);
        chusb_host_pipe_transfer(pipe, USB_PID_OUT, NULL, 0);
    }
}

static void chusb_bulk_pipe_init(struct chusb_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    if (pipe->ep_addr & 0x80) {
        /*!< IN */
        g_chusb_hcd.current_token = USB_PID_IN;
        if (pipe->data_pid == 1) {
            SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG | USBFS_UH_R_TOG);
        } else {
            SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG);
        }
    } else {
        /*!< OUT */
        g_chusb_hcd.current_token = USB_PID_OUT;
        if (pipe->data_pid == 1) {
            SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG | USBFS_UH_T_TOG);
        } else {
            SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG);
        }
    }

    chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
}

static void chusb_intr_pipe_init(struct chusb_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    if (pipe->ep_addr & 0x80) {
        /*!< IN */
        g_chusb_hcd.current_token = USB_PID_IN;
        if (pipe->data_pid == 1) {
            SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG | USBFS_UH_R_TOG);
        } else {
            SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG);
        }
    } else {
        /*!< OUT */
        g_chusb_hcd.current_token = USB_PID_OUT;
        if (pipe->data_pid == 1) {
            SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG | USBFS_UH_T_TOG);
        } else {
            SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG);
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
        SET_UH_RX_CTRL(USBFS_UH_R_AUTO_TOG | USBFS_UH_R_RES);
    } else {
        /*!< OUT */
        g_chusb_hcd.current_token = USB_PID_OUT;
        SET_UH_TX_CTRL(USBFS_UH_T_AUTO_TOG | USBFS_UH_T_RES);
    }

    chusb_host_pipe_transfer(pipe, g_chusb_hcd.current_token, buffer, buflen);
}

static void chusbh_set_self_speed(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
    } else if (speed == USB_SPEED_FULL) {
        USBFS_HOST->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
        USBFS_HOST->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
        USBFS_HOST->HOST_SETUP &= ~USBFS_UH_PRE_PID_EN;
    } else {
        USBFS_HOST->BASE_CTRL |= USBFS_UC_LOW_SPEED;
        USBFS_HOST->HOST_CTRL |= USBFS_UH_LOW_SPEED;
        USBFS_HOST->HOST_SETUP |= USBFS_UH_PRE_PID_EN;
    }
}

static int usbh_reset_port(const uint8_t port)
{
    /*!< Disable detect interrupt */
    USBFS_HOST->INT_EN &= (~USBFS_UIE_DETECT);
    USBFS_HOST->HOST_CTRL &= ~USBFS_UH_SOF_EN;

    g_chusb_hcd.port_pe = 0;
    /*!< Set dev add 0 */
    USBFS_HOST->DEV_ADDR = (USBFS_HOST->DEV_ADDR & USBFS_UDA_GP_BIT) | (0x00 & USBFS_USB_ADDR_MASK);
    chusbh_set_self_speed(USB_SPEED_FULL);
    /*!< Close port */
    USBFS_HOST->HOST_CTRL &= ~USBFS_UH_PORT_EN;
    /*!< Start reset */
    USBFS_HOST->HOST_CTRL |= USBFS_UH_BUS_RESET;
    usb_osal_msleep(30);
    /*!< Stop reset */
    USBFS_HOST->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
    usb_osal_msleep(20);

    if ((USBFS_HOST->HOST_CTRL & USBFS_UH_PORT_EN) == 0) {
        volatile uint8_t speed = (USBFS_HOST->MIS_ST & USBFS_UMS_DM_LEVEL) ? 0 : 1;
        if (speed == 0) {
            /*!< Low speed */
            USB_LOG_INFO("Dev USB_SPEED_LOW \r\n");
            USBFS_HOST->HOST_CTRL |= USBFS_UH_LOW_SPEED;
            g_chusb_hcd.dev_speed = USB_SPEED_LOW;
            chusbh_set_self_speed(USB_SPEED_LOW);
        } else {
            /*!< Full speed */
            USB_LOG_INFO("Dev USB_SPEED_FULL \r\n");
            USBFS_HOST->HOST_CTRL &= ~USBFS_UH_LOW_SPEED;
            g_chusb_hcd.dev_speed = USB_SPEED_FULL;
        }
    }

    /*!< Enable HUB Port */
    USBFS_HOST->HOST_CTRL |= USBFS_UH_PORT_EN;
    // USBFS_HOST->HOST_SETUP |= USBFS_UH_SOF_EN;
    USBFS_HOST->INT_EN |= USBFS_UIE_DETECT;
    g_chusb_hcd.port_pe = 1;
    return 0;
}

static uint8_t usbh_get_port_speed(const uint8_t port)
{
    (void)port;
    USBFS_HOST->HOST_SETUP |= USBFS_UH_SOF_EN;
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
    USBFS_HOST->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    static uint32_t wait_ct = 50000;
    while (wait_ct--) {
    }
    USBFS_HOST->BASE_CTRL = 0;

    USBFS_HOST->BASE_CTRL = USBFS_UC_HOST_MODE;
    USBFS_HOST->HOST_CTRL = 0;

    USBFS_HOST->DEV_ADDR = 0x00;

    USBFS_HOST->HOST_EP_MOD = USBFS_UH_EP_TX_EN | USBFS_UH_EP_RX_EN;

    USBFS_HOST->HOST_RX_CTRL = 0;
    USBFS_HOST->HOST_TX_CTRL = 0;

    USBFS_HOST->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    // USBFS_HOST->HOST_SETUP = USBFS_UH_SOF_EN;
    USBFS_HOST->INT_FG = 0xFF;

    USBFS_HOST->INT_EN = USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
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

int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t mult)
{
    struct chusb_pipe *ppipe = (struct chusb_pipe *)pipe;

    ppipe->dev_addr = dev_addr;
    ppipe->ep_mps = ep_mps;

    USBFS_HOST->DEV_ADDR = (USBFS_DEV_ADDR_OFFSET & USBFS_UDA_GP_BIT) | (dev_addr & USBFS_USB_ADDR_MASK);
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

        // g_chusb_hcd.current_pipe_timeout = 0;

        ret = urb->errorcode;
    }
    return ret;
errout_timeout:
    pipe->waiter = false;
    g_chusb_hcd.current_token = 0;
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
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

static int8_t chusb_outpipe_irq_handler(uint8_t res_state)
{
    uint16_t current_tx_length = USBFS_HOST->HOST_TX_LEN;
    struct usbh_urb *urb;
    urb = (g_chusb_hcd.current_pipe->urb);

    if (g_chusb_hcd.current_pipe->ep_type != USB_ENDPOINT_TYPE_CONTROL) {
        if ((g_chusb_hcd.current_pipe->ep_addr & 0x80) != 0) {
            /* Error */
            USB_LOG_ERR("ep_addr is not out add \r\n");
            USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
            return -1;
        }
    }

    switch (res_state) {
        case USB_PID_STALL:
            urb->errorcode = -EPERM;
            if (g_chusb_hcd.current_token == USB_PID_SETUP) {
                USB_LOG_ERR("USB_PID_SETUP STALL \r\n");
            } else {
                USB_LOG_ERR("USB_PID_OUT STALL \r\n");
                if (g_chusb_hcd.current_pipe->ep_type != USB_ENDPOINT_TYPE_CONTROL) {
                    /**
                     * Reset data pid
                     */
                    g_chusb_hcd.current_pipe->data_pid = 0;
                }
            }
            USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
            return -2;
        case USB_PID_NAK:
            urb->errorcode = -EAGAIN;
            if (g_chusb_hcd.current_pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                if (g_chusb_hcd.current_token == USB_PID_SETUP) {
                    /**
                     * Device must ack for setup package
                     */
                    USB_LOG_ERR("Setup NAK \r\n");
                    g_chusb_hcd.current_token = 0;
                    USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
                    return -3;
                } else {
                    if (g_chusb_hcd.current_pipe->waiter == true) {
                        USB_LOG_DBG("Control endpoint out nak and retry\r\n");
                        chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_OUT,
                                                 g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                    }
                }
            } else {
                if (g_chusb_hcd.prv_set_zero == true) {
                    /**
                     * It is unlikely to run here,
                     * because the device can probably receive 0 length byte packets
                     */
                    urb->errorcode = 0;
                    g_chusb_hcd.prv_set_zero = false;
                    g_chusb_hcd.current_token = 0;
                    urb->actual_length = g_chusb_hcd.current_pipe->xfrd;
                    chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
                } else {
                    if (g_chusb_hcd.current_pipe->waiter == true) {
                        USB_LOG_DBG("Normal endpoint out nak and retry\r\n");
                        urb->errorcode = -EAGAIN;
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
                            USB_LOG_WRN("Your device seems unable to receive data\r\n");
                            urb->errorcode = -EBUSY;
                            chusb_pipe_waitup(g_chusb_hcd.current_pipe, false);
                        }
                    }
                }
            }
            break;
        case USB_PID_ACK:
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
                    USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
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
                        USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
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

    USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
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
            USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
            return -1;
        }
    }

    switch (res_state) {
        case USB_PID_STALL:
            USB_LOG_ERR("USB_PID_IN USB_PID_STALL\r\n");
            g_chusb_hcd.current_token = 0;
            urb->errorcode = -EPERM;
            USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
            return -2;
        case USB_PID_NAK:
            g_chusb_hcd.current_token = 0;
            if (g_chusb_hcd.current_pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                if (g_chusb_hcd.current_pipe->waiter == true) {
                    urb->errorcode = -EAGAIN;
                    chusb_host_pipe_transfer(g_chusb_hcd.current_pipe, USB_PID_IN,
                                             g_chusb_hcd.current_pipe->buffer, g_chusb_hcd.current_pipe->xferlen);
                } else {
                    urb->errorcode = 0;
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
                                /**
                                 * Retry in
                                 */
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
                                 *  Try again
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
                        urb->errorcode = -EIO;
                        USB_LOG_ERR("xferlen == 0//should get zero package\r\n");
                        USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
                        return -1;
                    }
                }
            }
            break;
        case USB_PID_ACK:
            urb->errorcode = 0;
            break;
        case USB_PID_DATA0:
        case USB_PID_DATA1:
            if ((USBFS_HOST->INT_ST) & USBFS_UIS_TOG_OK) {
                /*!< Data is OK */
                rx_len = USBFS_HOST->RX_LEN;

                urb->errorcode = 0;

                g_chusb_hcd.current_pipe->xfrd += rx_len;

                if (g_chusb_hcd.current_pipe->xferlen < rx_len) {
                    g_chusb_hcd.current_pipe->data_pid ^= 1;
                    USB_LOG_ERR("Please provide the correct data length parameter\r\n");
                    if (g_chusb_hcd.prv_get_zero == true) {
                        g_chusb_hcd.prv_get_zero = false;
                    }
                    USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
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
                        USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
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
                            USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
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
                    if (rx_len == 0 || (rx_len & (g_chusb_hcd.current_pipe->ep_mps - 1)) || ((g_chusb_hcd.current_pipe->xfrd == g_chusb_hcd.current_pipe->urb->transfer_buffer_length))) {
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
                USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
                return -3;
            }
            break;
        default:
            break;
    }

    USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
    return 0;
}

void USBH_IRQHandler(void)
{
    INT_PRE_HANDLER();
    volatile uint8_t intflag = 0;
    volatile uint8_t res = 0;
    intflag = USBFS_HOST->INT_FG;

    if (intflag & USBFS_UIF_TRANSFER) {
        /*!< Check the equipment response status */
        res = (USBFS_HOST->INT_ST) & USBFS_UIS_ENDP_MASK;
        /*!< Stop this transmission after successful transmission */
        USBFS_HOST->HOST_EP_PID = 0x00;
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
                USBFS_HOST->INT_FG = USBFS_UIF_TRANSFER;
                break;
        }
    } else if (intflag & USBFS_UIF_DETECT) {
        if (USBFS_HOST->MIS_ST & USBFS_UMS_DEV_ATTACH) {
            USB_LOG_INFO("Dev connect \r\n");
            g_chusb_hcd.port_csc = 1;
            g_chusb_hcd.port_pec = 1;
            g_chusb_hcd.port_pe = 1;
            usbh_roothub_thread_wakeup(1);
        } else {
            USB_LOG_INFO("Dev remove \r\n");
            /**
             * Device remove
             * Disable port and stop send sof
             */
            USBFS_HOST->HOST_SETUP &= ~USBFS_UH_SOF_EN;
            USBFS_HOST->HOST_CTRL &= ~USBFS_UH_PORT_EN;
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
        USBFS_HOST->INT_FG = USBFS_UIF_DETECT;
    } else {
        USB_LOG_INFO("Unkonwn \r\n");
        USBFS_HOST->INT_FG = intflag;
    }
    INT_POST_HANDLER();
    return;
pipe_wait:
    /**
     * enerally, only errors can arrive here.
     * After testing, most cases arrive here because of the problem of the DATA PID,
     * but the transmission will be completed correctly next time.
     */
    chusb_pipe_waitup(g_chusb_hcd.current_pipe, true);
    INT_POST_HANDLER();
}
