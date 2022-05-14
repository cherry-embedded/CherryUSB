#include "usbd_core.h"
#include "usbd_xxx.h"

static int xxx_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_WRN("XXX Class request: "
                 "bRequest 0x%02x\r\n",
                 setup->bRequest);

    switch (setup->bRequest) {
        default:
            USB_LOG_WRN("Unhandled XXX Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void xxx_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:

            break;

        default:
            break;
    }
}

void usbd_xxx_add_interface(usbd_class_t *devclass, usbd_interface_t *intf)
{
    static usbd_class_t *last_class = NULL;

    if (last_class != devclass) {
        last_class = devclass;
        usbd_class_register(devclass);
    }

    intf->class_handler = xxx_class_request_handler;
    intf->custom_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = xxx_notify_handler;

    usbd_class_add_interface(devclass, intf);
}
