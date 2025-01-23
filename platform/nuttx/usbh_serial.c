/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>

#include "usbh_core.h"
#include "usbh_cdc_acm.h"

#include <nuttx/mm/circbuf.h>

#define DEV_FORMAT "/dev/ttyACM%d"

#ifndef CONFIG_USBHOST_CDCACM_RXBUFSIZE
#define CONFIG_USBHOST_CDCACM_RXBUFSIZE 512
#endif

#ifndef CONFIG_USBHOST_CDCACM_TXBUFSIZE
#define CONFIG_USBHOST_CDCACM_TXBUFSIZE 512
#endif

struct usbhost_serial_s {
    struct circbuf_s circ;
    __attribute__((aligned(32))) uint8_t cache_rxbuffer[CONFIG_USBHOST_CDCACM_RXBUFSIZE];
    __attribute__((aligned(32))) uint8_t cache_txbuffer[CONFIG_USBHOST_CDCACM_TXBUFSIZE];
};

static int nuttx_errorcode(int error)
{
    int err = 0;

    switch (error) {
        case -USB_ERR_NOMEM:
            err = -EIO;
            break;
        case -USB_ERR_INVAL:
            err = -EINVAL;
            break;
        case -USB_ERR_NODEV:
            err = -ENODEV;
            break;
        case -USB_ERR_NOTCONN:
            err = -ENOTCONN;
            break;
        case -USB_ERR_NOTSUPP:
            err = -EIO;
            break;
        case -USB_ERR_BUSY:
            err = -EBUSY;
            break;
        case -USB_ERR_RANGE:
            err = -ERANGE;
            break;
        case -USB_ERR_STALL:
            err = -EPERM;
            break;
        case -USB_ERR_NAK:
            err = -EAGAIN;
            break;
        case -USB_ERR_DT:
            err = -EIO;
            break;
        case -USB_ERR_IO:
            err = -EIO;
            break;
        case -USB_ERR_SHUTDOWN:
            err = -ESHUTDOWN;
            break;
        case -USB_ERR_TIMEOUT:
            err = -ETIMEDOUT;
            break;

        default:
            break;
    }
    return err;
}

/* Character driver methods */

static int usbhost_open(FAR struct file *filep);
static int usbhost_close(FAR struct file *filep);
static ssize_t usbhost_read(FAR struct file *filep, FAR char *buffer,
                            size_t buflen);
static ssize_t usbhost_write(FAR struct file *filep,
                             FAR const char *buffer, size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_usbhostops = {
    usbhost_open,  /* open */
    usbhost_close, /* close */
    usbhost_read,  /* read */
    usbhost_write, /* write */
    NULL,          /* seek */
    NULL,          /* ioctl */
    NULL,          /* mmap */
    NULL,          /* truncate */
    NULL           /* poll */
};

static int usbhost_open(FAR struct file *filep)
{
    struct usbh_cdc_acm *cdc_acm_class;
    FAR struct inode *inode = filep->f_inode;

    DEBUGASSERT(inode->i_private);
    cdc_acm_class = (struct usbh_cdc_acm *)inode->i_private;

    if (cdc_acm_class->hport && cdc_acm_class->hport->connected) {
        return OK;
    } else {
        return -ENODEV;
    }
}

static int usbhost_close(FAR struct file *filep)
{
    FAR struct inode *inode = filep->f_inode;

    DEBUGASSERT(inode->i_private);
    return 0;
}

static ssize_t usbhost_read(FAR struct file *filep, FAR char *buffer,
                            size_t buflen)
{
    struct usbh_cdc_acm *cdc_acm_class;
    FAR struct inode *inode = filep->f_inode;
    struct usbhost_serial_s *serial;
    __attribute__((aligned(32))) uint8_t cache_tempbuffer[512];
    int ret;

    DEBUGASSERT(inode->i_private || cdc_acm_class->user_data);
    cdc_acm_class = (struct usbh_cdc_acm *)inode->i_private;
    serial = cdc_acm_class->user_data;

    while (circbuf_used(&serial->circ) == 0) {
        ret = usbh_cdc_acm_bulk_in_transfer(cdc_acm_class, cache_tempbuffer, cdc_acm_class->bulkin->wMaxPacketSize, 0xffffffff);
        if (ret < 0) {
            return nuttx_errorcode(ret);
        }
#if defined(CONFIG_ARCH_DCACHE) && !defined(CONFIG_USB_DCACHE_ENABLE)
        up_invalidate_dcache((uintptr_t)cache_tempbuffer, (uintptr_t)(cache_tempbuffer + cdc_acm_class->bulkin->wMaxPacketSize));
#endif
        circbuf_overwrite(&serial->circ, cache_tempbuffer, USB_ALIGN_UP(ret, 64));
    }
    return circbuf_read(&serial->circ, buffer, buflen);
}

static ssize_t usbhost_write(FAR struct file *filep, FAR const char *buffer,
                             size_t buflen)
{
    struct usbh_cdc_acm *cdc_acm_class;
    FAR struct inode *inode = filep->f_inode;
    struct usbhost_serial_s *serial;
    int ret;

    DEBUGASSERT(inode->i_private || cdc_acm_class->user_data);
    cdc_acm_class = (struct usbh_cdc_acm *)inode->i_private;
    serial = cdc_acm_class->user_data;

#ifdef CONFIG_ARCH_DCACHE
    uint32_t write_len = 0;

    while (write_len < buflen) {
        uint32_t len = buflen - write_len;
        if (len > CONFIG_USBHOST_CDCACM_TXBUFSIZE) {
            len = CONFIG_USBHOST_CDCACM_TXBUFSIZE;
        }
        memcpy(serial->cache_txbuffer, buffer + write_len, len);
#ifndef CONFIG_USB_DCACHE_ENABLE
        up_clean_dcache((uintptr_t)serial->cache_txbuffer, (uintptr_t)(serial->cache_txbuffer + USB_ALIGN_UP(len, 64)));
#endif
        ret = usbh_cdc_acm_bulk_out_transfer(cdc_acm_class, serial->cache_txbuffer, len, 0xffffffff);
        if (ret < 0) {
            return nuttx_errorcode(ret);
        } else {
            write_len += len;
        }
    }
    return buflen;
#else
    ret = usbh_cdc_acm_bulk_out_transfer(cdc_acm_class, (uint8_t *)buffer, buflen, 0xffffffff);
    if (ret < 0) {
        return nuttx_errorcode(ret);
    } else {
        return buflen;
    }
#endif
}

void usbh_cdc_acm_run(struct usbh_cdc_acm *cdc_acm_class)
{
    char devname[32];
    struct usbhost_serial_s *serial;

    snprintf(devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cdc_acm_class->minor);

    serial = kmm_malloc(sizeof(struct usbhost_serial_s));
    DEBUGASSERT(serial);

    memset(serial, 0, sizeof(struct usbhost_serial_s));

    circbuf_init(&serial->circ, serial->cache_rxbuffer, CONFIG_USBHOST_CDCACM_RXBUFSIZE);

    cdc_acm_class->user_data = serial;

    struct cdc_line_coding linecoding;

    linecoding.dwDTERate = 115200;
    linecoding.bDataBits = 8;
    linecoding.bParityType = 0;
    linecoding.bCharFormat = 0;
    usbh_cdc_acm_set_line_coding(cdc_acm_class, &linecoding);
    usbh_cdc_acm_set_line_state(cdc_acm_class, true, false);

    register_driver(devname, &g_usbhostops, 0666, cdc_acm_class);
}

void usbh_cdc_acm_stop(struct usbh_cdc_acm *cdc_acm_class)
{
    char devname[32];
    struct usbhost_serial_s *serial;

    snprintf(devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, cdc_acm_class->minor);
    unregister_driver(devname);

    serial = cdc_acm_class->user_data;

    kmm_free(serial);
}