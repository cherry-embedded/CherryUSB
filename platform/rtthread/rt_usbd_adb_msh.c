/*
 * Copyright (c) 2024 ~ 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <rtthread.h>
#include <rtdevice.h>

#include "usbd_core.h"
#include "usbd_adb.h"

#ifdef RT_USING_FINSH
#include <shell.h>
#include <finsh.h>
#endif

#ifndef CONFIG_USBDEV_SHELL_RX_BUFSIZE
#define CONFIG_USBDEV_SHELL_RX_BUFSIZE (2048)
#endif

struct usbd_adb_shell {
    struct rt_device parent;
    struct rt_ringbuffer rx_rb;
    rt_uint8_t rx_rb_buffer[CONFIG_USBDEV_SHELL_RX_BUFSIZE];
} g_usbd_adb_shell;

static rt_err_t usbd_adb_shell_open(struct rt_device *dev, rt_uint16_t oflag)
{
    while (!usb_device_is_configured(0)) {
        rt_thread_mdelay(10);
    }
    return RT_EOK;
}

static rt_err_t usbd_adb_shell_close(struct rt_device *dev)
{
    return RT_EOK;
}

static rt_ssize_t usbd_adb_shell_read(struct rt_device *dev,
                                      rt_off_t pos,
                                      void *buffer,
                                      rt_size_t size)
{
    return rt_ringbuffer_get(&g_usbd_adb_shell.rx_rb, (rt_uint8_t *)buffer, size);
}

static rt_ssize_t usbd_adb_shell_write(struct rt_device *dev,
                                       rt_off_t pos,
                                       const void *buffer,
                                       rt_size_t size)
{
    int ret = 0;

    RT_ASSERT(dev != RT_NULL);

    usbd_adb_write(ADB_LOCALID_SHELL, buffer, size);

    return size;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops usbd_adb_shell_ops = {
    NULL,
    usbd_adb_shell_open,
    usbd_adb_shell_close,
    usbd_adb_shell_read,
    usbd_adb_shell_write,
    NULL
};
#endif

void rt_usbd_adb_shell_init(void)
{
    rt_err_t ret;
    struct rt_device *device;

    device = &(g_usbd_adb_shell.parent);

    device->type = RT_Device_Class_Char;
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

#ifdef RT_USING_DEVICE_OPS
    device->ops = &usbd_adb_shell_ops;
#else
    device->init = NULL;
    device->open = usbd_adb_shell_open;
    device->close = usbd_adb_shell_close;
    device->read = usbd_adb_shell_read;
    device->write = usbd_adb_shell_write;
    device->control = NULL;
#endif
    device->user_data = NULL;

    /* register a character device */
    ret = rt_device_register(device, "adb-sh", RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_REMOVABLE);

#ifdef RT_USING_POSIX_DEVIO
    /* set fops */
    device->fops = NULL;
#endif

    rt_ringbuffer_init(&g_usbd_adb_shell.rx_rb, g_usbd_adb_shell.rx_rb_buffer, sizeof(g_usbd_adb_shell.rx_rb_buffer));
}

static void adb_shell_on_open(uint32_t remoteid)
{
    USB_LOG_INFO("adb shell open, remoteid:%u\r\n", (unsigned)remoteid);
}

static void adb_shell_on_close(uint32_t remoteid)
{
    USB_LOG_INFO("adb shell close, remoteid:%u\r\n", (unsigned)remoteid);
}

static void adb_shell_on_write(uint32_t remoteid, const uint8_t *data, uint32_t len)
{
    rt_size_t put;

    put = rt_ringbuffer_put(&g_usbd_adb_shell.rx_rb, data, len);
    if (g_usbd_adb_shell.parent.rx_indicate != RT_NULL) {
        g_usbd_adb_shell.parent.rx_indicate(&g_usbd_adb_shell.parent, put);
    }
}

static const struct adb_service adb_shell_service = {
    .name = "shell:",
    .localid = ADB_LOCALID_SHELL,
    .on_open = adb_shell_on_open,
    .on_close = adb_shell_on_close,
    .on_write = adb_shell_on_write,
};

void usbd_adb_shell_init(void)
{
    if (usbd_adb_service_register(&adb_shell_service) != 0) {
        USB_LOG_ERR("adb shell: register failed\r\n");
    }
    rt_usbd_adb_shell_init();
}

static int adb_enter(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    finsh_set_device("adb-sh");
    rt_console_set_device("adb-sh");

    return 0;
}
MSH_CMD_EXPORT(adb_enter, adb_enter);

static int adb_exit(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    usbd_adb_close(ADB_LOCALID_SHELL);

    if (rt_console_get_device() != rt_device_find(RT_CONSOLE_DEVICE_NAME)) {
        rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
        finsh_set_device(RT_CONSOLE_DEVICE_NAME);
    }

    return 0;
}
MSH_CMD_EXPORT(adb_exit, adb_exit);
