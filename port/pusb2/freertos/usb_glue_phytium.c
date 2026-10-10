/*
 * Copyright : (C) 2024 Phytium Information Technology, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Modify History:
 *  Ver   Who        Date         Changes
 * -----  ------     --------    --------------------------------------
 * 1.0   zhugengyu  2024/6/26 first commit
 */

/***************************** Include Files *********************************/
#include <stdio.h>
#include <string.h>

#include "fassert.h"
#include "fparameters.h"
#include "finterrupt.h"
#include "fcpu_info.h"
#include "fdebug.h"
#include "fcache.h"
#include "fmemory_pool.h"
#include "usb_config.h"

#ifdef CONFIG_USB_OTG_ENABLE
#include "FreeRTOS.h"
#include "task.h"
#include "usbotg_core.h"
#endif

extern void cps_otg_switch_to_host(uintptr_t reg_base);
extern void cps_otg_switch_to_device(uintptr_t reg_base);
extern uint8_t cps_otg_read_state(uintptr_t reg_base);

/************************** Constant Definitions *****************************/
#define USB_MEMP_TOTAL_SIZE SZ_1M

/**************************** Type Definitions *******************************/

/************************** Variable Definitions *****************************/
static FMemp memp;
static u8 memp_buf[USB_MEMP_TOTAL_SIZE] __attribute__((aligned(8))) = { 0 };

/* Memory pool reference counter - shared between DC and HC */
static u32 memp_ref_cnt = 0;

/* OTG mode flag: set when OTG is initialized */
static u8 g_otg_mode_active = 0;

/************************** Function Implementations *************************/

/**
 * @brief Initialize USB system memory pool
 *
 * Called by both DC and HC low_level_init. Memory pool is created on first call.
 */
void usb_sys_mem_init(void)
{
    if (FT_COMPONENT_IS_READY != memp.is_ready) {
        FASSERT(FT_SUCCESS == FMempInit(&memp, &memp_buf[0], &memp_buf[0] + USB_MEMP_TOTAL_SIZE));
    }
}

/**
 * @brief Deinitialize USB system memory pool
 *
 * Called by both DC and HC low_level_deinit. Memory pool is destroyed when
 * last user releases it.
 */
void usb_sys_mem_deinit(void)
{
    if (FT_COMPONENT_IS_READY == memp.is_ready) {
        FMempDeinit(&memp);
    }
}

/**
 * @brief Allocate aligned memory from USB memory pool
 */
void *usb_sys_malloc_align(size_t align, size_t size)
{
    void *result = FMempMallocAlign(&memp, size, align);

    if (result) {
        memset(result, 0U, size);
    }

    return result;
}

/**
 * @brief Allocate memory from USB memory pool (aligned to void*)
 */
void *usb_sys_mem_malloc(size_t size)
{
    return usb_sys_malloc_align(sizeof(void *), size);
}

/**
 * @brief Free memory back to USB memory pool
 */
void usb_sys_mem_free(void *ptr)
{
    if (NULL != ptr) {
        FMempFree(&memp, ptr);
    }
}

/**
 * @brief USB assert handler
 */
void usb_assert(const char *filename, int linenum)
{
    FAssert(filename, linenum, 0xff);
}

/**
 * @brief Increment memory pool reference counter
 *
 * Called by DC/HC low_level_init to track memory pool users.
 */
void usb_sys_mem_inc_ref(void)
{
    memp_ref_cnt++;
}

/**
 * @brief Decrement memory pool reference counter
 *
 * Called by DC/HC low_level_deinit. When counter reaches 0,
 * memory pool is deinitialized.
 * @return 1 if this was the last user (pool deinitialized), 0 otherwise
 */
u32 usb_sys_mem_dec_ref(void)
{
    if (memp_ref_cnt > 0) {
        memp_ref_cnt--;
    }
    return (memp_ref_cnt == 0) ? 1 : 0;
}

/**
 * @brief Get current memory pool reference count
 */
u32 usb_sys_mem_get_ref(void)
{
    return memp_ref_cnt;
}

/**
 * @brief Check if running inside interrupt context
 */
extern int vApplicationInIrq(void);
int xPortIsInsideInterrupt(void)
{
    return vApplicationInIrq();
}

/**
 * @brief Check if OTG mode is active
 */
u8 usb_otg_is_active(void)
{
    return g_otg_mode_active;
}

/**
 * @brief Set OTG mode active flag
 */
void usb_otg_set_active(u8 active)
{
    g_otg_mode_active = active;
}

#ifdef CONFIG_USB_OTG_ENABLE
/**
 * @brief Get USB OTG register base address
 */
unsigned long usb_otg_get_register_base(uint32_t id)
{
    FASSERT(id == CONFIG_USB_PUSB2_BUS_ID);
    return FUSB2_0_VHUB_BASE_ADDR;
}

/* External declaration for OTG IRQ handler provided by usbotg_core.c */
extern void USBOTG_IRQHandler(uint8_t busid);

/**
 * @brief OTG interrupt handler for PUSB2
 */
static void usb_otg_pusb2_interrupt_handler(s32 vector, void *param)
{
    (void)param;
    FASSERT(vector == FUSB2_0_VHUB_IRQ_NUM);
    USBOTG_IRQHandler((uint8_t)CONFIG_USB_PUSB2_BUS_ID);
}

/**
 * @brief Install OTG interrupt for PUSB2
 */
static void usb_otg_setup_interrupt(void)
{
    u32 cpu_id;
    u32 irq_num = FUSB2_0_VHUB_IRQ_NUM;
    u32 irq_priority = 13U;

    GetCpuId(&cpu_id);
    InterruptSetTargetCpus(irq_num, cpu_id);

    InterruptSetPriority(irq_num, irq_priority);

    /* Register OTG interrupt callback */
    InterruptInstall(irq_num,
                     usb_otg_pusb2_interrupt_handler,
                     NULL,
                     NULL);

    /* Enable IRQ */
    InterruptUmask(irq_num);

    FT_DEBUG_PRINT_I("OTG-GLUE", "Installed OTG interrupt, irq=%d\r\n", irq_num);
}

/**
 * @brief Initialize USB OTG hardware
 */
int usb_otg_init(uint8_t busid)
{
    FASSERT(busid == CONFIG_USB_PUSB2_BUS_ID);

    /* Install OTG interrupt handler */
    usb_otg_setup_interrupt();

    usb_otg_set_active(1);
    return 0;
}

/**
 * @brief Deinitialize USB OTG hardware
 */
int usb_otg_deinit(uint8_t busid)
{
    FASSERT(busid == CONFIG_USB_PUSB2_BUS_ID);
    usb_otg_set_active(0);
    return 0;
}

/**
 * @brief Switch USB OTG to Host mode
 */
void usb_otg_switch_to_host(uint8_t busid)
{
    uintptr_t reg_base = usb_otg_get_register_base(busid);
    cps_otg_switch_to_host(reg_base);
}

/**
 * @brief Switch USB OTG to Device mode
 */
void usb_otg_switch_to_device(uint8_t busid)
{
    uintptr_t reg_base = usb_otg_get_register_base(busid);
    cps_otg_switch_to_device(reg_base);
}

/* ==================== OTG Auto (software polling) ==================== */

#define OTG_AUTO_POLL_MS 500

enum {
    OTG_STATE_A_IDLE = 0x00,
    OTG_STATE_A_HOST = 0x03,
    OTG_STATE_B_IDLE = 0x10,
    OTG_STATE_B_PERIPHERAL = 0x11,
};

static int otg_auto_running = 0;
static uint8_t otg_auto_role = USBOTG_MODE_HOST;

static void otg_auto_poll_task(void *param)
{
    uint8_t busid = *(uint8_t *)param;
    uintptr_t reg_base = usb_otg_get_register_base(busid);
    int idle_ticks = 0;

    vTaskDelay(pdMS_TO_TICKS(300));

    while (otg_auto_running) {
        uint8_t state = cps_otg_read_state(reg_base);
        uint8_t role = 0;
        int need_switch = 0;
        int target_role = 0;

        switch (state) {
            case OTG_STATE_A_HOST:
                role = USBOTG_MODE_HOST;
                break;
            case OTG_STATE_B_PERIPHERAL:
                role = USBOTG_MODE_DEVICE;
                break;
            default:
                break;
        }

        if (role) {
            /* hardware detected a role: Host (U-disk) or Device (PC) */
            idle_ticks = 0;
            if (otg_auto_role != role) {
                need_switch = 1;
                target_role = role;
            }
        } else if (otg_auto_role == USBOTG_MODE_DEVICE) {
            /* role == 0 means nothing connected to either side.
             * If we're in Device mode, Host was killed; re-init so
             * VBUS comes back and U-disk can be powered/detected. */
            idle_ticks++;
            if (idle_ticks >= 1) {
                need_switch = 1;
                target_role = USBOTG_MODE_HOST;
                idle_ticks = 0;
            }
        }

        if (need_switch) {
            otg_auto_role = target_role;
            usbotg_trigger_role_change(busid, target_role);
            FT_DEBUG_PRINT_I("OTG-AUTO", "-> %s\r\n",
                             target_role == USBOTG_MODE_HOST ? "Host" : "Device");
        }

        vTaskDelay(pdMS_TO_TICKS(OTG_AUTO_POLL_MS));
    }
    vTaskDelete(NULL);
}

static TaskHandle_t g_otg_poll_task = NULL;

int usb_otg_auto_poll_start(uint8_t busid, int default_role)
{
    static uint8_t busid_arg;

    otg_auto_role = default_role;
    otg_auto_running = 1;
    busid_arg = busid;

    (void)xTaskCreate(otg_auto_poll_task,
                      "otgapol",
                      4096U,
                      &busid_arg,
                      configMAX_PRIORITIES - 2,
                      &g_otg_poll_task);
    return 0;
}

void usb_otg_auto_poll_stop(void)
{
    otg_auto_running = 0;
    if (g_otg_poll_task) {
        vTaskDelay(pdMS_TO_TICKS(OTG_AUTO_POLL_MS + 100));
        g_otg_poll_task = NULL;
    }
}

uint8_t usb_otg_auto_get_role(void)
{
    return otg_auto_role;
}

#endif /* CONFIG_USB_OTG_ENABLE */