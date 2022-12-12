Porting 如何编写
==============================

本节主要介绍没有支持的芯片如何做 porting。

从机 porting
----------------------------

- 首先复制一份从 `port/template` 复制一份 `usb_dc.c` 并参与编译，保证能编译过。
- 实现 ``usb_dc_init`` 保证能进入中断
- 在中断中判断 reset 中断并能够正常触发 reset 中断，在 reset 中断中调用 ``usbd_event_reset_handler``，如果可能，还需要启动读取 setup 包。
- 在中断中判断 setup 中断并能够正常触发 setup 中断，然后将读取的数据传入 ``usbd_event_ep0_setup_complete_handler``
- 实现 ``usbd_ep_start_write`` 并能够触发发送完成中断
- 实现 ``usbd_ep_start_read`` 并能够触发接收完成中断
- 分包处理

主机 porting
----------------------------

- 首先复制一份从 `port/template` 复制一份 `usb_hc.c` 并参与编译，保证能编译过。
- 实现 ``usb_hc_init`` 保证能进入中断
- 能够进入插拔中断，比如 ``connect`` 和 ``disconnect``，并调用 ``usbh_roothub_thread_wakeup`` 能够唤醒 hub 线程
- 实现 ``usbh_roothub_control`` 并根据第三个条件，能够完成 ``usbh_hub_events`` 中 ``usbh_enumerate`` 之前的流程
- 实现 ``usbh_submit_urb``