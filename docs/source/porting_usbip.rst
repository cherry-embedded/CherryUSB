USB IP 差别说明
==============================

本节主要对常见的 USB IP 在不同芯片上实现时，列出需要注意的一些差别说明。


FSDEV
--------------------------

这个 ip 不同厂家基本都是基于标准的 usb 寄存器，所以，用户使用时，仅需要修改 `USBD_IRQHandler` 、 `USB_NUM_BIDIR_ENDPOINTS` 、 `USB_BASE` 即可。

MUSB
--------------------------

- ES32 和 MSP432 走的标准的寄存器，所以，用户使用时，仅需要修改 `USBD_IRQHandler` 和 `USB_BASE` 即可。
- 全志不同系列芯片都魔改了 musb 的寄存器，包括寄存器的偏移，所以除了修改 `USBD_IRQHandler` 和 `USB_BASE` ，还需要实现对应的 `寄存器结构体 <https://github.com/sakumisu/CherryUSB/blob/master/port/musb/usb_musb_reg.h>`_ 。以及寄存器偏移地址，举例如下：

.. code-block:: C

    #define USB_TXMAPx_BASE       (USB_BASE + MUSB_IND_TXMAP_OFFSET)
    #define USB_RXMAPx_BASE       (USB_BASE + MUSB_IND_RXMAP_OFFSET)
    #define USB_TXCSRLx_BASE      (USB_BASE + MUSB_IND_TXCSRL_OFFSET)
    #define USB_RXCSRLx_BASE      (USB_BASE + MUSB_IND_RXCSRL_OFFSET)
    #define USB_TXCSRHx_BASE      (USB_BASE + MUSB_IND_TXCSRH_OFFSET)
    #define USB_RXCSRHx_BASE      (USB_BASE + MUSB_IND_RXCSRH_OFFSET)
    #define USB_RXCOUNTx_BASE     (USB_BASE + MUSB_IND_RXCOUNT_OFFSET)
    #define USB_FIFO_BASE(ep_idx) (USB_BASE + MUSB_FIFO_OFFSET + 0x4 * ep_idx)
    #define USB_TXTYPEx_BASE      (USB_BASE + MUSB_IND_TXTYPE_OFFSET)
    #define USB_RXTYPEx_BASE      (USB_BASE + MUSB_IND_RXTYPE_OFFSET)
    #define USB_TXINTERVALx_BASE  (USB_BASE + MUSB_IND_TXINTERVAL_OFFSET)
    #define USB_RXINTERVALx_BASE  (USB_BASE + MUSB_IND_RXINTERVAL_OFFSET)

    #ifdef CONFIG_USB_MUSB_SUNXI
    #define USB_TXADDR_BASE(ep_idx)    (&USB->TXFUNCADDR0)
    #define USB_TXHUBADDR_BASE(ep_idx) (&USB->TXHUBADDR0)
    #define USB_TXHUBPORT_BASE(ep_idx) (&USB->TXHUBPORT0)
    #define USB_RXADDR_BASE(ep_idx)    (&USB->RXFUNCADDR0)
    #define USB_RXHUBADDR_BASE(ep_idx) (&USB->RXHUBADDR0)
    #define USB_RXHUBPORT_BASE(ep_idx) (&USB->RXHUBPORT0)
    #else
    #define USB_TXADDR_BASE(ep_idx)    (&USB->TXFUNCADDR0 + 0x8 * ep_idx)
    #define USB_TXHUBADDR_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 2)
    #define USB_TXHUBPORT_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 3)
    #define USB_RXADDR_BASE(ep_idx)    (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 4)
    #define USB_RXHUBADDR_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 6)
    #define USB_RXHUBPORT_BASE(ep_idx) (&USB->TXFUNCADDR0 + 0x8 * ep_idx + 7)
    #endif


SYNOPSYS
--------------------------

- STM32 芯片中，H7 的 FS 控制器（使用 PA11/PA12）基地址(**0x40080000UL**)与其他芯片 FS 控制器基地址(**0x50000000UL**)不一样， HS 基地址都是一样的。其次是关于 **VBUS_SENSE** 的控制，虽然寄存器相同，但是个别芯片对该 bit 操作 0 和操作 1 是相反的（估计是硬件设计问题），此问题会影响枚举，所以需要根据芯片控制是否使能 **CONFIG_USB_SYNOPSYS_NOVBUSSEN**。
不可靠的统计是新款 F4、F7、H7、L4 都是需要使能的。

.. code-block:: C

    /* Deactivate VBUS Sensing B */
    USBx->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    USBx->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;

    两者都是关闭作用，并且其中 USB_OTG_GCCFG_NOVBUSSENS 和 USB_OTG_GCCFG_VBDEN 都是 21 bit