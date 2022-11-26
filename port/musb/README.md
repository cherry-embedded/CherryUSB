# Note

## Support Chip List

### HEZHOU

- AIR105

### ES32

- ES32F3xx

### TI

- MSP432E4x

### AllwinnerTech

- F1Cxxx

## Before Use

Your should implement `usb_dc_low_level_init` and `usb_dc_low_level_deinit`.
- Enable or disable USB clock and set USB clock for 48M.
- Enable or disable gpio and gpio clk for usb dp and dm.
- Enable or disable usb irq