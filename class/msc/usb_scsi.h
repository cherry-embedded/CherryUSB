/**
 * @file
 * @brief USB Mass Storage Class SCSI public header
 *
 * Header follows the Mass Storage Class Specification
 * (Mass_Storage_Specification_Overview_v1.4_2-19-2010.pdf) and
 * Mass Storage Class Bulk-Only Transport Specification
 * (usbmassbulk_10.pdf).
 * Header is limited to Bulk-Only Transfer protocol.
 */

#ifndef _USB_SCSI_H_
#define _USB_SCSI_H_

/* SCSI Commands */
#define SCSI_TEST_UNIT_READY             0x00
#define SCSI_REQUEST_SENSE               0x03
#define SCSI_FORMAT_UNIT                 0x04
#define SCSI_INQUIRY                     0x12
#define SCSI_MODE_SELECT6                0x15
#define SCSI_MODE_SENSE6                 0x1A
#define SCSI_START_STOP_UNIT             0x1B
#define SCSI_SEND_DIAGNOSTIC             0x1D
#define SCSI_PREVENT_ALLOW_MEDIA_REMOVAL 0x1E
#define SCSI_READ_FORMAT_CAPACITIES      0x23
#define SCSI_READ_CAPACITY10             0x25
#define SCSI_READ10                      0x28
#define SCSI_WRITE10                     0x2A
#define SCSI_VERIFY10                    0x2F
#define SCSI_SYNC_CACHE10                0x35
#define SCSI_READ12                      0xA8
#define SCSI_WRITE12                     0xAA
#define SCSI_MODE_SELECT10               0x55
#define SCSI_MODE_SENSE10                0x5A
#define SCSI_ATA_COMMAND_PASS_THROUGH16  0x85
#define SCSI_READ16                      0x88
#define SCSI_WRITE16                     0x8A
#define SCSI_VERIFY16                    0x8F
#define SCSI_SYNC_CACHE16                0x91
#define SCSI_SERVICE_ACTION_IN16         0x9E
#define SCSI_READ_CAPACITY16             0x9E
#define SCSI_SERVICE_ACTION_OUT16        0x9F
#define SCSI_ATA_COMMAND_PASS_THROUGH12  0xA1
#define SCSI_REPORT_ID_INFO              0xA3
#define SCSI_READ12                      0xA8
#define SCSI_SERVICE_ACTION_OUT12        0xA9
#define SCSI_SERVICE_ACTION_IN12         0xAB
#define SCSI_VERIFY12                    0xAF

/* SCSI Sense Key */
#define SCSI_SENSE_NONE            0x00
#define SCSI_SENSE_RECOVERED_ERROR 0x01
#define SCSI_SENSE_NOT_READY       0x02
#define SCSI_SENSE_MEDIUM_ERROR    0x03
#define SCSI_SENSE_HARDWARE_ERROR  0x04
#define SCSI_SENSE_ILLEGAL_REQUEST 0x05
#define SCSI_SENSE_UNIT_ATTENTION  0x06
#define SCSI_SENSE_DATA_PROTECT    0x07
#define SCSI_SENSE_FIRMWARE_ERROR  0x08
#define SCSI_SENSE_ABORTED_COMMAND 0x0b
#define SCSI_SENSE_EQUAL           0x0c
#define SCSI_SENSE_VOLUME_OVERFLOW 0x0d
#define SCSI_SENSE_MISCOMPARE      0x0e

/* Additional Sense Code */
#define SCSI_ASC_INVALID_CDB                     0x20U
#define SCSI_ASC_INVALID_FIELED_IN_COMMAND       0x24U
#define SCSI_ASC_PARAMETER_LIST_LENGTH_ERROR     0x1AU
#define SCSI_ASC_INVALID_FIELD_IN_PARAMETER_LIST 0x26U
#define SCSI_ASC_ADDRESS_OUT_OF_RANGE            0x21U
#define SCSI_ASC_MEDIUM_NOT_PRESENT              0x3AU
#define SCSI_ASC_MEDIUM_HAVE_CHANGED             0x28U
#define SCSI_ASC_WRITE_PROTECTED                 0x27U
#define SCSI_ASC_UNRECOVERED_READ_ERROR          0x11U
#define SCSI_ASC_WRITE_FAULT                     0x03U

#define READ_FORMAT_CAPACITY_DATA_LEN 0x0CU
#define READ_CAPACITY10_DATA_LEN      0x08U
#define REQUEST_SENSE_DATA_LEN        0x12U
#define STANDARD_INQUIRY_DATA_LEN     0x24U
#define MODE_SENSE6_DATA_LEN          0x17U
#define MODE_SENSE10_DATA_LEN         0x1BU

#define SCSI_MEDIUM_STATUS_UNLOCKED 0x00U
#define SCSI_MEDIUM_STATUS_LOCKED   0x01U
#define SCSI_MEDIUM_STATUS_EJECTED  0x02U

#endif /* USB_SCSI_H */
