#include "usbd_core.h"
#include "usbd_cdc.h"
#include "DAP_config.h"
#include "DAP.h"

#define DAP_IN_EP  0x81
#define DAP_OUT_EP 0x02

#define CDC_IN_EP  0x83
#define CDC_OUT_EP 0x04
#define CDC_INT_EP 0x85

#define USBD_VID           0xd6e7
#define USBD_PID           0x3507
#define USBD_MAX_POWER     500
#define USBD_LANGID_STRING 1033

#define CMSIS_DAP_INTERFACE_SIZE (9 + 7 + 7)
#define USB_CONFIG_SIZE          (9 + CMSIS_DAP_INTERFACE_SIZE + CDC_ACM_DESCRIPTOR_LEN)

#ifdef CONFIG_USB_HS
#define CDC_MAX_MPS 512
#else
#define CDC_MAX_MPS 64
#endif

#define WCID_VENDOR_CODE 0x01

__ALIGN_BEGIN const uint8_t WCID_StringDescriptor_MSOS[18] __ALIGN_END = {
    ///////////////////////////////////////
    /// MS OS string descriptor
    ///////////////////////////////////////
    0x12,                       /* bLength */
    USB_DESCRIPTOR_TYPE_STRING, /* bDescriptorType */
    /* MSFT100 */
    'M', 0x00, 'S', 0x00, 'F', 0x00, 'T', 0x00, /* wcChar_7 */
    '1', 0x00, '0', 0x00, '0', 0x00,            /* wcChar_7 */
    WCID_VENDOR_CODE,                           /* bVendorCode */
    0x00,                                       /* bReserved */
};

__ALIGN_BEGIN const uint8_t WINUSB_WCIDDescriptor[40] __ALIGN_END = {
    ///////////////////////////////////////
    /// WCID descriptor
    ///////////////////////////////////////
    0x28, 0x00, 0x00, 0x00,                   /* dwLength */
    0x00, 0x01,                               /* bcdVersion */
    0x04, 0x00,                               /* wIndex */
    0x01,                                     /* bCount */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* bReserved_7 */

    ///////////////////////////////////////
    /// WCID function descriptor
    ///////////////////////////////////////
    0x00, /* bFirstInterfaceNumber */
    0x01, /* bReserved */
    /* WINUSB */
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, /* cCID_8 */
    /*  */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* cSubCID_8 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,             /* bReserved_6 */
};

__ALIGN_BEGIN const uint8_t WINUSB_IF0_WCIDProperties[142] __ALIGN_END = {
    ///////////////////////////////////////
    /// WCID property descriptor
    ///////////////////////////////////////
    0x8e, 0x00, 0x00, 0x00, /* dwLength */
    0x00, 0x01,             /* bcdVersion */
    0x05, 0x00,             /* wIndex */
    0x01, 0x00,             /* wCount */

    ///////////////////////////////////////
    /// registry propter descriptor
    ///////////////////////////////////////
    0x84, 0x00, 0x00, 0x00, /* dwSize */
    0x01, 0x00, 0x00, 0x00, /* dwPropertyDataType */
    0x28, 0x00,             /* wPropertyNameLength */
    /* DeviceInterfaceGUID */
    'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00,  /* wcName_20 */
    'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00,  /* wcName_20 */
    't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00,  /* wcName_20 */
    'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00,  /* wcName_20 */
    'U', 0x00, 'I', 0x00, 'D', 0x00, 0x00, 0x00, /* wcName_20 */
    0x4e, 0x00, 0x00, 0x00,                      /* dwPropertyDataLength */
    /* {CDB3B5AD-293B-4663-AA36-1AAE46463776} */
    '{', 0x00, 'C', 0x00, 'D', 0x00, 'B', 0x00, /* wcData_39 */
    '3', 0x00, 'B', 0x00, '5', 0x00, 'A', 0x00, /* wcData_39 */
    'D', 0x00, '-', 0x00, '2', 0x00, '9', 0x00, /* wcData_39 */
    '3', 0x00, 'B', 0x00, '-', 0x00, '4', 0x00, /* wcData_39 */
    '6', 0x00, '6', 0x00, '3', 0x00, '-', 0x00, /* wcData_39 */
    'A', 0x00, 'A', 0x00, '3', 0x00, '6', 0x00, /* wcData_39 */
    '-', 0x00, '1', 0x00, 'A', 0x00, 'A', 0x00, /* wcData_39 */
    'E', 0x00, '4', 0x00, '6', 0x00, '4', 0x00, /* wcData_39 */
    '6', 0x00, '3', 0x00, '7', 0x00, '7', 0x00, /* wcData_39 */
    '6', 0x00, '}', 0x00, 0x00, 0x00,           /* wcData_39 */
};

struct usb_msosv1_descriptor msosv1_desc = {
    .string = (uint8_t *)WCID_StringDescriptor_MSOS,
    .string_len = 18,
    .vendor_code = WCID_VENDOR_CODE,
    .compat_id = (uint8_t *)WINUSB_WCIDDescriptor,
    .compat_id_len = sizeof(WINUSB_WCIDDescriptor),
    .comp_id_property = (uint8_t *)WINUSB_IF0_WCIDProperties,
    .comp_id_property_len = sizeof(WINUSB_IF0_WCIDProperties),
};

const uint8_t daplink_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01),
    /* Configuration 0 */
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x03, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /* Interface 0 */
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0xFF, 0x00, 0x00, 0x02),
    /* Endpoint OUT 1 */
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_IN_EP, USB_ENDPOINT_TYPE_BULK, CDC_MAX_MPS, 0x00),
    /* Endpoint IN 2 */
    USB_ENDPOINT_DESCRIPTOR_INIT(DAP_OUT_EP, USB_ENDPOINT_TYPE_BULK, CDC_MAX_MPS, 0x00),
    CDC_ACM_DESCRIPTOR_INIT(0x01, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 0x00),
    /* String 0 (LANGID) */
    USB_LANGID_INIT(USBD_LANGID_STRING),
    /* String 1 (Manufacturer) */
    0x14,                       /* bLength */
    USB_DESCRIPTOR_TYPE_STRING, /* bDescriptorType */
    'C', 0x00,                  /* wcChar0 */
    'h', 0x00,                  /* wcChar1 */
    'e', 0x00,                  /* wcChar2 */
    'r', 0x00,                  /* wcChar3 */
    'r', 0x00,                  /* wcChar4 */
    'y', 0x00,                  /* wcChar5 */
    'U', 0x00,                  /* wcChar6 */
    'S', 0x00,                  /* wcChar7 */
    'B', 0x00,                  /* wcChar8 */
    /* String 2 (Product) */
    0x28,                       // bLength
    USB_DESCRIPTOR_TYPE_STRING, // bDescriptorType
    'C', 0x00,                  // wcChar0
    'h', 0x00,                  // wcChar1
    'e', 0x00,                  // wcChar2
    'r', 0x00,                  // wcChar3
    'r', 0x00,                  // wcChar4
    'y', 0x00,                  // wcChar5
    'U', 0x00,                  // wcChar6
    'S', 0x00,                  // wcChar7
    'B', 0x00,                  // wcChar8
    ' ', 0x00,                  // wcChar9
    'C', 0x00,                  // wcChar10
    'M', 0x00,                  // wcChar11
    'S', 0x00,                  // wcChar12
    'I', 0x00,                  // wcChar13
    'S', 0x00,                  // wcChar14
    '-', 0x00,                  // wcChar15
    'D', 0x00,                  // wcChar16
    'A', 0x00,                  // wcChar17
    'P', 0x00,                  // wcChar18
    /* String 3 (Serial Number) */
    0x1A,                       // bLength
    USB_DESCRIPTOR_TYPE_STRING, // bDescriptorType
    '0', 0,                     // wcChar0
    '1', 0,                     // wcChar1
    '2', 0,                     // wcChar2
    '3', 0,                     // wcChar3
    '4', 0,                     // wcChar4
    '5', 0,                     // wcChar5
    'A', 0,                     // wcChar6
    'B', 0,                     // wcChar7
    'C', 0,                     // wcChar8
    'D', 0,                     // wcChar9
    'E', 0,                     // wcChar10
    'F', 0,                     // wcChar11
#ifdef CONFIG_USB_HS
    /* Device Qualifier */
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x10,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
#endif
    /* End */
    0x00
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t cdc_read_buffer[CDC_MAX_MPS];

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t USB_Request[DAP_PACKET_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t USB_Response[DAP_PACKET_SIZE];

volatile bool ep_tx_busy_flag = false;

void usbd_configure_done_callback(void)
{
    /* setup first out ep read transfer */
    usbd_ep_start_read(CDC_OUT_EP, cdc_read_buffer, CDC_MAX_MPS);
    usbd_ep_start_read(DAP_OUT_EP, USB_Request, DAP_PACKET_SIZE);
}

void usbd_cdc_acm_bulk_out(uint8_t ep, uint32_t nbytes)
{
    /* setup next out ep read transfer */
    if (1) {
        usbd_ep_start_read(CDC_OUT_EP, cdc_read_buffer, CDC_MAX_MPS);
    }
}

void usbd_cdc_acm_bulk_in(uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % CDC_MAX_MPS) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(CDC_IN_EP, NULL, 0);
    } else {
        ep_tx_busy_flag = false;
    }
}

void usb_dap_recv_callback(uint8_t ep, uint32_t nbytes)
{
    uint32_t actual_len;
    if (USB_Request[0] == ID_DAP_TransferAbort) {
        usbd_ep_start_read(ep, USB_Request, DAP_PACKET_SIZE);
        DAP_TransferAbort = 1;
        return;
    }
    actual_len = DAP_ProcessCommand(USB_Request, USB_Response);

    usbd_ep_start_write(DAP_IN_EP, USB_Response, actual_len);
    usbd_ep_start_read(ep, USB_Request, DAP_PACKET_SIZE);
}

void usb_dap_send_callback(uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % CDC_MAX_MPS) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(DAP_IN_EP, NULL, 0);
    } else {
        ep_tx_busy_flag = false;
    }
}

static struct usbd_endpoint cdc_out_ep = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = usbd_cdc_acm_bulk_out
};

static struct usbd_endpoint cdc_in_ep = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = usbd_cdc_acm_bulk_in
};

static struct usbd_endpoint dap_out_ep = {
    .ep_addr = DAP_OUT_EP,
    .ep_cb = usb_dap_recv_callback
};
static struct usbd_endpoint dap_in_ep = {
    .ep_addr = DAP_IN_EP,
    .ep_cb = usb_dap_send_callback
};

struct usbd_interface dap_interface;
struct usbd_interface intf1;
struct usbd_interface intf2;

void daplink_init(void)
{
    usbd_desc_register(daplink_descriptor);
    usbd_msosv1_desc_register(&msosv1_desc);

    /*!< winusb */
    usbd_add_interface(&dap_interface);
    usbd_add_endpoint(&dap_out_ep);
    usbd_add_endpoint(&dap_in_ep);

    /*!< cdc acm */
    usbd_add_interface(usbd_cdc_acm_init_intf(&intf1));
    usbd_add_interface(usbd_cdc_acm_init_intf(&intf2));
    usbd_add_endpoint(&cdc_out_ep);
    usbd_add_endpoint(&cdc_in_ep);

    usbd_initialize();
}