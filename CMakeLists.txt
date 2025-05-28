cmake_minimum_required(VERSION 3.15)

if(CONFIG_CHERRYUSB)

if(BL_SDK_BASE)
    message(STATUS "enable cherryusb in bouffalo_sdk")

    set(CONFIG_CHERRYUSB_DEVICE_CDC_ACM 1)
    set(CONFIG_CHERRYUSB_DEVICE_HID 1)
    set(CONFIG_CHERRYUSB_DEVICE_MSC 1)
    set(CONFIG_CHERRYUSB_DEVICE_AUDIO 1)
    set(CONFIG_CHERRYUSB_DEVICE_VIDEO 1)

    set(CONFIG_CHERRYUSB_HOST_CDC_ACM 1)
    set(CONFIG_CHERRYUSB_HOST_CDC_ECM 1)
    set(CONFIG_CHERRYUSB_HOST_CDC_NCM 1)
    set(CONFIG_CHERRYUSB_HOST_HID 1)
    set(CONFIG_CHERRYUSB_HOST_MSC 1)
    set(CONFIG_CHERRYUSB_HOST_VIDEO 1)
    set(CONFIG_CHERRYUSB_HOST_AUDIO 1)
    set(CONFIG_CHERRYUSB_HOST_CDC_RNDIS 1)
    # set(CONFIG_CHERRYUSB_HOST_BLUETOOTH 1)
    set(CONFIG_CHERRYUSB_HOST_ASIX 1)
    set(CONFIG_CHERRYUSB_HOST_RTL8152 1)
    set(CONFIG_CHERRYUSB_HOST_CH34X 1)
    set(CONFIG_CHERRYUSB_HOST_CP210X 1)
    set(CONFIG_CHERRYUSB_HOST_FTDI 1)
    set(CONFIG_CHERRYUSB_HOST_PL2303 1)

    set(CONFIG_CHERRYUSB_DEVICE_BL 1)
    set(CONFIG_CHERRYUSB_HOST_EHCI_BL 1)
    set(CONFIG_CHERRYUSB_OSAL "freertos")

    include(${CMAKE_CURRENT_LIST_DIR}/cherryusb.cmake)
    list(REMOVE_DUPLICATES cherryusb_srcs)
    list(REMOVE_DUPLICATES cherryusb_incs)

    sdk_generate_library(cherryusb)
    sdk_add_include_directories(${cherryusb_incs})
    sdk_library_add_sources(${cherryusb_srcs})
elseif(ESP_PLATFORM)
    message(STATUS "enable cherryusb in esp-idf")

    set(CONFIG_CHERRYUSB_DEVICE_DWC2_ESP 1)
    set(CONFIG_CHERRYUSB_HOST_DWC2_ESP 1)
    set(CONFIG_CHERRYUSB_OSAL "idf")

    include(${CMAKE_CURRENT_LIST_DIR}/cherryusb.cmake)
    list(REMOVE_DUPLICATES cherryusb_srcs)
    list(REMOVE_DUPLICATES cherryusb_incs)

    set(ldfragments "osal/idf/linker.lf")

    if(CONFIG_CHERRYUSB_HOST_CDC_ECM
       OR CONFIG_CHERRYUSB_HOST_CDC_RNDIS
       OR CONFIG_CHERRYUSB_HOST_CDC_NCM
       OR CONFIG_CHERRYUSB_HOST_ASIX
       OR CONFIG_CHERRYUSB_HOST_RTL8152
       OR CONFIG_CHERRYUSB_HOST_BL616
    )
        idf_component_get_property(lwip lwip COMPONENT_LIB)
        target_compile_definitions(${lwip} PRIVATE "-DPBUF_POOL_BUFSIZE=1600")
    endif()

    idf_component_get_property(freertos_include freertos ORIG_INCLUDE_PATH)

    idf_component_register(
        SRCS
        ${cherryusb_srcs}
        INCLUDE_DIRS
        ${cherryusb_incs}
        ${freertos_include}
        PRIV_REQUIRES
        usb
        LDFRAGMENTS
        ${ldfragments}
    )

    if(CONFIG_CHERRYUSB_HOST)
        target_linker_script(${COMPONENT_LIB} INTERFACE "osal/idf/usbh_class_info.ld")

        # 强制链接器不删除符号
        if(CONFIG_CHERRYUSB_HOST_CDC_ACM)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_acm_class_info")
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_data_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_HID)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u hid_custom_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_MSC)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u msc_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_CDC_ECM)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_ecm_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_CDC_RNDIS)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u rndis_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_CDC_NCM)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cdc_ncm_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_VIDEO)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u video_ctrl_class_info")
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u video_streaming_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_AUDIO)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u audio_ctrl_intf_class_info")
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u audio_streaming_intf_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_BLUETOOTH)
            if(CONFIG_USBHOST_BLUETOOTH_HCI_H4)
                target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bluetooth_h4_nrf_class_info")
            else()
                target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bluetooth_class_info")
            endif()
        endif()
        if(CONFIG_CHERRYUSB_HOST_ASIX)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u asix_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_RTL8152)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u rtl8152_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_FTDI)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ftdi_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_CH34X)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u ch34x_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_CP210X)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u cp210x_class_info")
        endif()
        if(CONFIG_CHERRYUSB_HOST_PL2303)
            target_link_libraries(${COMPONENT_LIB} INTERFACE "-u pl2303_class_info")
        endif()
    endif()

    if(CONFIG_CHERRYUSB)
        set_source_files_properties("class/audio/usbd_audio.c" PROPERTIES COMPILE_FLAGS -Wno-maybe-uninitialized)
    endif()
elseif(ZEPHYR_BASE)
    message(STATUS "enable cherryusb in zephyr")

    set(CONFIG_CHERRYUSB_OSAL "zephyr")
    include(${CMAKE_CURRENT_LIST_DIR}/cherryusb.cmake)
    list(REMOVE_DUPLICATES cherryusb_srcs)
    list(REMOVE_DUPLICATES cherryusb_incs)

    if (CONFIG_SHELL)
        list(APPEND cherryusb_srcs ${CMAKE_CURRENT_LIST_DIR}/platform/zephyr/usb_cmd.c)
    endif ()

    zephyr_library()
    if(cherryusb_incs)
        zephyr_include_directories(${cherryusb_incs})
    endif()
    if(cherryusb_srcs)
        zephyr_library_sources(${cherryusb_srcs})
    endif()

    if (CONFIG_CHERRYUSB_HOST)
        zephyr_linker_sources(SECTIONS zephyr/usbh_class_info.ld)
    endif()
elseif(HPM_SDK_BASE)
    message(STATUS "enable cherryusb in hpm_sdk")

    set(CONFIG_CHERRYUSB_DEVICE_CDC_ACM 1)
    set(CONFIG_CHERRYUSB_DEVICE_HID 1)
    set(CONFIG_CHERRYUSB_DEVICE_MSC 1)
    set(CONFIG_CHERRYUSB_DEVICE_AUDIO 1)
    set(CONFIG_CHERRYUSB_DEVICE_VIDEO 1)

    set(CONFIG_CHERRYUSB_HOST_CDC_ACM 1)
    set(CONFIG_CHERRYUSB_HOST_CDC_ECM 1)
    set(CONFIG_CHERRYUSB_HOST_CDC_NCM 1)
    set(CONFIG_CHERRYUSB_HOST_HID 1)
    set(CONFIG_CHERRYUSB_HOST_MSC 1)
    set(CONFIG_CHERRYUSB_HOST_VIDEO 1)
    set(CONFIG_CHERRYUSB_HOST_AUDIO 1)
    set(CONFIG_CHERRYUSB_HOST_CDC_RNDIS 1)
    # set(CONFIG_CHERRYUSB_HOST_BLUETOOTH 1)
    set(CONFIG_CHERRYUSB_HOST_ASIX 1)
    set(CONFIG_CHERRYUSB_HOST_RTL8152 1)
    set(CONFIG_CHERRYUSB_HOST_CH34X 1)
    set(CONFIG_CHERRYUSB_HOST_CP210X 1)
    set(CONFIG_CHERRYUSB_HOST_FTDI 1)
    set(CONFIG_CHERRYUSB_HOST_PL2303 1)
    set(CONFIG_CHERRYUSB_HOST_BL616 1)

    set(CONFIG_CHERRYUSB_DEVICE_HPM 1)
    set(CONFIG_CHERRYUSB_HOST_EHCI_HPM 1)
    set(CONFIG_CHERRYUSB_OSAL "freertos")

    include(${CMAKE_CURRENT_LIST_DIR}/cherryusb.cmake)
    list(REMOVE_DUPLICATES cherryusb_srcs)
    list(REMOVE_DUPLICATES cherryusb_incs)

    sdk_inc(${cherryusb_incs})
    sdk_src(${cherryusb_srcs})
endif()

endif()