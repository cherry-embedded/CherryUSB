/**
 * @file usbd_rndis.c
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
#include "usbd_core.h"
#include "usbd_rndis.h"
#include "rndis_protocol.h"

#define RNDIS_INQUIRY_PUT(src, len)   (memcpy(infomation_buffer, src, len))
#define RNDIS_INQUIRY_PUT_LE32(value) (*(uint32_t *)infomation_buffer = (value))

/* Device data structure */
struct usbd_rndis_cfg_priv {
    uint32_t drv_version;
    uint32_t media_status;
    uint32_t speed;
    uint32_t mtu;
    uint32_t net_filter;
    usb_eth_stat_t eth_state;
    rndis_state_t init_state;
    uint8_t mac[6];
    uint32_t vendor_id;
    uint8_t *vendor_desc;
} usbd_rndis_cfg = { .drv_version = 0x0001,
                .media_status = NDIS_MEDIA_STATE_DISCONNECTED,
                .mtu = RNDIS_MTU,
                .speed = 0,
                .init_state = rndis_uninitialized,
                .mac = { 0x00, 0x00, 0x5E, 0x00, 0x53, 0x01 },
                .vendor_id = 0xffffffff,
                .vendor_desc = "CherryUSB" };

/* RNDIS options list */
const uint32_t oid_supported_list[] = {
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_MAC_OPTIONS
};

static uint8_t rndis_encapsulated_resp_buffer[CONFIG_RNDIS_RESP_BUFFER_SIZE];

static int rndis_encapsulated_cmd_handler(uint8_t *data, uint32_t len);
// static int rndis_encapsulated_resp_handler(uint8_t *data, uint32_t *len);
static void rndis_notify_rsp(void);
/**
 * @brief Handler called for Class requests not handled by the USB stack.
 *
 * @param setup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
static int rndis_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    switch (setup->bRequest) {
        case CDC_REQUEST_SEND_ENCAPSULATED_COMMAND:
            rndis_encapsulated_cmd_handler(data, len);
            break;
        case CDC_REQUEST_GET_ENCAPSULATED_RESPONSE:
            *data = rndis_encapsulated_resp_buffer;
            *len = ((rndis_generic_msg_t *)rndis_encapsulated_resp_buffer)->MessageLength;
            break;

        default:
            return -1;
    }
}

static void rndis_notify_rsp(void)
{
    uint8_t notify_buf[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    usbd_ep_write(0x81, notify_buf, 8, NULL);
}

static int rndis_init_cmd_handler(uint8_t *data, uint32_t len);
static int rndis_halt_cmd_handler(uint8_t *data, uint32_t len);
static int rndis_query_cmd_handler(uint8_t *data, uint32_t len);
static int rndis_set_cmd_handler(uint8_t *data, uint32_t len);
static int rndis_reset_cmd_handler(uint8_t *data, uint32_t len);
static int rndis_keepalive_cmd_handler(uint8_t *data, uint32_t len);

static int rndis_encapsulated_cmd_handler(uint8_t *data, uint32_t len)
{
    switch (((rndis_generic_msg_t *)data)->MessageType) {
        case REMOTE_NDIS_INITIALIZE_MSG:
            rndis_init_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_HALT_MSG:
            rndis_halt_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_QUERY_MSG:
            rndis_query_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_SET_MSG:
            rndis_set_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_RESET_MSG:
            rndis_reset_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_KEEPALIVE_MSG:
            rndis_keepalive_cmd_handler(data, len);
            break;

        default:
            break;
    }
}

static int rndis_init_cmd_handler(uint8_t *data, uint32_t len)
{
    rndis_initialize_msg_t *cmd = (rndis_initialize_msg_t *)data;
    rndis_initialize_cmplt_t *resp;

    resp = ((rndis_initialize_cmplt_t *)rndis_encapsulated_resp_buffer);
    resp->RequestId = cmd->RequestId;
    resp->MessageType = REMOTE_NDIS_INITIALIZE_CMPLT;
    resp->MessageLength = sizeof(rndis_initialize_cmplt_t);
    resp->MajorVersion = RNDIS_MAJOR_VERSION;
    resp->MinorVersion = RNDIS_MINOR_VERSION;
    resp->Status = RNDIS_STATUS_SUCCESS;
    resp->DeviceFlags = RNDIS_DF_CONNECTIONLESS;
    resp->Medium = RNDIS_MEDIUM_802_3;
    resp->MaxPacketsPerTransfer = 1;
    resp->MaxTransferSize = usbd_rndis_cfg.mtu + ETH_HEADER_SIZE + sizeof(rndis_data_packet_t);
    resp->PacketAlignmentFactor = 0;
    resp->AfListOffset = 0;
    resp->AfListSize = 0;

    usbd_rndis_cfg.init_state = rndis_initialized;

    rndis_notify_rsp();
    return 0;
}

static int rndis_halt_cmd_handler(uint8_t *data, uint32_t len)
{
    usbd_rndis_cfg.init_state = rndis_uninitialized;

    return 0;
}

static int rndis_query_cmd_handler(uint8_t *data, uint32_t len)
{
    rndis_query_msg_t *cmd = (rndis_initialize_msg_t *)data;
    rndis_query_cmplt_t *resp;
    uint8_t *infomation_buffer;
    uint32_t infomation_len = 0;

    resp = ((rndis_initialize_cmplt_t *)rndis_encapsulated_resp_buffer);
    resp->Status = RNDIS_STATUS_SUCCESS;
    resp->RequestId = cmd->RequestId;
    resp->MessageType = REMOTE_NDIS_QUERY_CMPLT;
    resp->InformationBufferOffset = 16;

    infomation_buffer = (uint8_t *)resp + sizeof(rndis_initialize_cmplt_t);

    switch (cmd->Oid) {
        case OID_GEN_SUPPORTED_LIST:
            RNDIS_INQUIRY_PUT(oid_supported_list, sizeof(oid_supported_list));
            infomation_len = sizeof(oid_supported_list);
            break;
        case OID_GEN_VENDOR_DRIVER_VERSION:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.drv_version);
            infomation_len = 4;
            break;
        case OID_802_3_CURRENT_ADDRESS:
            RNDIS_INQUIRY_PUT(usbd_rndis_cfg.mac, 6);
            infomation_len = 6;
            break;
        case OID_802_3_PERMANENT_ADDRESS:
            RNDIS_INQUIRY_PUT(usbd_rndis_cfg.mac, 6);
            infomation_len = 6;
            break;
        case OID_GEN_MEDIA_SUPPORTED:
            RNDIS_INQUIRY_PUT_LE32(NDIS_MEDIUM_802_3);
            infomation_len = 4;
            break;
        case OID_GEN_MEDIA_IN_USE:
            RNDIS_INQUIRY_PUT_LE32(NDIS_MEDIUM_802_3);
            infomation_len = 4;
            break;
        case OID_GEN_PHYSICAL_MEDIUM:
            RNDIS_INQUIRY_PUT_LE32(NDIS_MEDIUM_802_3);
            infomation_len = 4;
            break;
        case OID_GEN_HARDWARE_STATUS:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_GEN_LINK_SPEED:
            RNDIS_INQUIRY_PUT_LE32(RNDIS_LINK_SPEED / 100);
            infomation_len = 4;
            break;
        case OID_GEN_VENDOR_ID:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.vendor_id);
            infomation_len = 4;
            break;
        case OID_GEN_VENDOR_DESCRIPTION:
            RNDIS_INQUIRY_PUT(usbd_rndis_cfg.vendor_desc, strlen(usbd_rndis_cfg.vendor_desc) + 1);
            infomation_len = (strlen(usbd_rndis_cfg.vendor_desc) + 1);
            break;
        case OID_GEN_CURRENT_PACKET_FILTER:
            RNDIS_INQUIRY_PUT_LE32(0x00FFFFFF);
            infomation_len = 4;
            break;
        case OID_GEN_MAXIMUM_FRAME_SIZE:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.mtu);
            infomation_len = 4;
            break;
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.mtu + ETH_HEADER_SIZE + sizeof(rndis_data_packet_t));
            infomation_len = 4;
            break;
        case OID_GEN_MEDIA_CONNECT_STATUS:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.media_status);
            infomation_len = 4;
            break;
        case OID_GEN_RNDIS_CONFIG_PARAMETER:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_802_3_MAXIMUM_LIST_SIZE:
            RNDIS_INQUIRY_PUT_LE32(1); /* one address */
            infomation_len = 4;
            break;
        case OID_802_3_MULTICAST_LIST:
            RNDIS_INQUIRY_PUT_LE32(0xE0000000); /* 224.0.0.0 */
            infomation_len = 4;
            break;
        case OID_802_3_MAC_OPTIONS:
            infomation_len = 0;
            break;
        case OID_GEN_MAC_OPTIONS:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_802_3_RCV_ERROR_ALIGNMENT:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_802_3_XMIT_ONE_COLLISION:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_802_3_XMIT_MORE_COLLISIONS:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_GEN_XMIT_OK:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.eth_state.txok);
            infomation_len = 4;
            break;
        case OID_GEN_RCV_OK:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.eth_state.rxok);
            infomation_len = 4;
            break;
        case OID_GEN_RCV_ERROR:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.eth_state.rxbad);
            infomation_len = 4;
            break;
        case OID_GEN_XMIT_ERROR:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.eth_state.txbad);
            infomation_len = 4;
            break;
        case OID_GEN_RCV_NO_BUFFER:
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        default:
            resp->Status = RNDIS_STATUS_NOT_SUPPORTED;
            USB_LOG_WRN("Unhandled query for Object ID 0x%x\r\n", cmd->Oid);
            break;
    }

    resp->MessageLength = sizeof(rndis_query_cmplt_t) + infomation_len;
    resp->InformationBufferLength = infomation_len;

    rndis_notify_rsp();
    return 0;
}

static int rndis_set_cmd_handler(uint8_t *data, uint32_t len)
{
    rndis_set_msg_t *cmd = (rndis_set_msg_t *)data;
    rndis_set_cmplt_t *resp;
    rndis_config_parameter_t *param;

    resp = ((rndis_set_cmplt_t *)rndis_encapsulated_resp_buffer);
    resp->RequestId = cmd->RequestId;
    resp->MessageType = REMOTE_NDIS_SET_CMPLT;
    resp->MessageLength = sizeof(rndis_set_cmplt_t);
    resp->Status = RNDIS_STATUS_SUCCESS;

    switch (cmd->Oid) {
        case OID_GEN_RNDIS_CONFIG_PARAMETER:
            break;
        case OID_GEN_CURRENT_PACKET_FILTER:
            if (cmd->InformationBufferLength < sizeof(usbd_rndis_cfg.net_filter)) {
                resp->Status = RNDIS_STATUS_INVALID_DATA;
            } else {
                /* Parameter starts at offset buf_offset of the req_id field */
                param = (rndis_config_parameter_t *)((uint8_t *)&cmd->RequestId + cmd->InformationBufferOffset);

                //usbd_rndis_cfg.net_filter = param->ParameterNameOffset;
            }
            break;
        case OID_GEN_CURRENT_LOOKAHEAD:
            break;
        case OID_GEN_PROTOCOL_OPTIONS:
            break;
        case OID_802_3_MULTICAST_LIST:
            break;
        case OID_PNP_ADD_WAKE_UP_PATTERN:
        case OID_PNP_REMOVE_WAKE_UP_PATTERN:
        case OID_PNP_ENABLE_WAKE_UP:
        default:
            resp->Status = RNDIS_STATUS_FAILURE;
            break;
    }

    rndis_notify_rsp();

    return 0;
}

static int rndis_reset_cmd_handler(uint8_t *data, uint32_t len)
{
    rndis_reset_msg_t *cmd = (rndis_reset_msg_t *)data;
    rndis_reset_cmplt_t *resp;

    resp = ((rndis_reset_cmplt_t *)rndis_encapsulated_resp_buffer);
    resp->MessageType = REMOTE_NDIS_RESET_CMPLT;
    resp->MessageLength = sizeof(rndis_reset_cmplt_t);
    resp->Status = RNDIS_STATUS_SUCCESS;
    resp->AddressingReset = 1;

    rndis_notify_rsp();

    return 0;
}

static int rndis_keepalive_cmd_handler(uint8_t *data, uint32_t len)
{
    rndis_keepalive_msg_t *cmd = (rndis_keepalive_msg_t *)data;
    rndis_keepalive_cmplt_t *resp;

    resp = ((rndis_keepalive_cmplt_t *)rndis_encapsulated_resp_buffer);
    resp->RequestId = cmd->RequestId;
    resp->MessageType = REMOTE_NDIS_KEEPALIVE_CMPLT;
    resp->MessageLength = sizeof(rndis_keepalive_cmplt_t);
    resp->Status = RNDIS_STATUS_SUCCESS;

    rndis_notify_rsp();

    return 0;
}

static void rndis_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:

            break;

        default:
            break;
    }
}

void usbd_rndis_add_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = rndis_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = rndis_notify_handler;
    usbd_class_add_interface(devclass, intf);
}
