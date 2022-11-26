/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_MSC_H
#define USBH_MSC_H

#include "usb_msc.h"
#include "usb_scsi.h"

struct usbh_msc {
    struct usbh_hubport *hport;

    uint8_t intf; /* Data interface number */
    uint8_t sdchar;
    usbh_pipe_t bulkin;          /* Bulk IN endpoint */
    usbh_pipe_t bulkout;         /* Bulk OUT endpoint */
    struct usbh_urb bulkin_urb;  /* Bulk IN urb */
    struct usbh_urb bulkout_urb; /* Bulk OUT urb */
    uint32_t blocknum;           /* Number of blocks on the USB mass storage device */
    uint16_t blocksize;          /* Block size of USB mass storage device */
};

int usbh_msc_scsi_write10(struct usbh_msc *msc_class, uint32_t start_sector, const uint8_t *buffer, uint32_t nsectors);
int usbh_msc_scsi_read10(struct usbh_msc *msc_class, uint32_t start_sector, const uint8_t *buffer, uint32_t nsectors);

void usbh_msc_run(struct usbh_msc *msc_class);
void usbh_msc_stop(struct usbh_msc *msc_class);

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* USBH_MSC_H */
