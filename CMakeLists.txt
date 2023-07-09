add_library(cherryusb STATIC)

target_include_directories(cherryusb PUBLIC
common
core
osal
class/cdc
class/hid
class/msc
class/audio
class/video
class/hub
class/wireless
)

if(CONFIG_CHERRYUSB_DEVICE)
    target_sources(cherryusb PRIVATE core/usbd_core.c)
    if(CONFIG_CHERRYUSB_DEVICE_CDC)
    target_sources(cherryusb PRIVATE class/cdc/usbd_cdc.c)
    endif()
    if(CONFIG_CHERRYUSB_DEVICE_HID)
    target_sources(cherryusb PRIVATE class/hid/usbd_hid.c)
    endif()
    if(CONFIG_CHERRYUSB_DEVICE_MSC)
    target_sources(cherryusb PRIVATE class/msc/usbd_msc.c)
    endif()
    if(CONFIG_CHERRYUSB_DEVICE_AUDIO)
    target_sources(cherryusb PRIVATE class/audio/usbd_audio.c)
    endif()
    if(CONFIG_CHERRYUSB_DEVICE_VIDEO)
    target_sources(cherryusb PRIVATE class/video/usbd_video.c)
    endif()
endif()

if(CONFIG_CHERRYUSB_HOST)
    target_sources(cherryusb PRIVATE 
    core/usbh_core.c
    class/hub/usbh_hub.c
    class/cdc/usbh_cdc_acm.c
    class/hid/usbh_hid.c
    class/msc/usbh_msc.c
    class/audio/usbh_audio.c
    class/video/usbh_video.c
    class/wireless/usbh_rndis.c
    )

endif()

if(DEFINED CONFIG_CHERRYUSB_OSAL)
    if("${CONFIG_CHERRYUSB_OSAL}" STREQUAL "freertos")
    target_sources(cherryusb PRIVATE osal/usb_osal_freertos.c)
    elseif("${CONFIG_CHERRYUSB_OSAL}" STREQUAL "rtthread")
    target_sources(cherryusb PRIVATE osal/usb_osal_rtthread.c)
    elseif("${CONFIG_CHERRYUSB_OSAL}" STREQUAL "yoc")
    target_sources(cherryusb PRIVATE osal/usb_osal_yoc.c)
    endif()
endif()