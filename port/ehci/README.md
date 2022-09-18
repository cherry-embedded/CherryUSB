# Note

## Support Chip List

### BouffaloLab

- BouffaloLab BL616/BL808

### HPMicro

- HPM all series

### AllwinnerTech

- F133

### Nuvoton

- Nuvoton all series

## Before Use

Your should implement `usb_hc_low_level_init`.
- Enable USB PHY、USB clock and set USB clock for 48M.
- Enable usb irq
- Config EHCI BASE and other macros in `cherryusb_config_tempate.h`