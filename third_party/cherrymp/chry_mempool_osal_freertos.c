/*
 * Copyright (c) 2024, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "chry_mempool.h"
#include <FreeRTOS.h>
#include "semphr.h"

chry_mempool_osal_sem_t chry_mempool_osal_sem_create(uint32_t max_count)
{
    return (chry_mempool_osal_sem_t)xSemaphoreCreateCounting(max_count, 0);
}

void chry_mempool_osal_sem_delete(chry_mempool_osal_sem_t sem)
{
    vSemaphoreDelete((SemaphoreHandle_t)sem);
}

int chry_mempool_osal_sem_take(chry_mempool_osal_sem_t sem, uint32_t timeout)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    if (xPortIsInsideInterrupt()) {
        ret = xSemaphoreTakeFromISR((SemaphoreHandle_t)sem, &xHigherPriorityTaskWoken);
        if (ret == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
        return (ret == pdPASS) ? 0 : -1;
    } else {
        return (xSemaphoreTake((SemaphoreHandle_t)sem, pdMS_TO_TICKS(timeout)) == pdPASS) ? 0 : -1;
    }
}

int chry_mempool_osal_sem_give(chry_mempool_osal_sem_t sem)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    int ret;

    if (xPortIsInsideInterrupt()) {
        ret = xSemaphoreGiveFromISR((SemaphoreHandle_t)sem, &xHigherPriorityTaskWoken);
        if (ret == pdPASS) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else {
        ret = xSemaphoreGive((SemaphoreHandle_t)sem);
    }

    return (ret == pdPASS) ? 0 : -1;
}