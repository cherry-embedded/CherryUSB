# Note

## Support Chip List

- IMXRT105x
- IMXRT106x

## Before Use

- You should set the heap size on greater than the sum of all usb endpoint buffers.
- Open the clock on USBPHY1.
- Set macro CONFIG_USB_HS for use the high-speed mode otherwise the full-speed mode.