General Chip Porting Guide
=======================================

This section mainly introduces the general steps and precautions for porting CherryUSB host and device protocol stacks to all chips with USB IP. Before porting, you need to **prepare a basic project that can print helloworld**. Default printing uses `printf`. If it's host mode, **you need to prepare a basic project that can execute OS scheduling normally**.

USB Device Porting Key Points
-------------------------------------

- Copy CherryUSB source code to the project directory and add source files and header file paths as needed. It's recommended to add all header file paths. Among them, `usbd_core.c` and `usb_dc_xxx.c` are required items. `usb_dc_xxx.c` is the USB IP DCD driver corresponding to the chip. If you don't know which USB IP your chip belongs to, refer to the readme of different USB IPs under the **port** directory. If the USB IP you're using is not supported, you'll have to implement it yourself
- Copy `cherryusb_config_template.h` file to your project directory, rename it to `usb_config.h`, and add the corresponding directory header file path
- Implement `usb_dc_low_level_init` function (this function is mainly responsible for USB clock, pin, and interrupt initialization). This function can be placed in any C file that participates in compilation. For USB clock, pin, interrupt initialization, please add them yourself according to the source code provided by your chip manufacturer.
- Call `USBD_IRQHandler` in the interrupt function and pass `busid`. If `USBD_IRQHandler` already exists in the interrupt entry of your SDK, please change the name in the USB protocol stack
- If the chip has cache, refer to the :ref:`usb_cache` section for cache modifications
- Register descriptors and call `usbd_initialize`, fill in `busid` and USB IP's `reg base`. `busid` starts from 0 and cannot exceed `CONFIG_USBDEV_MAX_BUS`. You can directly use templates under the demo directory

USB Host Porting Key Points
-------------------------------------

- Copy CherryUSB source code to project directory and add source files and header file paths as needed. It's recommended to add all header file paths. Among them, `usbh_core.c`, `usb_hc_xxx.c` and source files under the **osal** directory (choose corresponding source files according to different OS) are required. `usb_hc_xxx.c` is the USB IP HCD driver corresponding to the chip. If you don't know which USB IP your chip belongs to, refer to the readme of different USB IPs under the **port** directory. If the USB IP you're using is not supported, you'll have to implement it yourself
- Copy `cherryusb_config_template.h` file to your project directory, rename it to `usb_config.h`, and add the corresponding directory header file path
- Implement `usb_hc_low_level_init` function (this function is mainly responsible for USB clock, pin, and interrupt initialization). This function can be placed in any C file that participates in compilation. For USB clock, pin, interrupt initialization, please add them yourself according to the source code provided by your chip manufacturer.
- Call `usbh_initialize` and fill in `busid`, USB IP's `reg base`, and `event_handler` (can be omitted as NULL). `busid` starts from 0 and cannot exceed `CONFIG_USBHOST_MAX_BUS`
- Call `USBH_IRQHandler` in the interrupt function and pass `busid`. If `USBH_IRQHandler` already exists in the interrupt entry of your SDK, please change the name in the USB protocol stack
- Refer to the :ref:`usbh_link_script` section for linker script modifications
- If the chip has cache, refer to the :ref:`usb_cache` section for cache modifications
- Call `usbh_initialize`, fill in `busid`, USB IP's `reg base`, and `event_handler` (can be omitted as NULL). `busid` starts from 0 and cannot exceed `CONFIG_USBHOST_MAX_BUS`. For basic CDC + HID + MSC, refer to the `usb_host.c` file. For others, refer to adaptations under the **platform** directory

.. _usbh_link_script:

Host Linker Script Modifications
-------------------------------------

When using the host, if you haven't modified the linker script, you will encounter `__usbh_class_info_start__` and `__usbh_class_info_end__` undefined errors. This is because the host protocol stack requires adding a section to the linker script to store class information.

- No modification needed if using KEIL

- If using GCC, add the following code to the linker script (needs to be placed in flash location, recommended at the end):

.. code-block:: C

        // Add the following code to the ld file
        . = ALIGN(4);
        __usbh_class_info_start__ = .;
        KEEP(*(.usbh_class_info))
        __usbh_class_info_end__ = .;

GCC example:

.. code-block:: C

        /* The program code and other data into "FLASH" Rom type memory */
        .text :
        {
        . = ALIGN(4);
        *(.text)           /* .text sections (code) */
        *(.text*)          /* .text* sections (code) */
        *(.glue_7)         /* glue arm to thumb code */
        *(.glue_7t)        /* glue thumb to arm code */
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))
        . = ALIGN(4);
        __usbh_class_info_start__ = .;
        KEEP(*(.usbh_class_info))
        __usbh_class_info_end__ = .;
        . = ALIGN(4);
        _etext = .;        /* define a global symbols at end of code */
        } > FLASH

- Segger Embedded Studio example:

.. code-block:: C

        define block cherryusb_usbh_class_info { section .usbh_class_info };

        define exported symbol __usbh_class_info_start__  = start of block cherryusb_usbh_class_info;
        define exported symbol __usbh_class_info_end__  = end of block cherryusb_usbh_class_info + 1;

        place in AXI_SRAM                         { block cherryusb_usbh_class_info };
        keep { section .usbh_class_info};


.. _usb_cache:

cache Configuration Modification
--------------------------------------

For chips with cache functionality, the protocol stack and port will not clean or invalidate RAM in the cache region, so a non-cache RAM area needs to be used for maintenance.
`USB_NOCACHE_RAM_SECTION` macro specifies variables to be placed in non-cache RAM. By default, `USB_NOCACHE_RAM_SECTION` is defined as `__attribute__((section(".noncacheable")))`.
Therefore, users need to add a no cache RAM section in the corresponding linker script, and the section must include `.noncacheable`.

.. note:: Note that just modifying the nocache section in the linker script is not sufficient. You also need to configure the RAM in that section to be truly nocache, which typically requires configuring MPU attributes (for ARM, refer to the stm32h7 demo).

GCC:

.. code-block:: C

        MEMORY
        {
        RAM    (xrw)    : ORIGIN = 0x20000000,   LENGTH = 256K - 64K
        RAM_nocache    (xrw)    : ORIGIN = 0x20030000,   LENGTH = 64K
        FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 512K
        }

        ._nocache_ram :
        {
        . = ALIGN(4);
        *(.noncacheable)
        } >RAM_nocache


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
