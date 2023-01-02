/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_rndis.h"
#include "rndis_protocol.h"

#define DEV_FORMAT "/dev/rndis"

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_rndis_buf[1024];

static int usbh_rndis_init_msg_transfer(struct usbh_rndis *rndis_class)
{
    struct usb_setup_packet *setup = &rndis_class->hport->setup;
    int ret = 0;
    rndis_initialize_msg_t *cmd;
    rndis_initialize_cmplt_t *resp;

    cmd = (rndis_initialize_msg_t *)g_rndis_buf;

    cmd->MessageType = REMOTE_NDIS_INITIALIZE_MSG;
    cmd->MessageLength = sizeof(rndis_initialize_msg_t);
    cmd->RequestId = rndis_class->request_id++;
    cmd->MajorVersion = 1;
    cmd->MinorVersion = 0;
    cmd->MaxTransferSize = 0x4000;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SEND_ENCAPSULATED_COMMAND;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = sizeof(rndis_initialize_msg_t);

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)cmd);
    if (ret < 0) {
        USB_LOG_ERR("rndis_initialize_msg_t send error, ret: %d\r\n", ret);
        return ret;
    }

    //ret = usbh_ep_intr_transfer()

    resp = (rndis_initialize_cmplt_t *)g_rndis_buf;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)resp);
    if (ret < 0) {
        USB_LOG_ERR("rndis_initialize_cmplt_t recv error, ret: %d\r\n", ret);
        return ret;
    }

    return ret;
}

int usbh_rndis_query_msg_transfer(struct usbh_rndis *rndis_class, uint32_t oid, uint32_t query_len, uint8_t *info, uint32_t *info_len)
{
    struct usb_setup_packet *setup = &rndis_class->hport->setup;
    int ret = 0;
    rndis_query_msg_t *cmd;
    rndis_query_cmplt_t *resp;

    cmd = (rndis_query_msg_t *)g_rndis_buf;

    cmd->MessageType = REMOTE_NDIS_QUERY_MSG;
    cmd->MessageLength = query_len + sizeof(rndis_query_msg_t);
    cmd->RequestId = rndis_class->request_id++;
    cmd->Oid = oid;
    cmd->InformationBufferLength = query_len;
    cmd->InformationBufferOffset = 20;
    cmd->DeviceVcHandle = 0;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SEND_ENCAPSULATED_COMMAND;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = sizeof(rndis_query_msg_t);

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)&cmd);
    if (ret < 0) {
        USB_LOG_ERR("oid:%08x send error, ret: %d\r\n", (unsigned int)oid, ret);
        return ret;
    }

    //ret = usbh_ep_intr_transfer()

    resp = (rndis_query_cmplt_t *)g_rndis_buf;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)resp);
    if (ret < 0) {
        USB_LOG_ERR("oid:%08x recv error, ret: %d\r\n", (unsigned int)oid, ret);
        return ret;
    }

    memcpy(info, ((uint8_t *)resp + sizeof(rndis_query_cmplt_t)), resp->InformationBufferLength);
    *info_len = resp->InformationBufferLength;

    return ret;
}

static int usbh_rndis_set_msg_transfer(struct usbh_rndis *rndis_class, uint32_t oid, uint8_t *info, uint32_t info_len)
{
    struct usb_setup_packet *setup = &rndis_class->hport->setup;
    int ret = 0;
    rndis_set_msg_t *cmd;
    rndis_set_cmplt_t *resp;

    cmd = (rndis_set_msg_t *)g_rndis_buf;

    cmd->MessageType = REMOTE_NDIS_SET_MSG;
    cmd->MessageLength = info_len + sizeof(rndis_set_msg_t);
    cmd->RequestId = rndis_class->request_id++;
    cmd->Oid = oid;
    cmd->InformationBufferLength = info_len;
    cmd->InformationBufferOffset = 20;
    cmd->DeviceVcHandle = 0;

    memcpy(((uint8_t *)cmd + sizeof(rndis_set_msg_t)), info, info_len);
    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SEND_ENCAPSULATED_COMMAND;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = sizeof(rndis_set_msg_t);

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)cmd);
    if (ret < 0) {
        USB_LOG_ERR("oid:%08x send error, ret: %d\r\n", (unsigned int)oid, ret);
        return ret;
    }

    //ret = usbh_ep_intr_transfer(rndis_class->hport->intin,buf,len,500);

    resp = (rndis_set_cmplt_t *)g_rndis_buf;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)resp);
    if (ret < 0) {
        USB_LOG_ERR("oid:%08x recv error, ret: %d\r\n", (unsigned int)oid, ret);
        return ret;
    }

    return ret;
}

int usbh_rndis_bulk_out_transfer(struct usbh_rndis *rndis_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usbh_urb *urb = &rndis_class->bulkout_urb;
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_bulk_urb_fill(urb, rndis_class->bulkout, buffer, buflen, timeout, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

int usbh_rndis_bulk_in_transfer(struct usbh_rndis *rndis_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usbh_urb *urb = &rndis_class->bulkin_urb;
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_bulk_urb_fill(urb, rndis_class->bulkin, buffer, buflen, timeout, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

int usbh_rndis_keepalive(struct usbh_rndis *rndis_class)
{
    struct usb_setup_packet *setup = &rndis_class->hport->setup;
    int ret = 0;
    rndis_keepalive_msg_t *cmd;
    rndis_keepalive_cmplt_t *resp;

    cmd = (rndis_keepalive_msg_t *)g_rndis_buf;

    cmd->MessageType = REMOTE_NDIS_KEEPALIVE_MSG;
    cmd->MessageLength = sizeof(rndis_keepalive_msg_t);
    cmd->RequestId = rndis_class->request_id++;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SEND_ENCAPSULATED_COMMAND;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = sizeof(rndis_keepalive_msg_t);

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)&cmd);
    if (ret < 0) {
        USB_LOG_ERR("keepalive send error, ret: %d\r\n", ret);
        return ret;
    }

    //ret = usbh_ep_intr_transfer(rndis_class->hport->intin,buf,len,500);

    resp = (rndis_keepalive_cmplt_t *)g_rndis_buf;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)resp);
    if (ret < 0) {
        USB_LOG_ERR("keepalive recv error, ret: %d\r\n", ret);
        return ret;
    }

    return ret;
}

static int usbh_rndis_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint32_t *oid_support_list;
    unsigned int oid = 0;
    unsigned int oid_num = 0;
    uint32_t data_len;
    uint8_t tmp_buffer[512];
    uint8_t data[32];

    struct usbh_rndis *rndis_class = usb_malloc(sizeof(struct usbh_rndis));
    if (rndis_class == NULL) {
        USB_LOG_ERR("Fail to alloc rndis_class\r\n");
        return -ENOMEM;
    }

    memset(rndis_class, 0, sizeof(struct usbh_rndis));

    rndis_class->hport = hport;
    rndis_class->ctrl_intf = intf;
    rndis_class->data_intf = intf + 1;

    hport->config.intf[intf].priv = rndis_class;
    hport->config.intf[intf + 1].priv = NULL;

#ifdef CONFIG_USBHOST_RNDIS_NOTIFY
    ep_desc = &hport->config.intf[intf].altsetting[0].ep[0].ep_desc;
    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;

    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.hport = hport;
    usbh_pipe_alloc(&rndis_class->intin, &ep_cfg);

#endif
    for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].altsetting[0].ep[i].ep_desc;

        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_hport_activate_epx(&rndis_class->bulkin, hport, ep_desc);
        } else {
            usbh_hport_activate_epx(&rndis_class->bulkout, hport, ep_desc);
        }
    }

    ret = usbh_rndis_init_msg_transfer(rndis_class);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("rndis init success\r\n");

    ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_SUPPORTED_LIST, 0, tmp_buffer, &data_len);
    if (ret < 0) {
        return ret;
    }
    oid_num = (data_len / 4);
    USB_LOG_INFO("rndis query OID_GEN_SUPPORTED_LIST success,oid num :%d\r\n", oid_num);

    oid_support_list = (uint32_t *)tmp_buffer;

    for (uint8_t i = 0; i < oid_num; i++) {
        oid = oid_support_list[i];
        switch (oid) {
            case OID_GEN_PHYSICAL_MEDIUM:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_PHYSICAL_MEDIUM, 4, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }
                break;
            case OID_GEN_MAXIMUM_FRAME_SIZE:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_MAXIMUM_FRAME_SIZE, 4, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }
                break;
            case OID_GEN_LINK_SPEED:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_LINK_SPEED, 4, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }

                memcpy(&rndis_class->link_speed, data, 4);
                break;
            case OID_GEN_MEDIA_CONNECT_STATUS:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_MEDIA_CONNECT_STATUS, 4, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }
                if (NDIS_MEDIA_STATE_CONNECTED == data[0]) {
                    rndis_class->link_status = true;
                } else {
                    rndis_class->link_status = false;
                }
                break;
            case OID_802_3_MAXIMUM_LIST_SIZE:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_802_3_MAXIMUM_LIST_SIZE, 4, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }
                break;
            case OID_802_3_CURRENT_ADDRESS:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_802_3_CURRENT_ADDRESS, 6, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }

                for (uint8_t j = 0; j < 6; j++) {
                    rndis_class->mac[j] = data[j];
                }
                break;
            case OID_802_3_PERMANENT_ADDRESS:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_802_3_PERMANENT_ADDRESS, 6, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
                }
                break;
            default:
                USB_LOG_WRN("Ignore rndis query iod:%08x\r\n", oid);
                continue;
        }
        USB_LOG_INFO("rndis query iod:%08x success\r\n", oid);
    }

    uint32_t packet_filter = 0x0f;
    usbh_rndis_set_msg_transfer(rndis_class, OID_GEN_CURRENT_PACKET_FILTER, (uint8_t *)&packet_filter, 4);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("rndis set OID_GEN_CURRENT_PACKET_FILTER success\r\n");

    uint8_t multicast_list[6] = { 0x01, 0x00, 0x5E, 0x00, 0x00, 0x01 };
    usbh_rndis_set_msg_transfer(rndis_class, OID_802_3_MULTICAST_LIST, multicast_list, 6);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("rndis set OID_802_3_MULTICAST_LIST success\r\n");

    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    USB_LOG_INFO("Register RNDIS Class:%s\r\n", hport->config.intf[intf].devname);
    usbh_rndis_run(rndis_class);
    return ret;
query_errorout:
    USB_LOG_ERR("rndis query iod:%08x error\r\n", oid);
    return ret;
}

static int usbh_rndis_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_rndis *rndis_class = (struct usbh_rndis *)hport->config.intf[intf].priv;

    if (rndis_class) {
        if (rndis_class->bulkin) {
            usbh_pipe_free(rndis_class->bulkin);
        }

        if (rndis_class->bulkout) {
            usbh_pipe_free(rndis_class->bulkout);
        }

        usbh_rndis_stop(rndis_class);
        memset(rndis_class, 0, sizeof(struct usbh_rndis));
        usb_free(rndis_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister RNDIS Class:%s\r\n", hport->config.intf[intf].devname);
    }

    return ret;
}

__WEAK void usbh_rndis_run(struct usbh_rndis *rndis_class)
{
}

__WEAK void usbh_rndis_stop(struct usbh_rndis *rndis_class)
{
}

static const struct usbh_class_driver rndis_class_driver = {
    .driver_name = "rndis",
    .connect = usbh_rndis_connect,
    .disconnect = usbh_rndis_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info rndis_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_WIRELESS,
    .subclass = 0x01,
    .protocol = 0x03,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &rndis_class_driver
};
