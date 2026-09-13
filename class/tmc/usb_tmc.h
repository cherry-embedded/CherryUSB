/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USB_TMC_H
#define USB_TMC_H

#define TMC_VERSION 0x0100

/* Table 43: TMC Class Code */
#define TMC_CLASS_APPLICATION        0xfe
#define TMC_APPLICATION_SUBCLASS_TMC 0x03

/* Table 44 */
#define TMC_PROTOCOL_NONE   0
#define TMC_PROTOCOL_USB488 1

/* USB TMC Class Specific Requests, Table 15 sec 4.2.1 */
#define TMC_REQUEST_INITIATE_ABORT_BULK_OUT     1
#define TMC_REQUEST_CHECK_ABORT_BULK_OUT_STATUS 2
#define TMC_REQUEST_INITIATE_ABORT_BULK_IN      3
#define TMC_REQUEST_CHECK_ABORT_BULK_IN_STATUS  4
#define TMC_REQUEST_INITIATE_CLEAR              5
#define TMC_REQUEST_CHECK_CLEAR_STATUS          6
#define TMC_REQUEST_GET_CAPABILITIES            7
#define TMC_REQUEST_INDICATOR_PULSE             64 //optional

/* USB TMC status values Table 16 */
#define TMC_STATUS_SUCCESS                  1
#define TMC_STATUS_PENDING                  2
#define TMC_STATUS_FAILED                   0x80
#define TMC_STATUS_TRANSFER_NOT_IN_PROGRESS 0x81
#define TMC_STATUS_SPLIT_NOT_IN_PROGRESS    0x82
#define TMC_STATUS_SPLIT_IN_PROGRESS        0x83

struct tmc_bulk_header {
    uint8_t MsgID;
    uint8_t bTag;
    uint8_t bTagInverse;
    uint8_t reserved;

    union {
        struct _dev_dep_msg_out {
            uint32_t transferSize;
            uint8_t bmTransferAttributes;
            uint8_t reserved[3];
        } dev_dep_msg_out;

        struct _req_dev_dep_msg_in {
            uint32_t transferSize;
            uint8_t bmTransferAttributes;
            uint8_t TermChar;
            uint8_t reserved[2];
        } req_dev_dep_msg_in;

        struct _dev_dep_msg_in {
            uint32_t transferSize;
            uint8_t bmTransferAttributes;
            uint8_t reserved[3];
        } dev_dep_msg_in;

        struct _vendor_specific_out {
            uint32_t transferSize;
            uint8_t reserved[4];
        } vendor_specific_out;

        struct _req_vendor_specific_in {
            uint32_t transferSize;
            uint8_t reserved[4];
        } req_vendor_specific_in;

        struct _vendor_specific_in {
            uint32_t transferSize;
            uint8_t reserved[4];
        } vendor_specific_in;

        uint8_t data[8];
    } msg_specific;
} __attribute__((packed));

/* Table 2, MsgId values */
#define TMC_MSGID_OUT_DEV_DEP_MSG_OUT        1
#define TMC_MSGID_OUT_REQUEST_DEV_DEP_MSG_IN 2
#define TMC_MSGID_IN_DEV_DEP_MSG_IN          2
/* 3-125 Reserved for USBTMC */
#define TMC_MSGID_OUT_VENDOR_SPECIFIC_OUT        126
#define TMC_MSGID_OUT_REQUEST_VENDOR_SPECIFIC_IN 127
#define TMC_MSGID_IN_VENDOR_SPECIFIC_IN          127
/* 128-255 Reserved for USBTMC subclass and VISA */

/** The last USBTMC message data byte in the transfer is the last byte of the
 * USBTMC message. */
#define TMC_BULK_HEADER_TRANSFER_ATTR_EOM (1 << 0)
/** The Bulk-IN transfer must terminate on the specified TermChar. The Host may
 * only set this bit if the USBTMC interface indicates it supports TermChar in
 * the GET_CAPABILITIES response packet */
#define TMC_BULK_HEADER_TRANSFER_ATTR_TERMCHAR (1 << 1)

#define TMC_INTERFACE_CAPABILITY_INDICATOR_PULSE (1 << 2)
#define TMC_INTERFACE_CAPABILITY_TALK_ONLY       (1 << 1)
#define TMC_INTERFACE_CAPABILITY_LISTEN_ONLY     (1 << 0)
#define TMC_DEVICE_CAPABILITY_TERMCHAR           (1 << 0)

struct tmc_capabilities_response {
    uint8_t USBTMC_status;
    uint8_t reserved0;
    uint16_t bcdUSBTMC;
    uint8_t InterfaceCapabilities;
    uint8_t DeviceCapabilities;
    uint8_t reserved1[6];
    uint16_t bcdUSB488;
    uint8_t InterfaceCapabilities488;
    uint8_t DeviceCapabilities488;
    uint8_t reserved_subclass[8];
} __attribute__((packed));

struct tmc_initiate_abort_response {
    uint8_t USBTMC_status;
    uint8_t bTag;
} __attribute__((packed));

struct tmc_check_abort_bulk_response {
    uint8_t USBTMC_status;
    uint8_t bmAbortBulkIn; // D0: BulkInFifoBytes/BulkOutFifoBytes
    uint8_t reserved[2];
    uint32_t NBYTES_RXD_TXD;
} __attribute__((packed));

struct tmc_check_clear_status_response {
    uint8_t USBTMC_status;
    uint8_t bmClear; // D0: BulkInFifoBytes/BulkOutFifoBytes
} __attribute__((packed));

// clang-format off
#define TMC_DESCRIPTOR_INIT(bInterfaceNumber, out_ep, in_ep, wMaxPacketSize, str_idx)       \
        USB_INTERFACE_DESCRIPTOR_INIT(bInterfaceNumber, 0x00, 0x02, 0xFE, 0x03, 0x00, str_idx), \
        USB_ENDPOINT_DESCRIPTOR_INIT(out_ep, USB_ENDPOINT_TYPE_BULK, wMaxPacketSize, 0x00), \
        USB_ENDPOINT_DESCRIPTOR_INIT(in_ep, USB_ENDPOINT_TYPE_BULK, wMaxPacketSize, 0x00)
// clang-format on

// clang-format off
#define TMC488_DESCRIPTOR_INIT(bInterfaceNumber, out_ep, in_ep, wMaxPacketSize, str_idx)       \
        USB_INTERFACE_DESCRIPTOR_INIT(bInterfaceNumber, 0x00, 0x02, 0xFE, 0x03, 0x01, str_idx), \
        USB_ENDPOINT_DESCRIPTOR_INIT(out_ep, USB_ENDPOINT_TYPE_BULK, wMaxPacketSize, 0x00), \
        USB_ENDPOINT_DESCRIPTOR_INIT(in_ep, USB_ENDPOINT_TYPE_BULK, wMaxPacketSize, 0x00)
// clang-format on

#endif /* USB_TMC_H */