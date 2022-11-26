/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_MTP_H
#define USB_MTP_H

#define USB_MTP_CLASS 0x06

#define USB_MTP_SUB_CLASS 0x01U
#define USB_MTP_PROTOCOL  0x01U

/* MTP class requests */
#define MTP_REQUEST_CANCEL             0x64U
#define MTP_REQUEST_GET_EXT_EVENT_DATA 0x65U
#define MTP_REQUEST_RESET              0x66U
#define MTP_REQUEST_GET_DEVICE_STATUS  0x67U

/* Container Types */
#define MTP_CONTAINER_TYPE_UNDEFINED 0U
#define MTP_CONTAINER_TYPE_COMMAND   1U
#define MTP_CONTAINER_TYPE_DATA      2U
#define MTP_CONTAINER_TYPE_RESPONSE  3U
#define MTP_CONTAINER_TYPE_EVENT     4U

/*
 * MTP Class specification Revision 1.1
 * Appendix D. Operations
 */

/* Operations code */
#define MTP_OP_GET_DEVICE_INFO            0x1001U
#define MTP_OP_OPEN_SESSION               0x1002U
#define MTP_OP_CLOSE_SESSION              0x1003U
#define MTP_OP_GET_STORAGE_IDS            0x1004U
#define MTP_OP_GET_STORAGE_INFO           0x1005U
#define MTP_OP_GET_NUM_OBJECTS            0x1006U
#define MTP_OP_GET_OBJECT_HANDLES         0x1007U
#define MTP_OP_GET_OBJECT_INFO            0x1008U
#define MTP_OP_GET_OBJECT                 0x1009U
#define MTP_OP_GET_THUMB                  0x100AU
#define MTP_OP_DELETE_OBJECT              0x100BU
#define MTP_OP_SEND_OBJECT_INFO           0x100CU
#define MTP_OP_SEND_OBJECT                0x100DU
#define MTP_OP_FORMAT_STORE               0x100FU
#define MTP_OP_RESET_DEVICE               0x1010U
#define MTP_OP_GET_DEVICE_PROP_DESC       0x1014U
#define MTP_OP_GET_DEVICE_PROP_VALUE      0x1015U
#define MTP_OP_SET_DEVICE_PROP_VALUE      0x1016U
#define MTP_OP_RESET_DEVICE_PROP_VALUE    0x1017U
#define MTP_OP_TERMINATE_OPEN_CAPTURE     0x1018U
#define MTP_OP_MOVE_OBJECT                0x1019U
#define MTP_OP_COPY_OBJECT                0x101AU
#define MTP_OP_GET_PARTIAL_OBJECT         0x101BU
#define MTP_OP_INITIATE_OPEN_CAPTURE      0x101CU
#define MTP_OP_GET_OBJECT_PROPS_SUPPORTED 0x9801U
#define MTP_OP_GET_OBJECT_PROP_DESC       0x9802U
#define MTP_OP_GET_OBJECT_PROP_VALUE      0x9803U
#define MTP_OP_SET_OBJECT_PROP_VALUE      0x9804U
#define MTP_OP_GET_OBJECT_PROPLIST        0x9805U
#define MTP_OP_GET_OBJECT_PROP_REFERENCES 0x9810U
#define MTP_OP_GETSERVICEIDS              0x9301U
#define MTP_OP_GETSERVICEINFO             0x9302U
#define MTP_OP_GETSERVICECAPABILITIES     0x9303U
#define MTP_OP_GETSERVICEPROPDESC         0x9304U

/* MTP response code */
#define MTP_RESPONSE_OK                                    0x2001U
#define MTP_RESPONSE_GENERAL_ERROR                         0x2002U
#define MTP_RESPONSE_PARAMETER_NOT_SUPPORTED               0x2006U
#define MTP_RESPONSE_INCOMPLETE_TRANSFER                   0x2007U
#define MTP_RESPONSE_INVALID_STORAGE_ID                    0x2008U
#define MTP_RESPONSE_INVALID_OBJECT_HANDLE                 0x2009U
#define MTP_RESPONSE_DEVICEPROP_NOT_SUPPORTED              0x200AU
#define MTP_RESPONSE_STORE_FULL                            0x200CU
#define MTP_RESPONSE_ACCESS_DENIED                         0x200FU
#define MTP_RESPONSE_STORE_NOT_AVAILABLE                   0x2013U
#define MTP_RESPONSE_SPECIFICATION_BY_FORMAT_NOT_SUPPORTED 0x2014U
#define MTP_RESPONSE_NO_VALID_OBJECT_INFO                  0x2015U
#define MTP_RESPONSE_DEVICE_BUSY                           0x2019U
#define MTP_RESPONSE_INVALID_PARENT_OBJECT                 0x201AU
#define MTP_RESPONSE_INVALID_PARAMETER                     0x201DU
#define MTP_RESPONSE_SESSION_ALREADY_OPEN                  0x201EU
#define MTP_RESPONSE_TRANSACTION_CANCELLED                 0x201FU
#define MTP_RESPONSE_INVALID_OBJECT_PROP_CODE              0xA801U
#define MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED    0xA807U
#define MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED             0xA80AU

/* MTP Object format codes */
#define MTP_OBJ_FORMAT_UNDEFINED            0x3000U
#define MTP_OBJ_FORMAT_ASSOCIATION          0x3001U
#define MTP_OBJ_FORMAT_SCRIPT               0x3002U
#define MTP_OBJ_FORMAT_EXECUTABLE           0x3003U
#define MTP_OBJ_FORMAT_TEXT                 0x3004U
#define MTP_OBJ_FORMAT_HTML                 0x3005U
#define MTP_OBJ_FORMAT_DPOF                 0x3006U
#define MTP_OBJ_FORMAT_AIFF                 0x3007U
#define MTP_OBJ_FORMAT_WAV                  0x3008U
#define MTP_OBJ_FORMAT_MP3                  0x3009U
#define MTP_OBJ_FORMAT_AVI                  0x300AU
#define MTP_OBJ_FORMAT_MPEG                 0x300BU
#define MTP_OBJ_FORMAT_ASF                  0x300CU
#define MTP_OBJ_FORMAT_DEFINED              0x3800U
#define MTP_OBJ_FORMAT_EXIF_JPEG            0x3801U
#define MTP_OBJ_FORMAT_TIFF_EP              0x3802U
#define MTP_OBJ_FORMAT_FLASHPIX             0x3803U
#define MTP_OBJ_FORMAT_BMP                  0x3804U
#define MTP_OBJ_FORMAT_CIFF                 0x3805U
#define MTP_OBJ_FORMAT_UNDEFINED_RESERVED0  0x3806U
#define MTP_OBJ_FORMAT_GIF                  0x3807U
#define MTP_OBJ_FORMAT_JFIF                 0x3808U
#define MTP_OBJ_FORMAT_CD                   0x3809U
#define MTP_OBJ_FORMAT_PICT                 0x380AU
#define MTP_OBJ_FORMAT_PNG                  0x380BU
#define MTP_OBJ_FORMAT_UNDEFINED_RESERVED1  0x380CU
#define MTP_OBJ_FORMAT_TIFF                 0x380DU
#define MTP_OBJ_FORMAT_TIFF_IT              0x380EU
#define MTP_OBJ_FORMAT_JP2                  0x380FU
#define MTP_OBJ_FORMAT_JPX                  0x3810U
#define MTP_OBJ_FORMAT_UNDEFINED_FIRMWARE   0xB802U
#define MTP_OBJ_FORMAT_WINDOWS_IMAGE_FORMAT 0xB881U
#define MTP_OBJ_FORMAT_UNDEFINED_AUDIO      0xB900U
#define MTP_OBJ_FORMAT_WMA                  0xB901U
#define MTP_OBJ_FORMAT_OGG                  0xB902U
#define MTP_OBJ_FORMAT_AAC                  0xB903U
#define MTP_OBJ_FORMAT_AUDIBLE              0xB904U
#define MTP_OBJ_FORMAT_FLAC                 0xB906U
#define MTP_OBJ_FORMAT_UNDEFINED_VIDEO      0xB980U
#define MTP_OBJ_FORMAT_WMV                  0xB981U
#define MTP_OBJ_FORMAT_MP4_CONTAINER        0xB982U
#define MTP_OBJ_FORMAT_MP2                  0xB983U
#define MTP_OBJ_FORMAT_3GP_CONTAINER        0xB984U

/*  MTP event codes*/
#define MTP_EVENT_UNDEFINED               0x4000U
#define MTP_EVENT_CANCELTRANSACTION       0x4001U
#define MTP_EVENT_OBJECTADDED             0x4002U
#define MTP_EVENT_OBJECTREMOVED           0x4003U
#define MTP_EVENT_STOREADDED              0x4004U
#define MTP_EVENT_STOREREMOVED            0x4005U
#define MTP_EVENT_DEVICEPROPCHANGED       0x4006U
#define MTP_EVENT_OBJECTINFOCHANGED       0x4007U
#define MTP_EVENT_DEVICEINFOCHANGED       0x4008U
#define MTP_EVENT_REQUESTOBJECTTRANSFER   0x4009U
#define MTP_EVENT_STOREFULL               0x400AU
#define MTP_EVENT_DEVICERESET             0x400BU
#define MTP_EVENT_STORAGEINFOCHANGED      0x400CU
#define MTP_EVENT_CAPTURECOMPLETE         0x400DU
#define MTP_EVENT_UNREPORTEDSTATUS        0x400EU
#define MTP_EVENT_OBJECTPROPCHANGED       0xC801U
#define MTP_EVENT_OBJECTPROPDESCCHANGED   0xC802U
#define MTP_EVENT_OBJECTREFERENCESCHANGED 0xC803U

/* MTP device properties code*/
#define MTP_DEV_PROP_UNDEFINED                      0x5000U
#define MTP_DEV_PROP_BATTERY_LEVEL                  0x5001U
#define MTP_DEV_PROP_FUNCTIONAL_MODE                0x5002U
#define MTP_DEV_PROP_IMAGE_SIZE                     0x5003U
#define MTP_DEV_PROP_COMPRESSION_SETTING            0x5004U
#define MTP_DEV_PROP_WHITE_BALANCE                  0x5005U
#define MTP_DEV_PROP_RGB_GAIN                       0x5006U
#define MTP_DEV_PROP_F_NUMBER                       0x5007U
#define MTP_DEV_PROP_FOCAL_LENGTH                   0x5008U
#define MTP_DEV_PROP_FOCUS_DISTANCE                 0x5009U
#define MTP_DEV_PROP_FOCUS_MODE                     0x500AU
#define MTP_DEV_PROP_EXPOSURE_METERING_MODE         0x500BU
#define MTP_DEV_PROP_FLASH_MODE                     0x500CU
#define MTP_DEV_PROP_EXPOSURE_TIME                  0x500DU
#define MTP_DEV_PROP_EXPOSURE_PROGRAM_MODE          0x500EU
#define MTP_DEV_PROP_EXPOSURE_INDEX                 0x500FU
#define MTP_DEV_PROP_EXPOSURE_BIAS_COMPENSATION     0x5010U
#define MTP_DEV_PROP_DATETIME                       0x5011U
#define MTP_DEV_PROP_CAPTURE_DELAY                  0x5012U
#define MTP_DEV_PROP_STILL_CAPTURE_MODE             0x5013U
#define MTP_DEV_PROP_CONTRAST                       0x5014U
#define MTP_DEV_PROP_SHARPNESS                      0x5015U
#define MTP_DEV_PROP_DIGITAL_ZOOM                   0x5016U
#define MTP_DEV_PROP_EFFECT_MODE                    0x5017U
#define MTP_DEV_PROP_BURST_NUMBER                   0x5018U
#define MTP_DEV_PROP_BURST_INTERVAL                 0x5019U
#define MTP_DEV_PROP_TIMELAPSE_NUMBER               0x501AU
#define MTP_DEV_PROP_TIMELAPSE_INTERVAL             0x501BU
#define MTP_DEV_PROP_FOCUS_METERING_MODE            0x501CU
#define MTP_DEV_PROP_UPLOAD_URL                     0x501DU
#define MTP_DEV_PROP_ARTIST                         0x501EU
#define MTP_DEV_PROP_COPYRIGHT_INFO                 0x501FU
#define MTP_DEV_PROP_SYNCHRONIZATION_PARTNER        0xD401U
#define MTP_DEV_PROP_DEVICE_FRIENDLY_NAME           0xD402U
#define MTP_DEV_PROP_VOLUME                         0xD403U
#define MTP_DEV_PROP_SUPPORTEDFORMATSORDERED        0xD404U
#define MTP_DEV_PROP_DEVICEICON                     0xD405U
#define MTP_DEV_PROP_PLAYBACK_RATE                  0xD410U
#define MTP_DEV_PROP_PLAYBACK_OBJECT                0xD411U
#define MTP_DEV_PROP_PLAYBACK_CONTAINER             0xD412U
#define MTP_DEV_PROP_SESSION_INITIATOR_VERSION_INFO 0xD406U
#define MTP_DEV_PROP_PERCEIVED_DEVICE_TYPE          0xD407U

/*
 * MTP Class specification Revision 1.1
 * Appendix B. Object Properties
 */

/* MTP OBJECT PROPERTIES supported*/
#define MTP_OB_PROP_STORAGE_ID                          0xDC01U
#define MTP_OB_PROP_OBJECT_FORMAT                       0xDC02U
#define MTP_OB_PROP_PROTECTION_STATUS                   0xDC03U
#define MTP_OB_PROP_OBJECT_SIZE                         0xDC04U
#define MTP_OB_PROP_ASSOC_TYPE                          0xDC05U
#define MTP_OB_PROP_ASSOC_DESC                          0xDC06U
#define MTP_OB_PROP_OBJ_FILE_NAME                       0xDC07U
#define MTP_OB_PROP_DATE_CREATED                        0xDC08U
#define MTP_OB_PROP_DATE_MODIFIED                       0xDC09U
#define MTP_OB_PROP_KEYWORDS                            0xDC0AU
#define MTP_OB_PROP_PARENT_OBJECT                       0xDC0BU
#define MTP_OB_PROP_ALLOWED_FOLD_CONTENTS               0xDC0CU
#define MTP_OB_PROP_HIDDEN                              0xDC0DU
#define MTP_OB_PROP_SYSTEM_OBJECT                       0xDC0EU
#define MTP_OB_PROP_PERS_UNIQ_OBJ_IDEN                  0xDC41U
#define MTP_OB_PROP_SYNCID                              0xDC42U
#define MTP_OB_PROP_PROPERTY_BAG                        0xDC43U
#define MTP_OB_PROP_NAME                                0xDC44U
#define MTP_OB_PROP_CREATED_BY                          0xDC45U
#define MTP_OB_PROP_ARTIST                              0xDC46U
#define MTP_OB_PROP_DATE_AUTHORED                       0xDC47U
#define MTP_OB_PROP_DESCRIPTION                         0xDC48U
#define MTP_OB_PROP_URL_REFERENCE                       0xDC49U
#define MTP_OB_PROP_LANGUAGELOCALE                      0xDC4AU
#define MTP_OB_PROP_COPYRIGHT_INFORMATION               0xDC4BU
#define MTP_OB_PROP_SOURCE                              0xDC4CU
#define MTP_OB_PROP_ORIGIN_LOCATION                     0xDC4DU
#define MTP_OB_PROP_DATE_ADDED                          0xDC4EU
#define MTP_OB_PROP_NON_CONSUMABLE                      0xDC4FU
#define MTP_OB_PROP_CORRUPTUNPLAYABLE                   0xDC50U
#define MTP_OB_PROP_PRODUCERSERIALNUMBER                0xDC51U
#define MTP_OB_PROP_REPRESENTATIVE_SAMPLE_FORMAT        0xDC81U
#define MTP_OB_PROP_REPRESENTATIVE_SAMPLE_SIZE          0xDC82U
#define MTP_OB_PROP_REPRESENTATIVE_SAMPLE_HEIGHT        0xDC83U
#define MTP_OB_PROP_REPRESENTATIVE_SAMPLE_WIDTH         0xDC84U
#define MTP_OB_PROP_REPRESENTATIVE_SAMPLE_DURATION      0xDC85U
#define MTP_OB_PROP_REPRESENTATIVE_SAMPLE_DATA          0xDC86U
#define MTP_OB_PROP_WIDTH                               0xDC87U
#define MTP_OB_PROP_HEIGHT                              0xDC88U
#define MTP_OB_PROP_DURATION                            0xDC89U
#define MTP_OB_PROP_RATING                              0xDC8AU
#define MTP_OB_PROP_TRACK                               0xDC8BU
#define MTP_OB_PROP_GENRE                               0xDC8CU
#define MTP_OB_PROP_CREDITS                             0xDC8DU
#define MTP_OB_PROP_LYRICS                              0xDC8EU
#define MTP_OB_PROP_SUBSCRIPTION_CONTENT_ID             0xDC8FU
#define MTP_OB_PROP_PRODUCED_BY                         0xDC90U
#define MTP_OB_PROP_USE_COUNT                           0xDC91U
#define MTP_OB_PROP_SKIP_COUNT                          0xDC92U
#define MTP_OB_PROP_LAST_ACCESSED                       0xDC93U
#define MTP_OB_PROP_PARENTAL_RATING                     0xDC94U
#define MTP_OB_PROP_META_GENRE                          0xDC95U
#define MTP_OB_PROP_COMPOSER                            0xDC96U
#define MTP_OB_PROP_EFFECTIVE_RATING                    0xDC97U
#define MTP_OB_PROP_SUBTITLE                            0xDC98U
#define MTP_OB_PROP_ORIGINAL_RELEASE_DATE               0xDC99U
#define MTP_OB_PROP_ALBUM_NAME                          0xDC9AU
#define MTP_OB_PROP_ALBUM_ARTIST                        0xDC9BU
#define MTP_OB_PROP_MOOD                                0xDC9CU
#define MTP_OB_PROP_DRM_STATUS                          0xDC9DU
#define MTP_OB_PROP_SUB_DESCRIPTION                     0xDC9EU
#define MTP_OB_PROP_IS_CROPPED                          0xDCD1U
#define MTP_OB_PROP_IS_COLOUR_CORRECTED                 0xDCD2U
#define MTP_OB_PROP_IMAGE_BIT_DEPTH                     0xDCD3U
#define MTP_OB_PROP_FNUMBER                             0xDCD4U
#define MTP_OB_PROP_EXPOSURE_TIME                       0xDCD5U
#define MTP_OB_PROP_EXPOSURE_INDEX                      0xDCD6U
#define MTP_OB_PROP_TOTAL_BITRATE                       0xDE91U
#define MTP_OB_PROP_BITRATE_TYPE                        0xDE92U
#define MTP_OB_PROP_SAMPLE_RATE                         0xDE93U
#define MTP_OB_PROP_NUMBER_OF_CHANNELS                  0xDE94U
#define MTP_OB_PROP_AUDIO_BITDEPTH                      0xDE95U
#define MTP_OB_PROP_SCAN_TYPE                           0xDE97U
#define MTP_OB_PROP_AUDIO_WAVE_CODEC                    0xDE99U
#define MTP_OB_PROP_AUDIO_BITRATE                       0xDE9AU
#define MTP_OB_PROP_VIDEO_FOURCC_CODEC                  0xDE9BU
#define MTP_OB_PROP_VIDEO_BITRATE                       0xDE9CU
#define MTP_OB_PROP_FRAMES_PER_THOUSAND_SECONDS         0xDE9DU
#define MTP_OB_PROP_KEYFRAME_DISTANCE                   0xDE9EU
#define MTP_OB_PROP_BUFFER_SIZE                         0xDE9FU
#define MTP_OB_PROP_ENCODING_QUALITY                    0xDEA0U
#define MTP_OB_PROP_ENCODING_PROFILE                    0xDEA1U
#define MTP_OB_PROP_DISPLAY_NAME                        0xDCE0U
#define MTP_OB_PROP_BODY_TEXT                           0xDCE1U
#define MTP_OB_PROP_SUBJECT                             0xDCE2U
#define MTP_OB_PROP_PRIORITY                            0xDCE3U
#define MTP_OB_PROP_GIVEN_NAME                          0xDD00U
#define MTP_OB_PROP_MIDDLE_NAMES                        0xDD01U
#define MTP_OB_PROP_FAMILY_NAME                         0xDD02U
#define MTP_OB_PROP_PREFIX                              0xDD03U
#define MTP_OB_PROP_SUFFIX                              0xDD04U
#define MTP_OB_PROP_PHONETIC_GIVEN_NAME                 0xDD05U
#define MTP_OB_PROP_PHONETIC_FAMILY_NAME                0xDD06U
#define MTP_OB_PROP_EMAIL_PRIMARY                       0xDD07U
#define MTP_OB_PROP_EMAIL_PERSONAL_1                    0xDD08U
#define MTP_OB_PROP_EMAIL_PERSONAL_2                    0xDD09U
#define MTP_OB_PROP_EMAIL_BUSINESS_1                    0xDD0AU
#define MTP_OB_PROP_EMAIL_BUSINESS_2                    0xDD0BU
#define MTP_OB_PROP_EMAIL_OTHERS                        0xDD0CU
#define MTP_OB_PROP_PHONE_NUMBER_PRIMARY                0xDD0DU
#define MTP_OB_PROP_PHONE_NUMBER_PERSONAL               0xDD0EU
#define MTP_OB_PROP_PHONE_NUMBER_PERSONAL_2             0xDD0FU
#define MTP_OB_PROP_PHONE_NUMBER_BUSINESS               0xDD10U
#define MTP_OB_PROP_PHONE_NUMBER_BUSINESS_2             0xDD11U
#define MTP_OB_PROP_PHONE_NUMBER_MOBILE                 0xDD12U
#define MTP_OB_PROP_PHONE_NUMBER_MOBILE_2               0xDD13U
#define MTP_OB_PROP_FAX_NUMBER_PRIMARY                  0xDD14U
#define MTP_OB_PROP_FAX_NUMBER_PERSONAL                 0xDD15U
#define MTP_OB_PROP_FAX_NUMBER_BUSINESS                 0xDD16U
#define MTP_OB_PROP_PAGER_NUMBER                        0xDD17U
#define MTP_OB_PROP_PHONE_NUMBER_OTHERS                 0xDD18U
#define MTP_OB_PROP_PRIMARY_WEB_ADDRESS                 0xDD19U
#define MTP_OB_PROP_PERSONAL_WEB_ADDRESS                0xDD1AU
#define MTP_OB_PROP_BUSINESS_WEB_ADDRESS                0xDD1BU
#define MTP_OB_PROP_INSTANT_MESSENGER_ADDRESS           0xDD1CU
#define MTP_OB_PROP_INSTANT_MESSENGER_ADDRESS_2         0xDD1DU
#define MTP_OB_PROP_INSTANT_MESSENGER_ADDRESS_3         0xDD1EU
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_FULL        0xDD1FU
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_LINE_1      0xDD20U
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_LINE_2      0xDD21U
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_CITY        0xDD22U
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_REGION      0xDD23U
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_POSTAL_CODE 0xDD24U
#define MTP_OB_PROP_POSTAL_ADDRESS_PERSONAL_COUNTRY     0xDD25U
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_FULL        0xDD26U
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_LINE_1      0xDD27U
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_LINE_2      0xDD28U
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_CITY        0xDD29U
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_REGION      0xDD2AU
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_POSTAL_CODE 0xDD2BU
#define MTP_OB_PROP_POSTAL_ADDRESS_BUSINESS_COUNTRY     0xDD2CU
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_FULL           0xDD2DU
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_LINE_1         0xDD2EU
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_LINE_2         0xDD2FU
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_CITY           0xDD30U
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_REGION         0xDD31U
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_POSTAL_CODE    0xDD32U
#define MTP_OB_PROP_POSTAL_ADDRESS_OTHER_COUNTRY        0xDD33U
#define MTP_OB_PROP_ORGANIZATION_NAME                   0xDD34U
#define MTP_OB_PROP_PHONETIC_ORGANIZATION_NAME          0xDD35U
#define MTP_OB_PROP_ROLE                                0xDD36U
#define MTP_OB_PROP_BIRTHDATE                           0xDD37U
#define MTP_OB_PROP_MESSAGE_TO                          0xDD40U
#define MTP_OB_PROP_MESSAGE_CC                          0xDD41U
#define MTP_OB_PROP_MESSAGE_BCC                         0xDD42U
#define MTP_OB_PROP_MESSAGE_READ                        0xDD43U
#define MTP_OB_PROP_MESSAGE_RECEIVED_TIME               0xDD44U
#define MTP_OB_PROP_MESSAGE_SENDER                      0xDD45U
#define MTP_OB_PROP_ACT_BEGIN_TIME                      0xDD50U
#define MTP_OB_PROP_ACT_END_TIME                        0xDD51U
#define MTP_OB_PROP_ACT_LOCATION                        0xDD52U
#define MTP_OB_PROP_ACT_REQUIRED_ATTENDEES              0xDD54U
#define MTP_OB_PROP_ACT_OPTIONAL_ATTENDEES              0xDD55U
#define MTP_OB_PROP_ACT_RESOURCES                       0xDD56U
#define MTP_OB_PROP_ACT_ACCEPTED                        0xDD57U
#define MTP_OB_PROP_OWNER                               0xDD5DU
#define MTP_OB_PROP_EDITOR                              0xDD5EU
#define MTP_OB_PROP_WEBMASTER                           0xDD5FU
#define MTP_OB_PROP_URL_SOURCE                          0xDD60U
#define MTP_OB_PROP_URL_DESTINATION                     0xDD61U
#define MTP_OB_PROP_TIME_BOOKMARK                       0xDD62U
#define MTP_OB_PROP_OBJECT_BOOKMARK                     0xDD63U
#define MTP_OB_PROP_BYTE_BOOKMARK                       0xDD64U
#define MTP_OB_PROP_LAST_BUILD_DATE                     0xDD70U
#define MTP_OB_PROP_TIME_TO_LIVE                        0xDD71U
#define MTP_OB_PROP_MEDIA_GUID                          0xDD72U

/* MTP storage type */
#define MTP_STORAGE_UNDEFINED     0U
#define MTP_STORAGE_FIXED_ROM     0x0001U
#define MTP_STORAGE_REMOVABLE_ROM 0x0002U
#define MTP_STORAGE_FIXED_RAM     0x0003U
#define MTP_STORAGE_REMOVABLE_RAM 0x0004U

/* MTP file system type */
#define MTP_FILESYSTEM_UNDEFINED        0U
#define MTP_FILESYSTEM_GENERIC_FLAT     0x0001U
#define MTP_FILESYSTEM_GENERIC_HIERARCH 0x0002U
#define MTP_FILESYSTEM_DCF              0x0003U

/* MTP access capability */
#define MTP_ACCESS_CAP_RW             0U /* read write */
#define MTP_ACCESS_CAP_RO_WITHOUT_DEL 0x0001U
#define MTP_ACCESS_CAP_RO_WITH_DEL    0x0002U

/* MTP standard data types supported */
#define MTP_DATATYPE_INT8    0x0001U
#define MTP_DATATYPE_UINT8   0x0002U
#define MTP_DATATYPE_INT16   0x0003U
#define MTP_DATATYPE_UINT16  0x0004U
#define MTP_DATATYPE_INT32   0x0005U
#define MTP_DATATYPE_UINT32  0x0006U
#define MTP_DATATYPE_INT64   0x0007U
#define MTP_DATATYPE_UINT64  0x0008U
#define MTP_DATATYPE_UINT128 0x000AU
#define MTP_DATATYPE_STR     0xFFFFU

/* MTP reading only or reading/writing */
#define MTP_PROP_GET     0x00U
#define MTP_PROP_GET_SET 0x01U

#define MTP_SESSION_CLOSED 0x00
#define MTP_SESSION_OPENED 0x01

struct mtp_container_command {
    uint32_t conlen;
    uint16_t contype;
    uint16_t code;
    uint32_t trans_id;
    uint32_t param1;
    uint32_t param2;
    uint32_t param3;
    uint32_t param4;
    uint32_t param5;
} __PACKED;

struct mtp_container_data {
    uint32_t conlen;
    uint16_t contype;
    uint16_t code;
    uint32_t trans_id;
    uint8_t data[512];
} __PACKED;

struct mtp_container_response {
    uint32_t conlen;
    uint16_t contype;
    uint16_t code;
    uint32_t trans_id;
} __PACKED;

/*Length of template descriptor: 23 bytes*/
#define MTP_DESCRIPTOR_LEN (9 + 7 + 7 + 7)

// clang-format off
#ifndef CONFIG_USB_HS
#define MTP_DESCRIPTOR_INIT(bFirstInterface, out_ep, in_ep, int_ep, str_idx) \
    /* Interface */                                                          \
    0x09,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType */                 \
    bFirstInterface,               /* bInterfaceNumber */                \
    0x00,                          /* bAlternateSetting */               \
    0x03,                          /* bNumEndpoints */                   \
    USB_DEVICE_CLASS_MASS_STORAGE, /* bInterfaceClass */                 \
    USB_MTP_SUB_CLASS,             /* bInterfaceSubClass */              \
    USB_MTP_PROTOCOL,              /* bInterfaceProtocol */              \
    str_idx,                       /* iInterface */                      \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    out_ep,                        /* bEndpointAddress */                \
    0x02,                          /* bmAttributes */                    \
    0x40, 0x00,                    /* wMaxPacketSize */                  \
    0x00,                          /* bInterval */                       \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    in_ep,                         /* bEndpointAddress */                \
    0x02,                          /* bmAttributes */                    \
    0x40, 0x00,                    /* wMaxPacketSize */                  \
    0x00,                          /* bInterval */                       \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    int_ep,                        /* bEndpointAddress */                \
    0x03,                          /* bmAttributes */                    \
    0x1c, 0x00,                    /* wMaxPacketSize */                  \
    0x06                           /* bInterval */
#else
#define MTP_DESCRIPTOR_INIT(bFirstInterface, out_ep, in_ep, int_ep, str_idx) \
    /* Interface */                                                          \
    0x09,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType */                 \
    bFirstInterface,               /* bInterfaceNumber */                \
    0x00,                          /* bAlternateSetting */               \
    0x03,                          /* bNumEndpoints */                   \
    USB_DEVICE_CLASS_MASS_STORAGE, /* bInterfaceClass */                 \
    USB_MTP_SUB_CLASS,             /* bInterfaceSubClass */              \
    USB_MTP_PROTOCOL,              /* bInterfaceProtocol */              \
    str_idx,                       /* iInterface */                      \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    out_ep,                        /* bEndpointAddress */                \
    0x02,                          /* bmAttributes */                    \
    0x00, 0x02,                    /* wMaxPacketSize */                  \
    0x00,                          /* bInterval */                       \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    in_ep,                         /* bEndpointAddress */                \
    0x02,                          /* bmAttributes */                    \
    0x00, 0x02,                    /* wMaxPacketSize */                  \
    0x00,                          /* bInterval */                       \
    0x07,                          /* bLength */                         \
    USB_DESCRIPTOR_TYPE_ENDPOINT,  /* bDescriptorType */                 \
    int_ep,                        /* bEndpointAddress */                \
    0x03,                          /* bmAttributes */                    \
    0x1c, 0x00,                    /* wMaxPacketSize */                  \
    0x06                           /* bInterval */
#endif
// clang-format on

#endif /* USB_MTP_H */
