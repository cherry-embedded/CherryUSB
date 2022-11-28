#include <rtthread.h>
#include "usbh_core.h"
#include "usbh_rndis.h"
#include "rndis_protocol.h"

/* RT-Thread LWIP ethernet interface */
#include <netif/ethernetif.h>
#include <netdev.h>

/* define the rdnis device state*/
#define RNDIS_BUS_UNINITIALIZED     0
#define RNDIS_BUS_INITIALIZED       1
#define RNDIS_INITIALIZED           2
#define RNDIS_DATA_INITIALIZED      3

#define USB_ETH_MTU               1500 + 14
#define RNDIS_MESSAGE_BUFFER_SIZE 128
#define RNDIS_INFO_BUFFER_OFFSET  20

// #define RESPONSE_AVAILABLE              0x00000001

/* rndis device power off time, unit:ms, 0:power off always */
#ifndef RNDIS_DEV_POWER_OFF_TIME
#define RNDIS_DEV_POWER_OFF_TIME 0
#endif

#define RNDIS_NET_DEV_NAME "u0"
#define MAX_ADDR_LEN       6
/* rndis device keepalive time 5000ms*/
#define RNDIS_DEV_KEEPALIVE_TIMEOUT 5000
/*should be the usb Integer multiple of maximum packet length  N*64*/
#define RNDIS_ETH_BUFFER_LEN (sizeof(struct rndis_packet_msg) + USB_ETH_MTU + 42)

struct rndis_packet_msg
{
    rt_uint32_t MessageType;
    rt_uint32_t MessageLength;
    rt_uint32_t DataOffset;
    rt_uint32_t DataLength;
    rt_uint32_t OOBDataOffset;
    rt_uint32_t OOBDataLength;
    rt_uint32_t NumOOBDataElements;
    rt_uint32_t PerPacketInfoOffset;
    rt_uint32_t PerPacketInfoLength;
    rt_uint32_t VcHandle;
    rt_uint32_t Reserved;
    rt_uint8_t  data[0];
};
typedef struct rndis_packet_msg* rndis_packet_msg_t;

struct rt_rndis_eth {
    /* inherit from ethernet device */
    struct eth_device parent;

    struct usbh_rndis *rndis_class;
    rt_mutex_t rndis_mutex;
    /* interface address info */
    rt_uint8_t dev_addr[MAX_ADDR_LEN];
    rt_uint16_t res;
    rt_uint32_t rndis_speed;
    rt_uint32_t res32;

    rt_uint8_t tx_buffer[RNDIS_ETH_BUFFER_LEN];
    rt_uint8_t rx_bufferA[RNDIS_ETH_BUFFER_LEN];
    rt_uint8_t rx_bufferB[RNDIS_ETH_BUFFER_LEN];
    rt_size_t rx_lengthA;
    rt_size_t rx_lengthB;
    rt_uint8_t *rx_buf_ptr;
    rt_uint32_t frame_debug;
    rt_uint32_t send_packet_counter;
    rt_uint32_t recv_packet_counter;

    rt_uint32_t rndis_state;
    rt_thread_t rndis_recv;
    rt_timer_t keepalive_timer;
};
typedef struct rt_rndis_eth *rt_rndis_eth_t;

void hex_data_print(const char *name, const rt_uint8_t *buf, rt_size_t size)
{
#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
#define WIDTH_SIZE     32

    rt_size_t i, j;
    rt_tick_t tick = 0;
    rt_uint32_t time = 0;

    tick = rt_tick_get();
    time = (tick * 1000) / RT_TICK_PER_SECOND;

    rt_kprintf("%s time=%d.%ds,len = %d\n", name, time / 1000, time % 1000, size);

    for (i = 0; i < size; i += WIDTH_SIZE) {
        rt_kprintf("[HEX] %s: %04X-%04X: ", name, i, i + WIDTH_SIZE);
        for (j = 0; j < WIDTH_SIZE; j++) {
            if (i + j < size) {
                rt_kprintf("%02X ", buf[i + j]);
            } else {
                rt_kprintf("   ");
            }
            if ((j + 1) % 8 == 0) {
                rt_kprintf(" ");
            }
        }
        rt_kprintf("  ");
        for (j = 0; j < WIDTH_SIZE; j++) {
            if (i + j < size) {
                rt_kprintf("%c", __is_print(buf[i + j]) ? buf[i + j] : '.');
            }
        }
        rt_kprintf("\n");
    }
}

#define RNDIS_DEV_DEBUG
#ifdef RNDIS_DEV_DEBUG
#define RNDIS_DEV_PRINTF        \
    rt_kprintf("[RNDIS_DEV] "); \
    rt_kprintf
#else
#define RNDIS_DEV_PRINTF(...)
#endif /* RNDIS_DEBUG */

static struct rt_rndis_eth usbh_rndis_eth_device;

/**
 * This function send the rndis data.
 *
 * @param rndis_class the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
static rt_err_t rt_rndis_msg_data_send(struct usbh_rndis *rndis_class, rt_uint8_t *buffer, int nbytes)
{
    int ret = 0;

    if (rndis_class == RT_NULL) {
        return -RT_ERROR;
    }

    rt_rndis_eth_t info = RT_NULL;

    info = (rt_rndis_eth_t)rndis_class->user_data;

    ret = usbh_rndis_bulk_out_transfer(rndis_class, buffer, nbytes, 5000);
    if (ret != nbytes) {
        rt_kprintf("rndis msg send fial\r\n");
    }
    rt_mutex_take(usbh_rndis_eth_device.rndis_mutex, RT_WAITING_FOREVER);
    if (info->keepalive_timer) {
        rt_timer_start(info->keepalive_timer);
    }
    rt_mutex_release(usbh_rndis_eth_device.rndis_mutex);

    return ret;
}

/**
 * This function recv the rndis data.
 *
 * @param rndis_class the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
static rt_err_t rndis_msg_data_recv(struct usbh_rndis *rndis_class, rt_uint8_t *buffer, int nbytes)
{
    int ret = 0;

    if (rndis_class == RT_NULL) {
        return -RT_ERROR;
    }

    rt_rndis_eth_t info = RT_NULL;

    info = (rt_rndis_eth_t)rndis_class->user_data;

    ret = usbh_rndis_bulk_in_transfer(rndis_class, buffer, nbytes, 3);
    rt_mutex_take(usbh_rndis_eth_device.rndis_mutex, RT_WAITING_FOREVER);
    if (info->keepalive_timer) {
        rt_timer_start(info->keepalive_timer);
    }
    rt_mutex_release(usbh_rndis_eth_device.rndis_mutex);

    return ret;
}

/**
 * This function send the rndis set msg.
 *
 * @param rndis_class the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
static rt_err_t rt_rndis_keepalive_msg(struct usbh_rndis *rndis_class)
{
    return usbh_rndis_keepalive(rndis_class);
}

/**
 * This function will send the bulk data to the usb device instance,
 *
 * @param device the usb device instance.
 * @param type the type of descriptor bRequest.
 * @param buffer the data buffer to save requested data
 * @param nbytes the size of buffer
 *
 * @return the error code, RT_EOK on successfully.
 */
void rt_usbh_rndis_data_recv_entry(void *pdata)
{
    int ret = 0;
    struct usbh_rndis *rndis_class = (struct usbh_rndis *)pdata;
    rt_rndis_eth_t device = RT_NULL;
    rndis_packet_msg_t pmsg = RT_NULL;
    device = (rt_rndis_eth_t)rndis_class->user_data;

    if ((pdata == RT_NULL) || (rndis_class == RT_NULL) ||
        (device == RT_NULL)) {
        return;
    }

    while (1) {
        ret = rndis_msg_data_recv(rndis_class, device->rx_buf_ptr, RNDIS_ETH_BUFFER_LEN);
        if (ret > 0) {
            pmsg = (rndis_packet_msg_t)device->rx_buf_ptr;

            if (device->frame_debug == RT_TRUE) {
                hex_data_print("rndis eth rx", device->rx_buf_ptr, ret);
            }
            if (device->rx_buf_ptr == device->rx_bufferA) {
                if (device->rx_lengthA) {
                    RNDIS_DEV_PRINTF("Rndis deivce rx bufferA overwrite!\n");
                }
                device->rx_lengthA = ret;
                device->rx_buf_ptr = device->rx_bufferB;
            } else {
                if (device->rx_lengthB) {
                    RNDIS_DEV_PRINTF("Rndis deivce rx bufferB overwrite!\n");
                }
                device->rx_lengthB = ret;
                device->rx_buf_ptr = device->rx_bufferA;
            }

            if ((pmsg->MessageType == REMOTE_NDIS_PACKET_MSG) && (pmsg->MessageLength == ret)) {
                device->recv_packet_counter++;
                eth_device_ready((struct eth_device *)device);
            } else {
                RNDIS_DEV_PRINTF("Rndis deivce recv data error!\n");
            }
        }

        if (ret == 0) {
            ret = 0;
            rt_thread_mdelay(10);
        } else {
            ret -= 1;
            device->recv_packet_counter += 0;
            rt_thread_mdelay(10);
        }
    }
}

/**
 * This function power off the rndis device and power up it again.
 *
 * @rndis_class intf the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
static rt_err_t rt_rndis_dev_power(struct usbh_rndis *rndis_class, rt_uint32_t time)
{
    /*power off the rndis device*/
    // rt_usbh_hub_clear_port_feature(intf->device->parent_hub, intf->device->port, PORT_FEAT_POWER);
    // if(time)
    // {
    //     rt_thread_mdelay(time);
    //     /*power up the rndis device */
    //     rt_usbh_hub_set_port_feature(intf->device->parent_hub, intf->device->port, PORT_FEAT_POWER);
    // }

    return RT_EOK;
}

void rt_rndis_dev_keepalive_timeout(void *pdata)
{
    struct usbh_rndis *rndis_class = (struct usbh_rndis *)pdata;
    static rt_uint32_t keepalive_error = 0;

    if (rndis_class == RT_NULL) {
        return;
    }

    rt_rndis_eth_t info = RT_NULL;

    info = (rt_rndis_eth_t)rndis_class->user_data;

    if (RT_EOK == rt_rndis_keepalive_msg(rndis_class)) {
        RNDIS_DEV_PRINTF("rndis dev keepalive success!\n");
        keepalive_error = 0;
        rt_mutex_take(usbh_rndis_eth_device.rndis_mutex, RT_WAITING_FOREVER);
        if (info->keepalive_timer) {
            rt_timer_start(info->keepalive_timer);
        }
        rt_mutex_release(usbh_rndis_eth_device.rndis_mutex);
    } else {
        keepalive_error++;
        RNDIS_DEV_PRINTF("rndis dev keepalive timeout!\n");
        if (keepalive_error > 3) {
            keepalive_error = 0;
            rt_rndis_dev_power(rndis_class, RNDIS_DEV_POWER_OFF_TIME);
            info->rndis_state = RNDIS_BUS_INITIALIZED;
        }
    }
}

/**
 * This function will run rndis driver when usb disk is detected.
 *
 * @param rndis_class the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
rt_err_t rt_rndis_run(struct usbh_rndis *rndis_class, struct rndis_dev_info *dev_info)
{
    rt_err_t ret = 0;
    // urndis_t rndis = RT_NULL;
    rt_uint8_t *recv_buf = RT_NULL;
    rt_uint32_t recv_len = 256;
    rt_uint32_t *psupport_oid_list = RT_NULL;
    rt_uint32_t *poid = RT_NULL;
    rt_uint32_t *pquery_rlt = RT_NULL;
    rt_uint32_t i = 0, j = 0;
    rt_uint32_t oid_len = 0;
    struct netdev *netdev = RT_NULL;

    /* check parameter */
    RT_ASSERT(rndis_class != RT_NULL);

    /*The host is configured to send and receive any of the RNDIS control messages for suitably
     configuring or querying the device, to receive status indications from the device,
     to reset the device, or to tear down the data and control channels*/
    usbh_rndis_eth_device.rndis_state = RNDIS_INITIALIZED;

    usbh_rndis_eth_device.keepalive_timer = rt_timer_create("keeplive", rt_rndis_dev_keepalive_timeout,
                                                            rndis_class,
                                                            RT_TICK_PER_SECOND * RNDIS_DEV_KEEPALIVE_TIMEOUT / 1000,
                                                            RT_TIMER_FLAG_ONE_SHOT | RT_TIMER_FLAG_SOFT_TIMER);

    if (usbh_rndis_eth_device.keepalive_timer == RT_NULL) {
        ret = -RT_ENOMEM;
        goto __exit;
    }

    rndis_class->user_data = (struct rt_device *)&usbh_rndis_eth_device;

    usbh_rndis_eth_device.rndis_recv = rt_thread_create("rndis",
                                                        (void (*)(void *parameter))rt_usbh_rndis_data_recv_entry,
                                                        rndis_class,
                                                        1024 + 512,
                                                        15,
                                                        20);

    if (usbh_rndis_eth_device.rndis_recv == RT_NULL) {
        ret = -RT_ENOMEM;
        goto __exit;
    }

    /*the LINK SPEED is 100Mbps*/
    usbh_rndis_eth_device.rndis_speed = dev_info->rndis_speed;

    eth_device_linkchange(&usbh_rndis_eth_device.parent, dev_info->up);

    for (j = 0; j < MAX_ADDR_LEN; j++) {
        usbh_rndis_eth_device.dev_addr[j] = dev_info->dev_addr[j];
    }

    /* update the mac addr to netif interface */
    rt_device_control((rt_device_t)&usbh_rndis_eth_device.parent, NIOCTL_GADDR,
                      usbh_rndis_eth_device.parent.netif->hwaddr);

    netdev = netdev_get_by_name(RNDIS_NET_DEV_NAME);
    if (netdev) {
        rt_memcpy(netdev->hwaddr, recv_buf, MAX_ADDR_LEN);
    }

__exit:
    if (ret == RT_EOK) {
        /*This state is entered after the host has received REMOTE_NDIS_SET_CMPLT
        messages from the device in response to the REMOTE_NDIS_SET_MSG
        that it had sent earlier to the device with all the OIDs required to configure the device for data transfer.
        When the host is in this state, apart from the control messages,
        it can exchange REMOTE_NDIS_PACKET_MSG messages for network data transfer with the device on the data channel*/
        usbh_rndis_eth_device.rndis_state = RNDIS_DATA_INITIALIZED;
        rt_thread_startup(usbh_rndis_eth_device.rndis_recv);
        RNDIS_DEV_PRINTF("rndis dev start!\n");
        usbh_rndis_eth_device.rndis_class = rndis_class;

    } else {
        RNDIS_DEV_PRINTF("rndis dev faile!\n");
        /*rndis device run error, power off the device, try it agin*/
        rt_rndis_dev_power(rndis_class, RNDIS_DEV_POWER_OFF_TIME);
    }

    return ret;
}

rt_err_t rt_rndis_stop(struct usbh_rndis *rndis_class)
{
    rt_rndis_eth_t info = RT_NULL;

    info = (rt_rndis_eth_t)rndis_class->user_data;

    if (info->rndis_recv) {
        rt_thread_delete(info->rndis_recv);
        info->rndis_recv = RT_NULL;
    }
    eth_device_linkchange(&usbh_rndis_eth_device.parent, RT_FALSE);
    usbh_rndis_eth_device.rndis_class = RT_NULL;

    /*disable the other thread etx call the rt_timer_start(rndis->keepalive_timer) cause the RT_ASSERT(rt_object_get_type(&timer->parent) == RT_Object_Class_Timer)*/
    rt_mutex_take(usbh_rndis_eth_device.rndis_mutex, RT_WAITING_FOREVER);
    if (info->keepalive_timer) {
        rt_timer_stop(info->keepalive_timer);
        rt_timer_delete(info->keepalive_timer);
        info->keepalive_timer = RT_NULL;
    }
    rt_mutex_release(usbh_rndis_eth_device.rndis_mutex);

    info->rndis_state = RNDIS_BUS_UNINITIALIZED;

    RNDIS_DEV_PRINTF("rndis dev stop!\n");
    return RT_EOK;
}

/**
 * This function rndis eth device.
 *
 * @param intf the usb interface instance.
 *
 * @return the error code, RT_EOK on successfully.
 */
#ifdef RT_USING_LWIP
/* initialize the interface */
static rt_err_t rt_rndis_eth_init(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t rt_rndis_eth_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_rndis_eth_close(rt_device_t dev)
{
    return RT_EOK;
}

static rt_size_t rt_rndis_eth_read(rt_device_t dev, rt_off_t pos, void *buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}

static rt_size_t rt_rndis_eth_write(rt_device_t dev, rt_off_t pos, const void *buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}
static rt_err_t rt_rndis_eth_control(rt_device_t dev, int cmd, void *args)
{
    rt_rndis_eth_t rndis_eth_dev = (rt_rndis_eth_t)dev;
    switch (cmd) {
        case NIOCTL_GADDR:
            /* get mac address */
            if (args) {
                rt_memcpy(args, rndis_eth_dev->dev_addr, MAX_ADDR_LEN);
            } else {
                return -RT_ERROR;
            }
            break;

        case NIOTCTL_GTXCOUNTER:
            if (args) {
                *(rt_uint32_t *)args = rndis_eth_dev->send_packet_counter;
            } else {
                return -RT_ERROR;
            }
            break;

        case NIOTCTL_GRXCOUNTER:
            if (args) {
                *(rt_uint32_t *)args = rndis_eth_dev->recv_packet_counter;
            } else {
                return -RT_ERROR;
            }
            break;
        default:
            break;
    }

    return RT_EOK;
}

/* ethernet device interface */

/* reception packet. */
struct pbuf *rt_rndis_eth_rx(rt_device_t dev)
{
    struct pbuf *p = RT_NULL;
    rt_uint32_t offset = 0;
    rt_rndis_eth_t device = (rt_rndis_eth_t)dev;
    rt_uint32_t recv_len = 0;

    rndis_packet_msg_t pmsg = RT_NULL;

    if (device->rx_buf_ptr == device->rx_bufferA) {
        pmsg = (rndis_packet_msg_t)device->rx_bufferB;
        recv_len = device->rx_lengthB;
    } else {
        pmsg = (rndis_packet_msg_t)device->rx_bufferA;
        recv_len = device->rx_lengthA;
    }

    if ((recv_len == 0) || (pmsg->DataLength == 0)) {
        return RT_NULL;
    }

    /* allocate buffer */
    p = pbuf_alloc(PBUF_LINK, pmsg->DataLength, PBUF_RAM);
    if (p != RT_NULL) {
        struct pbuf *q;

        for (q = p; q != RT_NULL; q = q->next) {
            /* Copy the received frame into buffer from memory pointed by the current ETHERNET DMA Rx descriptor */
            rt_memcpy(q->payload,
                      (rt_uint8_t *)((pmsg->data) + offset),
                      q->len);
            offset += q->len;
        }
    }

    if (device->rx_buf_ptr == device->rx_bufferA) {
        device->rx_lengthB = 0;
    } else {
        device->rx_lengthA = 0;
    }

    return p;
}

/* transmit packet. */
rt_err_t rt_rndis_eth_tx(rt_device_t dev, struct pbuf *p)
{
    struct pbuf *q;
    rt_uint8_t *buffer = RT_NULL;
    rt_err_t result = RT_EOK;
    rt_rndis_eth_t device = (rt_rndis_eth_t)dev;
    rndis_packet_msg_t msg = RT_NULL;

    if (!device->parent.link_status) {
        RNDIS_DEV_PRINTF("linkdown, drop pkg\r\n");
        return RT_EOK;
    }

    RT_ASSERT((p->tot_len + sizeof(struct rndis_packet_msg)) < sizeof(device->tx_buffer));
    if (p->tot_len > sizeof(device->tx_buffer)) {
        RNDIS_DEV_PRINTF("RNDIS MTU is:%d, but the send packet size is %d\r\n",
                         sizeof(device->tx_buffer), p->tot_len);
        p->tot_len = sizeof(device->tx_buffer);
    }

    msg = (rndis_packet_msg_t)&device->tx_buffer;
    msg->MessageType = REMOTE_NDIS_PACKET_MSG;
    msg->DataOffset = sizeof(struct rndis_packet_msg) - 8;
    msg->DataLength = p->tot_len;
    msg->OOBDataLength = 0;
    msg->OOBDataOffset = 0;
    msg->NumOOBDataElements = 0;
    msg->PerPacketInfoOffset = 0;
    msg->PerPacketInfoLength = 0;
    msg->VcHandle = 0;
    msg->Reserved = 0;
    msg->MessageLength = sizeof(struct rndis_packet_msg) + p->tot_len;

    buffer = msg->data;
    for (q = p; q != NULL; q = q->next) {
        rt_memcpy(buffer, q->payload, q->len);
        buffer += q->len;
    }

    /* send */
    if ((msg->MessageLength & 0x3F) == 0) {
        /* pad a dummy. */
        msg->MessageLength += 1;
    }

    if (device->frame_debug == RT_TRUE) {
        hex_data_print("rndis eth tx", (rt_uint8_t *)msg, msg->MessageLength);
    }
    result = rt_rndis_msg_data_send(device->rndis_class, (rt_uint8_t *)msg, msg->MessageLength);
    device->send_packet_counter++;

    return result;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops rndis_device_ops = {
    rt_rndis_eth_init,
    rt_rndis_eth_open,
    rt_rndis_eth_close,
    rt_rndis_eth_read,
    rt_rndis_eth_write,
    rt_rndis_eth_control
}
#endif
#endif

int usbh_rndis_eth_device_init(void)
{
    /* OUI 00-00-00, only for test. */
    usbh_rndis_eth_device.dev_addr[0] = 0xFF;
    usbh_rndis_eth_device.dev_addr[1] = 0xFF;
    usbh_rndis_eth_device.dev_addr[2] = 0xFF;
    /* generate random MAC. */
    usbh_rndis_eth_device.dev_addr[3] = 0xFF;
    usbh_rndis_eth_device.dev_addr[4] = 0xFF;
    usbh_rndis_eth_device.dev_addr[5] = 0xFF;

    usbh_rndis_eth_device.rndis_mutex = rt_mutex_create("rndis", RT_IPC_FLAG_PRIO);

    if (usbh_rndis_eth_device.rndis_mutex == RT_NULL) {
        RNDIS_DEV_PRINTF("Rndis mutex creat faile!\r\n");
    }

#ifdef RT_USING_DEVICE_OPS
    usbh_rndis_eth_device.parent.parent.ops = &rndis_device_ops;
#else
    usbh_rndis_eth_device.parent.parent.init = rt_rndis_eth_init;
    usbh_rndis_eth_device.parent.parent.open = rt_rndis_eth_open;
    usbh_rndis_eth_device.parent.parent.close = rt_rndis_eth_close;
    usbh_rndis_eth_device.parent.parent.read = rt_rndis_eth_read;
    usbh_rndis_eth_device.parent.parent.write = rt_rndis_eth_write;
    usbh_rndis_eth_device.parent.parent.control = rt_rndis_eth_control;
#endif
    usbh_rndis_eth_device.parent.parent.user_data = RT_NULL;

    usbh_rndis_eth_device.parent.eth_rx = rt_rndis_eth_rx;
    usbh_rndis_eth_device.parent.eth_tx = rt_rndis_eth_tx;

    /* register eth device */
    usbh_rndis_eth_device.rx_lengthA = 0;
    usbh_rndis_eth_device.rx_lengthB = 0;
    usbh_rndis_eth_device.rx_buf_ptr = usbh_rndis_eth_device.rx_bufferA;
    usbh_rndis_eth_device.frame_debug = RT_FALSE;

    usbh_rndis_eth_device.send_packet_counter = 0;
    usbh_rndis_eth_device.recv_packet_counter = 0;

    eth_device_init(&usbh_rndis_eth_device.parent, RNDIS_NET_DEV_NAME);

    eth_device_linkchange(&usbh_rndis_eth_device.parent, RT_FALSE);
    return RT_EOK;
}
INIT_APP_EXPORT(usbh_rndis_eth_device_init);

/*********************************************************************************************************
** Function name        eth_rndis_frame_debug()
** Descriptions:        rndis frame print
** input parameters
** output parameters     None
** Returned value:      RT_EOK or RT_ERROR
*********************************************************************************************************/
static void eth_rndis_frame_debug(int argc, char **argv)
{
    if (argc != 2) {
        rt_kprintf("Please check the command you enter, it like this: rndis_debug on/off!\n");
    } else {
        if (rt_strcmp(argv[1], "on") == 0) {
            usbh_rndis_eth_device.frame_debug = RT_TRUE;
        } else {
            usbh_rndis_eth_device.frame_debug = RT_FALSE;
        }
    }
}

#ifdef FINSH_USING_MSH
#include <finsh.h>
MSH_CMD_EXPORT_ALIAS(eth_rndis_frame_debug, rndis_debug, set eth rndis frame print);
#endif /* FINSH_USING_MSH */