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
//����������ƫ�Ƶ�ַ
//NVIC_VectTab:��ַ
//Offset:ƫ����
void MY_NVIC_SetVectorTable(u32 NVIC_VectTab, u32 Offset)
{
    SCB->VTOR = NVIC_VectTab | (Offset & (u32)0x1FFFFF80); //����NVIC��������ƫ�ƼĴ���
    //���ڱ�ʶ����������CODE��������RAM��
}
//����NVIC����
//NVIC_Group:NVIC���� 0~4 �ܹ�5��
void MY_NVIC_PriorityGroupConfig(u8 NVIC_Group)
{
    u32 temp, temp1;
    temp1 = (~NVIC_Group) & 0x07; //ȡ����λ
    temp1 <<= 8;
    temp = SCB->AIRCR; //��ȡ��ǰ������
    temp &= 0X0000F8FF; //�����ǰ����
    temp |= 0X05FA0000; //д��Կ��
    temp |= temp1;
    SCB->AIRCR = temp; //���÷���
}
//����NVIC
//NVIC_PreemptionPriority:��ռ���ȼ�
//NVIC_SubPriority       :��Ӧ���ȼ�
//NVIC_Channel           :�жϱ��
//NVIC_Group             :�жϷ��� 0~4
//ע�����ȼ����ܳ����趨����ķ�Χ!����������벻���Ĵ���
//�黮��:
//��0:0λ��ռ���ȼ�,4λ��Ӧ���ȼ�
//��1:1λ��ռ���ȼ�,3λ��Ӧ���ȼ�
//��2:2λ��ռ���ȼ�,2λ��Ӧ���ȼ�
//��3:3λ��ռ���ȼ�,1λ��Ӧ���ȼ�
//��4:4λ��ռ���ȼ�,0λ��Ӧ���ȼ�
//NVIC_SubPriority��NVIC_PreemptionPriority��ԭ����,��ֵԽС,Խ����
void MY_NVIC_Init(u8 NVIC_PreemptionPriority, u8 NVIC_SubPriority, u8 NVIC_Channel, u8 NVIC_Group)
{
    u32 temp;
    MY_NVIC_PriorityGroupConfig(NVIC_Group);//���÷���
    temp = NVIC_PreemptionPriority << (4 - NVIC_Group);
    temp |= NVIC_SubPriority & (0x0f >> NVIC_Group);
    temp &= 0xf;								//ȡ����λ
    NVIC->ISER[NVIC_Channel / 32] |= (1 << NVIC_Channel % 32); //ʹ���ж�λ(Ҫ����Ļ�,�෴������OK)
    NVIC->IP[NVIC_Channel] |= temp << 4;		//������Ӧ���ȼ����������ȼ�
}
//�ⲿ�ж����ú���
//ֻ���GPIOA~G;������PVD,RTC��USB����������
//����:
//GPIOx:0~6,����GPIOA~G
//BITx:��Ҫʹ�ܵ�λ;
//TRIM:����ģʽ,1,������;2,�Ͻ���;3�������ƽ����
//�ú���һ��ֻ������1��IO��,���IO��,���ε���
//�ú������Զ�������Ӧ�ж�,�Լ�������
void Ex_NVIC_Config(u8 GPIOx, u8 BITx, u8 TRIM)
{
    u8 EXTADDR;
    u8 EXTOFFSET;
    EXTADDR = BITx / 4; //�õ��жϼĴ�����ı��
    EXTOFFSET = (BITx % 4) * 4;
    RCC->APB2ENR |= 0x01; //ʹ��io����ʱ��
    AFIO->EXTICR[EXTADDR] &= ~(0x000F << EXTOFFSET); //���ԭ�����ã�����
    AFIO->EXTICR[EXTADDR] |= GPIOx << EXTOFFSET; //EXTI.BITxӳ�䵽GPIOx.BITx
    //�Զ�����
    EXTI->IMR |= 1 << BITx; //  ����line BITx�ϵ��ж�
    //EXTI->EMR|=1<<BITx;//������line BITx�ϵ��¼� (������������,��Ӳ�����ǿ��Ե�,��������������ʱ���޷������ж�!)
    if(TRIM & 0x01)EXTI->FTSR |= 1 << BITx; //line BITx���¼��½��ش���
    if(TRIM & 0x02)EXTI->RTSR |= 1 << BITx; //line BITx���¼��������ش���
}
//����������ִ���������踴λ!�����������𴮿ڲ�����.
//������ʱ�ӼĴ�����λ
void MYRCC_DeInit(void)
{
    RCC->APB1RSTR = 0x00000000;//��λ����
    RCC->APB2RSTR = 0x00000000;

    RCC->AHBENR = 0x00000014;  //˯��ģʽ�����SRAMʱ��ʹ��.�����ر�.
    RCC->APB2ENR = 0x00000000; //����ʱ�ӹر�.
    RCC->APB1ENR = 0x00000000;
    RCC->CR |= 0x00000001;     //ʹ���ڲ�����ʱ��HSION
    RCC->CFGR &= 0xF8FF0000;   //��λSW[1:0],HPRE[3:0],PPRE1[2:0],PPRE2[2:0],ADCPRE[1:0],MCO[2:0]
    RCC->CR &= 0xFEF6FFFF;     //��λHSEON,CSSON,PLLON
    RCC->CR &= 0xFFFBFFFF;     //��λHSEBYP
    RCC->CFGR &= 0xFF80FFFF;   //��λPLLSRC, PLLXTPRE, PLLMUL[3:0] and USBPRE
    RCC->CIR = 0x00000000;     //�ر������ж�
    //����������
#ifdef  VECT_TAB_RAM
    MY_NVIC_SetVectorTable(0x20000000, 0x0);
#else
    MY_NVIC_SetVectorTable(0x08000000, 0x0);
#endif
}
//THUMBָ�֧�ֻ������
//�������·���ʵ��ִ�л��ָ��WFI
void WFI_SET(void)
{
    __ASM volatile("wfi");
}
//�ر������ж�
void INTX_DISABLE(void)
{
    __ASM volatile("cpsid i");
}
//���������ж�
void INTX_ENABLE(void)
{
    __ASM volatile("cpsie i");
}


//�������ģʽ
void Sys_Standby(void)
{
    SCB->SCR |= SCB_SCR_SLEEPDEEP; //ʹ��SLEEPDEEPλ (SYS->CTRL)
    RCC->APB1ENR |= RCC_APB1RSTR_PWRRST;   //ʹ�ܵ�Դʱ��
    PWR->CSR |= PWR_CSR_EWUP;        //����WKUP���ڻ���
    PWR->CR |= PWR_CR_CWUF;         //���Wake-up ��־
    PWR->CR |= PWR_CR_PDDS;         //PDDS��λ
    WFI_SET();				 //ִ��WFIָ��
}
//ϵͳ��λ
void Sys_Soft_Reset(void)
{
    SCB->AIRCR = 0X05FA0000 | (u32)0x04;
}
//JTAGģʽ����,��������JTAG��ģʽ
//mode:jtag,swdģʽ����;00,ȫʹ��;01,ʹ��SWD;10,ȫ�ر�;
//#define JTAG_SWD_DISABLE   0X02
//#define SWD_ENABLE         0X01
//#define JTAG_SWD_ENABLE    0X00
void JTAG_Set(u8 mode)
{
    u32 temp;
    temp = mode;
    temp <<= 25;
    RCC->APB2ENR |= RCC_APB2RSTR_AFIORST;   //��������ʱ��
    AFIO->MAPR &= 0XF8FFFFFF; //���MAPR��[26:24]
    AFIO->MAPR |= temp;     //����jtagģʽ
}
//ϵͳʱ�ӳ�ʼ������
//pll:ѡ��ı�Ƶ������2��ʼ�����ֵΪ16
void System_Clock_Init(u8 PLL)
{
    unsigned char temp = 0;
    MYRCC_DeInit();		  //��λ������������
    RCC->CR |= RCC_CR_HSEON; //�ⲿ����ʱ��ʹ��HSEON
    while(!(RCC->CR & RCC_CR_HSERDY)); //�ȴ��ⲿʱ�Ӿ���
    RCC->CFGR = RCC_CFGR_PPRE1_2; //APB1=DIV2;APB2=DIV1;AHB=DIV1;

    RCC->CFGR |= RCC_CFGR_PLLSRC;	 //PLLSRC ON
    RCC->CR &= ~(RCC_CR_PLLON);		//��PLL//	RCC->CR &=~(7<<20);		//��PLL

    RCC->CR &= ~(0x1f << 26);
    RCC->CR |= (PLL - 1) << 26; //����PLLֵ 2~16

    FLASH->ACR |= FLASH_ACR_LATENCY_1 | FLASH_ACR_PRFTBE | FLASH_ACR_PRFTBS;	 //FLASH 2����ʱ����

    RCC->CR |= RCC_CR_PLLON; //PLLON
    while(!(RCC->CR & RCC_CR_PLLRDY)); //�ȴ�PLL����
    RCC->CFGR |= RCC_CFGR_SW_PLL; //PLL��Ϊϵͳʱ��
    while(temp != 0x02)   //�ȴ�PLL��Ϊϵͳʱ�����óɹ�
    {
        temp = RCC->CFGR >> 2;
        temp &= 0x03;
    }

}
/*

HSE �ⲿʱ�� 8M

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
    //  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // ���⸴�����
    //	GPIO_Init(GPIOA, &GPIO_InitStructure);

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;  //ʹ��PORTAʱ��
    GPIOA->CRH &= 0XFFFFFFF0;
    GPIOA->CRH |= GPIO_CRH_MODE8; //PA8 �������
    GPIOA->ODR |= GPIO_ODR_ODR8;    //PA8 �����


    //------------------------------------------------------------
    //add start
    /*
    #define  RCC_CFGR_MCO_2                      ((uint32_t)0x04000000)
    λ26:24
    MCO�� ΢������ʱ�����
    ������á�1�������㡣
    00x��û��ʱ�������
    010��LSI ʱ�������
    011��LSE ʱ�������
    100��ϵͳʱ��(SYSCLK)�����
    101��HSI ʱ�������
    110��HSE ʱ�������
    111��PLL ʱ��2��Ƶ�������
    ע�⣺
    - ��ʱ��������������л�MCOʱ��Դʱ���ܻᱻ�ضϡ�
    - ��ϵͳʱ����Ϊ�����MCO�ܽ�ʱ���뱣֤���ʱ��Ƶ�ʲ�����50MHz (IO�����Ƶ��)
    */

    //	RCC->CR &= (uint32_t)~RCC_CFGR_MCO;  //CLEAR pll PARAMETERS
    //-------------------------------------------------
    //	RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_PLL ;//PLL/2
    if(SystemCoreClock < 8000000)
    {
        //RCC_MCOConfig(RCC_MCO_HSE);  //ͨ��PA8 pin �۲�Ƶ��
        RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_HSE ;//PLL/2
    }
    else if(SystemCoreClock <= 48000000)
    {
        //RCC_MCOConfig(RCC_MCO_SYSCLK);  //ͨ��PA8 pin �۲�Ƶ�� SYSCLK<48M
        RCC->CFGR |= (uint32_t)RCC_CFGR_MCO_SYSCLK ;//PLL/2
    }
    else
    {
        //RCC_MCOConfig(RCC_MCO_PLLCLK_Div2);  //ͨ��PA8 pin �۲�Ƶ��
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







