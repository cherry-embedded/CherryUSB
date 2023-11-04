# Note

## Support Chip List

- M032 Series
- M480 Series

## Before Use

Configure USBD_EPNUM.
Your should implement `usb_dc_low_level_init` and `usb_dc_low_level_deinit`.
- Enable or disable USB clock and set USB clock for 48M.
- Enable or disable gpio and gpio clk for usb dp and dm.
- Enable or disable usb irq
