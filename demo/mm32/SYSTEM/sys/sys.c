/**
******************************************************************************
* @file    sys.c
* @author  AE Team
* @version V1.3.9
* @date    28/08/2019
* @brief   This file provides all the sys firmware functions.
******************************************************************************
* @copy
*
* THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
* WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
* TIME. AS A RESULT, MindMotion SHALL NOT BE HELD LIABLE FOR ANY
* DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
* FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
* CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* <h2><center>&copy; COPYRIGHT 2019 MindMotion</center></h2>
*/
#include "sys.h"

extern u32 SystemCoreClock;
//设置向量表偏移地址
//NVIC_VectTab:基址
//Offset:偏移量
void MY_NVIC_SetVectorTable(u32 NVIC_VectTab, u32 Offset)
{
    SCB->VTOR = NVIC_VectTab | (Offset & (u32)0x1FFFFF80); //设置NVIC的向量表偏移寄存器
    //用于标识向量表是在CODE区还是在RAM区
}
//设置NVIC分组
//NVIC_Group:NVIC分组 0~4 总共5组
void MY_NVIC_PriorityGroupConfig(u8 NVIC_Group)
{
    u32 temp, temp1;
    temp1 = (~NVIC_Group) & 0x07; //取后三位
    temp1 <<= 8;
    temp = SCB->AIRCR; //读取先前的设置
    temp &= 0X0000F8FF; //清空先前分组
    temp |= 0X05FA0000; //写入钥匙
    temp |= temp1;
    SCB->AIRCR = temp; //设置分组
}
//设置NVIC
//NVIC_PreemptionPriority:抢占优先级
//NVIC_SubPriority       :响应优先级
//NVIC_Channel           :中断编号
//NVIC_Group             :中断分组 0~4
//注意优先级不能超过设定的组的范围!否则会有意想不到的错误
//组划分:
//组0:0位抢占优先级,4位响应优先级
//组1:1位抢占优先级,3位响应优先级
//组2:2位抢占优先级,2位响应优先级
//组3:3位抢占优先级,1位响应优先级
//组4:4位抢占优先级,0位响应优先级
//NVIC_SubPriority和NVIC_PreemptionPriority的原则是,数值越小,越优先
void MY_NVIC_Init(u8 NVIC_PreemptionPriority, u8 NVIC_SubPriority, u8 NVIC_Channel, u8 NVIC_Group)
{
    u32 temp;
    MY_NVIC_PriorityGroupConfig(NVIC_Group);//设置分组
    temp = NVIC_PreemptionPriority << (4 - NVIC_Group);
    temp |= NVIC_SubPriority & (0x0f >> NVIC_Group);
    temp &= 0xf;								//取低四位
    NVIC->ISER[NVIC_Channel / 32] |= (1 << NVIC_Channel % 32); //使能中断位(要清除的话,相反操作就OK)
    NVIC->IP[NVIC_Channel] |= temp << 4;		//设置响应优先级和抢断优先级
}
//外部中断配置函数
//只针对GPIOA~G;不包括PVD,RTC和USB唤醒这三个
//参数:
//GPIOx:0~6,代表GPIOA~G
//BITx:需要使能的位;
//TRIM:触发模式,1,下升沿;2,上降沿;3，任意电平触发
//该函数一次只能配置1个IO口,多个IO口,需多次调用
//该函数会自动开启对应中断,以及屏蔽线
void Ex_NVIC_Config(u8 GPIOx, u8 BITx, u8 TRIM)
{
    u8 EXTADDR;
    u8 EXTOFFSET;
    EXTADDR = BITx / 4; //得到中断寄存器组的编号
    EXTOFFSET = (BITx % 4) * 4;
    RCC->APB2ENR |= 0x01; //使能io复用时钟
    AFIO->EXTICR[EXTADDR] &= ~(0x000F << EXTOFFSET); //清除原来设置！！！
    AFIO->EXTICR[EXTADDR] |= GPIOx << EXTOFFSET; //EXTI.BITx映射到GPIOx.BITx
    //自动设置
    EXTI->IMR |= 1 << BITx; //  开启line BITx上的中断
    //EXTI->EMR|=1<<BITx;//不屏蔽line BITx上的事件 (如果不屏蔽这句,在硬件上是可以的,但是在软件仿真的时候无法进入中断!)
    if(TRIM & 0x01)EXTI->FTSR |= 1 << BITx; //line BITx上事件下降沿触发
    if(TRIM & 0x02)EXTI->RTSR |= 1 << BITx; //line BITx上事件上升降沿触发
}
//不能在这里执行所有外设复位!否则至少引起串口不工作.
//把所有时钟寄存器复位
void MYRCC_DeInit(void)
{
    RCC->APB1RSTR = 0x00000000;//复位结束
    RCC->APB2RSTR = 0x00000000;

    RCC->AHBENR = 0x00000014;  //睡眠模式闪存和SRAM时钟使能.其他关闭.
    RCC->APB2ENR = 0x00000000; //外设时钟关闭.
    RCC->APB1ENR = 0x00000000;
    RCC->CR |= 0x00000001;     //使能内部高速时钟HSION
    RCC->CFGR &= 0xF8FF0000;   //复位SW[1:0],HPRE[3:0],PPRE1[2:0],PPRE2[2:0],ADCPRE[1:0],MCO[2:0]
    RCC->CR &= 0xFEF6FFFF;     //复位HSEON,CSSON,PLLON
    RCC->CR &= 0xFFFBFFFF;     //复位HSEBYP
    RCC->CFGR &= 0xFF80FFFF;   //复位PLLSRC, PLLXTPRE, PLLMUL[3:0] and USBPRE
    RCC->CIR = 0x00000000;     //关闭所有中断
    //配置向量表
#ifdef  VECT_TAB_RAM
    MY_NVIC_SetVectorTable(0x20000000, 0x0);
#else
    MY_NVIC_SetVectorTable(0x08000000, 0x0);
#endif
}
//THUMB指令不支持汇编内联
//采用如下方法实现执行汇编指令WFI
void WFI_SET(void)
{
    __ASM volatile("wfi");
}
//关闭所有中断
void INTX_DISABLE(void)
{
    __ASM volatile("cpsid i");
}
//开启所有中断
void INTX_ENABLE(void)
{
    __ASM volatile("cpsie i");
}


//进入待机模式
void Sys_Standby(void)
{
    SCB->SCR |= SCB_SCR_SLEEPDEEP; //使能SLEEPDEEP位 (SYS->CTRL)
    RCC->APB1ENR |= RCC_APB1RSTR_PWRRST;   //使能电源时钟
    PWR->CSR |= PWR_CSR_EWUP;        //设置WKUP用于唤醒
    PWR->CR |= PWR_CR_CWUF;         //清除Wake-up 标志
    PWR->CR |= PWR_CR_PDDS;         //PDDS置位
    WFI_SET();				 //执行WFI指令
}
//系统软复位
void Sys_Soft_Reset(void)
{
    SCB->AIRCR = 0X05FA0000 | (u32)0x04;
}
//JTAG模式设置,用于设置JTAG的模式
//mode:jtag,swd模式设置;00,全使能;01,使能SWD;10,全关闭;
//#define JTAG_SWD_DISABLE   0X02
//#define SWD_ENABLE         0X01
//#define JTAG_SWD_ENABLE    0X00
void JTAG_Set(u8 mode)
{
    u32 temp;
    temp = mode;
    temp <<= 25;
    RCC->APB2ENR |= RCC_APB2RSTR_AFIORST;   //开启辅助时钟
    AFIO->MAPR &= 0XF8FFFFFF; //清除MAPR的[26:24]
    AFIO->MAPR |= temp;     //设置jtag模式
}
//系统时钟初始化函数
//pll:选择的倍频数，从2开始，最大值为16
void System_Clock_Init(u8 PLL)
{
    unsigned char temp = 0;
    MYRCC_DeInit();		  //复位并配置向量表
    RCC->CR |= RCC_CR_HSEON; //外部高速时钟使能HSEON
    while(!(RCC->CR & RCC_CR_HSERDY)); //等待外部时钟就绪
    RCC->CFGR = RCC_CFGR_PPRE1_2; //APB1=DIV2;APB2=DIV1;AHB=DIV1;

    RCC->CFGR |= RCC_CFGR_PLLSRC;	 //PLLSRC ON
    RCC->CR &= ~(RCC_CR_PLLON);		//清PLL//	RCC->CR &=~(7<<20);		//清PLL

    RCC->CR &= ~(0x1f << 26);
    RCC->CR |= (PLL - 1) << 26; //设置PLL值 2~16

    FLASH->ACR |= FLASH_ACR_LATENCY_1 | FLASH_ACR_PRFTBE | FLASH_ACR_PRFTBS;	 //FLASH 2个延时周期

    RCC->CR |= RCC_CR_PLLON; //PLLON
    while(!(RCC->CR & RCC_CR_PLLRDY)); //等待PLL锁定
    RCC->CFGR |= RCC_CFGR_SW_PLL; //PLL作为系统时钟
    while(temp != 0x02)   //等待PLL作为系统时钟设置成功
    {
        temp = RCC->CFGR >> 2;
        temp &= 0x03;
    }

}
/*

HSE 外部时钟 8M

*/

#if 0
void RCC_MCOConfig(uint8_t RCC_MCO)
{
    /* Check the parameters */
    assert_param(IS_RCC_MCO(RCC_MCO));
    /* Perform Byte access to MCO[2:0] bits to select the MCO source */
    *(__IO uint8_t *) CFGR_BYTE4_ADDRESS = RCC_MCO;
}
#endif
void SystemClk_Output(void)
{

    //	GPIO_InitTypeDef  GPIO_InitStructure;
    //	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    //	GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_8;   //mco  pa8
    //	GPIO_InitStructure.GPIO_Speed =  GPIO_Speed_50MHz;
    //  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 推免复用输出
    //	GPIO_Init(GPIOA, &GPIO_InitStructure);

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;  //使能PORTA时钟
    GPIOA->CRH &= 0XFFFFFFF0;
    GPIOA->CRH |= GPIO_CRH_MODE8; //PA8 推挽输出
    GPIOA->ODR |= GPIO_ODR_ODR8;    //PA8 输出高


    //------------------------------------------------------------
    //add start
    /*
    #define  RCC_CFGR_MCO_2                      ((uint32_t)0x04000000)
    位26:24
    MCO： 微控制器时钟输出
    由软件置’1’或清零。
    00x：没有时钟输出；
    010：LSI 时钟输出；
    011：LSE 时钟输出；
    100：系统时钟(SYSCLK)输出；
    101：HSI 时钟输出；
    110：HSE 时钟输出；
    111：PLL 时钟2分频后输出。
    注意：
    - 该时钟输出在启动和切换MCO时钟源时可能会被截断。
    - 在系统时钟作为输出至MCO管脚时，请保证输出时钟频率不超过50MHz (IO口最高频率)
    */

    //	RCC->CR &= (uint32_t)~RCC_CFGR_MCO;  //CLEAR pll PARAMETERS
    //-------------------------------------------------
    //	RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_PLL ;//PLL/2
    if(SystemCoreClock < 8000000)
    {
        //RCC_MCOConfig(RCC_MCO_HSE);  //通过PA8 pin 观察频率
        RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_HSE ;//PLL/2
    }
    else if(SystemCoreClock <= 48000000)
    {
        //RCC_MCOConfig(RCC_MCO_SYSCLK);  //通过PA8 pin 观察频率 SYSCLK<48M
        RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_SYSCLK ;//PLL/2
    }
    else
    {
        //RCC_MCOConfig(RCC_MCO_PLLCLK_Div2);  //通过PA8 pin 观察频率
        RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_PLL ;//PLL/2
    }
}

/**
* @}
*/

/**
* @}
*/

/**
* @}
*/

/*-------------------------(C) COPYRIGHT 2019 MindMotion ----------------------*/







