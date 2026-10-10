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
#include "semphr.h"
#include "timers.h"

#include "sdkconfig.h"

#include "fassert.h"
#include "finterrupt.h"

#include "usbh_core.h"

#if defined(CONFIG_CHERRY_USB_PORT_XHCI_PCIE)

#include "fpcie_ecam.h"
#include "fpcie_ecam_common.h"

/************************** Constant Definitions *****************************/
#define USB_XHCI_IRQ_TASK_STACK 8192U
#define FUSB3_PCIE_NUM          1 /* PCIe single controller */

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static FPcieEcam pcie_device;
static SemaphoreHandle_t s_xhci_irq_sem[FUSB3_PCIE_NUM] = { NULL };
static TimerHandle_t s_port_poll_timer[FUSB3_PCIE_NUM] = { NULL };

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/* Port poll timer callback: runs in the timer daemon task context */
static void cusb_port_poll_timer_cb(TimerHandle_t handle)
{
    extern void cusb_xhci_port_poll(uint8_t busid);

    cusb_xhci_port_poll((uint8_t)(uintptr_t)pvTimerGetTimerID(handle));
}

/******************************* Functions ***********************************/

static void usb_hc_xhci_irq_task(void *arg)
{
    uint32_t id = (uint32_t)(uintptr_t)arg;
    extern void USBH_IRQHandler(uint8_t busid);

    for (;;) {
        if (s_xhci_irq_sem[id] == NULL) {
            USB_LOG_ERR("xhci_irq sem NULL id=%lu\n", (unsigned long)id);
            while (1)
                ;
        }
        xSemaphoreTake(s_xhci_irq_sem[id], portMAX_DELAY);
        USBH_IRQHandler((uint8_t)id);
    }
}

static void usb_hc_xhci_pcie_interrupt_handler(void *param)
{
    BaseType_t higher_prio_woken = pdFALSE;
    extern void cusb_xhci_irq_ack(uint8_t busid);
    uint32_t id = (uint32_t)(uintptr_t)0;

    (void)param;
    cusb_xhci_irq_ack((uint8_t)id);

    if (s_xhci_irq_sem[id] != NULL) {
        xSemaphoreGiveFromISR(s_xhci_irq_sem[id], &higher_prio_woken);
        portYIELD_FROM_ISR(higher_prio_woken);
    }
}

static void usb_hc_pcie_intx_init(FPcieEcam *instance_p)
{
    u32 cpu_id;
    u32 irq_num = FPCIE_ECAM_INTA_IRQ_NUM;
    u32 irq_priority = USB_XHCI_GIC_IRQ_PRIO;

    (void)GetCpuId(&cpu_id);
    USB_LOG_DBG("interrupt num: %d", irq_num);
    (void)InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* register intr callback */
    InterruptInstall(irq_num,
                     FPcieEcamIntxIrqHandler,
                     &pcie_device,
                     NULL);
}

static FError usb_hc_pcie_init(FPcieEcam *pcie_device)
{
    FError ret = FT_SUCCESS;

    ret = FPcieEcamCfgInitialize(pcie_device, FPcieEcamLookupConfig(FPCIE_ECAM_INSTANCE0), NULL);
    if (FT_SUCCESS != ret) {
        return ret;
    }

    USB_LOG_DBG("\n");
    USB_LOG_DBG("	PCI:\n");
    USB_LOG_DBG("	B:D:F			VID:PID			parent_BDF			class_code\n");
    ret = FPcieEcamEnumerateBus(pcie_device, 0);
    if (FT_SUCCESS != ret) {
        return ret;
    }

    usb_hc_pcie_intx_init(pcie_device); /* register pcie_device intx handler */

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
    if (FT_SUCCESS != ret) {
        USB_LOG_ERR("FPcieIntxRegiterIrqHandler failed.\n");
        return ret;
    }

    return ret;
}

void usb_hc_disable_interrupt(u32 id)
{
    u32 irq_num = FPCIE_ECAM_INTA_IRQ_NUM;

    InterruptMask(irq_num);
}

void usb_hc_enable_interrupt(u32 id)
{
    u32 irq_num = FPCIE_ECAM_INTA_IRQ_NUM;

    InterruptUmask(irq_num);
}

void usb_hc_setup_xhci_interrupt(u32 id)
{
    char task_name[configMAX_TASK_NAME_LEN];

    if (s_xhci_irq_sem[id] == NULL) {
        s_xhci_irq_sem[id] = xSemaphoreCreateBinary();
        FASSERT(s_xhci_irq_sem[id] != NULL);
        snprintf(task_name, sizeof(task_name), "xhci_irq%lu", (unsigned long)id);
        FASSERT(usb_osal_thread_create(task_name, USB_XHCI_IRQ_TASK_STACK,
                                       CONFIG_USB_XHCI_IRQ_PRIO, usb_hc_xhci_irq_task,
                                       (void *)(uintptr_t)id) != NULL);
    }

    /* Port poll timer: deterministic fallback scanning PORTSC every
     * 100 ms; deinit stops it but never deletes it. */
    if (s_port_poll_timer[id] == NULL) {
        s_port_poll_timer[id] = xTimerCreate("usb_poll",
                                             pdMS_TO_TICKS(100), pdTRUE,
                                             (void *)(uintptr_t)id, cusb_port_poll_timer_cb);
        FASSERT(s_port_poll_timer[id] != NULL);
    }
    xTimerStart(s_port_poll_timer[id], portMAX_DELAY);
}

void usb_hc_port_poll_stop(u32 id)
{
    if (s_port_poll_timer[id] != NULL) {
        xTimerStop(s_port_poll_timer[id], portMAX_DELAY);
    }
}

unsigned long usb_hc_get_register_base(uint32_t id)
{
    return 0U;
}

unsigned long usb_hc_setup_xhci_pcie(struct usbh_bus *usb)
{
    FError ret = FT_SUCCESS;
    s32 host;
    u32 bdf;
    u32 class;
    u16 pci_command;
    u8 bus, device, function;
    u16 vid, did;
    uintptr bar0_addr = 0;
    uintptr bar1_addr = 0;
    unsigned long usb_base = 0U;
    const u32 class_code = FPCI_CLASS_SERIAL_USB_XHCI; /* sub class and base class definition */
    u32 config_data;

    ret = usb_hc_pcie_init(&pcie_device);
    if (FT_SUCCESS != ret) {
        USB_LOG_ERR("FPcieInit failed.\n");
        return usb_base;
    }

    /* find xhci host from pcie_device instance */
    for (host = 0; host < pcie_device.scans_bdf_count; host++) {
        bus = pcie_device.scans_bdf[host].bus;
        device = pcie_device.scans_bdf[host].device;
        function = pcie_device.scans_bdf[host].function;

        FPcieEcamReadConfigSpace(&pcie_device, bus, device, function, FPCIE_CCR_REV_CLASSID_REGS, &config_data);
        class = config_data >> 8;

        if (class == class_code) {
            (void)FPcieEcamReadConfigSpace(&pcie_device, bus, device, function, FPCIE_CCR_ID_REG, &config_data);
            vid = FPCIE_CCR_VENDOR_ID_MASK(config_data);
            did = FPCIE_CCR_DEVICE_ID_MASK(config_data);

            USB_LOG_DBG("xHCI-PCI HOST found !!!, b.d.f = %x.%x.%x\n", bus, device, function);
            FPcieEcamReadConfigSpace(&pcie_device, bus, device, function, FPCIE_CCR_BAR_ADDR0_REGS, (u32 *)&bar0_addr);
            bar0_addr &= ~0xfff;

#if defined(FAARCH64_USE)
            FPcieEcamReadConfigSpace(&pcie_device, bus, device, function, FPCIE_CCR_BAR_ADDR1_REGS, (u32 *)&bar1_addr);
#endif

            USB_LOG_DBG("XHCI-PCI BarAddress %p:%p", bar1_addr, bar0_addr);

            if ((0x0 == bar0_addr) && (0x0 == bar1_addr)) {
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