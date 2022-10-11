#include "pico/stdlib.h"
#include "usbd_core.h"
#include <stdio.h>
#include <string.h>

/**
 *
 */
#include "dfu_with_st_tool_template.c"

uint8_t *dfu_read_flash(uint8_t *src, uint8_t *dest, uint32_t len) {
  uint32_t i = 0;
  uint8_t *psrc = src;

  for (i = 0; i < len; i++) {
    dest[i] = *psrc++;
  }
  /* Return a valid address to avoid HardFault */
  return (uint8_t *)(dest);
}

uint16_t dfu_write_flash(uint8_t *src, uint8_t *dest, uint32_t len) {
  printf("dfu write flash -- src_add:%08x -- dest_add:%08x -- len:%d\r\n", src,
         dest, len);

  return 0;
}

uint16_t dfu_erase_flash(uint32_t add) {
  /**
   * The length of erasure needs to be consistent with USBD_DFU_XFER_SIZE equal
   * If you want to modify USBD_DFU_XFER_SIZE, please pay attention to the
   * CONFIG_USBDEV_REQUEST_BUFFER_LEN in the usb_config.h file
   * CONFIG_USBDEV_REQUEST_BUFFER_LEN need >= USBD_DFU_XFER_SIZE
   */
  printf("dfu erase flash start add:%08x\r\n", add);
  return 0;
}

void dfu_leave(void) { printf("dfu leave\r\n"); }

int main(void) {
  stdio_init_all();
  printf("CherryUSB device dfu example\n");
  extern void dfu_flash_init(void);
  dfu_flash_init();

  // Wait until configured
  while (!usb_device_is_configured()) {
    tight_loop_contents();
  }

  // Everything is interrupt driven so just loop here
  while (1) {
  }
  return 0;
}
