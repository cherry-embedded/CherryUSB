/*
 * Change Logs
 * Date           Author       Notes
 * 2022-04-17     aozima       the first version for CherryUSB.
 */

#include <string.h>

#include "usbh_core.h"
#include "axusbnet.h"

static const char *DEV_FORMAT = "/dev/u%d";

static int usbh_axusbnet_connect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;

    USB_LOG_INFO("%s %d\r\n", __FUNCTION__, __LINE__);

    struct usbh_axusbnet *class = usb_malloc(sizeof(struct usbh_axusbnet));
    if (class == NULL) 
    {
        USB_LOG_ERR("Fail to alloc class\r\n");
        return -ENOMEM;
    }
    memset(class, 0, sizeof(struct usbh_axusbnet));
    class->hport = hport;

    class->intf = intf;

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, intf);
    USB_LOG_INFO("Register axusbnet Class:%s\r\n", hport->config.intf[intf].devname);
    hport->config.intf[intf].priv = class;

#if 1
    USB_LOG_INFO("hport=%p, intf=%d, intf_desc.bNumEndpoints:%d\r\n", hport, intf, hport->config.intf[intf].intf_desc.bNumEndpoints);
    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++) 
    {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;

        USB_LOG_INFO("ep[%d] bLength=%d, type=%d\r\n", i, ep_desc->bLength, ep_desc->bDescriptorType);
        USB_LOG_INFO("ep_addr=%02X, attr=%02X\r\n", ep_desc->bEndpointAddress, ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK);
        USB_LOG_INFO("wMaxPacketSize=%d, bInterval=%d\r\n\r\n", ep_desc->wMaxPacketSize, ep_desc->bInterval);
    }
#endif

    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++) 
    {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;

        if(ep_cfg.ep_type == USB_ENDPOINT_TYPE_BULK)
        {
            if (ep_desc->bEndpointAddress & 0x80) {
                usbh_ep_alloc(&class->bulkin, &ep_cfg);
            } else {
                usbh_ep_alloc(&class->bulkout, &ep_cfg);
            }
        }
        else
        {
            usbh_ep_alloc(&class->int_notify, &ep_cfg);
        }
    }

    return ret;
}

static int usbh_axusbnet_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    USB_LOG_ERR("TBD: %s %d\r\n", __FUNCTION__, __LINE__);
    return ret;
}

// Class:0xff,Subclass:0xff,Protocl:0x00
static const struct usbh_class_driver axusbnet_class_driver = {
    .driver_name = "axusbnet",
    .connect = usbh_axusbnet_connect,
    .disconnect = usbh_axusbnet_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info axusbnet_class_info = {
    .match_flags = USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_PRODUCT | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_VEND_SPECIFIC,
    .subclass = 0xff,
    .protocol = 0x00,
    .vid = 0x0b95,
    .pid = 0x772b,
    .class_driver = &axusbnet_class_driver
};
