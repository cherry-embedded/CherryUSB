#include "usbh_core.h"
#include "ftypes.h"

static u32 memp_ref_cnt = 0;

void usb_sys_mem_init(void);
void usb_sys_mem_deinit(void);
void usb_hc_setup_interrupt(u32 id);
void usb_hc_revoke_interrupt(u32 id);
void usb_hc_port_poll_stop(u32 id);
void cusb_callout_drain_resource_init(void);

void usb_hc_low_level_init(struct usbh_bus *bus)
{
    if (memp_ref_cnt == 0) {
        usb_sys_mem_init();                 /* create memory pool before first bus init */
        cusb_callout_drain_resource_init(); /* init global drain lock/semaphore */
    }

    memp_ref_cnt++; /* one more bus is using the memory pool */

    usb_hc_setup_interrupt(bus->busid);
}

void usb_hc_low_level_deinit(struct usbh_bus *bus)
{
    memp_ref_cnt--; /* one more bus is leaving */

    usb_hc_port_poll_stop(bus->busid);
    usb_hc_revoke_interrupt(bus->busid);

    if (memp_ref_cnt == 0) {
        usb_sys_mem_deinit(); /* release memory pool after the last bus left */
    }
}

__WEAK void USBOTG_IRQHandler(uint8_t busid)
{
}

__WEAK void USBH_IRQHandler(uint8_t busid)
{
}