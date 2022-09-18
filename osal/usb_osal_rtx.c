#include "usb_osal.h"
#include "usb_errno.h"
#include "stdlib.h"
#include "RTL.h"

uint32_t enter_critical(void);
uint32_t exit_critical(uint32_t sr);

//延时=timeout/os_tick， os tick设置为1ms，延时=timeout
//移植为arm9，其他版本可能需要调整汇编代码

//按最大端点数量设置，这里设为8
_declare_box8(sem_mpool, sizeof(OS_SEM), 8);
_declare_box8(mut_mpool, sizeof(OS_MUT), 8);

usb_osal_thread_t usb_osal_thread_create(const char *name, uint32_t stack_size, uint32_t prio, usb_thread_entry_t entry, void *args)
{
    void *stk = malloc(stack_size);
    _init_box8(sem_mpool, sizeof(sem_mpool), sizeof(OS_SEM));
    _init_box8(mut_mpool, sizeof(mut_mpool), sizeof(OS_MUT));

    return (usb_osal_thread_t)os_tsk_create_user_ex (entry, prio,
                                  stk, stack_size,
                                  args);
}

usb_osal_sem_t usb_osal_sem_create(uint32_t initial_count)
{
    void *sem = _alloc_box(sem_mpool);
    if(sem != NULL) os_sem_init(sem, initial_count);
    return (usb_osal_sem_t)sem;
}

//删除os同步量，并不简单    //不能有无限等待的信号量，否则不能删除
void usb_osal_sem_delete(usb_osal_sem_t sem)
{
    _free_box(sem_mpool, sem);
}

int usb_osal_sem_take(usb_osal_sem_t sem, uint32_t timeout)
{
    return (os_sem_wait(sem, timeout) != OS_R_TMO) ? 0 : -ETIMEDOUT;
}

//在线程，中断均调用
int usb_osal_sem_give(usb_osal_sem_t sem)
{
    uint32_t intstatus = 0;
     /* Obtain the number of the currently executing interrupt. */
     __asm volatile ( "mrs intstatus, cpsr" );
    if ((intstatus & 0xf) == 0) {   //user mode
        os_sem_send(sem);
    } else {
        isr_sem_send(sem);
    }

    return 0;
}

usb_osal_mutex_t usb_osal_mutex_create(void)
{
    void *mut = _alloc_box(mut_mpool);
    if(mut != NULL) os_mut_init(mut);
    return (usb_osal_mutex_t)mut;
}

void usb_osal_mutex_delete(usb_osal_mutex_t mutex)
{
    _free_box(mut_mpool, mutex);
}

int usb_osal_mutex_take(usb_osal_mutex_t mutex)
{
    os_mut_wait(mutex, 0xffff);
    return 0;
}

int usb_osal_mutex_give(usb_osal_mutex_t mutex)
{
    return (os_mut_release(mutex) == OS_R_OK) ? 0 : -EINVAL;
}

//这两个函数只在线程调用，可以改为tsk_lock和tsk_unlock版本
size_t usb_osal_enter_critical_section(void)
{
    return enter_critical();
}

void usb_osal_leave_critical_section(size_t flag)
{
    exit_critical(flag);
}

void usb_osal_msleep(uint32_t delay)
{
    os_dly_wait(delay);
}

#pragma arm

__asm uint32_t __builtin_ctz(uint32_t val)
{
    rsb r3, r0, #0
    and r0, r3, r0
    clz r0, r0
    rsb r0, r0, #31
    bx  lr
}

__asm uint32_t enter_critical(void)
{
    mrs R0, CPSR
    orr R1, R0, #0xC0   ; Disable IRQ and FIQ
    msr CPSR_c, R1
    bx  lr
}

__asm uint32_t exit_critical(uint32_t sr)
{
    msr CPSR_c, R0
    bx  lr
}
