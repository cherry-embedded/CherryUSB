/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "usbd_core.h"
#include "usbd_tmc.h"

/*!< endpoint address */
#define CDC_IN_EP  0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define USBD_VID           0xFFFF
#define USBD_PID           0xFFFF
#define USBD_MAX_POWER     100
#define USBD_LANGID_STRING 1033

/*!< config descriptor size */
#define USB_CONFIG_SIZE (9 + (9 + 7 + 7))

static const uint8_t device_descriptor[] = {
    USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0x00, 0x00, 0x00, USBD_VID, USBD_PID, 0x0100, 0x01)
};

static const uint8_t config_descriptor_hs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    TMC_DESCRIPTOR_INIT(0x00, CDC_OUT_EP, CDC_IN_EP, USB_BULK_EP_MPS_HS, 0X00),
};

static const uint8_t config_descriptor_fs[] = {
    USB_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    TMC_DESCRIPTOR_INIT(0x00, CDC_OUT_EP, CDC_IN_EP, USB_BULK_EP_MPS_FS, 0X00),
};

static const uint8_t device_quality_descriptor[] = {
    USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, 0x01),
};

static const uint8_t other_speed_config_descriptor_hs[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    TMC_DESCRIPTOR_INIT(0x00, CDC_OUT_EP, CDC_IN_EP, USB_BULK_EP_MPS_FS, 0X00),
};

static const uint8_t other_speed_config_descriptor_fs[] = {
    USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(USB_CONFIG_SIZE, 0x01, 0x01, USB_CONFIG_BUS_POWERED, USBD_MAX_POWER),
    TMC_DESCRIPTOR_INIT(0x00, CDC_OUT_EP, CDC_IN_EP, USB_BULK_EP_MPS_HS, 0X00),
};

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    "CherryUSB",                  /* Manufacturer */
    "CherryUSB TMC DEMO",         /* Product */
    "2024051702",                 /* Serial Number */
};

static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    (void)speed;

    return device_descriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
        return config_descriptor_hs;
    } else if (speed == USB_SPEED_FULL) {
        return config_descriptor_fs;
    } else {
        return NULL;
    }
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    (void)speed;

    return device_quality_descriptor;
}

static const uint8_t *other_speed_config_descriptor_callback(uint8_t speed)
{
    if (speed == USB_SPEED_HIGH) {
        return other_speed_config_descriptor_hs;
    } else if (speed == USB_SPEED_FULL) {
        return other_speed_config_descriptor_fs;
    } else {
        return NULL;
    }
}

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;

    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor cdc_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .other_speed_descriptor_callback = other_speed_config_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
};

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    switch (event) {
        case USBD_EVENT_RESET:
            break;
        case USBD_EVENT_CONNECTED:
            break;
        case USBD_EVENT_DISCONNECTED:
            break;
        case USBD_EVENT_RESUME:
            break;
        case USBD_EVENT_SUSPEND:
            break;
        case USBD_EVENT_CONFIGURED:
            break;
        case USBD_EVENT_SET_REMOTE_WAKEUP:
            break;
        case USBD_EVENT_CLR_REMOTE_WAKEUP:
            break;

        default:
            break;
    }
}

static const scpi_command_t scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    {
        .pattern = "*CLS",
        .callback = SCPI_CoreCls,
    },
    {
        .pattern = "*ESE",
        .callback = SCPI_CoreEse,
    },
    {
        .pattern = "*ESE?",
        .callback = SCPI_CoreEseQ,
    },
    {
        .pattern = "*ESR?",
        .callback = SCPI_CoreEsrQ,
    },
    {
        .pattern = "*IDN?",
        .callback = SCPI_CoreIdnQ,
    },
    {
        .pattern = "*OPC",
        .callback = SCPI_CoreOpc,
    },
    {
        .pattern = "*OPC?",
        .callback = SCPI_CoreOpcQ,
    },
    {
        .pattern = "*RST",
        .callback = SCPI_CoreRst,
    },
    {
        .pattern = "*SRE",
        .callback = SCPI_CoreSre,
    },
    {
        .pattern = "*SRE?",
        .callback = SCPI_CoreSreQ,
    },
    {
        .pattern = "*STB?",
        .callback = SCPI_CoreStbQ,
    },
    {
        .pattern = "*TST?",
        .callback = SCPI_CoreTstQ,
    },
    {
        .pattern = "*WAI",
        .callback = SCPI_CoreWai,
    },

    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {
        .pattern = "SYSTem:ERRor[:NEXT]?",
        .callback = SCPI_SystemErrorNextQ,
    },
    {
        .pattern = "SYSTem:ERRor:COUNt?",
        .callback = SCPI_SystemErrorCountQ,
    },
    {
        .pattern = "SYSTem:VERSion?",
        .callback = SCPI_SystemVersionQ,
    },

    {
        .pattern = "STATus:QUEStionable[:EVENt]?",
        .callback = SCPI_StatusQuestionableEventQ,
    },
    {
        .pattern = "STATus:QUEStionable:CONDition?",
        .callback = SCPI_StatusQuestionableConditionQ,
    },
    {
        .pattern = "STATus:QUEStionable:ENABle",
        .callback = SCPI_StatusQuestionableEnable,
    },
    {
        .pattern = "STATus:QUEStionable:ENABle?",
        .callback = SCPI_StatusQuestionableEnableQ,
    },

    {
        .pattern = "STATus:OPERation[:EVENt]?",
        .callback = SCPI_StatusOperationEventQ,
    },
    {
        .pattern = "STATus:OPERation:CONDition?",
        .callback = SCPI_StatusOperationConditionQ,
    },
    {
        .pattern = "STATus:OPERation:ENABle",
        .callback = SCPI_StatusOperationEnable,
    },
    {
        .pattern = "STATus:OPERation:ENABle?",
        .callback = SCPI_StatusOperationEnableQ,
    },

    {
        .pattern = "STATus:PRESet",
        .callback = SCPI_StatusPreset,
    },

    {
        .pattern = "STUB",
        .callback = SCPI_Stub,
    },
    {
        .pattern = "STUB?",
        .callback = SCPI_StubQ,
    },

    SCPI_CMD_LIST_END
};

#define SCPI_IDN1 "cherry-embedded"
#define SCPI_IDN2 "CherryUSB"
#define SCPI_IDN3 "20250607123456"
#define SCPI_IDN4 "v1.0.0"

static const char *idn[] = {
    SCPI_IDN1,
    SCPI_IDN2,
    SCPI_IDN3,
    SCPI_IDN4,
};

static struct usbd_interface intf0;

void tmc_init(uint8_t busid, uint32_t reg_base)
{
    usbd_desc_register(busid, &cdc_descriptor);
    usbd_add_interface(busid, usbd_tmc_init_intf(busid, &intf0, CDC_OUT_EP, CDC_IN_EP, scpi_commands, idn));
    usbd_initialize(busid, reg_base, usbd_event_handler);
}