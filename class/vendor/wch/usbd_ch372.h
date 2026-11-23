/*
 * Copyright (c) 2026, Links (lhd@wch.cn)
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_CH372_H
#define USBD_CH372_H

#ifdef __cplusplus
extern "C" {
#endif

/*Length of template descriptor: 51 bytes*/
#define CH372_DESCRIPTOR_LEN (9 + 7 + 7 + 7 + 7 + 7 + 7)

// clang-format off
#define CH372_DESCRIPTOR_INIT(in_ep1, out_ep1, in_ep2, out_ep2, in_ep3, out_ep3, wMaxPacketSize, str_idx) \
    0x09,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_INTERFACE,                         /* bDescriptorType */               \
    0x00,                                                  /* bInterfaceNumber */              \
    0x00,                                                  /* bAlternateSetting */             \
    0x06,                                                  /* bNumEndpoints */                 \
    0xFF,                                                  /* bInterfaceClass */               \
    0xFF,                                                  /* bInterfaceSubClass */            \
    0xFF,                                                  /* bInterfaceProtocol */            \
    str_idx,                                               /* iInterface */                    \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    in_ep1,                                                /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(wMaxPacketSize),                                 /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    out_ep1,                                               /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(wMaxPacketSize),                                 /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    in_ep2,                                                /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(wMaxPacketSize),                                 /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    out_ep2,                                               /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(wMaxPacketSize),                                 /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    in_ep3,                                                /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(wMaxPacketSize),                                 /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    out_ep3,                                               /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(wMaxPacketSize),                                 /* wMaxPacketSize */                \
    0x00                                                   /* bInterval */

    /*Length of template descriptor: 87 bytes*/
#define USB3_CH372_DESCRIPTOR_LEN (9 + 7 + 6 + 7 + 6 + 7 + 6 + 7 + 6 + 7 + 6 + 7 + 6)

// clang-format off
#define USB3_CH372_DESCRIPTOR_INIT(in_ep1, out_ep1, in_ep2, out_ep2, in_ep3, out_ep3, max_burst, str_idx) \
    0x09,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_INTERFACE,                         /* bDescriptorType */               \
    0x00,                                                  /* bInterfaceNumber */              \
    0x00,                                                  /* bAlternateSetting */             \
    0x06,                                                  /* bNumEndpoints */                 \
    0xFF,                                                  /* bInterfaceClass */               \
    0xFF,                                                  /* bInterfaceSubClass */            \
    0xFF,                                                  /* bInterfaceProtocol */            \
    str_idx,                                               /* iInterface */                    \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    in_ep1,                                                /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(1024),                                           /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x06,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT_COMPANION,                /* bDescriptorType */               \
    max_burst - 1,                                         /* bMaxBurst */                     \
    0x00,                                                  /* bmAttributes */                  \
    0x00, 0x00,                                            /* wBytesPerInterval  */            \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    out_ep1,                                               /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(1024),                                           /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x06,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT_COMPANION,                /* bDescriptorType */               \
    max_burst - 1,                                         /* bMaxBurst */                     \
    0x00,                                                  /* bmAttributes */                  \
    0x00, 0x00,                                            /* wBytesPerInterval  */            \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    in_ep2,                                                /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(1024),                                           /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x06,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT_COMPANION,                /* bDescriptorType */               \
    max_burst - 1,                                         /* bMaxBurst */                     \
    0x00,                                                  /* bmAttributes */                  \
    0x00, 0x00,                                            /* wBytesPerInterval  */            \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    out_ep2,                                               /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(1024),                                           /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x06,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT_COMPANION,                /* bDescriptorType */               \
    max_burst - 1,                                         /* bMaxBurst */                     \
    0x00,                                                  /* bmAttributes */                  \
    0x00, 0x00,                                            /* wBytesPerInterval  */            \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    in_ep3,                                                /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(1024),                                           /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x06,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT_COMPANION,                /* bDescriptorType */               \
    max_burst - 1,                                         /* bMaxBurst */                     \
    0x00,                                                  /* bmAttributes */                  \
    0x00, 0x00,                                            /* wBytesPerInterval  */            \
    0x07,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT,                          /* bDescriptorType */               \
    out_ep3,                                               /* bEndpointAddress */              \
    0x02,                                                  /* bmAttributes */                  \
    WBVAL(1024),                                           /* wMaxPacketSize */                \
    0x00,                                                  /* bInterval */                     \
    0x06,                                                  /* bLength */                       \
    USB_DESCRIPTOR_TYPE_ENDPOINT_COMPANION,                /* bDescriptorType */               \
    max_burst - 1,                                         /* bMaxBurst */                     \
    0x00,                                                  /* bmAttributes */                  \
    0x00, 0x00                                             /* wBytesPerInterval  */

/* Init ch372 interface driver */
struct usbd_interface *usbd_ch372_init_intf(uint8_t busid, struct usbd_interface *intf);

#ifdef __cplusplus
}
#endif

#endif /* USBD_CH372_H */
