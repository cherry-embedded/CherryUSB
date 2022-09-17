/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
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
    uint8_t int_ep;
    uint8_t mac[6];
    uint32_t vendor_id;
    uint8_t *vendor_desc;
} usbd_rndis_cfg = { .drv_version = 0x0001,
                     .media_status = NDIS_MEDIA_STATE_DISCONNECTED,
                     .mtu = CONFIG_USBDEV_RNDIS_MTU,
                     .speed = RNDIS_LINK_SPEED,
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

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t rndis_encapsulated_resp_buffer[CONFIG_USBDEV_RNDIS_RESP_BUFFER_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t NOTIFY_RESPONSE_AVAILABLE[8] = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static int rndis_encapsulated_cmd_handler(uint8_t *data, uint32_t len);

static void rndis_notify_rsp(void)
{
    usbd_ep_start_write(usbd_rndis_cfg.int_ep, NOTIFY_RESPONSE_AVAILABLE, 8);
}

static int rndis_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
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
            return rndis_init_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_HALT_MSG:
            return rndis_halt_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_QUERY_MSG:
            return rndis_query_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_SET_MSG:
            return rndis_set_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_RESET_MSG:
            return rndis_reset_cmd_handler(data, len);
            break;
        case REMOTE_NDIS_KEEPALIVE_MSG:
            return rndis_keepalive_cmd_handler(data, len);
            break;

        default:
            break;
    }
    return -1;
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
    rndis_halt_msg_t *resp;

    resp = ((rndis_halt_msg_t *)rndis_encapsulated_resp_buffer);
    resp->MessageLength = 0;

    usbd_rndis_cfg.init_state = rndis_uninitialized;

    return 0;
}

static int rndis_query_cmd_handler(uint8_t *data, uint32_t len)
{
    rndis_query_msg_t *cmd = (rndis_query_msg_t *)data;
    rndis_query_cmplt_t *resp;
    uint8_t *infomation_buffer;
    uint32_t infomation_len = 0;

    resp = ((rndis_query_cmplt_t *)rndis_encapsulated_resp_buffer);
    resp->MessageType = REMOTE_NDIS_QUERY_CMPLT;
    resp->RequestId = cmd->RequestId;
    resp->InformationBufferOffset = sizeof(rndis_query_cmplt_t) - sizeof(rndis_generic_msg_t);
    resp->Status = RNDIS_STATUS_SUCCESS;

    infomation_buffer = (uint8_t *)resp + sizeof(rndis_query_cmplt_t);

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
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.net_filter);
            infomation_len = 4;
            break;
        case OID_GEN_MAXIMUM_FRAME_SIZE:
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.mtu);
            infomation_len = 4;
            break;
        case OID_GEN_MAXIMUM_TOTAL_SIZE:
        case OID_GEN_TRANSMIT_BLOCK_SIZE:
        case OID_GEN_RECEIVE_BLOCK_SIZE:
            //RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.mtu + ETH_HEADER_SIZE + sizeof(rndis_data_packet_t));
            RNDIS_INQUIRY_PUT_LE32(usbd_rndis_cfg.mtu + ETH_HEADER_SIZE);
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
            //RNDIS_INQUIRY_PUT_LE32(0xE0000000); /* 224.0.0.0 */
            resp->Status = RNDIS_STATUS_NOT_SUPPORTED;
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
            break;
        case OID_802_3_MAC_OPTIONS:
            // infomation_len = 0;
            resp->Status = RNDIS_STATUS_NOT_SUPPORTED;
            RNDIS_INQUIRY_PUT_LE32(0);
            infomation_len = 4;
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
            resp->Status = RNDIS_STATUS_FAILURE;
            infomation_len = 0;
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
            param = (rndis_config_parameter_t *)((uint8_t *)&(cmd->RequestId) + cmd->InformationBufferOffset);
            USB_LOG_WRN("RNDIS cfg param: NameOfs=%d, NameLen=%d, ValueOfs=%d, ValueLen=%d\r\n",
                        param->ParameterNameOffset, param->ParameterNameLength,
                        param->ParameterValueOffset, param->ParameterValueLength);
            break;
        case OID_GEN_CURRENT_PACKET_FILTER:
            if (cmd->InformationBufferLength < sizeof(usbd_rndis_cfg.net_filter)) {
                USB_LOG_WRN("PACKET_FILTER!\r\n");
                resp->Status = RNDIS_STATUS_INVALID_DATA;
            } else {
                uint32_t *filter;
                /* Parameter starts at offset buf_offset of the req_id field */
                filter = (uint32_t *)((uint8_t *)&(cmd->RequestId) + cmd->InformationBufferOffset);

                //usbd_rndis_cfg.net_filter = param->ParameterNameOffset;
                usbd_rndis_cfg.net_filter = *(uint32_t *)filter;
                if (usbd_rndis_cfg.net_filter) {
                    usbd_rndis_cfg.init_state = rndis_data_initialized;
                } else {
                    usbd_rndis_cfg.init_state = rndis_initialized;
                }
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
            USB_LOG_WRN("Unhandled query for Object ID 0x%x\r\n", cmd->Oid);
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

    usbd_rndis_cfg.init_state = rndis_uninitialized;

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

struct usbd_interface *usbd_rndis_alloc_intf(uint8_t int_ep, uint8_t mac[6], uint32_t vendor_id, uint8_t *vendor_desc)
{
    struct usbd_interface *intf = (struct usbd_interface *)usb_malloc(sizeof(struct usbd_interface));
    if (intf == NULL) {
        USB_LOG_ERR("no mem to alloc intf\r\n");
        return NULL;
    }

    usbd_rndis_cfg.int_ep = int_ep;
    memcpy(usbd_rndis_cfg.mac, mac, 6);
    usbd_rndis_cfg.vendor_id = vendor_id;
    usbd_rndis_cfg.vendor_desc = vendor_desc;

    intf->class_interface_handler = rndis_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = rndis_notify_handler;

    return intf;
}
