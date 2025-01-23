/*
 * Copyright (c) 2025, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <fcntl.h>

#include "usbd_core.h"
#include "usbd_cdc_acm.h"

#include <nuttx/mm/circbuf.h>

#ifndef CONFIG_USBDEV_CDCACM_RXBUFSIZE
#define CONFIG_USBDEV_CDCACM_RXBUFSIZE 512
#endif

#ifndef CONFIG_USBDEV_CDCACM_TXBUFSIZE
#define CONFIG_USBDEV_CDCACM_TXBUFSIZE 512
#endif

USB_NOCACHE_RAM_SECTION struct usbdev_serial_s {
    char name[16];
    struct circbuf_s circ;
    uint8_t inep;
    uint8_t outep;
    struct usbd_interface ctrl_intf;
    struct usbd_interface data_intf;
    __attribute__((aligned(32))) uint8_t cache_tempbuffer[512];
    __attribute__((aligned(32))) uint8_t cache_rxbuffer[CONFIG_USBDEV_CDCACM_RXBUFSIZE];
    __attribute__((aligned(32))) uint8_t cache_txbuffer[CONFIG_USBDEV_CDCACM_TXBUFSIZE];
};

struct usbdev_serial_ep_s {
    uint32_t rxlen;
    int error;
    sem_t txdone_sem;
    sem_t rxdone_sem;
    bool used;
};

struct usbdev_serial_s *g_usb_cdcacm_serial[8] = { 0 };
struct usbdev_serial_ep_s g_usb_cdcacm_serial_ep[2][8] = { 0 };

void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    g_usb_cdcacm_serial_ep[0][ep & 0x0f].error = 0;
    g_usb_cdcacm_serial_ep[0][ep & 0x0f].rxlen = nbytes;
    nxsem_post(&g_usb_cdcacm_serial_ep[0][ep & 0x0f].rxdone_sem);
}

void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(busid, ep, NULL, 0);
    } else {
        nxsem_post(&g_usb_cdcacm_serial_ep[1][ep & 0x0f].txdone_sem);
    }
}

/* Character driver methods */

static int usbdev_open(FAR struct file *filep);
static int usbdev_close(FAR struct file *filep);
static ssize_t usbdev_read(FAR struct file *filep, FAR char *buffer,
                           size_t buflen);
static ssize_t usbdev_write(FAR struct file *filep,
                            FAR const char *buffer, size_t buflen);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_usbdevops = {
    usbdev_open,  /* open */
    usbdev_close, /* close */
    usbdev_read,  /* read */
    usbdev_write, /* write */
    NULL,         /* seek */
    NULL,         /* ioctl */
    NULL,         /* mmap */
    NULL,         /* truncate */
    NULL          /* poll */
};

static int usbdev_open(FAR struct file *filep)
{
    FAR struct inode *inode = filep->f_inode;

    DEBUGASSERT(inode->i_private);

    if (usb_device_is_configured(0)) {
        return OK;
    } else {
        return -ENODEV;
    }
}

static int usbdev_close(FAR struct file *filep)
{
    FAR struct inode *inode = filep->f_inode;

    DEBUGASSERT(inode->i_private);

    if (!usb_device_is_configured(0)) {
        return -ENODEV;
    }

    return 0;
}

static ssize_t usbdev_read(FAR struct file *filep, FAR char *buffer,
                           size_t buflen)
{
    FAR struct inode *inode = filep->f_inode;
    struct usbdev_serial_s *serial;
    int ret;

    DEBUGASSERT(inode->i_private);
    serial = (struct usbdev_serial_s *)inode->i_private;

    if (!usb_device_is_configured(0)) {
        return -ENODEV;
    }

    while (circbuf_used(&serial->circ) == 0) {
        nxsem_reset(&g_usb_cdcacm_serial_ep[0][serial->outep & 0x0f].rxdone_sem, 0);
        usbd_ep_start_read(0, serial->outep, serial->cache_tempbuffer, usbd_get_ep_mps(0, serial->outep));
        ret = nxsem_wait(&g_usb_cdcacm_serial_ep[0][serial->outep & 0x0f].rxdone_sem);
        if (ret < 0) {
            return ret;
        }
        if (g_usb_cdcacm_serial_ep[0][serial->outep & 0x0f].error < 0) {
            return g_usb_cdcacm_serial_ep[0][serial->outep & 0x0f].error;
        }
#if defined(CONFIG_ARCH_DCACHE) && !defined(CONFIG_USB_DCACHE_ENABLE)
        up_invalidate_dcache((uintptr_t)serial->cache_tempbuffer, (uintptr_t)(serial->cache_tempbuffer + USB_ALIGN_UP(g_usb_cdcacm_serial_ep[0][serial->outep & 0x0f].rxlen, 64)));
#endif
        circbuf_overwrite(&serial->circ, serial->cache_tempbuffer, g_usb_cdcacm_serial_ep[0][serial->outep & 0x0f].rxlen);
    }
    return circbuf_read(&serial->circ, buffer, buflen);
}

static ssize_t usbdev_write(FAR struct file *filep, FAR const char *buffer,
                            size_t buflen)
{
    FAR struct inode *inode = filep->f_inode;
    struct usbdev_serial_s *serial;
    int ret;

    DEBUGASSERT(inode->i_private);
    serial = (struct usbdev_serial_s *)inode->i_private;

    if (!usb_device_is_configured(0)) {
        return -ENODEV;
    }

#ifdef CONFIG_ARCH_DCACHE
    uint32_t write_len = 0;

    while (write_len < buflen) {
        uint32_t len = buflen - write_len;
        if (len > CONFIG_USBDEV_CDCACM_TXBUFSIZE) {
            len = CONFIG_USBDEV_CDCACM_TXBUFSIZE;
        }
        memcpy(serial->cache_txbuffer, buffer + write_len, len);
#ifndef CONFIG_USB_DCACHE_ENABLE
        up_clean_dcache((uintptr_t)serial->cache_txbuffer, (uintptr_t)(serial->cache_txbuffer + USB_ALIGN_UP(len, 64)));
#endif
        nxsem_reset(&g_usb_cdcacm_serial_ep[0][serial->inep & 0x0f].txdone_sem, 0);
        usbd_ep_start_write(0, serial->inep, serial->cache_txbuffer, len);
        ret = nxsem_wait(&g_usb_cdcacm_serial_ep[0][serial->inep & 0x0f].txdone_sem);
        if (ret < 0) {
            return ret;
        } else {
            if (g_usb_cdcacm_serial_ep[1][serial->inep & 0x0f].error < 0) {
                return g_usb_cdcacm_serial_ep[1][serial->inep & 0x0f].error;
            }
            write_len += len;
        }
    }
    return buflen;
#else
    nxsem_reset(&g_usb_cdcacm_serial_ep[0][outep & 0x0f].txdone_sem, 0);
    usbd_ep_start_write(0, serial->inep, buffer, buflen);
    ret = nxsem_wait(&g_usb_cdcacm_serial_ep[0][outep & 0x0f].txdone_sem);
    if (ret < 0) {
        return ret;
    } else {
        if (g_usb_cdcacm_serial_ep[1][serial->inep & 0x0f].error < 0) {
            return g_usb_cdcacm_serial_ep[1][serial->inep & 0x0f].error;
        }
        return buflen;
    }
#endif
}

static struct usbd_endpoint cdc_out_ep[8] = { 0 };
static struct usbd_endpoint cdc_in_ep[8] = { 0 };

static void cdcacm_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_DISCONNECTED:
            for (size_t i = 0; i < 8; i++) {
                if (g_usb_cdcacm_serial_ep[0][i & 0x0f].used) {
                    g_usb_cdcacm_serial_ep[0][i & 0x0f].error = -ESHUTDOWN;
                    nxsem_post(&g_usb_cdcacm_serial_ep[0][i & 0x0f].rxdone_sem);
                }
                if (g_usb_cdcacm_serial_ep[1][i & 0x0f].used) {
                    g_usb_cdcacm_serial_ep[1][i & 0x0f].error = -ESHUTDOWN;
                    nxsem_post(&g_usb_cdcacm_serial_ep[1][i & 0x0f].txdone_sem);
                }
            }
            break;
        case USBD_EVENT_CONFIGURED:
            for (size_t i = 0; i < 8; i++) {
                if (g_usb_cdcacm_serial_ep[0][i & 0x0f].used) {
                    g_usb_cdcacm_serial_ep[0][i & 0x0f].error = 0;
                    nxsem_post(&g_usb_cdcacm_serial_ep[0][i & 0x0f].rxdone_sem);
                }
                if (g_usb_cdcacm_serial_ep[1][i & 0x0f].used) {
                    g_usb_cdcacm_serial_ep[1][i & 0x0f].error = 0;
                    nxsem_post(&g_usb_cdcacm_serial_ep[1][i & 0x0f].txdone_sem);
                }
            }
            break;
        default:
            break;
    }
}

void usbd_cdcacm_init(uint8_t busid, uint8_t id, const char *path, uint8_t outep, uint8_t inep)
{
    g_usb_cdcacm_serial[id] = kmm_malloc(sizeof(struct usbdev_serial_s));
    DEBUGASSERT(g_usb_cdcacm_serial[id]);

    memset(g_usb_cdcacm_serial[id], 0, sizeof(struct usbdev_serial_s));
    strncpy(g_usb_cdcacm_serial[id]->name, path, sizeof(g_usb_cdcacm_serial[id]->name) - 1);

    circbuf_init(&g_usb_cdcacm_serial[id]->circ, g_usb_cdcacm_serial[id]->cache_rxbuffer, CONFIG_USBDEV_CDCACM_RXBUFSIZE);

    nxsem_init(&g_usb_cdcacm_serial_ep[0][outep & 0x0f].rxdone_sem, 0, 0);
    nxsem_init(&g_usb_cdcacm_serial_ep[1][inep & 0x0f].txdone_sem, 0, 0);
    g_usb_cdcacm_serial_ep[0][outep & 0x0f].used = true;
    g_usb_cdcacm_serial_ep[1][inep & 0x0f].used = true;

    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &g_usb_cdcacm_serial[id]->ctrl_intf));
    usbd_add_interface(busid, usbd_cdc_acm_init_intf(busid, &g_usb_cdcacm_serial[id]->data_intf));
    g_usb_cdcacm_serial[id]->ctrl_intf.notify_handler = cdcacm_notify_handler;
    g_usb_cdcacm_serial[id]->outep = outep;
    g_usb_cdcacm_serial[id]->inep = inep;

    cdc_out_ep[id].ep_addr = outep;
    cdc_out_ep[id].ep_cb = usbd_cdc_acm_bulk_out;
    cdc_in_ep[id].ep_addr = inep;
    cdc_in_ep[id].ep_cb = usbd_cdc_acm_bulk_in;

    usbd_add_endpoint(busid, &cdc_out_ep[id]);
    usbd_add_endpoint(busid, &cdc_in_ep[id]);

    register_driver(path, &g_usbdevops, 0666, g_usb_cdcacm_serial[id]);
}

void usbd_cdcacm_deinit(uint8_t busid, uint8_t id)
{
    unregister_driver(g_usb_cdcacm_serial[id]->name);

    kmm_free(g_usb_cdcacm_serial[id]);
}