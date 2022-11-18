#include "usbd_core.h"
#include "usb_rp2040_reg.h"

#ifndef USBD_IRQHandler
#define USBD_IRQHandler isr_irq5
#endif

#ifndef USB_NUM_BIDIR_ENDPOINTS
#define USB_NUM_BIDIR_ENDPOINTS 16
#endif

#ifndef FORCE_VBUS_DETECT
#define FORCE_VBUS_DETECT 1
#endif

/* Endpoint state */
struct usb_dc_ep_state {
    uint16_t ep_mps;    /* Endpoint max packet size */
    uint8_t ep_type;    /* Endpoint type */
    uint8_t ep_stalled; /* Endpoint stall flag */
    uint8_t ep_enable;  /* Endpoint enable */
    uint8_t ep_addr;    /* Endpoint address */
    uint8_t *xfer_buf;
    uint32_t xfer_len;
    uint32_t actual_xfer_len;
    /**
     * For rp2040
     */
    volatile uint32_t *endpoint_control; /*!< Endpoint control register */
    volatile uint32_t *buffer_control;   /*!< Buffer control register */
    uint8_t *dpram_data_buf;             /*!< Buffer pointer in usb dpram */
    uint8_t next_pid;                    /*!< Toggle after each packet (unless replying to a SETUP) */
};

/* Driver state */
struct rp2040_udc {
    volatile uint8_t dev_addr;
    struct usb_dc_ep_state in_ep[USB_NUM_BIDIR_ENDPOINTS];  /*!< IN endpoint parameters*/
    struct usb_dc_ep_state out_ep[USB_NUM_BIDIR_ENDPOINTS]; /*!< OUT endpoint parameters */
    struct usb_setup_packet setup;                          /*!< Setup package that may be used in interrupt processing (outside the protocol stack) */
} g_rp2040_udc;

static uint8_t *next_buffer_ptr;

/**
 * @brief Take a buffer pointer located in the USB RAM and return as an offset of the RAM.
 *
 * @param buf
 * @return uint32_t
 */
static inline uint32_t usb_buffer_offset(volatile uint8_t *buf)
{
    return (uint32_t)buf ^ (uint32_t)usb_dpram;
}

/**
 * @brief Alloc the endpoint dpram and set up ep (if applicable. Not valid for EP0).
 *
 * @param ep
 */
static int8_t rp2040_usb_config_ep(struct usb_dc_ep_state *ep)
{
    if (!ep->endpoint_control) {
        USB_LOG_WRN("Not valid for EP0 \r\n");
        return 0;
    }

    /*!< size must be multiple of 64 */
    uint16_t size = ((ep->ep_mps + 64 - 1) / 64) * 64;
    /*!< Get current buffer ptr */
    ep->dpram_data_buf = next_buffer_ptr;
    /*!< Update the next buffer ptr */
    next_buffer_ptr += size;
    if (((uint32_t)next_buffer_ptr & 0b111111u) != 0) {
        USB_LOG_ERR("DPRAM Not 64 byte aligned \r\n");
        return -1;
    }
    uint32_t dpram_offset = usb_buffer_offset(ep->dpram_data_buf);
    if (dpram_offset > USB_DPRAM_MAX) {
        USB_LOG_ERR("DPRAM overflow \r\n");
        return -2;
    }
    USB_LOG_INFO("Alloced %d bytes at offset 0x%x (0x%p)\r\n", size, dpram_offset, ep->dpram_data_buf);
    /*!< Enable ep and perbuffer trigger interrupt */
    volatile uint32_t reg = EP_CTRL_ENABLE_BITS | EP_CTRL_INTERRUPT_PER_BUFFER | ((ep->ep_type) << EP_CTRL_BUFFER_TYPE_LSB) | dpram_offset;
    *ep->endpoint_control = reg;
    return 0;
}

static void rp2040_usb_init(void)
{
    /*!< Reset usb controller */
    reset_block(RESETS_RESET_USBCTRL_BITS);
    unreset_block_wait(RESETS_RESET_USBCTRL_BITS);

    /*!< Clear any previous state just in case */
    memset(usb_hw, 0, sizeof(*usb_hw));
    memset(usb_dpram, 0, sizeof(*usb_dpram));

    /*!< Mux the controller to the onboard usb phy */
    usb_hw->muxing = USB_USB_MUXING_TO_PHY_BITS | USB_USB_MUXING_SOFTCON_BITS;
}

int usb_dc_init(void)
{
    memset(&g_rp2040_udc, 0, sizeof(struct rp2040_udc));
    rp2040_usb_init();
#if FORCE_VBUS_DETECT
    /*!< Force VBUS detect so the device thinks it is plugged into a host */
    usb_hw->pwr = USB_USB_PWR_VBUS_DETECT_BITS | USB_USB_PWR_VBUS_DETECT_OVERRIDE_EN_BITS;
#endif

    /**
     * Initializes the USB peripheral for device mode and enables it.
     * Don't need to enable the pull up here. Force VBUS
     */
    usb_hw->main_ctrl = USB_MAIN_CTRL_CONTROLLER_EN_BITS;

    /**
     * Enable individual controller IRQS here. Processor interrupt enable will be used
     * for the global interrupt enable...
     * Note: Force VBUS detect cause disconnection not detectable
     */
    usb_hw->sie_ctrl = USB_SIE_CTRL_EP0_INT_1BUF_BITS;
    usb_hw->inte = USB_INTS_BUFF_STATUS_BITS | USB_INTS_BUS_RESET_BITS | USB_INTS_SETUP_REQ_BITS |
                   USB_INTS_DEV_SUSPEND_BITS | USB_INTS_DEV_RESUME_FROM_HOST_BITS |
                   (FORCE_VBUS_DETECT ? 0 : USB_INTS_DEV_CONN_DIS_BITS);

    /**
     * Enable interrupt
     * Clear pending before enable
     * (if IRQ is actually asserted, it will immediately re-pend)
     */
    *((io_rw_32 *)(PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET)) = 1 << 5;
    *((io_rw_32 *)(PPB_BASE + M0PLUS_NVIC_ISER_OFFSET)) = 1 << 5;

    usb_hw_set->sie_ctrl = USB_SIE_CTRL_PULLUP_EN_BITS;
    return 0;
}

/**
 * @brief Starts a transfer on a given endpoint.
 *
 * @param ep, the endpoint configuration.
 * @param buf, the data buffer to send. Only applicable if the endpoint is TX
 * @param len, the length of the data in buf (this example limits max len to one packet - 64 bytes)
 */
static void usb_start_transfer(struct usb_dc_ep_state *ep, uint8_t *buf, uint16_t len)
{
    /*!< Prepare buffer control register value */
    uint32_t val = len | USB_BUF_CTRL_AVAIL;
    if (len < ep->ep_mps) {
        val |= USB_BUF_CTRL_LAST;
    }

    if (USB_EP_DIR_IS_IN(ep->ep_addr)) {
        /*!< Need to copy the data from the user buffer to the usb memory */
        if (buf != NULL) {
            memcpy((void *)ep->dpram_data_buf, (void *)buf, len);
        }
        /*!< Mark as full */
        val |= USB_BUF_CTRL_FULL;
    } else {
    }

    /*!< Set pid and flip for next transfer */
    val |= ep->next_pid ? USB_BUF_CTRL_DATA1_PID : USB_BUF_CTRL_DATA0_PID;
    ep->next_pid ^= 1u;
    /**
     * !Need delay some cycles
     * nop for some clk_sys cycles to ensure that at least one clk_usb cycle has passed. For example if clk_sys was running
     * at 125MHz and clk_usb was running at 48MHz then 125/48 rounded up would be 3 nop instructions
     */
    *ep->buffer_control = val & ~USB_BUF_CTRL_AVAIL;
    __asm volatile(
        "b 1f\n"
        "1: b 1f\n"
        "1: b 1f\n"
        "1: b 1f\n"
        "1: b 1f\n"
        "1: b 1f\n"
        "1: b 1f\n"
        "1:\n"
        :
        :
        : "memory");
    *ep->buffer_control = val;
}

int usb_dc_deinit(void)
{
    return 0;
}

int usbd_set_address(const uint8_t addr)
{
    if (addr != 0) {
        g_rp2040_udc.dev_addr = addr;
    }
    return 0;
}

uint8_t usbd_get_port_speed(const uint8_t port)
{
    return USB_SPEED_FULL;
}

int usbd_ep_open(const struct usbd_endpoint_cfg *ep_cfg)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep_cfg->ep_addr);

    if (ep_idx == 0) {
        /**
         * A device must support Endpoint 0 so that it can reply to SETUP packets and be enumerated. As a result, there is no
         * endpoint control register for EP0. Its buffers begin at 0x100. All other endpoints can have either single or dual buffers
         * and are mapped at the base address programmed. As EP0 has no endpoint control register, the interrupt enable
         * controls for EP0 come from SIE_CTRL.
         */
        g_rp2040_udc.out_ep[ep_idx].endpoint_control = NULL;
        g_rp2040_udc.out_ep[ep_idx].dpram_data_buf = (uint8_t *)&usb_dpram->ep0_buf_a[0];
        g_rp2040_udc.in_ep[ep_idx].endpoint_control = NULL;
        g_rp2040_udc.in_ep[ep_idx].dpram_data_buf = (uint8_t *)&usb_dpram->ep0_buf_a[0];
    }

    if (USB_EP_DIR_IS_OUT(ep_cfg->ep_addr)) {
        g_rp2040_udc.out_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_rp2040_udc.out_ep[ep_idx].ep_type = ep_cfg->ep_type;
        g_rp2040_udc.out_ep[ep_idx].ep_addr = ep_cfg->ep_addr;
        g_rp2040_udc.out_ep[ep_idx].ep_enable = true;
        /*!< Get control reg */
        g_rp2040_udc.out_ep[ep_idx].buffer_control = &usb_dpram->ep_buf_ctrl[ep_idx].out;
        /*!< Clear control reg */
        *(g_rp2040_udc.out_ep[ep_idx].buffer_control) = 0;

        if (ep_idx != 0) {
            g_rp2040_udc.out_ep[ep_idx].endpoint_control = &usb_dpram->ep_ctrl[ep_idx - 1].out;
            /**
             * Allocate a buffer on DPRAM for the endpoint
             */
            return rp2040_usb_config_ep(&g_rp2040_udc.out_ep[ep_idx]);
        }

    } else {
        g_rp2040_udc.in_ep[ep_idx].ep_mps = ep_cfg->ep_mps;
        g_rp2040_udc.in_ep[ep_idx].ep_type = ep_cfg->ep_type;
        g_rp2040_udc.in_ep[ep_idx].ep_addr = ep_cfg->ep_addr;
        g_rp2040_udc.in_ep[ep_idx].ep_enable = true;
        /*!< Get control reg */
        g_rp2040_udc.in_ep[ep_idx].buffer_control = &usb_dpram->ep_buf_ctrl[ep_idx].in;
        /*!< Clear control reg */
        *(g_rp2040_udc.in_ep[ep_idx].buffer_control) = 0;

        if (ep_idx != 0) {
            g_rp2040_udc.in_ep[ep_idx].endpoint_control = &usb_dpram->ep_ctrl[ep_idx - 1].in;
            /**
             * Allocate a buffer on DPRAM for the endpoint
             */
            return rp2040_usb_config_ep(&g_rp2040_udc.in_ep[ep_idx]);
        }
    }
    return 0;
}

int usbd_ep_close(const uint8_t ep)
{
    /*!< Ep id */
    uint16_t size = 0;

    uint8_t epid = USB_EP_GET_IDX(ep);
    if (USB_EP_DIR_IS_IN(ep)) {
        /*!< In */
        size = ((g_rp2040_udc.in_ep[epid].ep_mps + 64 - 1) / 64) * 64;
        memset(g_rp2040_udc.in_ep[epid].dpram_data_buf, 0, size);
        next_buffer_ptr -= size;
        g_rp2040_udc.in_ep[epid].ep_enable = false;
    } else if (USB_EP_DIR_IS_OUT(ep)) {
        /*!< Out */
        size = ((g_rp2040_udc.out_ep[epid].ep_mps + 64 - 1) / 64) * 64;
        memset(g_rp2040_udc.out_ep[epid].dpram_data_buf, 0, size);
        next_buffer_ptr -= size;
        g_rp2040_udc.out_ep[epid].ep_enable = false;
    }
    return 0;
}

int usbd_ep_set_stall(const uint8_t ep)
{
    if (USB_EP_GET_IDX(ep) == 0) {
        /**
         * A stall on EP0 has to be armed so it can be cleared on the next setup packet
         */
        usb_hw_set->ep_stall_arm = (USB_EP_DIR_IS_IN(ep)) ? USB_EP_STALL_ARM_EP0_IN_BITS : USB_EP_STALL_ARM_EP0_OUT_BITS;
    }

    if (USB_EP_DIR_IS_OUT(ep)) {
        *(g_rp2040_udc.out_ep[USB_EP_GET_IDX(ep)].buffer_control) = USB_BUF_CTRL_STALL;
    } else {
        *(g_rp2040_udc.in_ep[USB_EP_GET_IDX(ep)].buffer_control) = USB_BUF_CTRL_STALL;
    }

    return 0;
}

int usbd_ep_clear_stall(const uint8_t ep)
{
    volatile uint32_t value = 0;
    if (USB_EP_GET_IDX(ep)) {
        if (USB_EP_DIR_IS_OUT(ep)) {
            g_rp2040_udc.out_ep[USB_EP_GET_IDX(ep)].next_pid = 0;
            value = *(g_rp2040_udc.out_ep[USB_EP_GET_IDX(ep)].buffer_control) & (~USB_BUF_CTRL_STALL);
            *(g_rp2040_udc.out_ep[USB_EP_GET_IDX(ep)].buffer_control) = value;
        } else {
            g_rp2040_udc.in_ep[USB_EP_GET_IDX(ep)].next_pid = 0;
            value = *(g_rp2040_udc.in_ep[USB_EP_GET_IDX(ep)].buffer_control) & (~USB_BUF_CTRL_STALL);
            *(g_rp2040_udc.in_ep[USB_EP_GET_IDX(ep)].buffer_control) = value;
        }
    }
    return 0;
}

int usbd_ep_is_stalled(const uint8_t ep, uint8_t *stalled)
{
    return 0;
}

int usbd_ep_start_write(const uint8_t ep, const uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_rp2040_udc.in_ep[ep_idx].ep_enable) {
        return -2;
    }

    g_rp2040_udc.in_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_rp2040_udc.in_ep[ep_idx].xfer_len = data_len;
    g_rp2040_udc.in_ep[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        usb_start_transfer(&g_rp2040_udc.in_ep[ep_idx], NULL, 0);
        return 0;
    } else {
        /*!< Not zlp */
        data_len = MIN(data_len, g_rp2040_udc.in_ep[ep_idx].ep_mps);
        usb_start_transfer(&g_rp2040_udc.in_ep[ep_idx], g_rp2040_udc.in_ep[ep_idx].xfer_buf, data_len);
    }

    return 0;
}

int usbd_ep_start_read(const uint8_t ep, uint8_t *data, uint32_t data_len)
{
    uint8_t ep_idx = USB_EP_GET_IDX(ep);

    if (!data && data_len) {
        return -1;
    }
    if (!g_rp2040_udc.out_ep[ep_idx].ep_enable) {
        return -2;
    }
    g_rp2040_udc.out_ep[ep_idx].xfer_buf = (uint8_t *)data;
    g_rp2040_udc.out_ep[ep_idx].xfer_len = data_len;
    g_rp2040_udc.out_ep[ep_idx].actual_xfer_len = 0;

    if (data_len == 0) {
        usb_start_transfer(&g_rp2040_udc.out_ep[ep_idx], NULL, 0);
        return 0;
    } else {
        /*!< Not zlp */
        data_len = MIN(data_len, g_rp2040_udc.out_ep[ep_idx].ep_mps);
        usb_start_transfer(&g_rp2040_udc.out_ep[ep_idx], g_rp2040_udc.out_ep[ep_idx].xfer_buf, data_len);
    }
    return 0;
}

/**
 * @brief Notify an endpoint that a transfer has completed.
 *
 * @param ep, the endpoint to notify.
 */
static void usb_handle_ep_buff_done(struct usb_dc_ep_state *ep)
{
    uint32_t buffer_control = *ep->buffer_control;
    /*!< Get the transfer length for this endpoint */
    uint16_t read_count = buffer_control & USB_BUF_CTRL_LEN_MASK;
    /*!< Call that endpoints buffer done handler */
    if (ep->ep_addr == 0x80) {
        /*!< EP0 In */
        /**
         * Determine the current setup direction
         */
        switch (g_rp2040_udc.setup.bmRequestType >> USB_REQUEST_DIR_SHIFT) {
            case 1:
                /*!< Get */
                if (g_rp2040_udc.in_ep[0].xfer_len > g_rp2040_udc.in_ep[0].ep_mps) {
                    g_rp2040_udc.in_ep[0].xfer_len -= g_rp2040_udc.in_ep[0].ep_mps;
                    g_rp2040_udc.in_ep[0].actual_xfer_len += g_rp2040_udc.in_ep[0].ep_mps;
                    usbd_event_ep_in_complete_handler(0 | 0x80, g_rp2040_udc.in_ep[0].actual_xfer_len);
                } else {
                    g_rp2040_udc.in_ep[0].actual_xfer_len += g_rp2040_udc.in_ep[0].xfer_len;
                    g_rp2040_udc.in_ep[0].xfer_len = 0;
                    /**
                     * EP0 In complete and host will send a out token to get 0 length packet
                     * In the next usbd_event_ep_in_complete_handler, stack will start read 0 length packet
                     * and host must send data1 packet.We resest the ep0 next_pid = 1 in setup interrupt head.
                     */
                    usbd_event_ep_in_complete_handler(0 | 0x80, g_rp2040_udc.in_ep[0].actual_xfer_len);
                }
                break;
            case 0:
                /*!< Set */
                if (g_rp2040_udc.dev_addr > 0) {
                    usb_hw->dev_addr_ctrl = g_rp2040_udc.dev_addr;
                    g_rp2040_udc.dev_addr = 0;
                } else {
                    /*!< Normal status stage // Setup  out...out  in  */
                    /**
                     * Perpar for next setup
                     */
                }
                break;
        }

    } else if (ep->ep_addr == 0x00) {
        /*!< EP0 Out */
        memcpy(g_rp2040_udc.out_ep[0].xfer_buf, g_rp2040_udc.out_ep[0].dpram_data_buf, read_count);
        if (read_count == 0) {
            /*!< Normal status stage // Setup  in...in  out  */
            /**
              * Perpar for next setup
              */
        }

        g_rp2040_udc.out_ep[0].actual_xfer_len += read_count;
        g_rp2040_udc.out_ep[0].xfer_len -= read_count;

        usbd_event_ep_out_complete_handler(0x00, g_rp2040_udc.out_ep[0].actual_xfer_len);
    } else {
        /*!< Others ep */
        uint16_t data_len = 0;
        if (USB_EP_DIR_IS_OUT(ep->ep_addr)) {
            /*!< flip the pid */
            memcpy(g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].xfer_buf, g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].dpram_data_buf, read_count);
            g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].xfer_buf += read_count;
            g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].actual_xfer_len += read_count;
            g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].xfer_len -= read_count;

            if (read_count < g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].ep_mps || g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].xfer_len == 0) {
                /*!< Out complete */
                usbd_event_ep_out_complete_handler(ep->ep_addr, g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].actual_xfer_len);
            } else {
                /*!< Need read again */
                data_len = MIN(g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].xfer_len, g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f].ep_mps);
                usb_start_transfer(&g_rp2040_udc.out_ep[(ep->ep_addr) & 0x0f], NULL, data_len);
            }
        } else {
            if (g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_len > g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].ep_mps) {
                /*!< Need tx again */
                g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_len -= g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].ep_mps;
                g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_buf += g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].ep_mps;
                g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].actual_xfer_len += g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].ep_mps;
                data_len = MIN(g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_len, g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].ep_mps);
                usb_start_transfer(&g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f], g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_buf, data_len);
            } else {
                /*!< In complete */
                g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].actual_xfer_len += g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_len;
                g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].xfer_len = 0;
                usbd_event_ep_in_complete_handler(ep->ep_addr, g_rp2040_udc.in_ep[(ep->ep_addr) & 0x0f].actual_xfer_len);
            }
        }
    }
}

/**
 * @brief Find the endpoint configuration for a specified endpoint number and
 * direction and notify it that a transfer has completed.
 *
 * @param ep_num
 * @param in
 */
static void usb_handle_buff_done(uint8_t ep_num, bool in)
{
    uint8_t ep_addr = ep_num | (in ? USB_EP_DIR_IN : 0);
    if (USB_EP_DIR_IS_OUT(ep_addr)) {
        usb_handle_ep_buff_done(&g_rp2040_udc.out_ep[ep_num]);
    } else {
        usb_handle_ep_buff_done(&g_rp2040_udc.in_ep[ep_num]);
    }
}

/**
 * @brief Handle a "buffer status" irq. This means that one or more
 * buffers have been sent / received. Notify each endpoint where this
 * is the case.
 */
static void usb_handle_buff_status()
{
    uint32_t buffers = usb_hw->buf_status;
    uint32_t remaining_buffers = buffers;

    uint32_t bit = 1u;
    for (uint8_t i = 0; remaining_buffers && i < USB_NUM_ENDPOINTS * 2; i++) {
        if (remaining_buffers & bit) {
            /*!< clear this in advance */
            usb_hw_clear->buf_status = bit;
            /*!< IN transfer for even i, OUT transfer for odd i */
            usb_handle_buff_done(i >> 1u, !(i & 1u));
            remaining_buffers &= ~bit;
        }
        bit <<= 1u;
    }
}

void USBD_IRQHandler(void)
{
    uint32_t const status = usb_hw->ints;
    uint32_t handled = 0;

    if (status & USB_INTS_BUFF_STATUS_BITS) {
        handled |= USB_INTS_BUFF_STATUS_BITS;
        usb_handle_buff_status();
    }

    if (status & USB_INTS_SETUP_REQ_BITS) {
        handled |= USB_INTS_SETUP_REQ_BITS;
        memcpy((uint8_t *)&g_rp2040_udc.setup, (uint8_t const *)&usb_dpram->setup_packet, 8);
        /**
         * reset pid to both 1 (data and ack)
         */
        g_rp2040_udc.in_ep[0].next_pid = 1;
        g_rp2040_udc.out_ep[0].next_pid = 1;
        usbd_event_ep0_setup_complete_handler((uint8_t *)&g_rp2040_udc.setup);
        usb_hw_clear->sie_status = USB_SIE_STATUS_SETUP_REC_BITS;
    }

#if FORCE_VBUS_DETECT == 0
    /**
     * Since we force VBUS detect On, device will always think it is connected and
     * couldn't distinguish between disconnect and suspend
     */
    if (status & USB_INTS_DEV_CONN_DIS_BITS) {
        handled |= USB_INTS_DEV_CONN_DIS_BITS;
        if (usb_hw->sie_status & USB_SIE_STATUS_CONNECTED_BITS) {
            /*!< Connected: nothing to do */
        } else {
            /*!< Disconnected */
        }
        usb_hw_clear->sie_status = USB_SIE_STATUS_CONNECTED_BITS;
    }
#endif

    /**
     * SE0 for 2.5 us or more (will last at least 10ms)
     */
    if (status & USB_INTS_BUS_RESET_BITS) {
        handled |= USB_INTS_BUS_RESET_BITS;
        usb_hw->dev_addr_ctrl = 0;

        for (uint8_t i = 0; i < USB_NUM_BIDIR_ENDPOINTS - 1; i++) {
            /*!< Start at ep1 */
            usb_dpram->ep_ctrl[i].in = 0;
            usb_dpram->ep_ctrl[i].out = 0;
        }
        /*!< reclaim buffer space */
        next_buffer_ptr = &usb_dpram->epx_data[0];

        usbd_event_reset_handler();
        usb_hw_clear->sie_status = USB_SIE_STATUS_BUS_RESET_BITS;

#if CHERRYUSB_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX
        /**
         * Only run enumeration walk-around if pull up is enabled
         */
        if (usb_hw->sie_ctrl & USB_SIE_CTRL_PULLUP_EN_BITS)
            rp2040_usb_device_enumeration_fix();
#endif
    }

    /**
     * Note from pico datasheet 4.1.2.6.4 (v1.2)
     * If you enable the suspend interrupt, it is likely you will see a suspend interrupt when
     * the device is first connected but the bus is idle. The bus can be idle for a few ms before
     * the host begins sending start of frame packets. You will also see a suspend interrupt
     * when the device is disconnected if you do not have a VBUS detect circuit connected. This is
     * because without VBUS detection, it is impossible to tell the difference between
     * being disconnected and suspended.
     */
    if (status & USB_INTS_DEV_SUSPEND_BITS) {
        handled |= USB_INTS_DEV_SUSPEND_BITS;
        /*!< Suspend */
        usb_hw_clear->sie_status = USB_SIE_STATUS_SUSPENDED_BITS;
    }

    if (status & USB_INTS_DEV_RESUME_FROM_HOST_BITS) {
        handled |= USB_INTS_DEV_RESUME_FROM_HOST_BITS;
        /*!< Resume */
        usb_hw_clear->sie_status = USB_SIE_STATUS_RESUME_BITS;
    }

    if (status ^ handled) {
        USB_LOG_INFO("Unhandled IRQ 0x%x\n", (uint32_t)(status ^ handled));
    }
}
