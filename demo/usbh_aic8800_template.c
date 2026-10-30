/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Minimal AIC8800 host hook. This file only shows USB attach/detach.
 * For STA + lwIP see demo/aic8800/usbh_aic8800_wifi_template.c.
 */
#include "usbh_core.h"
#include "usbh_aic8800.h"

void usbh_aic8800_run(struct usbh_aic8800 *aic8800_class)
{
    USB_LOG_INFO("aic8800 %04x:%04x %s\r\n",
                 aic8800_class->vendor_id,
                 aic8800_class->product_id,
                 aic8800_class->runtime ? "runtime" : "boot-loader");

    if (!aic8800_class->runtime) {
        /* Download firmware on the BootROM pipes, then wait for 8d81. */
    } else {
        /* Start the FullMAC / netif layer here. */
    }
}

void usbh_aic8800_stop(struct usbh_aic8800 *aic8800_class)
{
    (void)aic8800_class;
}

void usbh_aic8800_template_init(uint8_t busid, uint32_t reg_base)
{
#ifdef CONFIG_USBHOST_AIC8800_ZEROCD
    usbh_aic8800_enable_zero_cd_modeswitch();
#endif
    usbh_initialize(busid, reg_base, NULL);
}
