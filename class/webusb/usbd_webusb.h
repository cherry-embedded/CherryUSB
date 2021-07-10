#ifndef _USBD_WEBUSB_H
#define _USBD_WEBUSB_H

/* WebUSB Descriptor Types */
#define WEBUSB_DESCRIPTOR_SET_HEADER_TYPE       0x00
#define WEBUSB_CONFIGURATION_SUBSET_HEADER_TYPE 0x01
#define WEBUSB_FUNCTION_SUBSET_HEADER_TYPE      0x02
#define WEBUSB_URL_TYPE                         0x03

/* WebUSB Request Codes */
#define WEBUSB_REQUEST_GET_URL 0x02

/* bScheme in URL descriptor */
#define WEBUSB_URL_SCHEME_HTTP  0x00
#define WEBUSB_URL_SCHEME_HTTPS 0x01

/* WebUSB Descriptor sizes */
#define WEBUSB_DESCRIPTOR_SET_HEADER_SIZE       5
#define WEBUSB_CONFIGURATION_SUBSET_HEADER_SIZE 4
#define WEBUSB_FUNCTION_SUBSET_HEADER_SIZE      3

/* BOS Capability webusb */
struct usb_bos_webusb_platform_capability_descriptor {
    struct usb_bos_capability_descriptor webusb_platform;
    uint16_t bcdVersion;
    uint8_t bVendorCode;
    uint8_t iLandingPage;
} __packed;

struct webusb_url_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bScheme;
    char URL[];
} __packed;

#endif