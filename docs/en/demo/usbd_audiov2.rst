AudioV2 Device
=================

When using UAC2.0, please note the following points:

- On Windows, when modifying any parameter in the descriptor, the string descriptor must be modified synchronously and the driver must be uninstalled. Otherwise, Windows will think the device has not changed and continue to use the old driver, resulting in device recognition failure. Linux is not subject to this limitation.
- You can download RemoveGhostDev64.exe from the QQ group files to automatically delete all USB registered driver information, eliminating the need for the first step
- Windows 10 UAC2.0 functionality is incomplete, please use Windows 11 to test UAC2.0 functionality. Linux is not subject to this limitation
- Windows has calculation errors in the sampling rate range setting for multi-channel (more than 2 channels). For example, if you set 8K~96K, the actual range is greater than or equal to 8K and less than 96K, not less than or equal to 96K. Linux is not subject to this limitation
- Prohibit adding prints and time-consuming operations in interrupts, otherwise it will affect USB transmission according to interval