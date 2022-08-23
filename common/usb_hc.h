/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_HC_H
#define USB_HC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*usbh_asynch_callback_t)(void *arg, int nbytes);
typedef void *usbh_epinfo_t;

/**
 * @brief USB Endpoint Configuration.
 *
 * Structure containing the USB endpoint configuration.
 */
struct usbh_endpoint_cfg {
    struct usbh_hubport *hport;
    uint8_t ep_addr;     /* Endpoint addr with direction */
    uint8_t ep_type;     /* Endpoint type */
    uint16_t ep_mps;     /* Endpoint max packet size */
    uint8_t ep_interval; /* Endpoint interval */
};

/**
 * @brief usb host software init, used for global reset.
 *
 * @return On success will return 0, and others indicate fail.
 */
int usb_hc_sw_init(void);

/**
 * @brief usb host controller hardware init.
 *
 * @return On success will return 0, and others indicate fail.
 */
int usb_hc_hw_init(void);

/**
 * @brief get port connect status
 *
 * @param port
 * @return On success will return 0, and others indicate fail.
 */
bool usbh_get_port_connect_status(const uint8_t port);

/**
 * @brief reset roothub port
 *
 * @param port port index
 * @return On success will return 0, and others indicate fail.
 */
int usbh_reset_port(const uint8_t port);

/**
 * @brief get roothub port speed
 *
 * @param port port index
 * @return return 1 means USB_SPEED_LOW, 2 means USB_SPEED_FULL and 3 means USB_SPEED_HIGH.
 */
uint8_t usbh_get_port_speed(const uint8_t port);

/**
 * @brief reconfig control endpoint.
 *
 * @param ep A memory location provided by the caller.
 * @param dev_addr device address.
 * @param ep_mps control endpoint max packet size.
 * @param speed port speed
 * @return On success will return 0, and others indicate fail.
 */
int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed);

/**
 * @brief Allocate and config endpoint
 *
 * @param ep A memory location provided by the caller in which to save the allocated endpoint info.
 * @param ep_cfg Describes the endpoint info to be allocated.
 * @return  On success will return 0, and others indicate fail.
 */
int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg);

/**
 * @brief Free a memory in which saves endpoint info.
 *
 * @param ep A memory location provided by the caller in which to free the allocated endpoint info.
 * @return On success will return 0, and others indicate fail.
 */
int usbh_ep_free(usbh_epinfo_t ep);

/**
 * @brief Perform a control transfer.
 * This is a blocking method; this method will not return until the transfer has completed.
 *
 * @param ep The control endpoint to send/receive the control request.
 * @param setup Setup packet to be sent.
 * @param buffer buffer used for sending the request and for returning any responses.  This buffer must be large enough to hold the length value
 *  in the request description.
 * @return On success will return 0, and others indicate fail.
 */
int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer);

/**
 * @brief  Process a request to handle a transfer descriptor.  This method will
 * enqueue the transfer request and wait for it to complete.  Only one transfer may be queued;
 * This is a blocking method; this method will not return until the transfer has completed.
 *
 * @param ep The IN or OUT endpoint descriptor for the device endpoint on which to perform the transfer.
 * @param buffer A buffer containing the data to be sent (OUT endpoint) or received (IN endpoint).
 * @param buflen The length of the data to be sent or received.
 * @param timeout Timeout for transfer, unit is ms.
 * @return On success, a non-negative value is returned that indicates the number
 *   of bytes successfully transferred.  On a failure, a negated errno value
 *   is returned that indicates the nature of the failure:
 *
 *     -EAGAIN - If devices NAKs the transfer (or NYET or other error where
 *              it may be appropriate to restart the entire transaction).
 *     -EPERM  - If the endpoint stalls
 *     -EIO    - On a TX or data toggle error
 *     -EPIPE  - Overrun errors
 *     -ETIMEDOUT  - Sem wait timeout
 *
 */
int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

/**
 * @brief  Process a request to handle a transfer descriptor.  This method will
 * enqueue the transfer request and wait for it to complete.  Only one transfer may be queued;
 * This is a blocking method; this method will not return until the transfer has completed.
 *
 * @param ep The IN or OUT endpoint descriptor for the device endpoint on which to perform the transfer.
 * @param buffer A buffer containing the data to be sent (OUT endpoint) or received (IN endpoint).
 * @param buflen The length of the data to be sent or received.
 * @param timeout Timeout for transfer, unit is ms.
 * @return On success, a non-negative value is returned that indicates the number
 *   of bytes successfully transferred.  On a failure, a negated errno value
 *   is returned that indicates the nature of the failure:
 *
 *     -EAGAIN - If devices NAKs the transfer (or NYET or other error where
 *              it may be appropriate to restart the entire transaction).
 *     -EPERM  - If the endpoint stalls
 *     -EIO    - On a TX or data toggle error
 *     -EPIPE  - Overrun errors
 *     -ETIMEDOUT  - Sem wait timeout
 *
 */
int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout);

/**
 * @brief Process a request to handle a transfer asynchronously.  This method
 * will enqueue the transfer request and return immediately.  Only one transfer may be queued on a given endpoint
 * When the transfer completes, the callback will be invoked with the provided argument.
 *
 * This method is useful for receiving interrupt transfers which may come infrequently.
 *
 * @param ep The IN or OUT endpoint descriptor for the device endpoint on which to perform the transfer.
 * @param buffer A buffer containing the data to be sent (OUT endpoint) or received (IN endpoint).
 * @param buflen The length of the data to be sent or received.
 * @param callback This function will be called when the transfer completes.
 * @param arg The arbitrary parameter that will be passed to the callback function when the transfer completes.
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg);

/**
 * @brief Process a request to handle a transfer asynchronously.  This method
 * will enqueue the transfer request and return immediately.  Only one transfer may be queued on a given endpoint
 * When the transfer completes, the callback will be invoked with the provided argument.
 *
 * This method is useful for receiving interrupt transfers which may come infrequently.
 *
 * @param ep The IN or OUT endpoint descriptor for the device endpoint on which to perform the transfer.
 * @param buffer A buffer containing the data to be sent (OUT endpoint) or received (IN endpoint).
 * @param buflen The length of the data to be sent or received.
 * @param callback This function will be called when the transfer completes.
 * @param arg The arbitrary parameter that will be passed to the callback function when the transfer completes.
 *
 * @return On success will return 0, and others indicate fail.
 */
int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg);

/**
 * @brief Cancel any pending syncrhonous or asynchronous transfer on an endpoint.
 *
 * @param ep The IN or OUT endpoint descriptor for the device endpoint on which to cancel.
 * @return On success will return 0, and others indicate fail.
 */
int usb_ep_cancel(usbh_epinfo_t ep);

/* usb hcd irq callback */

void usbh_event_notify_handler(uint8_t event, uint8_t rhport);

#ifdef __cplusplus
}
#endif

#endif /* USB_HC_H */
