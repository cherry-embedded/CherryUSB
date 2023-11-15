from building import *

cwd = GetCurrentDir()
path = [cwd + '/common']
path += [cwd + '/core']
path += [cwd + '/class/cdc']
path += [cwd + '/class/msc']
path += [cwd + '/class/hid']
path += [cwd + '/class/audio']
path += [cwd + '/class/video']
path += [cwd + '/class/wireless']
path += [cwd + '/class/dfu']
src = []

CPPDEFINES = []

# USB DEVICE
if GetDepend(['PKG_CHERRYUSB_DEVICE']):
    path += [cwd + '/osal']
    src += Glob('core/usbd_core.c')
    src += Glob('osal/usb_osal_rtthread.c')

    if GetDepend(['PKG_CHERRYUSB_DEVICE_HS']):
        CPPDEFINES+=['CONFIG_USB_HS']

    if GetDepend(['PKG_CHERRYUSB_DEVICE_CDC']):
        src += Glob('class/cdc/usbd_cdc.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_HID']):
        src += Glob('class/hid/usbd_hid.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_MSC']):
        src += Glob('class/msc/usbd_msc.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_AUDIO']):
        src += Glob('class/audio/usbd_audio.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_VIDEO']):
        src += Glob('class/video/usbd_video.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_RNDIS']):
        src += Glob('class/wireless/usbd_rndis.c')
    if GetDepend(['PKG_CHERRYUSB_USING_DFU']):
        src += Glob('class/dfu/usbd_dfu.c')

    if GetDepend(['PKG_CHERRYUSB_DEVICE_CDC_TEMPLATE']):
        src += Glob('demo/cdc_acm_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_HID_MOUSE_TEMPLATE']):
        src += Glob('demo/hid_mouse_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_HID_KEYBOARD_TEMPLATE']):
        src += Glob('demo/hid_keyboard_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_MSC_TEMPLATE']):
        src += Glob('demo/msc_ram_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_MSC_STORAGE_TEMPLATE']):
        src += Glob('demo/msc_storage_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_AUDIO_V1_TEMPLATE']):
        src += Glob('demo/audio_v1_mic_speaker_multichan_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_AUDIO_V2_TEMPLATE']):
        src += Glob('demo/audio_v2_mic_speaker_multichan_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_VIDEO_TEMPLATE']):
        src += Glob('demo/video_static_mjpeg_template.c')
    if GetDepend(['PKG_CHERRYUSB_DEVICE_RNDIS_TEMPLATE']):
        src += Glob('demo/cdc_rndis_template.c')

    if GetDepend(['PKG_CHERRYUSB_DEVICE_FSDEV']):
        src += Glob('port/fsdev/usb_dc_fsdev.c')

    if GetDepend(['PKG_CHERRYUSB_DEVICE_DWC2']):
        src += Glob('port/dwc2/usb_dc_dwc2.c')
        if GetDepend(['PKG_CHERRYUSB_DEVICE_DWC2_PORT_FS']):
            CPPDEFINES += ['CONFIG_USB_DWC2_PORT=FS_PORT']
        elif GetDepend(['PKG_CHERRYUSB_DEVICE_DWC2_PORT_HS']):
            CPPDEFINES += ['CONFIG_USB_DWC2_PORT=HS_PORT']
        if GetDepend(['SOC_SERIES_STM32F7']):
            CPPDEFINES += ['STM32F7']
        elif GetDepend(['SOC_SERIES_STM32H7']):
            CPPDEFINES += ['STM32H7']

    if GetDepend(['PKG_CHERRYUSB_DEVICE_MUSB']):
        src += Glob('port/musb/usb_dc_musb.c')
        if GetDepend(['PKG_CHERRYUSB_DEVICE_MUSB_SUNXI']):
            CPPDEFINES += ['CONFIG_USB_MUSB_SUNXI']

    if GetDepend(['PKG_CHERRYUSB_DEVICE_HPM']):
        src += Glob('port/hpm/usb_dc_hpm.c')

    if GetDepend(['PKG_CHERRYUSB_DEVICE_CH32_CH32V307']):
        if GetDepend(['PKG_CHERRYUSB_DEVICE_HS']):
            src += Glob('port/ch32/usb_dc_usbhs.c')
        else:
            src += Glob('port/ch32/usb_dc_usbfs.c')

    if GetDepend(['PKG_CHERRYUSB_DEVICE_PUSB2']):
        path += [cwd + '/port/pusb2/common']
        path += [cwd + '/port/pusb2/fpusb2']
        src += Glob('port/pusb2/fpusb2' + '/*.c')
        src += Glob('port/pusb2/usb_dc_pusb2.c') 

# USB HOST
if GetDepend(['PKG_CHERRYUSB_HOST']):
    path += [cwd + '/osal']
    path += [cwd + '/class/hub']
    src += Glob('core/usbh_core.c')
    src += Glob('class/hub/usbh_hub.c')
    src += Glob('osal/usb_osal_rtthread.c')

    if GetDepend(['PKG_CHERRYUSB_HOST_CDC']):
        src += Glob('class/cdc/usbh_cdc_acm.c')
    if GetDepend(['PKG_CHERRYUSB_HOST_HID']):
        src += Glob('class/hid/usbh_hid.c')
    if GetDepend(['PKG_CHERRYUSB_HOST_MSC']):
        src += Glob('class/msc/usbh_msc.c')
    if GetDepend(['PKG_CHERRYUSB_HOST_RNDIS']):
        src += Glob('class/wireless/usbh_rndis.c')
        src += Glob('third_party/rt-thread-4.1.1/rndis_host/rndis_host.c')

    if GetDepend(['PKG_CHERRYUSB_HOST_DWC2']):
        src += Glob('port/dwc2/usb_hc_dwc2.c')

    if GetDepend(['PKG_CHERRYUSB_HOST_MUSB']):
        src += Glob('port/musb/usb_hc_musb.c')
        if GetDepend(['PKG_CHERRYUSB_HOST_MUSB_SUNXI']):
            CPPDEFINES += ['CONFIG_USB_MUSB_SUNXI']

    if GetDepend(['PKG_CHERRYUSB_HOST_EHCI']):
        src += Glob('port/ehci/usb_hc_ehci.c')
        if GetDepend(['PKG_CHERRYUSB_HOST_EHCI_HPM']):
            src += Glob('port/ehci/usb_glue_hpm.c')

    if GetDepend(['PKG_CHERRYUSB_HOST_XHCI']):
        src += Glob('port/xhci/usb_hc_xhci.c')
        src += Glob('port/xhci/xhci_dbg.c')
        src += Glob('port/xhci/xhci.c')

    if GetDepend(['PKG_CHERRYUSB_HOST_PUSB2']):
        path += [cwd + '/port/pusb2/common']
        path += [cwd + '/port/pusb2/fpusb2']
        src += Glob('port/pusb2/fpusb2' + '/*.c')
        src += Glob('port/pusb2/usb_hc_pusb2.c')         

    if GetDepend(['PKG_CHERRYUSB_HOST_TEMPLATE']):
        src += Glob('demo/usb_host.c')

    if GetDepend(['PKG_CHERRYUSB_HOST_CP210X']):
        path += [cwd + '/class/vendor/cp201x']
        src += Glob('class/vendor/cp201x/usbh_cp210x.c')
        src += Glob('third_party/rt-thread-4.1.1/dfs/drv_usbh_cp210x_rtt.c')

src += Glob('third_party/rt-thread-4.1.1/msh_cmd.c')

group = DefineGroup('CherryUSB', src, depend = ['PKG_USING_CHERRYUSB'], CPPPATH = path, CPPDEFINES = CPPDEFINES)

Return('group')

