#include "usbd_core.h"
#include "usbd_hid.h"

#define HID_STATE_IDLE 0
#define HID_STATE_BUSY 1

/*!< keyboard in endpoint */
#define KBD_IN_EP          0x81 /*!< address */
#define KBD_IN_EP_SIZE     8    /*!< max packet length */
#define KBD_IN_EP_INTERVAL 10   /*!< polling time */

/*!< keyboard out endpoint */
#define KBD_OUT_EP          0x01 /*!< address */
#define KBD_OUT_EP_SIZE     1    /*!< max packet length */
#define KBD_OUT_EP_INTERVAL 10   /*!< polling time */

/*!< hidraw in endpoint */
#define HIDRAW_IN_EP       0x82
#define HIDRAW_IN_SIZE     64
#define HIDRAW_IN_INTERVAL 10

/*!< hidraw out endpoint */
#define HIDRAW_OUT_EP          0x02
#define HIDRAW_OUT_EP_SIZE     64
#define HIDRAW_OUT_EP_INTERVAL 10

#define USBD_VID           0xffff
#define USBD_PID           0xffff
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< hid state ! Data can be sent only when state is idle  */
uint8_t keyboard_state = HID_STATE_IDLE;
uint8_t custom_state = HID_STATE_IDLE;

/*!< config descriptor size */
#define USB_HID_CONFIG_DESC_SIZ 73

/*!< report descriptor size */
#define HID_KEYBOARD_REPORT_DESC_SIZE 63

/*!< custom hid report descriptor size */
#define HID_CUSTOM_REPORT_DESC_SIZE 34

/*!< global descriptor */
static const uint8_t hid_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0002, 0x01),
    USB_CONFIG_DESCRIPTOR_INIT(USB_HID_CONFIG_DESC_SIZ, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    /************** Descriptor of Keyboard interface ****************/
    /* 09 */
    0x09,                          /* bLength: Interface Descriptor size */
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType: Interface descriptor type */
    0x00,                          /* bInterfaceNumber: Number of Interface */
    0x00,                          /* bAlternateSetting: Alternate setting */
    0x02,                          /* bNumEndpoints */
    0x03,                          /* bInterfaceClass: HID */
    0x01,                          /* bInterfaceSubClass : 1=BOOT, 0=no boot */
    0x01,                          /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
    0,                             /* iInterface: Index of string descriptor */
    /******************** Descriptor of Keyboard HID ********************/
    /* 18 */
    0x09,                    /* bLength: HID Descriptor size */
    HID_DESCRIPTOR_TYPE_HID, /* bDescriptorType: HID */
    0x11,                    /* bcdHID: HID Class Spec release number */
    0x01,
    0x00,                          /* bCountryCode: Hardware target country */
    0x01,                          /* bNumDescriptors: Number of HID class descriptors to follow */
    0x22,                          /* bDescriptorType */
    HID_KEYBOARD_REPORT_DESC_SIZE, /* wItemLength: Total length of Report descriptor */
    0x00,
    /******************** Descriptor of Keyboard in endpoint ********************/
    /* 27 */
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */
    KBD_IN_EP,                    /* bEndpointAddress: Endpoint Address (IN) */
    0x03,                         /* bmAttributes: Interrupt endpoint */
    KBD_IN_EP_SIZE,               /* wMaxPacketSize: 4 Byte max */
    0x00,
    KBD_IN_EP_INTERVAL, /* bInterval: Polling Interval */
    /******************** Descriptor of Keyboard out endpoint ********************/
    /* 34 */
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */
    KBD_OUT_EP,                   /* bEndpointAddress: Endpoint Address (IN) */
    0x03,                         /* bmAttributes: Interrupt endpoint */
    KBD_OUT_EP_SIZE,              /* wMaxPacketSize: 4 Byte max */
    0x00,
    KBD_OUT_EP_INTERVAL, /* bInterval: Polling Interval */
    /************** Descriptor of Custom interface *****************/
    /* 41 */
    0x09,                          /* bLength: Interface Descriptor size */
    USB_DESCRIPTOR_TYPE_INTERFACE, /* bDescriptorType: Interface descriptor type */
    0x01,                          /* bInterfaceNumber: Number of Interface */
    0x00,                          /* bAlternateSetting: Alternate setting */
    0x02,                          /* bNumEndpoints */
    0x03,                          /* bInterfaceClass: HID */
    0x01,                          /* bInterfaceSubClass : 1=BOOT, 0=no boot */
    0x00,                          /* nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse */
    0,                             /* iInterface: Index of string descriptor */
    /******************** Descriptor of Custom HID ********************/
    /* 50 */
    0x09,                    /* bLength: HID Descriptor size */
    HID_DESCRIPTOR_TYPE_HID, /* bDescriptorType: HID */
    0x11,                    /* bcdHID: HID Class Spec release number */
    0x01,
    0x00,                        /* bCountryCode: Hardware target country */
    0x01,                        /* bNumDescriptors: Number of HID class descriptors to follow */
    0x22,                        /* bDescriptorType */
    HID_CUSTOM_REPORT_DESC_SIZE, /* wItemLength: Total length of Report descriptor */
    0x00,
    /******************** Descriptor of Custom in endpoint ********************/
    /* 59 */
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */
    HIDRAW_IN_EP,                 /* bEndpointAddress: Endpoint Address (IN) */
    0x03,                         /* bmAttributes: Interrupt endpoint */
    HIDRAW_IN_SIZE,               /* wMaxPacketSize: 4 Byte max */
    0x00,
    HIDRAW_IN_INTERVAL, /* bInterval: Polling Interval */
    /******************** Descriptor of Custom out endpoint ********************/
    /* 66 */
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_DESCRIPTOR_TYPE_ENDPOINT, /* bDescriptorType: */
    HIDRAW_OUT_EP,                /* bEndpointAddress: Endpoint Address (IN) */
    0x03,                         /* bmAttributes: Interrupt endpoint */
    HIDRAW_OUT_EP_SIZE,           /* wMaxPacketSize: 4 Byte max */
    0x00,
    HIDRAW_OUT_EP_INTERVAL, /* bInterval: Polling Interval */
    /* 73 */
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
    'H', 0x00,                  /* wcChar10 */
    'I', 0x00,                  /* wcChar11 */
    'D', 0x00,                  /* wcChar12 */
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
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
#endif
    0x00
};

/*!< hid keyboard report descriptor */
static const uint8_t hid_keyboard_report_desc[HID_KEYBOARD_REPORT_DESC_SIZE] = {
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x06, // USAGE (Keyboard)
    0xa1, 0x01, // COLLECTION (Application)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x19, 0xe0, // USAGE_MINIMUM (Keyboard LeftControl)
    0x29, 0xe7, // USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x08, // REPORT_COUNT (8)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x08, // REPORT_SIZE (8)
    0x81, 0x03, // INPUT (Cnst,Var,Abs)
    0x95, 0x05, // REPORT_COUNT (5)
    0x75, 0x01, // REPORT_SIZE (1)
    0x05, 0x08, // USAGE_PAGE (LEDs)
    0x19, 0x01, // USAGE_MINIMUM (Num Lock)
    0x29, 0x05, // USAGE_MAXIMUM (Kana)
    0x91, 0x02, // OUTPUT (Data,Var,Abs)
    0x95, 0x01, // REPORT_COUNT (1)
    0x75, 0x03, // REPORT_SIZE (3)
    0x91, 0x03, // OUTPUT (Cnst,Var,Abs)
    0x95, 0x06, // REPORT_COUNT (6)
    0x75, 0x08, // REPORT_SIZE (8)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0xFF, // LOGICAL_MAXIMUM (255)
    0x05, 0x07, // USAGE_PAGE (Keyboard)
    0x19, 0x00, // USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65, // USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00, // INPUT (Data,Ary,Abs)
    0xc0        // END_COLLECTION
};

/*!< custom hid report descriptor */
static const uint8_t hid_custom_report_desc[HID_CUSTOM_REPORT_DESC_SIZE] = {
    /* USER CODE BEGIN 0 */
    0x06, 0x00, 0xff, // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,       // USAGE (Vendor Usage 1)
    0xa1, 0x01,       // COLLECTION (Application)
    0x09, 0x01,       //   USAGE (Vendor Usage 1)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM (255)
    0x95, 0x40,       //   REPORT_COUNT (64)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x81, 0x02,       //   INPUT (Data,Var,Abs)
    /* <___________________________________________________> */
    0x09, 0x01,       //   USAGE (Vendor Usage 1)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM (255)
    0x95, 0x40,       //   REPORT_COUNT (64)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x91, 0x02,       //   OUTPUT (Data,Var,Abs)
    /* USER CODE END 0 */
    0xC0 /*     END_COLLECTION	             */
};

/*!< class */
static usbd_class_t hid_class;

/*!< interface one */
static usbd_interface_t hid_intf_1;

/*!< interface two */
static usbd_interface_t hid_intf_2;

/* function ------------------------------------------------------------------*/
static void usbd_hid_kbd_in_callback(uint8_t ep)
{
    /*!< endpoint call back */
    /*!< transfer successfully */
    if (keyboard_state == HID_STATE_BUSY) {
        /*!< update the state  */
        keyboard_state = HID_STATE_IDLE;
    }
}

static void usbd_hid_kbd_out_callback(uint8_t ep)
{
    /*!< here you can write the LED processing from the host */
    enum hid_kbd_led led_state;
    /*!< read the led data from host send */
    usbd_ep_read(ep, &led_state, KBD_OUT_EP_SIZE, NULL);
    /*!< diy */
    if (led_state & HID_KBD_LED_NUM_LOCK) {
        /*!< num lock */
        /*!< do what you like */
    } else {
    }
    if (led_state & HID_KBD_LED_CAPS_LOCK) {
        /*!< caps lock */
    } else {
    }

    if (led_state & HID_KBD_LED_SCROLL_LOCK) {
        /*!< scroll lock */
        /*!< do what you like */
    } else {
    }
    if (led_state & HID_KBD_LED_COMPOSE) {
        /*!< compose led */
        /*!< do what you like */
    } else {
    }
    if (led_state & HID_KBD_LED_KANA) {
        /*!< kana led */
        /*!< do what you like */
    } else {
    }
}

static void usbd_hid_custom_in_callback(uint8_t ep)
{
    /*!< endpoint call back */
    /*!< transfer successfully */
    if (custom_state == HID_STATE_BUSY) {
        /*!< update the state  */
        custom_state = HID_STATE_IDLE;
    }
}

static void usbd_hid_custom_out_callback(uint8_t ep)
{
    /*!< read the data from host send */
    uint8_t custom_data[HIDRAW_OUT_EP_SIZE];
    usbd_ep_read(HIDRAW_OUT_EP, custom_data, HIDRAW_OUT_EP_SIZE, NULL);

    /*!< you can use the data do some thing you like */
}

/*!< endpoint call back */
static usbd_endpoint_t keyboard_in_ep = {
    .ep_cb = usbd_hid_kbd_in_callback,
    .ep_addr = 0x81
};

static usbd_endpoint_t keyboard_out_ep = {
    .ep_cb = usbd_hid_kbd_out_callback,
    .ep_addr = 0x01
};

static usbd_endpoint_t custom_in_ep = {
    .ep_cb = usbd_hid_custom_in_callback,
    .ep_addr = 0x82
};

static usbd_endpoint_t custom_out_ep = {
    .ep_cb = usbd_hid_custom_out_callback,
    .ep_addr = 0x02
};

/* function ------------------------------------------------------------------*/
/**
  * @brief            hid custom init
  * @pre              none
  * @param[in]        none
  * @retval           none
  */
void hid_custom_keyboard_init(void)
{
    usbd_desc_register(hid_descriptor);
    /*!< add interface ! the first interface */
    usbd_hid_add_interface(&hid_class, &hid_intf_1);
    /*!< interface0 add endpoint ! the first endpoint */
    usbd_interface_add_endpoint(&hid_intf_1, &keyboard_in_ep);
    /*!< interface0 add endpoint ! the second endpoint */
    usbd_interface_add_endpoint(&hid_intf_1, &keyboard_out_ep);
    /*!< add interface the ! second interface */
    usbd_hid_add_interface(&hid_class, &hid_intf_2);
    /*!< interface1 add endpoint ! the first endpoint */
    usbd_interface_add_endpoint(&hid_intf_2, &custom_in_ep);
    /*!< interface1 add endpoint ! the second endpoint */
    usbd_interface_add_endpoint(&hid_intf_2, &custom_out_ep);
    /*!< register report descriptor interface 0 */
    usbd_hid_report_descriptor_register(0, hid_keyboard_report_desc, HID_KEYBOARD_REPORT_DESC_SIZE);
    /*!< register report descriptor interface 1 */
    usbd_hid_report_descriptor_register(1, hid_custom_report_desc, HID_CUSTOM_REPORT_DESC_SIZE);

    usbd_initialize();
}

/**
  * @brief            device send report to host
  * @pre              none
  * @param[in]        ep endpoint address
  * @param[in]        data points to the data buffer waiting to be sent
  * @param[in]        len length of data to be sent
  * @retval           none
  */
void hid_keyboard_send_report(uint8_t ep, uint8_t *data, uint8_t len)
{
    if (usb_device_is_configured()) {
        if (keyboard_state == HID_STATE_IDLE) {
            /*!< updata the state */
            keyboard_state = HID_STATE_BUSY;
            /*!< write buffer */
            usbd_ep_write(ep, data, len, NULL);
        }
    }
}

/**
  * @brief            device send report to host
  * @pre              none
  * @param[in]        ep endpoint address
  * @param[in]        data points to the data buffer waiting to be sent
  * @param[in]        len length of data to be sent
  * @retval           none
  */
void hid_custom_send_report(uint8_t ep, uint8_t *data, uint8_t len)
{
    if (usb_device_is_configured()) {
        if (custom_state == HID_STATE_IDLE) {
            /*!< updata the state */
            custom_state = HID_STATE_BUSY;
            /*!< write buffer */
            usbd_ep_write(ep, data, len, NULL);
        }
    }
}

/**
  * @brief            hid custom test
  * @pre              none
  * @param[in]        none
  * @retval           none
  */
void hid_custom_test(void)
{
    /*!< keyboard test */
    uint8_t sendbuffer1[8] = { 0x00, 0x00, HID_KEY_A, 0x00, 0x00, 0x00, 0x00, 0x00 }; //A
    hid_keyboard_send_report(KBD_IN_EP, sendbuffer1, KBD_IN_EP_SIZE);
    /*!< delay 10ms */
    //HAL_Delay(10);
    /*!< send button up */
    sendbuffer1[2] = 0;
    hid_keyboard_send_report(KBD_IN_EP, sendbuffer1, KBD_IN_EP_SIZE);
    /*!< delay 1000ms the custom test */
    //HAL_Delay(1000);
    /*!< custom test */
    uint8_t sendbuffer2[64] = { 6 };
    hid_custom_send_report(HIDRAW_IN_EP, sendbuffer2, HIDRAW_IN_SIZE);
    //HAL_Delay(1000);
}