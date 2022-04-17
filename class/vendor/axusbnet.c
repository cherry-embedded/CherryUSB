/*
 * Change Logs
 * Date           Author       Notes
 * 2022-04-17     aozima       the first version for CherryUSB.
 */

#include <string.h>

#include "usbh_core.h"
#include "axusbnet.h"

static const char *DEV_FORMAT = "/dev/u%d";


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
                usbh_ep_alloc(&class->bulkin, &ep_cfg);
            } else {
                usbh_ep_alloc(&class->bulkout, &ep_cfg);
            }
        }
        else
        {
            usbh_ep_alloc(&class->int_notify, &ep_cfg);
        }
    }

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
