#include "pico/stdlib.h"
#include "pico/time.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

uint32_t chey_board_millis(void) {
  return to_ms_since_boot(get_absolute_time());
}

/**
 *
 */
#include "cherryusb_uvc_demo.c"

int main(void) {
  stdio_init_all();
  printf("CherryUSB device uvc example\n");
  extern void video_init(void);
  video_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
    extern void video_task(void);
    video_task();
  }
  return 0;
}
