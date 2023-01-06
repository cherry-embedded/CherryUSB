/*
 * Copyright (C) 2006 Bertrik Sikken (bertrik@sikken.nl)
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#ifdef CONFIG_USBDEV_TX_RX_THREAD
#include "usb_osal.h"
#endif

/* general descriptor field offsets */
#define DESC_bLength         0 /** Length offset */
#define DESC_bDescriptorType 1 /** Descriptor type offset */

/* config descriptor field offsets */
#define CONF_DESC_wTotalLength        2 /** Total length offset */
#define CONF_DESC_bConfigurationValue 5 /** Configuration value offset */
#define CONF_DESC_bmAttributes        7 /** configuration characteristics */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber  2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting 3 /** Alternate setting offset */

#define USB_EP_OUT_NUM 8
#define USB_EP_IN_NUM  8

struct usbd_tx_rx_msg {
    uint8_t ep;
    uint32_t nbytes;
    usbd_endpoint_callback cb;
};

USB_NOCACHE_RAM_SECTION struct usbd_core_cfg_priv {
    /** Setup packet */
    USB_MEM_ALIGNX struct usb_setup_packet setup;
    /** Pointer to data buffer */
    uint8_t *ep0_data_buf;
    /** Remaining bytes in buffer */
    uint32_t ep0_data_buf_residue;
    /** Total length of control transfer */
    uint32_t ep0_data_buf_len;
    /** Zero length packet flag of control transfer */
    bool zlp_flag;
    /** Pointer to registered descriptors */
#if defined(CHERRYUSB_VERSION) && (CHERRYUSB_VERSION > 0x000700)
    struct usb_descriptor *descriptors;
#else
    const uint8_t *descriptors;
#endif
    /* Buffer used for storing standard, class and vendor request data */
    USB_MEM_ALIGNX uint8_t req_data[CONFIG_USBDEV_REQUEST_BUFFER_LEN];

    /** Variable to check whether the usb has been configured */
    bool configured;
    /** Currently selected configuration */
    uint8_t configuration;
    uint8_t speed;
#ifdef CONFIG_USBDEV_TEST_MODE
    bool test_mode;
#endif
    uint8_t intf_offset;
} usbd_core_cfg;

usb_slist_t usbd_intf_head = USB_SLIST_OBJECT_INIT(usbd_intf_head);

static struct usb_msosv1_descriptor *msosv1_desc;
static struct usb_msosv2_descriptor *msosv2_desc;
static struct usb_bos_descriptor *bos_desc;

struct usbd_tx_rx_msg tx_msg[USB_EP_IN_NUM];
struct usbd_tx_rx_msg rx_msg[USB_EP_OUT_NUM];

#ifdef CONFIG_USBDEV_TX_RX_THREAD
usb_osal_mq_t usbd_tx_rx_mq;
usb_osal_thread_t usbd_tx_rx_thread;
#endif

static void usbd_class_event_notify_handler(uint8_t event, void *arg);

static void usbd_print_setup(struct usb_setup_packet *setup)
{
    USB_LOG_INFO("Setup: "
                 "bmRequestType 0x%02x, bRequest 0x%02x, wValue 0x%04x, wIndex 0x%04x, wLength 0x%04x\r\n",
                 setup->bmRequestType,
                 setup->bRequest,
                 setup->wValue,
                 setup->wIndex,
                 setup->wLength);
}

static bool is_device_configured(void)
{
    return (usbd_core_cfg.configuration != 0);
}

/**
 * @brief configure and enable endpoint
 *
 * This function sets endpoint configuration according to one specified in USB
 * endpoint descriptor and then enables it for data transfers.
 *
 * @param [in]  ep_desc Endpoint descriptor byte array
 *
 * @return true if successfully configured and enabled
 */
static bool usbd_set_endpoint(const struct usb_endpoint_descriptor *ep_desc)
{
    struct usbd_endpoint_cfg ep_cfg;

    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    USB_LOG_INFO("Open endpoint:0x%x type:%u mps:%u\r\n",
                 ep_cfg.ep_addr, ep_cfg.ep_type, ep_cfg.ep_mps);

    return usbd_ep_open(&ep_cfg) == 0 ? true : false;
}
/**
 * @brief Disable endpoint for transferring data
 *
 * This function cancels transfers that are associated with endpoint and
 * disabled endpoint itself.
 *
 * @param [in]  ep_desc Endpoint descriptor byte array
 *
 * @return true if successfully deconfigured and disabled
 */
static bool usbd_reset_endpoint(const struct usb_endpoint_descriptor *ep_desc)
{
    struct usbd_endpoint_cfg ep_cfg;

    ep_cfg.ep_addr = ep_desc->bEndpointAddress;
    ep_cfg.ep_mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;

    USB_LOG_INFO("Close endpoint:0x%x type:%u\r\n",
                 ep_cfg.ep_addr, ep_cfg.ep_type);

    return usbd_ep_close(ep_cfg.ep_addr) == 0 ? true : false;
}

/**
 * @brief get specified USB descriptor
 *
 * This function parses the list of installed USB descriptors and attempts
 * to find the specified USB descriptor.
 *
 * @param [in]  type_index Type and index of the descriptor
 * @param [out] data       Descriptor data
 * @param [out] len        Descriptor length
 *
 * @return true if the descriptor was found, false otherwise
 */
#if defined(CHERRYUSB_VERSION) && (CHERRYUSB_VERSION > 0x000700)
static bool usbd_get_descriptor(uint16_t type_index, uint8_t **data, uint32_t *len)
{
    uint8_t type = 0U;
    uint8_t index = 0U;
    bool found = true;
    uint8_t str_len = 0;

    type = HI_BYTE(type_index);
    index = LO_BYTE(type_index);

    switch (type) {
        case USB_DESCRIPTOR_TYPE_DEVICE:
            *data = (uint8_t *)usbd_core_cfg.descriptors->device_descriptor;
            *len = usbd_core_cfg.descriptors->device_descriptor[0];
            break;
        case USB_DESCRIPTOR_TYPE_CONFIGURATION:
            usbd_core_cfg.speed = usbd_get_port_speed(0);
            if (usbd_core_cfg.speed == USB_SPEED_HIGH) {
                if (usbd_core_cfg.descriptors->hs_config_descriptor[index]) {
                    *data = (uint8_t *)usbd_core_cfg.descriptors->hs_config_descriptor[index];
                    *len = (usbd_core_cfg.descriptors->hs_config_descriptor[index][CONF_DESC_wTotalLength] |
                            (usbd_core_cfg.descriptors->hs_config_descriptor[index][CONF_DESC_wTotalLength + 1] << 8));
                } else {
                    found = false;
                }
            } else {
                if (usbd_core_cfg.descriptors->fs_config_descriptor[index]) {
                    *data = (uint8_t *)usbd_core_cfg.descriptors->fs_config_descriptor[index];
                    *len = (usbd_core_cfg.descriptors->fs_config_descriptor[index][CONF_DESC_wTotalLength] |
                            (usbd_core_cfg.descriptors->fs_config_descriptor[index][CONF_DESC_wTotalLength + 1] << 8));
                } else {
                    found = false;
                }
            }

            break;
        case USB_DESCRIPTOR_TYPE_STRING:
            if (index == USB_STRING_LANGID_INDEX) {
                (*data)[0] = 0x04;
                (*data)[1] = 0x03;
                (*data)[2] = 0x09;
                (*data)[3] = 0x04;
                *len = 4;
            } else if (index == USB_OSDESC_STRING_DESC_INDEX) {
                if (usbd_core_cfg.descriptors->msosv1_descriptor) {
                    USB_LOG_INFO("read MS OS 1.0 descriptor string\r\n");
                    *data = usbd_core_cfg.descriptors->msosv1_descriptor->string;
                    *len = usbd_core_cfg.descriptors->msosv1_descriptor->string_len;
                } else {
                }
            } else {
                if (usbd_core_cfg.descriptors->string_descriptor[index - 1]) {
                    str_len = strlen((const char *)usbd_core_cfg.descriptors->string_descriptor[index - 1]);

                    (*data)[0] = str_len * 2 + 2;
                    (*data)[1] = 0x03;
                    for (uint16_t i = 0; i < str_len; i++) {
                        (*data)[i * 2 + 2] = usbd_core_cfg.descriptors->string_descriptor[index - 1][i];
                        (*data)[i * 2 + 3] = 0;
                    }

                    *len = str_len * 2 + 2;
                } else {
                    found = false;
                }
            }
            break;
        case USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
            if (usbd_core_cfg.descriptors->device_quality_descriptor) {
                *data = (uint8_t *)usbd_core_cfg.descriptors->device_quality_descriptor;
                *len = usbd_core_cfg.descriptors->device_quality_descriptor[0];
            } else {
                found = false;
            }

            break;
        case USB_DESCRIPTOR_TYPE_OTHER_SPEED:
            if (usbd_core_cfg.speed == USB_SPEED_HIGH) {
                if (usbd_core_cfg.descriptors->fs_other_speed_descriptor) {
                    *data = (uint8_t *)usbd_core_cfg.descriptors->fs_other_speed_descriptor;
                    *len = (usbd_core_cfg.descriptors->fs_other_speed_descriptor[CONF_DESC_wTotalLength] |
                            (usbd_core_cfg.descriptors->fs_other_speed_descriptor[CONF_DESC_wTotalLength] << 8));
                } else {
                    found = false;
                }
            } else {
                if (usbd_core_cfg.descriptors->hs_other_speed_descriptor) {
                    *data = (uint8_t *)usbd_core_cfg.descriptors->hs_other_speed_descriptor;
                    *len = (usbd_core_cfg.descriptors->hs_other_speed_descriptor[CONF_DESC_wTotalLength] |
                            (usbd_core_cfg.descriptors->hs_other_speed_descriptor[CONF_DESC_wTotalLength] << 8));
                } else {
                    found = false;
                }
            }
            break;

        case USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE:
            USB_LOG_INFO("read BOS descriptor string\r\n");
            break;

        default:
            found = false;
            break;
    }

    if (found == false) {
        /* nothing found */
        USB_LOG_ERR("descriptor <type:%x,index:%x> not found!\r\n", type, index);
    }
    return found;
}
#else
static bool usbd_get_descriptor(uint16_t type_index, uint8_t **data, uint32_t *len)
{
    uint8_t type = 0U;
    uint8_t index = 0U;
    uint8_t *p = NULL;
    uint32_t cur_index = 0U;
    bool found = false;

    type = HI_BYTE(type_index);
    index = LO_BYTE(type_index);

    if ((type == USB_DESCRIPTOR_TYPE_STRING) && (index == USB_OSDESC_STRING_DESC_INDEX)) {
        USB_LOG_INFO("read MS OS 2.0 descriptor string\r\n");

        if (!msosv1_desc) {
            return false;
        }

        *data = (uint8_t *)msosv1_desc->string;
        *len = msosv1_desc->string_len;

        return true;
    } else if (type == USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE) {
        USB_LOG_INFO("read BOS descriptor string\r\n");

        if (!bos_desc) {
            return false;
        }

        *data = bos_desc->string;
        *len = bos_desc->string_len;
        return true;
    }
    /*
     * Invalid types of descriptors,
     * see USB Spec. Revision 2.0, 9.4.3 Get Descriptor
     */
    else if ((type == USB_DESCRIPTOR_TYPE_INTERFACE) || (type == USB_DESCRIPTOR_TYPE_ENDPOINT) ||
#ifndef CONFIG_USB_HS
             (type > USB_DESCRIPTOR_TYPE_ENDPOINT)) {
#else
             (type > USB_DESCRIPTOR_TYPE_OTHER_SPEED)) {
#endif
        return false;
    }

    p = (uint8_t *)usbd_core_cfg.descriptors;

    cur_index = 0U;

    while (p[DESC_bLength] != 0U) {
        if (p[DESC_bDescriptorType] == type) {
            if (cur_index == index) {
                found = true;
                break;
            }

            cur_index++;
        }

        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

    if (found) {
        if ((type == USB_DESCRIPTOR_TYPE_CONFIGURATION) || ((type == USB_DESCRIPTOR_TYPE_OTHER_SPEED))) {
            /* configuration or other speed descriptor is an
             * exception, length is at offset 2 and 3
             */
            *len = (p[CONF_DESC_wTotalLength]) |
                   (p[CONF_DESC_wTotalLength + 1] << 8);
        } else {
            /* normally length is at offset 0 */
            *len = p[DESC_bLength];
        }
        memcpy(*data, p, *len);
    } else {
        /* nothing found */
        USB_LOG_ERR("descriptor <type:%x,index:%x> not found!\r\n", type, index);
    }

    return found;
}
#endif

/**
 * @brief set USB configuration
 *
 * This function configures the device according to the specified configuration
 * index and alternate setting by parsing the installed USB descriptor list.
 * A configuration index of 0 unconfigures the device.
 *
 * @param [in] config_index Configuration index
 * @param [in] alt_setting  Alternate setting number
 *
 * @return true if successfully configured false if error or unconfigured
 */
static bool usbd_set_configuration(uint8_t config_index, uint8_t alt_setting)
{
    uint8_t cur_alt_setting = 0xFF;
    uint8_t cur_config = 0xFF;
    bool found = false;
    uint8_t *p;
#if defined(CHERRYUSB_VERSION) && (CHERRYUSB_VERSION > 0x000700)
    if (usbd_core_cfg.speed == USB_SPEED_HIGH) {
        p = (uint8_t *)usbd_core_cfg.descriptors->hs_config_descriptor[0];
    } else {
        p = (uint8_t *)usbd_core_cfg.descriptors->fs_config_descriptor[0];
    }
#else
    p = (uint8_t *)usbd_core_cfg.descriptors;
#endif
    /* configure endpoints for this configuration/altsetting */
    while (p[DESC_bLength] != 0U) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_CONFIGURATION:
                /* remember current configuration index */
                cur_config = p[CONF_DESC_bConfigurationValue];

                if (cur_config == config_index) {
                    found = true;
                }

                break;

            case USB_DESCRIPTOR_TYPE_INTERFACE:
                /* remember current alternate setting */
                cur_alt_setting =
                    p[INTF_DESC_bAlternateSetting];
                break;

            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                if ((cur_config != config_index) ||
                    (cur_alt_setting != alt_setting)) {
                    break;
                }

                found = usbd_set_endpoint((struct usb_endpoint_descriptor *)p);
                break;

            default:
                break;
        }

        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

    return found;
}

/**
 * @brief set USB interface
 *
 * @param [in] iface Interface index
 * @param [in] alt_setting  Alternate setting number
 *
 * @return true if successfully configured false if error or unconfigured
 */
static bool usbd_set_interface(uint8_t iface, uint8_t alt_setting)
{
    const uint8_t *if_desc = NULL;
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t cur_alt_setting = 0xFF;
    uint8_t cur_iface = 0xFF;
    bool ret = false;
    uint8_t *p;
#if defined(CHERRYUSB_VERSION) && (CHERRYUSB_VERSION > 0x000700)
    if (usbd_core_cfg.speed == USB_SPEED_HIGH) {
        p = (uint8_t *)usbd_core_cfg.descriptors->hs_config_descriptor[0];
    } else {
        p = (uint8_t *)usbd_core_cfg.descriptors->fs_config_descriptor[0];
    }
#else
    p = (uint8_t *)usbd_core_cfg.descriptors;
#endif
    USB_LOG_DBG("iface %u alt_setting %u\r\n", iface, alt_setting);

    while (p[DESC_bLength] != 0U) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                /* remember current alternate setting */
                cur_alt_setting = p[INTF_DESC_bAlternateSetting];
                cur_iface = p[INTF_DESC_bInterfaceNumber];

                if (cur_iface == iface &&
                    cur_alt_setting == alt_setting) {
                    if_desc = (void *)p;
                }

                USB_LOG_DBG("Current iface %u alt setting %u",
                            cur_iface, cur_alt_setting);
                break;

            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                if (cur_iface == iface) {
                    ep_desc = (struct usb_endpoint_descriptor *)p;

                    if (cur_alt_setting != alt_setting) {
                        ret = usbd_reset_endpoint(ep_desc);
                    } else {
                        ret = usbd_set_endpoint(ep_desc);
                    }
                }

                break;

            default:
                break;
        }

        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

    usbd_class_event_notify_handler(USBD_EVENT_SET_INTERFACE, (void *)if_desc);

    return ret;
}

/**
 * @brief handle a standard device request
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] data     Data buffer
 * @param [in,out] len      Pointer to data length
 *
 * @return true if the request was handled successfully
 */
static bool usbd_std_device_req_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint16_t value = setup->wValue;
    bool ret = true;

    switch (setup->bRequest) {
        case USB_REQUEST_GET_STATUS:
            /* bit 0: self-powered */
            /* bit 1: remote wakeup */
            (*data)[0] = 0x00;
            (*data)[1] = 0x00;
            *len = 2;
            break;

        case USB_REQUEST_CLEAR_FEATURE:
        case USB_REQUEST_SET_FEATURE:
            if (value == USB_FEATURE_REMOTE_WAKEUP) {
            } else if (value == USB_FEATURE_TEST_MODE) {
#ifdef CONFIG_USBDEV_TEST_MODE
                usbd_core_cfg.test_mode = true;
                usbd_execute_test_mode(setup);
#endif
            }
            *len = 0;
            break;

        case USB_REQUEST_SET_ADDRESS:
            usbd_set_address(value);
            *len = 0;
            break;

        case USB_REQUEST_GET_DESCRIPTOR:
            ret = usbd_get_descriptor(value, data, len);
            break;

        case USB_REQUEST_SET_DESCRIPTOR:
            ret = false;
            break;

        case USB_REQUEST_GET_CONFIGURATION:
            *data = (uint8_t *)&usbd_core_cfg.configuration;
            *len = 1;
            break;

        case USB_REQUEST_SET_CONFIGURATION:
            value &= 0xFF;

            if (!usbd_set_configuration(value, 0)) {
                ret = false;
            } else {
                usbd_core_cfg.configuration = value;
                usbd_core_cfg.configured = true;
                usbd_class_event_notify_handler(USBD_EVENT_CONFIGURED, NULL);
                usbd_configure_done_callback();
            }
            *len = 0;
            break;

        case USB_REQUEST_GET_INTERFACE:
        case USB_REQUEST_SET_INTERFACE:
            ret = false;
            break;

        default:
            ret = false;
            break;
    }

    return ret;
}

/**
 * @brief handle a standard interface request
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] data     Data buffer
 * @param [in,out] len      Pointer to data length
 *
 * @return true if the request was handled successfully
 */
static bool usbd_std_interface_req_handler(struct usb_setup_packet *setup,
                                           uint8_t **data, uint32_t *len)
{
    uint8_t type = HI_BYTE(setup->wValue);
    uint8_t intf_num = LO_BYTE(setup->wIndex);
    bool ret = true;

    /* Only when device is configured, then interface requests can be valid. */
    if (!is_device_configured()) {
        return false;
    }

    switch (setup->bRequest) {
        case USB_REQUEST_GET_STATUS:
            (*data)[0] = 0x00;
            (*data)[1] = 0x00;
            *len = 2;
            break;

        case USB_REQUEST_GET_DESCRIPTOR:
            if (type == 0x22) { /* HID_DESCRIPTOR_TYPE_HID_REPORT */
                USB_LOG_INFO("read hid report descriptor\r\n");
                usb_slist_t *i;

                usb_slist_for_each(i, &usbd_intf_head)
                {
                    struct usbd_interface *intf = usb_slist_entry(i, struct usbd_interface, list);

                    if (intf->intf_num == intf_num) {
                        *data = (uint8_t *)intf->hid_report_descriptor;
                        *len = intf->hid_report_descriptor_len;
                        return true;
                    }
                }
            }
            ret = false;
            break;
        case USB_REQUEST_CLEAR_FEATURE:
        case USB_REQUEST_SET_FEATURE:
            ret = false;
            break;
        case USB_REQUEST_GET_INTERFACE:
            (*data)[0] = 0;
            *len = 1;
            break;

        case USB_REQUEST_SET_INTERFACE:
            usbd_set_interface(setup->wIndex, setup->wValue);
            *len = 0;
            break;

        default:
            ret = false;
            break;
    }

    return ret;
}

/**
 * @brief handle a standard endpoint request
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] data     Data buffer
 * @param [in,out] len      Pointer to data length
 *
 * @return true if the request was handled successfully
 */
static bool usbd_std_endpoint_req_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t ep = (uint8_t)setup->wIndex;
    bool ret = true;

    /* Only when device is configured, then endpoint requests can be valid. */
    if (!is_device_configured()) {
        return false;
    }

    switch (setup->bRequest) {
        case USB_REQUEST_GET_STATUS:
            (*data)[0] = 0x00;
            (*data)[1] = 0x00;
            *len = 2;
            break;
        case USB_REQUEST_CLEAR_FEATURE:
            if (setup->wValue == USB_FEATURE_ENDPOINT_HALT) {
                USB_LOG_ERR("ep:%02x clear halt\r\n", ep);

                usbd_ep_clear_stall(ep);
                break;
            } else {
                ret = false;
            }
            *len = 0;
            break;
        case USB_REQUEST_SET_FEATURE:
            if (setup->wValue == USB_FEATURE_ENDPOINT_HALT) {
                USB_LOG_ERR("ep:%02x set halt\r\n", ep);

                usbd_ep_set_stall(ep);
            } else {
                ret = false;
            }
            *len = 0;
            break;

        case USB_REQUEST_SYNCH_FRAME:
            ret = false;
            break;
        default:
            ret = false;
            break;
    }

    return ret;
}

/**
 * @brief handle standard requests (list in chapter 9)
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] data     Data buffer
 * @param [in,out] len      Pointer to data length
 *
 * @return true if the request was handled successfully
 */
static int usbd_standard_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    int rc = 0;

    switch (setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) {
        case USB_REQUEST_RECIPIENT_DEVICE:
            if (usbd_std_device_req_handler(setup, data, len) == false) {
                rc = -1;
            }

            break;

        case USB_REQUEST_RECIPIENT_INTERFACE:
            if (usbd_std_interface_req_handler(setup, data, len) == false) {
                rc = -1;
            }

            break;

        case USB_REQUEST_RECIPIENT_ENDPOINT:
            if (usbd_std_endpoint_req_handler(setup, data, len) == false) {
                rc = -1;
            }

            break;

        default:
            rc = -1;
            break;
    }

    return rc;
}

/**
 * @brief handler for class requests
 *
 * If a custom request handler was installed, this handler is called first.
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] data     Data buffer
 * @param [in,out] len      Pointer to data length
 *
 * @return true if the request was handled successfully
 */
static int usbd_class_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    usb_slist_t *i;
    if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_INTERFACE) {
        usb_slist_for_each(i, &usbd_intf_head)
        {
            struct usbd_interface *intf = usb_slist_entry(i, struct usbd_interface, list);

            if (intf->class_interface_handler && (intf->intf_num == (setup->wIndex & 0xFF))) {
                return intf->class_interface_handler(setup, data, len);
            }
        }
    } else if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_ENDPOINT) {
        usb_slist_for_each(i, &usbd_intf_head)
        {
            struct usbd_interface *intf = usb_slist_entry(i, struct usbd_interface, list);

            if (intf->class_endpoint_handler && (intf->intf_num == ((setup->wIndex >> 8) & 0xFF))) {
                return intf->class_endpoint_handler(setup, data, len);
            }
        }
    }
    return -1;
}

/**
 * @brief handler for vendor requests
 *
 * If a custom request handler was installed, this handler is called first.
 *
 * @param [in]     setup    The setup packet
 * @param [in,out] data     Data buffer
 * @param [in,out] len      Pointer to data length
 *
 * @return true if the request was handled successfully
 */
static int usbd_vendor_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
#if defined(CHERRYUSB_VERSION) && (CHERRYUSB_VERSION > 0x000700)
    if (usbd_core_cfg.descriptors->msosv1_descriptor) {
        if (setup->bRequest == usbd_core_cfg.descriptors->msosv1_descriptor->vendor_code) {
            switch (setup->wIndex) {
                case 0x04:
                    USB_LOG_INFO("get Compat ID\r\n");
                    *data = (uint8_t *)usbd_core_cfg.descriptors->msosv1_descriptor->compat_id;
                    *len = usbd_core_cfg.descriptors->msosv1_descriptor->compat_id_len;
                    return 0;
                case 0x05:
                    USB_LOG_INFO("get Compat id properties\r\n");
                    *data = (uint8_t *)usbd_core_cfg.descriptors->msosv1_descriptor->comp_id_property;
                    *len = usbd_core_cfg.descriptors->msosv1_descriptor->comp_id_property_len;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    } else if (usbd_core_cfg.descriptors->msosv2_descriptor) {
        if (setup->bRequest == usbd_core_cfg.descriptors->msosv2_descriptor->vendor_code) {
            switch (setup->wIndex) {
                case WINUSB_REQUEST_GET_DESCRIPTOR_SET:
                    USB_LOG_INFO("GET MS OS 2.0 Descriptor\r\n");
                    *data = (uint8_t *)usbd_core_cfg.descriptors->msosv2_descriptor->compat_id;
                    *len = usbd_core_cfg.descriptors->msosv2_descriptor->compat_id_len;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    }
#else
    if (msosv1_desc) {
        if (setup->bRequest == msosv1_desc->vendor_code) {
            switch (setup->wIndex) {
                case 0x04:
                    USB_LOG_INFO("get Compat ID\r\n");
                    *data = (uint8_t *)msosv1_desc->compat_id;
                    *len = msosv1_desc->compat_id_len;

                    return 0;
                case 0x05:
                    USB_LOG_INFO("get Compat id properties\r\n");
                    *data = (uint8_t *)msosv1_desc->comp_id_property;
                    *len = msosv1_desc->comp_id_property_len;

                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    } else if (msosv2_desc) {
        if (setup->bRequest == msosv2_desc->vendor_code) {
            switch (setup->wIndex) {
                case WINUSB_REQUEST_GET_DESCRIPTOR_SET:
                    USB_LOG_INFO("GET MS OS 2.0 Descriptor\r\n");
                    *data = (uint8_t *)msosv2_desc->compat_id;
                    *len = msosv2_desc->compat_id_len;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    }
#endif
    usb_slist_t *i;

    usb_slist_for_each(i, &usbd_intf_head)
    {
        struct usbd_interface *intf = usb_slist_entry(i, struct usbd_interface, list);

        if (intf->vendor_handler && !intf->vendor_handler(setup, data, len)) {
            return 0;
        }
    }

    return -1;
}

/**
 * @brief handle setup request( standard/class/vendor/other)
 *
 * @param [in]     setup The setup packet
 * @param [in,out] data  Data buffer
 * @param [in,out] len   Pointer to data length
 *
 * @return true if the request was handles successfully
 */
static bool usbd_setup_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    switch (setup->bmRequestType & USB_REQUEST_TYPE_MASK) {
        case USB_REQUEST_STANDARD:
            if (usbd_standard_request_handler(setup, data, len) < 0) {
#ifndef CONFIG_USB_HS
                if ((setup->bRequest == 0x06) && (setup->wValue = 0x0600)) {
                    USB_LOG_WRN("Ignore device quality in full speed\r\n");
                    return false;
                }
#endif
                USB_LOG_ERR("standard request error\r\n");
                usbd_print_setup(setup);
                return false;
            }
            break;
        case USB_REQUEST_CLASS:
            if (usbd_class_request_handler(setup, data, len) < 0) {
                USB_LOG_ERR("class request error\r\n");
                usbd_print_setup(setup);
                return false;
            }
            break;
        case USB_REQUEST_VENDOR:
            if (usbd_vendor_request_handler(setup, data, len) < 0) {
                USB_LOG_ERR("vendor request error\r\n");
                usbd_print_setup(setup);
                return false;
            }
            break;

        default:
            return false;
    }

    return true;
}

static void usbd_class_event_notify_handler(uint8_t event, void *arg)
{
    usb_slist_t *i;
    usb_slist_for_each(i, &usbd_intf_head)
    {
        struct usbd_interface *intf = usb_slist_entry(i, struct usbd_interface, list);

        if (intf->notify_handler) {
            intf->notify_handler(event, arg);
        }
    }
}

void usbd_event_connect_handler(void)
{
    usbd_class_event_notify_handler(USBD_EVENT_CONNECTED, NULL);
}

void usbd_event_disconnect_handler(void)
{
    usbd_class_event_notify_handler(USBD_EVENT_DISCONNECTED, NULL);
}

void usbd_event_resume_handler(void)
{
    usbd_class_event_notify_handler(USBD_EVENT_RESUME, NULL);
}

void usbd_event_suspend_handler(void)
{
    usbd_class_event_notify_handler(USBD_EVENT_SUSPEND, NULL);
}

void usbd_event_reset_handler(void)
{
    usbd_set_address(0);
    usbd_core_cfg.configured = 0;
    usbd_core_cfg.configuration = 0;

#ifdef CONFIG_USBDEV_TEST_MODE
    usbd_core_cfg.test_mode = false;
#endif
    struct usbd_endpoint_cfg ep0_cfg;

    ep0_cfg.ep_mps = USB_CTRL_EP_MPS;
    ep0_cfg.ep_type = USB_ENDPOINT_TYPE_CONTROL;
    ep0_cfg.ep_addr = USB_CONTROL_IN_EP0;
    usbd_ep_open(&ep0_cfg);

    ep0_cfg.ep_addr = USB_CONTROL_OUT_EP0;
    usbd_ep_open(&ep0_cfg);

    usbd_class_event_notify_handler(USBD_EVENT_RESET, NULL);
}

void usbd_event_ep0_setup_complete_handler(uint8_t *psetup)
{
    struct usb_setup_packet *setup = &usbd_core_cfg.setup;

    memcpy(setup, psetup, 8);
#ifdef CONFIG_USBDEV_SETUP_LOG_PRINT
    usbd_print_setup(setup);
#endif
    if (setup->wLength > CONFIG_USBDEV_REQUEST_BUFFER_LEN) {
        if ((setup->bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT) {
            USB_LOG_ERR("Request buffer too small\r\n");
            usbd_ep_set_stall(USB_CONTROL_IN_EP0);
            return;
        }
    }

    usbd_core_cfg.ep0_data_buf = usbd_core_cfg.req_data;
    usbd_core_cfg.ep0_data_buf_residue = setup->wLength;
    usbd_core_cfg.ep0_data_buf_len = setup->wLength;
    usbd_core_cfg.zlp_flag = false;

    /* handle class request when all the data is received */
    if (setup->wLength && ((setup->bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
        USB_LOG_DBG("Start reading %d bytes from ep0\r\n", setup->wLength);
        usbd_ep_start_read(USB_CONTROL_OUT_EP0, usbd_core_cfg.ep0_data_buf, setup->wLength);
        return;
    }

    /* Ask installed handler to process request */
    if (!usbd_setup_request_handler(setup, &usbd_core_cfg.ep0_data_buf, &usbd_core_cfg.ep0_data_buf_len)) {
        usbd_ep_set_stall(USB_CONTROL_IN_EP0);
        return;
    }
#ifdef CONFIG_USBDEV_TEST_MODE
    /* send status in test mode, so do not execute downward, just return */
    if (usbd_core_cfg.test_mode) {
        usbd_core_cfg.test_mode = false;
        return;
    }
#endif
    /* Send smallest of requested and offered length */
    usbd_core_cfg.ep0_data_buf_residue = MIN(usbd_core_cfg.ep0_data_buf_len, setup->wLength);
    if (usbd_core_cfg.ep0_data_buf_residue > CONFIG_USBDEV_REQUEST_BUFFER_LEN) {
        USB_LOG_ERR("Request buffer too small\r\n");
        return;
    }

    /* Send data or status to host */
    usbd_ep_start_write(USB_CONTROL_IN_EP0, usbd_core_cfg.ep0_data_buf, usbd_core_cfg.ep0_data_buf_residue);
    /*
    * Set ZLP flag when host asks for a bigger length and the data size is
    * multiplier of USB_CTRL_EP_MPS, to indicate the transfer done after zlp
    * sent.
    */
    if ((setup->wLength > usbd_core_cfg.ep0_data_buf_len) && (!(usbd_core_cfg.ep0_data_buf_len % USB_CTRL_EP_MPS))) {
        usbd_core_cfg.zlp_flag = true;
        USB_LOG_DBG("EP0 Set zlp\r\n");
    }
}

void usbd_event_ep0_in_complete_handler(uint8_t ep, uint32_t nbytes)
{
    struct usb_setup_packet *setup = &usbd_core_cfg.setup;

    usbd_core_cfg.ep0_data_buf += nbytes;
    usbd_core_cfg.ep0_data_buf_residue -= nbytes;

    USB_LOG_DBG("EP0 send %d bytes, %d remained\r\n", nbytes, usbd_core_cfg.ep0_data_buf_residue);

    if (usbd_core_cfg.ep0_data_buf_residue != 0) {
        /* Start sending the remain data */
        usbd_ep_start_write(USB_CONTROL_IN_EP0, usbd_core_cfg.ep0_data_buf, usbd_core_cfg.ep0_data_buf_residue);
    } else {
        if (usbd_core_cfg.zlp_flag == true) {
            usbd_core_cfg.zlp_flag = false;
            /* Send zlp to host */
            USB_LOG_DBG("EP0 Send zlp\r\n");
            usbd_ep_start_write(USB_CONTROL_IN_EP0, NULL, 0);
        } else {
            /* Satisfying three conditions will jump here.
                * 1. send status completely
                * 2. send zlp completely
                * 3. send last data completely.
                */
            if (setup->wLength && ((setup->bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_IN)) {
                /* if all data has sent completely, start reading out status */
                usbd_ep_start_read(USB_CONTROL_OUT_EP0, NULL, 0);
            }
        }
    }
}

void usbd_event_ep0_out_complete_handler(uint8_t ep, uint32_t nbytes)
{
    struct usb_setup_packet *setup = &usbd_core_cfg.setup;

    if (nbytes > 0) {
        usbd_core_cfg.ep0_data_buf += nbytes;
        usbd_core_cfg.ep0_data_buf_residue -= nbytes;

        USB_LOG_DBG("EP0 recv %d bytes, %d remained\r\n", nbytes, usbd_core_cfg.ep0_data_buf_residue);

        if (usbd_core_cfg.ep0_data_buf_residue == 0) {
            /* Received all, send data to handler */
            usbd_core_cfg.ep0_data_buf = usbd_core_cfg.req_data;
            if (!usbd_setup_request_handler(setup, &usbd_core_cfg.ep0_data_buf, &usbd_core_cfg.ep0_data_buf_len)) {
                usbd_ep_set_stall(USB_CONTROL_IN_EP0);
                return;
            }

            /*Send status to host*/
            usbd_ep_start_write(USB_CONTROL_IN_EP0, NULL, 0);
        } else {
            /* Start reading the remain data */
            usbd_ep_start_read(USB_CONTROL_OUT_EP0, usbd_core_cfg.ep0_data_buf, usbd_core_cfg.ep0_data_buf_residue);
        }
    } else {
        /* Read out status completely, do nothing */
        USB_LOG_DBG("EP0 recv out status\r\n");
    }
}

void usbd_event_ep_in_complete_handler(uint8_t ep, uint32_t nbytes)
{
#ifndef CONFIG_USBDEV_TX_RX_THREAD
    if (tx_msg[ep & 0x7f].cb) {
        tx_msg[ep & 0x7f].cb(ep, nbytes);
    }
#else
    tx_msg[ep & 0x7f].nbytes = nbytes;
    usb_osal_mq_send(usbd_tx_rx_mq, (uint32_t)&tx_msg[ep & 0x7f]);
#endif
}

void usbd_event_ep_out_complete_handler(uint8_t ep, uint32_t nbytes)
{
#ifndef CONFIG_USBDEV_TX_RX_THREAD
    if (rx_msg[ep & 0x7f].cb) {
        rx_msg[ep & 0x7f].cb(ep, nbytes);
    }
#else
    rx_msg[ep & 0x7f].nbytes = nbytes;
    usb_osal_mq_send(usbd_tx_rx_mq, (uint32_t)&rx_msg[ep & 0x7f]);
#endif
}

#ifdef CONFIG_USBDEV_TX_RX_THREAD
static void usbdev_tx_rx_thread(void *argument)
{
    struct usbd_tx_rx_msg *msg;
    int ret;

    while (1) {
        ret = usb_osal_mq_recv(usbd_tx_rx_mq, (uint32_t *)&msg, 0xffffffff);
        if (ret < 0) {
            continue;
        }

        if (msg->cb) {
            msg->cb(msg->ep, msg->nbytes);
        }
    }
}
#endif

#if defined(CHERRYUSB_VERSION) && (CHERRYUSB_VERSION > 0x000700)
void usbd_desc_register(struct usb_descriptor *desc)
{
    usbd_core_cfg.descriptors = desc;
    usbd_core_cfg.intf_offset = 0;
    tx_msg[0].ep = 0x80;
    tx_msg[0].cb = usbd_event_ep0_in_complete_handler;
    rx_msg[0].ep = 0x00;
    rx_msg[0].cb = usbd_event_ep0_out_complete_handler;
}
#else
void usbd_desc_register(const uint8_t *desc)
{
    usbd_core_cfg.descriptors = desc;
    usbd_core_cfg.intf_offset = 0;
    tx_msg[0].ep = 0x80;
    tx_msg[0].cb = usbd_event_ep0_in_complete_handler;
    rx_msg[0].ep = 0x00;
    rx_msg[0].cb = usbd_event_ep0_out_complete_handler;
}

/* Register MS OS Descriptors version 1 */
void usbd_msosv1_desc_register(struct usb_msosv1_descriptor *desc)
{
    msosv1_desc = desc;
}

/* Register MS OS Descriptors version 2 */
void usbd_msosv2_desc_register(struct usb_msosv2_descriptor *desc)
{
    msosv2_desc = desc;
}

void usbd_bos_desc_register(struct usb_bos_descriptor *desc)
{
    bos_desc = desc;
}
#endif

void usbd_add_interface(struct usbd_interface *intf)
{
    intf->intf_num = usbd_core_cfg.intf_offset;
    usb_slist_add_tail(&usbd_intf_head, &intf->list);
    usbd_core_cfg.intf_offset++;
}

void usbd_add_endpoint(struct usbd_endpoint *ep)
{
    if (ep->ep_addr & 0x80) {
        tx_msg[ep->ep_addr & 0x7f].ep = ep->ep_addr;
        tx_msg[ep->ep_addr & 0x7f].cb = ep->ep_cb;
    } else {
        rx_msg[ep->ep_addr & 0x7f].ep = ep->ep_addr;
        rx_msg[ep->ep_addr & 0x7f].cb = ep->ep_cb;
    }
}

bool usb_device_is_configured(void)
{
    return usbd_core_cfg.configured;
}

int usbd_initialize(void)
{
#ifdef CONFIG_USBDEV_TX_RX_THREAD
    usbd_tx_rx_mq = usb_osal_mq_create(32);
    if (usbd_tx_rx_mq == NULL) {
        return -1;
    }
    usbd_tx_rx_thread = usb_osal_thread_create("usbd_tx_rx", CONFIG_USBDEV_TX_RX_STACKSIZE, CONFIG_USBDEV_TX_RX_PRIO, usbdev_tx_rx_thread, NULL);
    if (usbd_tx_rx_thread == NULL) {
        return -1;
    }
#endif
    return usb_dc_init();
}

int usbd_deinitialize(void)
{
    usbd_core_cfg.intf_offset = 0;
    usb_slist_init(&usbd_intf_head);
    usb_dc_deinit();
#ifdef CONFIG_USBDEV_TX_RX_THREAD
#endif
    return 0;
}
