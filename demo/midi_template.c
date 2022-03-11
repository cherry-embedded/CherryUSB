#include "usbd_core.h"
#include "usb_midi.h"

USB_DESC_SECTION const uint8_t midi_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01),
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    // Standard AC Interface Descriptor
    0x09,
    0x04,
    0x00,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    // Class-specific AC Interface Descriptor
    0x09,
    0x24,
    0x01,
    0x00,
    0x01,
    0x09,
    0x00,
    0x01,
    0x01,
    // MIDIStreaming Interface Descriptors
    0x09,
    0x04,
    0x01,
    0x00,
    0x02,
    0x01,
    0x03,
    0x00,
    0x00,
    // Class-Specific MS Interface Header Descriptor
    0x07,
    0x24,
    0x01,
    0x00,
    0x01,
    WBVAL(65),

    // MIDI_IN_JACK_DESCRIPTOR_INIT(MIDI_JACK_TYPE_EMBEDDED, 0x01),
    // MIDI_IN_JACK_DESCRIPTOR_INIT(MIDI_JACK_TYPE_EXTERNAL, 0x02),
    // MIDI_OUT_JACK_DESCRIPTOR_INIT(MIDI_JACK_TYPE_EMBEDDED, 0x03, 0x02),
    // MIDI_OUT_JACK_DESCRIPTOR_INIT(MIDI_JACK_TYPE_EXTERNAL, 0x04, 0x01),
    MIDI_JACK_DESCRIPTOR_INIT(0x01),
    // OUT endpoint descriptor
    0x09, 0x05, 0x02, 0x02, WBVAL(64), 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x01,

    // IN endpoint descriptor
    0x09, 0x05, 0x81, 0x02, WBVAL(64), 0x00, 0x00, 0x00,
    0x05, 0x25, 0x01, 0x01, 0x03,

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
    0x28,                       /* bLength */
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
    'M', 0x00,                  /* wcChar10 */
    'I', 0x00,                  /* wcChar11 */
    'D', 0x00,                  /* wcChar12 */
    'I', 0x00,                  /* wcChar13 */
    ' ', 0x00,                  /* wcChar14 */
    'D', 0x00,                  /* wcChar15 */
    'E', 0x00,                  /* wcChar16 */
    'M', 0x00,                  /* wcChar17 */
    'O', 0x00,                  /* wcChar18 */
    ///////////////////////////////////////
    /// string3 descriptor
    ///////////////////////////////////////
    0x16,                       /* bLength */
    USB_DESCRIPTOR_TYPE_STRING, /* bDescriptorType */
    '2', 0x00,                  /* wcChar0 */
    '0', 0x00,                  /* wcChar1 */
    '2', 0x00,                  /* wcChar2 */
    '1', 0x00,                  /* wcChar3 */
    '0', 0x00,                  /* wcChar4 */
    '3', 0x00,                  /* wcChar5 */
    '1', 0x00,                  /* wcChar6 */
    '0', 0x00,                  /* wcChar7 */
    '0', 0x00,                  /* wcChar8 */
    '0', 0x00,                  /* wcChar9 */
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

void usbd_midi_bulk_out(uint8_t ep)
{
}

void usbd_midi_bulk_in(uint8_t ep)
{
}

usbd_class_t midi_class;
usbd_interface_t midi_cmd_intf;
usbd_interface_t midi_data_intf;

usbd_endpoint_t midi_out_ep = {
    .ep_addr = MIDI_OUT_EP,
    .ep_cb = usbd_midi_bulk_out
};

usbd_endpoint_t midi_in_ep = {
    .ep_addr = MIDI_IN_EP,
    .ep_cb = usbd_midi_bulk_in
};


void midi_init(void)
{
    usbd_desc_register(midi_descriptor);

    usbd_class_add_interface(&midi_class, &midi_cmd_intf);
    usbd_class_add_interface(&midi_class, &midi_data_intf);
    usbd_interface_add_endpoint(&midi_data_intf, &midi_out_ep);
    usbd_interface_add_endpoint(&midi_data_intf, &midi_in_ep);

    usbd_initialize();
}