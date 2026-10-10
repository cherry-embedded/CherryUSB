# AIC8800D80 U02 firmware

These five binaries are the D80 U02 USB set copied from the board tree
`ThirdParty/AIC8800D80/firmware`. They are **not** Apache-2.0.

- Source: https://github.com/radxa-pkg/aic8800
- Commit: `df4c783b663eba1956579c681acd5e45f25c671d`
- Upstream path: `src/USB/driver_fw/fw/aic8800D80`
- License: GPL-3.0, see `LICENSE.GPL-3.0`

Use this set for revision `0x07` non-H devices (BootROM chip register
`0xf3078820` on the validated AIC8800D80 stick).

| File | Bytes | Load address |
|---|---:|---|
| `fw_patch_table_8800d80_u02.bin` | 1384 | parsed by the BootROM loader |
| `fw_adid_8800d80_u02.bin` | 1708 | `0x00201940` |
| `fw_patch_8800d80_u02.bin` | 32700 | `0x001E0000` |
| `fw_patch_8800d80_u02_ext0.bin` | 16136 | `0x0020B43C` |
| `fmacfw_8800d80_u02.bin` | 358072 | `0x00120000` |

Register them before `usbh_initialize()`:

```c
const struct aic8800_fw_blob images[AIC8800_FW_IMAGE_COUNT] = {
    { fw_patch_table_8800d80_u02, 1384U },
    { fw_adid_8800d80_u02,        1708U },
    { fw_patch_8800d80_u02,      32700U },
    { fw_patch_8800d80_u02_ext0, 16136U },
    { fmacfw_8800d80_u02,       358072U },
};

aic8800_fw_set_images(images);
```

Or pack them into an `AICFWPKG` and call `aic8800_fw_set_package()`, or map
that package and set `AIC8800_FW_PACKAGE_BASE`. Images must stay readable
until BootROM download finishes.
