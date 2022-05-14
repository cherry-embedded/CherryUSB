#ifndef _USBH_XXX_H
#define _USBH_XXX_H

#include "usb_xxx.h"

struct usbh_xxx {
    struct usbh_hubport *hport;

    uint8_t intf; /* interface number */
    usbh_epinfo_t intin;  /* INTR IN endpoint */
    usbh_epinfo_t intout; /* INTR OUT endpoint */
};

#endif