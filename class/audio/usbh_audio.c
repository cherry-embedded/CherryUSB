/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_audio.h"
#include <math.h>

#undef USB_DBG_TAG
#define USB_DBG_TAG "usbh_audio"
#include "usb_log.h"

#define DEV_FORMAT "/dev/audio%d"

/* general descriptor field offsets */
#define DESC_bLength            0 /** Length offset */
#define DESC_bDescriptorType    1 /** Descriptor type offset */
#define DESC_bDescriptorSubType 2 /** Descriptor subtype offset */

/* interface descriptor field offsets */
#define INTF_DESC_bInterfaceNumber  2 /** Interface number offset */
#define INTF_DESC_bAlternateSetting 3 /** Alternate setting offset */

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_audio_buf[USB_ALIGN_UP(128, CONFIG_USB_ALIGN_SIZE)];

static struct usbh_audio g_audio_class[CONFIG_USBHOST_MAX_AUDIO_CLASS];
static uint32_t g_devinuse = 0;

static float volume_hex2float(int16_t volume_hex)
{
    return (float)volume_hex / 256.0f;
}

static int16_t volume_float2hex(float volume_db)
{
    return (int16_t)(volume_db * 256);
}

static struct usbh_audio *usbh_audio_class_alloc(void)
{
    uint8_t devno;

    for (devno = 0; devno < CONFIG_USBHOST_MAX_AUDIO_CLASS; devno++) {
        if ((g_devinuse & (1U << devno)) == 0) {
            g_devinuse |= (1U << devno);
            memset(&g_audio_class[devno], 0, sizeof(struct usbh_audio));
            g_audio_class[devno].minor = devno;
            return &g_audio_class[devno];
        }
    }
    return NULL;
}

static void usbh_audio_class_free(struct usbh_audio *audio_class)
{
    uint8_t devno = audio_class->minor;

    if (devno < 32) {
        g_devinuse &= ~(1U << devno);
    }
    memset(audio_class, 0, sizeof(struct usbh_audio));
}

/* INPUT --> FEATURE UNIT --> OUTPUT */
static uint8_t usbh_audio_find_input_unit_id(const uint8_t *desc, uint8_t unit_id, uint8_t **feat_desc)
{
    uint32_t total_length;
    uint32_t desc_len = 0;
    const uint8_t *p;

    *feat_desc = NULL;

    if (!desc) {
        return 0;
    }

    total_length = ((struct audio_cs_if_ac_header_descriptor *)desc)->wTotalLength;
    p = desc;

    while (p[DESC_bLength] && (desc_len <= total_length)) {
        switch (p[DESC_bDescriptorSubType]) {
            case AUDIO_CONTROL_HEADER:
            case AUDIO_CONTROL_INPUT_TERMINAL:
                break;
            case AUDIO_CONTROL_OUTPUT_TERMINAL: {
                struct audio_cs_if_ac_output_terminal_descriptor *output_terminal_desc = (struct audio_cs_if_ac_output_terminal_descriptor *)p;
                if (output_terminal_desc->bSourceID == unit_id) {
                    return output_terminal_desc->bTerminalID;
                }
                break;
            }
            case AUDIO_CONTROL_FEATURE_UNIT: {
                struct audio_cs_if_ac_feature_unit_descriptor *feature_unit_desc = (struct audio_cs_if_ac_feature_unit_descriptor *)p;
                if (feature_unit_desc->bSourceID == unit_id) {
                    *feat_desc = (uint8_t *)feature_unit_desc;
                    return feature_unit_desc->bUnitID;
                }
                break;
            }
            case AUDIO_CONTROL_SELECTOR_UNIT: {
                struct audio_cs_if_ac_selector_unit_descriptor *selector_unit_desc = (struct audio_cs_if_ac_selector_unit_descriptor *)p;
                for (int i = 0; i < selector_unit_desc->bNrInPins; i++) {
                    if (selector_unit_desc->baSourceID[i] == unit_id) {
                        return selector_unit_desc->bUnitID;
                    }
                }
                break;
            }
            case AUDIO_CONTROL_MIXER_UNIT: {
                struct audio_cs_if_ac_mixer_unit_descriptor *mixer_unit_desc = (struct audio_cs_if_ac_mixer_unit_descriptor *)p;
                for (int i = 0; i < mixer_unit_desc->bNrInPins; i++) {
                    if (mixer_unit_desc->baSourceID[i] == unit_id) {
                        return mixer_unit_desc->bUnitID;
                    }
                }
                break;
            }
            default:
                USB_LOG_ERR("UAC Unknown Descriptor Subtype %d\r\n", p[DESC_bDescriptorSubType]);
                break;
        }
        /* skip to next descriptor */
        desc_len += p[DESC_bLength];
        p += p[DESC_bLength];
    }
    return 0;
}

/* OUTPUT --> FEATURE UNIT --> INPUT */
static uint8_t usbh_audio_find_output_unit_id(const uint8_t *desc, uint8_t unit_id, uint8_t **feat_desc)
{
    uint32_t total_length;
    uint32_t desc_len = 0;
    const uint8_t *p;

    *feat_desc = NULL;

    if (!desc) {
        return 0;
    }

    total_length = ((struct audio_cs_if_ac_header_descriptor *)desc)->wTotalLength;
    p = desc;

    while (p[DESC_bLength] && (desc_len <= total_length)) {
        switch (p[DESC_bDescriptorSubType]) {
            case AUDIO_CONTROL_HEADER:
            case AUDIO_CONTROL_INPUT_TERMINAL:
                break;
            case AUDIO_CONTROL_OUTPUT_TERMINAL: {
                struct audio_cs_if_ac_output_terminal_descriptor *output_terminal_desc = (struct audio_cs_if_ac_output_terminal_descriptor *)p;
                if (output_terminal_desc->bTerminalID == unit_id) {
                    return output_terminal_desc->bSourceID;
                }
                break;
            }
            case AUDIO_CONTROL_FEATURE_UNIT: {
                struct audio_cs_if_ac_feature_unit_descriptor *feature_unit_desc = (struct audio_cs_if_ac_feature_unit_descriptor *)p;
                if (feature_unit_desc->bUnitID == unit_id) {
                    *feat_desc = (uint8_t *)feature_unit_desc;
                    return feature_unit_desc->bSourceID;
                }
                break;
            }
            case AUDIO_CONTROL_SELECTOR_UNIT: {
                struct audio_cs_if_ac_selector_unit_descriptor *selector_unit_desc = (struct audio_cs_if_ac_selector_unit_descriptor *)p;
                if (selector_unit_desc->bUnitID == unit_id) {
                    // for basic audio device, the first source should be microphone
                    return selector_unit_desc->baSourceID[0];
                }
                break;
            }
            case AUDIO_CONTROL_MIXER_UNIT: {
                struct audio_cs_if_ac_mixer_unit_descriptor *mixer_unit_desc = (struct audio_cs_if_ac_mixer_unit_descriptor *)p;
                if (mixer_unit_desc->bUnitID == unit_id) {
                    return mixer_unit_desc->baSourceID[0];
                }
                break;
            }
            default:
                USB_LOG_ERR("UAC Unknown Descriptor Subtype %d\r\n", p[DESC_bDescriptorSubType]);
                break;
        }
        /* skip to next descriptor */
        desc_len += p[DESC_bLength];
        p += p[DESC_bLength];
    }
    return 0;
}

static struct audio_cs_if_ac_feature_unit_descriptor *usbh_audio_find_feature_unit(const uint8_t *header_desc, uint8_t terminal_id, bool is_tx)
{
    if (!header_desc) {
        return NULL;
    }

    uint8_t unit_id = terminal_id;
    struct audio_cs_if_ac_feature_unit_descriptor *feature_unit_desc = NULL;
    if (is_tx) {
        while (feature_unit_desc == NULL && unit_id != 0) {
            unit_id = usbh_audio_find_input_unit_id(header_desc, unit_id, (uint8_t **)&feature_unit_desc);
        }
    } else {
        while (feature_unit_desc == NULL && unit_id != 0) {
            unit_id = usbh_audio_find_output_unit_id(header_desc, unit_id, (uint8_t **)&feature_unit_desc);
        }
    }

    return feature_unit_desc;
}

int usbh_audio_open(struct usbh_audio *audio_class, uint32_t samp_freq, uint8_t bitresolution, bool is_tx)
{
    struct usb_setup_packet *setup;
    struct usb_endpoint_descriptor *ep_desc;
    uint8_t mult;
    uint16_t mps;
    int ret;
    uint8_t intf = 0xff;
    uint8_t altsetting = 0xff;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }
    setup = audio_class->hport->setup;

    if (audio_class->is_opened) {
        return 0;
    }

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            intf = audio_class->as_msg_table[i].stream_intf;
            for (uint8_t j = 1; j < audio_class->hport->config.intf[audio_class->as_msg_table[i].stream_intf].altsetting_num; j++) {
                if (audio_class->as_msg_table[i].as_format[j].bBitResolution == bitresolution) {
                    for (uint8_t k = 0; k < audio_class->as_msg_table[i].as_format[j].bSamFreqType; k++) {
                        uint32_t freq = 0;

                        memcpy(&freq, &audio_class->as_msg_table[i].as_format[j].tSamFreq[3 * k], 3);
                        if (freq == samp_freq) {
                            altsetting = j;
                            goto freq_found;
                        }
                    }
                }
            }
        }
    }

freq_found:
    if (altsetting == 0xff) {
        return -USB_ERR_NODEV;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_SET_INTERFACE;
    setup->wValue = altsetting;
    setup->wIndex = intf;
    setup->wLength = 0;

    ret = usbh_control_transfer(audio_class->hport, setup, NULL);
    if (ret < 0) {
        return ret;
    }

    ep_desc = &audio_class->hport->config.intf[intf].altsetting[altsetting].ep[0].ep_desc;

    if (audio_class->as_msg_table[intf - audio_class->ctrl_intf - 1].ep_attr & AUDIO_EP_CONTROL_SAMPLING_FEQ) {
        setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_ENDPOINT;
        setup->bRequest = AUDIO_REQUEST_SET_CUR;
        setup->wValue = (AUDIO_EP_CONTROL_SAMPLING_FEQ << 8) | 0x00;
        setup->wIndex = ep_desc->bEndpointAddress;
        setup->wLength = 3;

        memcpy(g_audio_buf, &samp_freq, 3);
        ret = usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
        if (ret < 0) {
            return ret;
        }
    }

    mult = (ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_MASK) >> USB_MAXPACKETSIZE_ADDITIONAL_TRANSCATION_SHIFT;
    mps = ep_desc->wMaxPacketSize & USB_MAXPACKETSIZE_MASK;
    if (ep_desc->bEndpointAddress & 0x80) {
        audio_class->isoin_mps = mps * (mult + 1);
        USBH_EP_INIT(audio_class->isoin, ep_desc);
    } else {
        audio_class->isoout_mps = mps * (mult + 1);
        USBH_EP_INIT(audio_class->isoout, ep_desc);
    }

    USB_LOG_INFO("Open audio %s stream, altsetting: %u\r\n", is_tx ? "tx" : "rx", altsetting);
    audio_class->is_opened = true;
    return ret;
}

int usbh_audio_close(struct usbh_audio *audio_class, bool is_tx)
{
    struct usb_setup_packet *setup;
    struct usb_endpoint_descriptor *ep_desc;
    int ret;
    uint8_t intf = 0xff;
    uint8_t altsetting = 1;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }
    setup = audio_class->hport->setup;

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            intf = audio_class->as_msg_table[i].stream_intf;
            goto intf_found;
        }
    }

intf_found:
    if (intf == 0xff) {
        return -USB_ERR_NODEV;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_STANDARD | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = USB_REQUEST_SET_INTERFACE;
    setup->wValue = 0;
    setup->wIndex = intf;
    setup->wLength = 0;

    ret = usbh_control_transfer(audio_class->hport, setup, NULL);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("Close audio %s stream\r\n", is_tx ? "tx" : "rx");
    audio_class->is_opened = false;

    ep_desc = &audio_class->hport->config.intf[intf].altsetting[altsetting].ep[0].ep_desc;
    if (ep_desc->bEndpointAddress & 0x80) {
        if (audio_class->isoin) {
            audio_class->isoin = NULL;
        }
    } else {
        if (audio_class->isoout) {
            audio_class->isoout = NULL;
        }
    }

    return ret;
}

static int usbh_audio_get_volume_info(struct usbh_audio *audio_class,
                               uint8_t ch,
                               int16_t *volume_cur,
                               int16_t *volume_min,
                               int16_t *volume_max,
                               int16_t *volume_res,
                               bool is_tx)
{
    struct usb_setup_packet *setup;
    int ret;
    uint8_t feature_id = 0xff;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }

    setup = audio_class->hport->setup;

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            feature_id = audio_class->as_msg_table[i].feature_unit_id;
            goto feature_found;
        }
    }

feature_found:
    if (feature_id == 0xff) {
        return -USB_ERR_NODEV;
    }

    if (volume_cur) {
        setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
        setup->bRequest = AUDIO_REQUEST_GET_CUR;
        setup->wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
        setup->wIndex = (feature_id << 8) | audio_class->ctrl_intf;
        setup->wLength = 2;

        ret = usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
        if (ret < 0) {
            return ret;
        }

        memcpy(volume_cur, g_audio_buf, 2);
    }

    if (volume_min) {
        setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
        setup->bRequest = AUDIO_REQUEST_GET_MIN;
        setup->wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
        setup->wIndex = (feature_id << 8) | audio_class->ctrl_intf;
        setup->wLength = 2;

        ret = usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
        if (ret < 0) {
            return ret;
        }

        memcpy(volume_min, g_audio_buf, 2);
    }

    if (volume_max) {
        setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
        setup->bRequest = AUDIO_REQUEST_GET_MAX;
        setup->wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
        setup->wIndex = (feature_id << 8) | audio_class->ctrl_intf;
        setup->wLength = 2;

        ret = usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
        if (ret < 0) {
            return ret;
        }
        memcpy(volume_max, g_audio_buf, 2);
    }

    if (volume_res) {
        setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
        setup->bRequest = AUDIO_REQUEST_GET_RES;
        setup->wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
        setup->wIndex = (feature_id << 8) | audio_class->ctrl_intf;
        setup->wLength = 2;

        ret = usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
        if (ret < 0) {
            return ret;
        }
        memcpy(volume_res, g_audio_buf, 2);
    }

    return 0;
}

static int __usbh_audio_set_volume(struct usbh_audio *audio_class, uint8_t ch, uint8_t volume, bool is_tx)
{
    struct usb_setup_packet *setup;
    int ret;
    uint8_t feature_id = 0xff;
    int16_t volume_hex;
    int16_t volume_cur;
    int16_t volume_min;
    int16_t volume_max;
    int16_t volume_res;
    float volume_db;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }

    if (volume > 100) {
        return -USB_ERR_INVAL;
    }

    setup = audio_class->hport->setup;

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            feature_id = audio_class->as_msg_table[i].feature_unit_id;
            goto feature_found;
        }
    }

feature_found:
    if (feature_id == 0xff) {
        return -USB_ERR_NODEV;
    }

    ret = usbh_audio_get_volume_info(audio_class, ch, &volume_cur, &volume_min, &volume_max, &volume_res, is_tx);
    if (ret < 0) {
        return ret;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = AUDIO_REQUEST_SET_CUR;
    setup->wValue = (AUDIO_FU_CONTROL_VOLUME << 8) | ch;
    setup->wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup->wLength = 2;

    /* Calculate target volume in float to avoid int16_t calculation overflow. */
    volume_db = volume_hex2float(volume_min) + (volume_hex2float(volume_max) - volume_hex2float(volume_min)) * (float)volume / 100.0f;
    /* Round to the nearest float value based on volume_res. */
    volume_db = roundf(volume_db / volume_hex2float(volume_res)) * volume_hex2float(volume_res);
    volume_hex = volume_float2hex(volume_db);

    USB_LOG_INFO("ch %d volume info: (%.2f dB ~ %.2f dB, step: %.2f dB)\r\n",
                 ch,
                 volume_hex2float(volume_min),
                 volume_hex2float(volume_max),
                 volume_hex2float(volume_res));

    memcpy(g_audio_buf, &volume_hex, 2);
    ret = usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
    if (ret < 0) {
        return ret;
    }
    USB_LOG_INFO("Set stream %s ch %d volume to %d%% (%.2f dB)\r\n", is_tx ? "tx" : "rx", ch, volume, volume_db);
    return 0;
}

int usbh_audio_set_volume(struct usbh_audio *audio_class, uint8_t volume, bool is_tx)
{
    bool found = false;

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            for (uint8_t ch = 0; ch <= audio_class->as_msg_table[i].bNrChannels; ch++) {
                if (audio_class->as_msg_table[i].bmaControls[ch * audio_class->as_msg_table[i].bControlSize] & AUDIO_FU_CONTROL_VOLUME) {
                    int ret = __usbh_audio_set_volume(audio_class, ch, volume, is_tx);
                    if (ret < 0) {
                        return ret;
                    }
                } else {
                    continue;
                }
            }

            found = true;
            break;
        }
    }

    return found ? 0 : -USB_ERR_NODEV;
}

static int __usbh_audio_set_mute(struct usbh_audio *audio_class, uint8_t ch, bool mute, bool is_tx)
{
    struct usb_setup_packet *setup;
    uint8_t feature_id = 0xff;

    if (!audio_class || !audio_class->hport) {
        return -USB_ERR_INVAL;
    }
    setup = audio_class->hport->setup;

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            feature_id = audio_class->as_msg_table[i].feature_unit_id;
            goto feature_found;
        }
    }

feature_found:
    if (feature_id == 0xff) {
        return -USB_ERR_NODEV;
    }

    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = AUDIO_REQUEST_SET_CUR;
    setup->wValue = (AUDIO_FU_CONTROL_MUTE << 8) | ch;
    setup->wIndex = (feature_id << 8) | audio_class->ctrl_intf;
    setup->wLength = 1;

    memcpy(g_audio_buf, &mute, 1);
    return usbh_control_transfer(audio_class->hport, setup, g_audio_buf);
}

int usbh_audio_set_mute(struct usbh_audio *audio_class, bool mute, bool is_tx)
{
    bool found = false;

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        if (audio_class->as_msg_table[i].is_tx == is_tx) {
            for (uint8_t ch = 0; ch <= audio_class->as_msg_table[i].bNrChannels; ch++) {
                if (audio_class->as_msg_table[i].bmaControls[ch * audio_class->as_msg_table[i].bControlSize] & AUDIO_FU_CONTROL_MUTE) {
                    int ret = __usbh_audio_set_mute(audio_class, ch, mute, is_tx);
                    if (ret < 0) {
                        return ret;
                    }
                } else {
                    continue;
                }
            }

            found = true;
            break;
        }
    }

    return found ? 0 : -USB_ERR_NODEV;
}

void usbh_audio_list_module(struct usbh_audio *audio_class)
{
    USB_LOG_INFO("============= Audio module information ===================\r\n");
    USB_LOG_RAW("bcdADC: %04x\r\n", audio_class->bcdADC);
    USB_LOG_RAW("Num of stream: %u\r\n", audio_class->bInCollection);

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        USB_LOG_RAW("\tstream dir: %s\r\n", audio_class->as_msg_table[i].is_tx ? "tx" : "rx");
        USB_LOG_RAW("\tstream intf: %u\r\n", audio_class->as_msg_table[i].stream_intf);
        USB_LOG_RAW("\tNum of altsetting: %u\r\n", audio_class->hport->config.intf[audio_class->as_msg_table[i].stream_intf].altsetting_num);

        for (uint8_t j = 0; j < audio_class->hport->config.intf[audio_class->as_msg_table[i].stream_intf].altsetting_num; j++) {
            if (j == 0) {
                USB_LOG_RAW("\t\tIgnore altsetting 0\r\n");
                continue;
            }
            USB_LOG_RAW("\t\tAltsetting: %u\r\n", j);
            USB_LOG_RAW("\t\t\tbNrChannels: %u\r\n", audio_class->as_msg_table[i].as_format[j].bNrChannels);
            USB_LOG_RAW("\t\t\tbBitResolution: %u\r\n", audio_class->as_msg_table[i].as_format[j].bBitResolution);
            USB_LOG_RAW("\t\t\tbSamFreqType: %u\r\n", audio_class->as_msg_table[i].as_format[j].bSamFreqType);

            for (uint8_t k = 0; k < audio_class->as_msg_table[i].as_format[j].bSamFreqType; k++) {
                uint32_t freq = 0;

                memcpy(&freq, &audio_class->as_msg_table[i].as_format[j].tSamFreq[3 * k], 3);
                USB_LOG_RAW("\t\t\t\tSampleFreq: %u\r\n", freq);
            }
        }
    }

    USB_LOG_INFO("============= Audio module information ===================\r\n");
}

static int usbh_audio_ctrl_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct audio_cs_if_ac_feature_unit_descriptor *feature_desc = NULL;
    int ret;
    uint8_t cur_iface = 0;
    uint8_t cur_iface_count = 0;
    uint8_t cur_alt_setting = 0;
    const uint8_t *p;
    const uint8_t *p_ac_desc = NULL;

    struct usbh_audio *audio_class = usbh_audio_class_alloc();
    if (audio_class == NULL) {
        USB_LOG_ERR("Fail to alloc audio_class\r\n");
        return -USB_ERR_NOMEM;
    }

    audio_class->hport = hport;
    audio_class->ctrl_intf = intf;
    hport->config.intf[intf].priv = audio_class;

    p = hport->raw_config_desc;
    while (p[DESC_bLength]) {
        switch (p[DESC_bDescriptorType]) {
            case USB_DESCRIPTOR_TYPE_INTERFACE_ASSOCIATION:
                cur_iface_count = p[3];
                break;
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                cur_iface = p[INTF_DESC_bInterfaceNumber];
                cur_alt_setting = p[INTF_DESC_bAlternateSetting];
                break;
            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                if (cur_iface == audio_class->ctrl_intf) {
                } else if ((cur_iface > audio_class->ctrl_intf) && (cur_iface < (audio_class->ctrl_intf + cur_iface_count))) {
                    struct usb_endpoint_descriptor *ep_desc = (struct usb_endpoint_descriptor *)p;
                    if (ep_desc->bEndpointAddress & 0x80) {
                        audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].is_tx = false;
                    } else {
                        audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].is_tx = true;
                    }
                } else {
                }
                break;
            case AUDIO_INTERFACE_DESCRIPTOR_TYPE:
                if (cur_iface_count == 0xff) {
                    USB_LOG_ERR("Audio descriptor must have iad descriptor\r\n");
                    return -USB_ERR_INVAL;
                }

                if (cur_iface == audio_class->ctrl_intf) {
                    switch (p[DESC_bDescriptorSubType]) {
                        case AUDIO_CONTROL_HEADER: {
                            struct audio_cs_if_ac_header_descriptor *desc = (struct audio_cs_if_ac_header_descriptor *)p;
                            audio_class->bcdADC = desc->bcdADC;
                            audio_class->bInCollection = desc->bInCollection;
                            USB_ASSERT(audio_class->bInCollection <= CONFIG_USBHOST_AUDIO_MAX_STREAMS);
                            USB_ASSERT(audio_class->bcdADC < 0x0200);

                            p_ac_desc = p;
                        } break;
                        default:
                            break;
                    }
                } else if ((cur_iface > audio_class->ctrl_intf) && (cur_iface < (audio_class->ctrl_intf + cur_iface_count))) {
                    switch (p[DESC_bDescriptorSubType]) {
                        case AUDIO_STREAMING_GENERAL: {
                            struct audio_cs_if_as_general_descriptor *desc = (struct audio_cs_if_as_general_descriptor *)p;

                            /* all altsetting have the same general */
                            audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].stream_intf = cur_iface;
                            audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].bTerminalLink = desc->bTerminalLink;
                        } break;
                        case AUDIO_STREAMING_FORMAT_TYPE: {
                            struct audio_cs_if_as_format_type_descriptor *desc = (struct audio_cs_if_as_format_type_descriptor *)p;

                            USB_ASSERT(desc->bSamFreqType == 1);
                            USB_ASSERT(desc->bNrChannels <= CONFIG_USBHOST_AUDIO_MAX_CHANNELS);
                            USB_ASSERT(cur_alt_setting < CONFIG_USBHOST_MAX_INTF_ALTSETTINGS);

                            audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].bNrChannels = desc->bNrChannels;
                            memcpy(&audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].as_format[cur_alt_setting], desc, desc->bLength);
                        } break;
                        default:
                            break;
                    }
                }
                break;
            case AUDIO_ENDPOINT_DESCRIPTOR_TYPE:
                if ((cur_iface > audio_class->ctrl_intf) && (cur_iface < (audio_class->ctrl_intf + cur_iface_count))) {
                    if (p[DESC_bDescriptorSubType] == AUDIO_ENDPOINT_GENERAL) {
                        struct audio_cs_ep_ep_general_descriptor *desc = (struct audio_cs_ep_ep_general_descriptor *)p;

                        audio_class->as_msg_table[cur_iface - audio_class->ctrl_intf - 1].ep_attr = desc->bmAttributes;
                    }
                }
                break;
            default:
                break;
        }
        /* skip to next descriptor */
        p += p[DESC_bLength];
    }

    if (p_ac_desc == NULL) {
        USB_LOG_ERR("Fail to find audio control header descriptor\r\n");
        return -USB_ERR_INVAL;
    }

    for (uint8_t i = 0; i < audio_class->bInCollection; i++) {
        feature_desc = usbh_audio_find_feature_unit(p_ac_desc, audio_class->as_msg_table[i].bTerminalLink, audio_class->as_msg_table[i].is_tx);
        if (feature_desc == NULL) {
            USB_LOG_ERR("Fail to find feature unit for stream %s\r\n", audio_class->as_msg_table[i].is_tx ? "tx" : "rx");
            return -USB_ERR_INVAL;
        }
        audio_class->as_msg_table[i].feature_unit_id = feature_desc->bUnitID;
        audio_class->as_msg_table[i].bControlSize = feature_desc->bControlSize;

        USB_ASSERT(feature_desc->bLength > 7);
        USB_ASSERT((feature_desc->bLength - 7) <= sizeof(audio_class->as_msg_table[i].bmaControls));
        usb_memcpy(&audio_class->as_msg_table[i].bmaControls[0], feature_desc->bmaControls, feature_desc->bLength - 7);

        ret = usbh_audio_close(audio_class, audio_class->as_msg_table[i].is_tx);
        if (ret < 0) {
            USB_LOG_ERR("Fail to close stream %s\r\n", audio_class->as_msg_table[i].is_tx ? "tx" : "rx");
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
        if (audio_class->isoin) {
        }

        if (audio_class->isoout) {
        }

        if (hport->config.intf[intf].devname[0] != '\0') {
            usb_osal_thread_schedule_other();
            USB_LOG_INFO("Unregister Audio Class:%s\r\n", hport->config.intf[intf].devname);
            usbh_audio_stop(audio_class);
        }

        usbh_audio_class_free(audio_class);
    }

    return ret;
}

static int usbh_audio_data_connect(struct usbh_hubport *hport, uint8_t intf)
{
    (void)hport;
    (void)intf;
    return 0;
}

static int usbh_audio_data_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    (void)hport;
    (void)intf;
    return 0;
}

__WEAK void usbh_audio_run(struct usbh_audio *audio_class)
{
    (void)audio_class;
}

__WEAK void usbh_audio_stop(struct usbh_audio *audio_class)
{
    (void)audio_class;
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
    .bInterfaceClass = USB_DEVICE_CLASS_AUDIO,
    .bInterfaceSubClass = AUDIO_SUBCLASS_AUDIOCONTROL,
    .bInterfaceProtocol = 0x00,
    .id_table = NULL,
    .class_driver = &audio_ctrl_class_driver
};

CLASS_INFO_DEFINE const struct usbh_class_info audio_streaming_intf_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS,
    .bInterfaceClass = USB_DEVICE_CLASS_AUDIO,
    .bInterfaceSubClass = AUDIO_SUBCLASS_AUDIOSTREAMING,
    .bInterfaceProtocol = 0x00,
    .id_table = NULL,
    .class_driver = &audio_streaming_class_driver
};
