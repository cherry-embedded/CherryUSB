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

typedef void (*usbh_complete_callback_t)(void *arg, int nbytes);
typedef void *usbh_pipe_t;

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
    uint8_t mult;        /* Endpoint additional transcation */
};

/**
 * @brief USB Iso Configuration.
 *
 * Structure containing the USB Iso configuration.
 */
struct usbh_iso_frame_packet {
    uint8_t *transfer_buffer;
    uint32_t transfer_buffer_length;
    uint32_t actual_length;
    int errorcode;
};

/**
 * @brief USB Urb Configuration.
 *
 * Structure containing the USB Urb configuration.
 */
struct usbh_urb {
    usbh_pipe_t pipe;
    struct usb_setup_packet *setup;
    uint8_t *transfer_buffer;
    uint32_t transfer_buffer_length;
    int transfer_flags;
    uint32_t actual_length;
    uint32_t timeout;
    int errorcode;
    uint32_t num_of_iso_packets;
    uint32_t start_frame;
    usbh_complete_callback_t complete;
    void *arg;
    struct usbh_iso_frame_packet iso_packet[0];
};

/**
 * @brief usb host controller hardware init.
 *
 * @return On success will return 0, and others indicate fail.
 */
int usb_hc_init(void);

/**
 * @brief Get frame number.
 *
 * @return frame number.
 */
uint16_t usbh_get_frame_number(void);
/**
 * @brief control roothub.
 *
 * @param setup setup request buffer.
 * @param buf buf for reading response or write data.
 * @return On success will return 0, and others indicate fail.
 */
int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf);

/**
 * @brief reconfig control endpoint pipe.
 *
 * @param pipe A memory allocated for pipe.
 * @param dev_addr device address.
 * @param ep_mps control endpoint max packet size.
 * @param speed port speed
 * @return On success will return 0, and others indicate fail.
 */
int usbh_ep0_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed);

/**
 * @brief Allocate pipe for endpoint
 *
 * @param pipe A memory location provided by the caller in which to save the allocated pipe.
 * @param ep_cfg Describes the endpoint info to be allocated.
 * @return  On success will return 0, and others indicate fail.
 */
int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg);

/**
 * @brief Free a pipe in which saves endpoint info.
 *
 * @param pipe A memory location provided by the caller in which to free the allocated endpoint info.
 * @return On success will return 0, and others indicate fail.
 */
int usbh_pipe_free(usbh_pipe_t pipe);

/**
 * @brief Submit a usb transfer request to an endpoint.
 *
 * If timeout is not zero, this function will be in poll transfer mode,
 * otherwise will be in async transfer mode.
 *
 * @param urb Usb request block.
 * @return  On success will return 0, and others indicate fail.
 */
int usbh_submit_urb(struct usbh_urb *urb);

/**
 * @brief Cancel a transfer request.
 *
 * This function will call When calls usbh_submit_urb and return -ETIMEOUT or -ESHUTDOWN.
 *
 * @param urb Usb request block.
 * @return  On success will return 0, and others indicate fail.
 */
int usbh_kill_urb(struct usbh_urb *urb);

#ifdef __cplusplus
}
#endif

#endif /* USB_HC_H */
