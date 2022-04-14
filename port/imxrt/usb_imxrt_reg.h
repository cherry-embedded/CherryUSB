#ifndef __USB_IMXRT_REG_H__
#define __USB_IMXRT_REG_H__

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define   __I     volatile const       /*!< Defines 'read only' permissions */
#define   __IO    volatile             /*!< Defines 'read / write' permissions */
/* ----------------------------------------------------------------------------
   -- USB Peripheral Access Layer
   ---------------------------------------------------------------------------- */

/*!
 * @addtogroup USB_Peripheral_Access_Layer USB Peripheral Access Layer
 * @{
 */

/** USB - Register Layout Typedef */
// typedef struct {
//   __I  uint32_t ID;                                /**< Identification register, offset: 0x0 */
//   __I  uint32_t HWGENERAL;                         /**< Hardware General, offset: 0x4 */
//   __I  uint32_t HWHOST;                            /**< Host Hardware Parameters, offset: 0x8 */
//   __I  uint32_t HWDEVICE;                          /**< Device Hardware Parameters, offset: 0xC */
//   __I  uint32_t HWTXBUF;                           /**< TX Buffer Hardware Parameters, offset: 0x10 */
//   __I  uint32_t HWRXBUF;                           /**< RX Buffer Hardware Parameters, offset: 0x14 */
//        uint8_t RESERVED_0[104];
//   __IO uint32_t GPTIMER0LD;                        /**< General Purpose Timer #0 Load, offset: 0x80 */
//   __IO uint32_t GPTIMER0CTRL;                      /**< General Purpose Timer #0 Controller, offset: 0x84 */
//   __IO uint32_t GPTIMER1LD;                        /**< General Purpose Timer #1 Load, offset: 0x88 */
//   __IO uint32_t GPTIMER1CTRL;                      /**< General Purpose Timer #1 Controller, offset: 0x8C */
//   __IO uint32_t SBUSCFG;                           /**< System Bus Config, offset: 0x90 */
//        uint8_t RESERVED_1[108];
//   __I  uint8_t CAPLENGTH;                          /**< Capability Registers Length, offset: 0x100 */
//        uint8_t RESERVED_2[1];
//   __I  uint16_t HCIVERSION;                        /**< Host Controller Interface Version, offset: 0x102 */
//   __I  uint32_t HCSPARAMS;                         /**< Host Controller Structural Parameters, offset: 0x104 */
//   __I  uint32_t HCCPARAMS;                         /**< Host Controller Capability Parameters, offset: 0x108 */
//        uint8_t RESERVED_3[20];
//   __I  uint16_t DCIVERSION;                        /**< Device Controller Interface Version, offset: 0x120 */
//        uint8_t RESERVED_4[2];
//   __I  uint32_t DCCPARAMS;                         /**< Device Controller Capability Parameters, offset: 0x124 */
//        uint8_t RESERVED_5[24];
//   __IO uint32_t USBCMD;                            /**< USB Command Register, offset: 0x140 */
//   __IO uint32_t USBSTS;                            /**< USB Status Register, offset: 0x144 */
//   __IO uint32_t USBINTR;                           /**< Interrupt Enable Register, offset: 0x148 */
//   __IO uint32_t FRINDEX;                           /**< USB Frame Index, offset: 0x14C */
//        uint8_t RESERVED_6[4];
//   union {                                          /* offset: 0x154 */
//     __IO uint32_t DEVICEADDR;                        /**< Device Address, offset: 0x154 */
//     __IO uint32_t PERIODICLISTBASE;                  /**< Frame List Base Address, offset: 0x154 */
//   };
//   union {                                          /* offset: 0x158 */
//     __IO uint32_t ASYNCLISTADDR;                     /**< Next Asynch. Address, offset: 0x158 */
//     __IO uint32_t ENDPTLISTADDR;                     /**< Endpoint List Address, offset: 0x158 */
//   };
//        uint8_t RESERVED_7[4];
//   __IO uint32_t BURSTSIZE;                         /**< Programmable Burst Size, offset: 0x160 */
//   __IO uint32_t TXFILLTUNING;                      /**< TX FIFO Fill Tuning, offset: 0x164 */
//        uint8_t RESERVED_8[16];
//   __IO uint32_t ENDPTNAK;                          /**< Endpoint NAK, offset: 0x178 */
//   __IO uint32_t ENDPTNAKEN;                        /**< Endpoint NAK Enable, offset: 0x17C */
//   __I  uint32_t CONFIGFLAG;                        /**< Configure Flag Register, offset: 0x180 */
//   __IO uint32_t PORTSC1;                           /**< Port Status & Control, offset: 0x184 */
//        uint8_t RESERVED_9[28];
//   __IO uint32_t OTGSC;                             /**< On-The-Go Status & control, offset: 0x1A4 */
//   __IO uint32_t USBMODE;                           /**< USB Device Mode, offset: 0x1A8 */
//   __IO uint32_t ENDPTSETUPSTAT;                    /**< Endpoint Setup Status, offset: 0x1AC */
//   __IO uint32_t ENDPTPRIME;                        /**< Endpoint Prime, offset: 0x1B0 */
//   __IO uint32_t ENDPTFLUSH;                        /**< Endpoint Flush, offset: 0x1B4 */
//   __I  uint32_t ENDPTSTAT;                         /**< Endpoint Status, offset: 0x1B8 */
//   __IO uint32_t ENDPTCOMPLETE;                     /**< Endpoint Complete, offset: 0x1BC */
//   __IO uint32_t ENDPTCTRL0;                        /**< Endpoint Control0, offset: 0x1C0 */
//   __IO uint32_t ENDPTCTRL[7];                      /**< Endpoint Control 1..Endpoint Control 7, array offset: 0x1C4, array step: 0x4 */
// } USB_Type;


/*! @brief The maximum value of ISO type maximum packet size for HS in USB specification 2.0 */
#define USB_DEVICE_MAX_HS_ISO_MAX_PACKET_SIZE (1024U)
/*! @brief The maximum value of interrupt type maximum packet size for HS in USB specification 2.0 */
#define USB_DEVICE_MAX_HS_INTERUPT_MAX_PACKET_SIZE (1024U)
/*! @brief The maximum value of bulk type maximum packet size for HS in USB specification 2.0 */
#define USB_DEVICE_MAX_HS_BULK_MAX_PACKET_SIZE (512U)
/*! @brief The maximum value of control type maximum packet size for HS in USB specification 2.0 */
#define USB_DEVICE_MAX_HS_CONTROL_MAX_PACKET_SIZE (64U)
#define USB_DEVICE_MAX_TRANSFER_PRIME_TIMES \
    (10000000U) /* The max prime times of EPPRIME, if still doesn't take effect, means status has been reset*/

/* Device QH */
#define USB_DEVICE_EHCI_QH_POINTER_MASK                 (0xFFFFFFC0U)
#define USB_DEVICE_EHCI_QH_MULT_MASK                    (0xC0000000U)
#define USB_DEVICE_EHCI_QH_ZLT_MASK                     (0x20000000U)
#define USB_DEVICE_EHCI_QH_MAX_PACKET_SIZE_MASK         (0x07FF0000U)
#define USB_DEVICE_EHCI_QH_MAX_PACKET_SIZE              (0x00000800U)
#define USB_DEVICE_EHCI_QH_IOS_MASK                     (0x00008000U)

/* Device DTD */
#define USB_DEVICE_ECHI_DTD_POINTER_MASK                (0xFFFFFFE0U)
#define USB_DEVICE_ECHI_DTD_TERMINATE_MASK              (0x00000001U)
#define USB_DEVICE_ECHI_DTD_PAGE_MASK                   (0xFFFFF000U)
#define USB_DEVICE_ECHI_DTD_PAGE_OFFSET_MASK            (0x00000FFFU)
#define USB_DEVICE_ECHI_DTD_PAGE_BLOCK                  (0x00001000U)
#define USB_DEVICE_ECHI_DTD_TOTAL_BYTES_MASK            (0x7FFF0000U)
#define USB_DEVICE_ECHI_DTD_TOTAL_BYTES                 (0x00004000U)
#define USB_DEVICE_ECHI_DTD_IOC_MASK                    (0x00008000U)
#define USB_DEVICE_ECHI_DTD_MULTIO_MASK                 (0x00000C00U)
#define USB_DEVICE_ECHI_DTD_STATUS_MASK                 (0x000000FFU)
#define USB_DEVICE_EHCI_DTD_STATUS_ERROR_MASK           (0x00000068U)
#define USB_DEVICE_ECHI_DTD_STATUS_ACTIVE               (0x00000080U)
#define USB_DEVICE_ECHI_DTD_STATUS_HALTED               (0x00000040U)
#define USB_DEVICE_ECHI_DTD_STATUS_DATA_BUFFER_ERROR    (0x00000020U)
#define USB_DEVICE_ECHI_DTD_STATUS_TRANSACTION_ERROR    (0x00000008U)

#define USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_MASK (0x1800u)
#define USB_DESCRIPTOR_ENDPOINT_MAXPACKETSIZE_MULT_TRANSACTIONS_SHFIT (11U)

/*! @brief Define current endian */
#ifndef ENDIANNESS
#define ENDIANNESS USB_LITTLE_ENDIAN
#endif

/*
 * The following MACROs (USB_GLOBAL, USB_BDT, USB_RAM_ADDRESS_ALIGNMENT, etc) are only used for USB device stack.
 * The USB device global variables are put into the section m_usb_global and m_usb_bdt
 * by using the MACRO USB_GLOBAL and USB_BDT. In this way, the USB device
 * global variables can be linked into USB dedicated RAM by USB_STACK_USE_DEDICATED_RAM.
 * The MACRO USB_STACK_USE_DEDICATED_RAM is used to decide the USB stack uses dedicated RAM or not. The value of
 * the macro can be set as 0, USB_STACK_DEDICATED_RAM_TYPE_BDT_GLOBAL, or USB_STACK_DEDICATED_RAM_TYPE_BDT.
 * The MACRO USB_STACK_DEDICATED_RAM_TYPE_BDT_GLOBAL means USB device global variables, including USB_BDT and
 * USB_GLOBAL, are put into the USB dedicated RAM. This feature can only be enabled when the USB dedicated RAM
 * is not less than 2K Bytes.
 * The MACRO USB_STACK_DEDICATED_RAM_TYPE_BDT means USB device global variables, only including USB_BDT, are put
 * into the USB dedicated RAM, the USB_GLOBAL will be put into .bss section. This feature is used for some SOCs,
 * the USB dedicated RAM size is not more than 512 Bytes.
 */
#if defined(__ICCARM__)

#define USB_WEAK_VAR __attribute__((weak))
#define USB_WEAK_FUN __attribute__((weak))
/* disable misra 19.13 */
_Pragma("diag_suppress=Pm120")
#define USB_ALIGN_PRAGMA(x) _Pragma(#x)
    _Pragma("diag_default=Pm120")

#define USB_RAM_ADDRESS_ALIGNMENT(n) USB_ALIGN_PRAGMA(data_alignment = n)
        _Pragma("diag_suppress=Pm120")
#define USB_LINK_SECTION_PART(str)  _Pragma(#str)
#define USB_LINK_DMA_INIT_DATA(sec) USB_LINK_SECTION_PART(location = #sec)
#define USB_LINK_USB_GLOBAL         _Pragma("location = \"m_usb_global\"")
#define USB_LINK_USB_BDT            _Pragma("location = \"m_usb_bdt\"")
#define USB_LINK_USB_GLOBAL_BSS
#define USB_LINK_USB_BDT_BSS
            _Pragma("diag_default=Pm120")
#define USB_LINK_DMA_NONINIT_DATA      _Pragma("location = \"m_usb_dma_noninit_data\"")
#define USB_LINK_NONCACHE_NONINIT_DATA _Pragma("location = \"NonCacheable\"")
#elif defined(__CC_ARM) || (defined(__ARMCC_VERSION))

#define USB_WEAK_VAR                 __attribute__((weak))
#define USB_WEAK_FUN                 __attribute__((weak))
#define USB_RAM_ADDRESS_ALIGNMENT(n) __attribute__((aligned(n)))
#define USB_LINK_DMA_INIT_DATA(sec)  __attribute__((section(#sec)))
#if defined(__CC_ARM)
#define USB_LINK_USB_GLOBAL __attribute__((section("m_usb_global"))) __attribute__((zero_init))
#else
#define USB_LINK_USB_GLOBAL __attribute__((section(".bss.m_usb_global")))
#endif
#if defined(__CC_ARM)
#define USB_LINK_USB_BDT __attribute__((section("m_usb_bdt"))) __attribute__((zero_init))
#else
#define USB_LINK_USB_BDT __attribute__((section(".bss.m_usb_bdt")))
#endif
#define USB_LINK_USB_GLOBAL_BSS
#define USB_LINK_USB_BDT_BSS
#if defined(__CC_ARM)
#define USB_LINK_DMA_NONINIT_DATA __attribute__((section("m_usb_dma_noninit_data"))) __attribute__((zero_init))
#else
#define USB_LINK_DMA_NONINIT_DATA __attribute__((section(".bss.m_usb_dma_noninit_data")))
#endif
#if defined(__CC_ARM)
#define USB_LINK_NONCACHE_NONINIT_DATA __attribute__((section("NonCacheable"))) __attribute__((zero_init))
#else
#define USB_LINK_NONCACHE_NONINIT_DATA __attribute__((section(".bss.NonCacheable")))
#endif

#elif defined(__GNUC__)

#define USB_WEAK_VAR                 __attribute__((weak))
#define USB_WEAK_FUN                 __attribute__((weak))
#define USB_RAM_ADDRESS_ALIGNMENT(n) __attribute__((aligned(n)))
#define USB_LINK_DMA_INIT_DATA(sec)  __attribute__((section(#sec)))
#define USB_LINK_USB_GLOBAL          __attribute__((section("m_usb_global, \"aw\", %nobits @")))
#define USB_LINK_USB_BDT             __attribute__((section("m_usb_bdt, \"aw\", %nobits @")))
#define USB_LINK_USB_GLOBAL_BSS
#define USB_LINK_USB_BDT_BSS
#define USB_LINK_DMA_NONINIT_DATA      __attribute__((section("m_usb_dma_noninit_data, \"aw\", %nobits @")))
#define USB_LINK_NONCACHE_NONINIT_DATA __attribute__((section("NonCacheable, \"aw\", %nobits @")))

#elif (defined(__DSC__) && defined(__CW__))
#define MAX(a, b)                    (((a) > (b)) ? (a) : (b))
#define USB_WEAK_VAR                 __attribute__((weak))
#define USB_WEAK_FUN                 __attribute__((weak))
#define USB_RAM_ADDRESS_ALIGNMENT(n) __attribute__((aligned(n)))
#define USB_LINK_USB_BDT_BSS
#define USB_LINK_USB_GLOBAL_BSS
#else
#error The tool-chain is not supported.
#endif

#if (defined(USB_DEVICE_CONFIG_BUFFER_PROPERTY_CACHEABLE) && (USB_DEVICE_CONFIG_BUFFER_PROPERTY_CACHEABLE)) || \
    (defined(USB_HOST_CONFIG_BUFFER_PROPERTY_CACHEABLE) && (USB_HOST_CONFIG_BUFFER_PROPERTY_CACHEABLE))
#define USB_DMA_DATA_NONINIT_SUB USB_LINK_DMA_NONINIT_DATA
#define USB_DMA_DATA_INIT_SUB    USB_LINK_DMA_INIT_DATA(m_usb_dma_init_data)
#define USB_CONTROLLER_DATA      USB_LINK_NONCACHE_NONINIT_DATA
#else
#if (defined(DATA_SECTION_IS_CACHEABLE) && (DATA_SECTION_IS_CACHEABLE))
#define USB_DMA_DATA_NONINIT_SUB USB_LINK_NONCACHE_NONINIT_DATA
#define USB_DMA_DATA_INIT_SUB    USB_LINK_DMA_INIT_DATA(NonCacheable.init)
#define USB_CONTROLLER_DATA      USB_LINK_NONCACHE_NONINIT_DATA
#else
#define USB_DMA_DATA_NONINIT_SUB
#define USB_DMA_DATA_INIT_SUB
#define USB_CONTROLLER_DATA USB_LINK_USB_GLOBAL
#endif
#endif

/*! @brief Endpoint initialization structure */
typedef struct _usb_device_endpoint_init_struct
{
    uint16_t maxPacketSize;  /*!< Endpoint maximum packet size */
    uint8_t endpointAddress; /*!< Endpoint address*/
    uint8_t transferType;    /*!< Endpoint transfer type*/
    uint8_t zlt;             /*!< ZLT flag*/
    uint8_t interval;        /*!< Endpoint interval*/
} usb_device_endpoint_init_struct_t;

typedef struct _usb_device_ehci_qh_struct
{
    union
    {
        volatile uint32_t capabilttiesCharacteristics;
        struct
        {
            volatile uint32_t reserved1 : 15;
            volatile uint32_t ios : 1;
            volatile uint32_t maxPacketSize : 11;
            volatile uint32_t reserved2 : 2;
            volatile uint32_t zlt : 1;
            volatile uint32_t mult : 2;
        } capabilttiesCharacteristicsBitmap;
    } capabilttiesCharacteristicsUnion;
    volatile uint32_t currentDtdPointer;
    volatile uint32_t nextDtdPointer;
    union
    {
        volatile uint32_t dtdToken;
        struct
        {
            volatile uint32_t status : 8;
            volatile uint32_t reserved1 : 2;
            volatile uint32_t multiplierOverride : 2;
            volatile uint32_t reserved2 : 3;
            volatile uint32_t ioc : 1;
            volatile uint32_t totalBytes : 15;
            volatile uint32_t reserved3 : 1;
        } dtdTokenBitmap;
    } dtdTokenUnion;
    volatile uint32_t bufferPointerPage[5];
    volatile uint32_t reserved1;
    uint32_t setupBuffer[2];
    uint32_t setupBufferBack[2];
    union
    {
        uint32_t endpointStatus;
        struct
        {
            uint32_t isOpened : 1;
            uint32_t zlt : 1;
            uint32_t : 30;
        } endpointStatusBitmap;
    } endpointStatusUnion;
    uint32_t reserved2;
} usb_device_ehci_qh_struct_t;

typedef struct _usb_device_ehci_dtd_struct
{
    volatile uint32_t nextDtdPointer;
    union
    {
        volatile uint32_t dtdToken;
        struct
        {
            volatile uint32_t status : 8;
            volatile uint32_t reserved1 : 2;
            volatile uint32_t multiplierOverride : 2;
            volatile uint32_t reserved2 : 3;
            volatile uint32_t ioc : 1;
            volatile uint32_t totalBytes : 15;
            volatile uint32_t reserved3 : 1;
        } dtdTokenBitmap;
    } dtdTokenUnion;
    volatile uint32_t bufferPointerPage[5];
    union
    {
        volatile uint32_t reserved;
        struct
        {
            uint32_t originalBufferOffest : 12;
            uint32_t originalBufferLength : 19;
            uint32_t dtdInvalid : 1;
        } originalBufferInfo;
    } reservedUnion;
} usb_device_ehci_dtd_struct_t;

/*! @brief EHCI state structure */
typedef struct _usb_device_ehci_state_struct
{
    USB_Type *registerBase;                       /*!< The base address of the register */
#if (defined(USB_DEVICE_CONFIG_LOW_POWER_MODE) && (USB_DEVICE_CONFIG_LOW_POWER_MODE > 0U))
    USBPHY_Type *registerPhyBase;                   /*!< The base address of the PHY register */
#if (defined(FSL_FEATURE_SOC_USBNC_COUNT) && (FSL_FEATURE_SOC_USBNC_COUNT > 0U))
    USBNC_Type *registerNcBase;                     /*!< The base address of the USBNC register */
#endif
#endif
    usb_device_ehci_qh_struct_t *qh;                /*!< The QH structure base address */
    usb_device_ehci_dtd_struct_t *dtd;              /*!< The DTD structure base address */
    usb_device_ehci_dtd_struct_t *dtdFree;          /*!< The idle DTD list head */
    usb_device_ehci_dtd_struct_t
        *dtdHead[USB_DEVICE_ENDPOINTS * 2];         /*!< The transferring DTD list head for each endpoint */
    usb_device_ehci_dtd_struct_t
        *dtdTail[USB_DEVICE_ENDPOINTS * 2];         /*!< The transferring DTD list tail for each endpoint */
    uint8_t dtdCount;                               /*!< The idle DTD node count */
    uint8_t endpointCount;                          /*!< The endpoint number of EHCI */
    uint8_t isResetting;                            /*!< Whether a PORT reset is occurring or not  */
    uint8_t controllerId;                           /*!< Controller ID */
    uint8_t speed;                                  /*!< Current speed of EHCI */
    uint8_t isSuspending;                           /*!< Is suspending of the PORT */
} usb_device_ehci_state_struct_t;

/*! @brief USB error code */
typedef enum _usb_status
{
    kStatus_USB_Success = 0x00U, /*!< Success */
    kStatus_USB_Error,           /*!< Failed */

    kStatus_USB_Busy,                       /*!< Busy */
    kStatus_USB_InvalidHandle,              /*!< Invalid handle */
    kStatus_USB_InvalidParameter,           /*!< Invalid parameter */
    kStatus_USB_InvalidRequest,             /*!< Invalid request */
    kStatus_USB_ControllerNotFound,         /*!< Controller cannot be found */
    kStatus_USB_InvalidControllerInterface, /*!< Invalid controller interface */

    kStatus_USB_NotSupported,   /*!< Configuration is not supported */
    kStatus_USB_Retry,          /*!< Enumeration get configuration retry */
    kStatus_USB_TransferStall,  /*!< Transfer stalled */
    kStatus_USB_TransferFailed, /*!< Transfer failed */
    kStatus_USB_AllocFail,      /*!< Allocation failed */
    kStatus_USB_LackSwapBuffer, /*!< Insufficient swap buffer for KHCI */
    kStatus_USB_TransferCancel, /*!< The transfer cancelled */
    kStatus_USB_BandwidthFail,  /*!< Allocate bandwidth failed */
    kStatus_USB_MSDStatusFail,  /*!< For MSD, the CSW status means fail */
    kStatus_USB_EHCIAttached,
    kStatus_USB_EHCIDetached,
    kStatus_USB_DataOverRun, /*!< The amount of data returned by the endpoint exceeded
                                  either the size of the maximum data packet allowed
                                  from the endpoint or the remaining buffer size. */
} usb_status_t;

#endif /* __USB_IMXRT_REG_H__ */
/* End of file ***************************************************************************/
