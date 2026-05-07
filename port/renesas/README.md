# Note

Refer to https://github.com/zephyrproject-rtos/hal_renesas

- Make sure usb interrupts exist in RASC.

![rasc](rasc_config.png)

- Remove all fsp usb source code. Otherwise you will build fail with multi function definitions.

![usb_code](usb_code.png)

## Support Chip List

- RA