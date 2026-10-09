/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_tmc.h"

#define TMC_OUT_EP_IDX 0
#define TMC_IN_EP_IDX  1

/* Describe EndPoints configuration */
static struct usbd_endpoint tmc_ep_data[2];

#ifndef CONFIG_USBDEV_TMC_MAX_BUFSIZE
#define CONFIG_USBDEV_TMC_MAX_BUFSIZE 1024
#endif

USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t tmc_read_buffer[CONFIG_USBDEV_TMC_MAX_BUFSIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t tmc_write_buffer[CONFIG_USBDEV_TMC_MAX_BUFSIZE];

static volatile uint32_t g_tmc_read_offset = 0;
static volatile uint32_t g_tmc_read_total = 0;

static scpi_t scpi_context;

static char scpi_input_buffer[CONFIG_USBDEV_TMC_MAX_BUFSIZE];

#define SCPI_ERROR_QUEUE_SIZE 4
static scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];

static size_t SCPI_Write(scpi_t *context, const char *data, size_t len)
{
    (void)context;

    struct tmc_bulk_header *header = (struct tmc_bulk_header *)tmc_write_buffer;

    USB_ASSERT_MSG((header->msg_specific.dev_dep_msg_in.transferSize + len) <= (CONFIG_USBDEV_TMC_MAX_BUFSIZE - sizeof(struct tmc_bulk_header)), "SCPI_Write: buffer overflow\r\n");

    memcpy(tmc_write_buffer + sizeof(struct tmc_bulk_header) + header->msg_specific.dev_dep_msg_in.transferSize, data, len);
    header->msg_specific.dev_dep_msg_in.transferSize += len;

    return len;
}

static scpi_result_t SCPI_Flush(scpi_t *context)
{
    (void)context;

    return SCPI_RES_OK;
}

static int SCPI_Error(scpi_t *context, int_fast16_t err)
{
    (void)context;

    return SCPI_RES_OK;
}

static scpi_result_t SCPI_Control(scpi_t *context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val)
{
    (void)context;

    return SCPI_RES_OK;
}

static scpi_result_t SCPI_Reset(scpi_t *context)
{
    (void)context;

    return SCPI_RES_OK;
}

static scpi_interface_t scpi_interface = {
    .error = SCPI_Error,
    .write = SCPI_Write,
    .control = SCPI_Control,
    .flush = SCPI_Flush,
    .reset = SCPI_Reset,
};

static int tmc_class_interface_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    USB_LOG_DBG("TMC Class request: "
                "bRequest 0x%02x\r\n",
                setup->bRequest);

    struct tmc_capabilities_response s_tmc_capabilities = {
        .USBTMC_status = TMC_STATUS_SUCCESS,
        .bcdUSBTMC = 0x0100,
        .reserved0 = 0,
        .reserved1 = { 0 },
        .reserved_subclass = { 0 },
        .InterfaceCapabilities = 0,
        .DeviceCapabilities = 0
    };

    switch (setup->bRequest) {
        case TMC_REQUEST_INITIATE_ABORT_BULK_OUT:
            (*data)[0] = TMC_STATUS_SUCCESS;
            *len = 2;
            break;
        case TMC_REQUEST_CHECK_ABORT_BULK_OUT_STATUS:
            (*data)[0] = TMC_STATUS_SUCCESS;
            (*data)[1] = 0; // bmAbortBulkIn
            (*data)[2] = 0; // reserved
            (*data)[3] = 0; // reserved
            *len = 4;
            break;
        case TMC_REQUEST_INITIATE_ABORT_BULK_IN:
            (*data)[0] = TMC_STATUS_SUCCESS;
            *len = 2;
            break;
        case TMC_REQUEST_CHECK_ABORT_BULK_IN_STATUS:
            (*data)[0] = TMC_STATUS_SUCCESS;
            (*data)[1] = 0; // bmAbortBulkIn
            (*data)[2] = 0; // reserved
            (*data)[3] = 0; // reserved
            *len = 4;
            break;
        case TMC_REQUEST_INITIATE_CLEAR:
            (*data)[0] = TMC_STATUS_SUCCESS;
            *len = 1;
            break;
        case TMC_REQUEST_CHECK_CLEAR_STATUS:
            (*data)[0] = TMC_STATUS_SUCCESS;
            (*data)[1] = 0;
            *len = 2;
            break;
        case TMC_REQUEST_GET_CAPABILITIES:
            memcpy(*data, &s_tmc_capabilities, sizeof(s_tmc_capabilities));
            *len = sizeof(s_tmc_capabilities);
            break;
        case TMC_REQUEST_INDICATOR_PULSE:
            (*data)[0] = TMC_STATUS_SUCCESS;
            *len = 1;
            break;
        default:
            USB_LOG_WRN("Unhandled TMC Class bRequest 0x%02x\r\n", setup->bRequest);
            return -1;
    }

    return 0;
}

void tmc_notify_handler(uint8_t busid, uint8_t event, void *arg)
{
    (void)arg;

    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONFIGURED:
            g_tmc_read_offset = 0;
            g_tmc_read_total = 0;
            usbd_ep_start_read(busid, tmc_ep_data[TMC_OUT_EP_IDX].ep_addr, tmc_read_buffer + g_tmc_read_offset, usbd_get_ep_mps(busid, tmc_ep_data[TMC_OUT_EP_IDX].ep_addr));
            break;

        default:
            break;
    }
}

void tmc_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    struct tmc_bulk_header *out_header = (struct tmc_bulk_header *)tmc_read_buffer;
    struct tmc_bulk_header *in_header = (struct tmc_bulk_header *)tmc_write_buffer;

    g_tmc_read_offset += nbytes;

    if (g_tmc_read_total == 0) {
        USB_ASSERT_MSG(nbytes >= sizeof(struct tmc_bulk_header), "tmc header size error\r\n");

        switch (out_header->MsgID) {
            case TMC_MSGID_OUT_DEV_DEP_MSG_OUT:
                g_tmc_read_total = ((out_header->msg_specific.dev_dep_msg_out.transferSize + 3u) & ~3u) + sizeof(struct tmc_bulk_header);
                break;
            case TMC_MSGID_OUT_REQUEST_DEV_DEP_MSG_IN:
                g_tmc_read_total = sizeof(struct tmc_bulk_header);
                break;
            default:
                USB_LOG_WRN("Unhandled TMC MsgID 0x%02x\r\n", out_header->MsgID);
                break;
        }
        USB_ASSERT_MSG(g_tmc_read_total <= CONFIG_USBDEV_TMC_MAX_BUFSIZE, "tmc read buffer overflow\r\n");
    }

    if (g_tmc_read_offset < g_tmc_read_total) {
        usbd_ep_start_read(busid, tmc_ep_data[TMC_OUT_EP_IDX].ep_addr, tmc_read_buffer + g_tmc_read_offset, usbd_get_ep_mps(busid, tmc_ep_data[TMC_OUT_EP_IDX].ep_addr));
        return;
    }

    switch (out_header->MsgID) {
        case TMC_MSGID_OUT_DEV_DEP_MSG_OUT:
            SCPI_Input(&scpi_context,
                       (const char *)(tmc_read_buffer + sizeof(struct tmc_bulk_header)),
                       g_tmc_read_total);
            break;
        case TMC_MSGID_OUT_REQUEST_DEV_DEP_MSG_IN:

            in_header->MsgID = TMC_MSGID_IN_DEV_DEP_MSG_IN;
            in_header->bTag = out_header->bTag;
            in_header->bTagInverse = out_header->bTagInverse;
            in_header->reserved = 0;
            in_header->msg_specific.dev_dep_msg_in.bmTransferAttributes = TMC_BULK_HEADER_TRANSFER_ATTR_TERMCHAR;

            usbd_ep_start_write(busid, tmc_ep_data[TMC_IN_EP_IDX].ep_addr, tmc_write_buffer, sizeof(struct tmc_bulk_header) + in_header->msg_specific.dev_dep_msg_in.transferSize);
            break;
        default:
            USB_LOG_WRN("Unhandled TMC MsgID 0x%02x\r\n", out_header->MsgID);
            break;
    }

    g_tmc_read_offset = 0;
    g_tmc_read_total = 0;
    usbd_ep_start_read(busid, tmc_ep_data[TMC_OUT_EP_IDX].ep_addr, tmc_read_buffer + g_tmc_read_offset, usbd_get_ep_mps(busid, tmc_ep_data[TMC_OUT_EP_IDX].ep_addr));
}

void tmc_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    if ((nbytes % usbd_get_ep_mps(busid, ep)) == 0 && nbytes) {
        /* send zlp */
        usbd_ep_start_write(busid, ep, NULL, 0);
    } else {
    }
}

struct usbd_interface *usbd_tmc_init_intf(uint8_t busid,
                                          struct usbd_interface *intf,
                                          const uint8_t out_ep, const uint8_t in_ep,
                                          const scpi_command_t *scpi_commands,
                                          const char **idn)
{
    (void)busid;

    intf->class_interface_handler = tmc_class_interface_request_handler;
    intf->class_endpoint_handler = NULL;
    intf->vendor_handler = NULL;
    intf->notify_handler = tmc_notify_handler;

    tmc_ep_data[TMC_OUT_EP_IDX].ep_addr = out_ep;
    tmc_ep_data[TMC_OUT_EP_IDX].ep_cb = tmc_bulk_out;
    tmc_ep_data[TMC_IN_EP_IDX].ep_addr = in_ep;
    tmc_ep_data[TMC_IN_EP_IDX].ep_cb = tmc_bulk_in;

    usbd_add_endpoint(busid, &tmc_ep_data[TMC_OUT_EP_IDX]);
    usbd_add_endpoint(busid, &tmc_ep_data[TMC_IN_EP_IDX]);

    SCPI_Init(&scpi_context,
              scpi_commands,
              &scpi_interface,
              scpi_units_def,
              idn[0], idn[1], idn[2], idn[3],
              scpi_input_buffer, CONFIG_USBDEV_TMC_MAX_BUFSIZE,
              scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);

    return intf;
}