/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef USBD_TMC_H
#define USBD_TMC_H

#include "usb_tmc.h"
#include "scpi/scpi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init tmc interface driver */
struct usbd_interface *usbd_tmc_init_intf(uint8_t busid,
                                          struct usbd_interface *intf,
                                          const uint8_t out_ep, const uint8_t in_ep,
                                          const scpi_command_t *scpi_commands,
                                          const char **idn);

#ifdef __cplusplus
}
#endif

#endif /* USBD_TMC_H */
