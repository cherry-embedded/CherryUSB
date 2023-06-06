/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_UDC_H
#define USBD_UDC_H

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_bus;
struct usbd_udc_driver {
    const char *driver_name;
    int (*udc_init)(struct usbd_bus *bus);
    int (*udc_deinit)(struct usbd_bus *bus);
    int (*udc_set_address)(struct usbd_bus *bus, const uint8_t addr);
    uint8_t (*udc_get_port_speed)(struct usbd_bus *bus);
    int (*udc_ep_open)(struct usbd_bus *bus, const struct usb_endpoint_descriptor *ep_desc);
    int (*udc_ep_close)(struct usbd_bus *bus, const uint8_t ep);
    int (*udc_ep_set_stall)(struct usbd_bus *bus, const uint8_t ep);
    int (*udc_ep_clear_stall)(struct usbd_bus *bus, const uint8_t ep);
    int (*udc_ep_is_stalled)(struct usbd_bus *bus, const uint8_t ep, uint8_t *stalled);
    int (*udc_ep_start_write)(struct usbd_bus *bus, const uint8_t ep, const uint8_t *data, uint32_t data_len);
    int (*udc_ep_start_read)(struct usbd_bus *bus, const uint8_t ep, uint8_t *data, uint32_t data_len);
    void (*udc_irq)(struct usbd_bus *bus);
};

struct usbd_bus {
    uint8_t busid;
    uint8_t endpoints;
    uint32_t reg_base;
    void *udc;
    void *udc_param;
    const struct usbd_udc_driver *driver;
};

void usbd_bus_add_udc(uint8_t busid, uint32_t reg_base, struct usbd_udc_driver *driver, void *param);

/**
 * @brief init device controller registers.
 * @return On success will return 0, and others indicate fail.
 */
int usbd_udc_init(uint8_t busid);

/**
 * @brief deinit device controller registers.
 * @return On success will return 0, and others indicate fail.
 */
int usbd_udc_deinit(uint8_t busid);

/**
 * @brief Set USB device address
 *
 * @param[in] addr Device address
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbd_set_address(uint8_t busid, const uint8_t addr);

/**
 * @brief Get USB device speed
 *
 * @param[in] port port index
 *
 * @return port speed, USB_SPEED_LOW or USB_SPEED_FULL or USB_SPEED_HIGH
 */
uint8_t usbd_get_port_speed(uint8_t busid);

/**
 * @brief configure and enable endpoint.
 *
 * @param [in]  ep_cfg Endpoint config.
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_open(uint8_t busid, const struct usb_endpoint_descriptor *ep_desc);

/**
 * @brief Disable the selected endpoint
 *
 * @param[in] ep Endpoint address
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_close(uint8_t busid, const uint8_t ep);

/**
 * @brief Set stall condition for the selected endpoint
 *
 * @param[in] ep Endpoint address
 *
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_set_stall(uint8_t busid, const uint8_t ep);

/**
 * @brief Clear stall condition for the selected endpoint
 *
 * @param[in] ep Endpoint address corresponding to the one
 *               listed in the device configuration table
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_clear_stall(uint8_t busid, const uint8_t ep);

/**
 * @brief Check if the selected endpoint is stalled
 *
 * @param[in]  ep       Endpoint address
 *
 * @param[out] stalled  Endpoint stall status
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbd_ep_is_stalled(uint8_t busid, const uint8_t ep, uint8_t *stalled);

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
int usbd_ep_start_write(uint8_t busid, const uint8_t ep, const uint8_t *data, uint32_t data_len);

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
int usbd_ep_start_read(uint8_t busid, const uint8_t ep, uint8_t *data, uint32_t data_len);

/**
 * @brief Usb irq entrance.
 */
void usbd_irq(uint8_t busid);

/* usb dcd irq callback (only port uses )*/

/**
 * @brief Usb connect irq callback.
 */
void usbd_event_connect_handler(uint8_t busid);

/**
 * @brief Usb disconnect irq callback.
 */
void usbd_event_disconnect_handler(uint8_t busid);

/**
 * @brief Usb resume irq callback.
 */
void usbd_event_resume_handler(uint8_t busid);

/**
 * @brief Usb suspend irq callback.
 */
void usbd_event_suspend_handler(uint8_t busid);

/**
 * @brief Usb reset irq callback.
 */
void usbd_event_reset_handler(uint8_t busid);

/**
 * @brief Usb setup packet recv irq callback.
 * @param[in]  psetup  setup packet.
 */
void usbd_event_ep0_setup_complete_handler(uint8_t busid, uint8_t *psetup);

/**
 * @brief In ep transfer complete irq callback.
 * @param[in]  ep        Endpoint address corresponding to the one
 *                       listed in the device configuration table
 * @param[in]  nbytes    How many nbytes have transferred.
 */
void usbd_event_ep_in_complete_handler(uint8_t busid, uint8_t ep, uint32_t nbytes);

/**
 * @brief Out ep transfer complete irq callback.
 * @param[in]  ep        Endpoint address corresponding to the one
 *                       listed in the device configuration table
 * @param[in]  nbytes    How many nbytes have transferred.
 */
void usbd_event_ep_out_complete_handler(uint8_t busid, uint8_t ep, uint32_t nbytes);

void usbd_udc_low_level_init(uint8_t busid);
void usbd_udc_low_level_deinit(uint8_t busid);

#ifdef __cplusplus
}
#endif

#endif /* USBD_UDC_H */