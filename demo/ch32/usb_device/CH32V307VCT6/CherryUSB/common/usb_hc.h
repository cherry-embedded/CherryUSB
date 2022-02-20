/**
 * @file usb_hc.h
 * @brief
 *
 * Copyright (c) 2022 sakumisu
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#ifndef _USB_HC_H
#define _USB_HC_H

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
    /** The number associated with the EP in the device
     *  configuration structure
     *       IN  EP = 0x80 | \<endpoint number\>
     *       OUT EP = 0x00 | \<endpoint number\>
     */
    uint8_t ep_addr;
    /** Endpoint Transfer Type.
     * May be Bulk, Interrupt, Control or Isochronous
     */
    uint8_t ep_type;
    uint8_t ep_interval;
    /** Endpoint max packet size */
    uint16_t ep_mps;
};

/**
 * @brief USB Host Core Layer API
 * @defgroup _usb_host_core_api USB Host Core API
 * @{
 */

/**
 * @brief usb host controller hardware init.
 *
 * @return int
 */
int usb_hc_init(void);

/**
 * @brief reset roothub port
 *
 * @param port port index
 * @return int
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
 * @return On success, a non-negative value is returned that indicates the number
 *   of bytes successfully transferred.  On a failure, a negated errno value
 *   is returned that indicates the nature of the failure:
 *
 *     EAGAIN - If devices NAKs the transfer (or NYET or other error where
 *              it may be appropriate to restart the entire transaction).
 *     EPERM  - If the endpoint stalls
 *     EIO    - On a TX or data toggle error
 *     EPIPE  - Overrun errors
 *
 */
int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen);

/**
 * @brief  Process a request to handle a transfer descriptor.  This method will
 * enqueue the transfer request and wait for it to complete.  Only one transfer may be queued;
 * This is a blocking method; this method will not return until the transfer has completed.
 *
 * @param ep The IN or OUT endpoint descriptor for the device endpoint on which to perform the transfer.
 * @param buffer A buffer containing the data to be sent (OUT endpoint) or received (IN endpoint).
 * @param buflen The length of the data to be sent or received.
 * @return On success, a non-negative value is returned that indicates the number
 *   of bytes successfully transferred.  On a failure, a negated errno value
 *   is returned that indicates the nature of the failure:
 *
 *     EAGAIN - If devices NAKs the transfer (or NYET or other error where
 *              it may be appropriate to restart the entire transaction).
 *     EPERM  - If the endpoint stalls
 *     EIO    - On a TX or data toggle error
 *     EPIPE  - Overrun errors
 *
 */
int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen);

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

#ifdef __cplusplus
}
#endif

#endif
