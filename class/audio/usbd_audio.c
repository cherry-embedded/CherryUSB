/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_audio.h"

#if CONFIG_USBDEV_AUDIO_VERSION < 0x0200
struct usbd_audio_volume_info {
    uint16_t vol_min;
    uint16_t vol_max;
    uint16_t vol_res;
    uint16_t vol_current;
};

struct usbd_audio_attribute_control {
    struct usbd_audio_volume_info volume[CONFIG_USBDEV_AUDIO_MAX_CHANNEL];
    uint8_t mute[CONFIG_USBDEV_AUDIO_MAX_CHANNEL];
    uint8_t automatic_gain[CONFIG_USBDEV_AUDIO_MAX_CHANNEL];
};
#else
struct audio_v2_control_range2_param_block_default {
    uint16_t wNumSubRanges;
    struct
    {
        uint16_t wMin;
        uint16_t wMax;
        uint16_t wRes;
    } subrange[CONFIG_USBDEV_AUDIO_MAX_CHANNEL];
} __PACKED;

struct usbd_audio_attribute_control {
    uint32_t volume_bCUR;
    uint32_t mute_bCUR;
    struct audio_v2_control_range2_param_block_default volume;
    uint8_t mute[CONFIG_USBDEV_AUDIO_MAX_CHANNEL];
};
#endif
struct audio_entity_info {
    usb_slist_t list;
    uint8_t bDescriptorSubtype;
    uint8_t bEntityId;
    void *priv[2];
};

static usb_slist_t usbd_audio_entity_info_head = USB_SLIST_OBJECT_INIT(usbd_audio_entity_info_head);

#if CONFIG_USBDEV_AUDIO_VERSION < 0x0200
static int audio_class_endpoint_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t control_selector;
    uint32_t sampling_freq = 0;
    uint8_t pitch_enable;
    uint8_t ep;

    if ((setup->bmRequestType & USB_REQUEST_RECIPIENT_MASK) == USB_REQUEST_RECIPIENT_ENDPOINT) {
        control_selector = HI_BYTE(setup->wValue);
        ep = LO_BYTE(setup->wIndex);

        switch (setup->bRequest) {
            case AUDIO_REQUEST_SET_CUR:
                switch (control_selector) {
                    case AUDIO_EP_CONTROL_SAMPLING_FEQ:
                        memcpy((uint8_t *)&sampling_freq, *data, *len);
                        USB_LOG_DBG("Set ep:%02x %d Hz\r\n", ep, (int)sampling_freq);
                        usbd_audio_set_sampling_freq(0, ep, sampling_freq);
                        break;
                    case AUDIO_EP_CONTROL_PITCH:
                        pitch_enable = (*data)[0];
                        usbd_audio_set_pitch(ep, pitch_enable);
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class control selector 0x%02x\r\n", control_selector);
                        return -1;
                }
                break;
            case AUDIO_REQUEST_GET_CUR:
                sampling_freq = 16000;
                memcpy(*data, &sampling_freq, 4);
                *len = 4;
                USB_LOG_DBG("Get ep:%02x %d Hz\r\n", ep, (int)sampling_freq);
                break;
            default:
                USB_LOG_WRN("Unhandled Audio Class bRequest 0x%02x\r\n", setup->bRequest);
                return -1;
        }
    } else {
        return -1;
    }
    return 0;
}
#endif

static int audio_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("AUDIO Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    struct audio_entity_info *current_entity_info = NULL;
    struct usbd_audio_attribute_control *current_feature_control = NULL;
#if CONFIG_USBDEV_AUDIO_VERSION >= 0x0200
    uint32_t *sampling_freq;
#endif
    usb_slist_t *i;
    uint8_t entity_id;
    uint8_t control_selector;
    uint8_t ch;
    uint8_t mute;
    uint16_t volume;
    const char *mute_string[2] = { "off", "on" };

    entity_id = HI_BYTE(setup->wIndex);
    control_selector = HI_BYTE(setup->wValue);
    ch = LO_BYTE(setup->wValue);

    ARG_UNUSED(mute_string);
    if (ch > (CONFIG_USBDEV_AUDIO_MAX_CHANNEL - 1)) {
        return -2;
    }

    usb_slist_for_each(i, &usbd_audio_entity_info_head)
    {
        struct audio_entity_info *tmp_entity_info = usb_slist_entry(i, struct audio_entity_info, list);
        if (tmp_entity_info->bEntityId == entity_id) {
            current_entity_info = tmp_entity_info;
            break;
        }
    }

    if (current_entity_info == NULL) {
        return -2;
    }

    current_feature_control = (struct usbd_audio_attribute_control *)current_entity_info->priv[0];
#if CONFIG_USBDEV_AUDIO_VERSION >= 0x0200
    sampling_freq = (uint32_t *)current_entity_info->priv[1];
#endif
    if (current_entity_info->bDescriptorSubtype == AUDIO_CONTROL_FEATURE_UNIT) {
#if CONFIG_USBDEV_AUDIO_VERSION < 0x0200
        float volume2db = 0.0;

        switch (control_selector) {
            case AUDIO_FU_CONTROL_MUTE:
                switch (setup->bRequest) {
                    case AUDIO_REQUEST_SET_CUR:
                        mute = (*data)[0];
                        current_feature_control->mute[ch] = mute;
                        USB_LOG_DBG("Set UnitId:%d ch[%d] mute %s\r\n", entity_id, ch, mute_string[mute]);
                        usbd_audio_set_mute(entity_id, ch, mute);
                        break;
                    case AUDIO_REQUEST_GET_CUR:
                        (*data)[0] = current_feature_control->mute[ch];
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class bRequest 0x%02x\r\n", setup->bRequest);
                        return -1;
                }

                break;
            case AUDIO_FU_CONTROL_VOLUME:
                switch (setup->bRequest) {
                    case AUDIO_REQUEST_SET_CUR:
                        volume = (((uint16_t)(*data)[1] << 8) | ((uint16_t)(*data)[0]));
                        current_feature_control->volume[ch].vol_current = volume;

                        if (volume < 0x8000) {
                            volume2db = 0.00390625 * volume;
                        } else if (volume > 0x8000) {
                            volume2db = -0.00390625 * (0xffff - volume + 1);
                        }

                        USB_LOG_DBG("Set UnitId:%d ch[%d] %0.4f dB\r\n", entity_id, ch, volume2db);
                        usbd_audio_set_volume(entity_id, ch, volume2db);
                        break;
                    case AUDIO_REQUEST_GET_CUR:
                        memcpy(*data, &current_feature_control->volume[ch].vol_current, 2);
                        *len = 2;
                        break;

                    case AUDIO_REQUEST_GET_MIN:
                        memcpy(*data, &current_feature_control->volume[ch].vol_min, 2);
                        *len = 2;
                        break;

                    case AUDIO_REQUEST_GET_MAX:
                        memcpy(*data, &current_feature_control->volume[ch].vol_max, 2);
                        *len = 2;
                        break;

                    case AUDIO_REQUEST_GET_RES:
                        memcpy(*data, &current_feature_control->volume[ch].vol_res, 2);
                        *len = 2;
                        break;

                    case AUDIO_REQUEST_SET_RES:
                        memcpy(&current_feature_control->volume[ch].vol_res, *data, 2);
                        *len = 2;
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class bRequest 0x%02x\r\n", setup->bRequest);
                        return -1;
                }
                break;
            default:
                USB_LOG_WRN("Unhandled Audio Class control selector 0x%02x\r\n", control_selector);
                return -1;
        }
#else
        switch (setup->bRequest) {
            case AUDIO_REQUEST_CUR:
                switch (control_selector) {
                    case AUDIO_FU_CONTROL_MUTE:
                        if (setup->bmRequestType & USB_REQUEST_DIR_MASK) {
                            (*data)[0] = current_feature_control->mute_bCUR;
                            *len = 1;
                        } else {
                            mute = (*data)[0];
                            current_feature_control->mute_bCUR = mute;
                            USB_LOG_DBG("Set UnitId:%d ch[%d] mute %s\r\n", entity_id, ch, mute_string[mute]);
                            usbd_audio_set_mute(entity_id, ch, mute);
                        }
                        break;
                    case AUDIO_FU_CONTROL_VOLUME:
                        if (setup->bmRequestType & USB_REQUEST_DIR_MASK) {
                            (*data)[0] = current_feature_control->volume_bCUR & 0XFF;
                            (*data)[1] = (current_feature_control->volume_bCUR >> 8) & 0xff;
                            *len = 2;
                        } else {
                            volume = (((uint16_t)(*data)[1] << 8) | ((uint16_t)(*data)[0]));
                            current_feature_control->volume_bCUR = volume;
                            USB_LOG_DBG("Set UnitId:%d ch[%d] %d dB\r\n", entity_id, ch, volume);
                            usbd_audio_set_volume(entity_id, ch, volume);
                        }
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class control selector 0x%02x\r\n", control_selector);
                        return -1;
                }
                break;
            case AUDIO_REQUEST_RANGE:
                switch (control_selector) {
                    case AUDIO_FU_CONTROL_VOLUME:
                        if (setup->bmRequestType & USB_REQUEST_DIR_MASK) {
                            *((uint16_t *)(*data + 0)) = current_feature_control->volume.wNumSubRanges;
                            *((uint16_t *)(*data + 2)) = current_feature_control->volume.subrange[ch].wMin;
                            *((uint16_t *)(*data + 4)) = current_feature_control->volume.subrange[ch].wMax;
                            *((uint16_t *)(*data + 6)) = current_feature_control->volume.subrange[ch].wRes;
                            *len = 8;
                        } else {
                        }
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class control selector 0x%02x\r\n", control_selector);
                        return -1;
                }

                break;
            default:
                USB_LOG_WRN("Unhandled Audio Class bRequest 0x%02x\r\n", setup->bRequest);
                return -1;
        }
#endif
    }
#if CONFIG_USBDEV_AUDIO_VERSION >= 0x0200
    else if (current_entity_info->bDescriptorSubtype == AUDIO_CONTROL_CLOCK_SOURCE) {
        switch (setup->bRequest) {
            case AUDIO_REQUEST_CUR:
                switch (control_selector) {
                    case AUDIO_CS_CONTROL_SAM_FREQ:
                        if (setup->bmRequestType & USB_REQUEST_DIR_MASK) {
                            memcpy(*data, &sampling_freq[ch], sizeof(uint32_t));
                            USB_LOG_DBG("Get ClockId:%d ch[%d] %d Hz\r\n", entity_id, ch, (int)sampling_freq[ch]);
                            *len = 4;
                        } else {
                            memcpy(&sampling_freq[ch], *data, setup->wLength);
                            USB_LOG_DBG("Set ClockId:%d ch[%d] %d Hz\r\n", entity_id, ch, (int)sampling_freq[ch]);
                            usbd_audio_set_sampling_freq(entity_id, ch, sampling_freq[ch]);
                        }
                        break;
                    case AUDIO_CS_CONTROL_CLOCK_VALID:
                        if (setup->bmRequestType & USB_REQUEST_DIR_MASK) {
                            (*data)[0] = 1;
                            *len = 1;
                        } else {
                        }
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class control selector 0x%02x\r\n", control_selector);
                        return -1;
                }
                break;
            case AUDIO_REQUEST_RANGE:
                switch (control_selector) {
                    case AUDIO_CS_CONTROL_SAM_FREQ:
                        if (setup->bmRequestType & USB_REQUEST_DIR_MASK) {
                            uint8_t *sampling_freq_table = NULL;
                            uint16_t num;

                            usbd_audio_get_sampling_freq_table(entity_id, &sampling_freq_table);
                            num = (uint16_t)((uint16_t)(sampling_freq_table[1] << 8) | ((uint16_t)sampling_freq_table[0]));
                            *data = sampling_freq_table;
                            *len = (12 * num + 2);
                            USB_LOG_DBG("Get sampling_freq_table entity_id:%d ch[%d] addr:%x\r\n", entity_id, ch, (uint32_t)sampling_freq_table);
                        } else {
                        }
                        break;
                    default:
                        USB_LOG_WRN("Unhandled Audio Class control selector 0x%02x\r\n", control_selector);
                        return -1;
                }

                break;
            default:
                USB_LOG_WRN("Unhandled Audio Class bRequest 0x%02x\r\n", setup->bRequest);
                return -1;
        }
    }
#endif
    else {
        return -1;
    }
    return 0;
}

static void audio_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:

            break;

        case USBD_EVENT_SET_INTERFACE: {
            struct usb_interface_descriptor *intf = (struct usb_interface_descriptor *)arg;
            if (intf->bAlternateSetting == 1) {
                usbd_audio_open(intf->bInterfaceNumber);
            } else {
                usbd_audio_close(intf->bInterfaceNumber);
            }
        }

        break;

        default:
            break;
    }
}

struct usbd_interface *usbd_audio_init_intf(struct usbd_interface *intf)
{
    intf->class_interface_handler = audio_class_interface_request_handler;
#if CONFIG_USBDEV_AUDIO_VERSION < 0x0200
    intf->class_endpoint_handler = audio_class_endpoint_request_handler;
#else
    intf->class_endpoint_handler = NULL;
#endif
    intf->vendor_handler = NULL;
    intf->notify_handler = audio_notify_handler;

    return intf;
}

void usbd_audio_add_entity(uint8_t entity_id, uint16_t bDescriptorSubtype)
{
    struct audio_entity_info *entity_info = usb_malloc(sizeof(struct audio_entity_info));
    memset(entity_info, 0, sizeof(struct audio_entity_info));
    entity_info->bEntityId = entity_id;
    entity_info->bDescriptorSubtype = bDescriptorSubtype;

    if (bDescriptorSubtype == AUDIO_CONTROL_FEATURE_UNIT) {
#if CONFIG_USBDEV_AUDIO_VERSION < 0x0200
        struct usbd_audio_attribute_control *control = usb_malloc(sizeof(struct usbd_audio_attribute_control));
        memset(control, 0, sizeof(struct usbd_audio_attribute_control));
        for (uint8_t ch = 0; ch < CONFIG_USBDEV_AUDIO_MAX_CHANNEL; ch++) {
            control->volume[ch].vol_min = 0xdb00;
            control->volume[ch].vol_max = 0x0000;
            control->volume[ch].vol_res = 0x0100;
            control->volume[ch].vol_current = 0xf600;
            control->mute[ch] = 0;
            control->automatic_gain[ch] = 0;
        }
#else
        struct usbd_audio_attribute_control *control = usb_malloc(sizeof(struct usbd_audio_attribute_control));
        memset(control, 0, sizeof(struct usbd_audio_attribute_control));
        for (uint8_t ch = 0; ch < CONFIG_USBDEV_AUDIO_MAX_CHANNEL; ch++) {
            control->volume.wNumSubRanges = 1;
            control->volume.subrange[ch].wMin = 0;
            control->volume.subrange[ch].wMax = 100;
            control->volume.subrange[ch].wRes = 1;
            control->mute[ch] = 0;
            control->volume_bCUR = 0;
            control->mute_bCUR = 0;
        }
#endif
        entity_info->priv[0] = control;
    } else if (bDescriptorSubtype == AUDIO_CONTROL_CLOCK_SOURCE) {
#if CONFIG_USBDEV_AUDIO_VERSION >= 0x0200
        uint32_t *sampling_freq = usb_malloc(sizeof(uint32_t) * CONFIG_USBDEV_AUDIO_MAX_CHANNEL);
        for (size_t ch = 0; ch < CONFIG_USBDEV_AUDIO_MAX_CHANNEL; ch++) {
            sampling_freq[ch] = 16000;
        }
        entity_info->priv[1] = sampling_freq;
#else
        entity_info->priv[1] = NULL;
#endif
    }

    usb_slist_add_tail(&usbd_audio_entity_info_head, &entity_info->list);
}

__WEAK void usbd_audio_set_volume(uint8_t entity_id, uint8_t ch, float dB)
{
}

__WEAK void usbd_audio_set_mute(uint8_t entity_id, uint8_t ch, uint8_t enable)
{
}

__WEAK void usbd_audio_set_sampling_freq(uint8_t entity_id, uint8_t ep_ch, uint32_t sampling_freq)
{
}

#if CONFIG_USBDEV_AUDIO_VERSION >= 0x0200
__WEAK void usbd_audio_get_sampling_freq_table(uint8_t entity_id, uint8_t **sampling_freq_table)
{
}
#endif

__WEAK void usbd_audio_set_pitch(uint8_t ep, bool enable)
{
}