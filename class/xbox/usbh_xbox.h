/*
 * Copyright (c) 2024, Till Harbaum
 */
#ifndef USBH_XBOX_H
#define USBH_XBOX_H

struct usbh_xbox {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *intin;  /* INTR IN endpoint */
    struct usb_endpoint_descriptor *intout; /* INTR OUT endpoint */
    struct usbh_urb intin_urb;              /* INTR IN urb */
    struct usbh_urb intout_urb;             /* INTR OUT urb */

    uint8_t intf; /* interface number */
    uint8_t minor;
};

#ifdef __cplusplus
extern "C" {
#endif

  //int usbh_xbox_set_idle(struct usbh_xbox *xbox_class, uint8_t report_id, uint8_t duration);
  //int usbh_xbox_get_idle(struct usbh_xbox *xbox_class, uint8_t *buffer);

void usbh_xbox_run(struct usbh_xbox *xbox_class);
void usbh_xbox_stop(struct usbh_xbox *xbox_class);

#ifdef __cplusplus
}
#endif

#endif /* USBH_XBOX_H */
