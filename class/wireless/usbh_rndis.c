#include "usbh_core.h"
#include "usbh_rndis.h"
#include "rndis_protocol.h"

#define DEV_FORMAT "/dev/rndis"

static int usbh_rndis_init_msg_transfer(struct usbh_rndis *rndis_class)
{
    struct usb_setup_packet *setup = rndis_class->hport->setup;
    int ret = 0;
    rndis_initialize_msg_t cmd;
    rndis_initialize_cmplt_t resp;

    cmd.MessageType = REMOTE_NDIS_INITIALIZE_MSG;
    cmd.MessageLength = sizeof(rndis_initialize_msg_t);
    cmd.RequestId = rndis_class->request_id++;
    cmd.MajorVersion = 1;
    cmd.MinorVersion = 0;
    cmd.MaxTransferSize = 0x4000;

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_SEND_ENCAPSULATED_COMMAND;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = sizeof(rndis_initialize_msg_t);

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)&cmd);
    if (ret < 0) {
        USB_LOG_ERR("rndis_initialize_msg_t send error, ret: %d\r\n", ret);
        return ret;
    }

    //ret = usbh_ep_intr_transfer()

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)&resp);
    if (ret < 0) {
        USB_LOG_ERR("rndis_initialize_cmplt_t recv error, ret: %d\r\n", ret);
        return ret;
    }

    return ret;
}

int usbh_rndis_query_msg_transfer(struct usbh_rndis *rndis_class, uint32_t oid, uint32_t query_len, uint8_t *info, uint32_t *info_len)
{
    struct usb_setup_packet *setup = rndis_class->hport->setup;
    int ret = 0;
    rndis_query_msg_t cmd;
    rndis_query_cmplt_t *resp;

    cmd.MessageType = REMOTE_NDIS_QUERY_MSG;
    cmd.MessageLength = query_len + sizeof(rndis_query_msg_t);
    cmd.RequestId = rndis_class->request_id++;
    cmd.Oid = oid;
    cmd.InformationBufferLength = query_len;
    cmd.InformationBufferOffset = 20;
    cmd.DeviceVcHandle = 0;

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

    resp = usb_iomalloc(sizeof(rndis_query_cmplt_t) + 512);
    if (resp == NULL) {
        USB_LOG_ERR("Fail to alloc resp\r\n");
        return -ENOMEM;
    }
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)resp);
    if (ret < 0) {
        USB_LOG_ERR("oid:%08x recv error, ret: %d\r\n", (unsigned int)oid, ret);
        goto error_out;
    }

    memcpy(info, ((uint8_t *)resp + sizeof(rndis_query_cmplt_t)), resp->InformationBufferLength);
    *info_len = resp->InformationBufferLength;

error_out:
    if (resp) {
        usb_iofree(resp);
    }
    return ret;
}

static int usbh_rndis_set_msg_transfer(struct usbh_rndis *rndis_class, uint32_t oid, uint8_t *info, uint32_t info_len)
{
    struct usb_setup_packet *setup = rndis_class->hport->setup;
    int ret = 0;
    rndis_set_msg_t *cmd;
    rndis_set_cmplt_t resp;

    cmd = usb_iomalloc(sizeof(rndis_set_msg_t) + info_len);
    if (cmd == NULL) {
        USB_LOG_ERR("Fail to alloc cmd\r\n");
        return -ENOMEM;
    }
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
        goto error_out;
    }

    //ret = usbh_ep_intr_transfer(rndis_class->hport->intin,buf,len,500);

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)&resp);
    if (ret < 0) {
        USB_LOG_ERR("oid:%08x recv error, ret: %d\r\n", (unsigned int)oid, ret);
        goto error_out;
    }

error_out:
    if (cmd) {
        usb_iofree(cmd);
    }
    return ret;
}

int usbh_rndis_keepalive(struct usbh_rndis *rndis_class)
{
    struct usb_setup_packet *setup = rndis_class->hport->setup;
    int ret = 0;
    rndis_keepalive_msg_t cmd;
    rndis_keepalive_cmplt_t resp;

    cmd.MessageType = REMOTE_NDIS_KEEPALIVE_MSG;
    cmd.MessageLength = sizeof(rndis_keepalive_msg_t);
    cmd.RequestId = rndis_class->request_id++;

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

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = CDC_REQUEST_GET_ENCAPSULATED_RESPONSE;
    setup->wValue = 0;
    setup->wIndex = 0;
    setup->wLength = 4096;

    ret = usbh_control_transfer(rndis_class->hport->ep0, setup, (uint8_t *)&resp);
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
    ep_desc = &hport->config.intf[intf].ep[0].ep_desc;
    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
    ep_cfg.ep_interval = ep_desc->bInterval;
    ep_cfg.hport = hport;
    usbh_ep_alloc(&rndis_class->intin, &ep_cfg);

#endif
    for (uint8_t i = 0; i < hport->config.intf[intf + 1].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_ep_alloc(&rndis_class->bulkin, &ep_cfg);
        } else {
            usbh_ep_alloc(&rndis_class->bulkout, &ep_cfg);
        }
    }

    ret = usbh_rndis_init_msg_transfer(rndis_class);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("rndis init success\r\n");

    uint8_t *tmp_buffer = usb_iomalloc(512);
    uint8_t data[32];

    ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_SUPPORTED_LIST, 0, tmp_buffer, &data_len);
    if (ret < 0) {
        goto query_errorout;
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
                break;
            case OID_GEN_MEDIA_CONNECT_STATUS:
                ret = usbh_rndis_query_msg_transfer(rndis_class, OID_GEN_MEDIA_CONNECT_STATUS, 4, data, &data_len);
                if (ret < 0) {
                    goto query_errorout;
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
    usb_iofree(tmp_buffer);

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
    return ret;
query_errorout:
    USB_LOG_ERR("rndis query iod:%08x error\r\n", oid);
    usb_iofree(tmp_buffer);
    return ret;
}

static int usbh_rndis_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_rndis *rndis_class = (struct usbh_rndis *)hport->config.intf[intf].priv;

    if (rndis_class) {
        if (rndis_class->bulkin) {
            ret = usb_ep_cancel(rndis_class->bulkin);
            if (ret < 0) {
            }
            usbh_ep_free(rndis_class->bulkin);
        }

        if (rndis_class->bulkout) {
            ret = usb_ep_cancel(rndis_class->bulkout);
            if (ret < 0) {
            }
            usbh_ep_free(rndis_class->bulkout);
        }

        usb_free(rndis_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister RNDIS Class:%s\r\n", hport->config.intf[intf].devname);
        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);

        hport->config.intf[intf].priv = NULL;
        hport->config.intf[intf + 1].priv = NULL;
    }

    return ret;
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
