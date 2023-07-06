/**
 * @file usbh_cp210x.c
 * @author 262666882@qq.com
 * @brief 从linux驱动移植过来的,支持cp210x芯片。目前只做了简单测试，基本功能可用。
 * @version 0.1
 * @date 2023-07-05
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "rtthread.h"
#include "rtdevice.h"

#include "usbh_cp210x.h"

struct usbh_cp210x_static_device {
    struct rt_device parent;

    struct usbh_cp210x *p_device;
    struct rt_mutex lock;
};

#define CS5 00000000
#define CS6 00000400
#define CS7 00001000
#define CS8 00001400

#define CSTOPB 00002000
#define PARENB 00010000
#define PARODD 00020000

#define CMSPAR 010000000000 /* mark or space (stick) parity */

#define DEV_COUNT 4
static struct usbh_cp210x_static_device g_devices[DEV_COUNT];

void drv_usbh_cp210x_run(struct usbh_cp210x *p_device)
{
    struct usbh_cp210x_static_device *p_dev_static = g_devices + p_device->index;
    rt_mutex_take(&p_dev_static->lock, RT_WAITING_FOREVER);
    p_dev_static->p_device = p_device;
    rt_mutex_release(&p_dev_static->lock);
}

void drv_usbh_cp210x_stop(struct usbh_cp210x *p_device)
{
    int index = p_device->index;
    rt_mutex_take(&g_devices[index].lock, RT_WAITING_FOREVER);
    g_devices[index].p_device = 0;
    rt_mutex_release(&g_devices[index].lock);
}

#if 1 /** FIXME: not tested */
static void __cp210x_set(struct usbh_cp210x *p_dev, int brate, int bits, int stopb, int parity, int hwctrl)
{
    memset(&p_dev->drv_data.termios, 0, sizeof(p_dev->drv_data.termios));
    p_dev->drv_data.termios.c_iflag = 0;
    p_dev->drv_data.termios.c_oflag = 0;
    if (bits == 8)
        p_dev->drv_data.termios.c_cflag = CS8;
    else if (bits == 7)
        p_dev->drv_data.termios.c_cflag = CS7;
    else if (bits == 6)
        p_dev->drv_data.termios.c_cflag = CS6;
    else if (bits == 5)
        p_dev->drv_data.termios.c_cflag = CS5;
    else
        p_dev->drv_data.termios.c_cflag = CS8;

    int c_cflag_p = 0;
    switch (parity) {
        case PARITY_NONE:
            break;
        case PARITY_EVEN:
            c_cflag_p = PARENB;
            break;
        case PARITY_ODD:
            c_cflag_p = PARENB | PARODD;
            break;
            //case PARITY_SPACE: c_cflag_p=PARENB|CMSPAR; break;
            //case PARITY_MARK: c_cflag_p=PARENB|CMSPAR|PARODD; break;
    }
    int stopbits = 0; /* 1 stopbit default */
    if (stopb == 2) {
        stopbits = CSTOPB;
    }
    p_dev->drv_data.termios.c_cflag |= c_cflag_p | stopbits;

    p_dev->drv_data.termios.c_lflag = 0;
    p_dev->drv_data.termios.c_cc[0] = 0;
    p_dev->drv_data.termios.c_ospeed = brate;
    cp210x_set_termios(&p_dev->drv_data);
    cp210x_break_ctl(&p_dev->drv_data, hwctrl);
}
#endif

static rt_err_t __init(struct rt_device *dev)
{
    rt_err_t result = RT_EOK;
    /*struct usbh_cp210x_static_device *p_this;*/

    RT_ASSERT(dev != RT_NULL);
    /*p_this = (struct usbh_cp210x_static_device *)dev;*/
    return result;
}

static rt_err_t __open(struct rt_device *dev, rt_uint16_t oflag)
{
    rt_uint16_t stream_flag = 0;

    RT_ASSERT(dev != RT_NULL);

    /* keep steam flag */
    if ((oflag & RT_DEVICE_FLAG_STREAM) || (dev->open_flag & RT_DEVICE_FLAG_STREAM))
        stream_flag = RT_DEVICE_FLAG_STREAM;

    /* get open flags */
    dev->open_flag = oflag & 0xff;
    /* set stream flag */
    dev->open_flag |= stream_flag;
    //dev->flag |= RT_DEVICE_FLAG_ACTIVATED;

    return RT_EOK;
}

static rt_err_t __close(struct rt_device *dev)
{
    struct usbh_cp210x_static_device *p_this;

    RT_ASSERT(dev != RT_NULL);
    p_this = (struct usbh_cp210x_static_device *)dev;
    (void)p_this;

    /* this device has more reference count */
    if (dev->ref_count > 1)
        return RT_EOK;

    dev->flag &= ~RT_DEVICE_FLAG_ACTIVATED;

    return RT_EOK;
}

static rt_ssize_t __read(struct rt_device *dev,
                         rt_off_t pos,
                         void *buffer,
                         rt_size_t size)
{
    struct usbh_cp210x_static_device *p_this;
    rt_ssize_t ret;

    RT_ASSERT(dev != RT_NULL);
    if (size == 0)
        return 0;

    p_this = (struct usbh_cp210x_static_device *)dev;

    rt_mutex_take(&p_this->lock, RT_WAITING_FOREVER);
    struct usbh_cp210x *p_device = p_this->p_device;
    if (!p_device) {
        rt_mutex_release(&p_this->lock);
        return 0;
    }
    struct usbh_urb *urb = &p_device->bulkout_urb;
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_bulk_urb_fill(urb, p_device->bulkin, buffer, size, 500, NULL, NULL);
    ret = usbh_submit_urb(urb);
    rt_mutex_release(&p_this->lock);
    return ret;
}

static rt_ssize_t __write(struct rt_device *dev,
                          rt_off_t pos,
                          const void *buffer,
                          rt_size_t size)
{
    struct usbh_cp210x_static_device *p_this;
    rt_ssize_t ret;

    RT_ASSERT(dev != RT_NULL);
    if (size == 0)
        return 0;

    p_this = (struct usbh_cp210x_static_device *)dev;

    rt_mutex_take(&p_this->lock, RT_WAITING_FOREVER);
    struct usbh_cp210x *p_device = p_this->p_device;
    if (!p_device) {
        rt_mutex_release(&p_this->lock);
        return 0;
    }
    struct usbh_urb *urb = &p_device->bulkout_urb;
    memset(urb, 0, sizeof(struct usbh_urb));

    usbh_bulk_urb_fill(urb, p_device->bulkout, (uint8_t *)buffer, size, 500, NULL, NULL);
    ret = usbh_submit_urb(urb);
    rt_mutex_release(&p_this->lock);

    return ret;
}

static rt_err_t __control(struct rt_device *dev,
                          int cmd,
                          void *args)
{
    rt_err_t ret = RT_EOK;
    struct usbh_cp210x_static_device *p_this;

    RT_ASSERT(dev != RT_NULL);
    p_this = (struct usbh_cp210x_static_device *)dev;

    (void)p_this;

    rt_mutex_take(&p_this->lock, RT_WAITING_FOREVER);
    struct usbh_cp210x *p_device = p_this->p_device;
    if (!p_device) {
        rt_mutex_release(&p_this->lock);
        return 0;
    }

    switch (cmd) {
        case RT_DEVICE_CTRL_SUSPEND:
            /* suspend device */
            dev->flag |= RT_DEVICE_FLAG_SUSPENDED;
            break;

        case RT_DEVICE_CTRL_RESUME:
            /* resume device */
            dev->flag &= ~RT_DEVICE_FLAG_SUSPENDED;
            break;

        case RT_DEVICE_CTRL_CONFIG:
/** FIXME: not tested */
#if 1
            if (args) {
                struct serial_configure *pconfig = (struct serial_configure *)args;
                __cp210x_set(p_device, pconfig->baud_rate, pconfig->data_bits, pconfig->stop_bits, pconfig->parity, pconfig->flowcontrol);
            }
#endif
            break;
        default:
            break;
    }

    rt_mutex_release(&p_this->lock);
    return ret;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops p_this_ops = {
    __init,
    __open,
    __close,
    __read,
    __write,
    __control
};
#endif

static void __device_register(const char *name, int index)
{
    rt_uint32_t flag = 0;
    struct rt_device *device;

    struct usbh_cp210x_static_device *p_dev = g_devices + index;

    memset(p_dev, 0, sizeof(*p_dev));

    device = &(p_dev->parent);

    device->type = RT_Device_Class_Char;
    device->rx_indicate = RT_NULL;
    device->tx_complete = RT_NULL;

#ifdef RT_USING_DEVICE_OPS
    device->ops = &p_this_ops;
#else
    device->init = __init;
    device->open = __open;
    device->close = __close;
    device->read = __read;
    device->write = __write;
    device->control = __control;
#endif
    device->user_data = 0;
    rt_mutex_init(&p_dev->lock, "USBx", 0);

    /* register a character device */
    rt_device_register(device, name, flag);
}

void register_all_ttyusb_devices(void)
{
    __device_register("ttyU0", 0);
    __device_register("ttyU1", 1);
    __device_register("ttyU2", 2);
    __device_register("ttyU3", 3);
}
