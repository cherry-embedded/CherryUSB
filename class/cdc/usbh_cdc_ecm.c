/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_cdc_ecm.h"

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbh_cdc_ecm"
#include "usb_log.h"

#define DEV_FORMAT "/dev/cdc_ether"

/* general descriptor field offsets */
#define DESC_bLength            0 /** Length offset */
#define DESC_bDescriptorType    1 /** Descriptor type offset */
#define DESC_bDescriptorSubType 2 /** Descriptor subtype offset */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber  2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting 3 /** Alternate setting offset */

#define CONFIG_USBHOST_CDC_ECM_PKT_FILTER   0x000C
#define CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE 1514U

static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ecm_rx_buffer[USB_ALIGN_UP(CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE, CONFIG_USB_ALIGN_SIZE)];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ecm_tx_buffer[USB_ALIGN_UP(CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE, CONFIG_USB_ALIGN_SIZE)];
static USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_cdc_ecm_intr_buffer[USB_ALIGN_UP(sizeof(struct cdc_eth_notification), CONFIG_USB_ALIGN_SIZE)];

static struct usbh_cdc_ecm g_cdc_ecm_class;

static void usbh_cdc_ecm_thread(CONFIG_USB_OSAL_THREAD_SET_ARGV);

static int usbh_cdc_ecm_set_eth_packet_filter(struct usbh_cdc_ecm *cdc_ecm_class, uint16_t filter_value)
{
    struct usb_setup_packet setup;

    if (!cdc_ecm_class || !cdc_ecm_class->hport) {
        return -USB_ERR_INVAL;
    }

    setup.bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup.bRequest = CDC_REQUEST_SET_ETHERNET_PACKET_FILTER;
    setup.wValue = filter_value;
    setup.wIndex = cdc_ecm_class->ctrl_intf;
    setup.wLength = 0;

    return usbh_control_transfer(cdc_ecm_class->hport, &setup, NULL);
}

static void usbh_cdc_ecm_rx_complete(void *arg, int nbytes)
{
    struct usbh_cdc_ecm *cdc_ecm_class = (struct usbh_cdc_ecm *)arg;

    (void)nbytes;
    usb_osal_sem_give(cdc_ecm_class->rx_sem);
}

static void usbh_cdc_ecm_intr_complete(void *arg, int nbytes)
{
    struct usbh_cdc_ecm *cdc_ecm_class = (struct usbh_cdc_ecm *)arg;
    struct cdc_eth_notification *notification = (struct cdc_eth_notification *)g_cdc_ecm_intr_buffer;

    if (nbytes < 0 || !cdc_ecm_class->hport->connected) {
        return;
    }

    if (nbytes >= offsetof(struct cdc_eth_notification, data)) {
        if (notification->bNotificationType == CDC_ECM_NOTIFY_CODE_NETWORK_CONNECTION) {
            if (notification->wValue == CDC_ECM_NET_CONNECTED) {
                if (cdc_ecm_class->link_state == USBH_CDC_ECM_LINK_STATE_DOWN) {
                    cdc_ecm_class->link_state = USBH_CDC_ECM_LINK_STATE_PENDING_UP;
                    usb_osal_sem_give(cdc_ecm_class->link_sem);
                }
            } else if (cdc_ecm_class->link_state == USBH_CDC_ECM_LINK_STATE_UP) {
                cdc_ecm_class->link_state = USBH_CDC_ECM_LINK_STATE_PENDING_DOWN;
                usb_osal_sem_give(cdc_ecm_class->rx_sem);
                usb_osal_sem_give(cdc_ecm_class->link_sem);
            }
        } else if (notification->bNotificationType == CDC_ECM_NOTIFY_CODE_CONNECTION_SPEED_CHANGE &&
                   nbytes >= sizeof(struct cdc_eth_notification)) {
            memcpy(&cdc_ecm_class->link_speed, notification->data, sizeof(cdc_ecm_class->link_speed));
        }
    }
}

static void usbh_cdc_ecm_intr_timeout(void *arg)
{
    struct usbh_cdc_ecm *cdc_ecm_class = (struct usbh_cdc_ecm *)arg;

    /* Do not receive another link change until the worker handles this one. */
    if ((cdc_ecm_class->link_state != USBH_CDC_ECM_LINK_STATE_DOWN &&
         cdc_ecm_class->link_state != USBH_CDC_ECM_LINK_STATE_UP) ||
        cdc_ecm_class->intin_urb.errorcode == -USB_ERR_BUSY) {
        return;
    }

    usbh_int_urb_fill(&cdc_ecm_class->intin_urb, cdc_ecm_class->hport, cdc_ecm_class->intin,
                      g_cdc_ecm_intr_buffer, sizeof(struct cdc_eth_notification), 0, usbh_cdc_ecm_intr_complete, cdc_ecm_class);
    usbh_submit_urb(&cdc_ecm_class->intin_urb);
}

static int usbh_cdc_ecm_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint8_t altsetting = 0;
    char mac_buffer[12];
    uint8_t *p;
    uint8_t cur_iface = 0xff;
    uint8_t mac_str_idx = 0xff;
    uint32_t desc_len = 0;

    struct usbh_cdc_ecm *cdc_ecm_class = &g_cdc_ecm_class;

    memset(cdc_ecm_class, 0, sizeof(struct usbh_cdc_ecm));

    cdc_ecm_class->hport = hport;
    cdc_ecm_class->ctrl_intf = intf;
    cdc_ecm_class->data_intf = intf + 1;

    usbh_set_intf_data(hport, intf, cdc_ecm_class);
    usbh_set_intf_data(hport, intf + 1, NULL);

    if (hport->config.intf[intf + 1].altsetting[0].intf_desc.bInterfaceClass != USB_DEVICE_CLASS_CDC_DATA) {
        ret = -USB_ERR_INVAL;
        goto errout;
    }

    p = hport->raw_config_desc;
    while (desc_len < hport->config.config_desc.wTotalLength) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                cur_iface = p[INTF_DESC_bInterfaceNumber];
                //cur_alt_setting = p[INTF_DESC_bAlternateSetting];
                break;
            case CDC_CS_INTERFACE:
                if ((cur_iface == cdc_ecm_class->ctrl_intf) &&
                    (p[DESC_bDescriptorSubType] == CDC_FUNC_DESC_ETHERNET_NETWORKING) &&
                    (p[DESC_bLength] == sizeof(struct cdc_eth_descriptor))) {
                    struct cdc_eth_descriptor *desc = (struct cdc_eth_descriptor *)p;
                    mac_str_idx = desc->iMACAddress;
                    cdc_ecm_class->max_segment_size = desc->wMaxSegmentSize;
                    goto get_mac;
                }
                break;

            default:
                break;
        }
        /* skip to next descriptor */
        desc_len += p[DESC_bLength];
        p += p[DESC_bLength];
    }

get_mac:
    if (mac_str_idx == 0xff) {
        USB_LOG_ERR("Do not find cdc ecm mac string\r\n");
        ret = -1;
        goto errout;
    }

    memset(mac_buffer, 0, 12);
    ret = usbh_get_string_desc(cdc_ecm_class->hport, mac_str_idx, (uint8_t *)mac_buffer, 12);
    if (ret < 0) {
        goto errout;
    }

    for (int i = 0, j = 0; i < 12; i += 2, j++) {
        char byte_str[3];
        byte_str[0] = mac_buffer[i];
        byte_str[1] = mac_buffer[i + 1];
        byte_str[2] = '\0';

        uint32_t byte = strtoul(byte_str, NULL, 16);
        cdc_ecm_class->mac[j] = (unsigned char)byte;
    }

    USB_LOG_INFO("MAC address %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                 cdc_ecm_class->mac[0],
                 cdc_ecm_class->mac[1],
                 cdc_ecm_class->mac[2],
                 cdc_ecm_class->mac[3],
                 cdc_ecm_class->mac[4],
                 cdc_ecm_class->mac[5]);

    if (cdc_ecm_class->max_segment_size > CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE) {
        USB_LOG_ERR("Max Segment Size is overflow, default is %u, but now %u\r\n", CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE, cdc_ecm_class->max_segment_size);
    } else {
        USB_LOG_INFO("Max Segment Size:%u\r\n", cdc_ecm_class->max_segment_size);
    }

    /* enable int ep */
    ep_desc = &hport->config.intf[intf].altsetting[0].ep[0].ep_desc;
    USBH_EP_INIT(cdc_ecm_class->intin, ep_desc);

    if (hport->config.intf[intf + 1].altsetting_num > 1) {
        altsetting = hport->config.intf[intf + 1].altsetting_num - 1;

        for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[altsetting].intf_desc.bNumEndpoints; i++) {
            ep_desc = &hport->config.intf[intf + 1].altsetting[altsetting].ep[i].ep_desc;

            if (ep_desc->bEndpointAddress & 0x80) {
                USBH_EP_INIT(cdc_ecm_class->bulkin, ep_desc);
            } else {
                USBH_EP_INIT(cdc_ecm_class->bulkout, ep_desc);
            }
        }

        USB_LOG_INFO("Select altsetting: %d\r\n", altsetting);
        usbh_set_interface(cdc_ecm_class->hport, cdc_ecm_class->data_intf, altsetting);
    } else {
        for (uint8_t i = 0; i < hport->config.intf[intf + 1].altsetting[0].intf_desc.bNumEndpoints; i++) {
            ep_desc = &hport->config.intf[intf + 1].altsetting[0].ep[i].ep_desc;

            if (ep_desc->bEndpointAddress & 0x80) {
                USBH_EP_INIT(cdc_ecm_class->bulkin, ep_desc);
            } else {
                USBH_EP_INIT(cdc_ecm_class->bulkout, ep_desc);
            }
        }
    }

    /* bit0 Promiscuous
    * bit1 ALL Multicast
    * bit2 Directed
    * bit3 Broadcast
    * bit4 Multicast
    */
    ret = usbh_cdc_ecm_set_eth_packet_filter(cdc_ecm_class, CONFIG_USBHOST_CDC_ECM_PKT_FILTER);
    if (ret < 0) {
        goto errout;
    }
    USB_LOG_INFO("Set packet filter:%04x\r\n", CONFIG_USBHOST_CDC_ECM_PKT_FILTER);

    strncpy(hport->config.intf[intf].devname, DEV_FORMAT, CONFIG_USBHOST_DEV_NAMELEN);

    cdc_ecm_class->link_sem = usb_osal_sem_create(0);
    if (cdc_ecm_class->link_sem == NULL) {
        ret = -USB_ERR_NOMEM;
        goto errout;
    }
    cdc_ecm_class->rx_sem = usb_osal_sem_create(0);
    if (cdc_ecm_class->rx_sem == NULL) {
        ret = -USB_ERR_NOMEM;
        goto errout;
    }
    cdc_ecm_class->link_timer = usb_osal_timer_create("cdc_ecm_intr", 100, usbh_cdc_ecm_intr_timeout, cdc_ecm_class, true);
    if (cdc_ecm_class->link_timer == NULL) {
        ret = -USB_ERR_NOMEM;
        goto errout;
    }

    cdc_ecm_class->thread = usb_osal_thread_create("usbh_cdc_ecm", CONFIG_USBHOST_NET_THREAD_STACKSIZE,
                                                   CONFIG_USBHOST_NET_THREAD_PRIO, usbh_cdc_ecm_thread, NULL);
    if (cdc_ecm_class->thread == NULL) {
        ret = -USB_ERR_NOMEM;
        goto errout;
    }

    USB_LOG_INFO("Register CDC ECM Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_cdc_ecm_run(cdc_ecm_class);
    usb_osal_timer_start(cdc_ecm_class->link_timer);
    return 0;
errout:
    if (cdc_ecm_class->link_timer) {
        usb_osal_timer_delete(cdc_ecm_class->link_timer);
    }
    if (cdc_ecm_class->rx_sem) {
        usb_osal_sem_delete(cdc_ecm_class->rx_sem);
    }
    if (cdc_ecm_class->link_sem) {
        usb_osal_sem_delete(cdc_ecm_class->link_sem);
    }
    usbh_set_intf_data(hport, intf, NULL);
    memset(cdc_ecm_class, 0, sizeof(struct usbh_cdc_ecm));
    return ret;
}

static int usbh_cdc_ecm_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_cdc_ecm *cdc_ecm_class = (struct usbh_cdc_ecm *)usbh_get_intf_data(hport, intf);

    cdc_ecm_class->link_state = USBH_CDC_ECM_LINK_STATE_DOWN;
    usb_osal_timer_stop(cdc_ecm_class->link_timer);
    usb_osal_timer_delete(cdc_ecm_class->link_timer);

    usbh_kill_urb(&cdc_ecm_class->intin_urb);
    usbh_kill_urb(&cdc_ecm_class->bulkin_urb);
    usbh_kill_urb(&cdc_ecm_class->bulkout_urb);

    usb_osal_thread_schedule_other();

    usb_osal_thread_delete(cdc_ecm_class->thread);
    usb_osal_sem_delete(cdc_ecm_class->rx_sem);
    usb_osal_sem_delete(cdc_ecm_class->link_sem);
    USB_LOG_INFO("Unregister CDC ECM Class:%s\r\n", hport->config.intf[intf].devname);
    usbh_cdc_ecm_stop(cdc_ecm_class);

    usbh_set_intf_data(hport, intf, NULL);
    memset(cdc_ecm_class, 0, sizeof(struct usbh_cdc_ecm));
    return ret;
}

static void usbh_cdc_ecm_thread(CONFIG_USB_OSAL_THREAD_SET_ARGV)
{
    struct usbh_cdc_ecm *cdc_ecm_class = &g_cdc_ecm_class;
    uint32_t actual_length;
    int ret;

    (void)CONFIG_USB_OSAL_THREAD_GET_ARGV;

    USB_LOG_INFO("Link Down\r\n");
    while (1) {
        usb_osal_sem_take(cdc_ecm_class->link_sem, USB_OSAL_WAITING_FOREVER);
        if (cdc_ecm_class->link_state == USBH_CDC_ECM_LINK_STATE_PENDING_UP) {
            cdc_ecm_class->link_state = USBH_CDC_ECM_LINK_STATE_UP;

            usb_osal_sem_reset(cdc_ecm_class->rx_sem);
            usbh_cdc_ecm_link_up(true);
            USB_LOG_INFO("Link Up\r\n");

            while (cdc_ecm_class->link_state == USBH_CDC_ECM_LINK_STATE_UP) {
                usbh_bulk_urb_fill(&cdc_ecm_class->bulkin_urb, cdc_ecm_class->hport, cdc_ecm_class->bulkin,
                                   g_cdc_ecm_rx_buffer, CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE, 0, usbh_cdc_ecm_rx_complete, cdc_ecm_class);
                ret = usbh_submit_urb(&cdc_ecm_class->bulkin_urb);
                if (ret < 0) {
                    break;
                }

                usb_osal_sem_take(cdc_ecm_class->rx_sem, USB_OSAL_WAITING_FOREVER);
                if (cdc_ecm_class->link_state != USBH_CDC_ECM_LINK_STATE_UP) {
                    break;
                }

                if (cdc_ecm_class->bulkin_urb.errorcode < 0) {
                    break;
                }

                actual_length = cdc_ecm_class->bulkin_urb.actual_length;
                if (actual_length == 0) {
                    continue;
                }
                if (actual_length < CONFIG_USBHOST_CDC_ECM_ETH_MAX_SIZE) {
                    USB_LOG_DBG("rxlen:%d\r\n", actual_length);
                    usbh_cdc_ecm_eth_input(g_cdc_ecm_rx_buffer, actual_length);
                } else {
                    USB_LOG_ERR("rx packet overflow\r\n");
                }
            }
        } else if (cdc_ecm_class->link_state == USBH_CDC_ECM_LINK_STATE_PENDING_DOWN) {
            cdc_ecm_class->link_state = USBH_CDC_ECM_LINK_STATE_DOWN;
            usbh_cdc_ecm_link_up(false);

            usbh_kill_urb(&cdc_ecm_class->bulkin_urb);
            usbh_kill_urb(&cdc_ecm_class->bulkout_urb);
            USB_LOG_INFO("Link Down\r\n");
        }
    }
}

uint8_t *usbh_cdc_ecm_get_eth_txbuf(void)
{
    return g_cdc_ecm_tx_buffer;
}

int usbh_cdc_ecm_eth_output(uint32_t buflen)
{
    if (g_cdc_ecm_class.link_state != USBH_CDC_ECM_LINK_STATE_UP) {
        return -USB_ERR_NOTCONN;
    }

    if (buflen > sizeof(g_cdc_ecm_tx_buffer)) {
        return -USB_ERR_NOMEM;
    }

    USB_LOG_DBG("txlen:%d\r\n", buflen);

    usbh_bulk_urb_fill(&g_cdc_ecm_class.bulkout_urb, g_cdc_ecm_class.hport, g_cdc_ecm_class.bulkout, g_cdc_ecm_tx_buffer, buflen, USB_OSAL_WAITING_FOREVER, NULL, NULL);
    return usbh_submit_urb(&g_cdc_ecm_class.bulkout_urb);
}

const struct usbh_class_driver cdc_ecm_class_driver = {
    .driver_name = "cdc_ecm",
    .connect = usbh_cdc_ecm_connect,
    .disconnect = usbh_cdc_ecm_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info cdc_ecm_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .bInterfaceClass = USB_DEVICE_CLASS_CDC,
    .bInterfaceSubClass = CDC_ETHERNET_NETWORKING_CONTROL_MODEL,
    .bInterfaceProtocol = CDC_COMMON_PROTOCOL_NONE,
    .id_table = NULL,
    .class_driver = &cdc_ecm_class_driver
};
