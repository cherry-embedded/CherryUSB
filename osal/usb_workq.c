#include "usb_list.h"
#include "usb_osal.h"
#include "usb_workq.h"
#include "usb_config.h"

void usb_workqueue_submit(struct usb_workqueue *queue, struct usb_work *work, usb_worker_t worker, void *arg, uint32_t ticks)
{
    uint32_t flags;
    flags = usb_osal_enter_critical_section();
    usb_dlist_remove(&work->list);
    work->worker = worker;
    work->arg = arg;

    if (ticks == 0) {
        usb_dlist_insert_after(&queue->work_list, &work->list);
        usb_osal_sem_give(queue->sem);
    }

    usb_osal_leave_critical_section(flags);
}

struct usb_workqueue g_hpworkq = { NULL };
struct usb_workqueue g_lpworkq = { NULL };

static void usbh_hpwork_thread(void *argument)
{
    struct usb_work *work;
    uint32_t flags;
    int ret;
    struct usb_workqueue *queue = (struct usb_workqueue *)argument;
    while (1) {
        ret = usb_osal_sem_take(queue->sem);
        if (ret < 0) {
            continue;
        }
        flags = usb_osal_enter_critical_section();
        if (usb_dlist_isempty(&queue->work_list)) {
            usb_osal_leave_critical_section(flags);
            continue;
        }
        work = usb_dlist_first_entry(&queue->work_list, struct usb_work, list);
        usb_dlist_remove(&work->list);
        usb_osal_leave_critical_section(flags);
        work->worker(work->arg);
    }
}

static void usbh_lpwork_thread(void *argument)
{
    struct usb_work *work;
    uint32_t flags;
    int ret;
    struct usb_workqueue *queue = (struct usb_workqueue *)argument;
    while (1) {
        ret = usb_osal_sem_take(queue->sem);
        if (ret < 0) {
            continue;
        }
        flags = usb_osal_enter_critical_section();
        if (usb_dlist_isempty(&queue->work_list)) {
            usb_osal_leave_critical_section(flags);
            continue;
        }
        work = usb_dlist_first_entry(&queue->work_list, struct usb_work, list);
        usb_dlist_remove(&work->list);
        usb_osal_leave_critical_section(flags);
        work->worker(work->arg);
    }
}

int usbh_workq_initialize(void)
{
    g_hpworkq.sem = usb_osal_sem_create(0);
    if (g_hpworkq.sem == NULL) {
        return -1;
    }
    g_hpworkq.thread = usb_osal_thread_create("usbh_hpworkq", CONFIG_USBHOST_HPWORKQ_STACKSIZE, CONFIG_USBHOST_HPWORKQ_PRIO, usbh_hpwork_thread, &g_hpworkq);
    if (g_hpworkq.thread == NULL) {
        return -1;
    }

    g_lpworkq.sem = usb_osal_sem_create(0);
    if (g_lpworkq.sem == NULL) {
        return -1;
    }

    g_lpworkq.thread = usb_osal_thread_create("usbh_lpworkq", CONFIG_USBHOST_LPWORKQ_STACKSIZE, CONFIG_USBHOST_LPWORKQ_PRIO, usbh_lpwork_thread, &g_lpworkq);
    if (g_lpworkq.thread == NULL) {
        return -1;
    }
    return 0;
}
