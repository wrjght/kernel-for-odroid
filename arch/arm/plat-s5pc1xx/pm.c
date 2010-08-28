/* linux/arch/arm/plat-s3c24xx/pm.c
 *
 * Copyright (c) 2004,2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * S3C24XX Power Manager (Suspend-To-RAM) support
 *
 * See Documentation/arm/Samsung-S3C24XX/Suspend.txt for more information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Parts based on arch/arm/mach-pxa/pm.c
 *
 * Thanks to Dimitry Andric for debugging
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/crc32.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <asm/cacheflush.h>
#include <mach/hardware.h>

#include <plat/map-base.h>
#include <plat/regs-serial.h>
#include <plat/regs-clock.h>
#include <plat/regs-power.h>
#include <plat/regs-gpio.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-mem.h>
#include <mach/regs-irq.h>
#include <asm/gpio.h>

#include <asm/mach/time.h>

#include <plat/pm.h>
#include <plat/regs-power.h>
#include <plat/s5pc1xx-dvfs.h>

#if defined(CONFIG_MACH_HKDKC100)	
	#include "hkc100_sleep.h"
#endif

#ifdef CONFIG_PM_PWR_GATING
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <mach/map.h>
#include <plat/regs-clock.h>

static DEFINE_MUTEX(power_lock);
static unsigned int s5p_domain_off_stat = 0x1FFC;
#endif
/* for external use */

#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
#include <linux/android_power.h>
extern void android_lock_partial_suspend_auto_expire(android_suspend_lock_t *lock, int timeout);
android_suspend_lock_t pm_idle_lock;
#endif
#elif CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
static struct wake_lock pm_wake_lock;
#endif
#ifdef CONFIG_LPAUDIO
extern void AudioSetModeLP(bool mode);
#endif
#if 1
#include <linux/uaccess.h>

#include <linux/stat.h>
#include <linux/proc_fs.h>

#define MODE_CFG	S5P_INFORM1

struct proc_dir_entry *proc_root_fp 	= NULL;

struct proc_dir_entry *proc_deep_command_fp 	= NULL;
char proc_deep_command_str[PAGE_SIZE-80] 	= { 0,};
#endif
int s5pc1xx_pm_eint_registered =0;
unsigned long s5pc1xx_pm_flags;
void __iomem *weint_base;
void __iomem *mp3reg;

enum PLL_TYPE
{
	PM_APLL,
	PM_MPLL,
	PM_EPLL,
	PM_HPLL
};

#define PFX "s5pc1xx-pm: "
static struct sleep_save core_save[] = {
	SAVE_ITEM(S5P_CLK_SRC0),
	SAVE_ITEM(S5P_CLK_SRC1),
	SAVE_ITEM(S5P_CLK_SRC2),
	SAVE_ITEM(S5P_CLK_SRC3),
	SAVE_ITEM(S5P_EPLL_CON),
	
	SAVE_ITEM(S5P_CLK_DIV0),
	SAVE_ITEM(S5P_CLK_DIV1),
	SAVE_ITEM(S5P_CLK_DIV2),
	SAVE_ITEM(S5P_CLK_DIV3),
	SAVE_ITEM(S5P_CLK_DIV4),

	SAVE_ITEM(S5P_CLK_OUT),

	SAVE_ITEM(S5P_CLKGATE_D00),
	SAVE_ITEM(S5P_CLKGATE_D01),
	SAVE_ITEM(S5P_CLKGATE_D02),

	SAVE_ITEM(S5P_CLKGATE_D10),
	SAVE_ITEM(S5P_CLKGATE_D11),
	SAVE_ITEM(S5P_CLKGATE_D12),
	SAVE_ITEM(S5P_CLKGATE_D13),
	SAVE_ITEM(S5P_CLKGATE_D14),
	SAVE_ITEM(S5P_CLKGATE_D15),

	SAVE_ITEM(S5P_CLKGATE_D20),

	SAVE_ITEM(S5P_SCLKGATE0),
	SAVE_ITEM(S5P_SCLKGATE1),

/*	SAVE_ITEM(S5P_MEM_SYS_CFG),		//register is not mapped to VA TODO
	SAVE_ITEM(S5P_CAM_MUX_SEL),
	SAVE_ITEM(S5P_MIXER_OUT_SEL),

	SAVE_ITEM(S5P_LPMP_MODE_SEL),
	SAVE_ITEM(S5P_MIPI_PHY_CON0),
	SAVE_ITEM(S5P_MIPI_PHY_CON1),
	SAVE_ITEM(S5P_HDMI_PHY_CON0),*/
};
#ifdef CONFIG_LPAUDIO
// see page 2-4-10
static struct sleep_save lpaudio_save[] = { //use VA
	//SAVE_ITEM(S5PC1XX_PA_LPMP3_MODE_SEL),
	//SAVE_ITEM(S3C64XX_IIS0REG(S3C64XX_IIS0LVL0ADDR)),
	//SAVE_ITEM(S3C64XX_IIS0REG(S3C64XX_IIS0SIZE)),
	//SAVE_ITEM(S3C64XX_IIS0REG(S3C64XX_IIS0AHB)),
	//SAVE_ITEM(S3C64XX_IIS0REG(S3C64XX_IIS0CON))
	//SAVE_ITEM(S3C64XX_IIS0REG(S5PC1XX_GPA0CON))
	// add more for other d/d
};
#endif

static struct sleep_save gpio_save[] = {
	SAVE_ITEM(S5PC1XX_GPA0CON),
	SAVE_ITEM(S5PC1XX_GPA0DAT),
	SAVE_ITEM(S5PC1XX_GPA0PUD),
	SAVE_ITEM(S5PC1XX_GPA1CON),
	SAVE_ITEM(S5PC1XX_GPA1DAT),
	SAVE_ITEM(S5PC1XX_GPA1PUD),
	SAVE_ITEM(S5PC1XX_GPBCON),
	SAVE_ITEM(S5PC1XX_GPBDAT),
	SAVE_ITEM(S5PC1XX_GPBPUD),
	SAVE_ITEM(S5PC1XX_GPCCON),
	SAVE_ITEM(S5PC1XX_GPCDAT),
	SAVE_ITEM(S5PC1XX_GPCPUD),
	SAVE_ITEM(S5PC1XX_GPDCON),
	SAVE_ITEM(S5PC1XX_GPDDAT),
	SAVE_ITEM(S5PC1XX_GPDPUD),
	SAVE_ITEM(S5PC1XX_GPE0CON),
	SAVE_ITEM(S5PC1XX_GPE0DAT),
	SAVE_ITEM(S5PC1XX_GPE0PUD),
	SAVE_ITEM(S5PC1XX_GPE1CON),
	SAVE_ITEM(S5PC1XX_GPE1DAT),
	SAVE_ITEM(S5PC1XX_GPE1PUD),
	SAVE_ITEM(S5PC1XX_GPF0CON),
	SAVE_ITEM(S5PC1XX_GPF0DAT),
	SAVE_ITEM(S5PC1XX_GPF0PUD),
	SAVE_ITEM(S5PC1XX_GPF1CON),
	SAVE_ITEM(S5PC1XX_GPF1DAT),
	SAVE_ITEM(S5PC1XX_GPF1PUD),
	SAVE_ITEM(S5PC1XX_GPF2CON),
	SAVE_ITEM(S5PC1XX_GPF2DAT),
	SAVE_ITEM(S5PC1XX_GPF2PUD),
	SAVE_ITEM(S5PC1XX_GPF3CON),
	SAVE_ITEM(S5PC1XX_GPF3DAT),
	SAVE_ITEM(S5PC1XX_GPF3PUD),
	SAVE_ITEM(S5PC1XX_GPG0CON),
	SAVE_ITEM(S5PC1XX_GPG0DAT),
	SAVE_ITEM(S5PC1XX_GPG0PUD),
	SAVE_ITEM(S5PC1XX_GPG1CON),
	SAVE_ITEM(S5PC1XX_GPG1DAT),
	SAVE_ITEM(S5PC1XX_GPG1PUD),
	SAVE_ITEM(S5PC1XX_GPG2CON),
	SAVE_ITEM(S5PC1XX_GPG2DAT),
	SAVE_ITEM(S5PC1XX_GPG2PUD),
	SAVE_ITEM(S5PC1XX_GPG3CON),
	SAVE_ITEM(S5PC1XX_GPG3DAT),
	SAVE_ITEM(S5PC1XX_GPG3PUD),
	SAVE_ITEM(S5PC1XX_GPH0CON),
	SAVE_ITEM(S5PC1XX_GPH0DAT),
	SAVE_ITEM(S5PC1XX_GPH0PUD),
	SAVE_ITEM(S5PC1XX_GPH1CON),
	SAVE_ITEM(S5PC1XX_GPH1DAT),
	SAVE_ITEM(S5PC1XX_GPH1PUD),
	SAVE_ITEM(S5PC1XX_GPH2CON),
	SAVE_ITEM(S5PC1XX_GPH2DAT),
	SAVE_ITEM(S5PC1XX_GPH2PUD),
	SAVE_ITEM(S5PC1XX_GPH3CON),
	SAVE_ITEM(S5PC1XX_GPH3DAT),
	SAVE_ITEM(S5PC1XX_GPH3PUD),
	SAVE_ITEM(S5PC1XX_GPICON),
	SAVE_ITEM(S5PC1XX_GPIDAT),
	SAVE_ITEM(S5PC1XX_GPIPUD),
	SAVE_ITEM(S5PC1XX_GPJ0CON),
	SAVE_ITEM(S5PC1XX_GPJ0DAT),
	SAVE_ITEM(S5PC1XX_GPJ0PUD),
	SAVE_ITEM(S5PC1XX_GPJ1CON),
	SAVE_ITEM(S5PC1XX_GPJ1DAT),
	SAVE_ITEM(S5PC1XX_GPJ1PUD),
	SAVE_ITEM(S5PC1XX_GPJ2CON),
	SAVE_ITEM(S5PC1XX_GPJ2DAT),
	SAVE_ITEM(S5PC1XX_GPJ2PUD),
	SAVE_ITEM(S5PC1XX_GPJ3CON),
	SAVE_ITEM(S5PC1XX_GPJ3DAT),
	SAVE_ITEM(S5PC1XX_GPJ3PUD),
	SAVE_ITEM(S5PC1XX_GPJ4CON),
	SAVE_ITEM(S5PC1XX_GPJ4DAT),
	SAVE_ITEM(S5PC1XX_GPJ4PUD),
	SAVE_ITEM(S5PC1XX_GPK0CON),
	SAVE_ITEM(S5PC1XX_GPK0DAT),
	SAVE_ITEM(S5PC1XX_GPK0PUD),
	SAVE_ITEM(S5PC1XX_GPK1CON),
	SAVE_ITEM(S5PC1XX_GPK1DAT),
	SAVE_ITEM(S5PC1XX_GPK1PUD),
	SAVE_ITEM(S5PC1XX_GPK2CON),
	SAVE_ITEM(S5PC1XX_GPK2DAT),
	SAVE_ITEM(S5PC1XX_GPK2PUD),
	SAVE_ITEM(S5PC1XX_GPK3CON),
	SAVE_ITEM(S5PC1XX_GPK3DAT),
	SAVE_ITEM(S5PC1XX_GPK3PUD),
#if 1 /* Add by Jay 7/18 */
	SAVE_ITEM(S5PC1XX_WKUPINT0CON),
	SAVE_ITEM(S5PC1XX_WKUPINT1CON),
	SAVE_ITEM(S5PC1XX_WKUPINT2CON),
	SAVE_ITEM(S5PC1XX_WKUPINT3CON),
	SAVE_ITEM(S5PC1XX_WKUPINT0FLTCON0),
	SAVE_ITEM(S5PC1XX_WKUPINT0FLTCON1),
	SAVE_ITEM(S5PC1XX_WKUPINT1FLTCON0),
	SAVE_ITEM(S5PC1XX_WKUPINT1FLTCON1),
	SAVE_ITEM(S5PC1XX_WKUPINT2FLTCON0),
	SAVE_ITEM(S5PC1XX_WKUPINT2FLTCON1),
	SAVE_ITEM(S5PC1XX_WKUPINT3FLTCON0),
	SAVE_ITEM(S5PC1XX_WKUPINT3FLTCON1),
	SAVE_ITEM(S5PC1XX_WKUPINT0MASK),
	SAVE_ITEM(S5PC1XX_WKUPINT1MASK),
	SAVE_ITEM(S5PC1XX_WKUPINT2MASK),
	SAVE_ITEM(S5PC1XX_WKUPINT3MASK),
	SAVE_ITEM(S5PC1XX_WKUPINT0PEND),
	SAVE_ITEM(S5PC1XX_WKUPINT1PEND),	
	SAVE_ITEM(S5PC1XX_WKUPINT2PEND),
	SAVE_ITEM(S5PC1XX_WKUPINT3PEND),
#endif	
};

/* this lot should be really saved by the IRQ code */
/* VICXADDRESSXX initilaization to be needed */
static struct sleep_save irq_save[] = {
	SAVE_ITEM(S5PC100_VIC0REG(VIC_INT_SELECT)),
	SAVE_ITEM(S5PC100_VIC1REG(VIC_INT_SELECT)),
	SAVE_ITEM(S5PC100_VIC2REG(VIC_INT_SELECT)),
	SAVE_ITEM(S5PC100_VIC0REG(VIC_INT_ENABLE)),
	SAVE_ITEM(S5PC100_VIC1REG(VIC_INT_ENABLE)),
	SAVE_ITEM(S5PC100_VIC2REG(VIC_INT_ENABLE)),
	SAVE_ITEM(S5PC100_VIC0REG(VIC_INT_SOFT)),
	SAVE_ITEM(S5PC100_VIC1REG(VIC_INT_SOFT)),
	SAVE_ITEM(S5PC100_VIC2REG(VIC_INT_SOFT)),
};

static struct sleep_save eint_save[] = {
	SAVE_ITEM(S5PC1XX_EINT0CON),
	SAVE_ITEM(S5PC1XX_EINT1CON),
	SAVE_ITEM(S5PC1XX_EINT2CON),
	SAVE_ITEM(S5PC1XX_EINT0MASK),
	SAVE_ITEM(S5PC1XX_EINT1MASK),
	SAVE_ITEM(S5PC1XX_EINT2MASK),
};

static struct sleep_save sromc_save[] = {
	SAVE_ITEM(S5PC1XX_SROM_BW),
	SAVE_ITEM(S5PC1XX_SROM_BC0),
	SAVE_ITEM(S5PC1XX_SROM_BC1),
	SAVE_ITEM(S5PC1XX_SROM_BC2),
	SAVE_ITEM(S5PC1XX_SROM_BC3),
	SAVE_ITEM(S5PC1XX_SROM_BC4),
	SAVE_ITEM(S5PC1XX_SROM_BC5),
};

/* NAND control registers */
#define PM_NFCONF             (S3C_VA_NAND + 0x00)        
#define PM_NFCONT             (S3C_VA_NAND + 0x04)        

static struct sleep_save nand_save[] = {
        SAVE_ITEM(PM_NFCONF),
        SAVE_ITEM(PM_NFCONT),
};

#define SAVE_UART(va) \
	SAVE_ITEM((va) + S3C2410_ULCON), \
	SAVE_ITEM((va) + S3C2410_UCON), \
	SAVE_ITEM((va) + S3C2410_UFCON), \
	SAVE_ITEM((va) + S3C2410_UMCON), \
	SAVE_ITEM((va) + S3C2410_UBRDIV), \
	SAVE_ITEM((va) + S3C2410_UDIVSLOT), \
	SAVE_ITEM((va) + S3C2410_UINTMSK)


static struct sleep_save uart_save[] = {
	SAVE_UART(S3C24XX_VA_UART0),
};

#define DBG(fmt...)

#define s5pc1xx_pm_debug_init() do { } while(0)
#define s5pc1xx_pm_check_prepare() do { } while(0)
#define s5pc1xx_pm_check_restore() do { } while(0)
#define s5pc1xx_pm_check_store()   do { } while(0)

#ifdef CONFIG_PM_PWR_GATING

/*
 * s5p_wait_blk_pwr_ready()
 *
 * check that the each block power is ready
 * 
*/
int s5p_wait_blk_pwr_ready(unsigned int power_domain)
{
	//unsigned int blk_pwr_stat;
	int timeout;
	int retvalue = 0;
//	mutex_lock(&power_lock);
	
	/* Wait max 20 ms */
	timeout = 20;
	while (!((readl(S5P_BLK_PWR_STAT)) & power_domain)) {
		if (timeout == 0) {
			printk(KERN_ERR "config %x: blk power never ready.\n", power_domain);
			retvalue = 1;
			goto s5p_wait_blk_pwr_ready_end;
		}
		timeout--;
		mdelay(1);
	}
s5p_wait_blk_pwr_ready_end:
//	mutex_unlock(&power_lock);
	return retvalue;
}
EXPORT_SYMBOL(s5p_wait_blk_pwr_ready);

/*
 * s5p_init_domain_power()
 *
 * Initailize power domain at booting 
 * 
*/
static void s5p_init_domain_power(void)
{
	s5p_power_gating(S5PC100_POWER_DOMAIN_TV,  DOMAIN_LP_MODE);
	s5p_power_gating(S5PC100_POWER_DOMAIN_G3D, DOMAIN_LP_MODE);
	s5p_power_gating(S5PC100_POWER_DOMAIN_MFC, DOMAIN_LP_MODE);
	s5p_power_gating(S5PC100_POWER_DOMAIN_AUDIO, DOMAIN_LP_MODE);

	// add by jay 10/14: LCD display after sw reboot
	s5p_power_gating(S5PC100_POWER_DOMAIN_LCD, DOMAIN_ACTIVE_MODE);
}

/*
 * s5p_domain_off_check()
 *
 * check if the power domain is off or not
 * 
*/
int s5p_domain_off_check(unsigned int power_domain)
{
	unsigned int poweroff = 0;

    switch( power_domain )
    {
		case S5PC100_POWER_DOMAIN_TV: //HDMI, MIXER, VP, TV clock masked ?
			if(!(readl(S5P_CLKGATE_D12) & 0xF))
			{ 
				poweroff = 1;
			}
			break;
			
	    case S5PC100_POWER_DOMAIN_LCD:  // G2D, CSI, DSI, JPEG, FIMC0/1/2, Rotator, LCDCon clock maksed
   			if(!(readl(S5P_CLKGATE_D11) & 0xFF) && !(readl(S5P_CLKGATE_D00) & 0x10))
			{
	        	poweroff = 1;
				//printk("**********>> [PM] LCD_DOMAIN will be off. \n");
			}
			break;
			
	    case S5PC100_POWER_DOMAIN_G3D: //G3D off

			if(!(readl(S5P_CLKGATE_D11) & 0x100))
			{			
	        	poweroff = 1;
			}
			break;

	    case S5PC100_POWER_DOMAIN_MFC: //MFC off

			if(!(readl(S5P_CLKGATE_D12) & 0x10))
	        {
				poweroff = 1;
			}
			break;

		case S5PC100_POWER_DOMAIN_AUDIO: //IIS0, FIM0.1.2 off

			// if(!(readl(S5P_CLKGATE_D15) & 0x01) && !(readl(S5P_CLKGATE_D11) & 0x1c))
			// modify by jay : IIS0 & FIM0, 1 off 
			if(!(readl(S5P_CLKGATE_D15) & 0x01) && !(readl(S5P_CLKGATE_D11) & 0x0c))
			{
		     		poweroff = 1;
			}
			break;

		default :
			printk( "[SYSCON][Err] S5PC100_Power_Gating - power_domain: %d \n", power_domain );
			break;
    }

	return poweroff;
}
EXPORT_SYMBOL(s5p_domain_off_check);


/*
 * s5p_power_gating()
 *
 * To do power gating
 * 
*/
int s5p_power_gating(unsigned int power_domain, unsigned int on_off)
{
	unsigned int tmp;
	int retvalue = 0;
	
	tmp = readl(S5P_BLK_PWR_STAT);
	if(((on_off == DOMAIN_ACTIVE_MODE) && (tmp & power_domain)) || ((on_off == DOMAIN_LP_MODE) && !(tmp & power_domain))){
			//printk("PM: Warning!! Incorrect Called. \n");
			return 2;
	}

	mutex_lock(&power_lock);

	if(on_off == DOMAIN_ACTIVE_MODE) {
		// masked by jay 10/16 for LCD display after sw reboot
		//if((power_domain == S5PC100_POWER_DOMAIN_TV) || (s5p_domain_off_check(power_domain))){
			tmp = readl(S5P_NORMAL_CFG) | (power_domain);	
			//tmp |= power_domain;	
			writel(tmp , S5P_NORMAL_CFG);
			while(!(readl(S5P_BLK_PWR_STAT) & (power_domain)));

			//s5p_wait_blk_pwr_ready(power_domain);
		
			retvalue = 1;			
		//}		
	}
	else if(on_off == DOMAIN_LP_MODE) {
		
		if(s5p_domain_off_check(power_domain)){
			tmp = readl(S5P_NORMAL_CFG) & ~(power_domain);
			//tmp &= ~(power_domain);
			writel(tmp , S5P_NORMAL_CFG);
    			while((readl(S5P_BLK_PWR_STAT) & (power_domain)));

			retvalue = 1;
		}
	}	
	mutex_unlock(&power_lock);

	return retvalue;
}
EXPORT_SYMBOL(s5p_power_gating);
#endif //CONFIG_PM_PWR_GATING
/* helper functions to save and restore register state */

void s5pc1xx_pm_do_save(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		ptr->val = __raw_readl(ptr->reg);
		//DBG("saved %p value %08lx\n", ptr->reg, ptr->val);
	}
}

/* s5pc1xx_pm_do_restore
 *
 * restore the system from the given list of saved registers
 *
 * Note, we do not use DBG() in here, as the system may not have
 * restore the UARTs state yet
*/

void s5pc1xx_pm_do_restore(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++)
		__raw_writel(ptr->val, ptr->reg);
}

/* s5pc1xx_pm_do_restore_core
 *
 * similar to s36410_pm_do_restore_core
 *
 * WARNING: Do not put any debug in here that may effect memory or use
 * peripherals, as things may be changing!
*/

/* s5pc1xx_pm_do_save_phy
 *
 * save register of system
 *
 * Note, I made this function to support driver with ioremap.
 * If you want to use this function, you should to input as first parameter
 * struct sleep_save_phy type
*/

void s5pc1xx_pm_do_save_phy(struct sleep_save_phy *ptr, struct platform_device *pdev, int count)
{
	void __iomem *target_reg;
	struct resource *res;
	u32 reg_size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	reg_size = res->end - res->start + 1;
	target_reg = ioremap(res->start,reg_size);

	for (; count > 0; count--, ptr++) {
		ptr->val = readl(target_reg + (ptr->reg));
	}
}

/* s5pc1xx_pm_do_restore_phy
 *
 * restore register of system
 *
 * Note, I made this function to support driver with ioremap.
 * If you want to use this function, you should to input as first parameter
 * struct sleep_save_phy type
*/

void s5pc1xx_pm_do_restore_phy(struct sleep_save_phy *ptr, struct platform_device *pdev, int count)
{
	void __iomem *target_reg;
	struct resource *res;
	u32 reg_size;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	reg_size = res->end - res->start + 1;
	target_reg = ioremap(res->start,reg_size);

	for (; count > 0; count--, ptr++) {
		writel(ptr->val, (target_reg + ptr->reg));
	}
}

static void s5pc1xx_pm_do_restore_core(struct sleep_save *ptr, int count)
{
	for (; count > 0; count--, ptr++) {
		__raw_writel(ptr->val, ptr->reg);
	}
}

/* s5pc1xx_pm_show_resume_irqs
 *
 * print any IRQs asserted at resume time (ie, we woke from)
*/

static void s5pc1xx_pm_show_resume_irqs(int start, unsigned long which,
					unsigned long mask)
{
	int i;

	which &= ~mask;

	for (i = 0; i <= 31; i++) {
		if ((which) & (1L<<i)) {
			DBG("IRQ %d asserted at resume\n", start+i);
		}
	}
}

static irqreturn_t s5pc1xx_eint11_interrupt(int irq, void *dev_id)
{
	printk("EINT11 is occured\n");
	//writel(0x03 , weint_base + S5P_APM_WEINT1_PEND);
	return IRQ_HANDLED;
}

static irqreturn_t s5pc1xx_eint31_interrupt(int irq, void *dev_id)
{
	printk("EINT31 is occured\n");
	//writel(0x07 , weint_base + S5P_APM_WEINT3_PEND);
	return IRQ_HANDLED;
}
static void s5pc1xx_pm_configure_extint(void)
{
/* for each of the external interrupts (EINT0..EINT15) we
 * need to check wether it is an external interrupt source,
 * and then configure it as an input if it is not
 * And SMDKC100 has two External Interrupt Switch EINT11(GPH1_3) and EINT31(GPH3_7)
 * So System can wake up with both External interrupt source.
 */

	u32 tmp;

	weint_base = ioremap(S5P_APM_BASE, 0x350);
	/* Mask all External Interrupt */
	writel(0xff , weint_base + S5P_APM_WEINT0_MASK);
	writel(0xff , weint_base + S5P_APM_WEINT1_MASK);
	writel(0xff , weint_base + S5P_APM_WEINT2_MASK);
	writel(0xdf , weint_base + S5P_APM_WEINT3_MASK);

	/* Clear all External Interrupt Pending */
	writel(0xff , weint_base + S5P_APM_WEINT0_PEND);
	writel(0xff , weint_base + S5P_APM_WEINT1_PEND);
	writel(0xff , weint_base + S5P_APM_WEINT2_PEND);
	writel(0xff , weint_base + S5P_APM_WEINT3_PEND);

	/* GPH1(3) setting */
	tmp = readl(weint_base + S5P_APM_GPH3CON);
	tmp &= ~(0xf << 24);
	tmp |= (0x2 << 24);		// WAKEUP_INT[11]
	writel(tmp , weint_base + S5P_APM_GPH3CON);

	/* EINT1_CON Reg setting */
	tmp = readl(weint_base + S5P_APM_WEINT3_CON);
	tmp &= ~(0x7 << 24);
	tmp |= (0x2 << 24);  //int1_con(3) falling edge triggered
	writel(tmp , weint_base + S5P_APM_WEINT3_CON);

	/* EINT1_FLTCON[0,1] Reg setting */
	tmp = readl(weint_base + S5P_APM_WEINT3_FLTCON1);
	tmp &= ~(0x1 << 22); // select delay filter
	tmp |= (0x1 << 23);  // enable filter
	writel(tmp , weint_base + S5P_APM_WEINT3_FLTCON1);

	/* EINT1 MASK Reg setting */
	tmp = readl(weint_base + S5P_APM_WEINT3_MASK);
	tmp &= ~(1 << 6);
	writel(tmp , weint_base + S5P_APM_WEINT3_MASK);

	udelay(50);

	set_irq_type(IRQ_EINT16_31, IRQ_TYPE_EDGE_FALLING);

#if 1
	/* EINT30 Pending */ 
	tmp = readl(weint_base + S5P_APM_WEINT3_PEND);
	tmp |= (1 << 6);
	writel(tmp , weint_base + S5P_APM_WEINT3_PEND);
#endif

	tmp = readl(S5P_EINT_WAKEUP_MASK);
	tmp = ~(1 << 30);
	writel(tmp , S5P_EINT_WAKEUP_MASK);
}

void (*pm_cpu_prep)(void);
void (*pm_cpu_sleep)(void);

#define any_allowed(mask, allow) (((mask) & (allow)) != (allow))
static int s5pc1xx_pm_clk(enum PLL_TYPE pm_pll,u32 mdiv, u32 pdiv, u32 sdiv)
{
	u32 pll_value;
	u32 pll_addr;

	pll_value = (1 << 31) | (mdiv << 16) | (pdiv << 8) | (sdiv << 0);

	switch(pm_pll)
	{
		case PM_APLL:
			pll_addr = (u32)S5P_APLL_CON;
		case PM_MPLL:
			pll_addr = (u32)S5P_MPLL_CON;
		case PM_EPLL:
			pll_addr = (u32)S5P_EPLL_CON;
		case PM_HPLL:
			pll_addr = (u32)S5P_HPLL_CON;
	}

	writel(pll_value , pll_addr);

	while(!((readl(pll_addr) >> 30) & 0x1)){}
	return 0;
}
#ifdef CONFIG_LPAUDIO
extern unsigned int LP_flag;
//extern onLED4(bool en);

int read_sleep_status(void)
{
	u32 inform1_val;
	inform1_val = __raw_readl(MODE_CFG);
	//printk("S5P_INFO1 has %x\n",inform1_val);
	return inform1_val;	
}

int read_I2Swakeup_status(void)
{
	u32 tmp;
	tmp = __raw_readl(S5P_WAKEUP_STAT);
	//printk("S5P_WAKEUP_STAT has %x\n",tmp);
  	tmp &= (1<<12);
	if(tmp) {
		// wakeup from I2S
		return 1;
	} else {
		return 0;
	}
}



void printClockRegs() {
	printk("[ks] CLK_GATE10=%x\n", readl(S5P_CLKGATE_D10));
	printk("[ks] CLK_GATE11=%x\n", readl(S5P_CLKGATE_D11));
	printk("[ks] CLK_GATE12=%x\n", readl(S5P_CLKGATE_D12));
	printk("[ks] CLK_GATE13=%x\n", readl(S5P_CLKGATE_D13));
	printk("[ks] CLK_GATE14=%x\n", readl(S5P_CLKGATE_D14));
	printk("[ks] CLK_GATE15=%x\n", readl(S5P_CLKGATE_D15));
	printk("[ks] CLK_GATE20=%x\n", readl(S5P_CLKGATE_D20));
}
extern bool i2sLPMode(void);
#endif //CONFIG_LPAUDIO

/* s5pc1xx_pm_enter
 *
 * central control for sleep/resume process
*/

static int s5pc1xx_pm_enter(suspend_state_t state)
{
	unsigned long regs_save[16];
	unsigned int tmp, wakeup_from_deepidle;
	unsigned int wkup_by_I2S=0;

#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
	android_unlock_suspend(&pm_idle_lock);
#endif
#else
	wake_unlock(&pm_wake_lock);
#endif

#ifdef CONFIG_LPAUDIO
	if (LP_flag) {
		__raw_writel(0x23452345,S5P_INFORM1);	
	} else {
		__raw_writel(0x0,S5P_INFORM1);
	}
#endif
	/* ensure the debug is initialised (if enabled) */

	DBG("s5pc1xx_pm_enter(%d)\n", state);

	if (pm_cpu_prep == NULL || pm_cpu_sleep == NULL) {
		printk(KERN_ERR PFX "error: no cpu sleep functions set\n");
		return -EINVAL;
	}
#ifdef CONFIG_CPU_FREQ
	if(!is_userspace_gov()) {
		tmp = s5pc1xx_target_frq(0xFFFFFFFF, 1);
		s5pc100_pm_target(tmp);
	}
	else {
#ifdef USE_DVS
	set_voltage(0);
#endif
	}
#endif
	/* store the physical address of the register recovery block */
	s5pc100_sleep_save_phys = virt_to_phys(regs_save);

	DBG("s5pc1xx_sleep_save_phys=0x%08lx\n", s5pc100_sleep_save_phys);

#if !defined(CONFIG_MACH_HKDKC100)
	/*EXT11 disable*/
	tmp = readl(weint_base + S5P_APM_GPH1PUD);
	tmp &= ~(0x3 << 6);
	tmp |= (0x0 << 6);		//disable:0, up :2, down:1
	writel(tmp , weint_base + S5P_APM_GPH1PUD);
	udelay(30);
#endif	

#if 1 /* To keep high of XciRESET */
	tmp = readl(S5PC1XX_GPE1CONPDN);
	tmp &= ~(0x3 << 8);
	tmp |= (0x1 << 8);
	writel(tmp , S5PC1XX_GPE1CONPDN);

	tmp = readl(S5PC1XX_GPE1PUDPDN);
	tmp &= ~(0x3 << 8);
	tmp |= (0x2 << 8);
	writel(tmp , S5PC1XX_GPE1PUDPDN);
	udelay(50);

	//2009.09.18. add by jay for SDMMC GPIO setting at Power down mode
	__raw_writel(0xFFFF,S5PC1XX_GPG0CONPDN); 
	__raw_writel(0x0000,S5PC1XX_GPG0PUDPDN);
#endif

	s5pc1xx_pm_do_save(gpio_save, ARRAY_SIZE(gpio_save));
	s5pc1xx_pm_do_save(irq_save, ARRAY_SIZE(irq_save));
	s5pc1xx_pm_do_save(core_save, ARRAY_SIZE(core_save));
	s5pc1xx_pm_do_save(sromc_save, ARRAY_SIZE(sromc_save));
//	s5pc1xx_pm_do_save(nand_save, ARRAY_SIZE(nand_save));
	s5pc1xx_pm_do_save(uart_save, ARRAY_SIZE(uart_save));
	s5pc1xx_pm_do_save(eint_save, ARRAY_SIZE(eint_save));
#ifdef CONFIG_LPAUDIO
	s5pc1xx_pm_do_save(lpaudio_save, ARRAY_SIZE(lpaudio_save));
#endif

	/* ensure INF_REG0  has the resume address */
	__raw_writel(virt_to_phys(s5pc100_cpu_resume), S5P_INFORM0);

	/* call cpu specific preperation */

	pm_cpu_prep();

	/* flush cache back to ram */
	flush_cache_all();

	/* send the cpu to sleep... */
	__raw_writel(0xffffffff, S5PC100_VIC0INTENCLEAR);
	__raw_writel(0xffffffff, S5PC100_VIC1INTENCLEAR);
//	__raw_writel(0xffffffff, S5PC100_VIC2INTENCLEAR);
	__raw_writel(0xfff8ffff, S5PC100_VIC2INTENCLEAR);
	__raw_writel(0xffffffff, S5PC100_VIC0SOFTINTCLEAR);
	__raw_writel(0xffffffff, S5PC100_VIC1SOFTINTCLEAR);
	__raw_writel(0xffffffff, S5PC100_VIC2SOFTINTCLEAR);

	/* Mask all wake up source */
	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= ~(0x1 << 7);
	tmp |= (0x7ff << 8);
	if(__raw_readl(MODE_CFG) == 0) {
		/* unmask alarm wakeup source */
		tmp &= ~(0x1 << 10);
	} else {
		tmp &= ~((0x1<<8)|(0x1<<10)|(0x1<<12)|(0x1<<17));  //key|alarm|ts|i2s
	}

	__raw_writel(tmp , S5P_PWR_CFG);
	__raw_writel(0xffffffff , S5P_EINT_WAKEUP_MASK);
	
	/* Wake up source setting */
	s5pc1xx_pm_configure_extint();

#if defined(CONFIG_MACH_HKDKC100)
	hkc100_sleep();
#endif

	/* : USB Power Control */
	/*   - USB PHY Disable */
	/*   - Make USB Tranceiver PAD to Suspend */
	tmp = __raw_readl(S5P_OTHERS);
   	tmp &= ~(1<<16);           	/* USB Signal Mask Clear */
   	__raw_writel(tmp, S5P_OTHERS);

	tmp = __raw_readl(S5PC1XX_UHOST);
	tmp |= (1<<0);
	__raw_writel(tmp, S5PC1XX_UHOST);

	/* Sleep Mode Pad Configuration */ // Changed :set to auto in android
	__raw_writel(0x0, S5PC1XX_PDNEN); /* Controlled by SLPEN Bit (You Should Clear SLPEN Bit in Wake Up Process...) */

	/* Set WFI instruction to SLEEP mode */
	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= S5P_CFG_WFI_CLEAN;

	if(__raw_readl(MODE_CFG) == 0){	
		tmp |= (S5P_CFG_WFI_SLEEP);
	}else{
		tmp &= ~( 0x1f << 27 );
//		tmp |= (S5P_CFG_WFI_DEEPIDLE | (1 << 31) | (1 << 29) | (0 << 27) | (0xfff << 7));
		tmp |= (S5P_CFG_WFI_DEEPIDLE | (1 << 31) | (1 << 29) | (0 << 27));
		__raw_writel(((0x1 << 0) | (0x1a0 << 16)) ,S5P_CLAMP_STABLE);
	}
	__raw_writel(tmp, S5P_PWR_CFG);

	/* Clear WAKEUP_STAT register for next wakeup */
	tmp = __raw_readl(S5P_WAKEUP_STAT);
	__raw_writel(tmp, S5P_WAKEUP_STAT);

	if(__raw_readl(MODE_CFG) == 0) {
		/* Set Power Stable Count */
		tmp = __raw_readl(S5P_OTHERS);
		tmp &=~(1 << S5P_OTHER_STA_TYPE);
		tmp |= (STA_TYPE_SFR << S5P_OTHER_STA_TYPE);
		__raw_writel(tmp , S5P_OTHERS);
	
		__raw_writel(((S5P_PWR_STABLE_COUNT << S5P_PWR_STA_CNT) | (1 << S5P_PWR_STA_EXP_SCALE)), S5P_PWR_STABLE);
	}

	/* Set Syscon Interrupt */
	tmp = __raw_readl(S5P_OTHERS);
	tmp |= (1 << S5P_OTHER_SYS_INT);
	__raw_writel(tmp, S5P_OTHERS);

	if(__raw_readl(MODE_CFG) == 0){
		/* Disable OSC_EN (Disable X-tal Osc Pad in Sleep mode) */
		tmp = __raw_readl(S5P_SLEEP_CFG);
		tmp &= ~(1 << 0);
		__raw_writel(tmp, S5P_SLEEP_CFG);
	}
#ifdef CONFIG_LPAUDIO
	if (i2sLPMode()) writel((0<<1) | (1<<0),mp3reg);			
#endif

	/* s5pc1xx_cpu_save will also act as our return point from when
	 * we resume as it saves its own register state, so use the return
	 * code to differentiate return from save and return from sleep */

	if (s5pc100_cpu_save(regs_save) == 0) {
		flush_cache_all();
		/* This function for Chip bug on EVT0 */
#if defined(CONFIG_MACH_HKDKC100)	
		s5pc1xx_pm_clk(PM_APLL, 512 , 2 , 5);
		s5pc1xx_pm_clk(PM_MPLL, 128 , 2 , 5);
		s5pc1xx_pm_clk(PM_EPLL, 128 , 2 , 5);
		s5pc1xx_pm_clk(PM_HPLL, 128 , 2 , 5);
#endif
		pm_cpu_sleep();
	}

	/* restore the cpu state */
	cpu_init();

#ifdef CONFIG_LPAUDIO 	
	if (i2sLPMode()) writel((0<<1) | (1<<0),mp3reg);	
#endif	
    
	
	//mdelay(10);

	//tmp = __raw_readl(S5PC1XX_GPH1DAT) | (0x1 << 6);	
	//__raw_writel(tmp, S5PC1XX_GPH1DAT);

	/* Sleep Mode Pad Configuration */
  	__raw_writel(0x0, S5PC1XX_PDNEN);	/* Clear SLPEN Bit for Pad back to Normal Mode */

#if 1 //moved by jay : To output i2s0_clock & i2s0_data after EPLL_RET_RELEASE
	tmp = __raw_readl(S5PC1XX_UHOST);
	tmp &= ~(1<<0);
	__raw_writel(tmp, S5PC1XX_UHOST);

	s5pc1xx_pm_do_restore(gpio_save, ARRAY_SIZE(gpio_save));
	s5pc1xx_pm_do_restore(irq_save, ARRAY_SIZE(irq_save));
	s5pc1xx_pm_do_restore(core_save, ARRAY_SIZE(core_save));
	s5pc1xx_pm_do_restore(sromc_save, ARRAY_SIZE(sromc_save));
//	s5pc1xx_pm_do_restore(nand_save, ARRAY_SIZE(nand_save));
	s5pc1xx_pm_do_restore(uart_save, ARRAY_SIZE(uart_save));
	s5pc1xx_pm_do_restore(eint_save, ARRAY_SIZE(eint_save));
#ifdef CONFIG_LPAUDIO
	s5pc1xx_pm_do_restore(lpaudio_save, ARRAY_SIZE(lpaudio_save));
#endif
#endif

#if defined(CONFIG_MACH_HKDKC100)	
	hkc100_wakeup();
#endif

	tmp = __raw_readl(S5P_RST_STAT);
	tmp &= ~0xffffff77;
	if (tmp==0x80) wakeup_from_deepidle=1;
	else  wakeup_from_deepidle=0;

	if (wakeup_from_deepidle) {
		tmp = __raw_readl(S5P_WAKEUP_STAT);
	  	tmp &= ~(1<<12);

		if(tmp) {	// any other wkup src exist
			wkup_by_I2S = 0;
//			while(1);
#ifdef CONFIG_LPAUDIO
			AudioSetModeLP(false);
#endif
		} else {		// only i2s exist
			wkup_by_I2S = 1;
//			while(1);
		}

		tmp = __raw_readl(S5P_OTHERS);
		// IO_RET_RELEASE | EPLL_RET_RELEASE | SDMMC_IO_RET_RELEASE | USB_PHY_EN
	  	tmp |= (1<<31) | (1<<30) | (1<<22) | (1<<16); 
		__raw_writel(tmp, S5P_OTHERS);

	} else {
		tmp = __raw_readl(S5P_OTHERS);
		// IO_RET_RELEASE | SDMMC_IO_RET_RELEASE | USB_PHY_EN
	  	tmp |= (1<<31) | (1<<22) | (1<<16); 
		__raw_writel(tmp, S5P_OTHERS);
	}
	/* Set WFI instruction to NORMAL mode */
	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= S5P_CFG_WFI_CLEAN;
	__raw_writel(tmp, S5P_PWR_CFG);

	tmp = readl(weint_base + S5P_APM_WEINT1_PEND); // for EINT30
	writel(tmp , weint_base + S5P_APM_WEINT1_PEND);

	tmp = readl(S5P_EINT_WAKEUP_MASK);
	tmp |= (1 << 30);
	writel(tmp , S5P_EINT_WAKEUP_MASK);

	printk("post sleep, preparing to return\n");

	s5pc1xx_pm_check_restore();
#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
	android_lock_partial_suspend_auto_expire(&pm_idle_lock, 10 * HZ);
#endif
#else
	wake_lock_timeout(&pm_wake_lock, 10 * HZ);
#endif

	/* ok, let's return from sleep */
	DBG("S5PC100 PM Resume (post-restore)\n");
	return 0;
}


static struct platform_suspend_ops s5pc1xx_pm_ops = {
	.enter		= s5pc1xx_pm_enter,
	.valid		= suspend_valid_only_mem,
};

#ifdef CONFIG_LPAUDIO
int write_deep_command(struct file *file, const char __user *buffer,
					unsigned long count, void *data)
{
	char *real_data;

	real_data = (char *) data;

	if(copy_from_user(real_data, buffer, count))
		return -EFAULT;

	if(!(strncmp(real_data,"ON",2))){
		printk(KERN_ERR "Deep idle mode \n");
		__raw_writel(0x23452345,S5P_INFORM1);	
		LP_flag=1;	
	}else if(!(strncmp(real_data,"OFF",3))){
		printk(KERN_ERR "Normal sleep mode \n");
		__raw_writel(0x0,S5P_INFORM1);
	}else{
		printk(KERN_ERR "invaild value\n");
	}

	return count;	
	
}

int read_deep_command(char *page, char **start, off_t off,
			int count, int *eof, void *data_unused)
{
	char *buf;
	u32 inform1_val;

	inform1_val = __raw_readl(MODE_CFG);

	buf = page;

	buf += sprintf(buf, "INFROM1 : %x\n", inform1_val);

	return buf - page;	
	
}


/* s5pc1xx_pm_init
 *
 * Attach the power management functions. This should be called
 * from the board specific initialisation if the board supports
 * it.
*/

int __init s5pc1xx_pm_init(void)
{
	unsigned int tmp;

//	printk("s5pc1xx Power Management init, (c) 2009 Samsung Electronics\n");
	printk("s5pc1xx Power Management init, (c) 2009 Samsung Electronics, RST_STAT=%x\n",S5P_RST_STAT );

#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
	pm_idle_lock.name = "pm_suspend_lock";
	android_init_suspend_lock(&pm_idle_lock);
#endif
#else
	wake_lock_init(&pm_wake_lock, WAKE_LOCK_SUSPEND, "pm_wake_lock");
#endif
	proc_root_fp = proc_mkdir("deep_idle_test", 0);

#ifdef CONFIG_PM_PWR_GATING
	s5p_init_domain_power();
#endif

#ifdef CONFIG_LPAUDIO 
	proc_deep_command_fp = create_proc_entry("deep_command", S_IFREG | S_IRWXU, proc_root_fp);
	if(proc_deep_command_fp) {
		proc_deep_command_fp->data = proc_deep_command_str;
		proc_deep_command_fp->write_proc = write_deep_command;
		proc_deep_command_fp->read_proc = read_deep_command;
	}	

	weint_base = ioremap(S5P_APM_BASE, 0x350);
	mp3reg = ioremap(S5PC1XX_PA_LPMP3_MODE_SEL, 0x4);
#endif

	/* set the irq configuration for wake */
	suspend_set_ops(&s5pc1xx_pm_ops);
	return 0;
}

#else

int __init s5pc1xx_pm_init(void)
{
	printk("s5pc1xx Power Management init, (c) 2009 Samsung Electronics\n");

#ifndef CONFIG_HAS_WAKELOCK
#ifdef CONFIG_ANDROID_POWER
	pm_idle_lock.name = "pm_suspend_lock";
	android_init_suspend_lock(&pm_idle_lock);
#endif
#else
	wake_lock_init(&pm_wake_lock, WAKE_LOCK_SUSPEND, "pm_wake_lock");
#endif

#ifdef CONFIG_PM_PWR_GATING
	s5p_init_domain_power();
#endif

	weint_base = ioremap(S5P_APM_BASE, 0x350);

	/* set the irq configuration for wake */
	suspend_set_ops(&s5pc1xx_pm_ops);
	return 0;
}
#endif

