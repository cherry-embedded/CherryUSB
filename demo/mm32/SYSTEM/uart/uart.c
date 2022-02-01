/**
******************************************************************************
* @file    uart_usb.c
* @author  AE Team
* @version V1.3.9
* @date    28/08/2019
* @brief   This file provides all the uart_usb firmware functions.
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
#include "uart.h"

#ifdef __GNUC__

#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)

#endif

#ifdef USE_IAR
PUTCHAR_PROTOTYPE
{
    while((UART1->CSR & 0x0001) == 0); //ѭ������,ֱ���������
    UART1->TDR = (ch & (uint16_t)0x00FF);
    return ch;
}

#else
#pragma import(__use_no_semihosting)
//��׼����Ҫ��֧�ֺ���
struct __FILE
{
    int handle;

};

FILE __stdout;
//����_sys_exit()�Ա���ʹ�ð�����ģʽ
int _sys_exit(int x)
{
    x = x;
}
//�ض���fputc����
int fputc(int ch, FILE *f)
{
    while((UART1->CSR & 0x0001) == 0); //ѭ������,ֱ���������
    UART1->TDR = (ch & (uint16_t)0x00FF);
    return ch;
}
#endif

//////////////////////////////////////////////////////////////////


void uart_sendByte(unsigned char data)
{
    while((UART1->CSR & UART_IT_TXIEN) == 0); //ѭ������,ֱ���������
    UART1->TDR = data;
}

void uart_sendArray(unsigned char *dataBuf, unsigned int len)
{
    while(len)
    {
        uart_sendByte(*dataBuf);
        len--;
        dataBuf++;
    }
}

#if EN_UART1_RX   //���ʹ���˽���
//����1�жϷ������
//ע��,��ȡUARTx->SR�ܱ���Ī������Ĵ���
u8 UART_RX_BUF[UART_REC_LEN];     //���ջ���,���UART_REC_LEN���ֽ�.
//����״̬
//bit15��	������ɱ�־
//bit14��	���յ�0x0d
//bit13~0��	���յ�����Ч�ֽ���Ŀ
u16 UART_RX_STA = 0;     //����״̬���

//��ʼ��IO ����1
//pclk2:PCLK2ʱ��Ƶ��(Mhz)
//bound:������
void uart_initwBaudRate(u32 pclk2, u32 bound)
{

    u32 tempBaud;

    //-------------------------------------------------
    RCC->APB2ENR |= 1 << 2; //ʹ��PORTA��ʱ��   #define RCC_APB2Periph_UART1            ((uint32_t)0x00004000) #define RCC_APB2Periph_GPIOA             ((uint32_t)0x00000004)
    /*
    APB2����ʱ��ʹ�ܼĴ�����RCC_APB2ENR��
    ƫ�Ƶ�ַ��0x18
    ��λֵ��0x0000 0000
    ���ʣ��޵ȴ����ڣ��֣����ֺ��ֽڷ���
    ע��������ʱ��û������ʱ��������ܶ�������Ĵ�������ֵ
    λ2
    IOPAEN��IO�˿�Aʱ��ʹ��
    ������á�1�����塯0��
    0��IO�˿�Aʱ�ӹرգ�
    1��IO�˿�Aʱ�ӿ�����
    */
    RCC->APB2ENR |= 1 << 14; //ʹ�ܴ���ʱ��
    /*
    λ14
    UART1EN��UART1ʱ��ʹ��
    ������á�1�����塯0��
    0��UART1ʱ�ӹرգ�
    1��UART1ʱ�ӿ�����
    */
    GPIOA->CRH &= 0XFFFFF00F; //IO״̬����
    GPIOA->CRH |= 0X000008B0; //IO״̬����
    /*
    8.2.2 �˿����ø߼Ĵ�����GPIOx_CRH��(x=A..E)
    ƫ�Ƶ�ַ��0x04
    ��λֵ��0x4444 4444
    ����PA9Ϊ B = 10 11  MODE9[1:0] = 11[5:4] �����ģʽ������ٶ�50MHz, CNF9[1:0] = 10[7:6] �����ù����������ģʽ
    ����PA10 Ϊ8 = 10 00 MODE10[1:0] = 00[9:8] ������ģʽ����λ���״̬��, CNF10[1:0] = 10[7:6] ��10������/��������ģʽ

    MODEy[1:0]:�˿�x��ģʽλ��y=8��15��
    ���ͨ����Щλ������Ӧ��I/O�˿ڣ���ο���15�˿�λ���ñ�
    00������ģʽ����λ���״̬��
    01�����ģʽ������ٶ�10MHz
    10�����ģʽ������ٶ�20MHz
    11�����ģʽ������ٶ�50MHz

    CNFy[1:0]:�˿�x����λ��8��15��
    ���ͨ����Щλ������Ӧ��I/O�˿ڣ���ο���15�˿�λ���ñ�
    ������ģʽ��MODE[1:0]==00��:
    00��ģ������ģʽ
    01����������ģʽ
    10������/��������ģʽ
    11������
    �����ģʽ��MODE[1:0]>00��:
    00��ͨ���������ģʽ
    01��ͨ�ÿ�©���ģʽ
    10�����ù����������ģʽ
    11�����ù��ܿ�©���ģʽ

    */
    RCC->APB2RSTR |= 1 << 14; //��λ����1
    RCC->APB2RSTR &= ~(1 << 14); //ֹͣ��λ
    //-------------------------------------------------
    //����������
    // 	UART1->BRR=mantissa; // ����������
    /* Determine the uart_baud*/
    tempBaud = (pclk2 * 1000000 * 10 / 16) / (bound);
    if((tempBaud % 5) > 4)
    {
        tempBaud = (tempBaud + 10) / 10;
    }
    else
    {
        tempBaud = tempBaud / 10;
    }
    /* Write to UART BRR */
    UART1->BRR = tempBaud;
    UART1->CCR |= 0X30; //1λֹͣ,��У��λ.
    //-------------------------------------------------
#if EN_UART1_RX		  //���ʹ���˽���
    //ʹ�ܽ����ж�
    UART1->GCR = 0X19;			//�շ�ʹ��	UART1->CCR|=1<<5;    //���ջ������ǿ��ж�ʹ��
    UART1->IER = 0X2;			//�����ж�ʹ��
    /*
    23.5.5 UART �ж�ʹ�ܼĴ���(UART_IER)
    ƫ�Ƶ�ַ��0x10
    ��λֵ��0x0000
    λ1
    RXIEN:���ջ����ж�ʹ��λ
    1=�ж�ʹ��
    0=�жϽ�ֹ
    */
    UART1->ICR = 0X2;			//������ж�
    /*
    23.5.6 UART �ж�����Ĵ���(UART_ICR)
    ƫ�Ƶ�ַ��0x14
    ��λֵ��0x0000
    λ1
    RXICLR: �����ж����λ
    1=�ж����
    0=�ж�û�����
    */
    MY_NVIC_Init(3, 3, UART1_IRQn, 2); //��2��������ȼ�
#endif
}

unsigned int curUartRxLenth = 0;

void UART1_IRQHandler(void)
{

    if((UART1->ISR & UART_IT_RXIEN) != (uint16_t)RESET)	//���յ�����
    {
        UART1->ICR |= 2;//������ж� //		UART_ClearITPendingBit(UART1,UART_IT_RXIEN);
        UART_RX_BUF[curUartRxLenth] = UART1->RDR;
        curUartRxLenth ++;

        if(curUartRxLenth > 199) {curUartRxLenth = 0;}
    }
}
#endif

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
