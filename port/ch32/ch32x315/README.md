# CH32X315 USBHS Device Port

This port adapts the CH32X315 USBHS device controller to CherryUSB. It is
device-only and supports High-Speed operation with a Full-Speed fallback.

## Controller properties

- Device register base: `0x40030000`
- Endpoint indices: EP0..EP7
- USB DMA fields contain offsets from the 64 KiB SRAM window at `0x20000000`
- DMA buffers must be 4-byte aligned and remain inside the SRAM window
- Non-zero endpoint maximum packet sizes must be multiples of 4 bytes so that
  multi-packet DMA transfers keep the buffer address aligned
- The controller provides a built-in High-Speed PHY

## Platform hooks

The DCD does not include a vendor SDK. The target platform should provide
strong definitions for these weak hooks:

```c
int usb_dc_ch32x315_low_level_init(uint8_t busid);
int usb_dc_ch32x315_low_level_deinit(uint8_t busid);
void usb_dc_ch32x315_bus_reset(void);
void usb_dc_ch32x315_irq_enable(uint8_t busid);
void usb_dc_ch32x315_irq_disable(uint8_t busid);
void usb_dc_ch32x315_delay_ms(uint32_t ms);
```

The low-level init hook is responsible for enabling the USBHS peripheral clock,
the PHY and any required clock calibration. The IRQ enable hook is responsible
for configuring the platform interrupt controller.

The platform interrupt vector should call:

```c
void USBHS_IRQHandler(void)
{
    usb_dc_ch32x315_irq_handler();
}
```

The application passes `USBHS_BASE` as the `reg_base` argument to
`usbd_initialize()`.

## Validation

The port was validated on a CH32X315 evaluation board using the official WCH
SDK as the platform layer. CherryUSB's CDC ACM speed-test pattern was used with
runtime logging disabled. Three 10 MiB runs measured 30.810-31.006 MB/s for
CDC OUT and 14.295-14.384 MB/s for CDC IN at High-Speed.
