#include "pico/stdlib.h"
#include "pico/time.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "cdc_acm_hid_msc_template.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device hid cdc msc composite example\n");
  extern void cdc_acm_hid_msc_descriptor_init(void);
  cdc_acm_hid_msc_descriptor_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
  }
  return 0;
}
