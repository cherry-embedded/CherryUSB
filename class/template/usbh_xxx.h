#ifndef _USBH_XXX_H
#define _USBH_XXX_H

#include "usb_xxx.h"

struct usbh_xxx {
    struct usbh_hubport *hport;

    uint8_t intf; /* interface number */
    uint8_t minor;
    usbh_pipe_t bulkin;  /* bulk IN endpoint */
    usbh_pipe_t bulkout; /* bulk OUT endpoint */
};

#endif