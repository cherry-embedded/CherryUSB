#include "usbd_core.h"
#include "usb_printer.h"

/*!< endpoint address */
#define PRINTER_IN_EP          0x81
#define PRINTER_IN_EP_SIZE     0x40
#define PRINTER_OUT_EP         0x02
#define PRINTER_OUT_EP_SIZE    0x40

#define USBD_VID           0x5A5A
#define USBD_PID           0xA5A5
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 0x0409

/*!< config descriptor size */
#define USB_CONFIG_SIZE (32)

/*!< global descriptor */
static const uint8_t printer_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0000, 0x01),
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_SELF_POWERED, USBD_MAX_POWER),
    USB_INTERFACE_DESCRIPTOR_INIT(0x00, 0x00, 0x02, 0x07, 0x01, 0x02, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(PRINTER_IN_EP, 0x02, PRINTER_IN_EP_SIZE, 0x00),
    USB_ENDPOINT_DESCRIPTOR_INIT(PRINTER_OUT_EP, 0x02, PRINTER_OUT_EP_SIZE, 0x00),
    ///////////////////////////////////////
    /// string0 descriptor
    ///////////////////////////////////////
    USB_LANGID_INIT(USBD_LANGID_STRING),
    ///////////////////////////////////////
    /// string1 descriptor
    ///////////////////////////////////////
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
    ///////////////////////////////////////
    /// string2 descriptor
    ///////////////////////////////////////
    0x2A,                       /* bLength */
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
    ' ', 0x00,                  /* wcChar9 */
    'P', 0x00,                  /* wcChar10 */
    'R', 0x00,                  /* wcChar11 */
    'I', 0x00,                  /* wcChar12 */
    'N', 0x00,                  /* wcChar13 */
    'T', 0x00,                  /* wcChar14 */
    ' ', 0x00,                  /* wcChar15 */
    'D', 0x00,                  /* wcChar16 */
    'E', 0x00,                  /* wcChar17 */
    'M', 0x00,                  /* wcChar18 */
    'O', 0x00,                  /* wcChar19 */
    ///////////////////////////////////////
    /// string3 descriptor
    ///////////////////////////////////////
    0x16,                       /* bLength */
    USB_DESCRIPTOR_TYPE_STRING, /* bDescriptorType */
    '2', 0x00,                  /* wcChar0 */
    '0', 0x00,                  /* wcChar1 */
    '2', 0x00,                  /* wcChar2 */
    '2', 0x00,                  /* wcChar3 */
    '1', 0x00,                  /* wcChar4 */
    '2', 0x00,                  /* wcChar5 */
    '3', 0x00,                  /* wcChar6 */
    '4', 0x00,                  /* wcChar7 */
    '5', 0x00,                  /* wcChar8 */
    '6', 0x00,                  /* wcChar9 */
#ifdef CONFIG_USB_HS
    ///////////////////////////////////////
    /// device qualifier descriptor
    ///////////////////////////////////////
    0x0a,
    USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x02,
    0x02,
    0x01,
    0x40,
    0x01,
    0x00,
#endif
    0x00
};

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t read_buffer[PRINTER_OUT_EP_SIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t write_buffer[PRINTER_IN_EP_SIZE];

void usbd_event_handler(uint8_t event)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            /* setup first out ep read transfer */
            usbd_ep_start_read(PRINTER_OUT_EP, read_buffer, PRINTER_OUT_EP_SIZE);
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}

void usbd_printer_bulk_out(uint8_t ep, uint32_t nbytes)
{
    USB_LOG_RAW("actual out len:%d\r\n", nbytes);
    // for (int i = 0; i < 100; i++) {
    //     printf("%02x ", read_buffer[i]);
    // }
    // printf("\r\n");
    /* setup next out ep read transfer */
    usbd_ep_start_read(PRINTER_OUT_EP, read_buffer, PRINTER_OUT_EP_SIZE);
}

void usbd_printer_bulk_in(uint8_t ep, uint32_t nbytes)
{
    USB_LOG_RAW("actual in len:%d\r\n", nbytes);

    if ((nbytes % PRINTER_IN_EP_SIZE) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(PRINTER_IN_EP, NULL, 0);
    } else {
    }
}

/*!< endpoint call back */
struct usbd_endpoint printer_out_ep = {
    .ep_addr = PRINTER_OUT_EP,
    .ep_cb = usbd_printer_bulk_out
};

struct usbd_endpoint printer_in_ep = {
    .ep_addr = PRINTER_IN_EP,
    .ep_cb = usbd_printer_bulk_in
};

struct usbd_interface intf0;

static const uint8_t printer_device_id[] =
{
  0x00, 51,
  'M','F','G',':','C','B','M',';',
  'C','M','D',':','G','D','I',';',
  'M','D','L',':','C','B','M','1','0','0','0',';',
  'C','L','S',':','P','R','I','N','T','E','R',';',
  'M','O','D','E',':','G','D','I',';'
};

void printer_init(void)
{
  usbd_desc_register(printer_descriptor);
  usbd_add_interface(usbd_printer_init_intf(&intf0, printer_device_id, sizeof(printer_device_id)));
  usbd_add_endpoint(&printer_out_ep);
  usbd_add_endpoint(&printer_in_ep);

  usbd_initialize();
}
