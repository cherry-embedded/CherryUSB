/**
 * @file
 * @brief USB Human Interface Device (HID) Class public header
 *
 * Header follows Device Class Definition for Human Interface Devices (HID)
 * Version 1.11 document (HID1_11-1.pdf).
 */
#ifndef _USBD_HID_H_
#define _USBD_HID_H_

#include "usb_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

void usbd_hid_descriptor_register(uint8_t intf_num, const uint8_t *desc);
void usbd_hid_report_descriptor_register(uint8_t intf_num, const uint8_t *desc, uint32_t desc_len);
void usbd_hid_add_interface(usbd_class_t *class, usbd_interface_t *intf);
void usbd_hid_reset_state(void);
void usbd_hid_send_report(uint8_t ep, uint8_t *data, uint8_t len);
// clang-format off
void usbd_hid_set_request_callback( uint8_t intf_num,
                                    uint8_t (*get_report_callback)(uint8_t report_id, uint8_t report_type),
                                    void (*set_report_callback)(uint8_t report_id, uint8_t report_type, uint8_t *report, uint8_t report_len),
                                    uint8_t (*get_idle_callback)(uint8_t report_id),
                                    void (*set_idle_callback)(uint8_t report_id, uint8_t duration),
                                    void (*set_protocol_callback)(uint8_t protocol),
                                    uint8_t (*get_protocol_callback)(void));
// clang-format on
#ifdef __cplusplus
}
#endif

#endif /* _USBD_HID_H_ */
