/********************************** (C) COPYRIGHT  *******************************
* File Name          : core_riscv.h
* Author             : WCH
* Version            : V1.0.0
* Date               : 2020/07/31
* Description        : RISC-V Core Peripheral Access Layer Header File
*******************************************************************************/
#ifndef __CORE_RV3A_H__
#define __CORE_RV3A_H__

#ifdef __cplusplus
 extern "C" {
#endif

/* IO definitions */
#ifdef __cplusplus
  #define     __I     volatile                /*!< defines 'read only' permissions      */
#else
  #define     __I     volatile const          /*!< defines 'read only' permissions     */
#endif
#define     __O     volatile                  /*!< defines 'write only' permissions     */
#define     __IO    volatile                  /*!< defines 'read / write' permissions   */
#define   RV_STATIC_INLINE  static  inline

//typedef enum {SUCCESS = 0, ERROR = !SUCCESS} ErrorStatus;

typedef enum {DISABLE = 0, ENABLE = !DISABLE} FunctionalState;
typedef enum {RESET = 0, SET = !RESET} FlagStatus, ITStatus;

/* memory mapped structure for Program Fast Interrupt Controller (PFIC) */
typedef struct __attribute__((packed)) {
    __I  UINT32 ISR[8];                 // 0
    __I  UINT32 IPR[8];                 // 20H
    __IO UINT32 ITHRESDR;               // 40H
    UINT8 RESERVED[4];                  // 44H
    __O UINT32 CFGR;                    // 48H
    __I  UINT32 GISR;                   // 4CH
    __IO UINT8 IDCFGR[4];               // 50H
    UINT8 RESERVED0[0x0C];              // 54H
    __IO UINT32 FIADDRR[4];             // 60H
    UINT8 RESERVED1[0x90];              // 70H
    __O  UINT32 IENR[8];                // 100H
    UINT8 RESERVED2[0x60];              // 120H
    __O  UINT32 IRER[8];                // 180H
    UINT8 RESERVED3[0x60];              // 1A0H
    __O  UINT32 IPSR[8];                // 200H
    UINT8 RESERVED4[0x60];              // 220H
    __O  UINT32 IPRR[8];                // 280H
    UINT8 RESERVED5[0x60];              // 2A0H
    __IO UINT32 IACTR[8];               // 300H
    UINT8 RESERVED6[0xE0];              // 320H
    __IO UINT8 IPRIOR[256];             // 400H
    UINT8 RESERVED7[0x810];             // 500H
    __IO UINT32 SCTLR;                  // D10H
}PFIC_Type;

/* memory mapped structure for SysTick */
typedef struct __attribute__((packed))
{
    __IO UINT32 CTLR;
    __IO UINT32 SR;
    __IO UINT64 CNT;
    __IO UINT64 CMP;
}SysTick_Type;


#define PFIC            ((PFIC_Type *) 0xE000E000 )
#define SysTick         ((SysTick_Type *) 0xE000F000)

#define PFIC_KEY1       ((UINT32)0xFA050000)
#define	PFIC_KEY2		((UINT32)0xBCAF0000)
#define	PFIC_KEY3		((UINT32)0xBEEF0000)


/* ##########################   define  #################################### */
#define  __nop()	asm volatile ("nop")

#define read_csr(reg) ({unsigned long __tmp;                        \
     asm volatile ("csrr %0, " #reg : "=r"(__tmp));                 \
         __tmp; })

#define write_csr(reg, val) ({                                      \
    if (__builtin_constant_p(val) && (unsigned long)(val) < 32)    \
      asm volatile ("csrw  " #reg ", %0" :: "i"(val));              \
    else                                                            \
      asm volatile ("csrw  " #reg ", %0" :: "r"(val)); })           \

#define PFIC_EnableAllIRQ()     write_csr(0x800, 0x88)
#define PFIC_DisableAllIRQ()    write_csr(0x800, 0x80)
/* ##########################   PFIC functions  #################################### */

/*******************************************************************************
* Function Name  : PFIC_EnableIRQ
* Description    : Enable Interrupt
* Input          : IRQn: Interrupt Numbers
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_EnableIRQ(IRQn_Type IRQn){
    PFIC->IENR[((UINT32)(IRQn) >> 5)] = (1 << ((UINT32)(IRQn) & 0x1F));
}

/*******************************************************************************
* Function Name  : PFIC_DisableIRQ
* Description    : Disable Interrupt
* Input          : IRQn: Interrupt Numbers
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_DisableIRQ(IRQn_Type IRQn){
    PFIC->IRER[((UINT32)(IRQn) >> 5)] = (1 << ((UINT32)(IRQn) & 0x1F));
    __nop();__nop();
}

/*******************************************************************************
* Function Name  : PFIC_GetStatusIRQ
* Description    : Get Interrupt Enable State
* Input          : IRQn: Interrupt Numbers
* Return         : 1: Interrupt Enable
*                  0: Interrupt Disable
*******************************************************************************/
RV_STATIC_INLINE UINT32 PFIC_GetStatusIRQ(IRQn_Type IRQn){
    return((UINT32) ((PFIC->ISR[(UINT32)(IRQn) >> 5] & (1 << ((UINT32)(IRQn) & 0x1F)))?1:0));
}

/*******************************************************************************
* Function Name  : PFIC_GetPendingIRQ
* Description    : Get Interrupt Pending State
* Input          : IRQn: Interrupt Numbers
* Return         : 1: Interrupt Pending Enable
*                  0: Interrupt Pending Disable
*******************************************************************************/
RV_STATIC_INLINE UINT32 PFIC_GetPendingIRQ(IRQn_Type IRQn){
    return((UINT32) ((PFIC->IPR[(UINT32)(IRQn) >> 5] & (1 << ((UINT32)(IRQn) & 0x1F)))?1:0));
}

/*******************************************************************************
* Function Name  : PFIC_SetPendingIRQ
* Description    : Set Interrupt Pending
* Input          : IRQn: Interrupt Numbers
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_SetPendingIRQ(IRQn_Type IRQn){
    PFIC->IPSR[((UINT32)(IRQn) >> 5)] = (1 << ((UINT32)(IRQn) & 0x1F));
}

/*******************************************************************************
* Function Name  : PFIC_ClearPendingIRQ
* Description    : Clear Interrupt Pending
* Input          : IRQn: Interrupt Numbers
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_ClearPendingIRQ(IRQn_Type IRQn){
    PFIC->IPRR[((UINT32)(IRQn) >> 5)] = (1 << ((UINT32)(IRQn) & 0x1F));
}

/*******************************************************************************
* Function Name  : PFIC_GetActive
* Description    : Get Interrupt Active State
* Input          : IRQn: Interrupt Numbers
* Return         : 1: Interrupt Active
*                  0: Interrupt No Active
*******************************************************************************/
RV_STATIC_INLINE UINT32 PFIC_GetActive(IRQn_Type IRQn){
    return((UINT32)((PFIC->IACTR[(UINT32)(IRQn) >> 5] & (1 << ((UINT32)(IRQn) & 0x1F)))?1:0));
}

/*******************************************************************************
* Function Name  : PFIC_SetPriority
* Description    : Set Interrupt Priority
* Input          : IRQn: Interrupt Numbers
*                  priority: bit7:pre-emption priority
*                            bit6-bit4: subpriority
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_SetPriority(IRQn_Type IRQn, UINT8 priority){
    PFIC->IPRIOR[(UINT32)(IRQn)] = priority;
}


/*******************************************************************************
* Function Name  : PFIC_EnableFastINTx
* Description    : Set fast Interrupt x,
* Input          : IRQn: Interrupt Numbers
*                  addr: interrupt service addr
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_EnableFastINT0(IRQn_Type IRQn, UINT32 addr)
{
    PFIC->IDCFGR[0] =  IRQn;
    PFIC->FIADDRR[0] = (addr&0xFFFFFFFE) | 1;
}
RV_STATIC_INLINE void PFIC_EnableFastINT1(IRQn_Type IRQn, UINT32 addr)
{
    PFIC->IDCFGR[1] = IRQn;
    PFIC->FIADDRR[1] = (addr&0xFFFFFFFE) | 1;
}
RV_STATIC_INLINE void PFIC_EnableFastINT2(IRQn_Type IRQn, UINT32 addr)
{
    PFIC->IDCFGR[2] = IRQn;
    PFIC->FIADDRR[2] = (addr&0xFFFFFFFE) | 1;
}
RV_STATIC_INLINE void PFIC_EnableFastINT3(IRQn_Type IRQn, UINT32 addr)
{
    PFIC->IDCFGR[3] = IRQn;
    PFIC->FIADDRR[3] = (addr&0xFFFFFFFE) | 1;
}

/*******************************************************************************
* Function Name  : PFIC_DisableFastINTx
* Description    : Disable fast Interrupt x,
* Input          : None
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_DisableFastINT0(void)
{
    PFIC->FIADDRR[0] = PFIC->FIADDRR[0]&0xFFFFFFFE;
}
RV_STATIC_INLINE void PFIC_DisableFastINT1(void)
{
    PFIC->FIADDRR[1] = PFIC->FIADDRR[1]&0xFFFFFFFE;
}
RV_STATIC_INLINE void PFIC_DisableFastINT2(void)
{
    PFIC->FIADDRR[2] = PFIC->FIADDRR[2]&0xFFFFFFFE;
}
RV_STATIC_INLINE void PFIC_DisableFastINT3(void)
{
    PFIC->FIADDRR[3] = PFIC->FIADDRR[3]&0xFFFFFFFE;
}

/*******************************************************************************
* Function Name  : __SEV
* Description    : Wait for Events
* Input          : None
* Return         : None
*******************************************************************************/
__attribute__( ( always_inline ) ) RV_STATIC_INLINE void __SEV(void){
    PFIC->SCTLR |= (1<<3);
}

/*******************************************************************************
* Function Name  : __WFI
* Description    : Wait for Interrupt
* Input          : None
* Return         : None
*******************************************************************************/
__attribute__( ( always_inline ) ) RV_STATIC_INLINE void __WFI(void){
    PFIC->SCTLR &= ~(1<<3);	// wfi
    asm volatile ("wfi");
}

/*******************************************************************************
* Function Name  : __WFE
* Description    : Wait for Events
* Input          : None
* Return         : None
*******************************************************************************/
__attribute__( ( always_inline ) ) RV_STATIC_INLINE void __WFE(void){
    PFIC->SCTLR |= (1<<3)|(1<<5);		// (wfi->wfe)+(__sev)
    asm volatile ("wfi");
    PFIC->SCTLR |= (1<<3);
    asm volatile ("wfi");
}

/*******************************************************************************
* Function Name  : PFIC_SystemReset
* Description    : Initiate a system reset request
* Input          : None
* Return         : None
*******************************************************************************/
RV_STATIC_INLINE void PFIC_SystemReset(void){
    PFIC->CFGR = PFIC_KEY3|(1<<7);
}


#define SysTick_LOAD_RELOAD_Msk            (0xFFFFFFFFFFFFFFFF)
#define SysTick_CTLR_INIT                  (1 << 5)
#define SysTick_CTLR_MODE                  (1 << 4)
#define SysTick_CTLR_STRE                  (1 << 3)
#define SysTick_CTLR_STCLK                 (1 << 2)
#define SysTick_CTLR_STIE                  (1 << 1)
#define SysTick_CTLR_STE                   (1 << 0)

#define SysTick_SR_CNTIF                   (1 << 0)


RV_STATIC_INLINE UINT32 SysTick_Config( UINT64 ticks ){
  if ((ticks - 1) > SysTick_LOAD_RELOAD_Msk)  return (1);      /* Reload value impossible */

  SysTick->CMP  = ticks - 1;                                  /* set reload register */
  PFIC_EnableIRQ( SysTick_IRQn );
  SysTick->CTLR  = SysTick_CTLR_INIT          |
                   SysTick_CTLR_STRE          |
                   SysTick_CTLR_STCLK         |
                   SysTick_CTLR_STIE          |
                   SysTick_CTLR_STE;                           /* Enable SysTick IRQ and SysTick Timer */
  return (0);                                                  /* Function successful */
}


#ifdef __cplusplus
}
#endif


#endif/* __CORE_RV3A_H__ */





