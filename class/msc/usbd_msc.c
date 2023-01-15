/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_msc.h"
#include "usb_scsi.h"

#define MSD_OUT_EP_IDX 0
#define MSD_IN_EP_IDX  1

/* Describe EndPoints configuration */
static struct usbd_endpoint mass_ep_data[2];

/* MSC Bulk-only Stage */
enum Stage {
    MSC_READ_CBW = 0, /* Command Block Wrapper */
    MSC_DATA_OUT = 1, /* Data Out Phase */
    MSC_DATA_IN = 2,  /* Data In Phase */
    MSC_SEND_CSW = 3, /* Command Status Wrapper */
    MSC_WAIT_CSW = 4, /* Command Status Wrapper */
};

/* Device data structure */
USB_NOCACHE_RAM_SECTION struct usbd_msc_cfg_priv {
    /* state of the bulk-only state machine */
    enum Stage stage;
    USB_MEM_ALIGNX struct CBW cbw;
    USB_MEM_ALIGNX struct CSW csw;

    bool readonly;
    uint8_t sKey; /* Sense key */
    uint8_t ASC;  /* Additional Sense Code */
    uint8_t ASQ;  /* Additional Sense Qualifier */
    uint8_t max_lun;
    uint32_t start_sector;
    uint32_t nsectors;
    uint16_t scsi_blk_size;
    uint32_t scsi_blk_nbr;

    uint8_t block_buffer[CONFIG_USBDEV_MSC_BLOCK_SIZE];
} usbd_msc_cfg;

static void usbd_msc_reset(void)
{
    usbd_msc_cfg.stage = MSC_READ_CBW;
    usbd_msc_cfg.readonly = false;
}

static int msc_storage_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("MSC Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    switch (setup->bRequest) {
        case MSC_REQUEST_RESET:
            usbd_msc_reset();
            break;

        case MSC_REQUEST_GET_MAX_LUN:
            (*data)[0] = usbd_msc_cfg.max_lun;
            *len = 1;
            break;

        default:
            USB_LOG_WRN("Unhandled MSC Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

void msc_storage_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            usbd_msc_reset();
            break;
        case USBD_EVENT_CONFIGURED:
            USB_LOG_DBG("Start reading cbw\r\n");
            usbd_ep_start_read(mass_ep_data[MSD_OUT_EP_IDX].ep_addr, (uint8_t *)&usbd_msc_cfg.cbw, USB_SIZEOF_MSC_CBW);
            break;

        default:
            break;
    }
}

static void usbd_msc_bot_abort(void)
{
    if ((usbd_msc_cfg.cbw.bmFlags == 0) && (usbd_msc_cfg.cbw.dDataLength != 0)) {
        usbd_ep_set_stall(mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
    }
    usbd_ep_set_stall(mass_ep_data[MSD_IN_EP_IDX].ep_addr);
    usbd_ep_start_read(mass_ep_data[0].ep_addr, (uint8_t *)&usbd_msc_cfg.cbw, USB_SIZEOF_MSC_CBW);
}

static void usbd_msc_send_csw(uint8_t CSW_Status)
{
    usbd_msc_cfg.csw.dSignature = MSC_CSW_Signature;
    usbd_msc_cfg.csw.bStatus = CSW_Status;

    /* updating the State Machine , so that we wait CSW when this
	 * transfer is complete, ie when we get a bulk in callback
	 */
    usbd_msc_cfg.stage = MSC_WAIT_CSW;

    USB_LOG_DBG("Send csw\r\n");
    usbd_ep_start_write(mass_ep_data[MSD_IN_EP_IDX].ep_addr, (uint8_t *)&usbd_msc_cfg.csw, sizeof(struct CSW));
}

static void usbd_msc_send_info(uint8_t *buffer, uint8_t size)
{
    size = MIN(size, usbd_msc_cfg.cbw.dDataLength);

    /* updating the State Machine , so that we send CSW when this
	 * transfer is complete, ie when we get a bulk in callback
	 */
    usbd_msc_cfg.stage = MSC_SEND_CSW;

    usbd_ep_start_write(mass_ep_data[MSD_IN_EP_IDX].ep_addr, buffer, size);

    usbd_msc_cfg.csw.dDataResidue -= size;
    usbd_msc_cfg.csw.bStatus = CSW_STATUS_CMD_PASSED;
}

static bool SCSI_processWrite(uint32_t nbytes);
static bool SCSI_processRead(void);

/**
* @brief  SCSI_SetSenseData
*         Load the last error code in the error list
* @param  sKey: Sense Key
* @param  ASC: Additional Sense Code
* @retval none

*/
static void SCSI_SetSenseData(uint32_t KCQ)
{
    usbd_msc_cfg.sKey = (uint8_t)(KCQ >> 16);
    usbd_msc_cfg.ASC = (uint8_t)(KCQ >> 8);
    usbd_msc_cfg.ASQ = (uint8_t)(KCQ);
}

/**
 * @brief SCSI Command list
 *
 */

static bool SCSI_testUnitReady(uint8_t **data, uint32_t *len)
{
    if (usbd_msc_cfg.cbw.dDataLength != 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    *data = NULL;
    *len = 0;
    return true;
}

static bool SCSI_requestSense(uint8_t **data, uint32_t *len)
{
    uint8_t data_len = SCSIRESP_FIXEDSENSEDATA_SIZEOF;
    if (usbd_msc_cfg.cbw.dDataLength == 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if (usbd_msc_cfg.cbw.CB[4] < SCSIRESP_FIXEDSENSEDATA_SIZEOF) {
        data_len = usbd_msc_cfg.cbw.CB[4];
    }

    uint8_t request_sense[SCSIRESP_FIXEDSENSEDATA_SIZEOF] = {
        0x70,
        0x00,
        0x00, /* Sense Key */
        0x00,
        0x00,
        0x00,
        0x00,
        SCSIRESP_FIXEDSENSEDATA_SIZEOF - 8,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, /* Additional Sense Code */
        0x00, /* Additional Sense Request */
        0x00,
        0x00,
        0x00,
        0x00,
    };

    request_sense[2] = usbd_msc_cfg.sKey;
    request_sense[12] = usbd_msc_cfg.ASC;
    request_sense[13] = usbd_msc_cfg.ASQ;
#if 0
    request_sense[ 2] = 0x06;           /* UNIT ATTENTION */
    request_sense[12] = 0x28;           /* Additional Sense Code: Not ready to ready transition */
    request_sense[13] = 0x00;           /* Additional Sense Code Qualifier */
#endif
#if 0
    request_sense[ 2] = 0x02;           /* NOT READY */
    request_sense[12] = 0x3A;           /* Additional Sense Code: Medium not present */
    request_sense[13] = 0x00;           /* Additional Sense Code Qualifier */
#endif
#if 0
    request_sense[ 2] = 0x05;         /* ILLEGAL REQUEST */
    request_sense[12] = 0x20;         /* Additional Sense Code: Invalid command */
    request_sense[13] = 0x00;         /* Additional Sense Code Qualifier */
#endif
#if 0
    request_sense[ 2] = 0x00;         /* NO SENSE */
    request_sense[12] = 0x00;         /* Additional Sense Code: No additional code */
    request_sense[13] = 0x00;         /* Additional Sense Code Qualifier */
#endif

    memcpy(*data, (uint8_t *)request_sense, data_len);
    *len = data_len;
    return true;
}

static bool SCSI_inquiry(uint8_t **data, uint32_t *len)
{
    uint8_t data_len = SCSIRESP_INQUIRY_SIZEOF;

    uint8_t inquiry00[6] = {
        0x00,
        0x00,
        0x00,
        (0x06 - 4U),
        0x00,
        0x80
    };

    /* USB Mass storage VPD Page 0x80 Inquiry Data for Unit Serial Number */
    uint8_t inquiry80[8] = {
        0x00,
        0x80,
        0x00,
        0x08,
        0x20, /* Put Product Serial number */
        0x20,
        0x20,
        0x20
    };

    uint8_t inquiry[SCSIRESP_INQUIRY_SIZEOF] = {
        /* 36 */

        /* LUN 0 */
        0x00,
        0x80,
        0x02,
        0x02,
        (SCSIRESP_INQUIRY_SIZEOF - 5),
        0x00,
        0x00,
        0x00,
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', /* Manufacturer : 8 bytes */
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', /* Product      : 16 Bytes */
        ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
        ' ', ' ', ' ', ' ' /* Version      : 4 Bytes */
    };

    memcpy(&inquiry[8], CONFIG_USBDEV_MSC_MANUFACTURER_STRING, strlen(CONFIG_USBDEV_MSC_MANUFACTURER_STRING));
    memcpy(&inquiry[16], CONFIG_USBDEV_MSC_PRODUCT_STRING, strlen(CONFIG_USBDEV_MSC_PRODUCT_STRING));
    memcpy(&inquiry[32], CONFIG_USBDEV_MSC_VERSION_STRING, strlen(CONFIG_USBDEV_MSC_VERSION_STRING));

    if (usbd_msc_cfg.cbw.dDataLength == 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if ((usbd_msc_cfg.cbw.CB[1] & 0x01U) != 0U) { /* Evpd is set */
        if (usbd_msc_cfg.cbw.CB[2] == 0U) {       /* Request for Supported Vital Product Data Pages*/
            data_len = 0x06;
            memcpy(*data, (uint8_t *)inquiry00, data_len);
        } else if (usbd_msc_cfg.cbw.CB[2] == 0x80U) { /* Request for VPD page 0x80 Unit Serial Number */
            data_len = 0x08;
            memcpy(*data, (uint8_t *)inquiry80, data_len);
        } else { /* Request Not supported */
            SCSI_SetSenseData(SCSI_KCQIR_INVALIDFIELDINCBA);
            return false;
        }
    } else {
        if (usbd_msc_cfg.cbw.CB[4] < SCSIRESP_INQUIRY_SIZEOF) {
            data_len = usbd_msc_cfg.cbw.CB[4];
        }
        memcpy(*data, (uint8_t *)inquiry, data_len);
    }

    *len = data_len;
    return true;
}

static bool SCSI_startStopUnit(uint8_t **data, uint32_t *len)
{
    if (usbd_msc_cfg.cbw.dDataLength != 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if ((usbd_msc_cfg.cbw.CB[4] & 0x3U) == 0x1U) /* START=1 */
    {
        //SCSI_MEDIUM_UNLOCKED;
    } else if ((usbd_msc_cfg.cbw.CB[4] & 0x3U) == 0x2U) /* START=0 and LOEJ Load Eject=1 */
    {
        //SCSI_MEDIUM_EJECTED;
    } else if ((usbd_msc_cfg.cbw.CB[4] & 0x3U) == 0x3U) /* START=1 and LOEJ Load Eject=1 */
    {
        //SCSI_MEDIUM_UNLOCKED;
    } else {
    }

    *data = NULL;
    *len = 0;
    return true;
}

static bool SCSI_preventAllowMediaRemoval(uint8_t **data, uint32_t *len)
{
    if (usbd_msc_cfg.cbw.dDataLength != 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    if (usbd_msc_cfg.cbw.CB[4] == 0U) {
        //SCSI_MEDIUM_UNLOCKED;
    } else {
        //SCSI_MEDIUM_LOCKED;
    }
    *data = NULL;
    *len = 0;
    return true;
}

static bool SCSI_modeSense6(uint8_t **data, uint32_t *len)
{
    uint8_t data_len = 4;
    if (usbd_msc_cfg.cbw.dDataLength == 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    if (usbd_msc_cfg.cbw.CB[4] < SCSIRESP_MODEPARAMETERHDR6_SIZEOF) {
        data_len = usbd_msc_cfg.cbw.CB[4];
    }

    uint8_t sense6[SCSIRESP_MODEPARAMETERHDR6_SIZEOF] = { 0x03, 0x00, 0x00, 0x00 };

    if (usbd_msc_cfg.readonly) {
        sense6[2] = 0x80;
    }
    memcpy(*data, (uint8_t *)sense6, data_len);
    *len = data_len;
    return true;
}

static bool SCSI_modeSense10(uint8_t **data, uint32_t *len)
{
    uint8_t data_len = 27;
    if (usbd_msc_cfg.cbw.dDataLength == 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if (usbd_msc_cfg.cbw.CB[8] < 27) {
        data_len = usbd_msc_cfg.cbw.CB[8];
    }

    uint8_t sense10[27] = {
        0x00,
        0x26,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x08,
        0x12,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00
    };

    memcpy(*data, (uint8_t *)sense10, data_len);
    *len = data_len;
    return true;
}

static bool SCSI_readFormatCapacity(uint8_t **data, uint32_t *len)
{
    if (usbd_msc_cfg.cbw.dDataLength == 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }
    uint8_t format_capacity[SCSIRESP_READFORMATCAPACITIES_SIZEOF] = {
        0x00,
        0x00,
        0x00,
        0x08, /* Capacity List Length */
        (uint8_t)((usbd_msc_cfg.scsi_blk_nbr >> 24) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_nbr >> 16) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_nbr >> 8) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_nbr >> 0) & 0xff),

        0x02, /* Descriptor Code: Formatted Media */
        0x00,
        (uint8_t)((usbd_msc_cfg.scsi_blk_size >> 8) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_size >> 0) & 0xff),
    };

    memcpy(*data, (uint8_t *)format_capacity, SCSIRESP_READFORMATCAPACITIES_SIZEOF);
    *len = SCSIRESP_READFORMATCAPACITIES_SIZEOF;
    return true;
}

static bool SCSI_readCapacity10(uint8_t **data, uint32_t *len)
{
    if (usbd_msc_cfg.cbw.dDataLength == 0U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    uint8_t capacity10[SCSIRESP_READCAPACITY10_SIZEOF] = {
        (uint8_t)(((usbd_msc_cfg.scsi_blk_nbr - 1) >> 24) & 0xff),
        (uint8_t)(((usbd_msc_cfg.scsi_blk_nbr - 1) >> 16) & 0xff),
        (uint8_t)(((usbd_msc_cfg.scsi_blk_nbr - 1) >> 8) & 0xff),
        (uint8_t)(((usbd_msc_cfg.scsi_blk_nbr - 1) >> 0) & 0xff),

        (uint8_t)((usbd_msc_cfg.scsi_blk_size >> 24) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_size >> 16) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_size >> 8) & 0xff),
        (uint8_t)((usbd_msc_cfg.scsi_blk_size >> 0) & 0xff),
    };

    memcpy(*data, (uint8_t *)capacity10, SCSIRESP_READCAPACITY10_SIZEOF);
    *len = SCSIRESP_READCAPACITY10_SIZEOF;
    return true;
}

static bool SCSI_read10(uint8_t **data, uint32_t *len)
{
    if (((usbd_msc_cfg.cbw.bmFlags & 0x80U) != 0x80U) || (usbd_msc_cfg.cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    usbd_msc_cfg.start_sector = GET_BE32(&usbd_msc_cfg.cbw.CB[2]); /* Logical Block Address of First Block */
    USB_LOG_DBG("lba: 0x%04x\r\n", usbd_msc_cfg.start_sector);

    usbd_msc_cfg.nsectors = GET_BE16(&usbd_msc_cfg.cbw.CB[7]); /* Number of Blocks to transfer */
    USB_LOG_DBG("nsectors: 0x%02x\r\n", usbd_msc_cfg.nsectors);

    if ((usbd_msc_cfg.start_sector + usbd_msc_cfg.nsectors) > usbd_msc_cfg.scsi_blk_nbr) {
        SCSI_SetSenseData(SCSI_KCQIR_LBAOUTOFRANGE);
        USB_LOG_ERR("LBA out of range\r\n");
        return false;
    }

    if (usbd_msc_cfg.cbw.dDataLength != (usbd_msc_cfg.nsectors * usbd_msc_cfg.scsi_blk_size)) {
        USB_LOG_ERR("scsi_blk_len does not match with dDataLength\r\n");
        return false;
    }
    usbd_msc_cfg.stage = MSC_DATA_IN;
    return SCSI_processRead();
}

static bool SCSI_read12(uint8_t **data, uint32_t *len)
{
    if (((usbd_msc_cfg.cbw.bmFlags & 0x80U) != 0x80U) || (usbd_msc_cfg.cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    usbd_msc_cfg.start_sector = GET_BE32(&usbd_msc_cfg.cbw.CB[2]); /* Logical Block Address of First Block */
    USB_LOG_DBG("lba: 0x%04x\r\n", usbd_msc_cfg.start_sector);

    usbd_msc_cfg.nsectors = GET_BE32(&usbd_msc_cfg.cbw.CB[6]); /* Number of Blocks to transfer */
    USB_LOG_DBG("nsectors: 0x%02x\r\n", usbd_msc_cfg.nsectors);

    if ((usbd_msc_cfg.start_sector + usbd_msc_cfg.nsectors) > usbd_msc_cfg.scsi_blk_nbr) {
        SCSI_SetSenseData(SCSI_KCQIR_LBAOUTOFRANGE);
        USB_LOG_ERR("LBA out of range\r\n");
        return false;
    }

    if (usbd_msc_cfg.cbw.dDataLength != (usbd_msc_cfg.nsectors * usbd_msc_cfg.scsi_blk_size)) {
        USB_LOG_ERR("scsi_blk_len does not match with dDataLength\r\n");
        return false;
    }
    usbd_msc_cfg.stage = MSC_DATA_IN;
    return SCSI_processRead();
}

static bool SCSI_write10(uint8_t **data, uint32_t *len)
{
    uint32_t data_len = 0;
    if (((usbd_msc_cfg.cbw.bmFlags & 0x80U) != 0x00U) || (usbd_msc_cfg.cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    usbd_msc_cfg.start_sector = GET_BE32(&usbd_msc_cfg.cbw.CB[2]); /* Logical Block Address of First Block */
    USB_LOG_DBG("lba: 0x%04x\r\n", usbd_msc_cfg.start_sector);

    usbd_msc_cfg.nsectors = GET_BE16(&usbd_msc_cfg.cbw.CB[7]); /* Number of Blocks to transfer */
    USB_LOG_DBG("nsectors: 0x%02x\r\n", usbd_msc_cfg.nsectors);

    data_len = usbd_msc_cfg.nsectors * usbd_msc_cfg.scsi_blk_size;
    if ((usbd_msc_cfg.start_sector + usbd_msc_cfg.nsectors) > usbd_msc_cfg.scsi_blk_nbr) {
        USB_LOG_ERR("LBA out of range\r\n");
        return false;
    }

    if (usbd_msc_cfg.cbw.dDataLength != data_len) {
        return false;
    }
    usbd_msc_cfg.stage = MSC_DATA_OUT;
    data_len = MIN(data_len, CONFIG_USBDEV_MSC_BLOCK_SIZE);
    usbd_ep_start_read(mass_ep_data[MSD_OUT_EP_IDX].ep_addr, usbd_msc_cfg.block_buffer, data_len);
    return true;
}

static bool SCSI_write12(uint8_t **data, uint32_t *len)
{
    uint32_t data_len = 0;
    if (((usbd_msc_cfg.cbw.bmFlags & 0x80U) != 0x00U) || (usbd_msc_cfg.cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    usbd_msc_cfg.start_sector = GET_BE32(&usbd_msc_cfg.cbw.CB[2]); /* Logical Block Address of First Block */
    USB_LOG_DBG("lba: 0x%04x\r\n", usbd_msc_cfg.start_sector);

    usbd_msc_cfg.nsectors = GET_BE32(&usbd_msc_cfg.cbw.CB[6]); /* Number of Blocks to transfer */
    USB_LOG_DBG("nsectors: 0x%02x\r\n", usbd_msc_cfg.nsectors);

    data_len = usbd_msc_cfg.nsectors * usbd_msc_cfg.scsi_blk_size;
    if ((usbd_msc_cfg.start_sector + usbd_msc_cfg.nsectors) > usbd_msc_cfg.scsi_blk_nbr) {
        USB_LOG_ERR("LBA out of range\r\n");
        return false;
    }

    if (usbd_msc_cfg.cbw.dDataLength != data_len) {
        return false;
    }
    usbd_msc_cfg.stage = MSC_DATA_OUT;
    data_len = MIN(data_len, CONFIG_USBDEV_MSC_BLOCK_SIZE);
    usbd_ep_start_read(mass_ep_data[MSD_OUT_EP_IDX].ep_addr, usbd_msc_cfg.block_buffer, data_len);
    return true;
}
/* do not use verify to reduce code size */
#if 0
static bool SCSI_verify10(uint8_t **data, uint32_t *len)
{
    /* Logical Block Address of First Block */
    uint32_t lba = 0;
    uint32_t blk_num = 0;

    if ((usbd_msc_cfg.cbw.CB[1] & 0x02U) == 0x00U) {
        return true;
    }

    if (((usbd_msc_cfg.cbw.bmFlags & 0x80U) != 0x00U) || (usbd_msc_cfg.cbw.dDataLength == 0U)) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    if ((usbd_msc_cfg.cbw.CB[1] & 0x02U) == 0x02U) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDFIELDINCBA);
        return false; /* Error, Verify Mode Not supported*/
    }

    lba = GET_BE32(&usbd_msc_cfg.cbw.CB[2]);
    USB_LOG_DBG("lba: 0x%x\r\n", lba);

    usbd_msc_cfg.scsi_blk_addr = lba * usbd_msc_cfg.scsi_blk_size;

    /* Number of Blocks to transfer */
    blk_num = GET_BE16(&usbd_msc_cfg.cbw.CB[7]);

    USB_LOG_DBG("num (block) : 0x%x\r\n", blk_num);
    usbd_msc_cfg.scsi_blk_len = blk_num * usbd_msc_cfg.scsi_blk_size;

    if ((lba + blk_num) > usbd_msc_cfg.scsi_blk_nbr) {
        USB_LOG_ERR("LBA out of range\r\n");
        return false;
    }

    if (usbd_msc_cfg.cbw.dDataLength != usbd_msc_cfg.scsi_blk_len) {
        return false;
    }

    usbd_msc_cfg.stage = MSC_DATA_OUT;
    return true;
}
#endif

static bool SCSI_processRead(void)
{
    uint32_t transfer_len;

    USB_LOG_DBG("read lba:%d\r\n", usbd_msc_cfg.start_sector);

    transfer_len = MIN(usbd_msc_cfg.nsectors * usbd_msc_cfg.scsi_blk_size, CONFIG_USBDEV_MSC_BLOCK_SIZE);

    if (usbd_msc_sector_read(usbd_msc_cfg.start_sector, usbd_msc_cfg.block_buffer, transfer_len) != 0) {
        SCSI_SetSenseData(SCSI_KCQHE_UREINRESERVEDAREA);
        return false;
    }

    usbd_ep_start_write(mass_ep_data[MSD_IN_EP_IDX].ep_addr, usbd_msc_cfg.block_buffer, transfer_len);

    usbd_msc_cfg.start_sector += (transfer_len / usbd_msc_cfg.scsi_blk_size);
    usbd_msc_cfg.nsectors -= (transfer_len / usbd_msc_cfg.scsi_blk_size);
    usbd_msc_cfg.csw.dDataResidue -= transfer_len;

    if (usbd_msc_cfg.nsectors == 0) {
        usbd_msc_cfg.stage = MSC_SEND_CSW;
    }

    return true;
}

static bool SCSI_processWrite(uint32_t nbytes)
{
    uint32_t data_len = 0;
    USB_LOG_DBG("write lba:%d\r\n", usbd_msc_cfg.start_sector);

    if (usbd_msc_sector_write(usbd_msc_cfg.start_sector, usbd_msc_cfg.block_buffer, nbytes) != 0) {
        SCSI_SetSenseData(SCSI_KCQHE_WRITEFAULT);
        return false;
    }

    usbd_msc_cfg.start_sector += (nbytes / usbd_msc_cfg.scsi_blk_size);
    usbd_msc_cfg.nsectors -= (nbytes / usbd_msc_cfg.scsi_blk_size);
    usbd_msc_cfg.csw.dDataResidue -= nbytes;

    if (usbd_msc_cfg.nsectors == 0) {
        usbd_msc_send_csw(CSW_STATUS_CMD_PASSED);
    } else {
        data_len = MIN(usbd_msc_cfg.nsectors * usbd_msc_cfg.scsi_blk_size, CONFIG_USBDEV_MSC_BLOCK_SIZE);
        usbd_ep_start_read(mass_ep_data[MSD_OUT_EP_IDX].ep_addr, usbd_msc_cfg.block_buffer, data_len);
    }

    return true;
}

static bool SCSI_CBWDecode(uint32_t nbytes)
{
    uint8_t *buf2send = usbd_msc_cfg.block_buffer;
    uint32_t len2send = 0;
    bool ret = false;

    if (nbytes != sizeof(struct CBW)) {
        USB_LOG_ERR("size != sizeof(cbw)\r\n");
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    }

    usbd_msc_cfg.csw.dTag = usbd_msc_cfg.cbw.dTag;
    usbd_msc_cfg.csw.dDataResidue = usbd_msc_cfg.cbw.dDataLength;

    if ((usbd_msc_cfg.cbw.bLUN > 1) || (usbd_msc_cfg.cbw.dSignature != MSC_CBW_Signature) || (usbd_msc_cfg.cbw.bCBLength < 1) || (usbd_msc_cfg.cbw.bCBLength > 16)) {
        SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
        return false;
    } else {
        USB_LOG_DBG("Decode CB:0x%02x\r\n", usbd_msc_cfg.cbw.CB[0]);
        switch (usbd_msc_cfg.cbw.CB[0]) {
            case SCSI_CMD_TESTUNITREADY:
                ret = SCSI_testUnitReady(&buf2send, &len2send);
                break;
            case SCSI_CMD_REQUESTSENSE:
                ret = SCSI_requestSense(&buf2send, &len2send);
                break;
            case SCSI_CMD_INQUIRY:
                ret = SCSI_inquiry(&buf2send, &len2send);
                break;
            case SCSI_CMD_STARTSTOPUNIT:
                ret = SCSI_startStopUnit(&buf2send, &len2send);
                break;
            case SCSI_CMD_PREVENTMEDIAREMOVAL:
                ret = SCSI_preventAllowMediaRemoval(&buf2send, &len2send);
                break;
            case SCSI_CMD_MODESENSE6:
                ret = SCSI_modeSense6(&buf2send, &len2send);
                break;
            case SCSI_CMD_MODESENSE10:
                ret = SCSI_modeSense10(&buf2send, &len2send);
                break;
            case SCSI_CMD_READFORMATCAPACITIES:
                ret = SCSI_readFormatCapacity(&buf2send, &len2send);
                break;
            case SCSI_CMD_READCAPACITY10:
                ret = SCSI_readCapacity10(&buf2send, &len2send);
                break;
            case SCSI_CMD_READ10:
                ret = SCSI_read10(NULL, 0);
                break;
            case SCSI_CMD_READ12:
                ret = SCSI_read12(NULL, 0);
                break;
            case SCSI_CMD_WRITE10:
                ret = SCSI_write10(NULL, 0);
                break;
            case SCSI_CMD_WRITE12:
                ret = SCSI_write12(NULL, 0);
                break;
            case SCSI_CMD_VERIFY10:
                //ret = SCSI_verify10(NULL, 0);
                ret = false;
                break;

            default:
                SCSI_SetSenseData(SCSI_KCQIR_INVALIDCOMMAND);
                USB_LOG_WRN("unsupported cmd:0x%02x\r\n", usbd_msc_cfg.cbw.CB[0]);
                ret = false;
                break;
        }
    }
    if (ret) {
        if (usbd_msc_cfg.stage == MSC_READ_CBW) {
            if (len2send) {
                USB_LOG_DBG("Send info len:%d\r\n", len2send);
                usbd_msc_send_info(buf2send, len2send);
            } else {
                usbd_msc_send_csw(CSW_STATUS_CMD_PASSED);
            }
        }
    }
    return ret;
}

void mass_storage_bulk_out(uint8_t ep, uint32_t nbytes)
{
    switch (usbd_msc_cfg.stage) {
        case MSC_READ_CBW:
            if (SCSI_CBWDecode(nbytes) == false) {
                USB_LOG_ERR("Command:0x%02x decode err\r\n", usbd_msc_cfg.cbw.CB[0]);
                usbd_msc_bot_abort();
                return;
            }
            break;
        case MSC_DATA_OUT:
            switch (usbd_msc_cfg.cbw.CB[0]) {
                case SCSI_CMD_WRITE10:
                case SCSI_CMD_WRITE12:
                    if (SCSI_processWrite(nbytes) == false) {
                        usbd_msc_send_csw(CSW_STATUS_CMD_FAILED); /* send fail status to host,and the host will retry*/
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

void mass_storage_bulk_in(uint8_t ep, uint32_t nbytes)
{
    switch (usbd_msc_cfg.stage) {
        case MSC_DATA_IN:
            switch (usbd_msc_cfg.cbw.CB[0]) {
                case SCSI_CMD_READ10:
                case SCSI_CMD_READ12:
                    if (SCSI_processRead() == false) {
                        usbd_msc_send_csw(CSW_STATUS_CMD_FAILED); /* send fail status to host,and the host will retry*/
                        return;
                    }
                    break;
                default:
                    break;
            }
            break;
        /*the device has to send a CSW*/
        case MSC_SEND_CSW:
            usbd_msc_send_csw(CSW_STATUS_CMD_PASSED);
            break;

        /*the host has received the CSW*/
        case MSC_WAIT_CSW:
            usbd_msc_cfg.stage = MSC_READ_CBW;
            USB_LOG_DBG("Start reading cbw\r\n");
            usbd_ep_start_read(mass_ep_data[MSD_OUT_EP_IDX].ep_addr, (uint8_t *)&usbd_msc_cfg.cbw, USB_SIZEOF_MSC_CBW);
            break;

        default:
            break;
    }
}

struct usbd_interface *usbd_msc_init_intf(struct usbd_interface *intf, const uint8_t out_ep, const uint8_t in_ep)
{
    intf->class_interface_handler = msc_storage_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = msc_storage_notify_handler;

    mass_ep_data[MSD_OUT_EP_IDX].ep_addr = out_ep;
    mass_ep_data[MSD_OUT_EP_IDX].ep_cb = mass_storage_bulk_out;
    mass_ep_data[MSD_IN_EP_IDX].ep_addr = in_ep;
    mass_ep_data[MSD_IN_EP_IDX].ep_cb = mass_storage_bulk_in;

    usbd_add_endpoint(&mass_ep_data[MSD_OUT_EP_IDX]);
    usbd_add_endpoint(&mass_ep_data[MSD_IN_EP_IDX]);

    memset((uint8_t *)&usbd_msc_cfg, 0, sizeof(struct usbd_msc_cfg_priv));

    usbd_msc_get_cap(0, &usbd_msc_cfg.scsi_blk_nbr, &usbd_msc_cfg.scsi_blk_size);

    if (usbd_msc_cfg.scsi_blk_size > CONFIG_USBDEV_MSC_BLOCK_SIZE) {
        USB_LOG_ERR("msc block buffer overflow\r\n");
        return NULL;
    }

    return intf;
}

void usbd_msc_set_readonly(bool readonly)
{
    usbd_msc_cfg.readonly = readonly;
}