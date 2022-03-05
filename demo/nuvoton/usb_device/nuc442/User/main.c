/******************************************************************************
 * @file     main.c
 * @brief    Demonstrate how to implement a USB virtual com port device.
 * @version  2.0.0
 * @date     22, Sep, 2014
 *
 * @note
 * Copyright (C) 2014 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#include <stdio.h>
#include "NUC472_442.h"

/*--------------------------------------------------------------------------*/
void SYS_Init(void)
{
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable External XTAL (4~24 MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);

    /* Waiting for 12MHz clock ready */
    CLK_WaitClockReady( CLK_STATUS_HXTSTB_Msk);

    /* Switch HCLK clock source to HXT */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HXT,CLK_CLKDIV0_HCLK(1));

    /* Set PLL to power down mode and PLL_STB bit in CLKSTATUS register will be cleared by hardware.*/
    CLK->PLLCTL |= CLK_PLLCTL_PD_Msk;

    /* Set PLL frequency */
    CLK->PLLCTL = CLK_PLLCTL_84MHz_HXT;

    /* Waiting for clock ready */
    CLK_WaitClockReady(CLK_STATUS_PLLSTB_Msk);

    /* Switch HCLK clock source to PLL */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_PLL,CLK_CLKDIV0_HCLK(1));

    /* Select IP clock source */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));

    /* Enable IP clock */
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(USBD_MODULE);

    /* Enable USB PHY */
    SYS->USBPHY = 0x100;  // USB device

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set GPG multi-function pins for UART0 RXD and TXD (15, 16) */
    SYS->GPG_MFPL &= ~(SYS_GPG_MFPL_PG1MFP_Msk | SYS_GPG_MFPL_PG2MFP_Msk);
    SYS->GPG_MFPL |= (SYS_GPG_MFPL_PG1MFP_UART0_RXD | SYS_GPG_MFPL_PG2MFP_UART0_TXD);

    /* Lock protected registers */
    SYS_LockReg();
}

void DelayMs(uint32_t ulMs)
{
    uint32_t i;
    uint32_t j;
    for(i = ulMs; i > 0; i--)
    {
        for(j = 4700; j > 0; j--);
    }
}

/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function                                                                                          */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main (void)
{
    SYS_Init();
    UART_Open(UART0, 115200);

    printf("NuMicro USB CDC VCOM\n");
    extern void cdc_acm_init(void);
    
    cdc_acm_init();
    NVIC_EnableIRQ(USBD_IRQn);

    while(1)
    {
        extern void cdc_acm_data_send_with_dtr_test();
        cdc_acm_data_send_with_dtr_test();
        DelayMs(100);

    }
}



/*** (C) COPYRIGHT 2013 Nuvoton Technology Corp. ***/

