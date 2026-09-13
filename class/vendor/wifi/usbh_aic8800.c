/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * AIC8800 USB Wi-Fi host class. This file stops at discovery and raw
 * bulk transport. Firmware, FMAC and lwIP stay in the application.
 */
#include "usbh_core.h"
#include "usbh_aic8800.h"

#ifdef CONFIG_USBHOST_AIC8800_ZEROCD
#include "usbh_msc.h"
#endif

#include <string.h>

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbh_aic8800"
#include "usb_log.h"

#define DEV_FORMAT "/dev/aic8800"

#define AIC8800_USB_INTERFACE_CLASS    0xffU
#define AIC8800_USB_INTERFACE_SUBCLASS 0xffU
#define AIC8800_USB_INTERFACE_PROTOCOL 0xffU

static struct usbh_aic8800 g_aic8800_class;

#ifdef CONFIG_USBHOST_AIC8800_ZEROCD
/*
 * 1111:1111 AIC8800D80 adapters initially expose a virtual CD-ROM.
 * The payload is a standard USB MSC CBW with vendor SCSI CDB
 * "FD 00 ... 00 F2". There is no data phase; the device normally
 * disconnects and comes back as a69c:8d80.
 */
static const uint8_t g_aic8800_zero_cd_cbw[31] = {
    0x55, 0x53, 0x42, 0x43,
    0x87, 0x65, 0x43, 0x21,
    0x00, 0x00, 0x00, 0x00,
    0x00,
    0x00,
    0x10,
    0xfd, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xf2
};

static struct usbh_msc_modeswitch_config g_aic8800_modeswitch[] = {
    {
        .name = "AIC8800D80 ZeroCD",
        .vid = 0x1111U,
        .pid = 0x1111U,
        .message_content = g_aic8800_zero_cd_cbw,
    },
    { 0 }
};
#endif

void usbh_aic8800_enable_zero_cd_modeswitch(void)
{
#ifdef CONFIG_USBHOST_AIC8800_ZEROCD
    usbh_msc_modeswitch_enable(g_aic8800_modeswitch);
    USB_LOG_INFO("1111:1111 ZeroCD mode switch registered\r\n");
#if !defined(CONFIG_USBHOST_MSC_MODESWITCH_NO_CSW) || \
    !defined(CONFIG_USBHOST_MSC_MODESWITCH_FORCE_REENUMERATE)
    USB_LOG_WRN("ZeroCD needs CONFIG_USBHOST_MSC_MODESWITCH_NO_CSW and FORCE_REENUMERATE\r\n");
#endif
#else
    USB_LOG_WRN("CONFIG_USBHOST_AIC8800_ZEROCD is not enabled\r\n");
#endif
}

static bool usbh_aic8800_product_is_runtime(uint16_t product_id)
{
    switch (product_id) {
        case USBH_AIC8800_PID_8801:
        case USBH_AIC8800_PID_DC:
        case USBH_AIC8800_PID_DW:
        case USBH_AIC8800_PID_D81:
        case USBH_AIC8800_PID_D83:
        case USBH_AIC8800_PID_D84:
        case USBH_AIC8800_PID_D85:
        case USBH_AIC8800_PID_D86:
        case USBH_AIC8800_PID_D88:
        case USBH_AIC8800_PID_D41:
        case USBH_AIC8800_PID_D81X2:
        case USBH_AIC8800_PID_D89X2:
            return true;
        default:
            return false;
    }
}

static enum usbh_aic8800_family usbh_aic8800_get_family(uint16_t vendor_id,
                                                        uint16_t product_id)
{
    if (vendor_id == USBH_AIC8800_VID) {
        if ((product_id == USBH_AIC8800_PID_8800) ||
            (product_id == USBH_AIC8800_PID_8801)) {
            return USBH_AIC8800_FAMILY_8800;
        }
        if ((product_id == USBH_AIC8800_PID_D40_BOOT) ||
            (product_id == USBH_AIC8800_PID_D41) ||
            (product_id == USBH_AIC8800_PID_D80_BOOT) ||
            ((product_id >= USBH_AIC8800_PID_D81) &&
             (product_id <= USBH_AIC8800_PID_D88))) {
            return USBH_AIC8800_FAMILY_D80;
        }
        if ((product_id == USBH_AIC8800_PID_DC) ||
            (product_id == USBH_AIC8800_PID_DW)) {
            return USBH_AIC8800_FAMILY_DC;
        }
    }

    if (vendor_id == USBH_AIC8800_VID_V2) {
        if ((product_id == USBH_AIC8800_PID_D80X2_BOOT) ||
            (product_id == USBH_AIC8800_PID_D81X2) ||
            (product_id == USBH_AIC8800_PID_D89X2)) {
            return USBH_AIC8800_FAMILY_D80X2;
        }
        if ((product_id >= USBH_AIC8800_PID_D81) &&
            (product_id <= USBH_AIC8800_PID_D88)) {
            return USBH_AIC8800_FAMILY_D80;
        }
    }
    return USBH_AIC8800_FAMILY_UNKNOWN;
}

static const char *usbh_aic8800_family_name(enum usbh_aic8800_family family)
{
    switch (family) {
        case USBH_AIC8800_FAMILY_8800:  return "AIC8800/AIC8801";
        case USBH_AIC8800_FAMILY_D80:   return "AIC8800D80/D40";
        case USBH_AIC8800_FAMILY_D80X2: return "AIC8800D80X2";
        case USBH_AIC8800_FAMILY_DC:    return "AIC8800DC/DW";
        default:                        return "unknown";
    }
}

struct usbh_aic8800 *usbh_aic8800_get_device(void)
{
    return g_aic8800_class.online ? &g_aic8800_class : NULL;
}

int usbh_aic8800_bulk_send(struct usbh_aic8800 *aic8800_class,
                           const void *data, uint32_t length,
                           uint32_t timeout_ms, bool message_pipe)
{
    struct usb_endpoint_descriptor *endpoint;
    int ret;

    if ((aic8800_class == NULL) || !aic8800_class->online ||
        (aic8800_class->hport == NULL) || !aic8800_class->hport->connected ||
        (data == NULL) || (length == 0U)) {
        return -USB_ERR_INVAL;
    }
    endpoint = (message_pipe && (aic8800_class->message_out != NULL)) ?
               aic8800_class->message_out : aic8800_class->data_out;
    if (endpoint == NULL) {
        return -USB_ERR_NODEV;
    }

    usbh_bulk_urb_fill(&aic8800_class->tx_urb, aic8800_class->hport, endpoint,
                       (uint8_t *)data, length, timeout_ms, NULL, NULL);
    ret = usbh_submit_urb(&aic8800_class->tx_urb);
    if (ret < 0) {
        return ret;
    }
    return (int)aic8800_class->tx_urb.actual_length;
}

int usbh_aic8800_bulk_receive(struct usbh_aic8800 *aic8800_class,
                              void *data, uint32_t capacity,
                              uint32_t timeout_ms, bool message_pipe)
{
    struct usb_endpoint_descriptor *endpoint;
    int ret;

    if ((aic8800_class == NULL) || !aic8800_class->online ||
        (aic8800_class->hport == NULL) || !aic8800_class->hport->connected ||
        (data == NULL) || (capacity == 0U)) {
        return -USB_ERR_INVAL;
    }
    endpoint = (message_pipe && (aic8800_class->message_in != NULL)) ?
               aic8800_class->message_in : aic8800_class->data_in;
    if (endpoint == NULL) {
        return -USB_ERR_NODEV;
    }

    usbh_bulk_urb_fill(&aic8800_class->rx_urb, aic8800_class->hport, endpoint,
                       (uint8_t *)data, capacity, timeout_ms, NULL, NULL);
    ret = usbh_submit_urb(&aic8800_class->rx_urb);
    if (ret < 0) {
        return ret;
    }
    return (int)aic8800_class->rx_urb.actual_length;
}

int usbh_aic8800_bulk_receive_async(struct usbh_aic8800 *aic8800_class,
                                    void *data, uint32_t capacity,
                                    usbh_complete_callback_t complete,
                                    void *arg, bool message_pipe)
{
    struct usb_endpoint_descriptor *endpoint;

    if ((aic8800_class == NULL) || !aic8800_class->online ||
        (aic8800_class->hport == NULL) || !aic8800_class->hport->connected ||
        (data == NULL) || (capacity == 0U) || (complete == NULL)) {
        return -USB_ERR_INVAL;
    }
    endpoint = (message_pipe && (aic8800_class->message_in != NULL)) ?
               aic8800_class->message_in : aic8800_class->data_in;
    if (endpoint == NULL) {
        return -USB_ERR_NODEV;
    }

    usbh_bulk_urb_fill(&aic8800_class->rx_urb, aic8800_class->hport, endpoint,
                       (uint8_t *)data, capacity, 0U, complete, arg);
    return usbh_submit_urb(&aic8800_class->rx_urb);
}

void usbh_aic8800_abort_receive(struct usbh_aic8800 *aic8800_class)
{
    if ((aic8800_class != NULL) && (aic8800_class->rx_urb.hcpriv != NULL)) {
        usbh_kill_urb(&aic8800_class->rx_urb);
    }
}

__WEAK void usbh_aic8800_run(struct usbh_aic8800 *aic8800_class)
{
    (void)aic8800_class;
}

__WEAK void usbh_aic8800_stop(struct usbh_aic8800 *aic8800_class)
{
    (void)aic8800_class;
}

static int usbh_aic8800_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_aic8800 *aic8800_class = &g_aic8800_class;
    struct usbh_interface_altsetting *setting;
    uint8_t index;

    if ((hport == NULL) || aic8800_class->online) {
        return -USB_ERR_BUSY;
    }

    memset(aic8800_class, 0, sizeof(*aic8800_class));
    aic8800_class->hport = hport;
    aic8800_class->intf = intf;
    aic8800_class->vendor_id = hport->device_desc.idVendor;
    aic8800_class->product_id = hport->device_desc.idProduct;
    aic8800_class->family = usbh_aic8800_get_family(aic8800_class->vendor_id,
                                                    aic8800_class->product_id);
    aic8800_class->runtime = usbh_aic8800_product_is_runtime(aic8800_class->product_id);

    setting = &hport->config.intf[intf].altsetting[0];
    for (index = 0U; index < setting->intf_desc.bNumEndpoints; index++) {
        struct usb_endpoint_descriptor *endpoint = &setting->ep[index].ep_desc;

        if (USB_GET_ENDPOINT_TYPE(endpoint->bmAttributes) !=
            USB_ENDPOINT_TYPE_BULK) {
            continue;
        }
        if ((endpoint->bEndpointAddress & USB_ENDPOINT_DIRECTION_MASK) != 0U) {
            if (aic8800_class->data_in == NULL) {
                USBH_EP_INIT(aic8800_class->data_in, endpoint);
            } else if (aic8800_class->message_in == NULL) {
                USBH_EP_INIT(aic8800_class->message_in, endpoint);
            }
        } else {
            if (aic8800_class->data_out == NULL) {
                USBH_EP_INIT(aic8800_class->data_out, endpoint);
            } else if (aic8800_class->message_out == NULL) {
                USBH_EP_INIT(aic8800_class->message_out, endpoint);
            }
        }
    }

    if ((aic8800_class->data_in == NULL) || (aic8800_class->data_out == NULL)) {
        USB_LOG_ERR("AIC %04x:%04x has no bulk endpoint pair\r\n",
                    aic8800_class->vendor_id, aic8800_class->product_id);
        memset(aic8800_class, 0, sizeof(*aic8800_class));
        return -USB_ERR_NODEV;
    }

    hport->config.intf[intf].priv = aic8800_class;
    strncpy(hport->config.intf[intf].devname, DEV_FORMAT,
            CONFIG_USBHOST_DEV_NAMELEN - 1U);
    hport->config.intf[intf].devname[CONFIG_USBHOST_DEV_NAMELEN - 1U] = '\0';
    aic8800_class->online = true;

    USB_LOG_INFO("Register AIC8800 Class:%s %04x:%04x, %s, %s\r\n",
                 hport->config.intf[intf].devname,
                 aic8800_class->vendor_id, aic8800_class->product_id,
                 usbh_aic8800_family_name(aic8800_class->family),
                 aic8800_class->runtime ? "runtime" : "boot-loader");
    USB_LOG_INFO("AIC pipes data=%02x/%02x message=%02x/%02x\r\n",
                 aic8800_class->data_in->bEndpointAddress,
                 aic8800_class->data_out->bEndpointAddress,
                 aic8800_class->message_in ? aic8800_class->message_in->bEndpointAddress : 0U,
                 aic8800_class->message_out ? aic8800_class->message_out->bEndpointAddress : 0U);

    usbh_aic8800_run(aic8800_class);
    return 0;
}

static int usbh_aic8800_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_aic8800 *aic8800_class;

    if (hport == NULL) {
        return 0;
    }
    aic8800_class = (struct usbh_aic8800 *)hport->config.intf[intf].priv;
    if (aic8800_class == NULL) {
        return 0;
    }

    aic8800_class->online = false;
    usbh_aic8800_stop(aic8800_class);
    if (aic8800_class->tx_urb.hcpriv != NULL) {
        usbh_kill_urb(&aic8800_class->tx_urb);
    }
    if (aic8800_class->rx_urb.hcpriv != NULL) {
        usbh_kill_urb(&aic8800_class->rx_urb);
    }
    USB_LOG_INFO("Unregister AIC8800 Class:%s\r\n", hport->config.intf[intf].devname);
    hport->config.intf[intf].priv = NULL;
    memset(aic8800_class, 0, sizeof(*aic8800_class));
    return 0;
}

static const uint16_t aic8800_id_table[][2] = {
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_8800 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_8801 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_DC },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_DW },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D40_BOOT },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D41 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D80_BOOT },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D81 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D83 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D84 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D85 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D86 },
    { USBH_AIC8800_VID,    USBH_AIC8800_PID_D88 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D81 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D83 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D84 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D85 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D86 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D88 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D80X2_BOOT },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D81X2 },
    { USBH_AIC8800_VID_V2, USBH_AIC8800_PID_D89X2 },
    { 0U, 0U }
};

static const struct usbh_class_driver aic8800_class_driver = {
    .driver_name = "aic8800",
    .connect = usbh_aic8800_connect,
    .disconnect = usbh_aic8800_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info aic8800_class_info = {
    .match_flags = USB_CLASS_MATCH_VID_PID |
                   USB_CLASS_MATCH_INTF_CLASS |
                   USB_CLASS_MATCH_INTF_SUBCLASS |
                   USB_CLASS_MATCH_INTF_PROTOCOL,
    .bInterfaceClass = AIC8800_USB_INTERFACE_CLASS,
    .bInterfaceSubClass = AIC8800_USB_INTERFACE_SUBCLASS,
    .bInterfaceProtocol = AIC8800_USB_INTERFACE_PROTOCOL,
    .bInterfaceNumber = 0,
    .id_table = aic8800_id_table,
    .class_driver = &aic8800_class_driver
};
