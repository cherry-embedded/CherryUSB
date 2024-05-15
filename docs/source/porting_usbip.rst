USB IP 勘误
==============================

本节主要对已经支持的 USB IP 在不同厂家上的一些差别说明并进行校对。欢迎补充。

FSDEV
--------------------------

FSDEV 仅支持从机。这个 ip 不同厂家基本都是基于标准的 usb 寄存器，有些芯片可能还需要配置 `PMA_ACCESS` 的值，默认为2。下表为具体芯片相关宏的修改值：

.. list-table::
    :widths: 30 20 30 30 30
    :header-rows: 1

    * - 芯片
      - 中断名
      - 寄存器地址
      - CONFIG_USBDEV_EP_NUM
      - PMA_ACCESS
    * - STM32F0
      - USB_IRQHandler
      - 0x40005C00
      - 8
      - 1
    * - STM32F1
      - USB_LP_CAN1_RX0_IRQHandler
      - 同上
      - 同上
      - 同上
    * - STM32F3
      - USB_LP_CAN_RX0_IRQHandler
      - 同上
      - 同上
      - 1 or 2
    * - STM32L0
      - USB_IRQHandler
      - 同上
      - 同上
      - 1
    * - STM32L1
      - USB_LP_IRQHandler
      - 同上
      - 同上
      - 2
    * - STM32L4
      - USB_IRQHandler
      - 同上
      - 同上
      - 1

fsdev 需要外置 dp 上拉才能使用，有些芯片可能是接上拉电阻，有些芯片可能是设置寄存器，举例如下：

.. code-block:: C

    USB->BCDR |= (uint16_t)USB_BCDR_DPPU;

如果不存在 BCDR 寄存器，则一般是配置如下，并且该设置需要配置到 `usb_dc_low_level_init` 中或者 `usb_dc_init` 最后都行：

.. code-block:: C

  /* Pull up controller register */
  #define DP_CTRL ((__IO unsigned*)(0x40001820))

  #define _EnPortPullup() (*DP_CTRL = (*DP_CTRL) | 0x10000000);
  #define _DisPortPullup() (*DP_CTRL = (*DP_CTRL) & 0xEFFFFFFF);

MUSB
--------------------------

MUSB IP 支持主从，并且由 **mentor** 定义了一套标准的寄存器偏移，如果非标准，则需要实现以下宏的偏移，以标准为例：

.. code-block:: C

    #define MUSB_FADDR_OFFSET 0x00
    #define MUSB_POWER_OFFSET 0x01
    #define MUSB_TXIS_OFFSET  0x02
    #define MUSB_RXIS_OFFSET  0x04
    #define MUSB_TXIE_OFFSET  0x06
    #define MUSB_RXIE_OFFSET  0x08
    #define MUSB_IS_OFFSET    0x0A
    #define MUSB_IE_OFFSET    0x0B

    #define MUSB_EPIDX_OFFSET 0x0E

    #define MUSB_IND_TXMAP_OFFSET      0x10
    #define MUSB_IND_TXCSRL_OFFSET     0x12
    #define MUSB_IND_TXCSRH_OFFSET     0x13
    #define MUSB_IND_RXMAP_OFFSET      0x14
    #define MUSB_IND_RXCSRL_OFFSET     0x16
    #define MUSB_IND_RXCSRH_OFFSET     0x17
    #define MUSB_IND_RXCOUNT_OFFSET    0x18
    #define MUSB_IND_TXTYPE_OFFSET     0x1A
    #define MUSB_IND_TXINTERVAL_OFFSET 0x1B
    #define MUSB_IND_RXTYPE_OFFSET     0x1C
    #define MUSB_IND_RXINTERVAL_OFFSET 0x1D

    #define MUSB_FIFO_OFFSET 0x20

    #define MUSB_DEVCTL_OFFSET 0x60

    #define MUSB_TXFIFOSZ_OFFSET  0x62
    #define MUSB_RXFIFOSZ_OFFSET  0x63
    #define MUSB_TXFIFOADD_OFFSET 0x64
    #define MUSB_RXFIFOADD_OFFSET 0x66

    #define MUSB_TXFUNCADDR0_OFFSET 0x80
    #define MUSB_TXHUBADDR0_OFFSET  0x82
    #define MUSB_TXHUBPORT0_OFFSET  0x83
    #define MUSB_TXFUNCADDRx_OFFSET 0x88
    #define MUSB_TXHUBADDRx_OFFSET  0x8A
    #define MUSB_TXHUBPORTx_OFFSET  0x8B
    #define MUSB_RXFUNCADDRx_OFFSET 0x8C
    #define MUSB_RXHUBADDRx_OFFSET  0x8E
    #define MUSB_RXHUBPORTx_OFFSET  0x8F

    #define USB_TXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx)
    #define USB_TXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 2)
    #define USB_TXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 3)
    #define USB_RXADDR_BASE(ep_idx)    (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 4)
    #define USB_RXHUBADDR_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 6)
    #define USB_RXHUBPORT_BASE(ep_idx) (USB_BASE + MUSB_TXFUNCADDR0_OFFSET + 0x8 * ep_idx + 7)

下表为具体芯片从机相关宏的修改值：

.. list-table::
    :widths: 30 30 30 30
    :header-rows: 1

    * - 芯片
      - 中断名
      - 寄存器地址
      - CONFIG_USBDEV_EP_NUM
    * - ES32F3xx
      - USB_INT_Handler
      - 0x40086400
      - 5
    * - MSP432Ex
      - 同上
      - 0x40050000
      - 同上
    * - F1C100S
      - USB_INT_Handler
      - 0x01c13000
      - 4

下表为具体芯片主机相关宏的修改值：

.. list-table::
    :widths: 30 30 30 30
    :header-rows: 1

    * - 芯片
      - 中断名
      - 寄存器地址
      - CONIFG_USB_MUSB_EP_NUM
    * - ES32F3xx
      - USB_INT_Handler
      - 0x40086400
      - 5
    * - MSP432Ex
      - 同上
      - 0x40050000
      - 同上
    * - F1C100S
      - USB_INT_Handler
      - 0x01c13000
      - 4

DWC2
--------------------------

DWC2 IP 支持主从，并且由 **synopsys** 定义了一套标准的寄存器偏移。大部分厂家都使用标准的寄存器偏移(除了 GCCFG(GGPIO)寄存器)，所以如果是从机仅需要修改 `中断名` 、 `USB_BASE` 、 `CONFIG_USBDEV_EP_NUM` ，主机仅需要修改 `中断名` 、 `USB_BASE`  即可。

.. note:: GCCFG(GGPIO) 根据不同的厂家设置不同，会影响 usb 枚举，需要根据厂家提供的手册进行配置,并实现 usbd_get_dwc2_gccfg_conf 和 usbh_get_dwc2_gccfg_conf 函数，填充相应需要使能的bit

.. caution:: 主机 port 仅支持有 dma 功能的 dwc2 ip(代码中会判断当前 ip 是否支持), 如果不支持 dma 模式，则无法使用。

下表为具体芯片从机相关宏的修改值：

.. list-table::
    :widths: 30 30 30 30
    :header-rows: 1

    * - 芯片
      - 中断名
      - 寄存器地址
      - CONFIG_USBDEV_EP_NUM
    * - STM32 非 H7
      - OTG_FS_IRQHandler/OTG_HS_IRQHandler
      - 0x50000000UL/0x40040000UL
      - 5
    * - STM32 H7
      - 同上
      - 0x40080000UL/0x40040000UL
      - 9

下表为具体芯片主机相关宏的修改值：

.. list-table::
    :widths: 30 30 30 30
    :header-rows: 1

    * - 芯片
      - 中断名
      - 寄存器地址
      - CONFIG_USBHOST_PIPE_NUM
    * - STM32 全系列
      - OTG_HS_IRQHandler
      - 0x40040000UL
      - 12

EHCI
--------------------------

EHCI 是 intel 制定的标准主机控制器接口，任何厂家都必须实现 EHCI 中定义的寄存器以及寄存器的功能。EHCI 相关配置宏如下：

.. code-block:: C

  #define CONFIG_USB_EHCI_HCCR_OFFSET     (0x0)
  #define CONFIG_USB_EHCI_FRAME_LIST_SIZE 1024
  #define CONFIG_USB_EHCI_QH_NUM          CONFIG_USBHOST_PIPE_NUM
  #define CONFIG_USB_EHCI_QTD_NUM         3
  #define CONFIG_USB_EHCI_ITD_NUM         20
  //是否关闭保留寄存器的占位，默认保留 9 个双字的占位
  #define CONFIG_USB_EHCI_HCOR_RESERVED_DISABLE
  //是否使能 configflag 寄存器中的 bit0
  #define CONFIG_USB_EHCI_CONFIGFLAG
  #define CONFIG_USB_EHCI_ISO
  // 不带 tt的 IP 一般使用OHCI
  #define CONFIG_USB_EHCI_WITH_OHCI

同时由于 EHCI 只是主机控制器并且只支持高速，一般配合一个 otg 控制器和一个低速全速兼容控制单元，而速度的获取一般是在 otg 寄存器中，所以需要用户实现 `usbh_get_port_speed` 函数。