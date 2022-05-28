USB IP 差别说明
==============================

本节主要对已经支持的 USB IP 在不同厂家上的一些差别说明并进行校对。欢迎补充。

FSDEV
--------------------------

FSDEV 仅支持从机。这个 ip 不同厂家基本都是基于标准的 usb 寄存器，所以用户使用时，仅需要修改 `USBD_IRQHandler` 、 `USB_BASE` 、 `USB_NUM_BIDIR_ENDPOINTS` 即可。有些芯片可能还需要配置 `PMA_ACCESS` 的值，默认为2。下表为具体芯片相关宏的修改值：

.. list-table::
    :widths: 30 20 30 30 30
    :header-rows: 1

    * - 芯片
      - USBD_IRQHandler
      - USB_BASE
      - USB_NUM_BIDIR_ENDPOINTS
      - PMA_ACCESS
    * - STM32F0
      - USB_IRQHandler
      - 0x40005C00
      - 8
      - 2
    * - STM32F1
      - USB_LP_CAN1_RX0_IRQHandler
      - 同上
      - 同上
      - 同上
    * - STM32F3
      - USB_LP_CAN_RX0_IRQHandler
      - 同上
      - 同上
      - 同上
    * - STM32L0
      - USB_IRQHandler
      - 同上
      - 同上
      - 同上
    * - STM32L1
      - USB_LP_IRQHandler
      - 同上
      - 同上
      - 同上
    * - STM32L4
      - USB_IRQHandler
      - 同上
      - 同上
      - 同上

MUSB
--------------------------

MUSB IP 支持主从，并且由 **mentor** 定义了一套标准的寄存器偏移，所以如果是走的标准，则从机仅需要修改 `USBD_IRQHandler` 、 `USB_BASE` 、 `USB_NUM_BIDIR_ENDPOINTS` ，主机仅需要修改 `USBH_IRQHandler` 、 `USB_BASE` 以及 `CONIFG_USB_MUSB_EP_NUM` 即可。如果非标准，则需要实现以下宏的偏移，以标准为例：

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
      - USBD_IRQHandler
      - USB_BASE
      - USB_NUM_BIDIR_ENDPOINTS
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
      - USBH_IRQHandler
      - USB_BASE
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

SYNOPSYS
--------------------------

- STM32 芯片中，H7 的 FS 控制器（使用 PA11/PA12）基地址(**0x40080000UL**)与其他芯片 FS 控制器基地址(**0x50000000UL**)不一样， HS 基地址都是一样的。其次是关于 **VBUS_SENSE** 的控制，虽然寄存器相同，但是个别芯片对该 bit 操作 0 和操作 1 是相反的（估计是硬件设计问题），此问题会影响枚举，所以需要根据芯片控制是否使能 **CONFIG_USB_SYNOPSYS_NOVBUSSEN**。
不可靠的统计是新款 F4、F7、H7、L4 都是需要使能的。

.. code-block:: C

    /* Deactivate VBUS Sensing B */
    USBx->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    USBx->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;

    两者都是关闭作用，并且其中 USB_OTG_GCCFG_NOVBUSSENS 和 USB_OTG_GCCFG_VBDEN 都是 21 bit