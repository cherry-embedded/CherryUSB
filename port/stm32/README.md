# 基本工程生成

- 首先使用 stm32cubemx 使能系统时钟、USB时钟、USB引脚，勾选 USB外设、使能 USB 中断，然后产生基本工程。
- 如果使用的是 usb_dc_nohal.c，需要注释掉 MX_USB_PCD_Init 函数，USB 中断中调用 USBD_IRQHandler 替代 HAL_PCD_IRQHandler 函数，HAL_PCD_MspInit 函数保留
- 如果使用的是 usb_dc_hal.c 则上条不需要
- 推荐使用 nohal 版本，极简代码


## 该目录下porting可以不再使用，选择 fsdev 或者 synopsys下的porting接口

