/*
 * Copyright (c) 2022, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_dfu.h"

/** Modify the following three parameters according to different platforms */
#ifndef USBD_DFU_XFER_SIZE
#define USBD_DFU_XFER_SIZE 1024
#endif

#ifndef USBD_DFU_APP_DEFAULT_ADD
#define USBD_DFU_APP_DEFAULT_ADD 0x8004000
#endif

#ifndef FLASH_PROGRAM_TIME
#define FLASH_PROGRAM_TIME 50
#endif

#ifndef FLASH_ERASE_TIME
#define FLASH_ERASE_TIME 50
#endif

struct dfu_cfg_priv {
    struct dfu_info info;
    union {
        uint32_t d32[USBD_DFU_XFER_SIZE / 4U];
        uint8_t d8[USBD_DFU_XFER_SIZE];
    } buffer;

    uint32_t wblock_num;
    uint32_t wlength;
    uint32_t data_ptr;
    uint32_t alt_setting;

    uint8_t dev_status[6];
    uint8_t ReservedForAlign[2];
    uint8_t dev_state;
    uint8_t manif_state;
    uint8_t firmwar_flag;
} usbd_dfu_cfg;

static void dfu_reset(void)
{
    memset(&usbd_dfu_cfg, 0, sizeof(usbd_dfu_cfg));

    usbd_dfu_cfg.alt_setting = 0U;
    usbd_dfu_cfg.data_ptr = USBD_DFU_APP_DEFAULT_ADD;
    usbd_dfu_cfg.wblock_num = 0U;
    usbd_dfu_cfg.wlength = 0U;

    usbd_dfu_cfg.manif_state = DFU_MANIFEST_COMPLETE;
    usbd_dfu_cfg.dev_state = DFU_STATE_DFU_IDLE;

    usbd_dfu_cfg.dev_status[0] = DFU_STATUS_OK;
    usbd_dfu_cfg.dev_status[1] = 0U;
    usbd_dfu_cfg.dev_status[2] = 0U;
    usbd_dfu_cfg.dev_status[3] = 0U;
    usbd_dfu_cfg.dev_status[4] = DFU_STATE_DFU_IDLE;
    usbd_dfu_cfg.dev_status[5] = 0U;
}

static uint16_t dfu_getstatus(uint32_t add, uint8_t cmd, uint8_t *buffer)
{
    switch (cmd) {
        case DFU_MEDIA_PROGRAM:
            buffer[1] = (uint8_t)FLASH_PROGRAM_TIME;
            buffer[2] = (uint8_t)(FLASH_PROGRAM_TIME << 8);
            buffer[3] = 0;
            break;

        case DFU_MEDIA_ERASE:
            buffer[1] = (uint8_t)FLASH_ERASE_TIME;
            buffer[2] = (uint8_t)(FLASH_ERASE_TIME << 8);
            buffer[3] = 0;
        default:

            break;
    }
    return (0);
}

static void dfu_request_detach(void)
{
    if ((usbd_dfu_cfg.dev_state == DFU_STATE_DFU_IDLE) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_SYNC) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_IDLE) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_MANIFEST_SYNC) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_UPLOAD_IDLE)) {
        /* Update the state machine */
        usbd_dfu_cfg.dev_state = DFU_STATE_DFU_IDLE;
        usbd_dfu_cfg.dev_status[0] = DFU_STATUS_OK;
        usbd_dfu_cfg.dev_status[1] = 0U;
        usbd_dfu_cfg.dev_status[2] = 0U;
        usbd_dfu_cfg.dev_status[3] = 0U; /*bwPollTimeout=0ms*/
        usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
        usbd_dfu_cfg.dev_status[5] = 0U; /*iString*/
        usbd_dfu_cfg.wblock_num = 0U;
        usbd_dfu_cfg.wlength = 0U;
    }
}

static void dfu_request_upload(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    struct usb_setup_packet *req = setup;
    uint32_t addr;
    uint8_t *phaddr;
    /* Data setup request */
    if (req->wLength > 0U) {
        if ((usbd_dfu_cfg.dev_state == DFU_STATE_DFU_IDLE) || (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_UPLOAD_IDLE)) {
            /* Update the global length and block number */
            usbd_dfu_cfg.wblock_num = req->wValue;
            usbd_dfu_cfg.wlength = MIN(req->wLength, USBD_DFU_XFER_SIZE);

            /* DFU Get Command */
            if (usbd_dfu_cfg.wblock_num == 0U) {
                /* Update the state machine */
                usbd_dfu_cfg.dev_state = (usbd_dfu_cfg.wlength > 3U) ? DFU_STATE_DFU_IDLE : DFU_STATE_DFU_UPLOAD_IDLE;

                usbd_dfu_cfg.dev_status[1] = 0U;
                usbd_dfu_cfg.dev_status[2] = 0U;
                usbd_dfu_cfg.dev_status[3] = 0U;
                usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;

                /* Store the values of all supported commands */
                usbd_dfu_cfg.buffer.d8[0] = DFU_CMD_GETCOMMANDS;
                usbd_dfu_cfg.buffer.d8[1] = DFU_CMD_SETADDRESSPOINTER;
                usbd_dfu_cfg.buffer.d8[2] = DFU_CMD_ERASE;

                /* Send the status data over EP0 */
                *data = usbd_dfu_cfg.buffer.d8;
                *len = 3;
            } else if (usbd_dfu_cfg.wblock_num > 1U) {
                usbd_dfu_cfg.dev_state = DFU_STATE_DFU_UPLOAD_IDLE;

                usbd_dfu_cfg.dev_status[1] = 0U;
                usbd_dfu_cfg.dev_status[2] = 0U;
                usbd_dfu_cfg.dev_status[3] = 0U;
                usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;

                addr = ((usbd_dfu_cfg.wblock_num - 2U) * USBD_DFU_XFER_SIZE) + usbd_dfu_cfg.data_ptr;

                /* Return the physical address where data are stored */
                phaddr = dfu_read_flash((uint8_t *)addr, usbd_dfu_cfg.buffer.d8, usbd_dfu_cfg.wlength);

                /* Send the status data over EP0 */
                *data = usbd_dfu_cfg.buffer.d8;
                *len = usbd_dfu_cfg.wlength;
            } else /* unsupported usbd_dfu_cfg.wblock_num */
            {
                usbd_dfu_cfg.dev_state = DFU_STATUS_ERR_STALLEDPKT;

                usbd_dfu_cfg.dev_status[1] = 0U;
                usbd_dfu_cfg.dev_status[2] = 0U;
                usbd_dfu_cfg.dev_status[3] = 0U;
                usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;

                /* Call the error management function (command will be NAKed */
                USB_LOG_ERR("Dfu_request_upload unsupported usbd_dfu_cfg.wblock_num\r\n");
            }
        }
        /* Unsupported state */
        else {
            usbd_dfu_cfg.wlength = 0U;
            usbd_dfu_cfg.wblock_num = 0U;

            /* Call the error management function (command will be NAKed */
            USB_LOG_ERR("Dfu_request_upload unsupported state\r\n");
        }
    }
    /* No Data setup request */
    else {
        usbd_dfu_cfg.dev_state = DFU_STATE_DFU_IDLE;

        usbd_dfu_cfg.dev_status[1] = 0U;
        usbd_dfu_cfg.dev_status[2] = 0U;
        usbd_dfu_cfg.dev_status[3] = 0U;
        usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
    }
}

static void dfu_request_dnload(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    /* Data setup request */
    struct usb_setup_packet *req = setup;
    if (req->wLength > 0U) {
        if ((usbd_dfu_cfg.dev_state == DFU_STATE_DFU_IDLE) || (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_IDLE)) {
            /* Update the global length and block number */
            usbd_dfu_cfg.wblock_num = req->wValue;
            usbd_dfu_cfg.wlength = MIN(req->wLength, USBD_DFU_XFER_SIZE);

            /* Update the state machine */
            usbd_dfu_cfg.dev_state = DFU_STATE_DFU_DNLOAD_SYNC;
            usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;

            /*!< Data has received complete */
            memcpy((uint8_t *)usbd_dfu_cfg.buffer.d8, (uint8_t *)*data, usbd_dfu_cfg.wlength);
            /*!< Set flag = 1 Write the firmware to the flash in the next dfu_request_getstatus */
            usbd_dfu_cfg.firmwar_flag = 1;
        }
        /* Unsupported state */
        else {
            USB_LOG_ERR("Dfu_request_dnload unsupported state\r\n");
        }
    }
    /* 0 Data DNLOAD request */
    else {
        /* End of DNLOAD operation*/
        if ((usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_IDLE) || (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_IDLE)) {
            usbd_dfu_cfg.manif_state = DFU_MANIFEST_IN_PROGRESS;
            usbd_dfu_cfg.dev_state = DFU_STATE_DFU_MANIFEST_SYNC;
            usbd_dfu_cfg.dev_status[1] = 0U;
            usbd_dfu_cfg.dev_status[2] = 0U;
            usbd_dfu_cfg.dev_status[3] = 0U;
            usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
        } else {
            /* Call the error management function (command will be NAKed */
            USB_LOG_ERR("Dfu_request_dnload End of DNLOAD operation but dev_state %02x \r\n", usbd_dfu_cfg.dev_state);
        }
    }
}

static int8_t dfu_getstatus_special_handler(void)
{
    uint32_t addr;
    if (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_BUSY) {
        /* Decode the Special Command */
        if (usbd_dfu_cfg.wblock_num == 0U) {
            if (usbd_dfu_cfg.wlength == 1U) {
                if (usbd_dfu_cfg.buffer.d8[0] == DFU_CMD_GETCOMMANDS) {
                    /* Nothing to do */
                }
            } else if (usbd_dfu_cfg.wlength == 5U) {
                if (usbd_dfu_cfg.buffer.d8[0] == DFU_CMD_SETADDRESSPOINTER) {
                    usbd_dfu_cfg.data_ptr = usbd_dfu_cfg.buffer.d8[1];
                    usbd_dfu_cfg.data_ptr += (uint32_t)usbd_dfu_cfg.buffer.d8[2] << 8;
                    usbd_dfu_cfg.data_ptr += (uint32_t)usbd_dfu_cfg.buffer.d8[3] << 16;
                    usbd_dfu_cfg.data_ptr += (uint32_t)usbd_dfu_cfg.buffer.d8[4] << 24;
                } else if (usbd_dfu_cfg.buffer.d8[0] == DFU_CMD_ERASE) {
                    usbd_dfu_cfg.data_ptr = usbd_dfu_cfg.buffer.d8[1];
                    usbd_dfu_cfg.data_ptr += (uint32_t)usbd_dfu_cfg.buffer.d8[2] << 8;
                    usbd_dfu_cfg.data_ptr += (uint32_t)usbd_dfu_cfg.buffer.d8[3] << 16;
                    usbd_dfu_cfg.data_ptr += (uint32_t)usbd_dfu_cfg.buffer.d8[4] << 24;

                    USB_LOG_DBG("Erase start add %08x \r\n", usbd_dfu_cfg.data_ptr);
                    /*!< Erase */
                    dfu_erase_flash(usbd_dfu_cfg.data_ptr);
                } else {
                    return -1;
                }
            } else {
                /* Reset the global length and block number */
                usbd_dfu_cfg.wlength = 0U;
                usbd_dfu_cfg.wblock_num = 0U;
                /* Call the error management function (command will be NAKed) */
                USB_LOG_ERR("Reset the global length and block number\r\n");
            }
        }
        /* Regular Download Command */
        else {
            if (usbd_dfu_cfg.wblock_num > 1U) {
                /* Decode the required address */
                addr = ((usbd_dfu_cfg.wblock_num - 2U) * USBD_DFU_XFER_SIZE) + usbd_dfu_cfg.data_ptr;

                /* Perform the write operation */
                /* Write flash */
                USB_LOG_DBG("Write start add %08x length %d\r\n", addr, usbd_dfu_cfg.wlength);
                dfu_write_flash(usbd_dfu_cfg.buffer.d8, (uint8_t *)addr, usbd_dfu_cfg.wlength);
            }
        }

        /* Reset the global length and block number */
        usbd_dfu_cfg.wlength = 0U;
        usbd_dfu_cfg.wblock_num = 0U;

        /* Update the state machine */
        usbd_dfu_cfg.dev_state = DFU_STATE_DFU_DNLOAD_SYNC;

        usbd_dfu_cfg.dev_status[1] = 0U;
        usbd_dfu_cfg.dev_status[2] = 0U;
        usbd_dfu_cfg.dev_status[3] = 0U;
        usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
    }
    return 0;
}

static void dfu_request_getstatus(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    /*!< Determine whether to leave DFU mode */
    if (usbd_dfu_cfg.manif_state == DFU_MANIFEST_IN_PROGRESS &&
        usbd_dfu_cfg.dev_state == DFU_STATE_DFU_MANIFEST_SYNC &&
        usbd_dfu_cfg.dev_status[1] == 0U &&
        usbd_dfu_cfg.dev_status[2] == 0U &&
        usbd_dfu_cfg.dev_status[3] == 0U &&
        usbd_dfu_cfg.dev_status[4] == usbd_dfu_cfg.dev_state) {
        usbd_dfu_cfg.manif_state = DFU_MANIFEST_COMPLETE;

        if ((0x0B & DFU_MANIFEST_MASK) != 0U) {
            usbd_dfu_cfg.dev_state = DFU_STATE_DFU_MANIFEST_SYNC;

            usbd_dfu_cfg.dev_status[1] = 0U;
            usbd_dfu_cfg.dev_status[2] = 0U;
            usbd_dfu_cfg.dev_status[3] = 0U;
            usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
            return;
        } else {
            usbd_dfu_cfg.dev_state = DFU_STATE_DFU_MANIFEST_WAIT_RESET;

            usbd_dfu_cfg.dev_status[1] = 0U;
            usbd_dfu_cfg.dev_status[2] = 0U;
            usbd_dfu_cfg.dev_status[3] = 0U;
            usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
            /* Generate system reset to allow jumping to the user code */
            dfu_leave();
        }
    }

    switch (usbd_dfu_cfg.dev_state) {
        case DFU_STATE_DFU_DNLOAD_SYNC:
            if (usbd_dfu_cfg.wlength != 0U) {
                usbd_dfu_cfg.dev_state = DFU_STATE_DFU_DNLOAD_BUSY;

                usbd_dfu_cfg.dev_status[1] = 0U;
                usbd_dfu_cfg.dev_status[2] = 0U;
                usbd_dfu_cfg.dev_status[3] = 0U;
                usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;

                if ((usbd_dfu_cfg.wblock_num == 0U) && (usbd_dfu_cfg.buffer.d8[0] == DFU_CMD_ERASE)) {
                    dfu_getstatus(usbd_dfu_cfg.data_ptr, DFU_MEDIA_ERASE, usbd_dfu_cfg.dev_status);
                } else {
                    dfu_getstatus(usbd_dfu_cfg.data_ptr, DFU_MEDIA_PROGRAM, usbd_dfu_cfg.dev_status);
                }
            } else /* (usbd_dfu_cfg.wlength==0)*/
            {
                usbd_dfu_cfg.dev_state = DFU_STATE_DFU_DNLOAD_IDLE;

                usbd_dfu_cfg.dev_status[1] = 0U;
                usbd_dfu_cfg.dev_status[2] = 0U;
                usbd_dfu_cfg.dev_status[3] = 0U;
                usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
            }
            break;

        case DFU_STATE_DFU_MANIFEST_SYNC:
            if (usbd_dfu_cfg.manif_state == DFU_MANIFEST_IN_PROGRESS) {
                usbd_dfu_cfg.dev_state = DFU_STATE_DFU_MANIFEST;

                usbd_dfu_cfg.dev_status[1] = 1U; /*bwPollTimeout = 1ms*/
                usbd_dfu_cfg.dev_status[2] = 0U;
                usbd_dfu_cfg.dev_status[3] = 0U;
                usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
            } else {
                if ((usbd_dfu_cfg.manif_state == DFU_MANIFEST_COMPLETE) &&
                    ((0x0B & DFU_MANIFEST_MASK) != 0U)) {
                    usbd_dfu_cfg.dev_state = DFU_STATE_DFU_IDLE;

                    usbd_dfu_cfg.dev_status[1] = 0U;
                    usbd_dfu_cfg.dev_status[2] = 0U;
                    usbd_dfu_cfg.dev_status[3] = 0U;
                    usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
                }
            }
            break;

        default:
            break;
    }

    /* Send the status data over EP0 */
    uint8_t temp_data[6];
    memcpy(temp_data, usbd_dfu_cfg.dev_status, 6);
    *data = temp_data;
    *len = 6;

    if (usbd_dfu_cfg.firmwar_flag == 1) {
        if (dfu_getstatus_special_handler() != 0) {
            USB_LOG_ERR("dfu_getstatus_special_handler error \r\n");
        }
        usbd_dfu_cfg.firmwar_flag = 0;
    }
}

static void dfu_request_clrstatus(void)
{
    if (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_ERROR) {
        usbd_dfu_cfg.dev_state = DFU_STATE_DFU_IDLE;
        usbd_dfu_cfg.dev_status[0] = DFU_STATUS_OK; /* bStatus */
        usbd_dfu_cfg.dev_status[1] = 0U;
        usbd_dfu_cfg.dev_status[2] = 0U;
        usbd_dfu_cfg.dev_status[3] = 0U;                     /* bwPollTimeout=0ms */
        usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state; /* bState */
        usbd_dfu_cfg.dev_status[5] = 0U;                     /* iString */
    } else {
        /* State Error */
        usbd_dfu_cfg.dev_state = DFU_STATE_DFU_ERROR;
        usbd_dfu_cfg.dev_status[0] = DFU_STATUS_ERR_UNKNOWN; /* bStatus */
        usbd_dfu_cfg.dev_status[1] = 0U;
        usbd_dfu_cfg.dev_status[2] = 0U;
        usbd_dfu_cfg.dev_status[3] = 0U;                     /* bwPollTimeout=0ms */
        usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state; /* bState */
        usbd_dfu_cfg.dev_status[5] = 0U;                     /* iString */
    }
}

static void dfu_request_getstate(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    /* Return the current state of the DFU interface */
    *data = &usbd_dfu_cfg.dev_state;
    *len = 1;
}

void dfu_request_abort(void)
{
    if ((usbd_dfu_cfg.dev_state == DFU_STATE_DFU_IDLE) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_SYNC) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_DNLOAD_IDLE) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_MANIFEST_SYNC) ||
        (usbd_dfu_cfg.dev_state == DFU_STATE_DFU_UPLOAD_IDLE)) {
        usbd_dfu_cfg.dev_state = DFU_STATE_DFU_IDLE;
        usbd_dfu_cfg.dev_status[0] = DFU_STATUS_OK;
        usbd_dfu_cfg.dev_status[1] = 0U;
        usbd_dfu_cfg.dev_status[2] = 0U;
        usbd_dfu_cfg.dev_status[3] = 0U; /* bwPollTimeout=0ms */
        usbd_dfu_cfg.dev_status[4] = usbd_dfu_cfg.dev_state;
        usbd_dfu_cfg.dev_status[5] = 0U; /* iString */
        usbd_dfu_cfg.wblock_num = 0U;
        usbd_dfu_cfg.wlength = 0U;
    }
}

static int dfu_class_interface_request_handler(struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("DFU Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    switch (setup->bRequest) {
        case DFU_REQUEST_DETACH:
            dfu_request_detach();
            break;
        case DFU_REQUEST_DNLOAD:
            dfu_request_dnload(setup, data, len);
            break;
        case DFU_REQUEST_UPLOAD:
            dfu_request_upload(setup, data, len);
            break;
        case DFU_REQUEST_GETSTATUS:
            dfu_request_getstatus(setup, data, len);
            break;
        case DFU_REQUEST_CLRSTATUS:
            dfu_request_clrstatus();
            break;
        case DFU_REQUEST_GETSTATE:
            dfu_request_getstate(setup, data, len);
            break;
        case DFU_REQUEST_ABORT:
            dfu_request_abort();
            break;
        default:
            USB_LOG_WRN("Unhandled DFU Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

static void dfu_notify_handler(uint8_t event, void *arg)
{
    switch (event) {
        case USBD_EVENT_RESET:
            dfu_reset();
            break;
        default:
            break;
    }
}

struct usbd_interface *usbd_dfu_init_intf(struct usbd_interface *intf)
{
    intf->class_interface_handler = dfu_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = dfu_notify_handler;

    return intf;
}

__WEAK uint8_t *dfu_read_flash(uint8_t *src, uint8_t *dest, uint32_t len)
{
    return dest;
}

__WEAK uint16_t dfu_write_flash(uint8_t *src, uint8_t *dest, uint32_t len)
{
    return 0;
}

__WEAK uint16_t dfu_erase_flash(uint32_t add)
{
    return 0;
}

__WEAK void dfu_leave(void)
{
}
