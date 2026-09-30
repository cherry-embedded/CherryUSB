# AIC8800 STA Host SDK

USB transport is `class/vendor/wifi/usbh_aic8800.c`. This folder is the
Wi-Fi layer: firmware download, FullMAC, WPA2-PSK and optional lwIP glue.

Validated on STM32H7R7 + CherryUSB DWC2 host HS + AIC8800D80 U02 USB
dongle. STA mode is WPA2-PSK/CCMP only. Open / WPA3 / AP mode are not
implemented.

## Files

| File | Role |
|---|---|
| `aic8800_wifi.c/.h` | Application API: SSID/password, events, scan table |
| `aic8800_fw.c/.h` | Firmware source: mapped package, image table, or weak `prepare` |
| `fw/` | D80 U02 vendor binaries + GPL-3.0 license |
| `aic8800_boot.c` | BootROM download for `a69c:8d80` |
| `aic8800_fmac.c/.h` | Scan, associate, EAPOL, data path |
| `aic8800_wpa2.c/.h` | PBKDF2 / HMAC-SHA1 / AES-unwrap |
| `aic8800_lwip.c/.h` | Optional netif + DHCP |
| `aic8800_wifi_config_template.h` | Copy to `aic8800_wifi_config.h` |
| `usbh_aic8800_wifi_template.c` | Minimal application hook |

Bluetooth, A2DP and iperf stay in the board BSP. Do not put real SSID or
password into the template.

## Firmware

`fw/` holds the five U02 images taken from
`ThirdParty/AIC8800D80/firmware` (Radxa `aic8800` commit
`df4c783b663eba1956579c681acd5e45f25c671d`). Those files are GPL-3.0;
CherryUSB code around them stays Apache-2.0. See `fw/README.md`.

Provide the images before `usbh_initialize()`:

1. `aic8800_fw_set_images()` with the five bins in `fw/`
2. `aic8800_fw_set_package()` with a packed `AICFWPKG`
3. Map an `AICFWPKG` and set `AIC8800_FW_PACKAGE_BASE` /
   `AIC8800_FW_PACKAGE_CAPACITY` (this board uses XSPI NOR `0x91F00000`)
4. Override weak `aic8800_fw_prepare()` and then call (1) or (2)

The loader checks the expected lengths: 1384 / 1708 / 32700 / 16136 /
358072. A mapped package is also CRC32-checked.

## Bring-up

1. Enable `CONFIG_CHERRYUSB_HOST_AIC8800` and host MSC.
2. In `usb_config.h` open the ZeroCD macros (see
   `docs/en/demo/usbh_wifi.rst`).
3. Copy `aic8800_wifi_config_template.h` to `aic8800_wifi_config.h` and
   set `AIC8800_WIFI_SSID` / `AIC8800_WIFI_PASSWORD`.
4. Register firmware with one of the methods above.
5. Place the FMAC TX queue in DMA-safe RAM via
   `AIC8800_FMAC_TX_QUEUE_SECTION`.
6. If lwIP is already running, set `AIC8800_LWIP_OWN_TCPIP` to `0`.

```c
aic8800_wifi_init();
aic8800_wifi_set_sta("your-ssid", "your-password");
aic8800_wifi_set_event_callback(on_wifi, NULL);
usbh_aic8800_enable_zero_cd_modeswitch();
usbh_initialize(0, USB_BASE, NULL);
/* GOT_IP: use aic8800_wifi_get_netif() or lwIP sockets */
```

## Radio API

`AIC8800_WIFI_AUTO_CONNECT` defaults to `1`: after `READY`, attach queues
`aic8800_wifi_connect()` (scan, then associate). Set it to `0` and call
these after `READY`:

- `aic8800_wifi_scan()`
- `aic8800_wifi_connect()` (scans again, then associates)
- `aic8800_wifi_disconnect()`

Events: `READY`, `SCAN_DONE`, `CONNECTED`, `GOT_IP`, `DISCONNECTED`,
`CONNECT_FAILED`. Callbacks may run in the FMAC worker or the lwIP
tcpip thread; keep them short.

Firmware load / FMAC worker threads use `usb_osal`. The data-path TX
thread is still FreeRTOS-specific.

## Expected USB sequence

`1111:1111` ZeroCD → `a69c:8d80` BootROM → `a69c:8d81` runtime. Runtime
Wi-Fi is usually interface 2 (`FF/FF/FF`). Interfaces 0/1 are Bluetooth
`e0/01/01` and are ignored when `usbh_bluetooth` is off.
