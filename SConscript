from building import *

cwd = GetCurrentDir()
path = [cwd + '/common']
path += [cwd + '/core']
src  = Glob('core/usbd_core.c')
CPPDEFINES = []
if GetDepend(['PKG_USB_STACK_USING_HS']):
    CPPDEFINES+=['CONFIG_USB_HS']
    
# USB DEVICE
if GetDepend(['PKG_USB_STACK_USING_DEVICE']):
    if GetDepend(['PKG_USB_STACK_USING_CDC']):
        path += [cwd + '/class/cdc']
        src += Glob('class/cdc/usbd_cdc.c')
    if GetDepend(['PKG_USB_STACK_USING_HID']):
        path += [cwd + '/class/hid']
        src += Glob('class/cdc/usbd_hid.c')        
    if GetDepend(['PKG_USB_STACK_USING_DFU']):
        path += [cwd + '/class/dfu']
        src += Glob('class/cdc/usbd_dfu.c')
    if GetDepend(['PKG_USB_STACK_USING_HUB']):
        path += [cwd + '/class/hub']
        src += Glob('class/cdc/usbd_hub.c')
    if GetDepend(['PKG_USB_STACK_USING_AUDIO']):
        path += [cwd + '/class/audio']
        src += Glob('class/cdc/usbd_audio.c')
    if GetDepend(['PKG_USB_STACK_USING_VIDEO']):
        path += [cwd + '/class/video']
        src += Glob('class/cdc/usbd_video.c')
    if GetDepend(['PKG_USB_STACK_USING_MSC']):
        path += [cwd + '/class/msc']
        src += Glob('class/cdc/usbd_msc.c')
    if GetDepend(['SOC_FAMILY_STM32']):
        if GetDepend(['SOC_SERIES_STM32F0']):
            src += Glob('port/stm32/usb_dc_nohal.c')
            src += Glob('../../../../libraries/STM32F0xx_HAL/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pcd.c')
            src += Glob('../../../../libraries/STM32F0xx_HAL/STM32F0xx_HAL_Driver/Src/stm32f0xx_hal_pcd_ex.c')
            src += Glob('../../../../libraries/STM32F0xx_HAL/STM32F0xx_HAL_Driver/Src/stm32f0xx_ll_usb.c')
            CPPDEFINES += ['STM32F0']
        elif GetDepend(['SOC_SERIES_STM32F1']):
            src += Glob('port/stm32/usb_dc_nohal.c')
            src += Glob('../../../../libraries/STM32F1xx_HAL/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd.c')
            src += Glob('../../../../libraries/STM32F1xx_HAL/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pcd_ex.c')
            src += Glob('../../../../libraries/STM32F1xx_HAL/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_usb.c')
            CPPDEFINES += ['STM32F1']
        elif GetDepend(['SOC_SERIES_STM32F3']):
            src += Glob('port/stm32/usb_dc_nohal.c')
            src += Glob('../../../../libraries/STM32F3xx_HAL/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pcd.c')
            src += Glob('../../../../libraries/STM32F3xx_HAL/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pcd_ex.c')
            src += Glob('../../../../libraries/STM32F3xx_HAL/STM32F3xx_HAL_Driver/Src/stm32f3xx_ll_usb.c')
            CPPDEFINES += ['STM32F3']
        elif GetDepend(['SOC_SERIES_STM32F4']):
            src += Glob('port/stm32/usb_dc_hal.c')
            src += Glob('../../../../libraries/STM32F4xx_HAL/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c')
            src += Glob('../../../../libraries/STM32F4xx_HAL/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c')
            src += Glob('../../../../libraries/STM32F4xx_HAL/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c')
            CPPDEFINES += ['STM32F4']
        elif GetDepend(['SOC_SERIES_STM32H7']):
            src += Glob('port/stm32/usb_dc_hal.c')
            src += Glob('../../../../libraries/STM32H7xx_HAL/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd.c')
            src += Glob('../../../../libraries/STM32H7xx_HAL/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd_ex.c')
            src += Glob('../../../../libraries/STM32H7xx_HAL/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usb.c')
            CPPDEFINES += ['STM32H7']

# USB HOST       
if GetDepend(['USB_STACK_USING_HOST']):
    pass;

group = DefineGroup('usb_stack', src, depend = ['PKG_USING_USB_STACK'], CPPPATH = path, CPPDEFINES = CPPDEFINES)

Return('group')

