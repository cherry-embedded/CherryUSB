#include "pico/stdlib.h"
#include "pico/time.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "hid_mouse_template.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device hid mouse example\n");
  extern void hid_mouse_init(void);
  hid_mouse_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
    extern void hid_mouse_test(void);
    hid_mouse_test();
    sleep_ms(1);
    uint8_t zero_data[4] = {0};
    usbd_ep_start_write(HID_INT_EP, zero_data, 4);
    sleep_ms(2000);
  }
  return 0;
}
