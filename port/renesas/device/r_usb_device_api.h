/*
 * Copyright (c) 2020 - 2024 Renesas Electronics Corporation and/or its affiliates
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef R_USB_DEVICE_API_H
#define R_USB_DEVICE_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/

/* Includes board and MCU related header files. */
#include "bsp_api.h"

/* Common macro for FSP header files. There is also a corresponding FSP_FOOTER macro at the end of this file. */
FSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/** USB setup packet */
typedef struct __PACKED st_usbd_setup
{
    uint16_t request_type;
    uint16_t request_value;
    uint16_t request_index;
    uint16_t request_length;
} usbd_setup_t;

/** USB Endpoint Descriptor */
typedef struct __PACKED st_usbd_desc_endpoint
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    union
    {
        uint8_t bmAttributes;
        struct
        {
            uint8_t xfer  : 2;
            uint8_t sync  : 2;
            uint8_t usage : 2;
            uint8_t       : 2;
        } Attributes;
    };

    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usbd_desc_endpoint_t;

/** USB speed */
typedef enum e_usbd_speed
{
    USBD_SPEED_LS = 0,
    USBD_SPEED_FS,
    USBD_SPEED_HS,
    USBD_SPEED_INVALID,
} usbd_speed_t;

/** USB event code */
typedef enum e_usbd_event_id
{
    R_USBD_EVENT_INVALID = 0,
    R_USBD_EVENT_BUS_RESET,
    R_USBD_EVENT_VBUS_RDY,
    R_USBD_EVENT_VBUS_REMOVED,
    R_USBD_EVENT_SOF,
    R_USBD_EVENT_SUSPEND,
    R_USBD_EVENT_RESUME,
    R_USBD_EVENT_SETUP_RECEIVED,
    R_USBD_EVENT_XFER_COMPLETE,
} usbd_event_id_t;

/** USB transfer result code */
typedef enum e_usbd_xfer_result
{
    USBD_XFER_RESULT_SUCCESS = 0,
    USBD_XFER_RESULT_FAILED,
    USBD_XFER_RESULT_STALLED,
    USBD_XFER_RESULT_TIMEOUT,
    USBD_XFER_RESULT_INVALID
} usbd_xfer_result_t;

/** USB bus reset event input argument */
typedef struct st_usbd_bus_reset_evt
{
    usbd_speed_t speed;
} usbd_bus_reset_evt_t;

/** USB SOF detection event input argument */
typedef struct st_usbd_sof_evt
{
    uint32_t frame_count;
} usbd_sof_evt_t;

/** USB transfer complete event input argument */
typedef struct st_usbd_xfer_complete
{
    usbd_xfer_result_t result;
    uint8_t            ep_addr;
    uint32_t           len;
} usbd_xfer_complete_t;

typedef struct st_usb_event
{
    usbd_event_id_t event_id;
    union
    {
        usbd_bus_reset_evt_t bus_reset;
        usbd_sof_evt_t       sof;
        usbd_setup_t         setup_received;
        usbd_xfer_complete_t xfer_complete;
    };
} usbd_event_t;

typedef struct st_usbd_callback_arg
{
    uint32_t     module_number;
    usbd_event_t event;
    void const * p_context;
} usbd_callback_arg_t;

/** USB configuration */
typedef struct st_usbd_cfg
{
    uint32_t     module_number;
    usbd_speed_t usb_speed;
    IRQn_Type    irq;
    IRQn_Type    irq_r;
    IRQn_Type    irq_d0;
    IRQn_Type    irq_d1;
    IRQn_Type    hs_irq;
    IRQn_Type    hsirq_d0;
    IRQn_Type    hsirq_d1;
    uint8_t      ipl;
    uint8_t      ipl_r;
    uint8_t      ipl_d0;
    uint8_t      ipl_d1;
    uint8_t      hsipl;
    uint8_t      hsipl_d0;
    uint8_t      hsipl_d1;
    void (* p_callback)(usbd_callback_arg_t * p_args);
    void const * p_context;
    void const * p_extend;
} usbd_cfg_t;

typedef void usbd_ctrl_t;

#endif /* R_USB_DEVICE_API_H */