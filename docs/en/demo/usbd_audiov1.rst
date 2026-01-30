AudioV1 Device
=================

UAC1 demo refers to `demo/audio_v1_*.c` template.

When using UAC1.0, pay attention to the following points:

- When using Windows, when modifying any descriptor parameters, you must synchronously modify the string descriptor and uninstall the driver, otherwise Windows will consider the device unchanged and continue to use the old driver, causing device recognition failure. Linux is not subject to this restriction.
- Download RemoveGhostDev64.exe from the QQ group files to automatically delete all USB registered driver information, eliminating the need for the first step
- Prohibit adding print statements and time-consuming operations in interrupts, otherwise it will affect USB transmission according to interval