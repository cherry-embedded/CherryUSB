#ifndef _USB_EHCI_PRIV_H
#define _USB_EHCI_PRIV_H

#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_hc_ehci.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBH_IRQHandler
#endif

#define EHCI_HCCR           ((struct ehci_hccr *)CONFIG_USB_EHCI_HCCR_BASE)
#define EHCI_HCOR           ((struct ehci_hcor *)CONFIG_USB_EHCI_HCOR_BASE)

#define EHCI_PTR2ADDR(x)    ((uint32_t)x)
#define EHCI_ADDRALIGN32(x) ((uint32_t)(x) & ~0x1F)
#define EHCI_ADDR2QH(x)     ((struct ehci_qh_hw *)((uint32_t)(x) & ~0x1F))
#define EHCI_ADDR2ITD(x)    ((struct ehci_itd_hw *)((uint32_t)(x) & ~0x1F))

#if CONFIG_USB_EHCI_FRAME_LIST_SIZE == 1024
#define EHCI_PERIOIDIC_QH_NUM 11
#elif CONFIG_USB_EHCI_FRAME_LIST_SIZE == 512
#define EHCI_PERIOIDIC_QH_NUM 10
#elif CONFIG_USB_EHCI_FRAME_LIST_SIZE == 256
#define EHCI_PERIOIDIC_QH_NUM 9
#else
#error Unsupported frame size list size
#endif

#define CONFIG_USB_EHCI_QH_NUM       CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USB_EHCI_QTD_NUM      (CONFIG_USBHOST_PIPE_NUM * 3)
#define CONFIG_USB_EHCI_ITD_NUM      10
#define CONFIG_USB_EHCI_ITD_POOL_NUM 2

extern uint8_t usbh_get_port_speed(const uint8_t port);

struct ehci_qh_hw;
struct ehci_itd_hw;
struct ehci_pipe {
    uint8_t dev_addr;
    uint8_t ep_addr;
    uint8_t ep_type;
    uint8_t ep_interval;
    uint8_t speed;
    uint8_t mult;
    uint16_t ep_mps;
    bool toggle;
    bool inuse;
    uint32_t xfrd;
    bool waiter;
    usb_osal_sem_t waitsem;
    struct usbh_hubport *hport;
    struct ehci_qh_hw *qh;
    struct usbh_urb *urb;
    uint8_t used_itd_num;
    uint8_t id;
    uint8_t mf_unmask;
    uint8_t mf_valid;
    usb_slist_t iso_list;
};

struct ehci_qh_hw {
    struct ehci_qh hw;
    uint32_t first_qtd;
    struct ehci_pipe *pipe;
} __attribute__((aligned(32)));

struct ehci_qtd_hw {
    struct ehci_qtd hw;
} __attribute__((aligned(32)));

struct ehci_itd_hw {
    struct ehci_itd hw;
} __attribute__((aligned(32)));

struct ehci_hcd {
    bool ehci_qh_used[CONFIG_USB_EHCI_QH_NUM];
    bool ehci_qtd_used[CONFIG_USB_EHCI_QTD_NUM];
    bool ehci_itd_pool_used[CONFIG_USB_EHCI_ITD_POOL_NUM];
    struct ehci_pipe pipe_pool[CONFIG_USB_EHCI_QH_NUM];
};

extern struct ehci_hcd g_ehci_hcd;
extern uint32_t g_framelist[];

int ehci_itd_pool_alloc(void);
void ehci_itd_pool_free(uint8_t id);
int ehci_iso_pipe_init(struct ehci_pipe *pipe, struct usbh_iso_frame_packet *iso_packet, uint32_t num_of_iso_packets);
void ehci_remove_itd_urb(struct usbh_urb *urb);
void ehci_scan_isochronous_list(void);

#endif