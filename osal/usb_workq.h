#ifndef _USB_WORKQUEUE_H
#define _USB_WORKQUEUE_H

/* Defines the work callback */
typedef void (*usb_worker_t)(void *arg);

struct usb_work
{
    usb_dlist_t list;
    usb_worker_t worker;
    void *arg;
};

struct usb_workqueue
{
    usb_dlist_t work_list;
    usb_dlist_t delay_work_list;
    usb_osal_sem_t sem;
    usb_osal_thread_t thread;
};

extern struct usb_workqueue g_hpworkq;
extern struct usb_workqueue g_lpworkq;

void usb_workqueue_submit(struct usb_workqueue *queue, struct usb_work *work, usb_worker_t worker, void *arg, uint32_t ticks);
int usbh_workq_initialize();
#endif