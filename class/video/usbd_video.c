/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_video.h"

struct video_entity_info {
    uint8_t bDescriptorSubtype;
    uint8_t bEntityId;
    uint16_t wTerminalType;
};

struct usbd_video_cfg_priv {
    struct video_probe_and_commit_controls probe;
    struct video_probe_and_commit_controls commit;
    uint8_t power_mode;
    uint8_t error_code;
    struct video_entity_info info[3];
} usbd_video_cfg = {
    .info[0] = { .bDescriptorSubtype = VIDEO_VC_INPUT_TERMINAL_DESCRIPTOR_SUBTYPE, .bEntityId = 0x01, .wTerminalType = VIDEO_ITT_CAMERA },
    .info[1] = { .bDescriptorSubtype = VIDEO_VC_OUTPUT_TERMINAL_DESCRIPTOR_SUBTYPE, .bEntityId = 0x03, .wTerminalType = 0x00 },
    .info[2] = { .bDescriptorSubtype = VIDEO_VC_PROCESSING_UNIT_DESCRIPTOR_SUBTYPE, .bEntityId = 0x02, .wTerminalType = 0x00 },
};

static int usbd_video_control_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t control_selector = (uint8_t)(setup->wValue >> 8);

    switch (control_selector) {
        case VIDEO_VC_VIDEO_POWER_MODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }

            break;
        case VIDEO_VC_REQUEST_ERROR_CODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_GET_CUR:
                    (*data)[0] = 0x06;
                    *len = 1;
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }

            break;
        default:
            break;
    }

    return 0;
}

static int usbd_video_control_unit_terminal_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t entity_id = (uint8_t)(setup->wIndex >> 8);
    uint8_t control_selector = (uint8_t)(setup->wValue >> 8);

    for (uint8_t i = 0; i < 3; i++) {
        struct video_entity_info *entity_info = &usbd_video_cfg.info[i];
        if (entity_info->bEntityId == entity_id) {
            switch (entity_info->bDescriptorSubtype) {
                case VIDEO_VC_HEADER_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_INPUT_TERMINAL_DESCRIPTOR_SUBTYPE:
                    if (entity_info->wTerminalType == VIDEO_ITT_CAMERA) {
                        switch (control_selector) {
                            case VIDEO_CT_AE_MODE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR:
                                        (*data)[0] = 0x08;
                                        *len = 1;
                                        break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint32_t dwExposureTimeAbsolute = 2500;
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint32_t dwExposureTimeAbsolute = 5; //0.0005sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint32_t dwExposureTimeAbsolute = 2500; //0.2500sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint32_t dwExposureTimeAbsolute = 5; //0.0005sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint32_t dwExposureTimeAbsolute = 2500; //0.2500sec
                                        memcpy(*data, (uint8_t *)&dwExposureTimeAbsolute, 4);
                                        *len = 4;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_FOCUS_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wFocusAbsolute = 0x0080;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint16_t wFocusAbsolute = 0;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint16_t wFocusAbsolute = 0x00ff;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint16_t wFocusAbsolute = 0x0001;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint16_t wFocusAbsolute = 0x0080;
                                        memcpy(*data, (uint8_t *)&wFocusAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_ZOOM_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wObjectiveFocalLength = 0x0064;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint16_t wObjectiveFocalLength = 0x0064;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint16_t wObjectiveFocalLength = 0x00c8;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint16_t wObjectiveFocalLength = 0x0001;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint16_t wObjectiveFocalLength = 0x0064;
                                        memcpy(*data, (uint8_t *)&wObjectiveFocalLength, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_ROLL_ABSOLUTE_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wRollAbsolute = 0x0000;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MIN: {
                                        uint16_t wRollAbsolute = 0x0000;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_MAX: {
                                        uint16_t wRollAbsolute = 0x00ff;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_RES: {
                                        uint16_t wRollAbsolute = 0x0001;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    case VIDEO_REQUEST_GET_INFO:
                                        (*data)[0] = 0x03; //struct video_camera_capabilities
                                        *len = 1;
                                        break;
                                    case VIDEO_REQUEST_GET_DEF: {
                                        uint16_t wRollAbsolute = 0x0000;
                                        memcpy(*data, (uint8_t *)&wRollAbsolute, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            case VIDEO_CT_FOCUS_AUTO_CONTROL:
                                switch (setup->bRequest) {
                                    case VIDEO_REQUEST_GET_CUR: {
                                        uint16_t wFocusAuto = 0x0000;
                                        memcpy(*data, (uint8_t *)&wFocusAuto, 2);
                                        *len = 2;
                                    } break;
                                    default:
                                        USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                        return -1;
                                }
                                break;
                            default:
                                USB_LOG_WRN("Unhandled Video Class control selector 0x%02x\r\n", control_selector);
                                return -1;
                        }
                    } else {
                        USB_LOG_WRN("Unhandled Video Class wTerminalType 0x%02x\r\n", entity_info->wTerminalType);
                        return -2;
                    }
                    break;
                case VIDEO_VC_OUTPUT_TERMINAL_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_SELECTOR_UNIT_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_PROCESSING_UNIT_DESCRIPTOR_SUBTYPE:
                    switch (control_selector) {
                        case VIDEO_PU_BACKLIGHT_COMPENSATION_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wBacklightCompensation = 0x0004;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wBacklightCompensation = 0;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wBacklightCompensation = 8;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wBacklightCompensation = 1;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wBacklightCompensation = 4;
                                    memcpy(*data, (uint8_t *)&wBacklightCompensation, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_BRIGHTNESS_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_SET_CUR: {
                                    uint16_t wBrightness = (uint16_t)(*data)[1] << 8 | (uint16_t)(*data)[0];
                                    USB_LOG_INFO("Video set brightness:%d\r\n", wBrightness);
                                } break;
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wBrightness = 0x0080;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wBrightness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wBrightness = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wBrightness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wBrightness = 0x0080;
                                    memcpy(*data, (uint8_t *)&wBrightness, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_CONTRAST_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wContrast = 0x0080;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wContrast = 0x0001;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wContrast = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wContrast = 0x0001;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wContrast = 0x0080;
                                    memcpy(*data, (uint8_t *)&wContrast, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_HUE_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wHue = 0x0080;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wHue = 0x0001;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wHue = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wHue = 0x0001;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wHue = 0x0080;
                                    memcpy(*data, (uint8_t *)&wHue, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_SATURATION_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wSaturation = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wSaturation = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wSaturation = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wSaturation = 0x0080;
                                    memcpy(*data, (uint8_t *)&wSaturation, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_SHARPNESS_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wSharpness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wSharpness = 0x00ff;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wSharpness = 0x0001;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wSharpness = 0x0080;
                                    memcpy(*data, (uint8_t *)&wSharpness, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_GAIN_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wGain = 0;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wGain = 255;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wGain = 1;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wGain = 255;
                                    memcpy(*data, (uint8_t *)&wGain, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_WHITE_BALANCE_TEMPERATURE_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wWhiteBalance_Temprature = 417;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MIN: {
                                    uint16_t wWhiteBalance_Temprature = 300;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_MAX: {
                                    uint16_t wWhiteBalance_Temprature = 600;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_RES: {
                                    uint16_t wWhiteBalance_Temprature = 1;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                case VIDEO_REQUEST_GET_INFO:
                                    (*data)[0] = 0x03; //struct video_camera_capabilities
                                    *len = 1;
                                    break;
                                case VIDEO_REQUEST_GET_DEF: {
                                    uint16_t wWhiteBalance_Temprature = 417;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature, 2);
                                    *len = 2;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        case VIDEO_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL:
                            switch (setup->bRequest) {
                                case VIDEO_REQUEST_GET_CUR: {
                                    uint16_t wWhiteBalance_Temprature_Auto = 1;
                                    memcpy(*data, (uint8_t *)&wWhiteBalance_Temprature_Auto, 1);
                                    *len = 1;
                                } break;
                                default:
                                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                                    return -1;
                            }
                            break;
                        default:
                            usbd_video_cfg.error_code = 0x06;
                            USB_LOG_WRN("Unhandled Video Class control selector 0x%02x\r\n", control_selector);
                            return -1;
                    }
                    break;
                case VIDEO_VC_EXTENSION_UNIT_DESCRIPTOR_SUBTYPE:
                    break;
                case VIDEO_VC_ENCODING_UNIT_DESCRIPTOR_SUBTYPE:
                    break;

                default:
                    break;
            }
        }
    }
    return 0;
}

static int usbd_video_stream_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    uint8_t control_selector = (uint8_t)(setup->wValue >> 8);

    switch (control_selector) {
        case VIDEO_VS_PROBE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    //memcpy((uint8_t *)usbd_video_cfg.probe, *data, setup->wLength);
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    *data = (uint8_t *)&usbd_video_cfg.probe;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;

                case VIDEO_REQUEST_GET_MIN:
                case VIDEO_REQUEST_GET_MAX:
                case VIDEO_REQUEST_GET_RES:
                case VIDEO_REQUEST_GET_DEF:
                    *data = (uint8_t *)&usbd_video_cfg.probe;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;
                case VIDEO_REQUEST_GET_LEN:
                    (*data)[0] = sizeof(struct video_probe_and_commit_controls);
                    *len = 1;
                    break;

                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x03;
                    *len = 1;
                    break;

                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        case VIDEO_VS_COMMIT_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_SET_CUR:
                    //memcpy((uint8_t *)usbd_video_cfg.commit, *data, setup->wLength);
                    break;
                case VIDEO_REQUEST_GET_CUR:
                    *data = (uint8_t *)&usbd_video_cfg.commit;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;
                case VIDEO_REQUEST_GET_MIN:
                case VIDEO_REQUEST_GET_MAX:
                case VIDEO_REQUEST_GET_RES:
                case VIDEO_REQUEST_GET_DEF:
                    *data = (uint8_t *)&usbd_video_cfg.commit;
                    *len = sizeof(struct video_probe_and_commit_controls);
                    break;

                case VIDEO_REQUEST_GET_LEN:
                    (*data)[0] = sizeof(struct video_probe_and_commit_controls);
                    *len = 1;
                    break;

                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x03;
                    *len = 1;
                    break;

                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        case VIDEO_VS_STREAM_ERROR_CODE_CONTROL:
            switch (setup->bRequest) {
                case VIDEO_REQUEST_GET_CUR:
                    *data = &usbd_video_cfg.error_code;
                    *len = 1;
                    break;
                case VIDEO_REQUEST_GET_INFO:
                    (*data)[0] = 0x01;
                    *len = 1;
                    break;
                default:
                    USB_LOG_WRN("Unhandled Video Class bRequest 0x%02x\r\n", setup->bRequest);
                    return -1;
            }
            break;
        default:
            break;
    }

    return 0;
}

static int video_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("Video Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    uint8_t intf_num = (uint8_t)setup->wIndex;
    uint8_t entity_id = (uint8_t)(setup->wIndex >> 8);

    if (intf_num == 0) { /* Video Control Interface */
        if (entity_id == 0) {
            return usbd_video_control_request_handler(setup, data, len); /* Interface Control Requests */
        } else {
            return usbd_video_control_unit_terminal_request_handler(setup, data, len); /* Unit and Terminal Requests */
        }
    } else if (intf_num == 1) {                                     /* Video Stream Inteface */
        return usbd_video_stream_request_handler(setup, data, len); /* Interface Stream Requests */
    }
    return -1;
}

static void video_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            usbd_video_cfg.error_code = 0;
            usbd_video_cfg.power_mode = 0;
            break;

        case USBD_EVENT_SET_INTERFACE: {
            struct usb_interface_descriptor *intf = (struct usb_interface_descriptor *)arg;
            if (intf->bAlternateSetting == 1) {
                usbd_video_open(intf->bInterfaceNumber);
            } else {
                usbd_video_close(intf->bInterfaceNumber);
            }
        }

        break;
        default:
            break;
    }
}

void usbd_video_probe_and_commit_controls_init(uint32_t dwFrameInterval, uint32_t dwMaxVideoFrameSize, uint32_t dwMaxPayloadTransferSize)
{
    usbd_video_cfg.probe.hintUnion.bmHint = 0x01;
    usbd_video_cfg.probe.hintUnion1.bmHint = 0;
    usbd_video_cfg.probe.bFormatIndex = 1;
    usbd_video_cfg.probe.bFrameIndex = 1;
    usbd_video_cfg.probe.dwFrameInterval = dwFrameInterval;
    usbd_video_cfg.probe.wKeyFrameRate = 0;
    usbd_video_cfg.probe.wPFrameRate = 0;
    usbd_video_cfg.probe.wCompQuality = 0;
    usbd_video_cfg.probe.wCompWindowSize = 0;
    usbd_video_cfg.probe.wDelay = 0;
    usbd_video_cfg.probe.dwMaxVideoFrameSize = dwMaxVideoFrameSize;
    usbd_video_cfg.probe.dwMaxPayloadTransferSize = dwMaxPayloadTransferSize;
    usbd_video_cfg.probe.dwClockFrequency = 0;
    usbd_video_cfg.probe.bmFramingInfo = 0;
    usbd_video_cfg.probe.bPreferedVersion = 0;
    usbd_video_cfg.probe.bMinVersion = 0;
    usbd_video_cfg.probe.bMaxVersion = 0;

    usbd_video_cfg.commit.hintUnion.bmHint = 0x01;
    usbd_video_cfg.commit.hintUnion1.bmHint = 0;
    usbd_video_cfg.commit.bFormatIndex = 1;
    usbd_video_cfg.commit.bFrameIndex = 1;
    usbd_video_cfg.commit.dwFrameInterval = dwFrameInterval;
    usbd_video_cfg.commit.wKeyFrameRate = 0;
    usbd_video_cfg.commit.wPFrameRate = 0;
    usbd_video_cfg.commit.wCompQuality = 0;
    usbd_video_cfg.commit.wCompWindowSize = 0;
    usbd_video_cfg.commit.wDelay = 0;
    usbd_video_cfg.commit.dwMaxVideoFrameSize = dwMaxVideoFrameSize;
    usbd_video_cfg.commit.dwMaxPayloadTransferSize = dwMaxPayloadTransferSize;
    usbd_video_cfg.commit.dwClockFrequency = 0;
    usbd_video_cfg.commit.bmFramingInfo = 0;
    usbd_video_cfg.commit.bPreferedVersion = 0;
    usbd_video_cfg.commit.bMinVersion = 0;
    usbd_video_cfg.commit.bMaxVersion = 0;
}

struct usbd_interface *usbd_video_init_intf(struct usbd_interface *intf,
                                            uint32_t dwFrameInterval,
                                            uint32_t dwMaxVideoFrameSize,
                                            uint32_t dwMaxPayloadTransferSize)
{
    intf->class_interface_handler = video_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = video_notify_handler;

    usbd_video_probe_and_commit_controls_init(dwFrameInterval, dwMaxVideoFrameSize, dwMaxPayloadTransferSize);
    return intf;
}

uint32_t usbd_video_mjpeg_payload_fill(uint8_t *input, uint32_t input_len, uint8_t *output, uint32_t *out_len)
{
    uint32_t packets;
    uint32_t last_packet_size;
    uint32_t picture_pos = 0;
    static uint8_t uvc_header[2] = { 0x02, 0x80 };

    packets = input_len / usbd_video_cfg.probe.dwMaxPayloadTransferSize + 1;
    last_packet_size = input_len - ((packets - 1) * (usbd_video_cfg.probe.dwMaxPayloadTransferSize - 2)) + 2;

    for (size_t i = 0; i < packets; i++) {
        output[usbd_video_cfg.probe.dwMaxPayloadTransferSize * i] = uvc_header[0];
        output[usbd_video_cfg.probe.dwMaxPayloadTransferSize * i + 1] = uvc_header[1];
        if (i == (packets - 1)) {
            memcpy(&output[2 + usbd_video_cfg.probe.dwMaxPayloadTransferSize * i], &input[picture_pos], last_packet_size - 2);
            output[usbd_video_cfg.probe.dwMaxPayloadTransferSize * i + 1] |= (1 << 1);
        } else {
            memcpy(&output[2 + usbd_video_cfg.probe.dwMaxPayloadTransferSize * i], &input[picture_pos], usbd_video_cfg.probe.dwMaxPayloadTransferSize - 2);
            picture_pos += usbd_video_cfg.probe.dwMaxPayloadTransferSize - 2;
        }
    }
    uvc_header[1] ^= 1;
    *out_len = (input_len + 2 * packets);
    return packets;
}