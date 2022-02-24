/**
 * @file usbh_msc.c
 * @brief
 *
 * Copyright (c) 2022 sakumisu
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
#include "usbh_core.h"
#include "usbh_msc.h"
#include "usb_scsi.h"

#define DEV_FORMAT "/dev/sd%c"

static uint32_t g_devinuse = 0;

/****************************************************************************
 * Name: usbh_msc_devno_alloc
 *
 * Description:
 *   Allocate a unique /dev/ttyACM[n] minor number in the range 0-31.
 *
 ****************************************************************************/

static int usbh_msc_devno_alloc(struct usbh_msc *priv)
{
    uint32_t flags;
    int devno;

    flags = usb_osal_enter_critical_section();
    for (devno = 0; devno < 26; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            priv->sdchar = 'a' + devno;
            usb_osal_leave_critical_section(flags);
            return 0;
        }
    }

    usb_osal_leave_critical_section(flags);
    return -EMFILE;
}

/****************************************************************************
 * Name: usbh_msc_devno_free
 *
 * Description:
 *   Free a /dev/sd[n] minor number so that it can be used.
 *
 ****************************************************************************/

static void usbh_msc_devno_free(struct usbh_msc *priv)
{
    int devno = priv->sdchar - 'a';

    if (devno >= 0 && devno < 26) {
        uint32_t flags = usb_osal_enter_critical_section();
        g_devinuse &= ~(1 << devno);
        usb_osal_leave_critical_section(flags);
    }
}

static int usbh_msc_get_maxlun(struct usbh_hubport *hport, uint8_t intf, uint8_t *buffer)
{
    struct usb_setup_packet *setup;
    struct usbh_msc *msc_class = (struct usbh_msc *)hport->config.intf[intf].priv;

    setup = hport->setup;

    if (msc_class->intf != intf) {
        return -1;
    }

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = MSC_REQUEST_GET_MAX_LUN;
    setup->wValue = 0;
    setup->wIndex = intf;
    setup->wLength = 1;

    return usbh_control_transfer(hport->ep0, setup, buffer);
}

static void usbh_msc_cbw_dump(struct CBW *cbw)
{
#if 0
    int i;

    USB_LOG_INFO("CBW:\r\n");
    USB_LOG_INFO("  signature: 0x%08x\r\n", (unsigned int)cbw->dSignature);
    USB_LOG_INFO("  tag:       0x%08x\r\n", (unsigned int)cbw->dTag);
    USB_LOG_INFO("  datlen:    0x%08x\r\n", (unsigned int)cbw->dDataLength);
    USB_LOG_INFO("  flags:     0x%02x\r\n", cbw->bmFlags);
    USB_LOG_INFO("  lun:       0x%02x\r\n", cbw->bLUN);
    USB_LOG_INFO("  cblen:    0x%02x\r\n", cbw->bCBLength);

    USB_LOG_INFO("CB:\r\n");
    for (i = 0; i < cbw->bCBLength; i += 8) {
        USB_LOG_INFO("  0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\r\n",
                     cbw->CB[i], cbw->CB[i + 1], cbw->CB[i + 2],
                     cbw->CB[i + 3], cbw->CB[i + 4], cbw->CB[i + 5],
                     cbw->CB[i + 6], cbw->CB[i + 7]);
    }
#endif
}

static void usbh_msc_csw_dump(struct CSW *csw)
{
#if 0
    USB_LOG_INFO("CSW:\r\n");
    USB_LOG_INFO("  signature: 0x%08x\r\n", (unsigned int)csw->dSignature);
    USB_LOG_INFO("  tag:       0x%08x\r\n", (unsigned int)csw->dTag);
    USB_LOG_INFO("  residue:   0x%08x\r\n", (unsigned int)csw->dDataResidue);
    USB_LOG_INFO("  status:    0x%02x\r\n", csw->bStatus);
#endif
}

static inline int usbh_msc_scsi_testunitready(struct usbh_msc *msc_class)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)msc_class->tx_buffer;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->bCBLength = SCSICMD_TESTUNITREADY_SIZEOF;
    cbw->CB[0] = SCSI_CMD_TESTUNITREADY;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW);
    if (nbytes >= 0) {
        /* Receive the CSW */
        nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, USB_SIZEOF_MSC_CSW);
        if (nbytes >= 0) {
            usbh_msc_csw_dump((struct CSW *)msc_class->tx_buffer);
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

static inline int usbh_msc_scsi_requestsense(struct usbh_msc *msc_class)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)msc_class->tx_buffer;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSIRESP_FIXEDSENSEDATA_SIZEOF;
    cbw->dDataLength = SCSICMD_REQUESTSENSE_SIZEOF;
    cbw->CB[0] = SCSI_CMD_REQUESTSENSE;
    cbw->CB[4] = SCSIRESP_FIXEDSENSEDATA_SIZEOF;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW);
    if (nbytes >= 0) {
        /* Receive the sense data response */
        nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, SCSIRESP_FIXEDSENSEDATA_SIZEOF);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, USB_SIZEOF_MSC_CSW);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)msc_class->tx_buffer);
            }
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

static inline int usbh_msc_scsi_inquiry(struct usbh_msc *msc_class)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)msc_class->tx_buffer;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = SCSIRESP_INQUIRY_SIZEOF;
    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSICMD_INQUIRY_SIZEOF;
    cbw->CB[0] = SCSI_CMD_INQUIRY;
    cbw->CB[4] = SCSIRESP_INQUIRY_SIZEOF;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW);
    if (nbytes >= 0) {
        /* Receive the sense data response */
        nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, SCSIRESP_INQUIRY_SIZEOF);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, USB_SIZEOF_MSC_CSW);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)msc_class->tx_buffer);
            }
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

static inline int usbh_msc_scsi_readcapacity10(struct usbh_msc *msc_class)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)msc_class->tx_buffer;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = SCSIRESP_READCAPACITY10_SIZEOF;
    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSICMD_READCAPACITY10_SIZEOF;
    cbw->CB[0] = SCSI_CMD_READCAPACITY10;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW);
    if (nbytes >= 0) {
        /* Receive the sense data response */
        nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, SCSIRESP_READCAPACITY10_SIZEOF);
        if (nbytes >= 0) {
            /* Save the capacity information */
            msc_class->blocknum = GET_BE32(&msc_class->tx_buffer[0]) + 1;
            msc_class->blocksize = GET_BE32(&msc_class->tx_buffer[4]);
            /* Receive the CSW */
            nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, USB_SIZEOF_MSC_CSW);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)msc_class->tx_buffer);
            }
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

int usbh_msc_scsi_write10(struct usbh_msc *msc_class, uint32_t start_sector, const uint8_t *buffer, uint32_t nsectors)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)msc_class->tx_buffer;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = (msc_class->blocksize * nsectors);
    cbw->bCBLength = SCSICMD_WRITE10_SIZEOF;
    cbw->CB[0] = SCSI_CMD_WRITE10;

    SET_BE32(&cbw->CB[2], start_sector);
    SET_BE16(&cbw->CB[7], nsectors);

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW);
    if (nbytes >= 0) {
        /* Send the user data */
        nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)buffer, msc_class->blocksize * nsectors);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, USB_SIZEOF_MSC_CSW);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)msc_class->tx_buffer);
            }
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

int usbh_msc_scsi_read10(struct usbh_msc *msc_class, uint32_t start_sector, const uint8_t *buffer, uint32_t nsectors)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)msc_class->tx_buffer;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = (msc_class->blocksize * nsectors);
    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSICMD_READ10_SIZEOF;
    cbw->CB[0] = SCSI_CMD_READ10;

    SET_BE32(&cbw->CB[2], start_sector);
    SET_BE16(&cbw->CB[7], nsectors);

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_ep_bulk_transfer(msc_class->bulkout, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW);
    if (nbytes >= 0) {
        /* Receive the user data */
        nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, (uint8_t *)buffer, msc_class->blocksize * nsectors);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_ep_bulk_transfer(msc_class->bulkin, msc_class->tx_buffer, USB_SIZEOF_MSC_CSW);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)msc_class->tx_buffer);
            }
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

int usbh_msc_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_msc *msc_class = usb_malloc(sizeof(struct usbh_msc));
    if (msc_class == NULL) {
        USB_LOG_ERR("Fail to alloc msc_class\r\n");
        return -ENOMEM;
    }

    memset(msc_class, 0, sizeof(struct usbh_msc));

    usbh_msc_devno_alloc(msc_class);
    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, msc_class->sdchar);

    hport->config.intf[intf].priv = msc_class;

    msc_class->tx_buffer = usb_iomalloc(32);
    if (msc_class->tx_buffer == NULL) {
        USB_LOG_ERR("Fail to alloc tx_buffer\r\n");
        return -ENOMEM;
    }

    ret = usbh_msc_get_maxlun(hport, intf, msc_class->tx_buffer);
    if (ret < 0) {
        return ret;
    }

    USB_LOG_INFO("Get max LUN:%u\r\n", msc_class->tx_buffer[0] + 1);

    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_ep_alloc(&msc_class->bulkin, &ep_cfg);
        } else {
            usbh_ep_alloc(&msc_class->bulkout, &ep_cfg);
        }
    }

    USB_LOG_INFO("Register MSC Class:%s\r\n", hport->config.intf[intf].devname);

    ret = usbh_msc_scsi_testunitready(msc_class);
    ret = usbh_msc_scsi_inquiry(msc_class);
    ret = usbh_msc_scsi_readcapacity10(msc_class);

    USB_LOG_INFO("Capacity info:\r\n");
    USB_LOG_INFO("Block num:%d,block size:%d\r\n", (unsigned int)msc_class->blocknum, (unsigned int)msc_class->blocksize);

    extern int msc_test();
    msc_test();
    return ret;
}

int usbh_msc_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_msc *msc_class = (struct usbh_msc *)hport->config.intf[intf].priv;

    if (msc_class) {
        usbh_msc_devno_free(msc_class);

        if (msc_class->bulkin) {
            ret = usb_ep_cancel(msc_class->bulkin);
            if (ret < 0) {
            }
            usbh_ep_free(msc_class->bulkin);
        }

        if (msc_class->bulkout) {
            ret = usb_ep_cancel(msc_class->bulkout);
            if (ret < 0) {
            }
            usbh_ep_free(msc_class->bulkout);
        }

        if (msc_class->tx_buffer)
            usb_iofree(msc_class->tx_buffer);

        usb_free(msc_class);

        USB_LOG_INFO("Unregister MSC Class:%s\r\n", hport->config.intf[intf].devname);

        memset(hport->config.intf[intf].devname, 0, CONFIG_USBHOST_DEV_NAMELEN);
        hport->config.intf[intf].priv = NULL;
    }

    return ret;
}

const struct usbh_class_driver msc_class_driver = {
    .driver_name = "msc",
    .connect = usbh_msc_connect,
    .disconnect = usbh_msc_disconnect
};