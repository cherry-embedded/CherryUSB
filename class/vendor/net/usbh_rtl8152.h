/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBH_RTL8152_H
#define USBH_RTL8152_H

#define USBH_RTL8152_LINK_STATE_DOWN         0
#define USBH_RTL8152_LINK_STATE_UP           1
#define USBH_RTL8152_LINK_STATE_PENDING_UP   2
#define USBH_RTL8152_LINK_STATE_PENDING_DOWN 3

struct usbh_rtl8152 {
    struct usbh_hubport *hport;
    struct usb_endpoint_descriptor *bulkin;  /* Bulk IN endpoint */
    struct usb_endpoint_descriptor *bulkout; /* Bulk OUT endpoint */
    struct usb_endpoint_descriptor *intin;   /* INTR IN endpoint  */
    struct usbh_urb bulkout_urb;
    struct usbh_urb bulkin_urb;
    struct usbh_urb intin_urb;

    uint8_t intf;

    uint8_t version;
    uint8_t eee_adv;
    uint8_t eee_en;
    uint8_t supports_gmii;
    uint16_t min_mtu;
    uint16_t max_mtu;
    uint16_t ocp_base;
    uint32_t saved_wolopts;
    uint32_t rx_buf_sz;

    struct rtl_ops {
        void (*init)(struct usbh_rtl8152 *tp);
        int (*enable)(struct usbh_rtl8152 *tp);
        void (*disable)(struct usbh_rtl8152 *tp);
        void (*up)(struct usbh_rtl8152 *tp);
        void (*down)(struct usbh_rtl8152 *tp);
        void (*unload)(struct usbh_rtl8152 *tp);
        bool (*in_nway)(struct usbh_rtl8152 *tp);
        void (*hw_phy_cfg)(struct usbh_rtl8152 *tp);
        void (*autosuspend_en)(struct usbh_rtl8152 *tp, bool enable);
        void (*change_mtu)(struct usbh_rtl8152 *tp);
    } rtl_ops;

    uint8_t mac[6];
    uint8_t link_state;
    uint32_t link_speed;
    usb_osal_sem_t link_sem;
    struct usb_osal_timer *link_timer;
    usb_osal_sem_t rx_sem;
    usb_osal_thread_t thread;

    void *user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

void usbh_rtl8152_run(struct usbh_rtl8152 *rtl8152_class);
void usbh_rtl8152_stop(struct usbh_rtl8152 *rtl8152_class);

uint8_t *usbh_rtl8152_get_eth_txbuf(void);
int usbh_rtl8152_eth_output(uint32_t buflen);
void usbh_rtl8152_eth_input(uint8_t *buf, uint32_t buflen);
void usbh_rtl8152_link_up(bool up);

#ifdef __cplusplus
}
#endif

#endif /* USBH_RTL8152_H */