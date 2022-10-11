#include "pico/stdlib.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "cdc_acm_multi_template.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device cdc acm multitude example\n");
  extern void cdc_acm_multi_init(void);
  cdc_acm_multi_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
  }
  return 0;
}
