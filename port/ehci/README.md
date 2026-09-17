# Note

## Support Chip List

### BouffaloLab

- BouffaloLab BL616/BL808 (fotg210 + EHCI)

### HPMicro

- HPM all series (chipidea + EHCI)

### AllwinnerTech

- Except F1Cxxx, F2Cxxx (musb + EHCI+ OHCI)

### Nuvoton

- Nuvoton all series

### Artinchip

- ALL series (aic + EHCI + OHCI)

### NXP

Modify USB_NOCACHE_RAM_SECTION

```
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".NonCacheable")))
```

- IMRT10XX/IMRT11XX (chipidea + EHCI)
- MCXN9XX/MCXN236 (chipidea + EHCI)

### Intel

- Intel 6 Series Chipset and Intel C200 Series Chipset
