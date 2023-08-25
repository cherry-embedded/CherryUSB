#ifndef _USB_EHCI_PRIV_H
#define _USB_EHCI_PRIV_H

#include "usbh_core.h"
#include "usbh_hub.h"
#include "usb_hc_ehci.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBH_IRQHandler
#endif

#define EHCI_HCCR        ((struct ehci_hccr *)CONFIG_USB_EHCI_HCCR_BASE)
#define EHCI_HCOR        ((struct ehci_hcor *)CONFIG_USB_EHCI_HCOR_BASE)

#define EHCI_PTR2ADDR(x) ((uint32_t)(x) & ~0x1F)
#define EHCI_ADDR2QH(x)  ((struct ehci_qh_hw *)((uint32_t)(x) & ~0x1F))
#define EHCI_ADDR2QTD(x) ((struct ehci_qtd_hw *)((uint32_t)(x) & ~0x1F))
#define EHCI_ADDR2ITD(x) ((struct ehci_itd_hw *)((uint32_t)(x) & ~0x1F))

#if CONFIG_USB_EHCI_FRAME_LIST_SIZE == 1024
#define EHCI_PERIOIDIC_QH_NUM 11
#elif CONFIG_USB_EHCI_FRAME_LIST_SIZE == 512
#define EHCI_PERIOIDIC_QH_NUM 10
#elif CONFIG_USB_EHCI_FRAME_LIST_SIZE == 256
#define EHCI_PERIOIDIC_QH_NUM 9
#else
#error Unsupported frame size list size
#endif

#define CONFIG_USB_EHCI_QH_NUM  CONFIG_USBHOST_PIPE_NUM
#define CONFIG_USB_EHCI_QTD_NUM (CONFIG_USBHOST_PIPE_NUM * 3)
#define CONFIG_USB_EHCI_ITD_NUM 20

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
    struct usbh_urb *urb;
    uint8_t mf_unmask;
    uint8_t mf_valid;
    uint8_t iso_packet_idx;
    uint8_t remain_itd_num;
};

struct ehci_qh_hw {
    struct ehci_qh hw;
    uint32_t first_qtd;
    struct usbh_urb *urb;
    uint8_t remove_in_iaad;
} __attribute__((aligned(32)));

struct ehci_qtd_hw {
    struct ehci_qtd hw;
    struct usbh_urb *urb;
    uint32_t total_len;
} __attribute__((aligned(32)));

struct ehci_itd_hw {
    struct ehci_itd hw;
    struct usbh_urb *urb;
    struct ehci_pipe *pipe;
    uint16_t start_frame;
    usb_slist_t list;
} __attribute__((aligned(32)));

struct ehci_hcd {
    bool ehci_qh_used[CONFIG_USB_EHCI_QH_NUM];
    bool ehci_qtd_used[CONFIG_USB_EHCI_QTD_NUM];
    bool ehci_itd_used[CONFIG_USB_EHCI_ITD_NUM];
    struct ehci_pipe pipe_pool[CONFIG_USB_EHCI_QH_NUM];
};

extern struct ehci_hcd g_ehci_hcd;
extern uint32_t g_framelist[];

int ehci_iso_pipe_init(struct ehci_pipe *pipe, struct usbh_urb *urb);
void ehci_remove_itd_urb(struct usbh_urb *urb);
void ehci_scan_isochronous_list(void);

#endif