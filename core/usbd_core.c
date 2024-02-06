/*
 * Copyright (C) 2006 Bertrik Sikken (bertrik@sikken.nl)
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"

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

#define USB_EP_OUT_NUM 16
#define USB_EP_IN_NUM  16

struct usbd_tx_rx_msg {
    uint8_t ep;
    uint32_t nbytes;
    usbd_endpoint_callback cb;
};

USB_NOCACHE_RAM_SECTION struct usbd_core_priv {
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
#ifdef CONFIG_USBDEV_ADVANCE_DESC
    const struct usb_descriptor *descriptors;
#else
    const uint8_t *descriptors;
    struct usb_msosv1_descriptor *msosv1_desc;
    struct usb_msosv2_descriptor *msosv2_desc;
    struct usb_bos_descriptor *bos_desc;
#endif
    /* Buffer used for storing standard, class and vendor request data */
    USB_MEM_ALIGNX uint8_t req_data[CONFIG_USBDEV_REQUEST_BUFFER_LEN];

    /** Currently selected configuration */
    uint8_t configuration;
    uint8_t speed;
#ifdef CONFIG_USBDEV_TEST_MODE
    bool test_mode;
#endif
    struct usbd_interface *intf[8];
    uint8_t intf_offset;

    struct usbd_tx_rx_msg tx_msg[USB_EP_IN_NUM];
    struct usbd_tx_rx_msg rx_msg[USB_EP_OUT_NUM];
} g_usbd_core;

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
    return (g_usbd_core.configuration != 0);
}

/**
 * @brief configure and enable endpoint
 *
 * This function sets endpoint configuration according to one specified in USB
 * endpoint descriptor and then enables it for data transfers.
 *
 * @param [in]  ep Endpoint descriptor byte array
 *
 * @return true if successfully configured and enabled
 */
static bool usbd_set_endpoint(const struct usb_endpoint_descriptor *ep)
{
    USB_LOG_INFO("Open ep:0x%02x type:%u mps:%u\r\n",
                 ep->bEndpointAddress,
                 USB_GET_ENDPOINT_TYPE(ep->bmAttributes),
                 USB_GET_MAXPACKETSIZE(ep->wMaxPacketSize));

    return usbd_ep_open(ep) == 0 ? true : false;
}
/**
 * @brief Disable endpoint for transferring data
 *
 * This function cancels transfers that are associated with endpoint and
 * disabled endpoint itself.
 *
 * @param [in]  ep Endpoint descriptor byte array
 *
 * @return true if successfully deconfigured and disabled
 */
static bool usbd_reset_endpoint(const struct usb_endpoint_descriptor *ep)
{
    USB_LOG_INFO("Close ep:0x%02x type:%u\r\n",
                 ep->bEndpointAddress,
                 USB_GET_ENDPOINT_TYPE(ep->bmAttributes));

    return usbd_ep_close(ep->bEndpointAddress) == 0 ? true : false;
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
#ifdef CONFIG_USBDEV_ADVANCE_DESC
static bool usbd_get_descriptor(uint16_t type_index, uint8_t **data, uint32_t *len)
{
    uint8_t type = 0U;
    uint8_t index = 0U;
    bool found = true;
    uint32_t desc_len = 0;
    const uint8_t *desc = NULL;

    type = HI_BYTE(type_index);
    index = LO_BYTE(type_index);

    switch (type) {
        case USB_DESCRIPTOR_TYPE_DEVICE:
            desc = g_usbd_core.descriptors->device_descriptor_callback(g_usbd_core.speed);
            if (desc == NULL) {
                found = false;
                break;
            }
            desc_len = desc[0];
            break;
        case USB_DESCRIPTOR_TYPE_CONFIGURATION:
            desc = g_usbd_core.descriptors->config_descriptor_callback(g_usbd_core.speed);
            if (desc == NULL) {
                found = false;
                break;
            }
            desc_len = ((desc[CONF_DESC_wTotalLength]) | (desc[CONF_DESC_wTotalLength + 1] << 8));
            break;
        case USB_DESCRIPTOR_TYPE_STRING:
            if (index == USB_OSDESC_STRING_DESC_INDEX) {
                USB_LOG_INFO("read MS OS 2.0 descriptor string\r\n");

                if (!g_usbd_core.descriptors->msosv1_descriptor) {
                    found = false;
                    break;
                }

                desc = (uint8_t *)g_usbd_core.descriptors->msosv1_descriptor->string;
                desc_len = g_usbd_core.descriptors->msosv1_descriptor->string[0];
            } else {
                desc = g_usbd_core.descriptors->string_descriptor_callback(g_usbd_core.speed, index);
                if (desc == NULL) {
                    found = false;
                    break;
                }
                desc_len = desc[0];
            }
            break;
        case USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
            desc = g_usbd_core.descriptors->device_quality_descriptor_callback(g_usbd_core.speed);
            if (desc == NULL) {
                found = false;
                break;
            }
            desc_len = desc[0];
            break;
        case USB_DESCRIPTOR_TYPE_OTHER_SPEED:
            desc = g_usbd_core.descriptors->other_speed_descriptor_callback(g_usbd_core.speed);
            if (desc == NULL) {
                found = false;
                break;
            }
            desc_len = ((desc[CONF_DESC_wTotalLength]) | (desc[CONF_DESC_wTotalLength + 1] << 8));
            break;

        case USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE:
            USB_LOG_INFO("read BOS descriptor string\r\n");

            if (!g_usbd_core.descriptors->bos_descriptor) {
                found = false;
                break;
            }

            desc = (uint8_t *)g_usbd_core.descriptors->bos_descriptor->string;
            desc_len = g_usbd_core.descriptors->bos_descriptor->string_len;
            break;

        default:
            found = false;
            break;
    }

    if (found == false) {
        /* nothing found */
        USB_LOG_ERR("descriptor <type:%x,index:%x> not found!\r\n", type, index);
    } else {
        // *data = desc;
        memcpy(*data, desc, desc_len);
        *len = desc_len;
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

        if (!g_usbd_core.msosv1_desc) {
            return false;
        }

        //*data = (uint8_t *)g_usbd_core.msosv1_desc->string;
        memcpy(*data, (uint8_t *)g_usbd_core.msosv1_desc->string, g_usbd_core.msosv1_desc->string[0]);
        *len = g_usbd_core.msosv1_desc->string[0];

        return true;
    } else if (type == USB_DESCRIPTOR_TYPE_BINARY_OBJECT_STORE) {
        USB_LOG_INFO("read BOS descriptor string\r\n");

        if (!g_usbd_core.bos_desc) {
            return false;
        }

        //*data = g_usbd_core.bos_desc->string;
        memcpy(*data, (uint8_t *)g_usbd_core.bos_desc->string, g_usbd_core.bos_desc->string_len);
        *len = g_usbd_core.bos_desc->string_len;
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

    p = (uint8_t *)g_usbd_core.descriptors;

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
        USB_LOG_ERR("descriptor <type:0x%02x,index:0x%02x> not found!\r\n", type, index);
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
    const uint8_t *p;
    uint32_t desc_len = 0;
    uint32_t current_desc_len = 0;

#ifdef CONFIG_USBDEV_ADVANCE_DESC
    p = g_usbd_core.descriptors->config_descriptor_callback(g_usbd_core.speed);
#else
    p = (uint8_t *)g_usbd_core.descriptors;
#endif
    /* configure endpoints for this configuration/altsetting */
    while (p[DESC_bLength] != 0U) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_CONFIGURATION:
                /* remember current configuration index */
                cur_config = p[CONF_DESC_bConfigurationValue];

                if (cur_config == config_index) {
                    found = true;

                    current_desc_len = 0;
                    desc_len = (p[CONF_DESC_wTotalLength]) |
                               (p[CONF_DESC_wTotalLength + 1] << 8);
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
        current_desc_len += p[DESC_bLength];
        if (current_desc_len >= desc_len && desc_len) {
            break;
        }
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
    const uint8_t *p;
    uint32_t desc_len = 0;
    uint32_t current_desc_len = 0;

#ifdef CONFIG_USBDEV_ADVANCE_DESC
    p = g_usbd_core.descriptors->config_descriptor_callback(g_usbd_core.speed);
#else
    p = (uint8_t *)g_usbd_core.descriptors;
#endif
    USB_LOG_DBG("iface %u alt_setting %u\r\n", iface, alt_setting);

    while (p[DESC_bLength] != 0U) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_CONFIGURATION:
                current_desc_len = 0;
                desc_len = (p[CONF_DESC_wTotalLength]) |
                           (p[CONF_DESC_wTotalLength + 1] << 8);

                break;

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
        current_desc_len += p[DESC_bLength];
        if (current_desc_len >= desc_len && desc_len) {
            break;
        }
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
                if (setup->bRequest == USB_REQUEST_SET_FEATURE) {
                    usbd_event_handler(USBD_EVENT_SET_REMOTE_WAKEUP);
                } else {
                    usbd_event_handler(USBD_EVENT_CLR_REMOTE_WAKEUP);
                }
            } else if (value == USB_FEATURE_TEST_MODE) {
#ifdef CONFIG_USBDEV_TEST_MODE
                g_usbd_core.test_mode = true;
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
            *data = (uint8_t *)&g_usbd_core.configuration;
            *len = 1;
            break;

        case USB_REQUEST_SET_CONFIGURATION:
            value &= 0xFF;

            if (!usbd_set_configuration(value, 0)) {
                ret = false;
            } else {
                g_usbd_core.configuration = value;
                usbd_class_event_notify_handler(USBD_EVENT_CONFIGURED, NULL);
                usbd_event_handler(USBD_EVENT_CONFIGURED);
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

                for (uint8_t i = 0; i < g_usbd_core.intf_offset; i++) {
                    struct usbd_interface *intf = g_usbd_core.intf[i];

                    if (intf && (intf->intf_num == intf_num)) {
                        //*data = (uint8_t *)intf->hid_report_descriptor;
                        memcpy(*data, intf->hid_report_descriptor, intf->hid_report_descriptor_len);
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
    if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_INTERFACE) {
        for (uint8_t i = 0; i < g_usbd_core.intf_offset; i++) {
            struct usbd_interface *intf = g_usbd_core.intf[i];

            if (intf && intf->class_interface_handler && (intf->intf_num == (setup->wIndex & 0xFF))) {
                return intf->class_interface_handler(setup, data, len);
            }
        }
    } else if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_ENDPOINT) {
        for (uint8_t i = 0; i < g_usbd_core.intf_offset; i++) {
            struct usbd_interface *intf = g_usbd_core.intf[i];

            if (intf && intf->class_endpoint_handler) {
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
    uint32_t desclen;
#ifdef CONFIG_USBDEV_ADVANCE_DESC
    if (g_usbd_core.descriptors->msosv1_descriptor) {
        if (setup->bRequest == g_usbd_core.descriptors->msosv1_descriptor->vendor_code) {
            switch (setup->wIndex) {
                case 0x04:
                    USB_LOG_INFO("get Compat ID\r\n");
                    desclen = g_usbd_core.descriptors->msosv1_descriptor->compat_id[0] +
                              (g_usbd_core.descriptors->msosv1_descriptor->compat_id[1] << 8) +
                              (g_usbd_core.descriptors->msosv1_descriptor->compat_id[2] << 16) +
                              (g_usbd_core.descriptors->msosv1_descriptor->compat_id[3] << 24);

                    //*data = (uint8_t *)g_usbd_core.descriptors->msosv1_descriptor->compat_id;
                    memcpy(*data, g_usbd_core.descriptors->msosv1_descriptor->compat_id, desclen);
                    *len = desclen;
                    return 0;
                case 0x05:
                    USB_LOG_INFO("get Compat id properties\r\n");
                    desclen = g_usbd_core.descriptors->msosv1_descriptor->comp_id_property[setup->wValue][0] +
                              (g_usbd_core.descriptors->msosv1_descriptor->comp_id_property[setup->wValue][1] << 8) +
                              (g_usbd_core.descriptors->msosv1_descriptor->comp_id_property[setup->wValue][2] << 16) +
                              (g_usbd_core.descriptors->msosv1_descriptor->comp_id_property[setup->wValue][3] << 24);

                    //*data = (uint8_t *)g_usbd_core.descriptors->msosv1_descriptor->comp_id_property[setup->wValue];
                    memcpy(*data, g_usbd_core.descriptors->msosv1_descriptor->comp_id_property[setup->wValue], desclen);
                    *len = desclen;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    } else if (g_usbd_core.descriptors->msosv2_descriptor) {
        if (setup->bRequest == g_usbd_core.descriptors->msosv2_descriptor->vendor_code) {
            switch (setup->wIndex) {
                case WINUSB_REQUEST_GET_DESCRIPTOR_SET:
                    USB_LOG_INFO("GET MS OS 2.0 Descriptor\r\n");

                    desclen = g_usbd_core.descriptors->msosv2_descriptor->compat_id_len;
                    //*data = (uint8_t *)g_usbd_core.descriptors->msosv2_descriptor->compat_id;
                    memcpy(*data, g_usbd_core.descriptors->msosv2_descriptor->compat_id, desclen);
                    *len = g_usbd_core.descriptors->msosv2_descriptor->compat_id_len;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    } else if (g_usbd_core.descriptors->webusb_url_descriptor) {
        if (setup->bRequest == g_usbd_core.descriptors->webusb_url_descriptor->vendor_code) {
            switch (setup->wIndex) {
                case WINUSB_REQUEST_GET_DESCRIPTOR_SET:
                    USB_LOG_INFO("GET Webusb url Descriptor\r\n");

                    desclen = g_usbd_core.descriptors->webusb_url_descriptor->string_len;
                    //*data = (uint8_t *)g_usbd_core.descriptors->webusb_url_descriptor->string;
                    memcpy(*data, g_usbd_core.descriptors->webusb_url_descriptor->string, desclen);
                    *len = desclen;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    }
#else
    if (g_usbd_core.msosv1_desc) {
        if (setup->bRequest == g_usbd_core.msosv1_desc->vendor_code) {
            switch (setup->wIndex) {
                case 0x04:
                    USB_LOG_INFO("get Compat ID\r\n");
                    //*data = (uint8_t *)msosv1_desc->compat_id;
                    desclen = g_usbd_core.msosv1_desc->compat_id[0] +
                              (g_usbd_core.msosv1_desc->compat_id[1] << 8) +
                              (g_usbd_core.msosv1_desc->compat_id[2] << 16) +
                              (g_usbd_core.msosv1_desc->compat_id[3] << 24);
                    memcpy(*data, g_usbd_core.msosv1_desc->compat_id, desclen);
                    *len = desclen;
                    return 0;
                case 0x05:
                    USB_LOG_INFO("get Compat id properties\r\n");
                    //*data = (uint8_t *)msosv1_desc->comp_id_property[setup->wValue];
                    desclen = g_usbd_core.msosv1_desc->comp_id_property[setup->wValue][0] +
                              (g_usbd_core.msosv1_desc->comp_id_property[setup->wValue][1] << 8) +
                              (g_usbd_core.msosv1_desc->comp_id_property[setup->wValue][2] << 16) +
                              (g_usbd_core.msosv1_desc->comp_id_property[setup->wValue][3] << 24);
                    memcpy(*data, g_usbd_core.msosv1_desc->comp_id_property[setup->wValue], desclen);
                    *len = desclen;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    } else if (g_usbd_core.msosv2_desc) {
        if (setup->bRequest == g_usbd_core.msosv2_desc->vendor_code) {
            switch (setup->wIndex) {
                case WINUSB_REQUEST_GET_DESCRIPTOR_SET:
                    USB_LOG_INFO("GET MS OS 2.0 Descriptor\r\n");
                    //*data = (uint8_t *)msosv2_desc->compat_id;
                    memcpy(*data, g_usbd_core.msosv2_desc->compat_id, g_usbd_core.msosv2_desc->compat_id_len);
                    *len = g_usbd_core.msosv2_desc->compat_id_len;
                    return 0;
                default:
                    USB_LOG_ERR("unknown vendor code\r\n");
                    return -1;
            }
        }
    }
#endif
    for (uint8_t i = 0; i < g_usbd_core.intf_offset; i++) {
        struct usbd_interface *intf = g_usbd_core.intf[i];

        if (intf && intf->vendor_handler && (intf->vendor_handler(setup, data, len) == 0)) {
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
#ifndef CONFIG_USB_HS
            //g_usbd_core.speed = USB_SPEED_FULL; /* next time will support getting device speed */
            if ((setup->bRequest == 0x06) && (setup->wValue == 0x0600) && (g_usbd_core.speed <= USB_SPEED_FULL)) {
                USB_LOG_WRN("Ignore DQD in fs\r\n"); /* Device Qualifier Descriptor */
                return false;
            }
#endif
            if (usbd_standard_request_handler(setup, data, len) < 0) {
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
    for (uint8_t i = 0; i < g_usbd_core.intf_offset; i++) {
        struct usbd_interface *intf = g_usbd_core.intf[i];

        if (arg) {
            struct usb_interface_descriptor *desc = (struct usb_interface_descriptor *)arg;
            if (intf && intf->notify_handler && (desc->bInterfaceNumber == (intf->intf_num))) {
                intf->notify_handler(event, arg);
            }
        } else {
            if (intf && intf->notify_handler) {
                intf->notify_handler(event, arg);
            }
        }
    }
}

void usbd_event_connect_handler(void)
{
    usbd_event_handler(USBD_EVENT_CONNECTED);
}

void usbd_event_disconnect_handler(void)
{
    usbd_event_handler(USBD_EVENT_DISCONNECTED);
}

void usbd_event_resume_handler(void)
{
    usbd_event_handler(USBD_EVENT_RESUME);
}

void usbd_event_suspend_handler(void)
{
    usbd_event_handler(USBD_EVENT_SUSPEND);
}

void usbd_event_reset_handler(void)
{
    usbd_set_address(0);
    g_usbd_core.configuration = 0;

#ifdef CONFIG_USBDEV_TEST_MODE
    g_usbd_core.test_mode = false;
#endif
    struct usb_endpoint_descriptor ep0;

    ep0.bLength = 7;
    ep0.bDescriptorType = USB_DESCRIPTOR_TYPE_ENDPOINT;
    ep0.wMaxPacketSize = USB_CTRL_EP_MPS;
    ep0.bmAttributes = USB_ENDPOINT_TYPE_CONTROL;
    ep0.bEndpointAddress = USB_CONTROL_IN_EP0;
    ep0.bInterval = 0;
    usbd_ep_open(&ep0);

    ep0.bEndpointAddress = USB_CONTROL_OUT_EP0;
    usbd_ep_open(&ep0);

    usbd_class_event_notify_handler(USBD_EVENT_RESET, NULL);
    usbd_event_handler(USBD_EVENT_RESET);
}

void usbd_event_ep0_setup_complete_handler(uint8_t *psetup)
{
    struct usb_setup_packet *setup = &g_usbd_core.setup;

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

    g_usbd_core.ep0_data_buf = g_usbd_core.req_data;
    g_usbd_core.ep0_data_buf_residue = setup->wLength;
    g_usbd_core.ep0_data_buf_len = setup->wLength;
    g_usbd_core.zlp_flag = false;

    /* handle class request when all the data is received */
    if (setup->wLength && ((setup->bmRequestType & USB_REQUEST_DIR_MASK) == USB_REQUEST_DIR_OUT)) {
        USB_LOG_DBG("Start reading %d bytes from ep0\r\n", setup->wLength);
        usbd_ep_start_read(USB_CONTROL_OUT_EP0, g_usbd_core.ep0_data_buf, setup->wLength);
        return;
    }

    /* Ask installed handler to process request */
    if (!usbd_setup_request_handler(setup, &g_usbd_core.ep0_data_buf, &g_usbd_core.ep0_data_buf_len)) {
        usbd_ep_set_stall(USB_CONTROL_IN_EP0);
        return;
    }
#ifdef CONFIG_USBDEV_TEST_MODE
    /* send status in test mode, so do not execute downward, just return */
    if (g_usbd_core.test_mode) {
        g_usbd_core.test_mode = false;
        return;
    }
#endif
    /* Send smallest of requested and offered length */
    g_usbd_core.ep0_data_buf_residue = MIN(g_usbd_core.ep0_data_buf_len, setup->wLength);
    if (g_usbd_core.ep0_data_buf_residue > CONFIG_USBDEV_REQUEST_BUFFER_LEN) {
        USB_LOG_ERR("Request buffer too small\r\n");
        return;
    }

    /* Send data or status to host */
    usbd_ep_start_write(USB_CONTROL_IN_EP0, g_usbd_core.ep0_data_buf, g_usbd_core.ep0_data_buf_residue);
    /*
    * Set ZLP flag when host asks for a bigger length and the data size is
    * multiplier of USB_CTRL_EP_MPS, to indicate the transfer done after zlp
    * sent.
    */
    if ((setup->wLength > g_usbd_core.ep0_data_buf_len) && (!(g_usbd_core.ep0_data_buf_len % USB_CTRL_EP_MPS))) {
        g_usbd_core.zlp_flag = true;
        USB_LOG_DBG("EP0 Set zlp\r\n");
    }
}

void usbd_event_ep0_in_complete_handler(uint8_t ep, uint32_t nbytes)
{
    struct usb_setup_packet *setup = &g_usbd_core.setup;

    g_usbd_core.ep0_data_buf += nbytes;
    g_usbd_core.ep0_data_buf_residue -= nbytes;

    USB_LOG_DBG("EP0 send %d bytes, %d remained\r\n", nbytes, g_usbd_core.ep0_data_buf_residue);

    if (g_usbd_core.ep0_data_buf_residue != 0) {
        /* Start sending the remain data */
        usbd_ep_start_write(USB_CONTROL_IN_EP0, g_usbd_core.ep0_data_buf, g_usbd_core.ep0_data_buf_residue);
    } else {
        if (g_usbd_core.zlp_flag == true) {
            g_usbd_core.zlp_flag = false;
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
    struct usb_setup_packet *setup = &g_usbd_core.setup;

    if (nbytes > 0) {
        g_usbd_core.ep0_data_buf += nbytes;
        g_usbd_core.ep0_data_buf_residue -= nbytes;

        USB_LOG_DBG("EP0 recv %d bytes, %d remained\r\n", nbytes, g_usbd_core.ep0_data_buf_residue);

        if (g_usbd_core.ep0_data_buf_residue == 0) {
            /* Received all, send data to handler */
            g_usbd_core.ep0_data_buf = g_usbd_core.req_data;
            if (!usbd_setup_request_handler(setup, &g_usbd_core.ep0_data_buf, &g_usbd_core.ep0_data_buf_len)) {
                usbd_ep_set_stall(USB_CONTROL_IN_EP0);
                return;
            }

            /*Send status to host*/
            usbd_ep_start_write(USB_CONTROL_IN_EP0, NULL, 0);
        } else {
            /* Start reading the remain data */
            usbd_ep_start_read(USB_CONTROL_OUT_EP0, g_usbd_core.ep0_data_buf, g_usbd_core.ep0_data_buf_residue);
        }
    } else {
        /* Read out status completely, do nothing */
        USB_LOG_DBG("EP0 recv out status\r\n");
    }
}

void usbd_event_ep_in_complete_handler(uint8_t ep, uint32_t nbytes)
{
    if (g_usbd_core.tx_msg[ep & 0x7f].cb) {
        g_usbd_core.tx_msg[ep & 0x7f].cb(ep, nbytes);
    }
}

void usbd_event_ep_out_complete_handler(uint8_t ep, uint32_t nbytes)
{
    if (g_usbd_core.rx_msg[ep & 0x7f].cb) {
        g_usbd_core.rx_msg[ep & 0x7f].cb(ep, nbytes);
    }
}

#ifdef CONFIG_USBDEV_ADVANCE_DESC
void usbd_desc_register(const struct usb_descriptor *desc)
{
    memset(&g_usbd_core, 0, sizeof(struct usbd_core_priv));

    g_usbd_core.descriptors = desc;
    g_usbd_core.intf_offset = 0;

    g_usbd_core.tx_msg[0].ep = 0x80;
    g_usbd_core.tx_msg[0].cb = usbd_event_ep0_in_complete_handler;
    g_usbd_core.rx_msg[0].ep = 0x00;
    g_usbd_core.rx_msg[0].cb = usbd_event_ep0_out_complete_handler;
}
#else
void usbd_desc_register(const uint8_t *desc)
{
    memset(&g_usbd_core, 0, sizeof(struct usbd_core_priv));

    g_usbd_core.descriptors = desc;
    g_usbd_core.intf_offset = 0;

    g_usbd_core.tx_msg[0].ep = 0x80;
    g_usbd_core.tx_msg[0].cb = usbd_event_ep0_in_complete_handler;
    g_usbd_core.rx_msg[0].ep = 0x00;
    g_usbd_core.rx_msg[0].cb = usbd_event_ep0_out_complete_handler;
}

/* Register MS OS Descriptors version 1 */
void usbd_msosv1_desc_register(struct usb_msosv1_descriptor *desc)
{
    g_usbd_core.msosv1_desc = desc;
}

/* Register MS OS Descriptors version 2 */
void usbd_msosv2_desc_register(struct usb_msosv2_descriptor *desc)
{
    g_usbd_core.msosv2_desc = desc;
}

void usbd_bos_desc_register(struct usb_bos_descriptor *desc)
{
    g_usbd_core.bos_desc = desc;
}
#endif

void usbd_add_interface(struct usbd_interface *intf)
{
    intf->intf_num = g_usbd_core.intf_offset;
    g_usbd_core.intf[g_usbd_core.intf_offset] = intf;
    g_usbd_core.intf_offset++;
}

void usbd_add_endpoint(struct usbd_endpoint *ep)
{
    if (ep->ep_addr & 0x80) {
        g_usbd_core.tx_msg[ep->ep_addr & 0x7f].ep = ep->ep_addr;
        g_usbd_core.tx_msg[ep->ep_addr & 0x7f].cb = ep->ep_cb;
    } else {
        g_usbd_core.rx_msg[ep->ep_addr & 0x7f].ep = ep->ep_addr;
        g_usbd_core.rx_msg[ep->ep_addr & 0x7f].cb = ep->ep_cb;
    }
}

bool usb_device_is_configured(void)
{
    return g_usbd_core.configuration;
}

int usbd_initialize(void)
{
    int ret;

    ret = usb_dc_init();
    usbd_class_event_notify_handler(USBD_EVENT_INIT, NULL);
    return ret;
}

int usbd_deinitialize(void)
{
    g_usbd_core.intf_offset = 0;
    usb_dc_deinit();
    usbd_class_event_notify_handler(USBD_EVENT_DEINIT, NULL);
    return 0;
}

__WEAK void usbd_event_handler(uint8_t event)
{
    switch (event) {
        case USBD_EVENT_INIT:
            break;
        case USBD_EVENT_DEINIT:
            break;
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}
