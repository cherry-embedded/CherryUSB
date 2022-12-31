/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_audio.h"

#define DEV_FORMAT "/dev/audio%d"

/* general descriptor field offsets */
#define DESC_bLength            0 /** Length offset */
#define DESC_bDescriptorType    1 /** Descriptor type offset */
#define DESC_bDescriptorSubType 2 /** Descriptor subtype offset */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber  2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting 3 /** Alternate setting offset */

static uint32_t g_devinuse = 0;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_audio_buf[128];

static int usbh_audio_devno_alloc(struct usbh_audio *audio_class)
{
    int devno;

    for (devno = 0; devno < 32; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            audio_class->minor = devno;
            return 0;
        }
    }

    return -EMFILE;
}

static void usbh_audio_devno_free(struct usbh_audio *audio_class)
{
    int devno = audio_class->minor;

    if (devno >= 0 && devno < 32) {
        g_devinuse &= ~(1 << devno);
    }
}

int usbh_audio_open(struct usbh_audio *audio_class, const char *name)
{
    struct usb_setup_packet *setup = &audio_class->hport->setup;
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t mult;
    uint16_t mps;
    int ret;
    uint8_t intf = 0xff;
    uint8_t altsetting = 1;

    if (audio_class->is_opened) {
        return -EMFILE;
    }

    for (size_t i = 0; i < audio_class->module_num; i++) {
        if (strcmp(name, audio_class->module[i].name) == 0) {
            intf = audio_class->module[i].data_intf;
        }
    }

    if (intf == 0xff) {
        return -ENODEV;
    }

    ep_desc = &audio_class->hport->config.intf[intf].altsetting[altsetting].ep[0].ep_desc;
    mult = (ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_MASK) >> USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_SHIFT;
    mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    if (ep_desc->bEndpointAddress & 0x80) {
        audio_class->isoin_mps = mps * (mult + 1);
        usbh_hport_activate_epx(&audio_class->isoin, audio_class->hport, ep_desc);
    } else {
        audio_class->isoout_mps = mps * (mult + 1);
        usbh_hport_activate_epx(&audio_class->isoout, audio_class->hport, ep_desc);
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_SET_INTERFACE;
    setup->wValue = 1;
    setup->wIndex = intf;
    setup->wLength = 0;

    ret = usbh_control_transfer(audio_class->hport->ep0, setup, NULL);
    if (ret < 0) {
        return ret;
    }

    USB_LOG_INFO("Open audio module :%s\r\n", name);
    audio_class->is_opened = false;
    return ret;
}

int usbh_audio_close(struct usbh_audio *audio_class, const char *name)
{
    struct usb_setup_packet *setup = &audio_class->hport->setup;
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint8_t intf = 0xff;
    uint8_t altsetting = 1;

    for (size_t i = 0; i < audio_class->module_num; i++) {
        if (strcmp(name, audio_class->module[i].name) == 0) {
            intf = audio_class->module[i].data_intf;
        }
    }

    if (intf == 0xff) {
        return -ENODEV;
    }

    ep_desc = &audio_class->hport->config.intf[intf].altsetting[altsetting].ep[0].ep_desc;
    if (ep_desc->bEndpointAddress & 0x80) {
        if (audio_class->isoin) {
            usbh_pipe_free(audio_class->isoin);
            audio_class->isoin = NULL;
        }
    } else {
        if (audio_class->isoout) {
            usbh_pipe_free(audio_class->isoout);
            audio_class->isoout = NULL;
        }
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_SET_INTERFACE;
    setup->wValue = 0;
    setup->wIndex = intf;
    setup->wLength = 0;

    ret = usbh_control_transfer(audio_class->hport->ep0, setup, NULL);
    if (ret < 0) {
        return ret;
    }

    USB_LOG_INFO("Close audio module :%s\r\n", name);
    audio_class->is_opened = false;
    return ret;
}

void usbh_audio_list_module(struct usbh_audio *audio_class)
{
    USB_LOG_INFO("============= Audio module information ===================\r\n");
    USB_LOG_INFO("bcdADC :%04x\r\n", audio_class->bcdADC);
    USB_LOG_INFO("Num of modules :%u\r\n", audio_class->module_num);

    for (uint8_t i = 0; i < audio_class->module_num; i++) {
        USB_LOG_INFO("  module name :%s\r\n", audio_class->module[i].name);
        USB_LOG_INFO("  module feature unit id :%d\r\n", audio_class->module[i].feature_unit_id);
        USB_LOG_INFO("  module channels :%u\r\n", audio_class->module[i].channels);
        //USB_LOG_INFO("    module format_type :%u\r\n",audio_class->module[i].format_type);
        USB_LOG_INFO("  module bitresolution :%u\r\n", audio_class->module[i].bitresolution);
        USB_LOG_INFO("  module sampfreq num :%u\r\n", audio_class->module[i].sampfreq_num);

        for (uint8_t j = 0; j < audio_class->module[i].sampfreq_num; j++) {
            USB_LOG_INFO("      module sampfreq :%d hz\r\n", audio_class->module[i].sampfreq[j]);
        }
    }

    USB_LOG_INFO("============= Audio module information ===================\r\n");
}

static int usbh_audio_ctrl_connect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret;
    uint8_t cur_iface = 0xff;
    uint8_t input_offset = 0;
    uint8_t output_offset = 0;
    uint8_t feature_unit_offset = 0;
    uint8_t format_offset = 0;
    uint8_t *p;

    struct usbh_audio *audio_class = usb_malloc(sizeof(struct usbh_audio));
    if (audio_class == NULL) {
        USB_LOG_ERR("Fail to alloc audio_class\r\n");
        return -ENOMEM;
    }

    memset(audio_class, 0, sizeof(struct usbh_audio));
    usbh_audio_devno_alloc(audio_class);
    audio_class->hport = hport;
    audio_class->ctrl_intf = intf;

    hport->config.intf[intf].priv = audio_class;

    p = hport->raw_config_desc;
    while (p[DESC_bLength]) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                cur_iface = p[INTF_DESC_bInterfaceNumber];
                break;
            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                break;
            case AUDIO_INTERFACE_DESCRIPTOR_TYPE:
                if (cur_iface == audio_class->ctrl_intf) {
                    switch (p[DESC_bDescriptorSubType]) {
                        case AUDIO_CONTROL_HEADER: {
                            struct audio_cs_if_ac_header_descriptor *desc = (struct audio_cs_if_ac_header_descriptor *)p;
                            audio_class->bcdADC = desc->bcdADC;
                            audio_class->bInCollection = desc->bInCollection;
                        } break;
                        case AUDIO_CONTROL_INPUT_TERMINAL: {
                            struct audio_cs_if_ac_input_terminal_descriptor *desc = (struct audio_cs_if_ac_input_terminal_descriptor *)p;

                            audio_class->module[input_offset].input_terminal_id = desc->bTerminalID;
                            audio_class->module[input_offset].input_terminal_type = desc->wTerminalType;
                            audio_class->module[input_offset].input_channel_config = desc->wChannelConfig;

                            if (desc->wTerminalType == AUDIO_TERMINAL_STREAMING) {
                                audio_class->module[input_offset].terminal_link_id = desc->bTerminalID;
                            }
                            if (desc->wTerminalType == AUDIO_INTERM_MIC) {
                                audio_class->module[input_offset].name = "mic";
                            }
                            input_offset++;
                        } break;
                            break;
                        case AUDIO_CONTROL_OUTPUT_TERMINAL: {
                            struct audio_cs_if_ac_output_terminal_descriptor *desc = (struct audio_cs_if_ac_output_terminal_descriptor *)p;
                            audio_class->module[output_offset].output_terminal_id = desc->bTerminalID;
                            audio_class->module[output_offset].output_terminal_type = desc->wTerminalType;
                            if (desc->wTerminalType == AUDIO_TERMINAL_STREAMING) {
                                audio_class->module[output_offset].terminal_link_id = desc->bTerminalID;
                            }
                            if (desc->wTerminalType == AUDIO_OUTTERM_SPEAKER) {
                                audio_class->module[output_offset].name = "speaker";
                            }
                            output_offset++;
                        } break;
                        case AUDIO_CONTROL_FEATURE_UNIT: {
                            struct audio_cs_if_ac_feature_unit_descriptor *desc = (struct audio_cs_if_ac_feature_unit_descriptor *)p;
                            audio_class->module[feature_unit_offset].feature_unit_id = desc->bUnitID;
                            audio_class->module[feature_unit_offset].feature_unit_controlsize = desc->bControlSize;

                            for (uint8_t j = 0; j < desc->bControlSize; j++) {
                                audio_class->module[feature_unit_offset].feature_unit_controls[j] = p[6 + j];
                            }
                            feature_unit_offset++;
                        } break;
                        case AUDIO_CONTROL_PROCESSING_UNIT:

                            break;
                        default:
                            break;
                    }
                } else {
                    switch (p[DESC_bDescriptorSubType]) {
                        case AUDIO_STREAMING_GENERAL:

                            break;
                        case AUDIO_STREAMING_FORMAT_TYPE: {
                            struct audio_cs_if_as_format_type_descriptor *desc = (struct audio_cs_if_as_format_type_descriptor *)p;

                            audio_class->module[format_offset].data_intf = cur_iface;
                            audio_class->module[format_offset].channels = desc->bNrChannels;
                            audio_class->module[format_offset].format_type = desc->bFormatType;
                            audio_class->module[format_offset].bitresolution = desc->bBitResolution;
                            audio_class->module[format_offset].sampfreq_num = desc->bSamFreqType;

                            for (uint8_t j = 0; j < desc->bSamFreqType; j++) {
                                audio_class->module[format_offset].sampfreq[j] = (uint32_t)(p[10 + j] << 16) |
                                                                                 (uint32_t)(p[9 + j] << 8) |
                                                                                 (uint32_t)(p[8 + j] << 0);
                            }
                            format_offset++;
                        } break;
                        default:
                            break;
                    }
                }
                break;
            default:
                break;
        }
        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

    if ((input_offset != output_offset) && (input_offset != feature_unit_offset) && (input_offset != format_offset)) {
        return -EINVAL;
    }

    audio_class->module_num = input_offset;

    for (size_t i = 0; i < audio_class->module_num; i++) {
        ret = usbh_audio_close(audio_class, audio_class->module[i].name);
        if (ret < 0) {
            USB_LOG_ERR("Fail to close audio module :%s\r\n", audio_class->module[i].name);
            return ret;
        }
    }

    usbh_audio_list_module(audio_class);

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, audio_class->minor);
    USB_LOG_INFO("Register Audio Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_audio_run(audio_class);
    return 0;
}

static int usbh_audio_ctrl_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_audio *audio_class = (struct usbh_audio *)hport->config.intf[intf].priv;

    if (audio_class) {
        usbh_audio_devno_free(audio_class);

        if (audio_class->isoin) {
            usbh_pipe_free(audio_class->isoin);
        }

        if (audio_class->isoout) {
            usbh_pipe_free(audio_class->isoout);
        }

        usbh_audio_stop(audio_class);
        memset(audio_class, 0, sizeof(struct usbh_audio));
        usb_free(audio_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister Audio Class:%s\r\n", hport->config.intf[intf].devname);
    }

    return ret;
}

static int usbh_audio_data_connect(struct usbh_hubport *hport, uint8_t intf)
{
    return 0;
}

static int usbh_audio_data_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    return 0;
}

__WEAK void usbh_audio_run(struct usbh_audio *audio_class)
{
}

__WEAK void usbh_audio_stop(struct usbh_audio *audio_class)
{
}

const struct usbh_class_driver audio_ctrl_class_driver = {
    .driver_name = "audio_ctrl",
    .connect = usbh_audio_ctrl_connect,
    .disconnect = usbh_audio_ctrl_disconnect
};

const struct usbh_class_driver audio_streaming_class_driver = {
    .driver_name = "audio_streaming",
    .connect = usbh_audio_data_connect,
    .disconnect = usbh_audio_data_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info audio_ctrl_intf_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS,
    .class = USB_DEVICE_CLASS_AUDIO,
    .subclass = AUDIO_SUBCLASS_AUDIOCONTROL,
    .protocol = 0x00,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &audio_ctrl_class_driver
};

CLASS_INFO_DEFINE const struct usbh_class_info audio_streaming_intf_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS,
    .class = USB_DEVICE_CLASS_AUDIO,
    .subclass = AUDIO_SUBCLASS_AUDIOSTREAMING,
    .protocol = 0x00,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &audio_streaming_class_driver
};