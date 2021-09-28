/**
 * @file usbd_msc.c
 * @brief
 *
 * Copyright (c) 2021 Bouffalolab team
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 */
#include "usbd_core.h"
#include "usbd_scsi.h"
#include "usbd_msc.h"

/* max USB packet size */
#ifndef CONFIG_USB_HS
#define MASS_STORAGE_BULK_EP_MPS 64
#else
#define MASS_STORAGE_BULK_EP_MPS 512
#endif

#define MSD_OUT_EP_IDX 0
#define MSD_IN_EP_IDX  1

/* Describe EndPoints configuration */
static usbd_endpoint_t mass_ep_data[2];

/* MSC Bulk-only Stage */
enum Stage {
    /* MSC Bulk-only Stage */
    MSC_BS_CBW = 0,                /* Command Block Wrapper */
    MSC_BS_DATA_OUT = 1,           /* Data Out Phase */
    MSC_BS_DATA_IN = 2,            /* Data In Phase */
    MSC_BS_DATA_IN_LAST = 3,       /* Data In Last Phase */
    MSC_BS_DATA_IN_LAST_STALL = 4, /* Data In Last Phase with Stall */
    MSC_BS_CSW = 5,                /* Command Status Wrapper */
    MSC_BS_ERROR = 6,              /* Error */
    MSC_BS_RESET = 7,              /* Bulk-Only Mass Storage Reset */
};

/* Device data structure */
struct usbd_msc_cfg_private {
    /* state of the bulk-only state machine */
    enum Stage stage;
    struct CBW cbw;
    struct CSW csw;

    uint8_t max_lun_count;
    uint16_t scsi_blk_size;
    uint32_t scsi_blk_nbr;

    uint32_t scsi_blk_addr;
    uint32_t scsi_blk_len;
    uint8_t *block_buffer;

} usbd_msc_cfg;

/*memory OK (after a usbd_msc_memory_verify)*/
static bool memOK;

static void usbd_msc_reset(void)
{
    usbd_msc_cfg.stage = MSC_BS_CBW;
    (void)memset((void *)&usbd_msc_cfg.cbw, 0, sizeof(struct CBW));
    (void)memset((void *)&usbd_msc_cfg.csw, 0, sizeof(struct CSW));
    usbd_msc_cfg.scsi_blk_addr = 0U;
    usbd_msc_cfg.scsi_blk_len = 0U;
    usbd_msc_get_cap(0, &usbd_msc_cfg.scsi_blk_nbr, &usbd_msc_cfg.scsi_blk_size);
    usbd_msc_cfg.max_lun_count = 0;

    if (usbd_msc_cfg.block_buffer) {
        free(usbd_msc_cfg.block_buffer);
    }
    usbd_msc_cfg.block_buffer = malloc(usbd_msc_cfg.scsi_blk_size * sizeof(uint8_t));
}

/**
 * @brief Handler called for Class requests not handled by the USB stack.
 *
 * @param pSetup    Information about the request to execute.
 * @param len       Size of the buffer.
 * @param data      Buffer containing the request result.
 *
 * @return  0 on success, negative errno code on fail.
 */
static int msc_storage_class_request_handler(struct usb_setup_packet *pSetup, uint8_t **data, uint32_t *len)
{
    switch (pSetup->bRequest) {
        case MSC_REQUEST_RESET:
            USBD_LOG_DBG("MSC_REQUEST_RESET");

            if (pSetup->wLength) {
                USBD_LOG_WRN("Invalid length");
                return -1;
            }

            usbd_msc_reset();
            break;

        case MSC_REQUEST_GET_MAX_LUN:
            USBD_LOG_DBG("MSC_REQUEST_GET_MAX_LUN");

            if (pSetup->wLength != 1) {
                USBD_LOG_WRN("Invalid length");
                return -1;
            }

            *data = (uint8_t *)(&usbd_msc_cfg.max_lun_count);
            *len = 1;
            break;

        default:
            USBD_LOG_WRN("Unknown request 0x%02x, value 0x%02x",
                         pSetup->bRequest, pSetup->wValue);
            return -1;
    }

    return 0;
}

static void usbd_msc_send_csw(void)
{
    usbd_msc_cfg.csw.Signature = MSC_CSW_Signature;

    if (usbd_ep_write(mass_ep_data[MSD_IN_EP_IDX].ep_addr, (uint8_t *)&usbd_msc_cfg.csw,
                      sizeof(struct CSW), NULL) != 0) {
        USBD_LOG_ERR("usb write failure");
    }

    usbd_msc_cfg.stage = MSC_BS_CSW;
}

static bool usbd_msc_datain_check(void)
{
    if (!usbd_msc_cfg.cbw.DataLength) {
        USBD_LOG_WRN("Zero length in CBW");
        usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
        usbd_msc_send_csw();
        return false;
    }

    if ((usbd_msc_cfg.cbw.Flags & 0x80) == 0) {
        usbd_ep_set_stall(mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
        usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
        usbd_msc_send_csw();
        return false;
    }

    return true;
}

static bool usbd_msc_send_to_host(uint8_t *buffer, uint16_t size)
{
    if (size >= usbd_msc_cfg.cbw.DataLength) {
        size = usbd_msc_cfg.cbw.DataLength;
        usbd_msc_cfg.stage = MSC_BS_DATA_IN_LAST;
    } else {
        usbd_msc_cfg.stage = MSC_BS_DATA_IN_LAST_STALL;
    }

    if (usbd_ep_write(mass_ep_data[MSD_IN_EP_IDX].ep_addr, buffer, size, NULL)) {
        USBD_LOG_ERR("USB write failed\r\n");
        return false;
    }

    usbd_msc_cfg.csw.DataResidue -= size;
    usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;

    return true;
}

static void usbd_msc_memory_verify(uint8_t *buf, uint16_t size)
{
    uint32_t n;

    if ((usbd_msc_cfg.scsi_blk_addr + size) > (usbd_msc_cfg.scsi_blk_nbr * usbd_msc_cfg.scsi_blk_size)) {
        size = usbd_msc_cfg.scsi_blk_nbr * usbd_msc_cfg.scsi_blk_size - usbd_msc_cfg.scsi_blk_addr;
        usbd_msc_cfg.stage = MSC_BS_ERROR;
        usbd_ep_set_stall(mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
        USBD_LOG_WRN("addr overflow,verify error\r\n");
    }

    /* beginning of a new block -> load a whole block in RAM */
    if (!(usbd_msc_cfg.scsi_blk_addr % usbd_msc_cfg.scsi_blk_size)) {
        USBD_LOG_DBG("Disk READ sector %d", usbd_msc_cfg.scsi_blk_addr / usbd_msc_cfg.scsi_blk_size);
        // if (disk_access_read(disk_pdrv, page, addr / BLOCK_SIZE, 1))
        // {
        //  USBD_LOG_ERR("---- Disk Read Error %d", addr / BLOCK_SIZE);
        // }
    }

    /* info are in RAM -> no need to re-read memory */
    for (n = 0U; n < size; n++) {
        if (usbd_msc_cfg.block_buffer[usbd_msc_cfg.scsi_blk_addr % usbd_msc_cfg.scsi_blk_size + n] != buf[n]) {
            USBD_LOG_DBG("Mismatch sector %d offset %d",
                         usbd_msc_cfg.scsi_blk_addr / usbd_msc_cfg.scsi_blk_size, n);
            memOK = false;
            break;
        }
    }

    usbd_msc_cfg.scsi_blk_addr += size;
    usbd_msc_cfg.scsi_blk_len -= size;
    usbd_msc_cfg.csw.DataResidue -= size;

    if (!usbd_msc_cfg.scsi_blk_len || (usbd_msc_cfg.stage == MSC_BS_CSW)) {
        usbd_msc_cfg.csw.Status = (memOK) ? CSW_STATUS_CMD_PASSED : CSW_STATUS_CMD_FAILED;
        usbd_msc_send_csw();
    }
}

static void usbd_msc_memory_write(uint8_t *buf, uint16_t size)
{
    USBD_LOG_DBG("w:%d\r\n", usbd_msc_cfg.scsi_blk_addr);

    if ((usbd_msc_cfg.scsi_blk_addr + size) > (usbd_msc_cfg.scsi_blk_nbr * usbd_msc_cfg.scsi_blk_size)) {
        size = usbd_msc_cfg.scsi_blk_nbr * usbd_msc_cfg.scsi_blk_size - usbd_msc_cfg.scsi_blk_addr;
        usbd_msc_cfg.stage = MSC_BS_ERROR;
        usbd_ep_set_stall(mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
        USBD_LOG_WRN("addr overflow,write error\r\n");
    }

    /* we fill an array in RAM of 1 block before writing it in memory */
    for (int i = 0; i < size; i++) {
        usbd_msc_cfg.block_buffer[usbd_msc_cfg.scsi_blk_addr % usbd_msc_cfg.scsi_blk_size + i] = buf[i];
    }

    /* if the array is filled, write it in memory */
    if ((usbd_msc_cfg.scsi_blk_addr % usbd_msc_cfg.scsi_blk_size) + size >= usbd_msc_cfg.scsi_blk_size) {
        usbd_msc_sector_write((usbd_msc_cfg.scsi_blk_addr / usbd_msc_cfg.scsi_blk_size), usbd_msc_cfg.block_buffer, usbd_msc_cfg.scsi_blk_size);
    }

    usbd_msc_cfg.scsi_blk_addr += size;
    usbd_msc_cfg.scsi_blk_len -= size;
    usbd_msc_cfg.csw.DataResidue -= size;

    if ((!usbd_msc_cfg.scsi_blk_len) || (usbd_msc_cfg.stage == MSC_BS_CSW)) {
        usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;
        usbd_msc_send_csw();
    }
}

static void usbd_msc_memory_read(void)
{
    uint32_t transfer_len;

    transfer_len = MIN(usbd_msc_cfg.scsi_blk_len, MASS_STORAGE_BULK_EP_MPS);

    /* we read an entire block */
    if (!(usbd_msc_cfg.scsi_blk_addr % usbd_msc_cfg.scsi_blk_size)) {
        usbd_msc_sector_read((usbd_msc_cfg.scsi_blk_addr / usbd_msc_cfg.scsi_blk_size), usbd_msc_cfg.block_buffer, usbd_msc_cfg.scsi_blk_size);
    }

    USBD_LOG_DBG("addr:%d\r\n", usbd_msc_cfg.scsi_blk_addr);

    usbd_ep_write(mass_ep_data[MSD_IN_EP_IDX].ep_addr,
                  &usbd_msc_cfg.block_buffer[usbd_msc_cfg.scsi_blk_addr % usbd_msc_cfg.scsi_blk_size], transfer_len, NULL);

    usbd_msc_cfg.scsi_blk_addr += transfer_len;
    usbd_msc_cfg.scsi_blk_len -= transfer_len;
    usbd_msc_cfg.csw.DataResidue -= transfer_len;

    if (!usbd_msc_cfg.scsi_blk_len) {
        usbd_msc_cfg.stage = MSC_BS_DATA_IN_LAST;
        usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;
    }
}

/*********************************SCSI CMD*******************************************************************/
static bool scsi_test_unit_ready(void)
{
    if (usbd_msc_cfg.cbw.DataLength != 0U) {
        if ((usbd_msc_cfg.cbw.Flags & 0x80) != 0U) {
            USBD_LOG_WRN("Stall IN endpoint\r\n");
            usbd_ep_set_stall(mass_ep_data[MSD_IN_EP_IDX].ep_addr);
        } else {
            USBD_LOG_WRN("Stall OUT endpoint\r\n");
            usbd_ep_set_stall(mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
        }

        return false;
    }

    usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;
    usbd_msc_send_csw();
    return true;
}

static bool scsi_request_sense(void)
{
    if (!usbd_msc_datain_check()) {
        return false;
    }

    scsi_sense_fixed_resp_t sense_rsp = {
        .response_code = 0x70,
        .valid = 1
    };

    sense_rsp.add_sense_len = sizeof(scsi_sense_fixed_resp_t) - 8;
    sense_rsp.sense_key = 0x00;
    sense_rsp.add_sense_code = 0x00;
    sense_rsp.add_sense_qualifier = 0x00;

    /* Win host requests maximum number of bytes but as all we have is 4 bytes we have
       to tell host back that it is all we have, that's why we correct residue */
    if (usbd_msc_cfg.csw.DataResidue > sizeof(sense_rsp)) {
        usbd_msc_cfg.cbw.DataLength = sizeof(sense_rsp);
        usbd_msc_cfg.csw.DataResidue = sizeof(sense_rsp);
    }

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

    return usbd_msc_send_to_host((uint8_t *)&sense_rsp, sizeof(sense_rsp));
}

static bool scsi_inquiry_request(void)
{
    if (!usbd_msc_datain_check()) {
        return false;
    }

    scsi_inquiry_resp_t inquiry_rsp = {
        .is_removable = 1, /* RMB = 1: Removable Medium */
        .version = 2,
        .response_data_format = 2,
        .additional_length = 31
    };
    // vendor_id, product_id, product_rev is space padded string
    memcpy(inquiry_rsp.vendor_id, "BL702USB", sizeof(inquiry_rsp.vendor_id));
    memcpy(inquiry_rsp.product_id, "FAT16 RAM DEMO  ", sizeof(inquiry_rsp.product_id));
    memcpy(inquiry_rsp.product_rev, "1.0 ", sizeof(inquiry_rsp.product_rev));

    /* Win host requests maximum number of bytes but as all we have is 4 bytes we have
       to tell host back that it is all we have, that's why we correct residue */
    if (usbd_msc_cfg.csw.DataResidue > sizeof(inquiry_rsp)) {
        usbd_msc_cfg.cbw.DataLength = sizeof(inquiry_rsp);
        usbd_msc_cfg.csw.DataResidue = sizeof(inquiry_rsp);
    }

    return usbd_msc_send_to_host((uint8_t *)&inquiry_rsp, sizeof(inquiry_rsp));
}

static bool scsi_mode_sense_6(void)
{
    if (!usbd_msc_datain_check()) {
        return false;
    }

    scsi_mode_sense6_resp_t mode_resp = {
        .data_len = 3,
        .medium_type = 0,
        .write_protected = false,
        .reserved = 0,
        .block_descriptor_len = 0 // no block descriptor are included
    };

    /* Win host requests maximum number of bytes but as all we have is 4 bytes we have
       to tell host back that it is all we have, that's why we correct residue */
    if (usbd_msc_cfg.csw.DataResidue > sizeof(mode_resp)) {
        usbd_msc_cfg.cbw.DataLength = sizeof(mode_resp);
        usbd_msc_cfg.csw.DataResidue = sizeof(mode_resp);
    }

    return usbd_msc_send_to_host((uint8_t *)&mode_resp, sizeof(mode_resp));
}

static bool scsi_start_stop_unit(void)
{
    // if (!cbw.CB[3]) {               /* If power condition modifier is 0 */
    //     USBD_MSC_MediaReady  = cbw.CB[4] & 0x01;   /* Media ready = START bit value */
    //     usbd_msc_start_stop(USBD_MSC_MediaReady);
    //     cbw.bStatus = CSW_CMD_PASSED; /* Start Stop Unit -> pass */
    //     USBD_MSC_SetCSW();
    //     return;
    // }

    // cbw.bStatus = CSW_CMD_FAILED;   /* Start Stop Unit -> fail */
    // usbd_msc_send_csw();
    usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;
    usbd_msc_send_csw();
    return true;
}
/*
 *  USB Device MSC SCSI Media Removal Callback
 *    Parameters:      None
 *    Return Value:    None
 */

static bool scsi_media_removal(void)
{
    // if (USBD_MSC_CBW.CB[4] & 1) {            /* If prevent */
    //     USBD_MSC_CSW.bStatus = CSW_CMD_FAILED;    /* Prevent media removal -> fail */
    // } else {                                 /* If allow */
    //     USBD_MSC_CSW.bStatus = CSW_CMD_PASSED;    /* Allow media removal -> pass */
    // }

    // USBD_MSC_SetCSW();
    usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;
    usbd_msc_send_csw();
    return true;
}

static bool scsi_read_format_capacity(void)
{
    if (!usbd_msc_datain_check()) {
        return false;
    }

    scsi_read_format_capacity_resp_t read_fmt_capa = {
        .list_length = 8, /* Capacity List Length */
        .block_num = 0,
        .descriptor_type = 2, /* Descriptor Code: Formatted Media */
        .block_size_u16 = 0
    };
    /* Block Count */
    read_fmt_capa.block_num = BSWAP32(usbd_msc_cfg.scsi_blk_nbr);
    /* Block Length */
    read_fmt_capa.block_size_u16 = BSWAP16(usbd_msc_cfg.scsi_blk_size);

    /* Win host requests maximum number of bytes but as all we have is 4 bytes we have
       to tell host back that it is all we have, that's why we correct residue */
    if (usbd_msc_cfg.csw.DataResidue > sizeof(read_fmt_capa)) {
        usbd_msc_cfg.cbw.DataLength = sizeof(read_fmt_capa);
        usbd_msc_cfg.csw.DataResidue = sizeof(read_fmt_capa);
    }

    return usbd_msc_send_to_host((uint8_t *)&read_fmt_capa, sizeof(read_fmt_capa));
}

static bool scsi_read_capacity(void)
{
    if (!usbd_msc_datain_check()) {
        return false;
    }

    scsi_read_capacity10_resp_t read_capa10;
    /* Last Logical Block */
    read_capa10.last_lba = BSWAP32((usbd_msc_cfg.scsi_blk_nbr - 1));
    /* Block Length */
    read_capa10.block_size = BSWAP32(usbd_msc_cfg.scsi_blk_size);

    /* Win host requests maximum number of bytes but as all we have is 4 bytes we have
       to tell host back that it is all we have, that's why we correct residue */
    if (usbd_msc_cfg.csw.DataResidue > sizeof(read_capa10)) {
        usbd_msc_cfg.cbw.DataLength = sizeof(read_capa10);
        usbd_msc_cfg.csw.DataResidue = sizeof(read_capa10);
    }

    return usbd_msc_send_to_host((uint8_t *)&read_capa10, sizeof(read_capa10));
}

static bool usbd_msc_read_write_process(void)
{
    /* Logical Block Address of First Block */
    uint32_t lba;
    uint32_t blk_num = 0;

    if (!usbd_msc_cfg.cbw.DataLength) {
        USBD_LOG_WRN("Zero length in CBW\r\n");
        usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
        usbd_msc_send_csw();
        return false;
    }

    lba = GET_BE32(&usbd_msc_cfg.cbw.CB[2]);

    USBD_LOG_DBG("LBA (block) : 0x%x\r\n", lba);
    usbd_msc_cfg.scsi_blk_addr = lba * usbd_msc_cfg.scsi_blk_size;

    /* Number of Blocks to transfer */
    switch (usbd_msc_cfg.cbw.CB[0]) {
        case SCSI_READ10:
        case SCSI_WRITE10:
        case SCSI_VERIFY10:
            blk_num = GET_BE16(&usbd_msc_cfg.cbw.CB[7]);
            break;

        case SCSI_READ12:
        case SCSI_WRITE12:
            blk_num = GET_BE32(&usbd_msc_cfg.cbw.CB[6]);
            break;

        default:
            break;
    }

    USBD_LOG_DBG("num (block) : 0x%x\r\n", blk_num);
    usbd_msc_cfg.scsi_blk_len = blk_num * usbd_msc_cfg.scsi_blk_size;

    if ((lba + blk_num) > usbd_msc_cfg.scsi_blk_nbr) {
        USBD_LOG_ERR("LBA out of range\r\n");
        usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_FAILED;
        usbd_msc_send_csw();
        return false;
    }

    if (usbd_msc_cfg.cbw.DataLength != usbd_msc_cfg.scsi_blk_len) {
        if ((usbd_msc_cfg.cbw.Flags & 0x80) != 0U) {
            USBD_LOG_WRN("read write process error\r\n");
            usbd_ep_set_stall(mass_ep_data[MSD_IN_EP_IDX].ep_addr);
        } else {
            USBD_LOG_WRN("read write process error\r\n");
            usbd_ep_set_stall(mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
        }

        usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_FAILED;
        usbd_msc_send_csw();
        return false;
    }

    return true;
}

static bool scsi_mode_sense_10(void)
{
    if (!usbd_msc_datain_check()) {
        return false;
    }

    scsi_mode_10_resp_t mode10_resp = {
        .mode_data_length_low = 0x06,
        .write_protect = 1,
    };

    /* Win host requests maximum number of bytes but as all we have is 4 bytes we have
       to tell host back that it is all we have, that's why we correct residue */
    if (usbd_msc_cfg.csw.DataResidue > sizeof(mode10_resp)) {
        usbd_msc_cfg.cbw.DataLength = sizeof(mode10_resp);
        usbd_msc_cfg.csw.DataResidue = sizeof(mode10_resp);
    }

    return usbd_msc_send_to_host((uint8_t *)&mode10_resp, sizeof(mode10_resp));
}

static void usbd_msc_cbw_decode(uint8_t *buf, uint16_t size)
{
    if (size != sizeof(usbd_msc_cfg.cbw)) {
        USBD_LOG_ERR("size != sizeof(cbw)");
        return;
    }

    memcpy((uint8_t *)&usbd_msc_cfg.cbw, buf, size);

    if (usbd_msc_cfg.cbw.Signature != MSC_CBW_Signature) {
        USBD_LOG_ERR("CBW Signature Mismatch");
        return;
    }

    usbd_msc_cfg.csw.Tag = usbd_msc_cfg.cbw.Tag;
    usbd_msc_cfg.csw.DataResidue = usbd_msc_cfg.cbw.DataLength;

    if ((usbd_msc_cfg.cbw.CBLength < 1) || (usbd_msc_cfg.cbw.CBLength > 16) || (usbd_msc_cfg.cbw.LUN != 0U)) {
        USBD_LOG_WRN("cbw.CBLength %d", usbd_msc_cfg.cbw.CBLength);
        /* Stall data stage */
        usbd_ep_set_stall(mass_ep_data[MSD_IN_EP_IDX].ep_addr);
        usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_FAILED;
        usbd_msc_send_csw();
    } else {
        switch (usbd_msc_cfg.cbw.CB[0]) {
            case SCSI_TEST_UNIT_READY:
                USBD_LOG_DBG(">> TUR");
                scsi_test_unit_ready();
                break;

            case SCSI_REQUEST_SENSE:
                USBD_LOG_DBG(">> REQ_SENSE");
                scsi_request_sense();
                break;

            case SCSI_INQUIRY:
                USBD_LOG_DBG(">> INQ");
                scsi_inquiry_request();
                break;

            case SCSI_START_STOP_UNIT:
                scsi_start_stop_unit();
                break;

            case SCSI_MEDIA_REMOVAL:
                scsi_media_removal();
                break;

            case SCSI_MODE_SENSE6:
                USBD_LOG_DBG(">> MODE_SENSE6");
                scsi_mode_sense_6();
                break;

            case SCSI_MODE_SENSE10:
                USBD_LOG_DBG(">> MODE_SENSE10");
                scsi_mode_sense_10();
                break;

            case SCSI_READ_FORMAT_CAPACITIES:
                USBD_LOG_DBG(">> READ_FORMAT_CAPACITY");
                scsi_read_format_capacity();
                break;

            case SCSI_READ_CAPACITY:
                USBD_LOG_DBG(">> READ_CAPACITY");
                scsi_read_capacity();
                break;

            case SCSI_READ10:
            case SCSI_READ12:
                USBD_LOG_DBG(">> READ");

                if (usbd_msc_read_write_process()) {
                    if ((usbd_msc_cfg.cbw.Flags & 0x80)) {
                        usbd_msc_cfg.stage = MSC_BS_DATA_IN;
                        usbd_msc_memory_read();
                    } else {
                        usbd_ep_set_stall(
                            mass_ep_data[MSD_OUT_EP_IDX].ep_addr);
                        USBD_LOG_WRN("Stall OUT endpoint");
                        usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
                        usbd_msc_send_csw();
                    }
                }

                break;

            case SCSI_WRITE10:
            case SCSI_WRITE12:
                USBD_LOG_DBG(">> WRITE");

                if (usbd_msc_read_write_process()) {
                    if (!(usbd_msc_cfg.cbw.Flags & 0x80)) {
                        usbd_msc_cfg.stage = MSC_BS_DATA_OUT;
                    } else {
                        usbd_ep_set_stall(
                            mass_ep_data[MSD_IN_EP_IDX].ep_addr);
                        USBD_LOG_WRN("Stall IN endpoint");
                        usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
                        usbd_msc_send_csw();
                    }
                }

                break;

            case SCSI_VERIFY10:
                USBD_LOG_DBG(">> VERIFY10");

                if (!(usbd_msc_cfg.cbw.CB[1] & 0x02)) {
                    usbd_msc_cfg.csw.Status = CSW_STATUS_CMD_PASSED;
                    usbd_msc_send_csw();
                    break;
                }

                if (usbd_msc_read_write_process()) {
                    if (!(usbd_msc_cfg.cbw.Flags & 0x80)) {
                        usbd_msc_cfg.stage = MSC_BS_DATA_OUT;
                        memOK = true;
                    } else {
                        usbd_ep_set_stall(
                            mass_ep_data[MSD_IN_EP_IDX].ep_addr);
                        USBD_LOG_WRN("Stall IN endpoint");
                        usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
                        usbd_msc_send_csw();
                    }
                }

                break;

            default:
                USBD_LOG_WRN(">> default CB[0] %x", usbd_msc_cfg.cbw.CB[0]);
                /* Stall data stage */
                usbd_ep_set_stall(mass_ep_data[MSD_IN_EP_IDX].ep_addr);
                usbd_msc_cfg.stage = MSC_BS_ERROR;
                break;
        }
    }
}

static void mass_storage_bulk_out(uint8_t ep)
{
    uint32_t bytes_read = 0U;
    uint8_t bo_buf[MASS_STORAGE_BULK_EP_MPS];

    usbd_ep_read(ep, bo_buf, MASS_STORAGE_BULK_EP_MPS,
                 &bytes_read);

    switch (usbd_msc_cfg.stage) {
        /*the device has to decode the CBW received*/
        case MSC_BS_CBW:
            USBD_LOG_DBG("> BO - MSC_BS_CBW\r\n");
            usbd_msc_cbw_decode(bo_buf, bytes_read);
            break;

        /*the device has to receive data from the host*/
        case MSC_BS_DATA_OUT:
            switch (usbd_msc_cfg.cbw.CB[0]) {
                case SCSI_WRITE10:
                case SCSI_WRITE12:
                    /* USBD_LOG_DBG("> BO - PROC_CBW WR");*/
                    usbd_msc_memory_write(bo_buf, bytes_read);
                    break;

                case SCSI_VERIFY10:
                    USBD_LOG_DBG("> BO - PROC_CBW VER\r\n");
                    usbd_msc_memory_verify(bo_buf, bytes_read);
                    break;

                default:
                    USBD_LOG_ERR("> BO - PROC_CBW default <<ERROR!!!>>\r\n");
                    break;
            }

            break;

        case MSC_BS_CSW:
            break;

        /*an error has occurred: stall endpoint and send CSW*/
        default:
            USBD_LOG_WRN("Stall OUT endpoint, stage: %d\r\n", usbd_msc_cfg.stage);
            // usbd_ep_set_stall(ep);
            // usbd_msc_cfg.csw.Status = CSW_STATUS_PHASE_ERROR;
            // usbd_msc_send_csw();
            break;
    }

    /*set ep ack to recv next data*/
    usbd_ep_read(ep, NULL, 0, NULL);
}

/**
 * @brief EP Bulk IN handler, used to send data to the Host
 *
 * @param ep        Endpoint address.
 * @param ep_status Endpoint status code.
 *
 * @return  N/A.
 */
static void mass_storage_bulk_in(uint8_t ep)
{
    USBD_LOG_DBG("I:%d\r\n", usbd_msc_cfg.stage);

    switch (usbd_msc_cfg.stage) {
        /*the device has to send data to the host*/
        case MSC_BS_DATA_IN:
            switch (usbd_msc_cfg.cbw.CB[0]) {
                case SCSI_READ10:
                case SCSI_READ12:
                    /* USBD_LOG_DBG("< BI - PROC_CBW  READ"); */
                    usbd_msc_memory_read();
                    break;

                default:
                    USBD_LOG_ERR("< BI-PROC_CBW default <<ERROR!!>>\r\n");
                    break;
            }

            break;

        /*the device has to send a CSW*/
        case MSC_BS_DATA_IN_LAST:
            USBD_LOG_DBG("< BI - MSC_BS_DATA_IN_LAST\r\n");
            usbd_msc_send_csw();
            break;

        case MSC_BS_DATA_IN_LAST_STALL:
            USBD_LOG_WRN("Stall IN endpoint, stage: %d\r\n", usbd_msc_cfg.stage);
            //usbd_ep_set_stall(mass_ep_data[MSD_IN_EP_IDX].ep_addr);
            usbd_msc_send_csw();
            break;

        /*the host has received the CSW -> we wait a CBW*/
        case MSC_BS_CSW:
            USBD_LOG_DBG("< BI - MSC_BS_CSW\r\n");
            usbd_msc_cfg.stage = MSC_BS_CBW;
            break;

        default:
            break;
    }
}

void msc_storage_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USB_EVENT_RESET:
            usbd_msc_reset();
            break;

        default:
            break;
    }
}

static usbd_class_t msc_class;

static usbd_interface_t msc_intf = {
    .class_handler = msc_storage_class_request_handler,
    .vendor_handler = NULL,
    .notify_handler = msc_storage_notify_handler,
};

void usbd_msc_class_init(uint8_t out_ep, uint8_t in_ep)
{
    msc_class.name = "usbd_msc";

    usbd_class_register(&msc_class);
    usbd_class_add_interface(&msc_class, &msc_intf);

    mass_ep_data[0].ep_addr = out_ep;
    mass_ep_data[0].ep_cb = mass_storage_bulk_out;
    mass_ep_data[1].ep_addr = in_ep;
    mass_ep_data[1].ep_cb = mass_storage_bulk_in;

    usbd_interface_add_endpoint(&msc_intf, &mass_ep_data[0]);
    usbd_interface_add_endpoint(&msc_intf, &mass_ep_data[1]);
}