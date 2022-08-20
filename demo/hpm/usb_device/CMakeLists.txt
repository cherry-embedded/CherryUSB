# Copyright 2021 hpmicro
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.13)

set(CONFIG_CHERRYUSB 1)
set(CONFIG_USB_DEVICE 1)
set(CONFIG_CHERRYUSB_DEVICE 1)
set(CONFIG_CHERRYUSB_DEVICE_CDC 1)
set(CONFIG_CHERRYUSB_DEVICE_HID 1)
set(CONFIG_CHERRYUSB_DEVICE_MSC 1)
set(CONFIG_CHERRYUSB_DEVICE_DEMO_CDC_MSC 1)

find_package(hpm-sdk REQUIRED HINTS $ENV{HPM_SDK_BASE})

project(cherryusb)

sdk_app_inc(src)
sdk_app_src(src/hello_world.c)
generate_ses_project()
