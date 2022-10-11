include_directories(${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/audio
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/cdc
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/dfu
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/hid
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/hub
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/midi
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/msc
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/mtp
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/printer
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/vendor
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/video
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/wireless

    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/common
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/core
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/demo
    ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/port/rp2040
    ${CMAKE_CURRENT_LIST_DIR}../../
)


add_library(cherryusblib STATIC)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/port/rp2040/usb_dc_rp2040.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/audio/usbd_audio.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/cdc/usbd_cdc.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/dfu/usbd_dfu.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/hid/usbd_hid.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/msc/usbd_msc.c)
# target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/mtp/usbd_mtp.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/printer/usbd_printer.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/video/usbd_video.c)
# target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/class/wireless/usbd_rndis.c)
target_sources(cherryusblib PRIVATE ${CMAKE_CURRENT_LIST_DIR}../../../../pico-sdk/lib/CherryUSB/core/usbd_core.c)

add_subdirectory(cdc_acm)
add_subdirectory(cdc_msc)
add_subdirectory(cdc_multitude)
add_subdirectory(dfu)
add_subdirectory(hid_cdc_msc)
add_subdirectory(hid_custom)
add_subdirectory(hid_kbd)
add_subdirectory(hid_mouse)
add_subdirectory(msc_ram)
add_subdirectory(uvc)

