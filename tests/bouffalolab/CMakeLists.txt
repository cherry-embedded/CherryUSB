cmake_minimum_required(VERSION 3.15)

include(proj.conf)

find_package(bouffalo_sdk REQUIRED HINTS $ENV{BL_SDK_BASE})

sdk_add_compile_definitions(-DCONFIG_USBHOST_PLATFORM_CDC_ECM)
sdk_add_compile_definitions(-DCONFIG_USBHOST_PLATFORM_CDC_NCM)
sdk_add_compile_definitions(-DCONFIG_USBHOST_PLATFORM_CDC_RNDIS)
sdk_add_compile_definitions(-DCONFIG_USBHOST_PLATFORM_ASIX)
sdk_add_compile_definitions(-DCONFIG_USBHOST_PLATFORM_RTL8152)
sdk_add_include_directories(inc)

target_sources(app PRIVATE ../../demo/usb_host.c)

set(CONFIG_CHERRYMP 1)
set(CONFIG_CHERRYUSB 1)
set(CONFIG_CHERRYUSB_DEVICE 1)
set(CONFIG_CHERRYUSB_HOST 1)

set(CONFIG_CHERRYUSB_DEVICE_CDC_RNDIS 1)
set(CONFIG_CHERRYUSB_DEVICE_CDC_ECM 1)

# add_subdirectory(src/cherryusb_hostuvcuac)
add_subdirectory(../.. cherryusb)

# sdk_add_link_options(-uusbd_cdc_acm_init_intf)
# sdk_add_link_options(-uusbd_hid_init_intf)
# sdk_add_link_options(-uusbd_msc_init_intf)
# sdk_add_link_options(-uusbd_video_init_intf)
# sdk_add_link_options(-uusbd_audio_init_intf)
# sdk_add_link_options(-uusbd_cdc_ecm_init_intf)
# sdk_add_link_options(-uusbd_rndis_init_intf)
# sdk_add_link_options(-uusbd_initialize)
# sdk_add_link_options(-uusbd_desc_register)
# sdk_add_link_options(-uusbd_add_interface)
# sdk_add_link_options(-uusbd_add_endpoint)
# sdk_add_link_options(-uusbd_rndis_start_write)
# sdk_add_link_options(-uusbd_rndis_start_read)
# sdk_add_link_options(-uusbd_cdc_ecm_start_write)
# sdk_add_link_options(-uusbd_cdc_ecm_start_read)
# sdk_add_link_options(-uusbd_video_stream_start_write)
# sdk_add_link_options(-uusbd_video_stream_split_transfer)

sdk_set_main_file(src/main.c)
project(cherryusb)
