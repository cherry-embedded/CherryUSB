#include "usbh_core.h"
#include "usbh_xxx.h"

#define DEV_FORMAT "/dev/xxx"

static int usbh_xxx_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_xxx *xxx_class = usb_malloc(sizeof(struct usbh_xxx));
    if (xxx_class == NULL) {
        USB_LOG_ERR("Fail to alloc xxx_class\r\n");
        return -ENOMEM;
    }

    memset(xxx_class, 0, sizeof(struct usbh_xxx));

    xxx_class->hport = hport;
    xxx_class->intf = intf;

    hport->config.intf[intf].priv = xxx_class;
    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    for (uint8_t i = 0; i < hport->config.intf[intf + 1].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf + 1].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_pipe_alloc(&rndis_class->bulkin, &ep_cfg);
        } else {
            usbh_pipe_alloc(&rndis_class->bulkout, &ep_cfg);
        }
    }

    return ret;

}


static int usbh_xxx_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_xxx *xxx_class = (struct usbh_xxx *)hport->config.intf[intf].priv;

    if (xxx_class) {
        if (xxx_class->bulkin) {
            usbh_pipe_free(xxx_class->bulkin);
        }

        if (xxx_class->bulkout) {
            usbh_pipe_free(xxx_class->bulkout);
        }

        usb_free(xxx_class);

        USB_LOG_INFO("Unregister xxx Class:%s\r\n", hport->config.intf[intf].devname);
        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);

        hport->config.intf[intf].priv = NULL;
    }

    return ret;
}


static const struct usbh_class_driver xxx_class_driver = {
    .driver_name = "xxx",
    .connect = usbh_xxx_connect,
    .disconnect = usbh_xxx_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info xxx_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = 0,
    .subclass = 0,
    .protocol = 0,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &xxx_class_driver
};
