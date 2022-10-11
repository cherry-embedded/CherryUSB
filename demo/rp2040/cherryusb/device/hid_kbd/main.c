#include "pico/stdlib.h"
#include "pico/time.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "hid_keyboard_template.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device hid keyboard example\n");
  extern void hid_keyboard_init(void);
  hid_keyboard_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
    extern void hid_keyboard_test(void);
    hid_keyboard_test();
    sleep_ms(1);
    uint8_t sendbuffer[8] = {0};
    usbd_ep_start_write(HID_INT_EP, sendbuffer, 8);
    sleep_ms(2000);
  }
  return 0;
}
