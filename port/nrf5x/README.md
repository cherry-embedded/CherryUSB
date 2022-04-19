# Note

## Support Chip List

- NRF5x

## Before Use

- You should set the heap size on greater than the sum of all usb endpoint buffers.
- Your should implement `usb_dc_low_level_init` and `usb_dc_low_level_deinit`.