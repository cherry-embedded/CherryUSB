Version Information
============================================
If there are no special circumstances, please use the latest version. The following lists only important updates. For detailed update information, please refer to https://github.com/cherry-embedded/CherryUSB/releases.

<= v0.10.2 Initial Version
---------------------------

- **Used to establish basic host/device framework, supporting only single USB IP**.
- **Host driver where each EP occupies one hardware pipe, does not support dynamic hardware pipe usage**.
- Related porting requires this version, no longer supported in subsequent versions (such as ch32, rp2040), as well as old versions of pusb2 and xhci (new versions no longer provide source code).

v1.0.0 Transition Version
---------------------------

- **Host supports dynamic hardware pipe usage, no longer fixed**

v1.1.0 Transition Version
---------------------------

- **Host and device support multiple USB IPs of the same type**
- **Host adds bluetooth, ch340, ftdi, cp210x, asix drivers**
- Device MSC supports multiple LUN, and CONFIG_USBDEV_MSC_BLOCK_SIZE changed to CONFIG_USBDEV_MSC_MAX_BUFSIZE

v1.2.0
---------------------------

- **Host adds rtl8152, cdc ncm drivers**
- Host adds timer to control interrupt transfers (hub modified to use timer control)
- Porting adds esp, aic host drivers
- **Optimize DWC2 code for easier reading, and add some FIFO configuration macros for users (because dwc2 fifo size is limited and there are many configuration methods, so exported to users for reasonable performance control)**
- Optimize ehci driver (qtd no longer uses dynamic allocation, bound to qh), for faster code execution

v1.3.0
---------------------------

- **Device supports automatic selection of multiple speed descriptors (enable CONFIG_USBDEV_ADVANCE_DESC)**
- Device core code unifies ep0 buffer usage for code beautification
- Host adds pl2303 driver; uses id table to support multiple VID/PID; adds user_data for user use
- Host network class driver adds TX/RX buffer macros, adds LWIP_TCPIP_CORE_LOCKING_INPUT usage for data zero-copy implementation
- Porting imports bouffalo, aic, stm32f723 device drivers
- **Fixed issue in porting host section where urb->timeout clearing had problems (no pipe alloc exception during large data transfers, mainly because transfer completed just after starting, timeout was modified to 0 before checking, didn't enter take sem flow), fixed in this version**
- EHCI enables IAAD in usbh_kill_urb, reads EHCI HCOR offset from HCCR caplength, enables OHCI for EHCI
- Adapted for NuttX OS

v1.3.1
---------------------------

- Bugfix (audio, video, CDC ECM related macros, structures, APIs)
- **Host hub enumeration thread removed, uses PSC thread, enumeration method changed to queue mode, canceled simultaneous enumeration of multiple devices functionality**
- Host scan driver information and instance uses recursive mode, removes linked list scanning
- Host network class driver optimization, supports receiving data above 16K (CDC ECM not supported), uses advanced memcpy API
- **Device protocol stack printing removed (no more printing in interrupts)**
- Porting MUSB FIFO configuration changed to obtain from FIFO table (this code references Linux), adapted for ES32, SUNXI, BEKEN

v1.4.0
---------------------------

- **Device starts supporting remote wakeup functionality, HID request(0x21), improved GET STATUS request (starting from this version can pass USB3CV testing)**
- Device adds UF2, ADB, WEBUSB functionality; MSC adds bare-metal read/write polling functionality, puts read/write in while1 execution; usbd_cdc renamed to usbd_cdc_acm
- Host adds USB WiFi(bl616), Xbox drivers; **restructured USB3.0 enumeration logic**
- **Host CDC_ACM, HID, MSC, serial transmission shared buffer, would have issues if multiple identical devices exist, changed to separate buffers**
- **Porting restructured XHCI/PUSB2 drivers, not open source**; EHCI and OHCI files renamed; added remote wakeup API
- ESP component library support
- **ChipIdea device driver support, NXP MCX series host/device support**
- ThreadX OS support

v1.4.1
---------------------------

- **Fixed device mode issue with repeated endpoint closure when using multiple altsettings, changed to close when altsetting is 0**
- **Restructured host audio descriptor parsing**
- **Added Kinetis USB IP**
- Host usbh_msc_get_maxlun request not supported by some USB drives, no error return
- Host usbh_hid_get_report_descriptor exported for user calls
- Static code checking
- GitHub action functionality

v1.4.2
---------------------------

- Device implements USB_REQUEST_GET_INTERFACE request
- **Device video transmission restructured, added dual buffering functionality**
- Device ECM restructured, maintains similar API to RNDIS
- Device and host audio volume configuration functionality restructured
- Host adds AOA driver
- C++ compatibility related modifications
- FSDEV doesn't support ISO and DWC2 high-speed hub doesn't support full-speed/low-speed checking
- **General OHCI code updates**

v1.4.3
---------------------------

- **Device EP0 processing adds thread mode**
- Device audio feedback macros and demo
- Device RNDIS adds passthrough functionality (without LWIP)
- **Host MSC moves SCSI initialization out of enumeration thread, calls it during mount phase, and adds testunit retries multiple times to be compatible with some USB drives**
- RP2040 host/device support
- **NuttX FS, serial, net component support**
- DWC2, EHCI, OHCI host DCache functionality support (improved in v1.5.0)
- T113, MCXA156, CH585, **STM32H7R support**
- Fixed issue in v1.4.1 where altsetting 0 should close all endpoints

v1.5.0
---------------------------

- **Protocol stack internal global buffer needs to use USB_ALIGN_UP alignment, for use when DCache is enabled and nocache is not enabled**
- **Improved EHCI/OHCI DCache mode handling**, add CONFIG_USB_EHCI_DESC_DCACHE_ENABLE for qh&qtd&itd, add CONFIG_USB_OHCI_DESC_DCACHE_ENABLE for ed&td
- **Platform code updates, platform-related moved to platform, added LVGL keyboard/mouse support, blackmagic support, FileX support, Zephyr disk support, ESP-IDF netif support**
- **Device SOF callback support**
- **DWC2, FSDEV ST implements low-level API and interrupts, directly calls HAL_PCD_MSP and HAL_HCD_MSP, no need for user copy-paste**
- **DWC2 implements SPLIT functionality, supports external high-speed hub interfacing with FS/LS devices in high-speed mode**
- LiteOS-M, Zephyr OS support
- Device MSC bare-metal read/write uses variable mode instead of ringbuffer
- EHCI QTD uses qtd alloc & free, saves memory, currently QH carries QTD
- RNDIS/ECM device, MSC demo updates, supports modification-free under RT-Thread
- **All memcpy replaced with usb_memcpy, ARM library has non-aligned access issues**
- **Restructured device MTP driver (commercial use)**
- **Device TMC driver (commercial use)**
- **Restructured device video transmission, directly fills UVC header in image data, achieving zero memcpy**
- **Added usb_osal_thread_schedule_other API for releasing all class threads before releasing class resources, avoiding threads still using class resources after class resources are released**
- **DWC2 device adds DCache functionality, usable for Cortex-M7/ESP32P4**
- **Bouffalo/HPM/ESP/ST/NXP DCache API support**
- CH32 device ISO updates, IP directory reclassification
- CMake, SCons, Kconfig updates
- Use USB_ASSERT_MSG for partial code checking, comprehensive warning fixes
- N32H4/MM32F5 device support
- Default enable CONFIG_USBDEV_ADVANCE_DESC

v1.5.1
---------------------------

- Support using ADB shell under RT-Thread, host serial/device CDC_ACM interfaces with RTDevice framework
- **DWC2 adds multiple USB port configuration functionality with different parameters, e.g., one full-speed and one high-speed, with different FIFO and PHY configurations**
- **EHCI control transfer memory leak when no data phase causes data QTD not to be released**
- **DWC2 reads setup using usbd_get_next_ep0_state to judge, avoiding conflict between setup and EP0 out usage under USB_OTG_DOEPINT_XFRC state**
- SIFLI USB device preliminary support

v1.5.2
---------------------------

- Some bugfixes for RT-Thread components under 1.5.1
- IDF timer OSAL replaced with ESP timer, FreeRTOS timer may fail to start; xTaskCreate replaced with xTaskCreatePinnedToCore for multi-core convenience
- In host enumeration, removed descriptor overflow related ASSERT operations, changed to return error. String descriptor acquisition changed to only get if supported. 2ms delay changed to 10ms because some OS use 100Hz, causing delay invalidation
- **DWC2 EP mult support, split transfer code optimization, modified split-related cache handling**
- **DWC2 halt cannot clear USB_OTG_HCCHAR_EPDIR, reset port uses timeout mechanism to prevent deadlock due to disconnection during enumeration**
- Updated DWC2 AT32, STM32, Kendryte, Espressif glue code
- MUSB for standard IP structure uses independent EP control register groups, not using EPIDX register for control
- Removed all CONFIG_USBDEV_EP_NUM & CONFIG_USBHOST_PIPE_NUM, no longer used because IP itself carries this information or manufacturer SDK provides corresponding macros
- CONFIG_USBHOST_MAX_INTF_ALTSETTINGS defaults to 2 to reduce memory, only UVC and UAC use it (commercial charging), so no need to set it large
- URB interval changed from u8 to u32, maximum support 2^15 * 125us

v1.5.3
---------------------------

- Added mongoose demo
- **Device supports custom EP0 MPS, only supports commercial IPs**
- Host adds UVC bulk support, **interface number matching driver functionality**, **host address allocation changed to cyclic increment mode**, restructured lsusb command
- Host control transfer adds retry mechanism, some devices have unstable communication, retry count references Linux
- **Host RNDIS driver adds non-standard 02/02/ff interface driver matching**
- MUSB IP disables multipoint feature support
- HPMicro, ChipIdea DCache support
- IDF host MSC support
- OTG framework restructured, current port only supports HPMicro
- CI compilation functionality, supports HPMicro/Espressif/BouffaloLab

v1.5.3.99
---------------------------

Bugfix for v1.5.3

v1.6.0
---------------------------

- **Host adds serial framework, unifies all serial-like drivers**
- **Host HID adds report descriptor parsing functionality**
- usbh_initialize adds event callback to notify users of host event changes, usually not needed, can be set to NULL
- Support gamepad device
- Added TI XMC, Infineon Edge E8x port support
- DWC2 adds usbd_dwc2_get_system_clock to replace SystemCoreClock; removes __UNALIGNED_UINT32_READ and __UNALIGNED_UINT32_WRITE macros; setup count set to 1; first setup read moved to USB_OTG_GINTSTS_ENUMDNE interrupt
- DWC2/EHCI adds root hub speed setting