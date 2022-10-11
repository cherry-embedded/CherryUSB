#include "pico/stdlib.h"
#include "pico/time.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "cdc_acm_template.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device cdc acm example\n");
  extern void cdc_acm_init(void);
  cdc_acm_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
    extern void cdc_acm_data_send_with_dtr_test(void);
    cdc_acm_data_send_with_dtr_test();
  }
  return 0;
}
