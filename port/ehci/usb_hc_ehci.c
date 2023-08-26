/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usb_ehci_priv.h"

#define EHCI_TUNE_CERR    3 /* 0-3 qtd retries; 0 == don't stop */
#define EHCI_TUNE_RL_HS   4 /* nak throttle; see 4.9 */
#define EHCI_TUNE_RL_TT   0
#define EHCI_TUNE_MULT_HS 1 /* 1-3 transactions/uframe; 4.10.3 */
#define EHCI_TUNE_MULT_TT 1

struct ehci_hcd g_ehci_hcd;

USB_NOCACHE_RAM_SECTION struct ehci_qh_hw ehci_qh_pool[CONFIG_USB_EHCI_QH_NUM];
USB_NOCACHE_RAM_SECTION struct ehci_qtd_hw ehci_qtd_pool[CONFIG_USB_EHCI_QTD_NUM];

/* The head of the asynchronous queue */
USB_NOCACHE_RAM_SECTION struct ehci_qh_hw g_async_qh_head;
/* The head of the periodic queue */
USB_NOCACHE_RAM_SECTION struct ehci_qh_hw g_periodic_qh_head[EHCI_PERIOIDIC_QH_NUM];

/* The frame list */
USB_NOCACHE_RAM_SECTION uint32_t g_framelist[CONFIG_USB_EHCI_FRAME_LIST_SIZE] __attribute__((aligned(4096)));

static struct ehci_qh_hw *ehci_qh_alloc(void)
{
    struct ehci_qh_hw *qh;
    size_t flags;

    for (uint32_t i = 0; i < CONFIG_USB_EHCI_QH_NUM; i++) {
        if (!g_ehci_hcd.ehci_qh_used[i]) {
            flags = usb_osal_enter_critical_section();
            g_ehci_hcd.ehci_qh_used[i] = true;
            usb_osal_leave_critical_section(flags);

            qh = &ehci_qh_pool[i];
            memset(qh, 0, sizeof(struct ehci_qh_hw));
            qh->hw.hlp = QTD_LIST_END;
            qh->hw.overlay.next_qtd = QTD_LIST_END;
            qh->hw.overlay.alt_next_qtd = QTD_LIST_END;
            return qh;
        }
    }
    return NULL;
}

static void ehci_qh_free(struct ehci_qh_hw *qh)
{
    size_t flags;

    for (uint32_t i = 0; i < CONFIG_USB_EHCI_QH_NUM; i++) {
        if (&ehci_qh_pool[i] == qh) {
            flags = usb_osal_enter_critical_section();
            g_ehci_hcd.ehci_qh_used[i] = false;
            usb_osal_leave_critical_section(flags);

            qh->urb = NULL;
            return;
        }
    }
}

static struct ehci_qtd_hw *ehci_qtd_alloc(void)
{
    struct ehci_qtd_hw *qtd;
    size_t flags;

    for (uint32_t i = 0; i < CONFIG_USB_EHCI_QTD_NUM; i++) {
        if (!g_ehci_hcd.ehci_qtd_used[i]) {
            flags = usb_osal_enter_critical_section();
            g_ehci_hcd.ehci_qtd_used[i] = true;
            usb_osal_leave_critical_section(flags);

            qtd = &ehci_qtd_pool[i];
            memset(qtd, 0, sizeof(struct ehci_qtd_hw));
            qtd->hw.next_qtd = QTD_LIST_END;
            qtd->hw.alt_next_qtd = QTD_LIST_END;
            qtd->hw.token = QTD_TOKEN_STATUS_HALTED;
            return qtd;
        }
    }
    return NULL;
}

static void ehci_qtd_free(struct ehci_qtd_hw *qtd)
{
    size_t flags;

    for (uint32_t i = 0; i < CONFIG_USB_EHCI_QTD_NUM; i++) {
        if (&ehci_qtd_pool[i] == qtd) {
            flags = usb_osal_enter_critical_section();
            g_ehci_hcd.ehci_qtd_used[i] = false;
            usb_osal_leave_critical_section(flags);

            qtd->urb = NULL;
            return;
        }
    }
}

static struct ehci_pipe *ehci_pipe_alloc(void)
{
    int pipe;

    for (pipe = 0; pipe < CONFIG_USB_EHCI_QH_NUM; pipe++) {
        if (!g_ehci_hcd.pipe_pool[pipe].inuse) {
            g_ehci_hcd.pipe_pool[pipe].inuse = true;
            return &g_ehci_hcd.pipe_pool[pipe];
        }
    }
    return NULL;
}

static void ehci_pipe_free(struct ehci_pipe *pipe)
{
    pipe->inuse = false;
}

static inline void ehci_qh_add_head(struct ehci_qh_hw *head, struct ehci_qh_hw *n)
{
    n->hw.hlp = head->hw.hlp;
    head->hw.hlp = QH_HLP_QH(n);
}

static inline void ehci_qh_remove(struct ehci_qh_hw *head, struct ehci_qh_hw *n)
{
    struct ehci_qh_hw *tmp = head;

    while (EHCI_ADDR2QH(tmp->hw.hlp) && EHCI_ADDR2QH(tmp->hw.hlp) != n) {
        tmp = EHCI_ADDR2QH(tmp->hw.hlp);
    }

    if (tmp) {
        tmp->hw.hlp = n->hw.hlp;
    }
}

static int ehci_caculate_smask(int binterval)
{
    int order, interval;

    interval = 1;
    while (binterval > 1) {
        interval *= 2;
        binterval--;
    }

    if (interval < 2) /* interval 1 */
        return 0xFF;
    if (interval < 4) /* interval 2 */
        return 0x55;
    if (interval < 8) /* interval 4 */
        return 0x22;
    for (order = 0; (interval > 1); order++) {
        interval >>= 1;
    }
    return (0x1 << (order % 8));
}

static struct ehci_qh_hw *ehci_get_periodic_qhead(uint8_t interval)
{
    interval /= 8;

    for (uint8_t i = 0; i < EHCI_PERIOIDIC_QH_NUM - 1; i++) {
        interval >>= 1;
        if (interval == 0) {
            return &g_periodic_qh_head[i];
        }
    }
    return &g_periodic_qh_head[EHCI_PERIOIDIC_QH_NUM - 1];
}

static void ehci_qh_fill(struct ehci_qh_hw *qh,
                         uint8_t dev_addr,
                         uint8_t ep_addr,
                         uint8_t ep_type,
                         uint16_t ep_mps,
                         uint8_t ep_mult,
                         uint8_t ep_interval,
                         uint8_t speed,
                         uint8_t hubaddr,
                         uint8_t hubport)
{
    uint32_t epchar = 0;
    uint32_t epcap = 0;

    /* QH endpoint characteristics:
     *
     * FIELD    DESCRIPTION
     * -------- -------------------------------
     * DEVADDR  Device address
     * I        Inactivate on Next Transaction
     * ENDPT    Endpoint number
     * EPS      Endpoint speed
     * DTC      Data toggle control
     * MAXPKT   Max packet size
     * C        Control endpoint
     * RL       NAK count reloaded
    */

    /* QH endpoint capabilities
     *
     * FIELD    DESCRIPTION
     * -------- -------------------------------
     * SSMASK   Interrupt Schedule Mask
     * SCMASK   Split Completion Mask
     * HUBADDR  Hub Address
     * PORT     Port number
     * MULT     High band width multiplier
     */

    epchar |= ((ep_addr & 0xf) << QH_EPCHAR_ENDPT_SHIFT);
    epchar |= (dev_addr << QH_EPCHAR_DEVADDR_SHIFT);
    epchar |= (ep_mps << QH_EPCHAR_MAXPKT_SHIFT);

    if (ep_type == USB_ENDPOINT_TYPE_CONTROL) {
        epchar |= QH_EPCHAR_DTC; /* toggle from qtd */
    }

    switch (speed) {
        case USB_SPEED_LOW:
            epchar |= QH_EPCHAR_EPS_LOW;
        case USB_SPEED_FULL:
            if (ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                epchar |= QH_EPCHAR_C; /* for TT */
            }

            if (ep_type != USB_ENDPOINT_TYPE_INTERRUPT) {
                epchar |= (EHCI_TUNE_RL_TT << QH_EPCHAR_RL_SHIFT);
            }

            epcap |= QH_EPCAPS_MULT(EHCI_TUNE_MULT_TT);

            epcap |= QH_EPCAPS_HUBADDR(hubaddr);
            epcap |= QH_EPCAPS_PORT(hubport);

            if (ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
                epcap |= QH_EPCAPS_SSMASK(2);
                epcap |= QH_EPCAPS_SCMASK(0x78);
            }

            break;
        case USB_SPEED_HIGH:
            epchar |= QH_EPCHAR_EPS_HIGH;
            if (ep_type == USB_ENDPOINT_TYPE_CONTROL) {
                epchar |= (EHCI_TUNE_RL_HS << QH_EPCHAR_RL_SHIFT);

                epcap |= QH_EPCAPS_MULT(EHCI_TUNE_MULT_HS);
            } else if (ep_type == USB_ENDPOINT_TYPE_BULK) {
                epcap |= QH_EPCAPS_MULT(EHCI_TUNE_MULT_HS);
            } else {
                /* only for interrupt ep */
                epcap |= QH_EPCAPS_MULT(ep_mult);
                epcap |= ehci_caculate_smask(ep_interval);
            }
            break;

        default:
            break;
    }

    qh->hw.epchar = epchar;
    qh->hw.epcap = epcap;
}

static void ehci_qtd_bpl_fill(struct ehci_qtd_hw *qtd, uint32_t bufaddr, size_t buflen)
{
    uint32_t rest;

    qtd->hw.bpl[0] = bufaddr;
    rest = 0x1000 - (bufaddr & 0xfff);

    if (buflen < rest) {
        rest = buflen;
    } else {
        bufaddr += 0x1000;
        bufaddr &= ~0x0fff;

        for (int i = 1; rest < buflen && i < 5; i++) {
            qtd->hw.bpl[i] = bufaddr;
            bufaddr += 0x1000;

            if ((rest + 0x1000) < buflen) {
                rest += 0x1000;
            } else {
                rest = buflen;
            }
        }
    }
}

static void ehci_qtd_fill(struct ehci_qtd_hw *qtd, uint32_t bufaddr, size_t buflen, uint32_t token)
{
    /* qTD token
     *
     * FIELD    DESCRIPTION
     * -------- -------------------------------
     * STATUS   Status
     * PID      PID Code
     * CERR     Error Counter
     * CPAGE    Current Page
     * IOC      Interrupt on complete
     * NBYTES   Total Bytes to Transfer
     * TOGGLE   Data Toggle
     */

    qtd->hw.token = token;

    ehci_qtd_bpl_fill(qtd, bufaddr, buflen);
    qtd->total_len = buflen;
}

static struct ehci_qh_hw *ehci_control_pipe_init(struct ehci_pipe *pipe, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    struct ehci_qh_hw *qh = NULL;
    struct ehci_qtd_hw *qtd_setup = NULL;
    struct ehci_qtd_hw *qtd_data = NULL;
    struct ehci_qtd_hw *qtd_status = NULL;
    uint32_t token;
    size_t flags;

    qh = ehci_qh_alloc();
    if (qh == NULL) {
        return NULL;
    }

    qtd_setup = ehci_qtd_alloc();
    if (buflen > 0) {
        qtd_data = ehci_qtd_alloc();
    }

    qtd_status = ehci_qtd_alloc();
    if (qtd_status == NULL) {
        ehci_qh_free(qh);
        if (qtd_setup) {
            ehci_qtd_free(qtd_setup);
        }
        if (qtd_data) {
            ehci_qtd_free(qtd_data);
        }
        return NULL;
    }

    ehci_qh_fill(qh,
                 pipe->dev_addr,
                 pipe->ep_addr,
                 pipe->ep_type,
                 pipe->ep_mps,
                 0,
                 pipe->ep_interval,
                 pipe->hport->speed,
                 pipe->hport->parent->hub_addr,
                 pipe->hport->port);

    /* fill setup qtd */
    token = QTD_TOKEN_STATUS_ACTIVE |
            QTD_TOKEN_PID_SETUP |
            ((uint32_t)EHCI_TUNE_CERR << QTD_TOKEN_CERR_SHIFT) |
            ((uint32_t)8 << QTD_TOKEN_NBYTES_SHIFT);

    ehci_qtd_fill(qtd_setup, (uintptr_t)setup, 8, token);
    qtd_setup->urb = pipe->urb;

    /* fill data qtd */
    if (setup->wLength > 0) {
        if ((setup->bmRequestType & 0x80) == 0x80) {
            token = QTD_TOKEN_PID_IN;
        } else {
            token = QTD_TOKEN_PID_OUT;
        }
        token |= QTD_TOKEN_STATUS_ACTIVE |
                 QTD_TOKEN_PID_OUT |
                 QTD_TOKEN_TOGGLE |
                 ((uint32_t)EHCI_TUNE_CERR << QTD_TOKEN_CERR_SHIFT) |
                 ((uint32_t)buflen << QTD_TOKEN_NBYTES_SHIFT);

        ehci_qtd_fill(qtd_data, (uintptr_t)buffer, buflen, token);
        qtd_data->urb = pipe->urb;
        qtd_setup->hw.next_qtd = EHCI_PTR2ADDR(qtd_data);
        qtd_data->hw.next_qtd = EHCI_PTR2ADDR(qtd_status);
    } else {
        qtd_setup->hw.next_qtd = EHCI_PTR2ADDR(qtd_status);
    }

    /* fill status qtd */
    if ((setup->bmRequestType & 0x80) == 0x80) {
        token = QTD_TOKEN_PID_OUT;
    } else {
        token = QTD_TOKEN_PID_IN;
    }
    token |= QTD_TOKEN_STATUS_ACTIVE |
             QTD_TOKEN_TOGGLE |
             QTD_TOKEN_IOC |
             ((uint32_t)EHCI_TUNE_CERR << QTD_TOKEN_CERR_SHIFT) |
             ((uint32_t)0 << QTD_TOKEN_NBYTES_SHIFT);

    ehci_qtd_fill(qtd_status, 0, 0, token);
    qtd_status->urb = pipe->urb;
    qtd_status->hw.next_qtd = QTD_LIST_END;

    /* update qh first qtd */
    qh->hw.curr_qtd = EHCI_PTR2ADDR(qtd_setup);
    qh->hw.overlay.next_qtd = EHCI_PTR2ADDR(qtd_setup);

    /* record qh first qtd */
    qh->first_qtd = EHCI_PTR2ADDR(qtd_setup);

    flags = usb_osal_enter_critical_section();

    qh->urb = pipe->urb;
    /* add qh into async list */
    ehci_qh_add_head(&g_async_qh_head, qh);

    EHCI_HCOR->usbcmd |= EHCI_USBCMD_ASEN;

    usb_osal_leave_critical_section(flags);
    return qh;
}

static struct ehci_qh_hw *ehci_bulk_pipe_init(struct ehci_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    struct ehci_qh_hw *qh = NULL;
    struct ehci_qtd_hw *qtd = NULL;
    struct ehci_qtd_hw *first_qtd = NULL;
    struct ehci_qtd_hw *prev_qtd = NULL;
    uint32_t qtd_num = 0;
    uint32_t xfer_len = 0;
    uint32_t token;
    size_t flags;

    qh = ehci_qh_alloc();
    if (qh == NULL) {
        return NULL;
    }

    for (uint32_t i = 0; i < CONFIG_USB_EHCI_QTD_NUM; i++) {
        if (!g_ehci_hcd.ehci_qtd_used[i]) {
            qtd_num++;
        }
    }

    if (qtd_num < ((buflen + 0x3fff) / 0x4000)) {
        ehci_qh_free(qh);
        return NULL;
    }

    ehci_qh_fill(qh,
                 pipe->dev_addr,
                 pipe->ep_addr,
                 pipe->ep_type,
                 pipe->ep_mps,
                 0,
                 pipe->ep_interval,
                 pipe->hport->speed,
                 pipe->hport->parent->hub_addr,
                 pipe->hport->port);

    while (buflen >= 0) {
        qtd = ehci_qtd_alloc();

        if (buflen > 0x4000) {
            xfer_len = 0x4000;
            buflen -= 0x4000;
        } else {
            xfer_len = buflen;
            buflen = 0;
        }

        if (pipe->ep_addr & 0x80) {
            token = QTD_TOKEN_PID_IN;
        } else {
            token = QTD_TOKEN_PID_OUT;
        }

        token |= QTD_TOKEN_STATUS_ACTIVE |
                 ((uint32_t)EHCI_TUNE_CERR << QTD_TOKEN_CERR_SHIFT) |
                 ((uint32_t)xfer_len << QTD_TOKEN_NBYTES_SHIFT);

        if (buflen == 0) {
            token |= QTD_TOKEN_IOC;
        }

        ehci_qtd_fill(qtd, (uintptr_t)buffer, xfer_len, token);
        qtd->urb = pipe->urb;
        qtd->hw.next_qtd = QTD_LIST_END;
        buffer += xfer_len;

        if (prev_qtd) {
            prev_qtd->hw.next_qtd = EHCI_PTR2ADDR(qtd);
        } else {
            first_qtd = qtd;
        }
        prev_qtd = qtd;

        if (buflen == 0) {
            break;
        }
    }

    /* update qh first qtd */
    qh->hw.curr_qtd = EHCI_PTR2ADDR(first_qtd);
    qh->hw.overlay.next_qtd = EHCI_PTR2ADDR(first_qtd);

    /* update data toggle */
    if (pipe->toggle) {
        qh->hw.overlay.token = QTD_TOKEN_TOGGLE;
    } else {
        qh->hw.overlay.token = 0;
    }

    /* record qh first qtd */
    qh->first_qtd = EHCI_PTR2ADDR(first_qtd);

    flags = usb_osal_enter_critical_section();

    qh->urb = pipe->urb;
    /* add qh into async list */
    ehci_qh_add_head(&g_async_qh_head, qh);

    EHCI_HCOR->usbcmd |= EHCI_USBCMD_ASEN;

    usb_osal_leave_critical_section(flags);
    return qh;
}

static struct ehci_qh_hw *ehci_intr_pipe_init(struct ehci_pipe *pipe, uint8_t *buffer, uint32_t buflen)
{
    struct ehci_qh_hw *qh = NULL;
    struct ehci_qtd_hw *qtd = NULL;
    struct ehci_qtd_hw *first_qtd = NULL;
    struct ehci_qtd_hw *prev_qtd = NULL;
    uint32_t qtd_num = 0;
    uint32_t xfer_len = 0;
    uint32_t token;
    size_t flags;

    qh = ehci_qh_alloc();
    if (qh == NULL) {
        return NULL;
    }

    for (uint32_t i = 0; i < CONFIG_USB_EHCI_QTD_NUM; i++) {
        if (!g_ehci_hcd.ehci_qtd_used[i]) {
            qtd_num++;
        }
    }

    if (qtd_num < ((buflen + 0x3fff) / 0x4000)) {
        ehci_qh_free(qh);
        return NULL;
    }

    ehci_qh_fill(qh,
                 pipe->dev_addr,
                 pipe->ep_addr,
                 pipe->ep_type,
                 pipe->ep_mps,
                 pipe->mult + 1,
                 pipe->ep_interval,
                 pipe->hport->speed,
                 pipe->hport->parent->hub_addr,
                 pipe->hport->port);

    while (buflen >= 0) {
        qtd = ehci_qtd_alloc();

        if (buflen > 0x4000) {
            xfer_len = 0x4000;
            buflen -= 0x4000;
        } else {
            xfer_len = buflen;
            buflen = 0;
        }

        if (pipe->ep_addr & 0x80) {
            token = QTD_TOKEN_PID_IN;
        } else {
            token = QTD_TOKEN_PID_OUT;
        }

        token |= QTD_TOKEN_STATUS_ACTIVE |
                 ((uint32_t)EHCI_TUNE_CERR << QTD_TOKEN_CERR_SHIFT) |
                 ((uint32_t)xfer_len << QTD_TOKEN_NBYTES_SHIFT);

        if (buflen == 0) {
            token |= QTD_TOKEN_IOC;
        }

        ehci_qtd_fill(qtd, (uintptr_t)buffer, xfer_len, token);
        qtd->urb = pipe->urb;
        qtd->hw.next_qtd = QTD_LIST_END;
        buffer += xfer_len;

        if (prev_qtd) {
            prev_qtd->hw.next_qtd = EHCI_PTR2ADDR(qtd);
        } else {
            first_qtd = qtd;
        }
        prev_qtd = qtd;

        if (buflen == 0) {
            break;
        }
    }

    /* update qh first qtd */
    qh->hw.curr_qtd = EHCI_PTR2ADDR(first_qtd);
    qh->hw.overlay.next_qtd = EHCI_PTR2ADDR(first_qtd);

    /* update data toggle */
    if (pipe->toggle) {
        qh->hw.overlay.token = QTD_TOKEN_TOGGLE;
    } else {
        qh->hw.overlay.token = 0;
    }

    /* record qh first qtd */
    qh->first_qtd = EHCI_PTR2ADDR(first_qtd);

    flags = usb_osal_enter_critical_section();

    qh->urb = pipe->urb;
    /* add qh into periodic list */
    if (pipe->speed == USB_SPEED_HIGH) {
        ehci_qh_add_head(ehci_get_periodic_qhead(pipe->ep_interval), qh);
    } else {
        ehci_qh_add_head(ehci_get_periodic_qhead(pipe->ep_interval * 8), qh);
    }

    EHCI_HCOR->usbcmd |= EHCI_USBCMD_PSEN;

    usb_osal_leave_critical_section(flags);
    return qh;
}

void ehci_pipe_waitup(struct ehci_pipe *pipe)
{
    struct usbh_urb *urb;

    urb = pipe->urb;
    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        usb_osal_sem_give(pipe->waitsem);
    }

    if (urb->complete) {
        if (urb->errorcode < 0) {
            urb->complete(urb->arg, urb->errorcode);
        } else {
            urb->complete(urb->arg, urb->actual_length);
        }
    }
}

static void ehci_qh_scan_qtds(struct ehci_qh_hw *qh)
{
    struct ehci_qtd_hw *qtd;

    qtd = EHCI_ADDR2QTD(qh->first_qtd);

    while (qtd) {
        // if (qtd->hw.token & QTD_TOKEN_STATUS_ACTIVE) {
        //     continue;
        // }

        qtd->urb->actual_length += (qtd->total_len - ((qtd->hw.token & QTD_TOKEN_NBYTES_MASK) >> QTD_TOKEN_NBYTES_SHIFT));

        ehci_qtd_free(qtd);
        qh->first_qtd = qtd->hw.next_qtd;
        qtd = EHCI_ADDR2QTD(qh->first_qtd);
    }
}

static void ehci_check_qh(struct ehci_qh_hw *qhead, struct ehci_qh_hw *qh)
{
    struct usbh_urb *urb;
    struct ehci_pipe *pipe;
    uint32_t token;

    token = qh->hw.overlay.token;

    if (token & QTD_TOKEN_STATUS_ACTIVE) {
    } else {
        urb = qh->urb;
        pipe = urb->pipe;

        ehci_qh_scan_qtds(qh);
        if (qh->first_qtd & QTD_LIST_END) {
            /* remove qh from list */
            ehci_qh_remove(qhead, qh);

            if ((token & QTD_TOKEN_STATUS_ERRORS) == 0) {
                if (token & QTD_TOKEN_TOGGLE) {
                    pipe->toggle = true;
                } else {
                    pipe->toggle = false;
                }
                urb->errorcode = 0;
            } else {
                if (token & QTD_TOKEN_STATUS_BABBLE) {
                    urb->errorcode = -EPERM;
                    pipe->toggle = 0;
                } else if (token & QTD_TOKEN_STATUS_HALTED) {
                    urb->errorcode = -EPERM;
                    pipe->toggle = 0;
                } else if (token & (QTD_TOKEN_STATUS_DBERR | QTD_TOKEN_STATUS_XACTERR)) {
                    urb->errorcode = -EIO;
                }
            }

            qh->remove_in_iaad = 1;

            EHCI_HCOR->usbcmd |= EHCI_USBCMD_IAAD;
        }
    }
}

static void ehci_kill_qh(struct ehci_qh_hw *qhead, struct ehci_qh_hw *qh)
{
    struct ehci_qtd_hw *qtd;

    ehci_qh_remove(qhead, qh);

    qtd = EHCI_ADDR2QTD(qh->first_qtd);

    while (qtd) {
        ehci_qtd_free(qtd);
        qh->first_qtd = qtd->hw.next_qtd;
        qtd = EHCI_ADDR2QTD(qh->first_qtd);
    }

    ehci_qh_free(qh);
}

static int usbh_reset_port(const uint8_t port)
{
    volatile uint32_t timeout = 0;
    uint32_t regval;

#if defined(CONFIG_USB_EHCI_HPMICRO) && CONFIG_USB_EHCI_HPMICRO
    if ((*(volatile uint32_t *)(CONFIG_HPM_USBH_BASE + 0x224) & 0xc0) == (2 << 6)) { /* Hardcode for hpm */
        EHCI_HCOR->portsc[port - 1] |= (1 << 29);
    } else {
        EHCI_HCOR->portsc[port - 1] &= ~(1 << 29);
    }
#endif
    regval = EHCI_HCOR->portsc[port - 1];
    regval &= ~EHCI_PORTSC_PE;
    regval |= EHCI_PORTSC_RESET;
    EHCI_HCOR->portsc[port - 1] = regval;
    usb_osal_msleep(55);

    EHCI_HCOR->portsc[port - 1] &= ~EHCI_PORTSC_RESET;
    while ((EHCI_HCOR->portsc[port - 1] & EHCI_PORTSC_RESET) != 0) {
        usb_osal_msleep(1);
        timeout++;
        if (timeout > 100) {
            return -ETIMEDOUT;
        }
    }

    return 0;
}

__WEAK void usb_hc_low_level_init(void)
{
}

__WEAK void usb_hc_low_level2_init(void)
{
}

int usb_hc_init(void)
{
    uint32_t interval;
    struct ehci_qh_hw *qh;

    volatile uint32_t timeout = 0;
    uint32_t regval;

    memset(&g_ehci_hcd, 0, sizeof(struct ehci_hcd));

    if (sizeof(struct ehci_qh_hw) % 32) {
        USB_LOG_ERR("struct ehci_qh_hw is not align 32\r\n");
        return -EINVAL;
    }
    if (sizeof(struct ehci_qtd_hw) % 32) {
        USB_LOG_ERR("struct ehci_qtd_hw is not align 32\r\n");
        return -EINVAL;
    }

    for (uint8_t index = 0; index < CONFIG_USB_EHCI_QH_NUM; index++) {
        struct ehci_pipe *pipe;

        pipe = &g_ehci_hcd.pipe_pool[index];
        pipe->waitsem = usb_osal_sem_create(0);
    }

    memset(&g_async_qh_head, 0, sizeof(struct ehci_qh_hw));
    g_async_qh_head.hw.hlp = QH_HLP_QH(&g_async_qh_head);
    g_async_qh_head.hw.epchar = QH_EPCHAR_H;
    g_async_qh_head.hw.overlay.next_qtd = QTD_LIST_END;
    g_async_qh_head.hw.overlay.alt_next_qtd = QTD_LIST_END;
    g_async_qh_head.hw.overlay.token = QTD_TOKEN_STATUS_HALTED;
    g_async_qh_head.first_qtd = QTD_LIST_END;

    memset(g_framelist, 0, sizeof(uint32_t) * CONFIG_USB_EHCI_FRAME_LIST_SIZE);

    for (int i = EHCI_PERIOIDIC_QH_NUM - 1; i >= 0; i--) {
        memset(&g_periodic_qh_head[i], 0, sizeof(struct ehci_qh_hw));
        g_periodic_qh_head[i].hw.hlp = QH_HLP_END;
        g_periodic_qh_head[i].hw.epchar = QH_EPCAPS_SSMASK(1);
        g_periodic_qh_head[i].hw.overlay.next_qtd = QTD_LIST_END;
        g_periodic_qh_head[i].hw.overlay.alt_next_qtd = QTD_LIST_END;
        g_periodic_qh_head[i].hw.overlay.token = QTD_TOKEN_STATUS_HALTED;
        g_periodic_qh_head[i].first_qtd = QTD_LIST_END;

        interval = 1 << i;
        for (uint32_t j = interval - 1; j < CONFIG_USB_EHCI_FRAME_LIST_SIZE; j += interval) {
            if (g_framelist[j] == 0) {
                g_framelist[j] = QH_HLP_QH(&g_periodic_qh_head[i]);
            } else {
                qh = EHCI_ADDR2QH(g_framelist[j]);
                while (1) {
                    if (qh == &g_periodic_qh_head[i]) {
                        break;
                    }
                    if (qh->hw.hlp == QH_HLP_END) {
                        qh->hw.hlp = QH_HLP_QH(&g_periodic_qh_head[i]);
                        break;
                    }

                    qh = EHCI_ADDR2QH(qh->hw.hlp);
                }
            }
        }
    }

    usb_hc_low_level_init();

    EHCI_HCOR->usbcmd |= EHCI_USBCMD_HCRESET;
    while (EHCI_HCOR->usbcmd & EHCI_USBCMD_HCRESET) {
        usb_osal_msleep(1);
        timeout++;
        if (timeout > 100) {
            return -ETIMEDOUT;
        }
    }

    usb_hc_low_level2_init();

    EHCI_HCOR->usbintr = 0;
    EHCI_HCOR->usbsts = EHCI_HCOR->usbsts;
#if CONFIG_USB_EHCI_HCCR_BASE != 0
    USB_LOG_INFO("EHCI HCIVERSION:%04x\r\n", (int)EHCI_HCCR->hciversion);
    USB_LOG_INFO("EHCI HCSPARAMS:%06x\r\n", (int)EHCI_HCCR->hcsparams);
    USB_LOG_INFO("EHCI HCCPARAMS:%04x\r\n", (int)EHCI_HCCR->hccparams);
#endif
    /* Set the Current Asynchronous List Address. */
    EHCI_HCOR->asynclistaddr = EHCI_PTR2ADDR(&g_async_qh_head);
    /* Set the Periodic Frame List Base Address. */
    EHCI_HCOR->periodiclistbase = EHCI_PTR2ADDR(g_framelist);

    regval = 0;
#if CONFIG_USB_EHCI_FRAME_LIST_SIZE == 1024
    regval |= EHCI_USBCMD_FLSIZE_1024;
#elif CONFIG_USB_EHCI_FRAME_LIST_SIZE == 512
    regval |= EHCI_USBCMD_FLSIZE_512;
#elif CONFIG_USB_EHCI_FRAME_LIST_SIZE == 256
    regval |= EHCI_USBCMD_FLSIZE_256;
#else
#error Unsupported frame size list size
#endif

    regval |= EHCI_USBCMD_ITHRE_1MF;
    regval |= EHCI_USBCMD_ASEN;
    regval |= EHCI_USBCMD_PSEN;
    regval |= EHCI_USBCMD_RUN;
    EHCI_HCOR->usbcmd = regval;

#ifdef CONFIG_USB_EHCI_CONFIGFLAG
    EHCI_HCOR->configflag = EHCI_CONFIGFLAG;
#endif
    /* Wait for the EHCI to run (no longer report halted) */
    timeout = 0;
    while (EHCI_HCOR->usbsts & EHCI_USBSTS_HALTED) {
        usb_osal_msleep(1);
        timeout++;
        if (timeout > 100) {
            return -ETIMEDOUT;
        }
    }
#ifdef CONFIG_USB_EHCI_PORT_POWER
    for (uint8_t port = 0; port < CONFIG_USBHOST_MAX_RHPORTS; port++) {
        regval = EHCI_HCOR->portsc[port];
        regval |= EHCI_PORTSC_PP;
        EHCI_HCOR->portsc[port] = regval;
    }
#endif

    /* Enable EHCI interrupts. */
    EHCI_HCOR->usbintr = EHCI_USBIE_INT | EHCI_USBIE_ERR | EHCI_USBIE_PCD | EHCI_USBIE_FATAL | EHCI_USBIE_IAA;
    return 0;
}

uint16_t usbh_get_frame_number(void)
{
    return (((EHCI_HCOR->frindex & EHCI_FRINDEX_MASK) >> 3) & 0x3ff);
}

int usbh_roothub_control(struct usb_setup_packet *setup, uint8_t *buf)
{
    uint8_t nports;
    uint8_t port;
    uint32_t temp, status;

    nports = CONFIG_USBHOST_MAX_RHPORTS;

    port = setup->wIndex;
    if (setup->bmRequestType & USB_REQUEST_RECIPIENT_DEVICE) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                switch (setup->wValue) {
                    case HUB_FEATURE_HUB_C_LOCALPOWER:
                        break;
                    case HUB_FEATURE_HUB_C_OVERCURRENT:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_DESCRIPTOR:
                break;
            case HUB_REQUEST_GET_STATUS:
                memset(buf, 0, 4);
                break;
            default:
                break;
        }
    } else if (setup->bmRequestType & USB_REQUEST_RECIPIENT_OTHER) {
        switch (setup->bRequest) {
            case HUB_REQUEST_CLEAR_FEATURE:
                if (!port || port > nports) {
                    return -EPIPE;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_ENABLE:
                        EHCI_HCOR->portsc[port - 1] &= ~EHCI_PORTSC_PE;
                        break;
                    case HUB_PORT_FEATURE_SUSPEND:
                    case HUB_PORT_FEATURE_C_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
#ifdef CONFIG_USB_EHCI_PORT_POWER
                        EHCI_HCOR->portsc[port - 1] &= ~EHCI_PORTSC_PP;
#endif
                        break;
                    case HUB_PORT_FEATURE_C_CONNECTION:
                        EHCI_HCOR->portsc[port - 1] |= EHCI_PORTSC_CSC;
                        break;
                    case HUB_PORT_FEATURE_C_ENABLE:
                        EHCI_HCOR->portsc[port - 1] |= EHCI_PORTSC_PEC;
                        break;
                    case HUB_PORT_FEATURE_C_OVER_CURREN:
                        EHCI_HCOR->portsc[port - 1] |= EHCI_PORTSC_OCC;
                        break;
                    case HUB_PORT_FEATURE_C_RESET:
                        break;
                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_SET_FEATURE:
                if (!port || port > nports) {
                    return -EPIPE;
                }

                switch (setup->wValue) {
                    case HUB_PORT_FEATURE_SUSPEND:
                        break;
                    case HUB_PORT_FEATURE_POWER:
#ifdef CONFIG_USB_EHCI_PORT_POWER
                        EHCI_HCOR->portsc[port - 1] |= EHCI_PORTSC_PP;
#endif
                        break;
                    case HUB_PORT_FEATURE_RESET:
                        usbh_reset_port(port);
                        break;

                    default:
                        return -EPIPE;
                }
                break;
            case HUB_REQUEST_GET_STATUS:
                if (!port || port > nports) {
                    return -EPIPE;
                }
                temp = EHCI_HCOR->portsc[port - 1];

                status = 0;
                if (temp & EHCI_PORTSC_CSC) {
                    status |= (1 << HUB_PORT_FEATURE_C_CONNECTION);
                }
                if (temp & EHCI_PORTSC_PEC) {
                    status |= (1 << HUB_PORT_FEATURE_C_ENABLE);
                }
                if (temp & EHCI_PORTSC_OCC) {
                    status |= (1 << HUB_PORT_FEATURE_C_OVER_CURREN);
                }

                if (temp & EHCI_PORTSC_CCS) {
                    status |= (1 << HUB_PORT_FEATURE_CONNECTION);
                }
                if (temp & EHCI_PORTSC_PE) {
                    status |= (1 << HUB_PORT_FEATURE_ENABLE);

                    if (usbh_get_port_speed(port) == USB_SPEED_LOW) {
                        status |= (1 << HUB_PORT_FEATURE_LOWSPEED);
                    } else if (usbh_get_port_speed(port) == USB_SPEED_HIGH) {
                        status |= (1 << HUB_PORT_FEATURE_HIGHSPEED);
                    }
                }
                if (temp & EHCI_PORTSC_SUSPEND) {
                    status |= (1 << HUB_PORT_FEATURE_SUSPEND);
                }
                if (temp & EHCI_PORTSC_OCA) {
                    status |= (1 << HUB_PORT_FEATURE_OVERCURRENT);
                }
                if (temp & EHCI_PORTSC_RESET) {
                    status |= (1 << HUB_PORT_FEATURE_RESET);
                }
                if (temp & EHCI_PORTSC_PP) {
                    status |= (1 << HUB_PORT_FEATURE_POWER);
                }
                memcpy(buf, &status, 4);
                break;
            default:
                break;
        }
    }
    return 0;
}

int usbh_ep_pipe_reconfigure(usbh_pipe_t pipe, uint8_t dev_addr, uint8_t ep_mps, uint8_t mult)
{
    struct ehci_pipe *ppipe = (struct ehci_pipe *)pipe;

    ppipe->dev_addr = dev_addr;
    ppipe->ep_mps = ep_mps;
    ppipe->mult = mult;

    return 0;
}

int usbh_pipe_alloc(usbh_pipe_t *pipe, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct ehci_pipe *ppipe;
    usb_osal_sem_t waitsem;

    ppipe = ehci_pipe_alloc();
    if (ppipe == NULL) {
        return -ENOMEM;
    }

    /* store variables */
    waitsem = ppipe->waitsem;

    memset(ppipe, 0, sizeof(struct ehci_pipe));

    ppipe->ep_addr = ep_cfg->ep_addr;
    ppipe->ep_type = ep_cfg->ep_type;
    ppipe->ep_mps = ep_cfg->ep_mps;
    ppipe->ep_interval = ep_cfg->ep_interval;
    ppipe->mult = ep_cfg->mult;
    ppipe->speed = ep_cfg->hport->speed;
    ppipe->dev_addr = ep_cfg->hport->dev_addr;
    ppipe->hport = ep_cfg->hport;

#ifdef CONFIG_USB_EHCI_ISO
    if ((ppipe->speed == USB_SPEED_HIGH) && (ppipe->ep_type == USB_ENDPOINT_TYPE_ISOCHRONOUS)) {
        if (ep_cfg->ep_interval == 0x01) { /* transfer interval 1 mf */
            ppipe->mf_unmask = 0xff;
            ppipe->mf_valid = 8;
        } else if (ep_cfg->ep_interval == 0x02) { /* transfer interval 2 mf */
            ppipe->mf_unmask = 0x55;
            ppipe->mf_valid = 4;
        } else if (ep_cfg->ep_interval == 0x03) { /* transfer interval 4 mf */
            ppipe->mf_unmask = 0x44;
            ppipe->mf_valid = 2;
        } else if (ep_cfg->ep_interval == 0x04) { /* transfer interval 8 mf */
            ppipe->mf_unmask = 0x01;
            ppipe->mf_valid = 1;
        }
    }
#endif
    /* restore variable */
    ppipe->inuse = true;
    ppipe->waitsem = waitsem;

    *pipe = (usbh_pipe_t)ppipe;

    return 0;
}

int usbh_pipe_free(usbh_pipe_t pipe)
{
    struct usbh_urb *urb;
    struct ehci_pipe *ppipe;

    ppipe = (struct ehci_pipe *)pipe;

    if (!ppipe) {
        return -EINVAL;
    }

    urb = ppipe->urb;

    if (urb) {
        usbh_kill_urb(urb);
    }

    ehci_pipe_free(ppipe);
    return 0;
}

int usbh_submit_urb(struct usbh_urb *urb)
{
    struct ehci_pipe *pipe;
    struct ehci_qh_hw *qh = NULL;
    size_t flags;
    int ret = 0;

    if (!urb || !urb->pipe) {
        return -EINVAL;
    }

    pipe = urb->pipe;

    if (!pipe->inuse || !(EHCI_HCOR->portsc[0] & EHCI_PORTSC_CCS) || !pipe->hport->connected) {
        return -ENODEV;
    }

    if (pipe->urb && (pipe->ep_type != USB_ENDPOINT_TYPE_ISOCHRONOUS)) {
        return -EBUSY;
    }

    flags = usb_osal_enter_critical_section();

    pipe->waiter = false;
    pipe->xfrd = 0;
    pipe->urb = urb;
    urb->errorcode = -EBUSY;
    urb->actual_length = 0;

    if (urb->timeout > 0) {
        pipe->waiter = true;
    }
    usb_osal_leave_critical_section(flags);

    switch (pipe->ep_type) {
        case USB_ENDPOINT_TYPE_CONTROL:
            qh = ehci_control_pipe_init(pipe, urb->setup, urb->transfer_buffer, urb->transfer_buffer_length);
            if (qh == NULL) {
                return -ENOMEM;
            }
            break;
        case USB_ENDPOINT_TYPE_BULK:
            qh = ehci_bulk_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
            if (qh == NULL) {
                return -ENOMEM;
            }
            break;
        case USB_ENDPOINT_TYPE_INTERRUPT:
            qh = ehci_intr_pipe_init(pipe, urb->transfer_buffer, urb->transfer_buffer_length);
            if (qh == NULL) {
                return -ENOMEM;
            }
            break;
        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
#ifdef CONFIG_USB_EHCI_ISO
            ehci_iso_pipe_init(pipe, urb);
#endif
            break;
        default:
            break;
    }

    if (urb->timeout > 0) {
        /* wait until timeout or sem give */
        ret = usb_osal_sem_take(pipe->waitsem, urb->timeout);
        if (ret < 0) {
            goto errout_timeout;
        }
        ret = urb->errorcode;
    }
    return ret;
errout_timeout:
    /* Timeout will run here */
    pipe->waiter = false;
    usbh_kill_urb(urb);
    return ret;
}

int usbh_kill_urb(struct usbh_urb *urb)
{
    struct ehci_pipe *pipe;
    struct ehci_qh_hw *qh = NULL;

    size_t flags;

    if (!urb) {
        return -EINVAL;
    }

    pipe = urb->pipe;

    if (!pipe) {
        return -EINVAL;
    }

    flags = usb_osal_enter_critical_section();

    EHCI_HCOR->usbcmd &= ~(EHCI_USBCMD_PSEN | EHCI_USBCMD_ASEN);

    if ((pipe->ep_type == USB_ENDPOINT_TYPE_CONTROL) || (pipe->ep_type == USB_ENDPOINT_TYPE_BULK)) {
        qh = EHCI_ADDR2QH(g_async_qh_head.hw.hlp);
        while ((qh != &g_async_qh_head) && qh) {
            if (qh->urb == urb) {
                ehci_kill_qh(&g_async_qh_head, qh);
            }
            qh = EHCI_ADDR2QH(qh->hw.hlp);
        }
    } else if (pipe->ep_type == USB_ENDPOINT_TYPE_INTERRUPT) {
        qh = EHCI_ADDR2QH(g_periodic_qh_head[EHCI_PERIOIDIC_QH_NUM - 1].hw.hlp);
        while (qh) {
            if (qh->urb == urb) {
                if (pipe->speed == USB_SPEED_HIGH) {
                    ehci_kill_qh(ehci_get_periodic_qhead(pipe->ep_interval), qh);
                } else {
                    ehci_kill_qh(ehci_get_periodic_qhead(pipe->ep_interval * 8), qh);
                }
            }
            qh = EHCI_ADDR2QH(qh->hw.hlp);
        }
    } else {
#ifdef CONFIG_USB_EHCI_ISO
        ehci_remove_itd_urb(urb);
#endif
    }

    EHCI_HCOR->usbcmd |= (EHCI_USBCMD_PSEN | EHCI_USBCMD_ASEN);

    pipe->urb = NULL;

    if (pipe->waiter) {
        pipe->waiter = false;
        urb->errorcode = -ESHUTDOWN;
        usb_osal_sem_give(pipe->waitsem);
    }

    usb_osal_leave_critical_section(flags);

    return 0;
}

static void ehci_scan_async_list(void)
{
    struct ehci_qh_hw *qh;

    qh = EHCI_ADDR2QH(g_async_qh_head.hw.hlp);
    while ((qh != &g_async_qh_head) && qh) {
        if (qh->urb) {
            ehci_check_qh(&g_async_qh_head, qh);
        }
        qh = EHCI_ADDR2QH(qh->hw.hlp);
    }
}

static void ehci_scan_periodic_list(void)
{
    struct ehci_qh_hw *qh;
    struct ehci_pipe *pipe;

    qh = EHCI_ADDR2QH(g_periodic_qh_head[EHCI_PERIOIDIC_QH_NUM - 1].hw.hlp);
    while (qh) {
        if (qh->urb && qh->urb->pipe) {
            pipe = (struct ehci_pipe *)qh->urb->pipe;
            if (pipe->speed == USB_SPEED_HIGH) {
                ehci_check_qh(ehci_get_periodic_qhead(pipe->ep_interval), qh);
            } else {
                ehci_check_qh(ehci_get_periodic_qhead(pipe->ep_interval * 8), qh);
            }
        }
        qh = EHCI_ADDR2QH(qh->hw.hlp);
    }
}

void USBH_IRQHandler(void)
{
    uint32_t usbsts;

    usbsts = EHCI_HCOR->usbsts & EHCI_HCOR->usbintr;
    EHCI_HCOR->usbsts = usbsts;

    if (usbsts & EHCI_USBSTS_INT) {
        ehci_scan_async_list();
        ehci_scan_periodic_list();
#ifdef CONFIG_USB_EHCI_ISO
        ehci_scan_isochronous_list();
#endif
    }

    if (usbsts & EHCI_USBSTS_ERR) {
        ehci_scan_async_list();
        ehci_scan_periodic_list();
#ifdef CONFIG_USB_EHCI_ISO
        ehci_scan_isochronous_list();
#endif
    }

    if (usbsts & EHCI_USBSTS_PCD) {
        for (int port = 0; port < CONFIG_USBHOST_MAX_RHPORTS; port++) {
            uint32_t portsc = EHCI_HCOR->portsc[port];

            if (portsc & EHCI_PORTSC_CSC) {
                if ((portsc & EHCI_PORTSC_CCS) == EHCI_PORTSC_CCS) {
                } else {
                    for (uint8_t index = 0; index < CONFIG_USB_EHCI_QH_NUM; index++) {
                        g_ehci_hcd.ehci_qh_used[index] = false;
                    }
                    for (uint8_t index = 0; index < CONFIG_USB_EHCI_QTD_NUM; index++) {
                        g_ehci_hcd.ehci_qtd_used[index] = false;
                    }
                    for (uint8_t index = 0; index < CONFIG_USB_EHCI_ITD_NUM; index++) {
                        g_ehci_hcd.ehci_itd_used[index] = false;
                    }
                }

                usbh_roothub_thread_wakeup(port + 1);
            }
        }
    }

    if (usbsts & EHCI_USBSTS_IAA) {
        for (uint8_t index = 0; index < CONFIG_USB_EHCI_QH_NUM; index++) {
            struct ehci_qh_hw *qh = &ehci_qh_pool[index];
            if (g_ehci_hcd.ehci_qh_used[index] && qh->remove_in_iaad) {
                struct usbh_urb *urb;
                struct ehci_pipe *pipe;

                urb = qh->urb;
                pipe = urb->pipe;

                qh->remove_in_iaad = 0;
                ehci_qh_free(qh);

                ehci_pipe_waitup(pipe);
            }
        }
    }

    if (usbsts & EHCI_USBSTS_FATAL) {
    }
}