/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbh_core.h"
#include "usbh_msc.h"
#include "usb_scsi.h"

#define DEV_FORMAT "/dev/sd%c"

static uint32_t g_devinuse = 0;

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t g_msc_buf[32];

static int usbh_msc_devno_alloc(struct usbh_msc *msc_class)
{
    int devno;

    for (devno = 0; devno < 26; devno++) {
        uint32_t bitno = 1 << devno;
        if ((g_devinuse & bitno) == 0) {
            g_devinuse |= bitno;
            msc_class->sdchar = 'a' + devno;
            return 0;
        }
    }

    return -EMFILE;
}

static void usbh_msc_devno_free(struct usbh_msc *msc_class)
{
    int devno = msc_class->sdchar - 'a';

    if (devno >= 0 && devno < 26) {
        g_devinuse &= ~(1 << devno);
    }
}

static int usbh_msc_get_maxlun(struct usbh_msc *msc_class, uint8_t *buffer)
{
    struct usb_setup_packet *setup = &msc_class->hport->setup;

    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_CLASS | USB_REQUEST_RECIPIENT_INTERFACE;
    setup->bRequest = MSC_REQUEST_GET_MAX_LUN;
    setup->wValue = 0;
    setup->wIndex = msc_class->intf;
    setup->wLength = 1;

    return usbh_control_transfer(msc_class->hport->ep0, setup, buffer);
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

static inline int usbh_msc_bulk_in_transfer(struct usbh_msc *msc_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usbh_urb *urb = &msc_class->bulkin_urb;
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_bulk_urb_fill(urb, msc_class->bulkin, buffer, buflen, timeout, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

static inline int usbh_msc_bulk_out_transfer(struct usbh_msc *msc_class, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;
    struct usbh_urb *urb = &msc_class->bulkout_urb;
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_bulk_urb_fill(urb, msc_class->bulkout, buffer, buflen, timeout, NULL, NULL);
    ret = usbh_submit_urb(urb);
    if (ret == 0) {
        ret = urb->actual_length;
    }
    return ret;
}

static inline int usbh_msc_scsi_testunitready(struct usbh_msc *msc_class)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)g_msc_buf;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->bCBLength = SCSICMD_TESTUNITREADY_SIZEOF;
    cbw->CB[0] = SCSI_CMD_TESTUNITREADY;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW, CONFIG_USBHOST_MSC_TIMEOUT);
    if (nbytes >= 0) {
        /* Receive the CSW */
        nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, USB_SIZEOF_MSC_CSW, CONFIG_USBHOST_MSC_TIMEOUT);
        if (nbytes >= 0) {
            usbh_msc_csw_dump((struct CSW *)g_msc_buf);
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

static inline int usbh_msc_scsi_requestsense(struct usbh_msc *msc_class)
{
    int nbytes;
    struct CBW *cbw;

    /* Construct the CBW */
    cbw = (struct CBW *)g_msc_buf;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSIRESP_FIXEDSENSEDATA_SIZEOF;
    cbw->dDataLength = SCSICMD_REQUESTSENSE_SIZEOF;
    cbw->CB[0] = SCSI_CMD_REQUESTSENSE;
    cbw->CB[4] = SCSIRESP_FIXEDSENSEDATA_SIZEOF;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW, CONFIG_USBHOST_MSC_TIMEOUT);
    if (nbytes >= 0) {
        /* Receive the sense data response */
        nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, SCSIRESP_FIXEDSENSEDATA_SIZEOF, CONFIG_USBHOST_MSC_TIMEOUT);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, USB_SIZEOF_MSC_CSW, CONFIG_USBHOST_MSC_TIMEOUT);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)g_msc_buf);
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
    cbw = (struct CBW *)g_msc_buf;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = SCSIRESP_INQUIRY_SIZEOF;
    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSICMD_INQUIRY_SIZEOF;
    cbw->CB[0] = SCSI_CMD_INQUIRY;
    cbw->CB[4] = SCSIRESP_INQUIRY_SIZEOF;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW, CONFIG_USBHOST_MSC_TIMEOUT);
    if (nbytes >= 0) {
        /* Receive the sense data response */
        nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, SCSIRESP_INQUIRY_SIZEOF, CONFIG_USBHOST_MSC_TIMEOUT);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, USB_SIZEOF_MSC_CSW, CONFIG_USBHOST_MSC_TIMEOUT);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)g_msc_buf);
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
    cbw = (struct CBW *)g_msc_buf;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = SCSIRESP_READCAPACITY10_SIZEOF;
    cbw->bmFlags = 0x80;
    cbw->bCBLength = SCSICMD_READCAPACITY10_SIZEOF;
    cbw->CB[0] = SCSI_CMD_READCAPACITY10;

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW, CONFIG_USBHOST_MSC_TIMEOUT);
    if (nbytes >= 0) {
        /* Receive the sense data response */
        nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, SCSIRESP_READCAPACITY10_SIZEOF, CONFIG_USBHOST_MSC_TIMEOUT);
        if (nbytes >= 0) {
            /* Save the capacity information */
            msc_class->blocknum = GET_BE32(&g_msc_buf[0]) + 1;
            msc_class->blocksize = GET_BE32(&g_msc_buf[4]);
            /* Receive the CSW */
            nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, USB_SIZEOF_MSC_CSW, CONFIG_USBHOST_MSC_TIMEOUT);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)g_msc_buf);
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
    cbw = (struct CBW *)g_msc_buf;
    memset(cbw, 0, USB_SIZEOF_MSC_CBW);
    cbw->dSignature = MSC_CBW_Signature;

    cbw->dDataLength = (msc_class->blocksize * nsectors);
    cbw->bCBLength = SCSICMD_WRITE10_SIZEOF;
    cbw->CB[0] = SCSI_CMD_WRITE10;

    SET_BE32(&cbw->CB[2], start_sector);
    SET_BE16(&cbw->CB[7], nsectors);

    usbh_msc_cbw_dump(cbw);
    /* Send the CBW */
    nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW, CONFIG_USBHOST_MSC_TIMEOUT);
    if (nbytes >= 0) {
        /* Send the user data */
        nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)buffer, msc_class->blocksize * nsectors, CONFIG_USBHOST_MSC_TIMEOUT);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, USB_SIZEOF_MSC_CSW, CONFIG_USBHOST_MSC_TIMEOUT);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)g_msc_buf);
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
    cbw = (struct CBW *)g_msc_buf;
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
    nbytes = usbh_msc_bulk_out_transfer(msc_class, (uint8_t *)cbw, USB_SIZEOF_MSC_CBW, CONFIG_USBHOST_MSC_TIMEOUT);
    if (nbytes >= 0) {
        /* Receive the user data */
        nbytes = usbh_msc_bulk_in_transfer(msc_class, (uint8_t *)buffer, msc_class->blocksize * nsectors, CONFIG_USBHOST_MSC_TIMEOUT);
        if (nbytes >= 0) {
            /* Receive the CSW */
            nbytes = usbh_msc_bulk_in_transfer(msc_class, g_msc_buf, USB_SIZEOF_MSC_CSW, CONFIG_USBHOST_MSC_TIMEOUT);
            if (nbytes >= 0) {
                usbh_msc_csw_dump((struct CSW *)g_msc_buf);
            }
        }
    }
    return nbytes < 0 ? (int)nbytes : 0;
}

static int usbh_msc_connect(struct usbh_hubport *hport, uint8_t intf)
{
    struct usb_endpoint_descriptor *ep_desc;
    int ret;

    struct usbh_msc *msc_class = usb_malloc(sizeof(struct usbh_msc));
    if (msc_class == NULL) {
        USB_LOG_ERR("Fail to alloc msc_class\r\n");
        return -ENOMEM;
    }

    memset(msc_class, 0, sizeof(struct usbh_msc));
    usbh_msc_devno_alloc(msc_class);
    msc_class->hport = hport;
    msc_class->intf = intf;

    hport->config.intf[intf].priv = msc_class;

    ret = usbh_msc_get_maxlun(msc_class, g_msc_buf);
    if (ret < 0) {
        return ret;
    }

    USB_LOG_INFO("Get max LUN:%u\r\n", g_msc_buf[0] + 1);

    for (uint8_t i = 0; i < hport->config.intf[intf].altsetting[0].intf_desc.bNumEndpoints; i++) {
        ep_desc = &hport->config.intf[intf].altsetting[0].ep[i].ep_desc;
        if (ep_desc->bEndpointAddress & 0x80) {
            usbh_hport_activate_epx(&msc_class->bulkin, hport, ep_desc);
        } else {
            usbh_hport_activate_epx(&msc_class->bulkout, hport, ep_desc);
        }
    }

    ret = usbh_msc_scsi_testunitready(msc_class);
    if (ret < 0) {
        USB_LOG_ERR("Fail to scsi_testunitready\r\n");
        return ret;
    }
    ret = usbh_msc_scsi_inquiry(msc_class);
    if (ret < 0) {
        USB_LOG_ERR("Fail to scsi_inquiry\r\n");
        return ret;
    }
    ret = usbh_msc_scsi_readcapacity10(msc_class);
    if (ret < 0) {
        USB_LOG_ERR("Fail to scsi_readcapacity10\r\n");
        return ret;
    }

    if (msc_class->blocksize > 0) {
        USB_LOG_INFO("Capacity info:\r\n");
        USB_LOG_INFO("Block num:%d,block size:%d\r\n", (unsigned int)msc_class->blocknum, (unsigned int)msc_class->blocksize);
    } else {
        USB_LOG_ERR("Invalid block size\r\n");
        return -ERANGE;
    }

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, msc_class->sdchar);

    USB_LOG_INFO("Register MSC Class:%s\r\n", hport->config.intf[intf].devname);

    usbh_msc_run(msc_class);
    return ret;
}

static int usbh_msc_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    struct usbh_msc *msc_class = (struct usbh_msc *)hport->config.intf[intf].priv;

    if (msc_class) {
        usbh_msc_devno_free(msc_class);

        if (msc_class->bulkin) {
            usbh_pipe_free(msc_class->bulkin);
        }

        if (msc_class->bulkout) {
            usbh_pipe_free(msc_class->bulkout);
        }

        usbh_msc_stop(msc_class);
        memset(msc_class, 0, sizeof(struct usbh_msc));
        usb_free(msc_class);

        if (hport->config.intf[intf].devname[0] != '\0')
            USB_LOG_INFO("Unregister MSC Class:%s\r\n", hport->config.intf[intf].devname);
    }

    return ret;
}

__WEAK void usbh_msc_run(struct usbh_msc *msc_class)
{

}

__WEAK void usbh_msc_stop(struct usbh_msc *msc_class)
{

}

const struct usbh_class_driver msc_class_driver = {
    .driver_name = "msc",
    .connect = usbh_msc_connect,
    .disconnect = usbh_msc_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info msc_class_info = {
    .match_flags = USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_MASS_STORAGE,
    .subclass = MSC_SUBCLASS_SCSI,
    .protocol = MSC_PROTOCOL_BULK_ONLY,
    .vid = 0x00,
    .pid = 0x00,
    .class_driver = &msc_class_driver
};
