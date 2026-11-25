# CherryUSB 工作约定

## 查阅资料

使用 CherryUSB 前，先查阅与任务相关的文档、示例和当前版本 API。以下路径相对于 CherryUSB 源码根目录，英文文档位于对应的 `docs/en/` 目录。

- 项目介绍与入门：README、`docs/zh/quick_start/start.rst`。
- Demo：`docs/zh/quick_start/demo.rst`、`docs/zh/demo/`，以及源码中的 `demo/`；结合目标芯片对应的示例工程使用。
- API：`docs/zh/api/api_device.rst`（从机注册及初始化）、`docs/zh/api/api_host.rst`（主机 core/class）、`docs/zh/api/api_port.rst`（端点读写、URB 提交和取消）、`docs/zh/api/api_config.rst`（配置）。
- 移植：`docs/zh/quick_start/transplant.rst`，结合 `port/`、`osal/` 和目标板工程。
- Q&A：`docs/zh/quick_start/q&a.rst`。

API 声明以当前 `core/usbd_core.h`、`core/usbh_core.h`、`common/usb_dc.h`、`common/usb_hc.h` 为准。结合 `usb_config.h` 确认实际控制器、DMA、cache 和 OSAL 配置。

## 环境

- MCU、RTOS、单核抢占式调度。
- CherryUSB 线程优先级数值越小，优先级越高。
- 主机枚举线程优先级需要小于其他 USB 应用线程，保持最低优先级，负责枚举和断连处理。
- 主机在断开时，USB 应用线程必须保证退出和不再使用 class 资源，否则用户需要在释放 class 前的 `usbh_xxx_stop` 中增加一些处理
