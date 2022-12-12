芯片通用移植指南
=========================

本节主要介绍所有带 USB IP 的芯片，移植 CherryUSB 主从协议栈时的通用步骤和注意事项。在往下看之前，需要 **你准备好一个可以打印 helloworld 的基本工程** ，并且实现了 `printf` 、 `malloc`、 `free`。如果是主机，需要 **准备好可以打印 helloworld 的带 OS 的工程**。

USB Device 移植要点
-----------------------

- 拷贝 CherryUSB 源码到工程目录下，并按需添加源文件和头文件路径，其中 `usbd_core.c` 和 `usb_dc_xxx.c` 为必须添加项。而 `usb_dc_xxx.c` 是芯片所对应的 USB IP dcd 部分驱动，如果不知道自己芯片属于那个 USB IP，参考 **port** 目录下的不同 USB IP 的 readme。如果使用的 USB IP 没有支持，只能自己实现了
- 添加 `USBD_IRQHandler=xxxx` 、 `USB_NUM_BIDIR_ENDPOINTS=x` 以及 `USB_BASE=0xxxxx` 三个 cflag 编译选项，如果没有添加则使用 `usb_dc_xxx.c` 中默认配置
- 拷贝 `cherryusb_config_template.h` 文件到自己工程目录下，命名为 `usb_config.h`，并添加相应的目录头文件路径。
- 实现 `usb_dc_low_level_init` 函数（该函数主要负责 USB 时钟、引脚、中断的初始化）。该函数可以放在你想要放的任何参与编译的 c 文件中。如何进行 USB 的时钟、引脚、中断等初始化，请自行根据你使用的芯片原厂提供的源码中进行添加。
- 描述符的注册、class的注册、接口的注册、端点中断的注册。不会的参考 demo 下的 template
- 调用 `usbd_initialize` 初始化 usb 硬件
- 编译使用。各个 class 如何使用，参考 demo 下的 template

.. note:: device 移植要点其实就三个，实现 `usb_dc_low_level_init` ;改 `USBD_IRQHandler=xxxx` 、`USB_BASE=0xxxxx` 、 `USB_NUM_BIDIR_ENDPOINTS=x`;改 `usb_config.h` 中的内容。其中前面说到的3个宏也可以在 `usb_config.h` 添加

USB Host 移植要点
-----------------------

- 拷贝 CherryUSB 源码到工程目录下，并按需添加源文件和头文件路径，其中 `usbh_core.c` 、 `usb_hc_xxx.c` 以及 **osal** 目录下源文件（根据不同的 os 选择对应的源文件）为必须添加项。而 `usb_hc_xxx.c` 是芯片所对应的 USB IP dcd 部分驱动，如果不知道自己芯片属于那个 USB IP，参考 **port** 目录下的不同 USB IP 的 readme。如果使用的 USB IP 没有支持，只能自己实现了
- 添加 `USBH_IRQHandler=xxxx`  以及 `USB_BASE=0xxxxx` 两个 cflag 编译选项，如果没有添加则使用 `usb_hc_xxx.c` 中默认配置
- 拷贝 `cherryusb_config_template.h` 文件到自己工程目录下，命名为 `usb_config.h`，并添加相应的目录头文件路径。
- 实现 `usb_hc_low_level_init` 函数（该函数主要负责 USB 时钟、引脚、中断的初始化）。该函数可以放在你想要放的任何参与编译的 c 文件中。如何进行 USB 的时钟、引脚、中断等初始化，请自行根据你使用的芯片原厂提供的源码中进行添加。
- 调用 `usbh_initialize` 初始化 usb 硬件
- 如果使用的是 GCC ，需要在链接脚本(ld)中添加如下代码：

.. code-block:: C

        /* section information for usbh class */
        . = ALIGN(4);
        __usbh_class_info_start__ = .;
        KEEP(*(.usbh_class_info))
        __usbh_class_info_end__ = .;

- 如果使用的是 Segger Embedded Studio ，需要在链接脚本(icf)中添加如下代码：

.. code-block:: C

        define block cherryusb_usbh_class_info { section .usbh_class_info };

        define exported symbol __usbh_class_info_start__  = start of block cherryusb_usbh_class_info;
        define exported symbol __usbh_class_info_end__  = end of block cherryusb_usbh_class_info + 1;

        place in AXI_SRAM                         { block cherryusb_usbh_class_info };
        keep { section .usbh_class_info};

- 编译使用。各个 class 如何使用，参考 demo 下的 `usb_host.c` 文件

.. caution:: 如果主从 ip 共用一个中断，设置 `USBD_IRQHandler=USBD_IRQHandler` 、 `USBH_IRQHandler=USBH_IRQHandler` ，然后由真正的中断函数根据主从模式调用这两个函数。

带 cache 功能的芯片使用注意
-------------------------------

协议栈以及 port 中不会对 cache 区域的 ram 进行 clean 或者 invalid，所以需要使用一块非 cache 区域的 ram 来维护。 `USB_NOCACHE_RAM_SECTION` 宏表示将变量指定到非 cache ram上，
因此，用户需要在对应的链接脚本中添加 no cache ram 的 section。默认 `USB_NOCACHE_RAM_SECTION` 定义为  `__attribute__((section(".noncacheable")))`。

GCC:

.. code-block:: C

        // 放在 no cache ram 的 region 中
        .no_cache_ram_region : AT (__no_cache_ram_addr)
        {
                . = ALIGN(4);
                *(.noncacheable)
                . = ALIGN(4);
        } > no_cache_ram

SCT:

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

ICF:

.. code-block:: C

        define region NONCACHEABLE_RAM = [from 0x1140000 size 256K];
        place in NONCACHEABLE_RAM                   { section .noncacheable, section .noncacheable.init, section .noncacheable.bss };  // Noncacheable
