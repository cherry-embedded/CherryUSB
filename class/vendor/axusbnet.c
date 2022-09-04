/*
 * Copyright (c) 2022, aozima
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/*
 * Change Logs
 * Date           Author       Notes
 * 2022-04-17     aozima       the first version for CherryUSB.
 */

#include <string.h>

#include "usbh_core.h"
#include "axusbnet.h"

static const char *DEV_FORMAT = "/dev/u%d";

#define USE_RTTHREAD    (1)
// #define RX_DUMP
// #define TX_DUMP
// #define DUMP_RAW

#if USE_RTTHREAD
#include <rtthread.h>

#include <netif/ethernetif.h>
#include <netdev.h>
#endif /* USE_RTTHREAD */

#define MAX_ADDR_LEN            6
#define ETH_ALEN                MAX_ADDR_LEN
#define mdelay                  rt_thread_delay
#define msleep                  rt_thread_delay
#define deverr(dev, fmt, ...)   USB_LOG_ERR(fmt, ##__VA_ARGS__)
#define cpu_to_le16(a)          (a)
#define le32_to_cpus(a)         (a)

/* Generic MII registers. */
#define MII_BMCR		0x00	/* Basic mode control register */
#define MII_BMSR		0x01	/* Basic mode status register  */
#define MII_PHYSID1		0x02	/* PHYS ID 1                   */
#define MII_PHYSID2		0x03	/* PHYS ID 2                   */
#define MII_ADVERTISE		0x04	/* Advertisement control reg   */
#define MII_LPA			0x05	/* Link partner ability reg    */
#define MII_EXPANSION		0x06	/* Expansion register          */
#define MII_CTRL1000		0x09	/* 1000BASE-T control          */
#define MII_STAT1000		0x0a	/* 1000BASE-T status           */
#define	MII_MMD_CTRL		0x0d	/* MMD Access Control Register */
#define	MII_MMD_DATA		0x0e	/* MMD Access Data Register */
#define MII_ESTATUS		0x0f	/* Extended Status             */
#define MII_DCOUNTER		0x12	/* Disconnect counter          */
#define MII_FCSCOUNTER		0x13	/* False carrier counter       */
#define MII_NWAYTEST		0x14	/* N-way auto-neg test reg     */
#define MII_RERRCOUNTER		0x15	/* Receive error counter       */
#define MII_SREVISION		0x16	/* Silicon revision            */
#define MII_RESV1		0x17	/* Reserved...                 */
#define MII_LBRERROR		0x18	/* Lpback, rx, bypass error    */
#define MII_PHYADDR		0x19	/* PHY address                 */
#define MII_RESV2		0x1a	/* Reserved...                 */
#define MII_TPISTATUS		0x1b	/* TPI status for 10mbps       */
#define MII_NCONFIG		0x1c	/* Network interface config    */

/* Basic mode control register. */
#define BMCR_RESV		0x003f	/* Unused...                   */
#define BMCR_SPEED1000		0x0040	/* MSB of Speed (1000)         */
#define BMCR_CTST		0x0080	/* Collision test              */
#define BMCR_FULLDPLX		0x0100	/* Full duplex                 */
#define BMCR_ANRESTART		0x0200	/* Auto negotiation restart    */
#define BMCR_ISOLATE		0x0400	/* Isolate data paths from MII */
#define BMCR_PDOWN		0x0800	/* Enable low power state      */
#define BMCR_ANENABLE		0x1000	/* Enable auto negotiation     */
#define BMCR_SPEED100		0x2000	/* Select 100Mbps              */
#define BMCR_LOOPBACK		0x4000	/* TXD loopback bits           */
#define BMCR_RESET		0x8000	/* Reset to default state      */
#define BMCR_SPEED10		0x0000	/* Select 10Mbps               */

/* Advertisement control register. */
#define ADVERTISE_SLCT		0x001f	/* Selector bits               */
#define ADVERTISE_CSMA		0x0001	/* Only selector supported     */
#define ADVERTISE_10HALF	0x0020	/* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL	0x0020	/* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL	0x0040	/* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF	0x0040	/* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF	0x0080	/* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE	0x0080	/* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL	0x0100	/* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM	0x0100	/* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4	0x0200	/* Try for 100mbps 4k packets  */
#define ADVERTISE_PAUSE_CAP	0x0400	/* Try for pause               */
#define ADVERTISE_PAUSE_ASYM	0x0800	/* Try for asymetric pause     */
#define ADVERTISE_RESV		0x1000	/* Unused...                   */
#define ADVERTISE_RFAULT	0x2000	/* Say we can detect faults    */
#define ADVERTISE_LPACK		0x4000	/* Ack link partners response  */
#define ADVERTISE_NPAGE		0x8000	/* Next page bit               */

#define ADVERTISE_FULL		(ADVERTISE_100FULL | ADVERTISE_10FULL | \
				  ADVERTISE_CSMA)
#define ADVERTISE_ALL		(ADVERTISE_10HALF | ADVERTISE_10FULL | \
				  ADVERTISE_100HALF | ADVERTISE_100FULL)

struct mii_if_info {
	int phy_id;
};

struct usbnet
{
#if USE_RTTHREAD
    /* inherit from ethernet device */
    struct eth_device parent;
#endif /* USE_RTTHREAD */

    struct usbh_axusbnet *class;

    uint8_t   dev_addr[MAX_ADDR_LEN];

    uint8_t internalphy:1;
	uint8_t PhySelect:1;
	uint8_t OperationMode:1;

	struct mii_if_info	mii;
};
typedef struct usbnet * usbnet_t;
static struct usbnet usbh_axusbnet_eth_device;

#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
static void dump_hex(const void *ptr, uint32_t buflen)
{
    unsigned char *buf = (unsigned char*)ptr;
    int i, j;

    for (i=0; i<buflen; i+=16)
    {
        printf("%08X:", i);

        for (j=0; j<16; j++)
            if (i+j < buflen)
            {
                if ((j % 8) == 0) {
                    printf("  ");
                }

                printf("%02X ", buf[i+j]);
            }
            else
                printf("   ");
        printf(" ");

        for (j=0; j<16; j++)
            if (i+j < buflen)
                printf("%c", __is_print(buf[i+j]) ? buf[i+j] : '.');
        printf("\n");
    }
}

#if defined(RX_DUMP) ||  defined(TX_DUMP)
static void packet_dump(const char * msg, const struct pbuf* p)
{
    rt_uint8_t header[6 + 6 + 2];
    rt_uint16_t type;

    pbuf_copy_partial(p, header, sizeof(header), 0);
    type = (header[12] << 8) | header[13];

    rt_kprintf("%02X:%02X:%02X:%02X:%02X:%02X <== %02X:%02X:%02X:%02X:%02X:%02X ",
               header[0], header[1], header[2], header[3], header[4], header[5],
               header[6], header[7], header[8], header[9], header[10], header[11]);

    switch (type)
    {
    case 0x0800:
        rt_kprintf("IPv4. ");
        break;

    case 0x0806:
        rt_kprintf("ARP.  ");
        break;

    case 0x86DD:
        rt_kprintf("IPv6. ");
        break;

    default:
        rt_kprintf("%04X. ", type);
        break;
    }

    rt_kprintf("%s %d byte. \n", msg, p->tot_len);
#ifdef DUMP_RAW
    const struct pbuf* q;
    rt_uint32_t i,j;
    rt_uint8_t *ptr;

    // rt_kprintf("%s %d byte\n", msg, p->tot_len);

    i=0;
    for(q=p; q != RT_NULL; q= q->next)
    {
        ptr = q->payload;

        for(j=0; j<q->len; j++)
        {
            if( (i%8) == 0 )
            {
                rt_kprintf("  ");
            }
            if( (i%16) == 0 )
            {
                rt_kprintf("\r\n");
            }
            rt_kprintf("%02X ", *ptr);

            i++;
            ptr++;
        }
    }

    rt_kprintf("\n\n");
#endif /* DUMP_RAW */
}
#else
#define packet_dump(...)
#endif /* dump */

static int ax8817x_read_cmd(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			    u16 size, void *data)
{
    int ret = 0;
    struct usbh_hubport *hport = dev->class->hport;
    struct usb_setup_packet *setup = hport->setup;
    setup->bmRequestType = USB_REQUEST_DIR_IN | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = cmd;
    setup->wValue = value;
    setup->wIndex = index;
    setup->wLength = size;

    ret = usbh_control_transfer(hport->ep0, setup, (uint8_t *)data);
    if (ret != 0) {
        USB_LOG_ERR("%s cmd=%d ret: %d\r\n", __FUNCTION__, cmd, ret);
        return ret;
    }

_exit:

    return ret;
}

static int ax8817x_write_cmd(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			     u16 size, void *data)
{
    int ret = 0;
    struct usbh_hubport *hport = dev->class->hport;
    struct usb_setup_packet *setup = hport->setup;
    setup->bmRequestType = USB_REQUEST_DIR_OUT | USB_REQUEST_VENDOR | USB_REQUEST_RECIPIENT_DEVICE;
    setup->bRequest = cmd;
    setup->wValue = value;
    setup->wIndex = index;
    setup->wLength = size;

    ret = usbh_control_transfer(hport->ep0, setup, (uint8_t *)data);
    if (ret != 0) {
        USB_LOG_ERR("%s cmd=%d ret: %d\r\n", __FUNCTION__, cmd, ret);
        return ret;
    }

_exit:

    return ret;
}

static int ax8817x_mdio_read(struct usbnet *dev, int phy_id, int loc)
{
    // struct usbnet *dev = netdev_priv(netdev);
    u16 res, ret;
    u8 smsr;
    int i = 0;

    // res = kmalloc(2, GFP_ATOMIC);
    // if (!res)
    //     return 0;

    do {
        ax8817x_write_cmd(dev, AX_CMD_SET_SW_MII, 0, 0, 0, NULL);

        msleep(1);

        // smsr = (u8 *)&ret;
        ax8817x_read_cmd(dev, AX_CMD_READ_STATMNGSTS_REG, 0, 0, 1, &smsr);
    } while (!(smsr & AX_HOST_EN) && (i++ < 30));

    ax8817x_read_cmd(dev, AX_CMD_READ_MII_REG, phy_id, (uint16_t)loc, 2, &res);
    ax8817x_write_cmd(dev, AX_CMD_SET_HW_MII, 0, 0, 0, NULL);

    // ret = *res & 0xffff;
    // kfree(res);

    return res;
}

/* same as above, but converts resulting value to cpu byte order */
static int ax8817x_mdio_read_le(struct usbnet *netdev, int phy_id, int loc)
{
	return (ax8817x_mdio_read(netdev, phy_id, loc));
}

static void
ax8817x_mdio_write(struct usbnet *dev, int phy_id, int loc, int val)
{
	// struct usbnet *dev = netdev_priv(netdev);
	u16 res;
	u8 smsr;
	int i = 0;

	// res = kmalloc(2, GFP_ATOMIC);
	// if (!res)
	// 	return;
	// smsr = (u8 *) res;

	do {
		ax8817x_write_cmd(dev, AX_CMD_SET_SW_MII, 0, 0, 0, NULL);

		msleep(1);

		ax8817x_read_cmd(dev, AX_CMD_READ_STATMNGSTS_REG, 0, 0, 1, &smsr);
	} while (!(smsr & AX_HOST_EN) && (i++ < 30));

	// *res = val;
    res = val;

    ax8817x_write_cmd(dev, AX_CMD_WRITE_MII_REG, phy_id, (uint16_t)loc, 2, &res);
    ax8817x_write_cmd(dev, AX_CMD_SET_HW_MII, 0, 0, 0, NULL);

	// kfree(res);
}

static void
ax88772b_mdio_write(struct usbnet *dev, int phy_id, int loc, int val)
{
	// struct usbnet *dev = netdev_priv(netdev);
	u16 res = val;

	// res = kmalloc(2, GFP_ATOMIC);
	// if (!res)
	// 	return;
	// *res = val;

	ax8817x_write_cmd(dev, AX_CMD_SET_SW_MII, 0, 0, 0, NULL);
    ax8817x_write_cmd(dev, AX_CMD_WRITE_MII_REG, phy_id, (uint16_t)loc, 2, &res);

    if (loc == MII_ADVERTISE) {
		res = cpu_to_le16(BMCR_ANENABLE | BMCR_ANRESTART);
		ax8817x_write_cmd(dev, AX_CMD_WRITE_MII_REG, phy_id, (uint16_t)MII_BMCR, 2, &res);
	}

	ax8817x_write_cmd(dev, AX_CMD_SET_HW_MII, 0, 0, 0, NULL);

	// kfree(res);
}

/* same as above, but converts new value to le16 byte order before writing */
static void
ax8817x_mdio_write_le(struct usbnet *netdev, int phy_id, int loc, int val)
{
	ax8817x_mdio_write(netdev, phy_id, loc, cpu_to_le16(val));
}

static int access_eeprom_mac(struct usbnet *dev, u8 *buf, u8 offset, bool wflag)
{
    int ret = 0, i;
    u16 *tmp = (u16 *)buf;

    if (wflag) {
        ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_EEPROM_EN,
                                0, 0, 0, NULL);
        if (ret < 0)
            return ret;

        mdelay(15);
    }

    for (i = 0; i < (ETH_ALEN >> 1); i++) {
        if (wflag) {
            // u16 wd = cpu_to_le16(*(tmp + i));
            u16 wd = (*(tmp + i));
            ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_EEPROM, offset + i,
                                    wd, 0, NULL);
            if (ret < 0)
                break;

            mdelay(15);
        } else {
            ret = ax8817x_read_cmd(dev, AX_CMD_READ_EEPROM,
                                   offset + i, 0, 2, tmp + i);
            if (ret < 0)
                break;
        }
    }

    if (!wflag) {
        if (ret < 0) {
// #if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 34)
//             netdev_dbg(dev->net, "Failed to read MAC address from EEPROM: %d\n", ret);
// #else
//             devdbg(dev, "Failed to read MAC address from EEPROM: %d\n", ret);
// #endif
            USB_LOG_ERR("Failed to read MAC address from EEPROM: %d\n", ret);
            return ret;
        }
        // memcpy(dev->net->dev_addr, buf, ETH_ALEN);
    } else {
        ax8817x_write_cmd(dev, AX_CMD_WRITE_EEPROM_DIS,
                          0, 0, 0, NULL);
        if (ret < 0)
            return ret;

        /* reload eeprom data */
        ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_GPIOS,
                                AXGPIOS_RSE, 0, 0, NULL);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int ax88772a_phy_powerup(struct usbnet *dev)
{
	int ret;
	/* set the embedded Ethernet PHY in power-down state */
	ret = ax8817x_write_cmd(dev, AX_CMD_SW_RESET,
				AX_SWRESET_IPPD | AX_SWRESET_IPRL, 0, 0, NULL);
	if (ret < 0) {
		deverr(dev, "Failed to power down PHY: %d", ret);
		return ret;
	}

	msleep(10);

	/* set the embedded Ethernet PHY in power-up state */
	ret = ax8817x_write_cmd(dev, AX_CMD_SW_RESET, AX_SWRESET_IPRL,
				0, 0, NULL);
	if (ret < 0) {
		deverr(dev, "Failed to reset PHY: %d", ret);
		return ret;
	}

	msleep(600);

	/* set the embedded Ethernet PHY in reset state */
	ret = ax8817x_write_cmd(dev, AX_CMD_SW_RESET, AX_SWRESET_CLEAR,
				0, 0, NULL);
	if (ret < 0) {
		deverr(dev, "Failed to power up PHY: %d", ret);
		return ret;
	}

	/* set the embedded Ethernet PHY in power-up state */
	ret = ax8817x_write_cmd(dev, AX_CMD_SW_RESET, AX_SWRESET_IPRL,
				0, 0, NULL);
	if (ret < 0) {
		deverr(dev, "Failed to reset PHY: %d", ret);
		return ret;
	}

	return 0;
}

static int ax88772b_reset(struct usbnet *dev)
{
	int ret;

	ret = ax88772a_phy_powerup(dev);
	if (ret < 0)
		return ret;

	/* Set the MAC address */
    ret = ax8817x_write_cmd(dev, AX88772_CMD_WRITE_NODE_ID,
                            0, 0, ETH_ALEN, dev->dev_addr);
    if (ret < 0) {
        deverr(dev, "set MAC address failed: %d", ret);
    }

    /* stop MAC operation */
	ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_RX_CTL, AX_RX_CTL_STOP,
				0, 0, NULL);
	if (ret < 0){
		deverr(dev, "Reset RX_CTL failed: %d", ret);
    }

	ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_MEDIUM_MODE,
				AX88772_MEDIUM_DEFAULT, 0, 0,
				NULL);
	if (ret < 0){
		deverr(dev, "Write medium mode register: %d", ret);
    }

	return ret;
}

static int ax8817x_get_mac(struct usbnet *dev, u8 *buf)
{
    int ret, i;

    ret = access_eeprom_mac(dev, buf, 0x04, 0);
    if (ret < 0)
        goto out;

    // if (ax8817x_check_ether_addr(dev)) {
    //     ret = access_eeprom_mac(dev, dev->net->dev_addr, 0x04, 1);
    //     if (ret < 0) {
    //         deverr(dev, "Failed to write MAC to EEPROM: %d", ret);
    //         goto out;
    //     }

    //     msleep(5);

    //     ret = ax8817x_read_cmd(dev, AX88772_CMD_READ_NODE_ID,
    //                            0, 0, ETH_ALEN, buf);
    //     if (ret < 0) {
    //         deverr(dev, "Failed to read MAC address: %d", ret);
    //         goto out;
    //     }

    //     for (i = 0; i < ETH_ALEN; i++)
    //         if (*(dev->net->dev_addr + i) != *((u8 *)buf + i)) {
    //             devwarn(dev, "Found invalid EEPROM part or non-EEPROM");
    //             break;
    //         }
    // }

    // memcpy(dev->net->perm_addr, dev->net->dev_addr, ETH_ALEN);

    // /* Set the MAC address */
    // ax8817x_write_cmd(dev, AX88772_CMD_WRITE_NODE_ID, 0, 0,
    //                   ETH_ALEN, dev->net->dev_addr);

    // if (ret < 0) {
    //     deverr(dev, "Failed to write MAC address: %d", ret);
    //     goto out;
    // }

    return 0;
out:
    return ret;
}

#if USE_RTTHREAD
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

static rt_size_t rt_rndis_eth_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}

static rt_size_t rt_rndis_eth_write (rt_device_t dev, rt_off_t pos, const void* buffer, rt_size_t size)
{
    rt_set_errno(-RT_ENOSYS);
    return 0;
}
static rt_err_t rt_rndis_eth_control(rt_device_t dev, int cmd, void *args)
{
    usbnet_t rndis_eth_dev = (usbnet_t)dev;

    USB_LOG_INFO("%s L%d\r\n", __FUNCTION__, __LINE__);
    switch(cmd)
    {
    case NIOCTL_GADDR:
        /* get mac address */
        if(args)
        {
            USB_LOG_INFO("%s L%d NIOCTL_GADDR\r\n", __FUNCTION__, __LINE__);
            rt_memcpy(args, rndis_eth_dev->dev_addr, MAX_ADDR_LEN);
        }
        else
        {
            return -RT_ERROR;
        }
        break;
    default :
        break;
    }

    return RT_EOK;
}

/* reception packet. */
static struct pbuf *rt_rndis_eth_rx(rt_device_t dev)
{
    struct pbuf* p = RT_NULL;

    // USB_LOG_INFO("%s L%d\r\n", __FUNCTION__, __LINE__);

    return p;
}

/* transmit packet. */
static rt_err_t rt_rndis_eth_tx(rt_device_t dev, struct pbuf* p)
{
    int ret = 0;
    rt_err_t result = RT_EOK;
    uint8_t *tmp_buf = RT_NULL;
    usbnet_t rndis_eth = (usbnet_t)dev;
    struct usbh_axusbnet *class = rndis_eth->class;
    rt_tick_t tick_start, tick_end;
    uint8_t int_notify_buf[8];

#ifdef TX_DUMP
    packet_dump("TX", p);
#endif /* TX_DUMP */

    tmp_buf = (uint8_t *)rt_malloc(16 + p->tot_len );
    if (!tmp_buf) {
        USB_LOG_INFO("[%s L%d], no memory for pbuf, len=%d.", __FUNCTION__, __LINE__, p->tot_len);
        goto _exit;
    }

    uint32_t slen = p->tot_len;

    uint32_t head = slen;
    head = ((head ^ 0x0000ffff) << 16) + (head);

    tmp_buf[0] = head & 0xFF;
    tmp_buf[1] = (head >> 8) & 0xFF;
    tmp_buf[2] = (head >> 16) & 0xFF;
    tmp_buf[3] = (head >> 24) & 0xFF;
    slen += 4;

    int padlen = ((p->tot_len + 4) % 512) ? 0 : 4;
    if (padlen) {
        tmp_buf[4 + slen + 0] = 0x00;
        tmp_buf[4 + slen + 1] = 0x00;
        tmp_buf[4 + slen + 2] = 0xFF;
        tmp_buf[4 + slen + 3] = 0xFF;
        slen += 4;
    }

    pbuf_copy_partial(p, tmp_buf + 4, p->tot_len, 0);

    tick_start = rt_tick_get();
    ret = usbh_ep_bulk_transfer(class->bulkout, tmp_buf, slen, 500);
    if (ret < 0) {
        result = -RT_EIO;
        USB_LOG_ERR("%s L%d send over ret:%d\r\n", __FUNCTION__, __LINE__, ret);
        goto _exit;
    }
    tick_end = rt_tick_get();

_exit:
    if(tmp_buf)
    {
        rt_free(tmp_buf);
    }

    return result;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops rndis_device_ops =
{
    rt_rndis_eth_init,
    rt_rndis_eth_open,
    rt_rndis_eth_close,
    rt_rndis_eth_read,
    rt_rndis_eth_write,
    rt_rndis_eth_control
}
#endif /* RT_USING_DEVICE_OPS */
#endif /* USE_RTTHREAD */

static void rt_thread_axusbnet_entry(void *parameter)
{
    int ret;
    struct usbh_hubport *hport;
    uint8_t intf;
    uint8_t buf[2+8];

    USB_LOG_INFO("%s L%d\r\n", __FUNCTION__, __LINE__);
    rt_thread_delay(200);
    USB_LOG_INFO("%s L%d\r\n\r\n\r\n\r\n", __FUNCTION__, __LINE__);

    const char *dname = "/dev/u0";
    struct usbh_axusbnet *class = (struct usbh_axusbnet *)usbh_find_class_instance(dname);
    if (class == NULL) {
        USB_LOG_ERR("do not find %s\r\n", dname);
        return;
    }
    USB_LOG_INFO("axusbnet=%p\r\n", dname);

    usbh_axusbnet_eth_device.class = class;

    struct usbnet *dev = &usbh_axusbnet_eth_device;

	ret = ax8817x_read_cmd(dev, AX_CMD_SW_PHY_STATUS,
			       0, 0, 1, buf);
    if (ret < 0) {
        USB_LOG_ERR("AX_CMD_SW_PHY_STATUS ret=%d\r\n", ret);
        return;
    }
    u8 tempphyselect = buf[0];
    if (tempphyselect == AX_PHYSEL_SSRMII) {
        USB_LOG_ERR("%s L%d AX_PHYSEL_SSRMII\r\n", __FUNCTION__, __LINE__);
        dev->internalphy = false;
        return;
        // dev->OperationMode = OPERATION_MAC_MODE;
        // dev->PhySelect = 0x00;
    } else if (tempphyselect == AX_PHYSEL_SSRRMII) {
        USB_LOG_ERR("%s L%d AX_PHYSEL_SSRRMII\r\n", __FUNCTION__, __LINE__);
        dev->internalphy = true;
        return;
        // dev->OperationMode = OPERATION_PHY_MODE;
        // dev->PhySelect = 0x00;
    } else if (tempphyselect == AX_PHYSEL_SSMII) {
        USB_LOG_INFO("%s L%d internalphy AX_PHYSEL_SSMII & OPERATION_MAC_MODE\r\n", __FUNCTION__, __LINE__);
        dev->internalphy = true;
        dev->OperationMode = OPERATION_MAC_MODE;
        dev->PhySelect = 0x01;
    } else {
        // deverr(dev, "Unknown MII type\n");
        USB_LOG_INFO("%s L%d Unknown MII type\r\n", __FUNCTION__, __LINE__);
        return;
    }

    /* reload eeprom data */
	ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_GPIOS, AXGPIOS_RSE, 0, 0, NULL);
    if (ret < 0) {
        USB_LOG_ERR("reload eeprom data ret=%d\r\n", ret);
        return;
    }

	/* Get the EEPROM data: power saving configuration*/
	ret = ax8817x_read_cmd(dev, AX_CMD_READ_EEPROM, 0x18, 0, 2, buf);
	if (ret < 0) {
		USB_LOG_ERR("read SROM address 18h failed: %d\r\n", ret);
		goto err_out;
	}
    USB_LOG_INFO("reading AX88772C psc: %02x %02x\r\n", buf[0], buf[1]);
	// le16_to_cpus(tmp16);
	// ax772b_data->psc = *tmp16 & 0xFF00;
	/* End of get EEPROM data */

	ret = ax8817x_get_mac(dev, buf);
	if (ret < 0) {
		USB_LOG_ERR("Get HW address failed: %d\r\n", ret);
		return;
	}
    dump_hex(buf, ETH_ALEN);
    memcpy(dev->dev_addr, buf, ETH_ALEN);

    uint16_t chipcode = 0xFFFF;
    {
        uint16_t smsr;
        // asix_read_cmd(dev, AX_CMD_STATMNGSTS_REG, 0, 0, 1, &chipcode, 0);
        ax8817x_read_cmd(dev, AX_CMD_READ_STATMNGSTS_REG, 0, 0, 1, &chipcode);
        USB_LOG_ERR("AX_CMD_READ_STATMNGSTS_REG ret: %d %04X\r\n", ret, chipcode);

// #define AX_CHIPCODE_MASK		0x70
// #define AX_AX88772_CHIPCODE		0x00
// #define AX_AX88772A_CHIPCODE		0x10
// #define AX_AX88772B_CHIPCODE		0x20
// #define AX_HOST_EN			0x01

        chipcode &= 0x70;//AX_CHIPCODE_MASK; AX_AX88772_CHIPCODE
        if(chipcode == 0x00)
        {
            USB_LOG_ERR("AX_CMD_READ_STATMNGSTS_REG AX_AX88772_CHIPCODE\r\n");
        }
        else if(chipcode == 0x10)
        {
            USB_LOG_ERR("AX_CMD_READ_STATMNGSTS_REG AX_AX88772A_CHIPCODE\r\n");
        }
        else if(chipcode == 0x20)
        {
            USB_LOG_ERR("AX_CMD_READ_STATMNGSTS_REG AX_AX88772B_CHIPCODE\r\n");
        }
    }

    /* Get the PHY id: E0 10 */
    ret = ax8817x_read_cmd(dev, AX_CMD_READ_PHY_ID, 0, 0, 2, buf);
    if (ret < 0) {
        USB_LOG_ERR("Error reading PHY ID: %02x\r\n", ret);
        return;
    }
    if (dev->internalphy) {
        dev->mii.phy_id = *((u8 *)buf + 1);
    } else {
        dev->mii.phy_id = *((u8 *)buf);
    }
    USB_LOG_INFO("reading %s PHY ID: %02x\r\n", dev->internalphy?"internal":"external", dev->mii.phy_id);

    ret = ax8817x_write_cmd(dev, AX_CMD_SW_PHY_SELECT, dev->PhySelect, 0, 0, NULL);
    if (ret < 0) {
        USB_LOG_ERR("Select PHY #1 failed: %d", ret);
        return;
    }

	ret = ax88772a_phy_powerup(dev);
	if (ret < 0) {
        USB_LOG_ERR("ax88772a_phy_powerup failed: %d", ret);
        return;
    }

	/* stop MAC operation */
	ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_RX_CTL,
				AX_RX_CTL_STOP, 0, 0, NULL);
	if (ret < 0) {
		USB_LOG_ERR("Reset RX_CTL failed: %d", ret);
		goto err_out;
	}

	/* make sure the driver can enable sw mii operation */
	ret = ax8817x_write_cmd(dev, AX_CMD_SET_SW_MII, 0, 0, 0, NULL);
	if (ret < 0) {
		USB_LOG_ERR("Enabling software MII failed: %d\r\n", ret);
		goto err_out;
	}

    if ((dev->OperationMode == OPERATION_MAC_MODE) &&
        (dev->PhySelect == 0x00)) {
        USB_LOG_ERR("not support the external phy\r\n");
        goto err_out;
    }

    if (dev->OperationMode == OPERATION_PHY_MODE) {
        ax8817x_mdio_write_le(dev, dev->mii.phy_id, MII_BMCR, 0x3900);
    }

    if (dev->mii.phy_id != 0x10)
    {
        USB_LOG_ERR("not support phy_id != 0x10\r\n");
		// ax8817x_mdio_write_le(dev->net, 0x10, MII_BMCR, 0x3900);
    }

    if (dev->mii.phy_id == 0x10 && dev->OperationMode != OPERATION_PHY_MODE) {
        u16 tmp16 = ax8817x_mdio_read_le(dev, dev->mii.phy_id, 0x12);
        ax8817x_mdio_write_le(dev, dev->mii.phy_id, 0x12, ((tmp16 & 0xFF9F) | 0x0040));
    }

	ax8817x_mdio_write_le(dev, dev->mii.phy_id, MII_ADVERTISE,
			ADVERTISE_ALL | ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP);

    // mii_nway_restart(&dev->mii);
    {
        /* if autoneg is off, it's an error */
        uint16_t bmcr = ax8817x_mdio_read_le(dev, dev->mii.phy_id, MII_BMCR);
        if (bmcr & BMCR_ANENABLE) {
            bmcr |= BMCR_ANRESTART;
            USB_LOG_ERR("BMCR_ANENABLE ==> BMCR_ANRESTART\r\n");
            ax8817x_mdio_write_le(dev, dev->mii.phy_id, MII_BMCR, bmcr);
        } else
        {
            USB_LOG_ERR("not BMCR_ANENABLE BMCR=%04X\r\n", bmcr);
        }
    }

    ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_MEDIUM_MODE, 0, 0, 0, NULL);
    if (ret < 0) {
		USB_LOG_ERR("Failed to write medium mode: %d", ret);
		goto err_out;
	}

	ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_IPG0,
			AX88772A_IPG0_DEFAULT | AX88772A_IPG1_DEFAULT << 8,
			AX88772A_IPG2_DEFAULT, 0, NULL);
	if (ret < 0) {
		USB_LOG_ERR("Failed to write interframe gap: %d", ret);
		goto err_out;
	}

	memset(buf, 0, 4);
	ret = ax8817x_read_cmd(dev, AX_CMD_READ_IPG012, 0, 0, 3, buf);
	*((u8 *)buf + 3) = 0x00;
	if (ret < 0) {
		USB_LOG_ERR("Failed to read IPG,IPG1,IPG2 failed: %d", ret);
		goto err_out;
	} else {
		uint32_t tmp32 = *((u32*)buf);
		le32_to_cpus(&tmp32);
		if (tmp32 != (AX88772A_IPG2_DEFAULT << 16 |
			AX88772A_IPG1_DEFAULT << 8 | AX88772A_IPG0_DEFAULT)) {
			USB_LOG_ERR("Non-authentic ASIX product\nASIX does not support it\n");
			// ret = -ENODEV;
			goto err_out;
		}
	}

    // TODO: optimized  for high speed.
    ret = ax8817x_write_cmd(dev, 0x2A, 0x8000, 0x8001, 0, NULL);
    if (ret < 0) {
        USB_LOG_ERR("Reset RX_CTL failed: %d", ret);
        goto err_out;
    }

    ret = ax88772b_reset(dev);
    if (ret < 0) {
        USB_LOG_ERR("ax88772b_reset failed: %d", ret);
        goto err_out;
    }

    // OUT 29  0   0 0   AX_CMD_WRITE_MONITOR_MODE
    ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_MONITOR_MODE, 0, 0, 0, NULL);
    if (ret < 0) {
        deverr(dev, "AX_CMD_WRITE_MONITOR_MODE failed: %d", ret);
    }

	/* Set the MAC address */
    ret = ax8817x_write_cmd(dev, AX88772_CMD_WRITE_NODE_ID,
                            0, 0, ETH_ALEN, dev->dev_addr);
    if (ret < 0) {
        deverr(dev, "set MAC address failed: %d", ret);
    }

    // update Multicast AX_CMD_WRITE_MULTI_FILTER.
    const uint8_t multi_filter[] = {0x00, 0x00, 0x20, 0x80, 0x00, 0x00, 0x00, 0x40};
    ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_MULTI_FILTER, 0, 0, AX_MCAST_FILTER_SIZE, (void *)multi_filter);
    if (ret < 0) {
        USB_LOG_ERR("Reset RX_CTL failed: %d", ret);
        goto err_out;
    }

    /* Configure RX header type */
    // u16 rx_reg = (AX_RX_CTL_PRO | AX_RX_CTL_AMALL | AX_RX_CTL_START | AX_RX_CTL_AB | AX_RX_HEADER_DEFAULT);
    u16 rx_reg = (AX_RX_CTL_AB | AX_RX_CTL_AM | AX_RX_CTL_START);
    ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_RX_CTL, rx_reg, 0, 0, NULL);
    if (ret < 0) {
        USB_LOG_ERR("Reset RX_CTL failed: %d", ret);
        goto err_out;
    }

    /* set the embedded Ethernet PHY in power-up state */
    // ax772b_data->psc = *tmp16 & 0xFF00;
    // psc: 15 5a AX88772C psc: %02x %02x\r\n", buf[0], buf[1]);
    ret = ax8817x_write_cmd(dev, AX_CMD_SW_RESET, AX_SWRESET_IPRL | (0x5a00 & 0x7FFF),
                            0, 0, NULL);
    if (ret < 0) {
        deverr(dev, "Failed to reset PHY: %d", ret);
        // return ret;
    }

    rt_thread_delay(1000);
    u16 mode = AX88772_MEDIUM_DEFAULT;
    ret = ax8817x_write_cmd(dev, AX_CMD_WRITE_MEDIUM_MODE, mode, 0, 0, NULL);
    if (ret < 0) {
        USB_LOG_ERR("AX_CMD_WRITE_MEDIUM_MODE failed: %d", ret);
        goto err_out;
    }

#if USE_RTTHREAD
#ifdef RT_USING_DEVICE_OPS
    usbh_axusbnet_eth_device.parent.parent.ops           = &rndis_device_ops;
#else
    usbh_axusbnet_eth_device.parent.parent.init          = rt_rndis_eth_init;
    usbh_axusbnet_eth_device.parent.parent.open          = rt_rndis_eth_open;
    usbh_axusbnet_eth_device.parent.parent.close         = rt_rndis_eth_close;
    usbh_axusbnet_eth_device.parent.parent.read          = rt_rndis_eth_read;
    usbh_axusbnet_eth_device.parent.parent.write         = rt_rndis_eth_write;
    usbh_axusbnet_eth_device.parent.parent.control       = rt_rndis_eth_control;
#endif
    usbh_axusbnet_eth_device.parent.parent.user_data     = RT_NULL;

    usbh_axusbnet_eth_device.parent.eth_rx               = rt_rndis_eth_rx;
    usbh_axusbnet_eth_device.parent.eth_tx               = rt_rndis_eth_tx;

    usbh_axusbnet_eth_device.class = class;

    eth_device_init(&usbh_axusbnet_eth_device.parent, "u0");
    eth_device_linkchange(&usbh_axusbnet_eth_device.parent, RT_FALSE);
#endif /* USE_RTTHREAD */
    // check link status.
    {
        u16 bmcr = ax8817x_mdio_read_le(dev, dev->mii.phy_id, MII_BMCR);
        u16 mode = AX88772_MEDIUM_DEFAULT;

        USB_LOG_ERR("%s L%d MII_BMCR=%04X\r\n", __FUNCTION__, __LINE__, bmcr);
		if (!(bmcr & BMCR_FULLDPLX))
        {
			mode &= ~AX88772_MEDIUM_FULL_DUPLEX;
            USB_LOG_ERR("%s L%d not AX88772_MEDIUM_FULL_DUPLEX\r\n", __FUNCTION__, __LINE__);
        }
		if (!(bmcr & BMCR_SPEED100))
        {
			mode &= ~AX88772_MEDIUM_100MB;
            USB_LOG_ERR("%s L%d not AX88772_MEDIUM_100MB\r\n", __FUNCTION__, __LINE__);
        }
		ax8817x_write_cmd(dev, AX_CMD_WRITE_MEDIUM_MODE, mode, 0, 0, NULL);
    }

    while (1)
    {
        // USB_LOG_INFO("%s L%d\r\n", __FUNCTION__, __LINE__);

        ret = usbh_ep_bulk_transfer(class->bulkin, class->bulkin_buf, sizeof(class->bulkin_buf), 1000);
        if (ret < 0) {
            if (ret != -2) {
                USB_LOG_ERR("%s L%d bulk in error ret=%d\r\n", __FUNCTION__, __LINE__, ret);
            }
            continue;
        }

        {
            const uint8_t *data = class->bulkin_buf;
            uint16_t len1, len2;

            len1 = data[0] | ((uint16_t)(data[1])<<8);
            len2 = data[2] | ((uint16_t)(data[3])<<8);

            // USB_LOG_INFO("transfer bulkin len1:%04X, len2:%04X, len2':%04X.\r\n", len1, len2, ~len2);

            len1 &= 0x07ff;

            if (data[0] != ((uint8_t)(~data[2]))) {
                USB_LOG_ERR("transfer bulkin len1:%04X, len2:%04X, len2':%04X.\r\n", len1, len2, ~len2);

                dump_hex(data, 32);
                continue;
            }

#if !USE_RTTHREAD
            {
                static uint32_t count = 0;
                USB_LOG_INFO("recv: #%d, len=%d\r\n", count, ret);
                dump_hex(data+4, 32);

                if ((count % 10) == 0) {
                    // 192.168.89.14 ==> 255.255.255.255:7 echo hello world!
                    const uint8_t packet_bytes[] = {
                        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x45, 0x00,
                        0x00, 0x36, 0xb0, 0xfd, 0x00, 0x00, 0x80, 0x11,
                        0x00, 0x00, 0xc0, 0xa8, 0x59, 0x0e, 0xff, 0xff,
                        0xff, 0xff, 0x00, 0x07, 0x00, 0x07, 0x00, 0x22,
                        0x53, 0x06, 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20,
                        0x77, 0x6f, 0x72, 0x6c, 0x64, 0x20, 0x66, 0x72,
                        0x6f, 0x6d, 0x20, 0x41, 0x58, 0x38, 0x38, 0x37,
                        0x37, 0x32, 0x43, 0x2e
                    };

                    uint8_t *send_buf = (uint8_t *)class->bulkin_buf;
                    send_buf[0] = sizeof(packet_bytes);
                    send_buf[1] = sizeof(packet_bytes) >> 8;
                    send_buf[2] = ~send_buf[0];
                    send_buf[3] = ~send_buf[1];
                    memcpy(send_buf+4, packet_bytes, sizeof(packet_bytes));
                    memcpy(send_buf+4+6, dev->dev_addr, 6);// update src mac.

                    ret = usbh_ep_bulk_transfer(class->bulkout, send_buf, 4 + sizeof(packet_bytes), 500);
                    USB_LOG_INFO("bulkout, ret=%d\r\n", ret);
                    dump_hex(send_buf, 64);
                }

                count++;
            }
#endif /* RT-Thread */

#if USE_RTTHREAD
            {
                static uint32_t count = 0;

                if (count == 0) {
                    eth_device_linkchange(&usbh_axusbnet_eth_device.parent, RT_TRUE);
                }

                count++;
            }

            /* allocate buffer */
            struct pbuf *p = RT_NULL;
            p = pbuf_alloc(PBUF_LINK, len1, PBUF_RAM);
            if (p != NULL) {
                pbuf_take(p, data + 4, len1);

#ifdef RX_DUMP
                packet_dump("RX", p);
#endif /* RX_DUMP */
                struct eth_device *eth_dev = &usbh_axusbnet_eth_device.parent;
                if ((eth_dev->netif->input(p, eth_dev->netif)) != ERR_OK) {
                    USB_LOG_INFO("F:%s L:%d IP input error\r\n", __FUNCTION__, __LINE__);
                    pbuf_free(p);
                    p = RT_NULL;
                }
                // USB_LOG_INFO("%s L%d input OK\r\n", __FUNCTION__, __LINE__);
            } else {
                USB_LOG_ERR("%s L%d pbuf_alloc NULL\r\n", __FUNCTION__, __LINE__);
            }
#endif /* RT-Thread */
        }
    } // while (1)

err_out:
out2:

    return;
}

static int axusbnet_startup(void)
{
    const char *tname = "axusbnet";
    usb_osal_thread_t usb_thread;

    usb_thread = usb_osal_thread_create(tname, 1024 * 6, CONFIG_USBHOST_PSC_PRIO, rt_thread_axusbnet_entry, NULL);
    if (usb_thread == NULL) {
        return -1;
    }

    return 0;
}

static int usbh_axusbnet_connect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;
    struct usbh_endpoint_cfg ep_cfg = { 0 };
    struct usb_endpoint_descriptor *ep_desc;

    USB_LOG_INFO("%s %d\r\n", __FUNCTION__, __LINE__);

    struct usbh_axusbnet *class = usb_malloc(sizeof(struct usbh_axusbnet));
    if (class == NULL)
    {
        USB_LOG_ERR("Fail to alloc class\r\n");
        return -ENOMEM;
    }
    memset(class, 0, sizeof(struct usbh_axusbnet));
    class->hport = hport;

    class->intf = intf;

    snprintf(hport->config.intf[intf].devname, CONFIG_USBHOST_DEV_NAMELEN, DEV_FORMAT, intf);
    USB_LOG_INFO("Register axusbnet Class:%s\r\n", hport->config.intf[intf].devname);
    hport->config.intf[intf].priv = class;

#if 1
    USB_LOG_INFO("hport=%p, intf=%d, intf_desc.bNumEndpoints:%d\r\n", hport, intf, hport->config.intf[intf].intf_desc.bNumEndpoints);
    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++)
    {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;

        USB_LOG_INFO("ep[%d] bLength=%d, type=%d\r\n", i, ep_desc->bLength, ep_desc->bDescriptorType);
        USB_LOG_INFO("ep_addr=%02X, attr=%02X\r\n", ep_desc->bEndpointAddress, ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK);
        USB_LOG_INFO("wMaxPacketSize=%d, bInterval=%d\r\n\r\n", ep_desc->wMaxPacketSize, ep_desc->bInterval);
    }
#endif

    for (uint8_t i = 0; i < hport->config.intf[intf].intf_desc.bNumEndpoints; i++)
    {
        ep_desc = &hport->config.intf[intf].ep[i].ep_desc;

        ep_cfg.ep_addr = ep_desc->bEndpointAddress;
        ep_cfg.ep_type = ep_desc->bmAttributes & USB_ENDPOINT_TYPE_MASK;
        ep_cfg.ep_mps = ep_desc->wMaxPacketSize;
        ep_cfg.ep_interval = ep_desc->bInterval;
        ep_cfg.hport = hport;

        if(ep_cfg.ep_type == USB_ENDPOINT_TYPE_BULK)
        {
            if (ep_desc->bEndpointAddress & 0x80) {
                usbh_pipe_alloc(&class->bulkin, &ep_cfg);
            } else {
                usbh_pipe_alloc(&class->bulkout, &ep_cfg);
            }
        }
        else
        {
            usbh_pipe_alloc(&class->int_notify, &ep_cfg);
        }
    }

    axusbnet_startup();

    return ret;
}

static int usbh_axusbnet_disconnect(struct usbh_hubport *hport, uint8_t intf)
{
    int ret = 0;

    USB_LOG_ERR("TBD: %s %d\r\n", __FUNCTION__, __LINE__);
    return ret;
}

// Class:0xff,Subclass:0xff,Protocl:0x00
static const struct usbh_class_driver axusbnet_class_driver = {
    .driver_name = "axusbnet",
    .connect = usbh_axusbnet_connect,
    .disconnect = usbh_axusbnet_disconnect
};

CLASS_INFO_DEFINE const struct usbh_class_info axusbnet_class_info = {
    .match_flags = USB_CLASS_MATCH_VENDOR | USB_CLASS_MATCH_PRODUCT | USB_CLASS_MATCH_INTF_CLASS | USB_CLASS_MATCH_INTF_SUBCLASS | USB_CLASS_MATCH_INTF_PROTOCOL,
    .class = USB_DEVICE_CLASS_VEND_SPECIFIC,
    .subclass = 0xff,
    .protocol = 0x00,
    .vid = 0x0b95,
    .pid = 0x772b,
    .class_driver = &axusbnet_class_driver
};
