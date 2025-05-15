# Note

## Support Chip List

Modify USB_NOCACHE_RAM_SECTION

```
#define USB_NOCACHE_RAM_SECTION __attribute__((section(".NonCacheable")))
```

- IMRT10XX/IMRT11XX (chipidea + EHCI)
- MCXN9XX/MCXN236 (chipidea + EHCI)
