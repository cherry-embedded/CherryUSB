/*
 * Copyright : (C) 2024 Phytium Information Technology, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * ----- ------     --------    --------------------------------------
 * 1.0   zhugengyu  2024/6/26 first commit
 */
/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "sdkconfig.h"

#include "fassert.h"
#include "finterrupt.h"

#include "usbh_core.h"

#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PCIE)

#include "fpcie_ecam.h"
#include "fpcie_ecam_common.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static FPcieEcam pcie_device;

static void usb_hc_xhci_pcie_interrupt_handler(void *param)
{
    extern void USBH_IRQHandler(uint8_t busid);
    USBH_IRQHandler((uint8_t)(uintptr_t)0);
}

static void usb_hc_pcie_intx_init(FPcieEcam *instance_p)
{
    u32 cpu_id;
    u32 irq_num = FPCIE_ECAM_INTA_IRQ_NUM;
    u32 irq_priority = 13U;

    (void)GetCpuId(&cpu_id);
    USB_LOG_DBG("interrupt num: %d", irq_num);
    (void)InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     FPcieEcamIntxIrqHandler,
                     &pcie_device,
                     NULL);

    /* enable irq */
    InterruptUmask(irq_num);
}

static FError usb_hc_pcie_init(FPcieEcam *pcie_device)
{
    FError ret = FT_SUCCESS;

    ret = FPcieEcamCfgInitialize(pcie_device, FPcieEcamLookupConfig(FPCIE_ECAM_INSTANCE0), NULL);
    if (FT_SUCCESS != ret)
    {
        return ret;
    }
    
    USB_LOG_DBG("\n");
    USB_LOG_DBG("	PCI:\n");
    USB_LOG_DBG("	B:D:F			VID:PID			parent_BDF			class_code\n");
    ret = FPcieEcamEnumerateBus(pcie_device, 0);
    if (FT_SUCCESS != ret)
    {
        return ret;
    }

    usb_hc_pcie_intx_init(pcie_device);    /* register pcie_device intx handler */

    return FT_SUCCESS;
}

static FError usb_hc_pcie_install_irq(FPcieEcam *pcie_device, struct usbh_bus *usb, u8 bus, u8 device, u8 function)
{
    FError ret = FT_SUCCESS;
    FPcieIntxFun intx_fun;
    intx_fun.IntxCallBack = usb_hc_xhci_pcie_interrupt_handler;
    intx_fun.args = usb;
    intx_fun.bus = bus;
	intx_fun.device = device;
	intx_fun.function = function;

    ret = FPcieEcamIntxRegister(pcie_device, bus, device, function, &intx_fun);
    if (FT_SUCCESS != ret)
    {
        USB_LOG_ERR("FPcieIntxRegiterIrqHandler failed.\n");
        return ret;
    }

    return ret;
}

unsigned long usb_hc_setup_xhci_pcie(struct usbh_bus *usb)
{
    FError ret = FT_SUCCESS;
    s32 host;
    u32 bdf;
    u32 class;
    u16 pci_command;
    u8 bus,device,function;
    u16 vid, did;
    uintptr bar0_addr = 0;
    uintptr bar1_addr = 0;
    unsigned long usb_base = 0U;
    const u32 class_code = FPCI_CLASS_SERIAL_USB_XHCI; /* sub class and base class definition */
    u32 config_data;

    ret = usb_hc_pcie_init(&pcie_device);
    if (FT_SUCCESS != ret)
    {
        USB_LOG_ERR("FPcieInit failed.\n");
        return usb_base;
    }

    /* find xhci host from pcie_device instance */
    for (host = 0; host < pcie_device.scans_bdf_count; host++)
    {
        bus		= pcie_device.scans_bdf[host].bus;
		device	= pcie_device.scans_bdf[host].device;
		function= pcie_device.scans_bdf[host].function;

        FPcieEcamReadConfigSpace(&pcie_device,bus,device,function,FPCIE_CCR_REV_CLASSID_REGS,&config_data);
		class =  config_data >> 8;

        if (class == class_code)
        {
            (void)FPcieEcamReadConfigSpace(&pcie_device,bus,device,function,FPCIE_CCR_ID_REG,&config_data);
			vid = FPCIE_CCR_VENDOR_ID_MASK(config_data);
			did = FPCIE_CCR_DEVICE_ID_MASK(config_data);            

            USB_LOG_DBG("xHCI-PCI HOST found !!!, b.d.f = %x.%x.%x\n", bus, device, function);
            FPcieEcamReadConfigSpace(&pcie_device,bus,device,function,FPCIE_CCR_BAR_ADDR0_REGS,(u32 *)&bar0_addr);
            bar0_addr &= ~0xfff;

#if defined(FAARCH64_USE)
            FPcieEcamReadConfigSpace(&pcie_device,bus,device,function,FPCIE_CCR_BAR_ADDR1_REGS,(u32 *)&bar1_addr);
#endif

            USB_LOG_DBG("FSataPcieIntrInstall BarAddress %p:%p", bar1_addr, bar0_addr);

            if ((0x0 == bar0_addr) && (0x0 == bar1_addr))
            {
                USB_LOG_ERR("Invalid Bar address");
                return usb_base;
            }        

            usb_hc_pcie_install_irq(&pcie_device, usb, bus, device, function);
#if defined(FAARCH64_USE)
            usb_base = (bar1_addr << 32U) | bar0_addr;
#else
            usb_base = bar0_addr;
#endif
            USB_LOG_INFO("xHCI base address: 0x%lx", usb_base);
        }                
    }

    return usb_base;
}
#endif