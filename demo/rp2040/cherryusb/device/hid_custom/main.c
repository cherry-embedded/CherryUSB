#include "pico/stdlib.h"
#include "pico/time.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "hid_custom_inout_template.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device hid custom example\n");
  extern void hid_custom_keyboard_init(void);
  hid_custom_keyboard_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
    extern void hid_custom_test(void);
    hid_custom_test();
  }
  return 0;
}
