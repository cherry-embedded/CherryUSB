# Note

## Support Chip List

- all of CH chips with usb HD ip are supported, like CH57x、CH58x、CH32Vxxx、CH32Fxxx

## Before Use

Your should implement `usb_dc_low_level_init` and `usb_dc_low_level_deinit`.
- Enable or disable USB clock and set USB clock for 48M.
- Enable or disable gpio and gpio clk for usb dp and dm.
- Enable or disable usb irq