/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Demo aliases for the CherryUSB usbh_aic8800 class. Boot/FMAC/Wi-Fi keep
 * the older aic8800_usb_* names; new code should include usbh_aic8800.h.
 */
#ifndef AIC8800_USB_H
#define AIC8800_USB_H

#include "usbh_aic8800.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AIC8800_USB_FAMILY_UNKNOWN USBH_AIC8800_FAMILY_UNKNOWN
#define AIC8800_USB_FAMILY_8800    USBH_AIC8800_FAMILY_8800
#define AIC8800_USB_FAMILY_D80     USBH_AIC8800_FAMILY_D80
#define AIC8800_USB_FAMILY_D80X2   USBH_AIC8800_FAMILY_D80X2
#define AIC8800_USB_FAMILY_DC      USBH_AIC8800_FAMILY_DC

#define aic8800_usb_device usbh_aic8800

#define aic8800_usb_get_device                usbh_aic8800_get_device
#define aic8800_usb_enable_zero_cd_modeswitch usbh_aic8800_enable_zero_cd_modeswitch
#define aic8800_usb_bulk_send                 usbh_aic8800_bulk_send
#define aic8800_usb_bulk_receive              usbh_aic8800_bulk_receive
#define aic8800_usb_bulk_receive_async        usbh_aic8800_bulk_receive_async
#define aic8800_usb_abort_receive             usbh_aic8800_abort_receive
#define aic8800_usb_attached                  usbh_aic8800_run
#define aic8800_usb_detaching                 usbh_aic8800_stop

#ifdef __cplusplus
}
#endif

#endif /* AIC8800_USB_H */
