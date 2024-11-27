基于现有 demo 快速验证
=========================

在学习 USB 或者是学习 CherryUSB 代码之前，我们需要先基于现有的 demo 进行快速验证，为什么？是为了提升对 USB 的兴趣，能有信心进行下一步的动作，如果 demo 都跑不起来，或者自己摸索写代码，或者先看 USB 基本概念，结果看到最后，
发现一点都看不懂，概念好多，根本记不住，从而丧失对 USB 的兴趣。因此，先跑 demo 非常重要。下面我将给大家罗列目前支持的 demo 仓库。

基于 bouffalolab 系列芯片
---------------------------

仓库参考：https://github.com/CherryUSB/cherryusb_bouffalolab

- BL616/BL808 是一个 USB2.0 并且内置高速 PHY 芯片，共 5个端点（包含端点0）。支持主从机。
- USB 的相关应用位于 `examples/usbdev` 和 `examples/usbhost` 目录下，根据官方环境搭建完成后，即可编译使用。

基于 HPMicro 系列芯片
---------------------------

仓库参考：https://github.com/CherryUSB/cherryusb_hpmicro

- HPM 系列芯片均 USB 2.0 并且内置高速 PHY，支持主从机，端点共 8/16 个，并且可以同时使用双向，不同芯片个数有差异
- USB 的相关应用位于 `samples/cherryusb` ，根据官方环境搭建完成后，即可编译使用。

基于 esp32s2/s3/p4 系列芯片
---------------------------

仓库参考：https://github.com/CherryUSB/cherryusb_esp32

- esp32s2/s3 支持全速主从机，esp32p4 支持高速主从机
- 默认提供主机 demo，并且使用 esp 组件库进行开发， 在 https://components.espressif.com/ 中搜索 cherryusb 即可

基于飞腾派系列芯片
---------------------------

仓库参考：https://gitee.com/phytium_embedded/phytium-free-rtos-sdk

- 飞腾派支持两个 USB3.0 主机， 两个 USB2.0 主从机
- USB 的相关应用位于 `example/peripheral/usb` ，根据官方环境搭建完成后，即可编译使用。

基于 ST 系列芯片
---------------------------

仓库参考：https://github.com/CherryUSB/cherryusb_stm32

默认提供以下 demo 工程：

- F103 使用 fsdev ip
- F429 主从使用 USB1, 引脚 pb14/pb15, 并且都使用 dma 模式
- H7 设备使用 USB0, 引脚 pa11/pa12，主机使用 USB_OTG_HS ,引脚 pb14/pb15，并且需要做 nocache 处理

demo 底下提供了 **stm32xxx.ioc** 文件，双击打开，点击 **Generate Code** 即可。

.. caution:: 生成完以后，请使用 git reset 功能将被覆盖的 `main.c` 和 `stm32xxx_it.c` 文件撤回，禁止被 cubemx 覆盖。

涵盖 F1/F4/H7，其余芯片基本类似，不再赘述，具体区别有：

- usb ip 区别：F1使用 fsdev，F4/H7使用 dwc2
- dwc2 ip 区别： USB0 (引脚是 PA11/PA12) 和 USB1 (引脚是 PB14/PB15), 其中 USB1 默认全速，可以接外部PHY 形成高速主机，并且带 dma 功能
- F4 无cache，H7 有 cache

如果是 STM32F7/STM32H7 这种带 cache 功能，需要将 usb 使用到的 ram 定位到 no cache ram 区域。举例如下

.. code-block:: C

    cpu_mpu_config(0, MPU_Normal_NonCache, 0x24070000, MPU_REGION_SIZE_64KB);

对应 keil 中的 sct 脚本修改：

.. code-block:: C

    LR_IROM1 0x08000000 0x00200000  {    ; load region size_region
    ER_IROM1 0x08000000 0x00200000  {  ; load address = execution address
    *.o (RESET, +First)
    *(InRoot$$Sections)
    .ANY (+RO)
    .ANY (+XO)
    }
    RW_IRAM2 0x24000000 0x00070000  {  ; RW data
    .ANY (+RW +ZI)
    }
    USB_NOCACHERAM 0x24070000 0x00010000  {  ; RW data
    *(.noncacheable)
    }
    }

USB Device 移植要点
^^^^^^^^^^^^^^^^^^^^^^

- 使用 **stm32cubemx** 创建工程，配置基本的 RCC、UART (作为log使用)

.. figure:: img/stm32_1.png
.. figure:: img/stm32_2.png

- 如果使用 fsdev ip，勾选 **USB** 。如果使用 dwc2 ip，勾选 **USB_OTG_FS** 或者勾选  **USB_OTG_HS**。开启 USB 中断，其他配置对我们没用，代码中不会使用任何 st 的 usb 库。

.. figure:: img/stm32_3_1.png
.. figure:: img/stm32_3.png

- 配置 usb clock 为 48M

.. figure:: img/stm32_4_1.png
.. figure:: img/stm32_4.png

- 选择好工程，这里我们选择 keil，设置好 stack 和 heap，如果使用 msc 可以推荐设置大点，然后点击 **Generate Code**。

.. figure:: img/stm32_5.png

- 添加 CherryUSB 必须要的源码（ **usbd_core.c** 、 **usb_dc_dwc2.c** 或者是 **usb_dc_fsdev.c**  ）,以及想要使用的 class 驱动，可以将对应的 class template 添加方便测试。

.. figure:: img/stm32_6.png

- 头文件该加的加

.. figure:: img/stm32_7.png

- 复制一份 **cherryusb_config_template.h**，放到 `Core/Inc` 目录下，并命名为 `usb_config.h`

.. figure:: img/stm32_8.png

- 如果使用 dwc2 ip，需要增加 **usb_glue_st.c** 文件，并在 `usb_config.h` 中实现以下宏：

.. code-block:: C

    // 以下细节如有出入，请对照 stm32xxx.h 文件修改
    // 需要根据硬件实际的 fifo 深度进行修改，默认是最基础的配置
    #define CONFIG_USBDEV_EP_NUM 6
    #define CONFIG_USB_DWC2_RXALL_FIFO_SIZE (1012 - 16 * 6)
    #define CONFIG_USB_DWC2_TX0_FIFO_SIZE (64 / 4)
    #define CONFIG_USB_DWC2_TX1_FIFO_SIZE (64 / 4)
    #define CONFIG_USB_DWC2_TX2_FIFO_SIZE (64 / 4)
    #define CONFIG_USB_DWC2_TX3_FIFO_SIZE (64 / 4)
    #define CONFIG_USB_DWC2_TX4_FIFO_SIZE (64 / 4)
    #define CONFIG_USB_DWC2_TX5_FIFO_SIZE (64 / 4)

- 如果使用 fsdev ip，在 `usb_config.h` 中实现以下宏：

.. code-block:: C

    #define CONFIG_USBDEV_EP_NUM 8
    #define CONFIG_USBDEV_FSDEV_PMA_ACCESS 2

- 编译器推荐使用 **AC6**。勾选 **Microlib**，并实现 **printf** ，方便后续查看 log。

.. figure:: img/stm32_10.png
.. figure:: img/stm32_11.png

- 拷贝 **xxx_msp.c** 中的 **HAL_PCD_MspInit** 函数中的内容到 **usb_dc_low_level_init** 函数中，屏蔽 st 生成的 usb 初始化

.. figure:: img/stm32_12.png
.. figure:: img/stm32_14.png

- 在中断函数中调用 `USBD_IRQHandler`，并传入 `busid`

.. figure:: img/stm32_13.png

- 调用 template 的内容初始化，并填入 `busid` 和 USB IP 的 `reg base`， `busid` 从 0 开始，不能超过 `CONFIG_USBDEV_MAX_BUS`

.. figure:: img/stm32_15.png

USB Host 移植要点
^^^^^^^^^^^^^^^^^^^^^^

前面 6 步与 Device 一样。需要注意，host 驱动只支持带 dma 的 hs port (引脚是 PB14/PB15)，所以 fs port (引脚是 PA11/PA12)不做支持（没有 dma 你玩什么主机）。

- 添加 CherryUSB 必须要的源码（ **usbh_core.c** 、 **usbh_hub.c** 、 **usb_hc_dwc2.c** 、以及 **osal** 目录下的适配层文件）,以及想要使用的 class 驱动，并且可以将对应的 **usb host.c** 添加方便测试。

.. figure:: img/stm32_16.png

- 编译器推荐使用 **AC6**。勾选 **Microlib**，并实现 **printf** ，方便后续查看 log。

.. figure:: img/stm32_10.png
.. figure:: img/stm32_11.png

- 复制一份 **cherryusb_config_template.h**，放到 `Core/Inc` 目录下，并命名为 `usb_config.h`

- 增加 **usb_glue_st.c** 文件，并在 `usb_config.h` 中实现以下宏：

.. code-block:: C

    // 以下细节如有出入，请对照 stm32xxx.h 文件修改
    // 需要根据硬件实际的 fifo 深度进行修改，默认是最基础的配置
    #define CONFIG_USBHOST_PIPE_NUM 12
    #define CONFIG_USB_DWC2_NPTX_FIFO_SIZE (512 / 4)
    #define CONFIG_USB_DWC2_PTX_FIFO_SIZE (1024 / 4)
    #define CONFIG_USB_DWC2_RX_FIFO_SIZE ((1012 - CONFIG_USB_DWC2_NPTX_FIFO_SIZE - CONFIG_USB_DWC2_PTX_FIFO_SIZE) / 4)

- 拷贝 **xxx_msp.c** 中的 `HAL_HCD_MspInit` 函数中的内容到 `usb_hc_low_level_init` 函数中，屏蔽 st 生成的 usb 初始化
- 在中断函数中调用 `USBH_IRQHandler`，并传入 `busid`
- 调用 `usbh_initialize` 并填入 `busid` 和 USB IP 的 `reg base`， `busid` 从 0 开始，不能超过 `CONFIG_USBHOST_MAX_BUS`
- 启动线程

.. figure:: img/stm32_18.png
.. figure:: img/stm32_19.png

- 如果使用 **msc**，并且带文件系统，需要自行添加文件系统文件了，对应的 porting 编写参考 **fatfs_usbh.c** 文件。

.. figure:: img/stm32_21.png

