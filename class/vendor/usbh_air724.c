#include "usbh_core.h"

#define DEV_FORMAT "/dev/air724"

static uint32_t g_devinuse = 0;

struct usbh_cdc_custom_air724 {
    struct usbh_hubport *hport;

    usbh_epinfo_t bulkin;  /* Bulk IN endpoint */
    usbh_epinfo_t bulkout; /* Bulk OUT endpoint */
};

int usbh_air724_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    if (intf != 3) {
        USB_LOG_WRN("ignore intf:%d\r\n", intf);
        return 0;
    }
    struct usbh_cdc_custom_air724 *cdc_custom_class = usb_malloc(sizeof(struct usbh_cdc_custom_air724));
    if (cdc_custom_class == NULL) {
        USB_LOG_ERR("Fail to alloc cdc_custom_class\r\n");
        return -ENOMEM;
    }

    memset(cdc_custom_class, 0, sizeof(struct usbh_cdc_custom_air724));
    cdc_custom_class->hport = hport;

    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    hport->config.intf[intf].priv = cdc_custom_class;

    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_ep_alloc(&cdc_custom_class->bulkin, &ep_cfg);
        } else {
            usbh_ep_alloc(&cdc_custom_class->bulkout, &ep_cfg);
        }
    }

    USB_LOG_INFO("Register air724 Class:%s\r\n", hport->config.intf[intf].devname);
    uint8_t cdc_buffer[32] = {0X41,0X54,0x0d,0x0a};
    ret = usbh_ep_bulk_transfer(cdc_custom_class->bulkout, cdc_buffer, 4, 3000);
    if (ret < 0) {
        USB_LOG_ERR("bulk out error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("send over:%d\r\n", ret);
    }
    ret = usbh_ep_bulk_transfer(cdc_custom_class->bulkin, cdc_buffer, 10, 3000);
    if (ret < 0) {
        USB_LOG_ERR("bulk in error,ret:%d\r\n", ret);
    } else {
        USB_LOG_RAW("recv over:%d\r\n", ret);
        for (size_t i = 0; i < ret; i++) {
            USB_LOG_RAW("0x%02x ", cdc_buffer[i]);
        }
    }

    return ret;
}

int usbh_air724_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    return 0;
}

const struct usbh_class_driver cdc_custom_class_driver = {
    .driver_name = "cdc_acm",
    .connect = usbh_air724_connect,
    .disconnect = usbh_air724_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info cdc_custom_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = 0xff,
    .subclass = 0,
    .protocol = 0,
    .vid = 0x1782,
    .pid = 0x4e00,
    .class_driver = &cdc_custom_class_driver
};
