SECTION_DATA_PROLOGUE(usbh_class_info,,)
{
    . = ALIGN(4);
    __usbh_class_info_start__ = .;
    KEEP(*(.usbh_class_info))
    __usbh_class_info_end__ = .;
} GROUP_LINK_IN(ROMABLE_REGION)