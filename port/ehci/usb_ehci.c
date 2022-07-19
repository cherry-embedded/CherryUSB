#include "usbh_core.h"
#include "usb_ehci.h"

#ifndef USBH_IRQHandler
#define USBH_IRQHandler USBH_IRQHandler
#endif

#define DEBUGASSERT(f)

/* Configurable number of Queue Head (QH) structures.  The default is one per
 * Root hub port plus one for EP0.
 */

#ifndef CONFIG_USB_EHCI_QH_NUM
#define CONFIG_USB_EHCI_QH_NUM (CONFIG_USBHOST_RHPORTS + 1)
#endif

/* Configurable number of Queue Element Transfer Descriptor (qTDs).  The
 * default is one per root hub plus three from EP0.
 */

#ifndef CONFIG_USB_EHCI_QTD_NUM
#define CONFIG_USB_EHCI_QTD_NUM (CONFIG_USBHOST_RHPORTS + 3)
#endif

/* Registers ****************************************************************/

/* Traditionally, NuttX specifies register locations using individual
 * register offsets from a base address.  That tradition is broken here and,
 * instead, register blocks are represented as structures.  This is done here
 * because, in principle, EHCI operational register address may not be known
 * at compile time; the operational registers lie at an offset specified in
 * the 'caplength' byte of the Host Controller Capability Registers.
 *
 * However, for the case of the LPC43 EHCI, we know apriori that locations
 * of these register blocks.
 */

/* Host Controller Capability Registers */

#define HCCR ((struct ehci_hccr_s *)CONFIG_USB_EHCI_HCCR_BASE)

/* Host Controller Operational Registers */

#define HCOR ((volatile struct ehci_hcor_s *)CONFIG_USB_EHCI_HCOR_BASE)

/* Interrupts ***************************************************************/

/* This is the set of interrupts handled by this driver */

#define EHCI_HANDLED_INTS (EHCI_INT_USBINT | EHCI_INT_USBERRINT | \
                           EHCI_INT_PORTSC | EHCI_INT_SYSERROR |  \
                           EHCI_INT_AAINT)

/* The periodic frame list is a 4K-page aligned array of Frame List Link
 * pointers. The length of the frame list may be programmable.  The
 * programmability of the periodic frame list is exported to system software
 * via the HCCPARAMS register. If non-programmable, the length is 1024
 * elements. If programmable, the length can be selected by system software
 * as one of 256, 512, or 1024 elements.
 */

#define FRAME_LIST_SIZE 1024

/* DMA **********************************************************************/

/* For now, we are assuming an identity mapping between physical and virtual
 * address spaces.
 */

#define usb_ehci_physramaddr(a) (a)
#define usb_ehci_virtramaddr(a) (a)

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* Internal representation of the EHCI Queue Head (QH) */

struct usb_ehci_epinfo_s;
struct usb_ehci_qh_s {
    /* Fields visible to hardware */

    struct ehci_qh_s hw; /* Hardware representation of the queue head */

    /* Internal fields used by the EHCI driver */

    struct usb_ehci_epinfo_s *epinfo; /* Endpoint used for the transfer */
    uint32_t fqp;                     /* First qTD in the list (physical address) */
#if (CONFIG_DCACHE_LINE_SIZE == 64)
    uint8_t pad[4]; /* Padding to assure 64-byte alignment */
#else
    uint8_t pad[8]; /* Padding to assure 32-byte alignment */
#endif
};

/* Internal representation of the EHCI Queue Element Transfer Descriptor
 * (qTD)
 */

struct usb_ehci_qtd_s {
    /* Fields visible to hardware */

    struct ehci_qtd_s hw; /* Hardware representation of the queue head */

    /* Internal fields used by the EHCI driver */
};

/* The following is used to manage lists of free QHs and qTDs */

struct usb_ehci_list_s {
    struct usb_ehci_list_s *flink; /* Link to next entry in the list */
                                   /* Variable length entry data follows */
};

/* List traversal callout functions */

typedef int (*foreach_qh_t)(struct usb_ehci_qh_s *qh, uint32_t **bp, void *arg);
typedef int (*foreach_qtd_t)(struct usb_ehci_qtd_s *qtd, uint32_t **bp, void *arg);

/* This structure describes one endpoint. */

struct usb_ehci_epinfo_s {
    uint8_t epno    : 7; /* Endpoint number */
    uint8_t dirin   : 1; /* 1:IN endpoint 0:OUT endpoint */
    uint8_t devaddr : 7; /* Device address */
    uint8_t toggle  : 1; /* Next data toggle */
#ifndef CONFIG_USBHOST_INT_DISABLE
    uint8_t interval; /* Polling interval */
#endif
    bool inuse;
    uint8_t status;           /* Retained token status bits (for debug purposes) */
    uint16_t maxpacket : 11;  /* Maximum packet size */
    uint16_t xfrtype   : 2;   /* See USB_EP_ATTR_XFER_* definitions in usb.h */
    uint16_t speed     : 2;   /* See USB_*_SPEED definitions in ehci.h */
    int result;               /* The result of the transfer */
    uint32_t xfrd;            /* On completion, will hold the number of bytes transferred */
    volatile bool iocwait;    /* TRUE: Thread is waiting for transfer completion */
    usb_osal_sem_t iocsem;    /* Semaphore used to wait for transfer completion */
    usb_osal_mutex_t exclsem; /* Support mutually exclusive access */
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback; /* Transfer complete callback */
    void *arg;                       /* Argument that accompanies the callback */
#endif
    struct usbh_hubport *hport;
    struct usb_ehci_qh_s *qh;
};

/* This structure retains the overall state of the USB host controller */

struct ehci_hcd {
    struct usb_ehci_list_s *qhfree;                                                      /* List of free Queue Head (QH) structures */
    struct usb_ehci_list_s *qtdfree;                                                     /* List of free Queue Element Transfer Descriptor (qTD) */
    __attribute__((aligned(32))) struct usb_ehci_qh_s qhpool[CONFIG_USB_EHCI_QH_NUM];    /* Queue Head (QH) pool */
    __attribute__((aligned(32))) struct usb_ehci_qtd_s qtdpool[CONFIG_USB_EHCI_QTD_NUM]; /* Queue Element Transfer Descriptor (qTD) pool */
    struct usb_ehci_epinfo_s chan[CONFIG_USBHOST_PIPE_NUM];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/* Register operations ******************************************************/

static uint16_t usb_ehci_read16(const uint8_t *addr);
static uint32_t usb_ehci_read32(const uint8_t *addr);
#if 0 /* Not used */
static void usb_ehci_write16(uint16_t memval, uint8_t *addr);
static void usb_ehci_write32(uint32_t memval, uint8_t *addr);
#endif

#ifdef CONFIG_ENDIAN_BIG
static uint16_t usb_ehci_swap16(uint16_t value);
static uint32_t usb_ehci_swap32(uint32_t value);
#else
#define usb_ehci_swap16(value) (value)
#define usb_ehci_swap32(value) (value)
#endif

#ifdef CONFIG_USB_DCACHE_ENABLE
void usb_ehci_dcache_clean(uintptr_t addr, uint32_t len);
void usb_ehci_dcache_invalidate(uintptr_t addr, uint32_t len);
void usb_ehci_dcache_clean_invalidate(uintptr_t addr, uint32_t len);
#else
#define usb_ehci_dcache_clean(addr, len)
#define usb_ehci_dcache_invalidate(addr, len)
#define usb_ehci_dcache_clean_invalidate(addr, len)
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* In this driver implementation, support is provided for only a single
 * USB device.  All status information can be simply retained in a single
 * global instance.
 */

/* Maps USB chapter 9 speed to EHCI speed */

static const uint8_t g_ehci_speed[4] = {
    0, EHCI_LOW_SPEED, EHCI_FULL_SPEED, EHCI_HIGH_SPEED
};

/* In this driver implementation, support is provided for only a single
 * USB device.  All status information can be simply retained in a single
 * global instance.
 */

static struct ehci_hcd g_ehci_hcd;

/* The head of the asynchronous queue */

static struct usb_ehci_qh_s g_asynchead __attribute__((aligned(32)));

/* The head of the periodic queue */

static struct usb_ehci_qh_s g_intrhead __attribute__((aligned(32)));

/* The frame list */
static uint32_t g_framelist[FRAME_LIST_SIZE] __attribute__((aligned(4096)));
/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: usb_ehci_read16
 *
 * Description:
 *   Read 16-bit little endian data
 *
 ****************************************************************************/

static uint16_t usb_ehci_read16(const uint8_t *addr)
{
#ifdef CONFIG_ENDIAN_BIG
    return (uint16_t)addr[0] << 8 | (uint16_t)addr[1];
#else
    return (uint16_t)addr[1] << 8 | (uint16_t)addr[0];
#endif
}

/****************************************************************************
 * Name: usb_ehci_read32
 *
 * Description:
 *   Read 32-bit little endian data
 *
 ****************************************************************************/

static inline uint32_t usb_ehci_read32(const uint8_t *addr)
{
#ifdef CONFIG_ENDIAN_BIG
    return (uint32_t)usb_ehci_read16(&addr[0]) << 16 |
           (uint32_t)usb_ehci_read16(&addr[2]);
#else
    return (uint32_t)usb_ehci_read16(&addr[2]) << 16 |
           (uint32_t)usb_ehci_read16(&addr[0]);
#endif
}

/****************************************************************************
 * Name: usb_ehci_swap16
 *
 * Description:
 *   Swap bytes on a 16-bit value
 *
 ****************************************************************************/

#ifdef CONFIG_ENDIAN_BIG
static uint16_t usb_ehci_swap16(uint16_t value)
{
    return ((value >> 8) & 0xff) | ((value & 0xff) << 8);
}
#endif

/****************************************************************************
 * Name: usb_ehci_swap32
 *
 * Description:
 *   Swap bytes on a 32-bit value
 *
 ****************************************************************************/

#ifdef CONFIG_ENDIAN_BIG
static uint32_t usb_ehci_swap32(uint32_t value)
{
    return (uint32_t)usb_ehci_swap16((uint16_t)((value >> 16) & 0xffff)) |
           (uint32_t)usb_ehci_swap16((uint16_t)(value & 0xffff)) << 16;
}
#endif

static inline void usb_ehci_putreg(uint32_t regval, volatile uint32_t *regaddr)
{
    *regaddr = regval;
}

static inline uint32_t usb_ehci_getreg(volatile uint32_t *regaddr)
{
    return *regaddr;
}

static int usb_ehci_chan_alloc(void)
{
    int chidx;

    /* Search the table of channels */

    for (chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
        /* Is this channel available? */
        if (!g_ehci_hcd.chan[chidx].inuse) {
            /* Yes... make it "in use" and return the index */

            g_ehci_hcd.chan[chidx].inuse = true;
            return chidx;
        }
    }

    /* All of the channels are "in-use" */

    return -EBUSY;
}

static void usb_ehci_chan_free(struct usb_ehci_epinfo_s *chan)
{
    /* Mark the channel available */
    chan->inuse = false;
}
/****************************************************************************
 * Name: usb_ehci_qh_alloc
 *
 * Description:
 *   Allocate a Queue Head (QH) structure by removing it from the free list
 *
 * Assumption:  Caller holds the exclsem
 *
 ****************************************************************************/

static struct usb_ehci_qh_s *usb_ehci_qh_alloc(void)
{
    struct usb_ehci_qh_s *qh;

    /* Remove the QH structure from the freelist */

    qh = (struct usb_ehci_qh_s *)g_ehci_hcd.qhfree;
    if (qh) {
        g_ehci_hcd.qhfree = ((struct usb_ehci_list_s *)qh)->flink;
        memset(qh, 0, sizeof(struct usb_ehci_qh_s));
    }

    return qh;
}

/****************************************************************************
 * Name: usb_ehci_qh_free
 *
 * Description:
 *   Free a Queue Head (QH) structure by returning it to the free list
 *
 * Assumption:  Caller holds the exclsem
 *
 ****************************************************************************/

static void usb_ehci_qh_free(struct usb_ehci_qh_s *qh)
{
    struct usb_ehci_list_s *entry = (struct usb_ehci_list_s *)qh;

    /* Put the QH structure back into the free list */

    entry->flink = g_ehci_hcd.qhfree;
    g_ehci_hcd.qhfree = entry;
}

/****************************************************************************
 * Name: usb_ehci_qtd_alloc
 *
 * Description:
 *   Allocate a Queue Element Transfer Descriptor (qTD) by removing it from
 *   the free list
 *
 * Assumption:  Caller holds the exclsem
 *
 ****************************************************************************/

static struct usb_ehci_qtd_s *usb_ehci_qtd_alloc(void)
{
    struct usb_ehci_qtd_s *qtd;

    /* Remove the qTD from the freelist */

    qtd = (struct usb_ehci_qtd_s *)g_ehci_hcd.qtdfree;
    if (qtd) {
        g_ehci_hcd.qtdfree = ((struct usb_ehci_list_s *)qtd)->flink;
        memset(qtd, 0, sizeof(struct usb_ehci_qtd_s));
    }

    return qtd;
}

/****************************************************************************
 * Name: usb_ehci_qtd_free
 *
 * Description:
 *   Free a Queue Element Transfer Descriptor (qTD) by returning it to the
 *   free list
 *
 * Assumption:  Caller holds the exclsem
 *
 ****************************************************************************/

static void usb_ehci_qtd_free(struct usb_ehci_qtd_s *qtd)
{
    struct usb_ehci_list_s *entry = (struct usb_ehci_list_s *)qtd;

    /* Put the qTD back into the free list */

    entry->flink = g_ehci_hcd.qtdfree;
    g_ehci_hcd.qtdfree = entry;
}

/****************************************************************************
 * Name: usb_ehci_qh_foreach
 *
 * Description:
 *   Give the first entry in a list of Queue Head (QH) structures, call the
 *   handler for each QH structure in the list (including the one at the head
 *   of the list).
 *
 ****************************************************************************/

static int usb_ehci_qh_foreach(struct usb_ehci_qh_s *qh, uint32_t **bp,
                               foreach_qh_t handler, void *arg)
{
    struct usb_ehci_qh_s *next;
    uintptr_t physaddr;
    int ret;

    DEBUGASSERT(qh && handler);
    while (qh) {
        /* Is this the end of the list?  Check the horizontal link pointer
       * (HLP) terminate (T) bit.  If T==1, then the HLP address is not
       * valid.
       */

        physaddr = usb_ehci_swap32(qh->hw.hlp);

        if ((physaddr & QH_HLP_T) != 0) {
            /* Set the next pointer to NULL.  This will terminate the loop. */

            next = NULL;
        }

        /* Is the next QH the asynchronous list head which will always be at
       * the end of the asynchronous queue?
       */

        else if (usb_ehci_virtramaddr(physaddr & QH_HLP_MASK) ==
                 (uintptr_t)&g_asynchead) {
            /* That will also terminate the loop */

            next = NULL;
        }

        /* Otherwise, there is a QH structure after this one that describes
       * another transaction.
       */

        else {
            physaddr = usb_ehci_swap32(qh->hw.hlp) & QH_HLP_MASK;
            next = (struct usb_ehci_qh_s *)usb_ehci_virtramaddr(physaddr);
        }

        /* Perform the user action on this entry.  The action might result in
       * unlinking the entry!  But that is okay because we already have the
       * next QH pointer.
       *
       * Notice that we do not manage the back pointer (bp).  If the callout
       * uses it, it must update it as necessary.
       */

        ret = handler(qh, bp, arg);

        /* If the handler returns any non-zero value, then terminate the
       * traversal early.
       */

        if (ret != 0) {
            return ret;
        }

        /* Set up to visit the next entry */

        qh = next;
    }

    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qtd_foreach
 *
 * Description:
 *   Give a Queue Head (QH) instance, call the handler for each qTD structure
 *   in the queue.
 *
 ****************************************************************************/

static int usb_ehci_qtd_foreach(struct usb_ehci_qh_s *qh, foreach_qtd_t handler,
                                void *arg)
{
    struct usb_ehci_qtd_s *qtd;
    struct usb_ehci_qtd_s *next;
    uintptr_t physaddr;
    uint32_t *bp;
    int ret;

    DEBUGASSERT(qh && handler);

    /* Handle the special case where the queue is empty */

    bp = &qh->fqp;                   /* Start of qTDs in original list */
    physaddr = usb_ehci_swap32(*bp); /* Physical address of first qTD in CPU order */

    if ((physaddr & QTD_NQP_T) != 0) {
        return 0;
    }

    /* Start with the first qTD in the list */

    qtd = (struct usb_ehci_qtd_s *)usb_ehci_virtramaddr(physaddr);
    next = NULL;

    /* And loop until we encounter the end of the qTD list */

    while (qtd) {
        /* Is this the end of the list?  Check the next qTD pointer (NQP)
       * terminate (T) bit.  If T==1, then the NQP address is not valid.
       */

        if ((usb_ehci_swap32(qtd->hw.nqp) & QTD_NQP_T) != 0) {
            /* Set the next pointer to NULL.  This will terminate the loop. */

            next = NULL;
        } else {
            physaddr = usb_ehci_swap32(qtd->hw.nqp) & QTD_NQP_NTEP_MASK;
            next = (struct usb_ehci_qtd_s *)usb_ehci_virtramaddr(physaddr);
        }

        /* Perform the user action on this entry.  The action might result in
       * unlinking the entry!  But that is okay because we already have the
       * next qTD pointer.
       *
       * Notice that we do not manage the back pointer (bp).  If the call-
       * out uses it, it must update it as necessary.
       */

        ret = handler(qtd, &bp, arg);

        /* If the handler returns any non-zero value, then terminate the
       * traversal early.
       */

        if (ret != 0) {
            return ret;
        }

        /* Set up to visit the next entry */

        qtd = next;
    }

    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qtd_discard
 *
 * Description:
 *   This is a usb_ehci_qtd_foreach callback.  It simply unlinks the QTD,
 *   updates the back pointer, and frees the QTD structure.
 *
 ****************************************************************************/

static int usb_ehci_qtd_discard(struct usb_ehci_qtd_s *qtd, uint32_t **bp,
                                void *arg)
{
    DEBUGASSERT(qtd && bp && *bp);

    /* Remove the qTD from the list by updating the forward pointer to skip
   * around this qTD.  We do not change that pointer because are repeatedly
   * removing the aTD at the head of the QH list.
   */

    **bp = qtd->hw.nqp;

    /* Then free the qTD */

    usb_ehci_qtd_free(qtd);
    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qh_discard
 *
 * Description:
 *   Free the Queue Head (QH) and all qTD's attached to the QH.
 *
 * Assumptions:
 *   The QH structure itself has already been unlinked from whatever list it
 *   may have been in.
 *
 ****************************************************************************/

static int usb_ehci_qh_discard(struct usb_ehci_qh_s *qh)
{
    int ret;

    DEBUGASSERT(qh);

    /* Free all of the qTD's attached to the QH */

    ret = usb_ehci_qtd_foreach(qh, usb_ehci_qtd_discard, NULL);
    if (ret < 0) {
    }

    /* Then free the QH itself */

    usb_ehci_qh_free(qh);
    return ret;
}
#ifdef CONFIG_USB_DCACHE_ENABLE
/****************************************************************************
 * Name: usb_ehci_qtd_flush
 *
 * Description:
 *   This is a callback from usb_ehci_qtd_foreach.  It simply flushes D-cache
 *   for address range of the qTD entry.
 *
 ****************************************************************************/

static int usb_ehci_qtd_flush(struct usb_ehci_qtd_s *qtd, uint32_t **bp, void *arg)
{
    /* Flush the D-Cache, i.e., make the contents of the memory match the
    * contents of the D-Cache in the specified address range and invalidate
    * the D-Cache to force re-loading of the data from memory when next
    * accessed.
    */

    usb_ehci_dcache_clean_invalidate((uintptr_t)&qtd->hw, sizeof(struct usb_ehci_qtd_s));

    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qh_flush
 *
 * Description:
 *   Invalidate the Queue Head and all qTD entries in the queue.
 *
 ****************************************************************************/

static int usb_ehci_qh_flush(struct usb_ehci_qh_s *qh)
{
    /* Flush the QH first.  This will write the contents of the D-cache to RAM
    * and invalidate the contents of the D-cache so that the next access will
    * be reloaded from D-Cache.
    */

    usb_ehci_dcache_clean_invalidate((uintptr_t)&qh->hw, sizeof(struct usb_ehci_qh_s));

    /* Then flush all of the qTD entries in the queue */

    return usb_ehci_qtd_foreach(qh, usb_ehci_qtd_flush, NULL);
}
#else
#define usb_ehci_qtd_flush(qtd, bp, arg)
#define usb_ehci_qtd_flush(qh)
#endif
/****************************************************************************
 * Name: usb_ehci_speed
 *
 * Description:
 *   Map a speed enumeration value per Chapter 9 of the USB specification to
 *   the speed enumeration required in the EHCI queue head.
 *
 ****************************************************************************/

static inline uint8_t usb_ehci_speed(uint8_t usbspeed)
{
    DEBUGASSERT(usbspeed >= USB_SPEED_LOW && usbspeed <= USB_SPEED_HIGH);
    return g_ehci_speed[usbspeed];
}

static struct usb_ehci_qh_s *usb_ehci_qh_create(struct usb_ehci_epinfo_s *epinfo)
{
    struct usb_ehci_qh_s *qh;
    uint32_t regval;
    uint8_t hubaddr;
    uint8_t hubport;
    struct usb_ehci_epinfo_s *ep0info;
    struct usbh_hubport *rhport;

    rhport = epinfo->hport;

    while (rhport->parent != NULL) {
        rhport = rhport->parent->parent;
    }
    ep0info = (struct usb_ehci_epinfo_s *)rhport->ep0;
    /* Allocate a new queue head structure */

    qh = usb_ehci_qh_alloc();
    if (qh == NULL) {
        return NULL;
    }

    /* Save the endpoint information with the QH itself */

    qh->epinfo = epinfo;

    /* Write QH endpoint characteristics:
   *
   * FIELD    DESCRIPTION                     VALUE/SOURCE
   * -------- ------------------------------- --------------------
   * DEVADDR  Device address                  Endpoint structure
   * I        Inactivate on Next Transaction  0
   * ENDPT    Endpoint number                 Endpoint structure
   * EPS      Endpoint speed                  Endpoint structure
   * DTC      Data toggle control             1
   * MAXPKT   Max packet size                 Endpoint structure
   * C        Control endpoint                Calculated
   * RL       NAK count reloaded              0
   */

    regval = ((uint32_t)epinfo->devaddr << QH_EPCHAR_DEVADDR_SHIFT) |
             ((uint32_t)epinfo->epno << QH_EPCHAR_ENDPT_SHIFT) |
             ((uint32_t)usb_ehci_speed(epinfo->speed) << QH_EPCHAR_EPS_SHIFT) |
             QH_EPCHAR_DTC |
             ((uint32_t)epinfo->maxpacket << QH_EPCHAR_MAXPKT_SHIFT) |
             ((uint32_t)0 << QH_EPCHAR_RL_SHIFT);

    /* Paragraph 3.6.3: "Control Endpoint Flag (C). If the QH.EPS field
    * indicates the endpoint is not a high-speed device, and the endpoint
    * is an control endpoint, then software must set this bit to a one.
    * Otherwise it should always set this bit to a zero."
    */

    if (epinfo->speed != USB_SPEED_HIGH &&
        epinfo->xfrtype == USB_ENDPOINT_TYPE_CONTROL) {
        regval |= QH_EPCHAR_C;
    }

    /* Save the endpoint characteristics word with the correct byte order */

    qh->hw.epchar = usb_ehci_swap32(regval);

    /* Write QH endpoint capabilities
    *
    * FIELD    DESCRIPTION                     VALUE/SOURCE
    * -------- ------------------------------- --------------------
    * SSMASK   Interrupt Schedule Mask         Depends on epinfo->xfrtype
    * SCMASK   Split Completion Mask           0
    * HUBADDR  Hub Address                     roothub port devaddr
    * PORT     Port number                     RH port index
    * MULT     High band width multiplier      1
    */

    hubaddr = ep0info->devaddr;
    hubport = rhport->port;

    regval = ((uint32_t)hubaddr << QH_EPCAPS_HUBADDR_SHIFT) |
             ((uint32_t)hubport << QH_EPCAPS_PORT_SHIFT) |
             ((uint32_t)1 << QH_EPCAPS_MULT_SHIFT);

#ifndef CONFIG_USBHOST_INT_DISABLE
    if (epinfo->xfrtype == USB_ENDPOINT_TYPE_INTERRUPT) {
        regval |= ((uint32_t)1 << QH_EPCAPS_SSMASK_SHIFT);
    }
#endif

    qh->hw.epcaps = usb_ehci_swap32(regval);

    /* Mark this as the end of this list.  This will be overwritten if/when the
    * next qTD is added to the queue.
    */

    qh->hw.hlp = usb_ehci_swap32(QH_HLP_T);
    qh->hw.overlay.nqp = usb_ehci_swap32(QH_NQP_T);
    qh->hw.overlay.alt = usb_ehci_swap32(QH_AQP_T);
    return qh;
}

static int usb_ehci_qtd_addbpl(struct usb_ehci_qtd_s *qtd, const void *buffer, size_t buflen)
{
    uint32_t physaddr;
    uint32_t nbytes;
    uint32_t next;
    int ndx;

    usb_ehci_dcache_clean_invalidate((uintptr_t)buffer, buflen);
    /* Loop, adding the aligned physical addresses of the buffer to the buffer
    * page list.  Only the first entry need not be aligned (because only the
    * first entry has the offset field). The subsequent entries must begin on
    * 4KB address boundaries.
    */

    physaddr = (uint32_t)usb_ehci_physramaddr((uintptr_t)buffer);

    for (ndx = 0; ndx < 5; ndx++) {
        /* Write the physical address of the buffer into the qTD buffer
       * pointer list.
       */

        qtd->hw.bpl[ndx] = usb_ehci_swap32(physaddr);

        /* Get the next buffer pointer (in the case where we will have to
        * transfer more then one chunk).  This buffer must be aligned to a
        * 4KB address boundary.
        */

        next = (physaddr + 4096) & ~4095;

        /* How many bytes were included in the last buffer?  Was it the whole
        * thing?
        */

        nbytes = next - physaddr;
        if (nbytes >= buflen) {
            /* Yes... it was the whole thing.  Break out of the loop early. */

            break;
        }

        /* Adjust the buffer length and physical address for the next time
        * through the loop.
        */

        buflen -= nbytes;
        physaddr = next;
    }

    /* Handle the case of a huge buffer > 4*4KB = 16KB */

    if (ndx >= 5) {
        return -EFBIG;
    }

    return 0;
}

static struct usb_ehci_qtd_s *usb_ehci_qtd_setupphase(struct usb_ehci_epinfo_s *epinfo, struct usb_setup_packet *setup)
{
    struct usb_ehci_qtd_s *qtd;
    uint32_t regval;
    int ret;

    /* Allocate a new Queue Element Transfer Descriptor (qTD) */

    qtd = usb_ehci_qtd_alloc();
    if (qtd == NULL) {
        return NULL;
    }

    /* Mark this as the end of the list (this will be overwritten if another
    * qTD is added after this one).
    */

    qtd->hw.nqp = usb_ehci_swap32(QTD_NQP_T);
    qtd->hw.alt = usb_ehci_swap32(QTD_AQP_T);

    /* Write qTD token:
    *
    * FIELD    DESCRIPTION                     VALUE/SOURCE
    * -------- ------------------------------- --------------------
    * STATUS   Status                          QTD_TOKEN_ACTIVE
    * PID      PID Code                        QTD_TOKEN_PID_SETUP
    * CERR     Error Counter                   3
    * CPAGE    Current Page                    0
    * IOC      Interrupt on complete           0
    * NBYTES   Total Bytes to Transfer         8
    * TOGGLE   Data Toggle                     0
    */

    regval = QTD_TOKEN_ACTIVE | QTD_TOKEN_PID_SETUP |
             ((uint32_t)3 << QTD_TOKEN_CERR_SHIFT) |
             ((uint32_t)8 << QTD_TOKEN_NBYTES_SHIFT);

    qtd->hw.token = usb_ehci_swap32(regval);

    /* Add the buffer data */
    ret = usb_ehci_qtd_addbpl(qtd, (uint8_t *)setup, 8);
    if (ret < 0) {
        usb_ehci_qtd_free(qtd);
        return NULL;
    }

    /* Add the data transfer size to the count in the epinfo structure */
    epinfo->xfrd += 8;

    return qtd;
}

static struct usb_ehci_qtd_s *usb_ehci_qtd_dataphase(struct usb_ehci_epinfo_s *epinfo, void *buffer, int buflen, uint32_t tokenbits)
{
    struct usb_ehci_qtd_s *qtd;
    uint32_t regval;
    int ret;

    /* Allocate a new Queue Element Transfer Descriptor (qTD) */

    qtd = usb_ehci_qtd_alloc();
    if (qtd == NULL) {
        return NULL;
    }

    /* Mark this as the end of the list (this will be overwritten if another
    * qTD is added after this one).
    */

    qtd->hw.nqp = usb_ehci_swap32(QTD_NQP_T);
    qtd->hw.alt = usb_ehci_swap32(QTD_AQP_T);

    /* Write qTD token:
    *
    * FIELD    DESCRIPTION                     VALUE/SOURCE
    * -------- ------------------------------- --------------------
    * STATUS   Status                          QTD_TOKEN_ACTIVE
    * PID      PID Code                        Contained in tokenbits
    * CERR     Error Counter                   3
    * CPAGE    Current Page                    0
    * IOC      Interrupt on complete           Contained in tokenbits
    * NBYTES   Total Bytes to Transfer         buflen
    * TOGGLE   Data Toggle                     Contained in tokenbits
    */

    regval = tokenbits | QTD_TOKEN_ACTIVE |
             ((uint32_t)3 << QTD_TOKEN_CERR_SHIFT) |
             ((uint32_t)buflen << QTD_TOKEN_NBYTES_SHIFT);

    qtd->hw.token = usb_ehci_swap32(regval);

    ret = usb_ehci_qtd_addbpl(qtd, buffer, buflen);
    if (ret < 0) {
        usb_ehci_qtd_free(qtd);
        return NULL;
    }

    /* Add the data transfer size to the count in the epinfo structure */
    epinfo->xfrd += buflen;

    return qtd;
}

static struct usb_ehci_qtd_s *usb_ehci_qtd_statusphase(uint32_t tokenbits)
{
    struct usb_ehci_qtd_s *qtd;
    uint32_t regval;

    /* Allocate a new Queue Element Transfer Descriptor (qTD) */

    qtd = usb_ehci_qtd_alloc();
    if (qtd == NULL) {
        return NULL;
    }

    /* Mark this as the end of the list (this will be overwritten if another
    * qTD is added after this one).
    */

    qtd->hw.nqp = usb_ehci_swap32(QTD_NQP_T);
    qtd->hw.alt = usb_ehci_swap32(QTD_AQP_T);

    /* Write qTD token:
    *
    * FIELD    DESCRIPTION                     VALUE/SOURCE
    * -------- ------------------------------- --------------------
    * STATUS   Status                          QTD_TOKEN_ACTIVE
    * PID      PID Code                        Contained in tokenbits
    * CERR     Error Counter                   3
    * CPAGE    Current Page                    0
    * IOC      Interrupt on complete           QTD_TOKEN_IOC
    * NBYTES   Total Bytes to Transfer         0
    * TOGGLE   Data Toggle                     Contained in tokenbits
    */

    regval = tokenbits | QTD_TOKEN_ACTIVE | QTD_TOKEN_IOC |
             ((uint32_t)3 << QTD_TOKEN_CERR_SHIFT);

    qtd->hw.token = usb_ehci_swap32(regval);

    return qtd;
}

static void usb_ehci_qh_enqueue(struct usb_ehci_qh_s *qhead, struct usb_ehci_qh_s *qh)
{
    uintptr_t physaddr;

    /* Set the internal fqp field.  When we transverse the QH list later,
    * we need to know the correct place to start because the overlay may no
    * longer point to the first qTD entry.
    */

    qh->fqp = qh->hw.overlay.nqp;

    /* Add the new QH to the head of the asynchronous queue list.
    *
    * First, attach the old head as the new QH HLP and flush the new QH and
    * its attached qTDs to RAM.
    */

    qh->hw.hlp = qhead->hw.hlp;
    usb_ehci_qh_flush(qh);
    /* Then set the new QH as the first QH in the asynchronous queue */

    physaddr = (uintptr_t)usb_ehci_physramaddr((uintptr_t)qh);
    qhead->hw.hlp = usb_ehci_swap32(physaddr | QH_HLP_TYP_QH);
    usb_ehci_dcache_clean((uintptr_t)&qhead->hw, sizeof(struct usb_ehci_qh_s));
}

static int usb_ehci_control_init(struct usb_ehci_epinfo_s *epinfo, struct usb_setup_packet *setup, uint8_t *buffer, uint32_t buflen)
{
    struct usb_ehci_qh_s *qh;
    struct usb_ehci_qtd_s *qtd;
    uint32_t tokenbits;
    uintptr_t physaddr;
    uint32_t *flink;
    uint32_t *alt;
    uint32_t toggle;
    bool dirin = false;

    /* Create and initialize a Queue Head (QH) structure for this transfer */
    qh = usb_ehci_qh_create(epinfo);
    if (qh == NULL) {
        return -ENOMEM;
    }

    epinfo->qh = qh;
    /* Initialize the QH link and get the next data toggle (not used for SETUP
    * transfers)
    */

    flink = &qh->hw.overlay.nqp;
    toggle = (uint32_t)epinfo->toggle << QTD_TOKEN_TOGGLE_SHIFT;

    /* Is there an EP0 SETUP request?  If so, we will queue two or three qTDs:
    *
    *   1) One for the SETUP phase,
    *   2) One for the DATA phase (if there is data), and
    *   3) One for the STATUS phase.
    */

    /* Allocate a new Queue Element Transfer Descriptor (qTD) for the SETUP
    * phase of the request sequence.
    */
    {
        qtd = usb_ehci_qtd_setupphase(epinfo, setup);
        if (qtd == NULL) {
            return -ENOMEM;
        }
        /* Link the new qTD to the QH head. */

        physaddr = usb_ehci_physramaddr((uintptr_t)qtd);
        *flink = usb_ehci_swap32(physaddr);

        /* Get the new forward link pointer and data toggle */

        flink = &qtd->hw.nqp;
        toggle = QTD_TOKEN_TOGGLE;
    }
    /* A buffer may or may be supplied with an EP0 SETUP transfer.  A buffer
    * will always be present for normal endpoint data transfers.
    */

    alt = NULL;

    if (buffer != NULL && buflen > 0) {
        /* Extra TOKEN bits include the data toggle, the data PID, and if
        * there is no request, an indication to interrupt at the end of this
        * transfer.
        */

        tokenbits = toggle;

        /* Get the data token direction.
        *
        * If this is a SETUP request, use the direction contained in the
        * request.  The IOC bit is not set.
        */
        if ((setup->bmRequestType & 0x80) == 0x80) {
            tokenbits |= QTD_TOKEN_PID_IN;
            dirin = true;
        } else {
            tokenbits |= QTD_TOKEN_PID_OUT;
            dirin = false;
        }

        /* Allocate a new Queue Element Transfer Descriptor (qTD) for the data
        * buffer.
        */

        qtd = usb_ehci_qtd_dataphase(epinfo, buffer, buflen, tokenbits);
        if (qtd == NULL) {
            return -ENOMEM;
        }

        /* Link the new qTD to either QH head of the SETUP qTD. */
        physaddr = usb_ehci_physramaddr((uintptr_t)qtd);
        *flink = usb_ehci_swap32(physaddr);

        /* Set the forward link pointer to this new qTD */

        flink = &qtd->hw.nqp;

        /* If this was an IN transfer, then setup a pointer alternate link.
        * The EHCI hardware will use this link if a short packet is received.
        */

        if (dirin) {
            alt = &qtd->hw.alt;
        }
    }

    {
        /* Extra TOKEN bits include the data toggle and the correct data PID. */

        tokenbits = toggle;

        /* The status phase direction is the opposite of the data phase.  If
        * this is an IN request, then we received the buffer and we will send
        * the zero length packet handshake.
        */
        if ((setup->bmRequestType & 0x80) == 0x80) {
            tokenbits |= QTD_TOKEN_PID_OUT;
        } else {
            /* Otherwise, this in an OUT request.  We send the buffer and we expect
            * to receive the NULL packet handshake.
            */
            tokenbits |= QTD_TOKEN_PID_IN;
        }

        /* Allocate a new Queue Element Transfer Descriptor (qTD) for the
        * status
        */
        qtd = usb_ehci_qtd_statusphase(tokenbits);
        if (qtd == NULL) {
            return -ENOMEM;
        }

        /* Link the new qTD to either the SETUP or data qTD. */
        physaddr = usb_ehci_physramaddr((uintptr_t)qtd);
        *flink = usb_ehci_swap32(physaddr);

        /* In an IN data qTD was also enqueued, then linked the data qTD's
        * alternate pointer to this STATUS phase qTD in order to handle short
        * transfers.
        */

        if (alt) {
            *alt = usb_ehci_swap32(physaddr);
        }
    }
    /* Add the new QH to the head of the asynchronous queue list */
    usb_ehci_qh_enqueue(&g_asynchead, qh);
    return 0;
}

static int usb_ehci_bulk_init(struct usb_ehci_epinfo_s *epinfo, uint8_t *buffer, uint32_t buflen)
{
    struct usb_ehci_qh_s *qh;
    struct usb_ehci_qtd_s *qtd;
    uint32_t tokenbits;
    uintptr_t physaddr;

    /* Create and initialize a Queue Head (QH) structure for this transfer */
    qh = usb_ehci_qh_create(epinfo);
    if (qh == NULL) {
        return -ENOMEM;
    }

    epinfo->qh = qh;

    /* Initialize the QH link and get the next data toggle */
    tokenbits = (uint32_t)epinfo->toggle << QTD_TOKEN_TOGGLE_SHIFT;

    if (buffer != NULL && buflen > 0) {
        /* Get the direction from the epinfo structure.  Since this is not an EP0 SETUP request,
        * nothing follows the data and we want the IOC interrupt when the data transfer completes.
        */
        if (epinfo->dirin) {
            tokenbits |= (QTD_TOKEN_PID_IN | QTD_TOKEN_IOC);
        } else {
            tokenbits |= (QTD_TOKEN_PID_OUT | QTD_TOKEN_IOC);
        }

        /* Allocate a new Queue Element Transfer Descriptor (qTD) for the data
        * buffer.
        */

        qtd = usb_ehci_qtd_dataphase(epinfo, buffer, buflen, tokenbits);
        if (qtd == NULL) {
            return -ENOMEM;
        }

        /* Link the new qTD to the QH. */
        physaddr = usb_ehci_physramaddr((uintptr_t)qtd);
        qh->hw.overlay.nqp = usb_ehci_swap32(physaddr);
    }

    /* Add the new QH to the head of the asynchronous queue list */
    usb_ehci_qh_enqueue(&g_asynchead, qh);
    return 0;
}

static int usb_ehci_intr_init(struct usb_ehci_epinfo_s *epinfo, uint8_t *buffer, uint32_t buflen)
{
    struct usb_ehci_qh_s *qh;
    struct usb_ehci_qtd_s *qtd;
    uint32_t tokenbits;
    uintptr_t physaddr;
    uint32_t regval;

    /* Create and initialize a Queue Head (QH) structure for this transfer */
    qh = usb_ehci_qh_create(epinfo);
    if (qh == NULL) {
        return -ENOMEM;
    }

    epinfo->qh = qh;

    /* Initialize the QH link and get the next data toggle */
    tokenbits = (uint32_t)epinfo->toggle << QTD_TOKEN_TOGGLE_SHIFT;

    /* Get the direction from the epinfo structure.  Since this is not an EP0 SETUP request,
    * nothing follows the data and we want the IOC interrupt when the data transfer completes.
    */
    if (epinfo->dirin) {
        tokenbits |= (QTD_TOKEN_PID_IN | QTD_TOKEN_IOC);
    } else {
        tokenbits |= (QTD_TOKEN_PID_OUT | QTD_TOKEN_IOC);
    }

    /* Allocate a new Queue Element Transfer Descriptor (qTD) for the data
    * buffer.
    */

    qtd = usb_ehci_qtd_dataphase(epinfo, buffer, buflen, tokenbits);
    if (qtd == NULL) {
        return -ENOMEM;
    }

    /* Link the new qTD to the QH. */
    physaddr = usb_ehci_physramaddr((uintptr_t)qtd);
    qh->hw.overlay.nqp = usb_ehci_swap32(physaddr);

    /* Disable the periodic schedule */
    regval = usb_ehci_getreg(&HCOR->usbcmd);
    regval &= ~EHCI_USBCMD_PSEN;
    usb_ehci_putreg(regval, &HCOR->usbcmd);

    /* Add the new QH to the head of the interrupt transfer list */
    usb_ehci_qh_enqueue(&g_intrhead, qh);

    /* Re-enable the periodic schedule */
    regval |= EHCI_USBCMD_PSEN;
    usb_ehci_putreg(regval, &HCOR->usbcmd);
    return 0;
}

/****************************************************************************
 * Name: usb_ehci_ioc_setup
 *
 * Description:
 *   Set the request for the IOC event well BEFORE enabling the transfer (as
 *   soon as we are absolutely committed to the to avoid transfer).  We do
 *   this to minimize race conditions.  This logic would have to be expanded
 *   if we want to have more than one packet in flight at a time!
 *
 * Assumption:  The caller holds tex EHCI exclsem
 *
 ****************************************************************************/

static int usb_ehci_ioc_setup(struct usb_ehci_epinfo_s *epinfo)
{
    uint32_t flags;
    int ret = -ENODEV;

    DEBUGASSERT(rhport && epinfo && !epinfo->iocwait);
#ifdef CONFIG_USBHOST_ASYNCH
    DEBUGASSERT(epinfo->callback == NULL);
#endif

    /* Is the device still connected? */

    flags = usb_osal_enter_critical_section();
    if (epinfo->hport->connected) {
        /* Then set iocwait to indicate that we expect to be informed when
        * either (1) the device is disconnected, or (2) the transfer
        * completed.
        */

        epinfo->iocwait = true;  /* We want to be awakened by IOC interrupt */
        epinfo->status = 0;      /* No status yet */
        epinfo->xfrd = 0;        /* Nothing transferred yet */
        epinfo->result = -EBUSY; /* Transfer in progress */
#ifdef CONFIG_USBHOST_ASYNCH
        epinfo->callback = NULL; /* No asynchronous callback */
        epinfo->arg = NULL;
#endif
        ret = 0; /* We are good to go */
    }
    usb_osal_leave_critical_section(flags);
    return ret;
}

/****************************************************************************
 * Name: usb_ehci_ioc_async_setup
 *
 * Description:
 *   Setup to receive an asynchronous notification when a transfer completes.
 *
 * Input Parameters:
 *   epinfo - The IN or OUT endpoint descriptor for the device endpoint on
 *      which the transfer will be performed.
 *   callback - The function to be called when the completes
 *   arg - An arbitrary argument that will be provided with the callback.
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *   - Called from the interrupt level
 *
 ****************************************************************************/

#ifdef CONFIG_USBHOST_ASYNCH
static int usb_ehci_ioc_async_setup(struct usb_ehci_epinfo_s *epinfo, usbh_asynch_callback_t callback, void *arg)
{
    uint32_t flags;
    int ret = -ENODEV;

    DEBUGASSERT(rhport && epinfo && !epinfo->iocwait);
#ifdef CONFIG_USBHOST_ASYNCH
    DEBUGASSERT(epinfo->callback == NULL);
#endif

    /* Is the device still connected? */

    flags = usb_osal_enter_critical_section();
    if (epinfo->hport->connected) {
        /* Then set iocwait to indicate that we expect to be informed when
        * either (1) the device is disconnected, or (2) the transfer
        * completed.
        */

        epinfo->iocwait = false; /* We want to be awakened by IOC interrupt */
        epinfo->status = 0;      /* No status yet */
        epinfo->xfrd = 0;        /* Nothing transferred yet */
        epinfo->result = -EBUSY; /* Transfer in progress */
#ifdef CONFIG_USBHOST_ASYNCH
        epinfo->callback = callback; /* No asynchronous callback */
        epinfo->arg = arg;
#endif
        ret = 0; /* We are good to go */
    }

    usb_osal_leave_critical_section(flags);
    return ret;
}
#endif

/****************************************************************************
 * Name: usb_ehci_asynch_completion
 *
 * Description:
 *   This function is called at the interrupt level when an asynchronous
 *   transfer completes.  It performs the pending callback.
 *
 * Input Parameters:
 *   epinfo - The IN or OUT endpoint descriptor for the device endpoint on
 *      which the transfer was performed.
 *
 * Returned Value:
 *   None
 *
 * Assumptions:
 *   - Called from the interrupt level
 *
 ****************************************************************************/

#ifdef CONFIG_USBHOST_ASYNCH
static void usb_ehci_asynch_completion(struct usb_ehci_epinfo_s *epinfo)
{
    usbh_asynch_callback_t callback;
    int nbytes;
    void *arg;
    int result;

    DEBUGASSERT(epinfo != NULL && epinfo->iocwait == false &&
                epinfo->callback != NULL);

    /* Extract and reset the callback info */

    callback = epinfo->callback;
    arg = epinfo->arg;
    result = epinfo->result;
    nbytes = epinfo->xfrd;

    epinfo->callback = NULL;
    epinfo->arg = NULL;
    epinfo->result = 0;
    epinfo->iocwait = false;

    /* Then perform the callback.  Provide the number of bytes successfully
   * transferred or the negated errno value in the event of a failure.
   */

    if (result < 0) {
        nbytes = (int)result;
    }

    callback(arg, nbytes);
}
#endif

/****************************************************************************
 * Name: usb_ehci_transfer_wait
 *
 * Description:
 *   Wait for an IN or OUT transfer to complete.
 *
 * Assumption:  The caller holds the EHCI exclsem.  The caller must be aware
 *   that the EHCI exclsem will released while waiting for the transfer to
 *   complete, but will be re-acquired when before returning.  The state of
 *   EHCI resources could be very different upon return.
 *
 * Returned Value:
 *   On success, this function returns the number of bytes actually
 *   transferred.  For control transfers, this size includes the size of the
 *   control request plus the size of the data (which could be short); for
 *   bulk transfers, this will be the number of data bytes transfers (which
 *   could be short).
 *
 ****************************************************************************/

static int usb_ehci_transfer_wait(struct usb_ehci_epinfo_s *epinfo, uint32_t timeout)
{
    int ret = 0;

    /* Wait for the IOC completion event */
    if (epinfo->iocwait) {
        ret = usb_osal_sem_take(epinfo->iocsem, timeout);
        if (ret < 0) {
            return ret;
        }
    }

    ret = epinfo->result;

    if (ret < 0) {
        return ret;
    }

    /* Transfer completed successfully.  Return the number of bytes transferred.*/
    return epinfo->xfrd;
}

/****************************************************************************
 * Name: usb_ehci_qtd_ioccheck
 *
 * Description:
 *   This function is a usb_ehci_qtd_foreach() callback function.  It services
 *   one qTD in the asynchronous queue.  It removes all of the qTD
 *   structures that are no longer active.
 *
 ****************************************************************************/

static int usb_ehci_qtd_ioccheck(struct usb_ehci_qtd_s *qtd, uint32_t **bp,
                                 void *arg)
{
    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)arg;
    DEBUGASSERT(qtd && epinfo);

    usb_ehci_dcache_invalidate((uintptr_t)&qtd->hw, sizeof(struct usb_ehci_qtd_s));

    /* Remove the qTD from the list
    *
    * NOTE that we don't check if the qTD is active nor do we check if there
    * are any errors reported in the qTD.  If the transfer halted due to
    * an error, then qTDs in the list after the error qTD will still appear
    * to be active.
    */

    **bp = qtd->hw.nqp;

    /* Subtract the number of bytes left untransferred.  The epinfo->xfrd
    * field is initialized to the total number of bytes to be transferred
    * (all qTDs in the list).  We subtract out the number of untransferred
    * bytes on each transfer and the final result will be the number of bytes
    * actually transferred.
    */

    epinfo->xfrd -= (usb_ehci_swap32(qtd->hw.token) & QTD_TOKEN_NBYTES_MASK) >>
                    QTD_TOKEN_NBYTES_SHIFT;

    /* Release this QH by returning it to the free list */

    usb_ehci_qtd_free(qtd);

    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qh_ioccheck
 *
 * Description:
 *   This function is a usb_ehci_qh_foreach() callback function.  It services
 *   one QH in the asynchronous queue.  It check all attached qTD structures
 *   and remove all of the structures that are no longer active.  if all of
 *   the qTD structures are removed, then QH itself will also be removed.
 *
 ****************************************************************************/

static int usb_ehci_qh_ioccheck(struct usb_ehci_qh_s *qh, uint32_t **bp, void *arg)
{
    struct usb_ehci_epinfo_s *epinfo;
    uint32_t token;
    int ret;

    DEBUGASSERT(qh && bp);

    usb_ehci_dcache_invalidate((uintptr_t)&qh->hw, sizeof(struct ehci_qh_s));

    /* Get the endpoint info pointer from the extended QH data.  Only the
    * g_asynchead QH can have a NULL epinfo field.
    */

    epinfo = qh->epinfo;
    DEBUGASSERT(epinfo);

    /* Paragraph 3.6.3:  "The nine DWords in [the Transfer Overlay] area
    * represent a transaction working space for the host controller.  The
    * general operational model is that the host controller can detect
    * whether the overlay area contains a description of an active transfer.
    * If it does not contain an active transfer, then it follows the Queue
    * Head Horizontal Link Pointer to the next queue head.  The host
    * controller will never follow the Next Transfer Queue Element or
    * Alternate Queue Element pointers unless it is actively attempting to
    * advance the queue ..."
    */

    /* Is the qTD still active? */

    token = usb_ehci_swap32(qh->hw.overlay.token);

    if ((token & QH_TOKEN_ACTIVE) != 0) {
        /* Yes... we cannot process the QH while it is still active.  Return
       * zero to visit the next QH in the list.
       */
        *bp = &qh->hw.hlp;
        return 0;
    }

    /* Remove all active, attached qTD structures from the inactive QH */
    ret = usb_ehci_qtd_foreach(qh, usb_ehci_qtd_ioccheck, (void *)qh->epinfo);
    if (ret < 0) {
    }
    /* If there is no longer anything attached to the QH, then remove it from
    * the asynchronous queue.
    */

    if ((usb_ehci_swap32(qh->fqp) & QTD_NQP_T) != 0) {
        /* Set the forward link of the previous QH to point to the next
        * QH in the list.
        */

        **bp = qh->hw.hlp;
        usb_ehci_dcache_clean((uintptr_t)*bp, sizeof(uint32_t));
        /* Check for errors, update the data toggle */

        if ((token & QH_TOKEN_ERRORS) == 0) {
            /* No errors.. Save the last data toggle value */

            epinfo->toggle = (token >> QTD_TOKEN_TOGGLE_SHIFT) & 1;

            /* Report success */

            epinfo->status = 0;
            epinfo->result = 0;

        } else {
            /* An error occurred */

            epinfo->status = (token & QH_TOKEN_STATUS_MASK) >>
                             QH_TOKEN_STATUS_SHIFT;

            /* The HALT condition is set on a variety of conditions:  babble,
            * error counter countdown to zero, or a STALL.  If we can rule
            * out babble (babble bit not set) and if the error counter is
            * non-zero, then we can assume a STALL. In this case, we return
            * -PERM to inform the class driver of the stall condition.
            */

            if ((token & (QH_TOKEN_BABBLE | QH_TOKEN_HALTED)) ==
                    QH_TOKEN_HALTED &&
                (token & QH_TOKEN_CERR_MASK) != 0) {
                /* It is a stall,  Note that the data toggle is reset
                * after the stall.
                */

                epinfo->result = -EPERM;
                epinfo->toggle = 0;
            } else {
                /* Otherwise, it is some kind of data transfer error */

                epinfo->result = -EIO;
            }
        }

        /* Is there a thread waiting for this transfer to complete? */
        if (epinfo->iocwait) {
            /* Yes... wake it up */
            epinfo->iocwait = false;
            usb_osal_sem_give(epinfo->iocsem);
        }
#ifdef CONFIG_USBHOST_ASYNCH
        /* No.. Is there a pending asynchronous transfer? */

        else if (epinfo->callback != NULL) {
            /* Yes.. perform the callback */

            usb_ehci_asynch_completion(epinfo);
        }
#endif
        /* Then release this QH by returning it to the free list */
        usb_ehci_qh_free(qh);
    } else {
        /* Otherwise, the horizontal link pointer of this QH will become the
        * next back pointer.
        */

        *bp = &qh->hw.hlp;
    }

    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qtd_cancel
 *
 * Description:
 *   This function is a usb_ehci_qtd_foreach() callback function.  It removes
 *   each qTD attached to a QH.
 *
 ****************************************************************************/
static int usb_ehci_qtd_cancel(struct usb_ehci_qtd_s *qtd, uint32_t **bp,
                               void *arg)
{
    DEBUGASSERT(qtd != NULL && bp != NULL);

    usb_ehci_dcache_invalidate((uintptr_t)&qtd->hw, sizeof(struct usb_ehci_qtd_s));

    /* Remove the qTD from the list
    *
    * NOTE that we don't check if the qTD is active nor do we check if there
    * are any errors reported in the qTD.  If the transfer halted due to
    * an error, then qTDs in the list after the error qTD will still appear
    * to be active.
    *
    * REVISIT: There is a race condition here that needs to be resolved.
    */

    **bp = qtd->hw.nqp;

    /* Release this QH by returning it to the free list */

    usb_ehci_qtd_free(qtd);
    return 0;
}

/****************************************************************************
 * Name: usb_ehci_qh_cancel
 *
 * Description:
 *   This function is a imxrt_qh_foreach() callback function.  It cancels
 *   one QH in the asynchronous queue.  It will remove all attached qTD
 *   structures and remove all of the structures that are no longer active.
 *   Then QH itself will also be removed.
 *
 ****************************************************************************/
static int usb_ehci_qh_cancel(struct usb_ehci_qh_s *qh, uint32_t **bp, void *arg)
{
    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)arg;
    uint32_t regval;
    int ret;

    DEBUGASSERT(qh != NULL && bp != NULL && epinfo != NULL);

    usb_ehci_dcache_invalidate((uintptr_t)&qh->hw, sizeof(struct usb_ehci_qh_s));

    /* Check if this is the QH that we are looking for */

    if (qh->epinfo == epinfo) {
        /* No... keep looking */

        return 0;
    }

    /* Disable both the asynchronous and period schedules */

    regval = usb_ehci_getreg(&HCOR->usbcmd);
    usb_ehci_putreg(regval & ~(EHCI_USBCMD_ASEN | EHCI_USBCMD_PSEN),
                    &HCOR->usbcmd);

    /* Remove the QH from the list
    *
    * NOTE that we don't check if the qTD is active nor do we check if there
    * are any errors reported in the qTD.  If the transfer halted due to
    * an error, then qTDs in the list after the error qTD will still appear
    * to be active.
    *
    * REVISIT: There is a race condition here that needs to be resolved.
    */

    **bp = qh->hw.hlp;
    usb_ehci_dcache_clean((uintptr_t)*bp, sizeof(uint32_t));
    /* Re-enable the schedules (if they were enabled before. */

    usb_ehci_putreg(regval, &HCOR->usbcmd);

    /* Remove all active, attached qTD structures from the removed QH */

    ret = usb_ehci_qtd_foreach(qh, usb_ehci_qtd_cancel, NULL);
    if (ret < 0) {
    }

    /* Then release this QH by returning it to the free list.  Return 1
    * to stop the traverse without an error.
    */

    usb_ehci_qh_free(qh);
    return 1;
}

static inline void usb_ehci_ioc_bottomhalf(void)
{
    struct usb_ehci_qh_s *qh;
    uint32_t *bp;
    int ret;

    usb_ehci_dcache_invalidate((uintptr_t)&g_asynchead.hw, sizeof(struct usb_ehci_qh_s));
    /* Set the back pointer to the forward qTD pointer of the asynchronous
    * queue head.
    */

    bp = (uint32_t *)&g_asynchead.hw.hlp;
    qh = (struct usb_ehci_qh_s *)
        usb_ehci_virtramaddr(usb_ehci_swap32(*bp) & QH_HLP_MASK);

    /* If the asynchronous queue is empty, then the forward point in the
    * asynchronous queue head will point back to the queue head.
    */
    if (qh && qh != &g_asynchead) {
        /* Then traverse and operate on every QH and qTD in the asynchronous
        * queue
        */
        usb_ehci_qh_foreach(qh, &bp, usb_ehci_qh_ioccheck, NULL);
    }
#ifndef CONFIG_USBHOST_INT_DISABLE
    /* Check the Interrupt Queue */

    usb_ehci_dcache_invalidate((uintptr_t)&g_intrhead.hw, sizeof(struct usb_ehci_qh_s));
    /* Set the back pointer to the forward qTD pointer of the asynchronous
    * queue head.
    */

    bp = (uint32_t *)&g_intrhead.hw.hlp;
    qh = (struct usb_ehci_qh_s *)
        usb_ehci_virtramaddr(usb_ehci_swap32(*bp) & QH_HLP_MASK);
    if (qh) {
        /* Then traverse and operate on every QH and qTD in the asynchronous
        * queue.
        */

        ret = usb_ehci_qh_foreach(qh, &bp, usb_ehci_qh_ioccheck, NULL);
        if (ret < 0) {
        }
    }
#endif
}

/****************************************************************************
 * Name: usb_ehci_portsc_bottomhalf
 *
 * Description:
 *   EHCI Port Change Detect "Bottom Half" interrupt handler
 *
 *  "The Host Controller sets this bit to a one when any port for which the
 *   Port Owner bit is set to zero ... has a change bit transition from a
 *   zero to a one or a Force Port Resume bit transition from a zero to a
 *   one as a result of a J-K transition detected on a suspended port.
 *   This bit will also be set as a result of the Connect Status Change
 *   being set to a one after system software has relinquished ownership of
 *   a connected port by writing a one to a port's Port Owner bit...
 *
 *  "This bit is allowed to be maintained in the Auxiliary power well.
 *   Alternatively, it is also acceptable that on a D3 to D0 transition of
 *   the EHCI HC device, this bit is loaded with the OR of all of the PORTSC
 *   change bits (including: Force port resume, over-current change,
 *   enable/disable change and connect status change)."
 *
 ****************************************************************************/
static inline void usb_ehci_portsc_bottomhalf(void)
{
    uint32_t portsc;
    int rhpndx;

    /* Handle root hub status change on each root port */

    for (rhpndx = 0; rhpndx < CONFIG_USBHOST_RHPORTS; rhpndx++) {
        portsc = usb_ehci_getreg(&HCOR->portsc[rhpndx]);

        /* Handle port connection status change (CSC) events */
        if ((portsc & EHCI_PORTSC_CSC) != 0) {
            if ((portsc & EHCI_PORTSC_CCS) == EHCI_PORTSC_CCS) {
                /* Connected ... Did we just become connected? */
                usbh_event_notify_handler(USBH_EVENT_CONNECTED, 1);
            } else {
                for (uint8_t chidx = 0; chidx < CONFIG_USBHOST_PIPE_NUM; chidx++) {
                    struct usb_ehci_epinfo_s *epinfo = &g_ehci_hcd.chan[chidx];
                    if (epinfo->iocwait) {
                        epinfo->iocwait = false;
                        usb_osal_sem_give(epinfo->iocsem);
                    }
                }
                usbh_event_notify_handler(USBH_EVENT_DISCONNECTED, 1);
            }
        }

        /* Clear all pending port interrupt sources by writing a '1' to the
        * corresponding bit in the PORTSC register.  In addition, we need
        * to preserve the values of all R/W bits (RO bits don't matter)
        */
        usb_ehci_putreg(portsc, &HCOR->portsc[rhpndx]);
    }
}

/****************************************************************************
 * Name: usb_ehci_reset
 *
 * Description:
 *   Set the HCRESET bit in the USBCMD register to reset the EHCI hardware.
 *
 *   Table 2-9. USBCMD - USB Command Register Bit Definitions
 *
 *    "Host Controller Reset (HCRESET) ... This control bit is used by
 *     software to reset the host controller. The effects of this on Root
 *     Hub registers are similar to a Chip Hardware Reset.
 *
 *    "When software writes a one to this bit, the Host Controller resets its
 *     internal pipelines, timers, counters, state machines, etc. to their
 *     initial value. Any transaction currently in progress on USB is
 *     immediately terminated. A USB reset is not driven on downstream
 *     ports.
 *
 *    "PCI Configuration registers are not affected by this reset. All
 *     operational registers, including port registers and port state
 *     machines are set to their initial values. Port ownership reverts
 *     to the companion host controller(s)... Software must reinitialize
 *     the host controller ... in order to return the host controller to
 *     an operational state.
 *
 *    "This bit is set to zero by the Host Controller when the reset process
 *     is complete. Software cannot terminate the reset process early by
 *     writing a zero to this register. Software should not set this bit to
 *     a one when the HCHalted bit in the USBSTS register is a zero.
 *     Attempting to reset an actively running host controller will result
 *     in undefined behavior."
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   Zero (OK) is returned on success; A negated errno value is returned
 *   on failure.
 *
 * Assumptions:
 * - Called during the initialization of the EHCI.
 *
 ****************************************************************************/

static int usb_ehci_reset(void)
{
    uint32_t regval;
    unsigned int timeout;

    /* Make sure that the EHCI is halted:  "When [the Run/Stop] bit is set to
    * 0, the Host Controller completes the current transaction on the USB and
    * then halts. The HC Halted bit in the status register indicates when the
    * Host Controller has finished the transaction and has entered the
    * stopped state..."
    */

    usb_ehci_putreg(0, &HCOR->usbcmd);

    /* "... Software should not set [HCRESET] to a one when the HCHalted bit in
    *  the USBSTS register is a zero. Attempting to reset an actively running
    *  host controller will result in undefined behavior."
    */

    timeout = 0;
    do {
        /* Wait one microsecond and update the timeout counter */

        usb_osal_msleep(1);
        timeout++;

        /* Get the current value of the USBSTS register.  This loop will
        * terminate when either the timeout exceeds one millisecond or when
        * the HCHalted bit is no longer set in the USBSTS register.
        */

        regval = usb_ehci_getreg(&HCOR->usbsts);
    } while (((regval & EHCI_USBSTS_HALTED) == 0) && (timeout < 1000));

    /* Is the EHCI still running?  Did we timeout? */

    if ((regval & EHCI_USBSTS_HALTED) == 0) {
        return -ETIMEDOUT;
    }

    /* Now we can set the HCReset bit in the USBCMD register to
    * initiate the reset
    */

    regval = usb_ehci_getreg(&HCOR->usbcmd);
    regval |= EHCI_USBCMD_HCRESET;
    usb_ehci_putreg(regval, &HCOR->usbcmd);

    /* Wait for the HCReset bit to become clear */

    do {
        /* Wait five microsecondw and update the timeout counter */

        usb_osal_msleep(5);
        timeout += 5;

        /* Get the current value of the USBCMD register.  This loop will
        * terminate when either the timeout exceeds one second or when the
        * HCReset bit is no longer set in the USBSTS register.
        */

        regval = usb_ehci_getreg(&HCOR->usbcmd);
    } while (((regval & EHCI_USBCMD_HCRESET) != 0) && (timeout < 1000000));

    /* Return either success or a timeout */

    return (regval & EHCI_USBCMD_HCRESET) != 0 ? -ETIMEDOUT : 0;
}

/****************************************************************************
 * Name: usb_ehci_wait_usbsts
 *
 * Description:
 *   Wait for either (1) a field in the USBSTS register to take a specific
 *   value, (2) for a timeout to occur, or (3) a error to occur.  Return
 *   a value to indicate which terminated the wait.
 *
 ****************************************************************************/

static int usb_ehci_wait_usbsts(uint32_t maskbits, uint32_t donebits, unsigned int delay)
{
    uint32_t regval;
    unsigned int timeout;

    timeout = 0;
    do {
        /* Wait 5usec before trying again */

        usb_osal_msleep(5);
        timeout += 5;

        /* Read the USBSTS register and check for a system error */

        regval = usb_ehci_getreg(&HCOR->usbsts);
        if ((regval & EHCI_INT_SYSERROR) != 0) {
            return -EIO;
        }

        /* Mask out the bits of interest */

        regval &= maskbits;

        /* Loop until the masked bits take the specified value or until a
       * timeout occurs.
       */
    } while (regval != donebits && timeout < delay);

    /* We got here because either the waited for condition or a timeout
    * occurred.  Return a value to indicate which.
    */

    return (regval == donebits) ? 0 : -ETIMEDOUT;
}

__WEAK void usb_hc_low_level_init(void)
{
}

int usb_hc_sw_init(void)
{
    memset(&g_ehci_hcd, 0, sizeof(struct ehci_hcd));

    /* Initialize the list of free Queue Head (QH) structures */
    for (uint8_t i = 0; i < CONFIG_USB_EHCI_QH_NUM; i++) {
        /* Put the QH structure in a free list */

        usb_ehci_qh_free(&g_ehci_hcd.qhpool[i]);
    }

    /* Initialize the list of free Queue Head (QH) structures */
    for (uint8_t i = 0; i < CONFIG_USB_EHCI_QTD_NUM; i++) {
        /* Put the QH structure in a free list */

        usb_ehci_qtd_free(&g_ehci_hcd.qtdpool[i]);
    }

    for (uint8_t chidx = 0; chidx < CONFIG_USB_EHCI_QH_NUM; chidx++) {
        struct usb_ehci_epinfo_s *epinfo;

        epinfo = &g_ehci_hcd.chan[chidx];
        epinfo->iocsem = usb_osal_sem_create(0);
        epinfo->exclsem = usb_osal_mutex_create();
    }

    return 0;
}

int usb_hc_hw_init(void)
{
    int ret;
    uint32_t regval;
    uintptr_t physaddr1;
    uintptr_t physaddr2;

    /* Initialize the head of the asynchronous queue/reclamation list.
    *
    * "In order to communicate with devices via the asynchronous schedule,
    *  system software must write the ASYNDLISTADDR register with the address
    *  of a control or bulk queue head. Software must then enable the
    *  asynchronous schedule by writing a one to the Asynchronous Schedule
    *  Enable bit in the USBCMD register. In order to communicate with devices
    *  via the periodic schedule, system software must enable the periodic
    *  schedule by writing a one to the Periodic Schedule Enable bit in the
    *  USBCMD register. Note that the schedules can be turned on before the
    *  first port is reset (and enabled)."
    */

    memset(&g_asynchead, 0, sizeof(struct usb_ehci_qh_s));
    physaddr1 = usb_ehci_physramaddr((uintptr_t)&g_asynchead);
    g_asynchead.hw.hlp = usb_ehci_swap32(physaddr1 | QH_HLP_TYP_QH);
    g_asynchead.hw.epchar = usb_ehci_swap32(QH_EPCHAR_H |
                                            QH_EPCHAR_EPS_FULL);
    g_asynchead.hw.overlay.nqp = usb_ehci_swap32(QH_NQP_T);
    g_asynchead.hw.overlay.alt = usb_ehci_swap32(QH_NQP_T);
    g_asynchead.hw.overlay.token = usb_ehci_swap32(QH_TOKEN_HALTED);
    g_asynchead.fqp = usb_ehci_swap32(QTD_NQP_T);

    usb_ehci_dcache_clean((uintptr_t)&g_asynchead.hw, sizeof(struct usb_ehci_qh_s));

    /* Initialize the head of the periodic list.  Since Isochronous
    * endpoints are not not yet supported, each element of the
    * frame list is initialized to point to the Interrupt Queue
    * Head (g_intrhead).
    */

    memset(&g_intrhead, 0, sizeof(struct usb_ehci_qh_s));
    g_intrhead.hw.hlp = usb_ehci_swap32(QH_HLP_T);
    g_intrhead.hw.overlay.nqp = usb_ehci_swap32(QH_NQP_T);
    g_intrhead.hw.overlay.alt = usb_ehci_swap32(QH_NQP_T);
    g_intrhead.hw.overlay.token = usb_ehci_swap32(QH_TOKEN_HALTED);
    g_intrhead.hw.epcaps = usb_ehci_swap32(QH_EPCAPS_SSMASK(1));

    /* Attach the periodic QH to Period Frame List */
    physaddr2 = usb_ehci_physramaddr((uintptr_t)&g_intrhead);
    for (uint32_t i = 0; i < FRAME_LIST_SIZE; i++) {
        g_framelist[i] = usb_ehci_swap32(physaddr2) | PFL_TYP_QH;
    }

    /* Set the Periodic Frame List Base Address. */
    physaddr2 = usb_ehci_physramaddr((uintptr_t)g_framelist);

    usb_ehci_dcache_clean((uintptr_t)&g_intrhead.hw, sizeof(struct usb_ehci_qh_s));
    usb_ehci_dcache_clean((uintptr_t)g_framelist, FRAME_LIST_SIZE * sizeof(uint32_t));

    usb_hc_low_level_init();
    /* Host Controller Initialization. Paragraph 4.1 */

    /* Reset the EHCI hardware */
    ret = usb_ehci_reset();
    if (ret < 0) {
        return -1;
    }

    /* Disable all interrupts */
    usb_ehci_putreg(0, &HCOR->usbintr);

    /* Clear pending interrupts.  Bits in the USBSTS register are cleared by
    * writing a '1' to the corresponding bit.
    */
    usb_ehci_putreg(EHCI_INT_ALLINTS, &HCOR->usbsts);

#if defined(CONFIG_USB_EHCI_INFO_ENABLE)
    /* Show the EHCI version */
    uint16_t regval16 = usb_ehci_swap16(HCCR->hciversion);
    USB_LOG_INFO("EHCI HCIVERSION %x.%02x\r\n", regval16 >> 8, regval16 & 0xff);

    /* Verify that the correct number of ports is reported */
    regval = usb_ehci_getreg(&HCCR->hcsparams);
    uint8_t nports = (regval & EHCI_HCSPARAMS_NPORTS_MASK) >> EHCI_HCSPARAMS_NPORTS_SHIFT;
    USB_LOG_INFO("EHCI nports=%d, HCSPARAMS=%04x\r\n", nports, regval);

    /* Show the HCCPARAMS register */
    regval = usb_ehci_getreg(&HCCR->hccparams);
    USB_LOG_INFO("EHCI HCCPARAMS=%06x\r\n", regval);
#endif
    /* Set the Current Asynchronous List Address. */
    usb_ehci_putreg(usb_ehci_swap32(physaddr1), &HCOR->asynclistaddr);
    /* Set the Periodic Frame List Base Address. */
    usb_ehci_putreg(usb_ehci_swap32(physaddr2), &HCOR->periodiclistbase);

    /* Enable the asynchronous schedule and, possibly enable the periodic
    * schedule and set the frame list size.
    */
    regval = usb_ehci_getreg(&HCOR->usbcmd);
    regval &= ~(EHCI_USBCMD_HCRESET | EHCI_USBCMD_FLSIZE_MASK |
                EHCI_USBCMD_PSEN | EHCI_USBCMD_ASEN | EHCI_USBCMD_IAADB);
    regval |= EHCI_USBCMD_ASEN;

#ifndef CONFIG_USBHOST_INT_DISABLE
    regval |= EHCI_USBCMD_PSEN;
#if FRAME_LIST_SIZE == 1024
    regval |= EHCI_USBCMD_FLSIZE_1024;
#elif FRAME_LIST_SIZE == 512
    regval |= EHCI_USBCMD_FLSIZE_512;
#elif FRAME_LIST_SIZE == 256
    regval |= EHCI_USBCMD_FLSIZE_256;
#else
#error Unsupported frame size list size
#endif
#endif

    usb_ehci_putreg(regval, &HCOR->usbcmd);

    /* Start the host controller by setting the RUN bit in the
    * USBCMD register.
    */
    regval = usb_ehci_getreg(&HCOR->usbcmd);
    regval |= EHCI_USBCMD_RUN;
    usb_ehci_putreg(regval, &HCOR->usbcmd);

    /* Route all ports to this host controller by setting the CONFIG flag. */
#ifdef CONFIG_USB_EHCI_CONFIGFLAG
    regval = usb_ehci_getreg(&HCOR->configflag);
    regval |= EHCI_CONFIGFLAG;
    usb_ehci_putreg(regval, &HCOR->configflag);
#endif
    /* Wait for the EHCI to run (i.e., no longer report halted) */
    ret = usb_ehci_wait_usbsts(EHCI_USBSTS_HALTED, 0, 100 * 1000);
    if (ret < 0) {
        return -2;
    }
#ifdef CONFIG_USB_EHCI_PORT_POWER
    for (uint8_t port = 1; i <= CONFIG_USBHOST_RHPORTS; port++) {
        regval = usb_ehci_getreg(&HCOR->portsc[port - 1]);
        regval |= EHCI_PORTSC_PP;
        usb_ehci_putreg(regval, &HCOR->portsc[port - 1]);
    }
#endif
    /* Enable EHCI interrupts.  Interrupts are still disabled at the level of
    * the interrupt controller.
    */
    usb_ehci_putreg(EHCI_HANDLED_INTS, &HCOR->usbintr);

    return ret;
}

bool usbh_get_port_connect_status(const uint8_t port)
{
    uint32_t portsc;
    portsc = usb_ehci_getreg(&HCOR->portsc[port - 1]);
    if ((portsc & EHCI_PORTSC_CCS) == EHCI_PORTSC_CCS) {
        return true;
    } else {
        return false;
    }
}

int usbh_reset_port(const uint8_t port)
{
    uint32_t timeout = 0;
    uint32_t regval;
    regval = usb_ehci_getreg(&HCOR->portsc[port - 1]);
    regval &= ~EHCI_PORTSC_PE;
    regval |= EHCI_PORTSC_RESET;
    usb_ehci_putreg(regval, &HCOR->portsc[port - 1]);

    usb_osal_msleep(55);

    regval = usb_ehci_getreg(&HCOR->portsc[port - 1]);
    regval &= ~EHCI_PORTSC_RESET;
    usb_ehci_putreg(regval, &HCOR->portsc[port - 1]);

    /* Wait for the port reset to complete
    *
    * Paragraph 2.3.9:
    *
    *  "Note that when software writes a zero to this bit there may be a
    *   delay before the bit status changes to a zero. The bit status will
    *   not read as a zero until after the reset has completed. If the port
    *   is in high-speed mode after reset is complete, the host controller
    *   will automatically enable this port (e.g. set the Port Enable bit
    *   to a one). A host controller must terminate the reset and stabilize
    *   the state of the port within 2 milliseconds of software transitioning
    *   this bit from a one to a zero ..."
    */

    while ((usb_ehci_getreg(&HCOR->portsc[port - 1]) & EHCI_PORTSC_RESET) != 0) {
        usb_osal_msleep(1);
        timeout++;
        if (timeout > 100) {
            return -ETIMEDOUT;
        }
    }

    return 0;
}

// __WEAK uint8_t usbh_get_port_speed(const uint8_t port)
// {
//     /* Defined by individual manufacturers */
//     return 0;
// }

int usbh_ep0_reconfigure(usbh_epinfo_t ep, uint8_t dev_addr, uint8_t ep_mps, uint8_t speed)
{
    struct usb_ehci_epinfo_s *epinfo;
    int ret;

    epinfo = (struct usb_ehci_epinfo_s *)ep;

    DEBUGASSERT(epinfo != NULL && ep_mps < 2048);

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    epinfo->devaddr = dev_addr;
    epinfo->speed = speed;
    epinfo->maxpacket = ep_mps;

    usb_osal_mutex_give(epinfo->exclsem);

    return ret;
}

int usbh_ep_alloc(usbh_epinfo_t *ep, const struct usbh_endpoint_cfg *ep_cfg)
{
    struct usb_ehci_epinfo_s *epinfo;
    struct usbh_hubport *hport;
    usb_osal_sem_t iocsem;
    usb_osal_mutex_t exclsem;
    int chidx;

    DEBUGASSERT(ep_cfg != NULL && ep_cfg->hport != NULL);

    hport = ep_cfg->hport;

    chidx = usb_ehci_chan_alloc();

    epinfo = &g_ehci_hcd.chan[chidx];

    iocsem = epinfo->iocsem;
    exclsem = epinfo->exclsem;

    memset(epinfo, 0, sizeof(struct usb_ehci_epinfo_s));

    epinfo->epno = ep_cfg->ep_addr & 0x7f;
    epinfo->dirin = (ep_cfg->ep_addr & 0x80) ? 1 : 0;
    epinfo->devaddr = hport->dev_addr;
#ifndef CONFIG_USBHOST_INT_DISABLE
    epinfo->interval = ep_cfg->ep_interval;
#endif
    epinfo->maxpacket = ep_cfg->ep_mps;
    epinfo->xfrtype = ep_cfg->ep_type;
    epinfo->speed = hport->speed;
    epinfo->hport = hport;

    /* restore variable */
    epinfo->inuse = true;
    epinfo->iocsem = iocsem;
    epinfo->exclsem = exclsem;

    *ep = (usbh_epinfo_t)epinfo;

    return 0;
}

int usbh_ep_free(usbh_epinfo_t ep)
{
    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;

    usb_ehci_chan_free(epinfo);

    return 0;
}

int usbh_control_transfer(usbh_epinfo_t ep, struct usb_setup_packet *setup, uint8_t *buffer)
{
    int ret;

    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;

    DEBUGASSERT(epinfo);

    /* A buffer may or may be supplied with an EP0 SETUP transfer.  A buffer
    * will always be present for normal endpoint data transfers.
    */

    DEBUGASSERT(setup || buffer);

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    /* Set the request for the IOC event well BEFORE initiating the transfer. */
    ret = usb_ehci_ioc_setup(epinfo);
    if (ret != 0) {
        goto errout_with_setup;
    }

    ret = usb_ehci_control_init(epinfo, setup, buffer, setup->wLength);
    if (ret < 0) {
        goto errout_with_iocwait;
    }

    /* And wait for the transfer to complete */
    ret = usb_ehci_transfer_wait(epinfo, CONFIG_USBHOST_CONTROL_TRANSFER_TIMEOUT);
    if (ret < 0) {
        goto errout_with_iocwait;
    }

    usb_osal_mutex_give(epinfo->exclsem);
    return ret;

errout_with_iocwait:
    epinfo->iocwait = false;
    /* Clean-up after an error */
    if (epinfo->qh) {
        usb_ehci_qh_discard(epinfo->qh);
        epinfo->qh = NULL;
    }
errout_with_setup:
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;
}

int usbh_ep_bulk_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;

    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;

    DEBUGASSERT(epinfo && buffer && buflen > 0);

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = usb_ehci_ioc_setup(epinfo);
    if (ret < 0) {
        goto errout_with_setup;
    }

    ret = usb_ehci_bulk_init(epinfo, buffer, buflen);
    if (ret < 0) {
        goto errout_with_iocwait;
    }

    /* And wait for the transfer to complete */
    ret = usb_ehci_transfer_wait(epinfo, timeout);
    if (ret < 0) {
        goto errout_with_iocwait;
    }
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;

errout_with_iocwait:
    epinfo->iocwait = false;
    /* Clean-up after an error */
    if (epinfo->qh) {
        usb_ehci_qh_discard(epinfo->qh);
        epinfo->qh = NULL;
    }
errout_with_setup:
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;
}

int usbh_ep_intr_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, uint32_t timeout)
{
    int ret;

    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;

    DEBUGASSERT(epinfo && buffer && buflen > 0);

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    ret = usb_ehci_ioc_setup(epinfo);
    if (ret < 0) {
        goto errout_with_setup;
    }

    ret = usb_ehci_intr_init(epinfo, buffer, buflen);
    if (ret < 0) {
        goto errout_with_iocwait;
    }

    /* And wait for the transfer to complete */
    ret = usb_ehci_transfer_wait(epinfo, timeout);
    if (ret < 0) {
        goto errout_with_iocwait;
    }
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;

errout_with_iocwait:
    epinfo->iocwait = false;
    /* Clean-up after an error */
    if (epinfo->qh) {
        usb_ehci_qh_discard(epinfo->qh);
        epinfo->qh = NULL;
    }
errout_with_setup:
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;
}
#ifdef CONFIG_USBHOST_ASYNCH
int usbh_ep_bulk_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;

    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;

    DEBUGASSERT(epinfo && buffer && buflen > 0);

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    /* Set the request for the callback well BEFORE initiating the transfer. */
    ret = usb_ehci_ioc_async_setup(epinfo, callback, arg);
    if (ret != 0) {
        goto errout_with_setup;
    }

    /* Check for errors in the setup of the transfer */
    ret = usb_ehci_bulk_init(epinfo, buffer, buflen);
    if (ret < 0) {
        goto errout_with_qh;
    }

    /* The transfer is in progress */
    usb_osal_mutex_give(epinfo->exclsem);
    return 0;

errout_with_qh:
    /* Clean-up after an error */
    if (epinfo->qh) {
        usb_ehci_qh_discard(epinfo->qh);
        epinfo->qh = NULL;
    }
errout_with_setup:
    epinfo->callback = NULL;
    epinfo->arg = NULL;
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;
}

int usbh_ep_intr_async_transfer(usbh_epinfo_t ep, uint8_t *buffer, uint32_t buflen, usbh_asynch_callback_t callback, void *arg)
{
    int ret;

    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;

    DEBUGASSERT(epinfo && buffer && buflen > 0);

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    /* Set the request for the callback well BEFORE initiating the transfer. */
    ret = usb_ehci_ioc_async_setup(epinfo, callback, arg);
    if (ret != 0) {
        goto errout_with_setup;
    }

    /* Check for errors in the setup of the transfer */
    ret = usb_ehci_intr_init(epinfo, buffer, buflen);
    if (ret < 0) {
        goto errout_with_qh;
    }

    /* The transfer is in progress */
    usb_osal_mutex_give(epinfo->exclsem);
    return 0;

errout_with_qh:
    /* Clean-up after an error */
    if (epinfo->qh) {
        usb_ehci_qh_discard(epinfo->qh);
        epinfo->qh = NULL;
    }
errout_with_setup:
    epinfo->callback = NULL;
    epinfo->arg = NULL;
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;
}
#endif
/****************************************************************************
 * Name: usb_ehci_cancel
 *
 * Description:
 *   Cancel a pending transfer on an endpoint.  Canceled synchronous or
 *   asynchronous transfer will complete normally with the error -ESHUTDOWN.
 *
 * Input Parameters:
 *   drvr - The USB host driver instance obtained as a parameter from the
 *          call to the class create() method.
 *   ep   - The IN or OUT endpoint descriptor for the device endpoint on
 *          which an asynchronous transfer should be transferred.
 *
 * Returned Value:
 *   On success, zero (OK) is returned. On a failure, a negated errno value
 *   is returned indicating the nature of the failure
 *
 ****************************************************************************/

int usb_ep_cancel(usbh_epinfo_t ep)
{
    struct usb_ehci_epinfo_s *epinfo = (struct usb_ehci_epinfo_s *)ep;
    struct usb_ehci_qh_s *qh;
#ifdef CONFIG_USBHOST_ASYNCH
    usbh_asynch_callback_t callback;
    void *arg;
#endif
    uint32_t *bp;
    uint32_t flags;
    bool iocwait;
    int ret;

    DEBUGASSERT(epinfo);

    /* We must have exclusive access to the EHCI hardware and data structures.
   * This will prevent servicing any transfer completion events while we
   * perform the cancellation, but will not prevent DMA-related race
   * conditions.
   *
   * REVISIT: This won't work.  This function must be callable from the
   * interrupt level.
   */

    ret = usb_osal_mutex_take(epinfo->exclsem);
    if (ret < 0) {
        return ret;
    }

    /* Sample and reset all transfer termination information.  This will
   * prevent any callbacks from occurring while are performing the
   * cancellation.  The transfer may still be in progress, however, so this
   * does not eliminate other DMA-related race conditions.
   */

    flags = usb_osal_enter_critical_section();
#ifdef CONFIG_USBHOST_ASYNCH
    callback = epinfo->callback;
    arg = epinfo->arg;
#endif
    iocwait = epinfo->iocwait;

#ifdef CONFIG_USBHOST_ASYNCH
    epinfo->callback = NULL;
    epinfo->arg = NULL;
#endif
    epinfo->iocwait = false;

    /* This will prevent any callbacks from occurring while are performing
   * the cancellation.  The transfer may still be in progress, however, so
   * this does not eliminate other DMA-related race conditions.
   */

    epinfo->callback = NULL;
    epinfo->arg = NULL;
    usb_osal_leave_critical_section(flags);

    /* Bail if there is no transfer in progress for this endpoint */

#ifdef CONFIG_USBHOST_ASYNCH
    if (callback == NULL && !iocwait)
#else
    if (!iocwait)
#endif
    {
        ret = 0;
        goto errout_with_sem;
    }

    /* Handle the cancellation according to the type of the transfer */

    switch (epinfo->xfrtype) {
        case USB_ENDPOINT_TYPE_CONTROL:
        case USB_ENDPOINT_TYPE_BULK: {
            /* Get the horizontal pointer from the head of the asynchronous
           * queue.
           */

            bp = (uint32_t *)&g_asynchead.hw.hlp;
            qh = (struct usb_ehci_qh_s *)
                usb_ehci_virtramaddr(usb_ehci_swap32(*bp) & QH_HLP_MASK);

            /* If the asynchronous queue is empty, then the forward point in
           * the asynchronous queue head will point back to the queue
           * head.
           */

            if (qh && qh != &g_asynchead) {
                /* Claim that we successfully cancelled the transfer */

                ret = 0;
                goto exit_terminate;
            }
        } break;

#ifndef CONFIG_USBHOST_INT_DISABLE
        case USB_ENDPOINT_TYPE_INTERRUPT: {
            /* Get the horizontal pointer from the head of the interrupt
           * queue.
           */

            bp = (uint32_t *)&g_intrhead.hw.hlp;
            qh = (struct usb_ehci_qh_s *)
                usb_ehci_virtramaddr(usb_ehci_swap32(*bp) & QH_HLP_MASK);
            if (qh) {
                /* if the queue is empty, then just claim that we successfully
               * cancelled the transfer.
               */

                ret = 0;
                goto exit_terminate;
            }
        } break;
#endif

        case USB_ENDPOINT_TYPE_ISOCHRONOUS:
        default:
            ret = -ENOSYS;
            goto errout_with_sem;
    }

    /* Find and remove the QH.  There are four possibilities:
    *
    * 1)  The transfer has already completed and the QH is no longer in the
    *     list.  In this case, sam_hq_foreach will return zero
    * 2a) The transfer is not active and still pending.  It was removed from
    *     the list and sam_hq_foreach will return one.
    * 2b) The is active but not yet complete.  This is currently handled the
    *     same as 2a).  REVISIT: This needs to be fixed.
    * 3)  Some bad happened and sam_hq_foreach returned an error code < 0.
    */

    ret = usb_ehci_qh_foreach(qh, &bp, usb_ehci_qh_cancel, epinfo);
    if (ret < 0) {
    }

    /* Was there a pending synchronous transfer? */

exit_terminate:
    epinfo->result = -ESHUTDOWN;
#ifdef CONFIG_USBHOST_ASYNCH
    if (iocwait) {
        /* Yes... wake it up */

        DEBUGASSERT(callback == NULL);
        usb_osal_sem_give(epinfo->iocsem);
    }

    /* No.. Is there a pending asynchronous transfer? */

    else /* if (callback != NULL) */
    {
        /* Yes.. perform the callback */

        callback(arg, -ESHUTDOWN);
    }

#else
    /* Wake up the waiting thread */

    usb_osal_sem_give(epinfo->iocsem);
#endif

errout_with_sem:
    usb_osal_mutex_give(epinfo->exclsem);
    return ret;
}

static void usb_ehci_bottomhalf(void *arg)
{
    uint32_t pending = (uint32_t)arg;

    /* USB Interrupt (USBINT)
    *
    *  "The Host Controller sets this bit to 1 on the completion of a USB
    *   transaction, which results in the retirement of a Transfer Descriptor
    *   that had its IOC bit set.
    *
    *  "The Host Controller also sets this bit to 1 when a short packet is
    *   detected (actual number of bytes received was less than the expected
    *   number of bytes)."
    *
    * USB Error Interrupt (USBERRINT)
    *
    *  "The Host Controller sets this bit to 1 when completion of a USB
    *   transaction results in an error condition (e.g., error counter
    *   underflow). If the TD on which the error interrupt occurred also
    *   had its IOC bit set, both this bit and USBINT bit are set. ..."
    *
    * We do the same thing in either case:  Traverse the asynchronous queue
    * and remove all of the transfers that are no longer active.
    */
    if ((pending & (EHCI_INT_USBINT | EHCI_INT_USBERRINT)) != 0) {
        usb_ehci_ioc_bottomhalf();
    }
    /* Port Change Detect
    *
    *  "The Host Controller sets this bit to a one when any port for which
    *   the Port Owner bit is set to zero ... has a change bit transition
    *   from a zero to a one or a Force Port Resume bit transition from a zero
    *   to a one as a result of a J-K transition detected on a suspended port.
    *   This bit will also be set as a result of the Connect Status Change
    *   being set to a one after system software has relinquished ownership
    *    of a connected port by writing a one to a port's Port Owner bit...
    *
    *  "This bit is allowed to be maintained in the Auxiliary power well.
    *   Alternatively, it is also acceptable that on a D3 to D0 transition
    *   of the EHCI HC device, this bit is loaded with the OR of all of the
    *   PORTSC change bits (including: Force port resume, over-current change,
    *   enable/disable change and connect status change)."
    */
    if ((pending & EHCI_INT_PORTSC) != 0) {
        usb_ehci_portsc_bottomhalf();
    }
    /* Frame List Rollover
    *
    *  "The Host Controller sets this bit to a one when the Frame List Index
    *   ... rolls over from its maximum value to zero. The exact value at
    *   which the rollover occurs depends on the frame list size. For example,
    *   if the frame list size (as programmed in the Frame List Size field of
    *   the USBCMD register) is 1024, the Frame Index Register rolls over
    *   every time FRINDEX[13] toggles. Similarly, if the size is 512, the
    *   Host Controller sets this bit to a one every time FRINDEX[12]
    *   toggles."
    */

#if 0 /* Not used */
        if ((pending & EHCI_INT_FLROLL) != 0)
            {

            }
#endif
    /* Host System Error
    *
    *  "The Host Controller sets this bit to 1 when a serious error occurs
    *   during a host system access involving the Host Controller module. ...
    *   When this error occurs, the Host Controller clears the Run/Stop bit
    *   in the Command register to prevent further execution of the scheduled
    *   TDs."
    */

    if ((pending & EHCI_INT_SYSERROR) != 0) {
    }
    /* Interrupt on Async Advance
    *
    *  "System software can force the host controller to issue an interrupt
    *   the next time the host controller advances the asynchronous schedule
    *   by writing a one to the Interrupt on Async Advance Doorbell bit in
    *   the USBCMD register. This status bit indicates the assertion of that
    *   interrupt source."
    */

    if ((pending & EHCI_INT_AAINT) != 0) {
    }
}

void USBH_IRQHandler(void)
{
    uint32_t usbsts;
    uint32_t pending;
    uint32_t regval;

    /* Read Interrupt Status and mask out interrupts that are not enabled. */

    usbsts = usb_ehci_getreg(&HCOR->usbsts);
    regval = usb_ehci_getreg(&HCOR->usbintr);

    /* Handle all unmasked interrupt sources */
    pending = usbsts & regval;

    if (pending != 0) {
        /* Disable further EHCI interrupts so that we do not overrun the work queue.*/
        usb_ehci_putreg(0, &HCOR->usbintr);
        /* Clear all pending status bits by writing the value of the pending
        * interrupt bits back to the status register.
        */
        usb_ehci_putreg(usbsts & EHCI_INT_ALLINTS, &HCOR->usbsts);

        usb_ehci_bottomhalf((void *)pending);

        /* Re-enable relevant EHCI interrupts.  Interrupts should still be enabled
        * at the level of the interrupt controller.
        */
        usb_ehci_putreg(EHCI_HANDLED_INTS, &HCOR->usbintr);
    }
}