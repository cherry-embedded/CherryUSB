/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_MTP_CONFIG_H
#define USB_MTP_CONFIG_H

#include "usb_mtp.h"

static const uint16_t VendExtDesc[] = { 'm', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', '.', 'c', 'o', 'm', ':', ' ', '1', '.', '0', ';', ' ', 0 }; /* last 2 bytes must be 0*/

static const uint16_t SuppOP[] = { MTP_OP_GET_DEVICE_INFO, MTP_OP_OPEN_SESSION, MTP_OP_CLOSE_SESSION,
                                   MTP_OP_GET_STORAGE_IDS, MTP_OP_GET_STORAGE_INFO, MTP_OP_GET_NUM_OBJECTS,
                                   MTP_OP_GET_OBJECT_HANDLES, MTP_OP_GET_OBJECT_INFO, MTP_OP_GET_OBJECT,
                                   MTP_OP_DELETE_OBJECT, MTP_OP_SEND_OBJECT_INFO, MTP_OP_SEND_OBJECT,
                                   MTP_OP_GET_DEVICE_PROP_DESC, MTP_OP_GET_DEVICE_PROP_VALUE,
                                   MTP_OP_SET_OBJECT_PROP_VALUE, MTP_OP_GET_OBJECT_PROP_VALUE,
                                   MTP_OP_GET_OBJECT_PROPS_SUPPORTED, MTP_OP_GET_OBJECT_PROPLIST,
                                   MTP_OP_GET_OBJECT_PROP_DESC, MTP_OP_GET_OBJECT_PROP_REFERENCES };

static const uint16_t SuppEvents[] = { MTP_EVENT_OBJECTADDED };

static const uint16_t DevicePropSupp[] = { MTP_DEV_PROP_DEVICE_FRIENDLY_NAME, MTP_DEV_PROP_BATTERY_LEVEL };

static const uint16_t SuppCaptFormat[] = { MTP_OBJ_FORMAT_UNDEFINED, MTP_OBJ_FORMAT_ASSOCIATION, MTP_OBJ_FORMAT_TEXT };

static const uint16_t SuppImgFormat[] = { MTP_OBJ_FORMAT_UNDEFINED, MTP_OBJ_FORMAT_TEXT, MTP_OBJ_FORMAT_ASSOCIATION,
                                          MTP_OBJ_FORMAT_EXECUTABLE, MTP_OBJ_FORMAT_WAV, MTP_OBJ_FORMAT_MP3,
                                          MTP_OBJ_FORMAT_EXIF_JPEG, MTP_OBJ_FORMAT_MPEG, MTP_OBJ_FORMAT_MP4_CONTAINER,
                                          MTP_OBJ_FORMAT_WINDOWS_IMAGE_FORMAT, MTP_OBJ_FORMAT_PNG, MTP_OBJ_FORMAT_WMA,
                                          MTP_OBJ_FORMAT_WMV };

static const uint16_t Manuf[] = { 'C', 'h', 'e', 'r', 'r', 'y', 'U', 'S', 'B', 0 }; /* last 2 bytes must be 0*/
static const uint16_t Model[] = { 'C', 'h', 'e', 'r', 'r', 'y', 'U', 'S', 'B', 0 }; /* last 2 bytes must be 0*/
static const uint16_t DeviceVers[] = { 'V', '1', '.', '0', '0', 0 };                /* last 2 bytes must be 0*/
/*SerialNbr shall be 32 character hexadecimal string for legacy compatibility reasons */
static const uint16_t SerialNbr[] = { '0', '0', '0', '0', '1', '0', '0', '0', '0', '1', '0', '0', '0', '0',
                                      '1', '0', '0', '0', '0', '1', '0', '0', '0', '0', '1', '0', '0', '0',
                                      '0', '1', '0', '0', 0 }; /* last 2 bytes must be 0*/

static const uint16_t DefaultFileName[] = { 'N', 'e', 'w', ' ', 'F', 'o', 'l', 'd', 'e', 'r', 0 };

static const uint16_t DevicePropDefVal[] = { 'C', 'h', 'e', 'r', 'r', 'y', 'U', 'S', 'B', 0 }; /* last 2 bytes must be 0*/
static const uint16_t DevicePropCurDefVal[] = { 'C', 'h', 'e', 'r', 'r', 'y', 'U', 'S', 'B', 0 };

/* required for all object format : storageID, objectFormat, ObjectCompressedSize,
persistent unique object identifier, name*/
static const uint16_t ObjectPropCode[] = { MTP_OB_PROP_STORAGE_ID, MTP_OB_PROP_OBJECT_FORMAT, MTP_OB_PROP_OBJECT_SIZE,
                                           MTP_OB_PROP_OBJ_FILE_NAME, MTP_OB_PROP_PARENT_OBJECT, MTP_OB_PROP_NAME,
                                           MTP_OB_PROP_PERS_UNIQ_OBJ_IDEN, MTP_OB_PROP_PROTECTION_STATUS };

#define MTP_STORAGE_ID 0x00010001U /* SD card is inserted*/

#define CONFIG_MTP_VEND_EXT_DESC_LEN        (sizeof(VendExtDesc) / 2U)
#define CONFIG_MTP_SUPP_OP_LEN              (sizeof(SuppOP) / 2U)
#define CONFIG_MTP_SUPP_EVENTS_LEN          (sizeof(SuppEvents) / 2U)
#define CONFIG_MTP_SUPP_DEVICE_PROP_LEN     (sizeof(DevicePropSupp) / 2U)
#define CONFIG_MTP_SUPP_CAPT_FORMAT_LEN     (sizeof(SuppCaptFormat) / 2U)
#define CONFIG_MTP_SUPP_IMG_FORMAT_LEN      (sizeof(SuppImgFormat) / 2U)
#define CONFIG_MTP_MANUF_LEN                (sizeof(Manuf) / 2U)
#define CONFIG_MTP_MODEL_LEN                (sizeof(Model) / 2U)
#define CONFIG_MTP_DEVICE_VERSION_LEN       (sizeof(DeviceVers) / 2U)
#define CONFIG_MTP_SERIAL_NBR_LEN           (sizeof(SerialNbr) / 2U)
#define CONFIG_MTP_SUPP_OBJ_PROP_LEN        (sizeof(ObjectPropCode) / 2U)
#define CONFIG_MTP_DEVICE_PROP_DESC_DEF_LEN (sizeof(DevicePropDefVal) / 2U)
#define CONFIG_MTP_DEVICE_PROP_DESC_CUR_LEN (sizeof(DevicePropCurDefVal) / 2U)
#define CONFIG_MTP_STORAGE_ID_LEN           1
#define CONFIG_MTP_OBJECT_HANDLE_LEN        100

struct mtp_device_info {
    uint16_t StandardVersion;
    uint32_t VendorExtensionID;
    uint16_t VendorExtensionVersion;
    uint8_t VendorExtensionDesc_len;
    uint16_t VendorExtensionDesc[CONFIG_MTP_VEND_EXT_DESC_LEN];
    uint16_t FunctionalMode;
    uint32_t OperationsSupported_len;
    uint16_t OperationsSupported[CONFIG_MTP_SUPP_OP_LEN];
    uint32_t EventsSupported_len;
    uint16_t EventsSupported[CONFIG_MTP_SUPP_EVENTS_LEN];
    uint32_t DevicePropertiesSupported_len;
    uint16_t DevicePropertiesSupported[CONFIG_MTP_SUPP_DEVICE_PROP_LEN];
    uint32_t CaptureFormats_len;
    uint16_t CaptureFormats[CONFIG_MTP_SUPP_CAPT_FORMAT_LEN];
    uint32_t ImageFormats_len;
    uint16_t ImageFormats[CONFIG_MTP_SUPP_IMG_FORMAT_LEN];
    uint8_t Manufacturer_len;
    uint16_t Manufacturer[CONFIG_MTP_MANUF_LEN];
    uint8_t Model_len;
    uint16_t Model[CONFIG_MTP_MODEL_LEN];
    uint8_t DeviceVersion_len;
    uint16_t DeviceVersion[CONFIG_MTP_DEVICE_VERSION_LEN];
    uint8_t SerialNumber_len;
    uint16_t SerialNumber[CONFIG_MTP_SERIAL_NBR_LEN];
} __PACKED;

struct mtp_object_props_support {
    uint32_t ObjectPropCode_len;
    uint16_t ObjectPropCode[CONFIG_MTP_SUPP_OBJ_PROP_LEN];
} __PACKED;

struct mtp_device_prop_desc {
    uint16_t DevicePropertyCode;
    uint16_t DataType;
    uint8_t GetSet;
    uint8_t DefaultValue_len;
    uint16_t DefaultValue[CONFIG_MTP_DEVICE_PROP_DESC_DEF_LEN];
    uint8_t CurrentValue_len;
    uint16_t CurrentValue[CONFIG_MTP_DEVICE_PROP_DESC_CUR_LEN];
    uint8_t FormFlag;
} __PACKED;

struct mtp_storage_id {
    uint32_t StorageIDS_len;
    uint32_t StorageIDS[CONFIG_MTP_STORAGE_ID_LEN];
} __PACKED;

struct mtp_storage_info {
    uint16_t StorageType;
    uint16_t FilesystemType;
    uint16_t AccessCapability;
    uint64_t MaxCapability;
    uint64_t FreeSpaceInBytes;
    uint32_t FreeSpaceInObjects;
    uint8_t StorageDescription;
    uint8_t VolumeLabel;
} __PACKED;

struct mtp_object_handle {
    uint32_t ObjectHandle_len;
    uint32_t ObjectHandle[CONFIG_MTP_OBJECT_HANDLE_LEN];
} __PACKED;

struct mtp_object_info {
    uint32_t Storage_id;
    uint16_t ObjectFormat;
    uint16_t ProtectionStatus;
    uint32_t ObjectCompressedSize;
    uint16_t ThumbFormat;
    uint32_t ThumbCompressedSize;
    uint32_t ThumbPixWidth;
    uint32_t ThumbPixHeight;
    uint32_t ImagePixWidth;
    uint32_t ImagePixHeight;
    uint32_t ImageBitDepth;
    uint32_t ParentObject;
    uint16_t AssociationType;
    uint32_t AssociationDesc;
    uint32_t SequenceNumber;
    uint8_t Filename_len;
    uint16_t Filename[255];
    uint32_t CaptureDate;
    uint32_t ModificationDate;
    uint8_t Keywords;
} __PACKED;

struct mtp_object_prop_desc {
    uint16_t ObjectPropertyCode;
    uint16_t DataType;
    uint8_t GetSet;
    uint8_t *DefValue;
    uint32_t GroupCode;
    uint8_t FormFlag;
} __PACKED;

struct mtp_object_prop_element {
    uint32_t ObjectHandle;
    uint16_t PropertyCode;
    uint16_t Datatype;
    uint8_t *propval;
} __PACKED;

struct mtp_object_prop_list {
    uint32_t Properties_len;
    struct mtp_object_prop_element Properties[CONFIG_MTP_SUPP_OBJ_PROP_LEN];
} __PACKED;

#endif /* USB_MTP_CONFIG_H */