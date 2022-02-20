/**
 * @file
 * @brief USB TMC Class public header
 *
 */

#ifndef _USB_TMC_H_
#define _USB_TMC_H_

/**@addtogroup MODULE_TMC USB TMC class
 * @brief This module contains USB Device Test and Measurement Class definitions.
 * @details This module based on
 *          [USB Device Test and Measurement Class Specification, Revision 1.0]
 *          (https://www.usb.org/sites/default/files/USBTMC_1_006a.zip)
 * @{*/

/**@name USB TMC class, subclass and protocol definitions
 * @{*/
#define TMC_SUBCLASS_TMC    0x03
#define TMC_PROTOCOL_NONE   0x00 /**< No subclass specification applies. */
#define TMC_PROTOCOL_USB488 0x01 /**< USBTMC USB488 subclass interface. */
/** @}*/

/**@name USBTMC requests
 * @{*/
#define TMC_REQUEST_INITIATE_ABORT_BULK_OUT     1
#define TMC_REQUEST_CHECK_ABORT_BULK_OUT_STATUS 2
#define TMC_REQUEST_INITIATE_ABORT_BULK_IN      3
#define TMC_REQUEST_CHECK_ABORT_BULK_IN_STATUS  4
#define TMC_REQUEST_INITIATE_CLEAR              5
#define TMC_REQUEST_CHECK_CLEAR_STATUS          6
#define TMC_REQUEST_GET_CAPABILITIES            7
#define TMC_REQUEST_INDICATOR_PULSE             64
/**@}*/

/**@name USBTMC status values
 * @{*/
#define TMC_STATUS_SUCCESS                  0x01
#define TMC_STATUS_PENDING                  0x02
#define TMC_STATUS_FAILED                   0x80
#define TMC_STATUS_TRANSFER_NOT_IN_PROGRESS 0x81
#define TMC_STATUS_SPLIT_NOT_IN_PROGRESS    0x82
#define TMC_STATUS_SPLIT_IN_PROGRESS        0x83
/**@}*/

/** GET_CAPABILITIES request response */
struct tmc_get_capabilities_response {
    uint8_t USBTMC_status;
    uint8_t Reserved0;
    uint16_t bcdUSBTMC;
    uint8_t InterfaceCapabilities;
    uint8_t DeviceCapabilities;
    uint8_t Reserved1[18];
} __PACKED;

/**@name MsgId values
 * @{*/
#define TMC_DEV_DEP_MSG_OUT            1
#define TMC_REQUEST_DEV_DEP_MSG_IN     2
#define TMC_DEV_DEP_MSG_IN             2
#define TMC_VENDOR_SPECIFIC_OUT        126
#define TMC_REQUEST_VENDOR_SPECIFIC_IN 127
#define TMC_VENDOR_SPECIFIC_IN         127
/**@}*/

/**@name Transfer Attributes
 * @{*/
/** The last USBTMC message data byte in the transfer is the last byte of the
 * USBTMC message. */
#define TMC_TRANSFER_ATTR_EOM 0x01
/** The Bulk-IN transfer must terminate on the specified TermChar. The Host may
 * only set this bit if the USBTMC interface indicates it supports TermChar in
 * the GET_CAPABILITIES response packet */
#define TMC_TRANSFER_ATTR_TERM_CHAR 0x02
/**@}*/

/** Message specific part of bulk header */
union usb_tmc_bulk_header_specific {
    struct {
        uint32_t TransferSize;
        uint8_t bmTransferAttributes;
        uint8_t Reserved[3];
    } dev_dep_msg_out;

    struct {
        uint32_t TransferSize;
        uint8_t bmTransferAttributes;
        uint8_t TermChar;
        uint8_t Reserved[2];
    } request_dev_dep_msg_in;

    struct {
        uint32_t TransferSize;
        uint8_t bmTransferAttributes;
        uint8_t Reserved[3];
    } dev_dep_msg_in;

    struct {
        uint32_t TransferSize;
        uint8_t Reserved[4];
    } vendor_specific_out;

    struct {
        uint32_t TransferSize;
        uint8_t Reserved[4];
    } request_vendor_specific_in;

    struct {
        uint32_t TransferSize;
        uint8_t Reserved[4];
    } vendor_specific_in;
};

/** Host must begin the first USB transaction in each Bulk transfer of
 * command message content with a Bulk Header. */
struct usb_tmc_bulk_header {
    /** Specifies the USBTMC message and the type of the USBTMC message. */
    uint8_t MsgId;
    /** A transfer identifier. The Host must set bTag different than the
     * bTag used in the previous Bulk-OUT Header. The Host should increment
     * the bTag by 1 each time it sends a new Bulk-OUT Header. */
    uint8_t bTag;
    /** The inverse (one's complement) of the bTag */
    uint8_t bTagInverse;
    uint8_t Reserved;
    /** USBTMC command message specific */
    union usb_tmc_bulk_header_specific MsgSpecific;
} __PACKED;

#endif /* _USB_TMC_H_ */
