/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

#include <stdio.h>
#include "board.h"
#include "netif/etharp.h"
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/icmp.h"
#include "lwip/udp.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"
#include <lwip/ip.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include "usbh_core.h"
#include "usbh_rndis.h"
#include "rndis_protocol.h"

/* Macro Definition */
/*--------------- Tasks Priority -------------*/
#define RNDIS_RECV_DATA_TASK_PRIO (tskIDLE_PRIORITY + 6)

#define RNDIS_ETH_BUFFER_LEN      (sizeof(rndis_data_packet_t) + 1514 + 42)
#define RNDIS_RXETH_BUFFER_LEN    (RNDIS_ETH_BUFFER_LEN * 3)

/* Static Variable Definition*/
static struct usbh_rndis *s_rndis_class_ptr;
static struct netif netif_data;
static ip4_addr_t ipaddr;
static ip4_addr_t netmask;
static ip4_addr_t gateway;
USB_NOCACHE_RAM_SECTION uint8_t tx_buffer[RNDIS_ETH_BUFFER_LEN];
USB_NOCACHE_RAM_SECTION uint8_t rx_buffer[RNDIS_RXETH_BUFFER_LEN];
static uint8_t *rx_buf_ptr;
SemaphoreHandle_t mutex_sem_handle;
SemaphoreHandle_t sig_sem_handle;
TimerHandle_t timer_handle;
TaskHandle_t data_recv_task_handle;

/* Static Function Declaration */
static void rndis_dev_keepalive_timeout(void *pdata);
static err_t netif_init_cb(struct netif *netif);
static err_t linkoutput_fn(struct netif *netif, struct pbuf *p);
static int usbh_rndis_eth_tx(struct pbuf *p);
static int rndis_msg_data_send(struct usbh_rndis *rndis_class, uint8_t *buffer, int nbytes);
static void lwip_rx_handle(void *pdata);

extern int usbh_rndis_query_msg_transfer(struct usbh_rndis *rndis_class,
                                         uint32_t oid, uint32_t query_len,
                                         uint8_t *info, uint32_t *info_len);

void usbh_rndis_run(struct usbh_rndis *rndis_class)
{
    struct netif *netif = &netif_data;

    s_rndis_class_ptr = rndis_class;
    rx_buf_ptr        = rx_buffer;

    /* Create tcp_ip stack thread */
    tcpip_init(NULL, NULL);

    netif->hwaddr_len = 6;
    memcpy(netif->hwaddr, rndis_class->mac, 6);

    IP4_ADDR(&ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&netmask, 0, 0, 0, 0);
    IP4_ADDR(&gateway, 0, 0, 0, 0);

    netif = netif_add(netif, &ipaddr, &netmask, &gateway, NULL, netif_init_cb, tcpip_input);
    netif_set_default(netif);
    netif_set_up(netif);

    mutex_sem_handle = xSemaphoreCreateMutex();
    if (NULL == mutex_sem_handle) {
        printf("mutex semaphore creat faile!\r\n");
        for (;;) {
            ;
        }
    }

    sig_sem_handle = xSemaphoreCreateCounting(1, 0);
    if (NULL == sig_sem_handle) {
        printf("Rndis sig semaphore creat faile!\r\n");
        for (;;) {
            ;
        }
    }

    timer_handle =
        xTimerCreate((const char *)NULL, (TickType_t)5000, (UBaseType_t)pdTRUE, (void *const)1, (TimerCallbackFunction_t)rndis_dev_keepalive_timeout);
    if (NULL != timer_handle) {
        xTimerStart(timer_handle, 0);
    } else {
        printf("timer creation failed!.\n");
        for (;;) {
            ;
        }
    }

    if (xTaskCreate(lwip_rx_handle, "rndis_lwip_rx", configMINIMAL_STACK_SIZE + 2048U, rndis_class, RNDIS_RECV_DATA_TASK_PRIO, &data_recv_task_handle) !=
        pdPASS) {
        printf("rndis_lwip_rx Task creation failed!.\n");
        for (;;) {
            ;
        }
    }
}

void usbh_rndis_stop(struct usbh_rndis *rndis_class)
{
    vTaskDelete(data_recv_task_handle);

    xTimerStop(timer_handle, 0);
    xTimerDelete(timer_handle, 0);

    vSemaphoreDelete(mutex_sem_handle);
    vSemaphoreDelete(sig_sem_handle);

    netif_remove(&netif_data);

    printf("rndis dev stop!\n");
}

static void rndis_dev_keepalive_timeout(void *pdata)
{
    int ret;

    xSemaphoreTake(mutex_sem_handle, portMAX_DELAY);
    ret = usbh_rndis_keepalive(s_rndis_class_ptr);
    xSemaphoreGive(mutex_sem_handle);
    if (ret < 0) {
        printf("rndis dev keepalive timeout!\n");
    }
}

static void lwip_rx_handle(void *pdata)
{
    err_t err;
    int ret = 0;
    struct pbuf *p;
    rndis_data_packet_t *pmsg;
    int pmg_offset;
    int payload_offset;

    struct usbh_rndis *rndis_class = (struct usbh_rndis *)pdata;
    rx_buf_ptr = rx_buffer;

    while (1) {
        pmg_offset = 0;
        payload_offset = 0;
        ret = usbh_rndis_bulk_in_transfer(rndis_class, rx_buf_ptr, RNDIS_RXETH_BUFFER_LEN, USB_OSAL_WAITING_FOREVER);
        if (ret <= 0) {
            vTaskDelay(1);
            continue;
        }
        while (ret > 0) {
            pmsg = (rndis_data_packet_t *)(rx_buf_ptr + pmg_offset);
            if (pmsg->MessageType == REMOTE_NDIS_PACKET_MSG) {
                /* allocate buffer */
                p = pbuf_alloc(PBUF_RAW, pmsg->DataLength, PBUF_POOL);
                if (p != NULL) {
                    struct pbuf *q;
                    for (q = p; q != NULL; q = q->next) {
                        /* Copy the received frame into buffer from memory pointed by the current ETHERNET DMA Rx descriptor */
                        memcpy(q->payload, ((uint8_t *)(&pmsg->DataOffset) + pmsg->DataOffset + payload_offset), q->len);
                        payload_offset += q->len;
                    }
                    /* entry point to the LwIP stack */
                    err = netif_data.input(p, &netif_data);
                    if (err != ERR_OK) {
                        LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
                        pbuf_free(p);
                    }
                    pmg_offset += pmsg->MessageLength;
                    ret -= pmsg->MessageLength;
                }
            }
        }
    }
}

static err_t netif_init_cb(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu        = 1500;
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    netif->state      = NULL;
    netif->name[0]    = 'E';
    netif->name[1]    = 'X';
    netif->linkoutput = linkoutput_fn;
    netif->output     = etharp_output;
    return ERR_OK;
}

static err_t linkoutput_fn(struct netif *netif, struct pbuf *p)
{
    int ret;

    ret = usbh_rndis_eth_tx(p);

    if (-EBUSY == ret) {
        ret = ERR_BUF;
    }

    return ret;
}

static int usbh_rndis_eth_tx(struct pbuf *p)
{
    struct pbuf *q;
    uint8_t *buffer;
    rndis_data_packet_t *hdr;
    int ret;
    int recount = 5;
    uint8_t data[4];
    uint32_t info_len = 0;

    if (!s_rndis_class_ptr->link_status) {
        printf("linkdown, drop pkg\r\n");
        while (recount--) {
            ret = usbh_rndis_query_msg_transfer(s_rndis_class_ptr, OID_GEN_MEDIA_CONNECT_STATUS, sizeof(data), data, &info_len);
            if (ret < 0) {
                return -EBUSY;
            }
            if (NDIS_MEDIA_STATE_CONNECTED == data[0]) {
                s_rndis_class_ptr->link_status = true;
                break;
            } else {
                s_rndis_class_ptr->link_status = false;
            }
            vTaskDelay(1000);
        }
        return 0;
    }

    assert((p->tot_len + sizeof(rndis_data_packet_t)) < sizeof(tx_buffer));
    if (p->tot_len > sizeof(tx_buffer)) {
        printf("RNDIS MTU is:%d, but the send packet size is %d\r\n", sizeof(tx_buffer), p->tot_len);
        p->tot_len = sizeof(tx_buffer);
    }

    hdr = (rndis_data_packet_t *)tx_buffer;
    memset(hdr, 0, sizeof(rndis_data_packet_t));
    hdr->MessageType   = REMOTE_NDIS_PACKET_MSG;
    hdr->MessageLength = sizeof(rndis_data_packet_t) + p->tot_len;
    hdr->DataOffset    = sizeof(rndis_data_packet_t) - sizeof(rndis_generic_msg_t);
    hdr->DataLength    = p->tot_len;

    buffer = (uint8_t *)(tx_buffer + sizeof(rndis_data_packet_t));
    for (q = p; q != NULL; q = q->next) {
        memcpy(buffer, q->payload, q->len);
        buffer += q->len;
    }
    /* send */
    if ((hdr->MessageLength & 0x1FF) == 0) {
        /* pad a dummy. */
        hdr->MessageLength += 1;
    }
    return rndis_msg_data_send(s_rndis_class_ptr, (uint8_t *)tx_buffer, hdr->MessageLength);
}

static int rndis_msg_data_send(struct usbh_rndis *rndis_class, uint8_t *buffer,
                               int nbytes)
{
    int ret = ERR_OK;
    int len = 0;
    xSemaphoreTake(mutex_sem_handle, portMAX_DELAY);
    len = usbh_rndis_bulk_out_transfer(rndis_class, buffer, nbytes, 5000);
    xSemaphoreGive(mutex_sem_handle);
    if (len != nbytes) {
        printf("rndis msg send fail\r\n");
        ret = -EBUSY;
    }
    return ret;
}
