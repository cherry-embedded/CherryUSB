#include "usbd_core.h"
#include "usbd_cdc.h"

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x01
#define CDC_INT_EP 0x85

#define CDC_IN_EP2  0x82
#define CDC_OUT_EP2 0x02
#define CDC_INT_EP2 0x86

#define CDC_IN_EP3  0x83
#define CDC_OUT_EP3 0x03
#define CDC_INT_EP3 0x87

#define CDC_IN_EP4  0x84
#define CDC_OUT_EP4 0x04
#define CDC_INT_EP4 0x88

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN * 4)

/*!< global descriptor */
static const uint8_t cdc_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, USBD_VID, USBD_PID, 0x0100, 0x01),
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x08, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP, 0x02),
    CDC_ACM_DESCRIPTOR_INIT(0x02, CDC_INT_EP2, CDC_OUT_EP2, CDC_IN_EP2, 0x02),
    CDC_ACM_DESCRIPTOR_INIT(0x04, CDC_INT_EP3, CDC_OUT_EP3, CDC_IN_EP3, 0x02),
    CDC_ACM_DESCRIPTOR_INIT(0x06, CDC_INT_EP4, CDC_OUT_EP4, CDC_IN_EP4, 0x02),
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
    0x26,                       /* bLength */
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
    'C', 0x00,                  /* wcChar10 */
    'D', 0x00,                  /* wcChar11 */
    'C', 0x00,                  /* wcChar12 */
    ' ', 0x00,                  /* wcChar13 */
    'D', 0x00,                  /* wcChar14 */
    'E', 0x00,                  /* wcChar15 */
    'M', 0x00,                  /* wcChar16 */
    'O', 0x00,                  /* wcChar17 */
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

/*!< class */
usbd_class_t cdc_class;
/*!< interface one */
usbd_interface_t cdc_cmd_intf;
/*!< interface two */
usbd_interface_t cdc_data_intf;

/* function ------------------------------------------------------------------*/
void usbd_cdc_acm_bulk_out(uint8_t ep)
{
    uint8_t data[64];
    uint32_t read_byte;

    usbd_ep_read(ep, data, 64, &read_byte);
    for (uint32_t i = 0; i < read_byte; i++) {
        USB_LOG_RAW("%02x ", data[i]);
    }
    USB_LOG_RAW("\r\n");
    USB_LOG_RAW("read len:%d\r\n", read_byte);
    usbd_ep_read(ep, NULL, 0, NULL);
}

void usbd_cdc_acm_bulk_in(uint8_t ep)
{
    USB_LOG_RAW("in\r\n");
}

/*!< endpoint call back */
usbd_class_t cdc_class1;
usbd_interface_t cdc_cmd_intf1;
usbd_interface_t cdc_data_intf1;

usbd_endpoint_t cdc_out_ep1 = {
    .ep_addr = CDC_OUT_EP,
    .ep_cb = usbd_cdc_acm_bulk_out
};

usbd_endpoint_t cdc_in_ep1 = {
    .ep_addr = CDC_IN_EP,
    .ep_cb = usbd_cdc_acm_bulk_in
};

usbd_class_t cdc_class2;
usbd_interface_t cdc_cmd_intf2;
usbd_interface_t cdc_data_intf2;

usbd_endpoint_t cdc_out_ep2 = {
    .ep_addr = CDC_OUT_EP2,
    .ep_cb = usbd_cdc_acm_bulk_out
};

usbd_endpoint_t cdc_in_ep2 = {
    .ep_addr = CDC_IN_EP2,
    .ep_cb = usbd_cdc_acm_bulk_in
};

usbd_class_t cdc_class3;
usbd_interface_t cdc_cmd_intf3;
usbd_interface_t cdc_data_intf3;

usbd_endpoint_t cdc_out_ep3 = {
    .ep_addr = CDC_OUT_EP3,
    .ep_cb = usbd_cdc_acm_bulk_out
};

usbd_endpoint_t cdc_in_ep3 = {
    .ep_addr = CDC_IN_EP3,
    .ep_cb = usbd_cdc_acm_bulk_in
};

usbd_class_t cdc_class4;
usbd_interface_t cdc_cmd_intf4;
usbd_interface_t cdc_data_intf4;

usbd_endpoint_t cdc_out_ep4 = {
    .ep_addr = CDC_OUT_EP4,
    .ep_cb = usbd_cdc_acm_bulk_out
};

usbd_endpoint_t cdc_in_ep4 = {
    .ep_addr = CDC_IN_EP4,
    .ep_cb = usbd_cdc_acm_bulk_in
};

/* function ------------------------------------------------------------------*/
void cdc_acm_multi_init(void)
{
    usbd_desc_register(cdc_descriptor);

    usbd_cdc_add_acm_interface(&cdc_class1, &cdc_cmd_intf1);
    usbd_cdc_add_acm_interface(&cdc_class1, &cdc_data_intf1);
    usbd_interface_add_endpoint(&cdc_data_intf1, &cdc_out_ep1);
    usbd_interface_add_endpoint(&cdc_data_intf1, &cdc_in_ep1);

    usbd_cdc_add_acm_interface(&cdc_class2, &cdc_cmd_intf2);
    usbd_cdc_add_acm_interface(&cdc_class2, &cdc_data_intf2);
    usbd_interface_add_endpoint(&cdc_data_intf2, &cdc_out_ep2);
    usbd_interface_add_endpoint(&cdc_data_intf2, &cdc_in_ep2);

    usbd_cdc_add_acm_interface(&cdc_class3, &cdc_cmd_intf3);
    usbd_cdc_add_acm_interface(&cdc_class3, &cdc_data_intf3);
    usbd_interface_add_endpoint(&cdc_data_intf3, &cdc_out_ep3);
    usbd_interface_add_endpoint(&cdc_data_intf3, &cdc_in_ep3);

    usbd_cdc_add_acm_interface(&cdc_class4, &cdc_cmd_intf4);
    usbd_cdc_add_acm_interface(&cdc_class4, &cdc_data_intf4);
    usbd_interface_add_endpoint(&cdc_data_intf4, &cdc_out_ep4);
    usbd_interface_add_endpoint(&cdc_data_intf4, &cdc_in_ep4);

    usbd_initialize();
}