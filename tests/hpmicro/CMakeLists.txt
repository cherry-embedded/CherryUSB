# Copyright (c) 2021 HPMicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

set(RV_ARCH "rv32imafc")
set(RV_ABI "ilp32f")

set(CONFIG_CHERRYSH 1)
set(CONFIG_CHERRYSH_INTERFACE "uart")
set(CONFIG_USB_DEVICE 1)

set(CONFIG_FREERTOS 1)
set(CONFIG_LWIP 1)
set(CONFIG_LWIP_IPERF 1)
set(CONFIG_LWIP_STRERR 1)
set(CONFIG_LWIP_NETDB 1)
set(CONFIG_LWIP_SOCKET_API 1)

set(CONFIG_HPM_PANEL 1)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})
project(cherryusb)

sdk_compile_definitions(-D__freertos_irq_stack_top=_stack)
sdk_compile_definitions(-DCONFIG_FREERTOS=1)
sdk_compile_definitions(-DUSE_NONVECTOR_MODE=1)
sdk_compile_definitions(-DDISABLE_IRQ_PREEMPTIVE=1)
sdk_compile_definitions(-DLWIP_TIMEVAL_PRIVATE=0)

sdk_compile_definitions(-DCONFIG_USBHOST_PLATFORM_CDC_ECM)
sdk_compile_definitions(-DCONFIG_USBHOST_PLATFORM_CDC_NCM)
sdk_compile_definitions(-DCONFIG_USBHOST_PLATFORM_CDC_RNDIS)
sdk_compile_definitions(-DCONFIG_USBHOST_PLATFORM_ASIX)
sdk_compile_definitions(-DCONFIG_USBHOST_PLATFORM_RTL8152)

# sdk_compile_definitions(-DCONFIG_USB_OTG_ENABLE)

sdk_compile_options("-O2")

sdk_ld_options(-uusbd_cdc_acm_init_intf)
sdk_ld_options(-uusbd_hid_init_intf)
sdk_ld_options(-uusbd_msc_init_intf)
sdk_ld_options(-uusbd_video_init_intf)
sdk_ld_options(-uusbd_audio_init_intf)
sdk_ld_options(-uusbd_cdc_ecm_init_intf)
sdk_ld_options(-uusbd_rndis_init_intf)
sdk_ld_options(-uusbd_dfu_init_intf)
sdk_ld_options(-uusbd_initialize)
sdk_ld_options(-uusbd_desc_register)
sdk_ld_options(-uusbd_add_interface)
sdk_ld_options(-uusbd_add_endpoint)
sdk_ld_options(-uusbd_rndis_start_write)
sdk_ld_options(-uusbd_rndis_start_read)
sdk_ld_options(-uusbd_cdc_ecm_start_write)
sdk_ld_options(-uusbd_cdc_ecm_start_read)
sdk_ld_options(-uusbd_video_stream_start_write)
sdk_ld_options(-uusbd_video_stream_split_transfer)

sdk_ld_options(-lm)

sdk_inc(inc)
sdk_app_src(inc/arch/sys_arch.c)
sdk_app_src(src/main.c)

sdk_inc(src/iperf)
sdk_app_src(src/iperf/iperf.c src/iperf/iperf_cli.c src/iperf/utils_getopt.c src/ping.c)
sdk_app_src(src/cdc_acm_otg_template.c)

set(CONFIG_CHERRYMP 1)
set(CONFIG_CHERRYUSB 1)
set(CONFIG_CHERRYUSB_DEVICE 1)
set(CONFIG_CHERRYUSB_HOST 1)

# sdk_app_src(src/uvc2lcd.c)
# sdk_app_src(src/font24x48.c)
# add_subdirectory(src/cherryusb_hostuvcuac)
add_subdirectory(../.. cherryusb)

generate_ses_project()
