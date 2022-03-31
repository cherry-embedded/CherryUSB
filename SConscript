from building import *

cwd = GetCurrentDir()
path = [cwd]
path += [cwd + '/common']
path += [cwd + '/core']
src = []

CPPDEFINES = []
if GetDepend(['PKG_CHERRYUSB_USING_HS']):
    CPPDEFINES+=['CONFIG_USB_HS']
elif GetDepend(['PKG_CHERRYUSB_USING_HS_IN_FULL']):
        CPPDEFINES += ['CONFIG_USB_HS_IN_FULL']

# USB DEVICE
if GetDepend(['PKG_CHERRYUSB_USING_DEVICE']):
    src += Glob('core/usbd_core.c')
    if GetDepend(['PKG_CHERRYUSB_USING_CDC']):
        path += [cwd + '/class/cdc']
        src += Glob('class/cdc/usbd_cdc.c')
    if GetDepend(['PKG_CHERRYUSB_USING_HID']):
        path += [cwd + '/class/hid']
        src += Glob('class/cdc/usbd_hid.c')
    if GetDepend(['PKG_CHERRYUSB_USING_DFU']):
        path += [cwd + '/class/dfu']
        src += Glob('class/cdc/usbd_dfu.c')
    if GetDepend(['PKG_CHERRYUSB_USING_HUB']):
        path += [cwd + '/class/hub']
        src += Glob('class/cdc/usbd_hub.c')
    if GetDepend(['PKG_CHERRYUSB_USING_AUDIO']):
        path += [cwd + '/class/audio']
        src += Glob('class/cdc/usbd_audio.c')
    if GetDepend(['PKG_CHERRYUSB_USING_VIDEO']):
        path += [cwd + '/class/video']
        src += Glob('class/cdc/usbd_video.c')
    if GetDepend(['PKG_CHERRYUSB_USING_MSC']):
        path += [cwd + '/class/msc']
        src += Glob('class/cdc/usbd_msc.c')
    if GetDepend(['SOC_FAMILY_STM32']):
        if GetDepend(['SOC_SERIES_STM32F0']) or GetDepend(['SOC_SERIES_STM32F1']) or GetDepend(['SOC_SERIES_STM32F3']) or GetDepend(['SOC_SERIES_STM32L0']):
            src += Glob('port/fsdev/usb_dc_fsdev.c')
        else:
            src += Glob('port/synopsys/usb_dc_synopsys.c')
            if GetDepend(['SOC_SERIES_STM32H7']) and GetDepend(['PKG_CHERRYUSB_USING_FS']):
                CPPDEFINES += ['USB_BASE=0x40080000UL']
    if GetDepend(['CHERRYUSB_USING_MUSB']):
        src += Glob('port/musb/usb_dc_musb.c')
    if GetDepend(['CHERRYUSB_USING_CDC_DEMO']):
        src += Glob('demo/cdc_acm_template.c')

# USB HOST
if GetDepend(['PKG_CHERRYUSB_USING_HOST']):
    src += Glob('core/usbh_core.c')
    path += [cwd + '/osal']
    src += Glob('osal/usb_osal_rtthread.c')
    src += Glob('osal/usb_workq.c')
    path += [cwd + '/class/cdc']
    src += Glob('class/cdc/usbh_cdc_acm.c')
    path += [cwd + '/class/hid']
    src += Glob('class/hid/usbh_hid.c')
    path += [cwd + '/class/msc']
    src += Glob('class/msc/usbh_msc.c')
    path += [cwd + '/class/hub']
    src += Glob('class/hub/usbh_hub.c')

    CPPDEFINES += ['CONFIG_USBHOST_HUB']
    if GetDepend(['SOC_FAMILY_STM32']):
        src += Glob('port/synopsys/usb_hc_synopsys.c')
    if GetDepend(['CHERRYUSB_USING_MUSB']):
        src += Glob('port/musb/usb_hc_musb.c')
    if GetDepend(['CHERRYUSB_USING_HOST_DEMO']):
        src += Glob('demo/usb_host.c')

group = DefineGroup('CherryUSB', src, depend = ['PKG_USING_CHERRYUSB'], CPPPATH = path, CPPDEFINES = CPPDEFINES)

Return('group')

