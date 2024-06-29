#include <stdio.h>
#include <rtthread.h>
#include <sys/time.h>
#include <delay.h>
#include "usb_musb_reg.h"
#include "usbd_core.h"
#include "uotg.h"

#define CLKRST_BASE                     0x60055000
#define CLKRST_UOTG_CTRL                (CLKRST_BASE + 0x318)
#define CLKRST_IRQ                      (CLKRST_BASE + 0x780)
#define CLKRST_SW_RESET1                (CLKRST_BASE + 0x604)
#define CLKRST_SW_RESET4                (CLKRST_BASE + 0x610)
#define VIM_USB2_PHYCLK_CONFIG	(CLKRST_BASE + 0x0440)	
#define MUSB_PHY_REG_SAMPLE_ID   0x400
#define MUSB_ID_PIN_MODE         0x040
#define SRC_MC		  (1 << 0)
#define SRC_DVBUS	  (1 << 3)
#define SRC_UDVBUS	  (1 << 4)
#define SRC_BWIRE_OUT (1 << 6)
#define SRC_BWIRE_IN  (1 << 7)
#define SRC_AWIRE_IN  (1 << 8)
/*
 * Old style IO functions
 */
static uint32_t musb_readl(uint32_t addr, uint32_t offset)
{
	uint32_t data = (*(volatile unsigned int *)(addr + offset));
	return data;
}
static void musb_writel(uint32_t addr, uint32_t offset,uint32_t data)
{
	(*(volatile unsigned int *)(addr + offset) = (unsigned int)(data));
}
static uint32_t vim_write_reg_bit_val(uint32_t adr, uint32_t sbit, uint32_t ebit, uint32_t val)
{
    uint32_t memVal;
    uint32_t tmp, tmp1;

	memVal = (*(volatile unsigned int *)(adr));
    if (sbit > ebit)
        return memVal;

    tmp1 = (1 << (ebit - sbit + 1)) - 1;
    val = val & tmp1;
    tmp1 <<= sbit;
    tmp =  memVal & (~tmp1);
    tmp |= (val << sbit) & tmp1;

	(*(volatile unsigned int *)(adr) = (unsigned int)(tmp));

    return memVal;
}
static uint32_t	vim_read_reg_bit_check(uint32_t adr, uint32_t bit)
{
	uint32_t val;
	val = (*(volatile unsigned int *)(adr));
	val = (val >> bit) & 0x01;
	return val;
}
static void vc0728_musb_enable_clk(void)  
{		
	uint32_t j=10;//1s

	vim_write_reg_bit_val(CLKRST_UOTG_CTRL, 9, 12, 0x0F);
	vim_write_reg_bit_val(CLKRST_UOTG_CTRL, 0, 7, 0x33);
	vim_write_reg_bit_val(CLKRST_UOTG_CTRL, 13, 13, 0);
	vim_write_reg_bit_val(CLKRST_UOTG_CTRL, 14, 14, 0);
	vim_write_reg_bit_val(CLKRST_UOTG_CTRL, 8, 8, 0);
	vim_write_reg_bit_val(VIM_USB2_PHYCLK_CONFIG, 25, 25, 0);
	vim_write_reg_bit_val(VIM_USB2_PHYCLK_CONFIG, 27, 27, 0);
	vim_write_reg_bit_val(CLKRST_IRQ, 15,15, 1);
	vim_write_reg_bit_val(CLKRST_SW_RESET1, 25, 25, 1);//为了防止global rst之后，来不了中断导致挂死
	vim_write_reg_bit_val(CLKRST_SW_RESET4, 12, 12, 1);
	
	//wait for uotg swrstdone irq
	while(j--){
		if (vim_read_reg_bit_check(CLKRST_IRQ, 15))
			break;
		msleep(100);
	}
	vim_write_reg_bit_val(CLKRST_IRQ, 15,15, 1);

 }

 void usb_dc_low_level_init(void)
{
	//enable pin 
	musb_writel(0x60054000,0x08,0x2700);
    vc0728_musb_enable_clk();
	// Enable ID Sampling.	
	musb_writel(PA_S6_UOTGC_BASE, MUSB_PHY_REG_SAMPLE_ID,musb_readl(PA_S6_UOTGC_BASE,MUSB_PHY_REG_SAMPLE_ID) | 0x02);
	//enable irq
	musb_writel(PA_S6_UOTGC_BASE,0x10,0x1f);
	musb_writel(PA_S6_UOTGC_BASE,0x14,0x1e);
	musb_writel(PA_S6_UOTGC_BASE,0x1c,0xf7);
	musb_writel(PA_S6_UOTGC_BASE,0x394,0x1ff); 
}
void VimMusbIrqHandler(int irq, void *param)
{
	uint32_t 	intpnd;

    intpnd = musb_readl(USB_BASE,MUSB_SRC_PND);
	musb_writel(USB_BASE,MUSB_SRC_PND,intpnd);

	if (intpnd & SRC_AWIRE_IN) {
	
	}
	
	if (intpnd & SRC_BWIRE_IN) {

	}
	
	if (intpnd & SRC_MC) {				
        USBD_IRQHandler();
	}

	if (intpnd & SRC_DVBUS) {

    }

    if (intpnd & SRC_UDVBUS) {

    }	
}