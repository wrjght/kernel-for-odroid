//[*]----------------------------------------------------------------------------------------------[*]
//
//
// 
//  HardKernel-C100 power down control (charles.park)
//  2010.01.06
// 
//
//[*]----------------------------------------------------------------------------------------------[*]
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/fs.h>

#include <mach/irqs.h>
#include <asm/system.h>

#include <linux/delay.h>
#include <plat/regs-gpio.h>

#include "hkc100_sleep.h"

//[*]----------------------------------------------------------------------------------------------[*]
#define	GPIO_I2C_SDA_CON_PORT	(*(unsigned long *)S5PC1XX_GPDCON)
#define	GPIO_I2C_SDA_DAT_PORT	(*(unsigned long *)S5PC1XX_GPDDAT)
#define	GPIO_SDA_PIN			3

#define	GPIO_I2C_CLK_CON_PORT	(*(unsigned long *)S5PC1XX_GPDCON)
#define	GPIO_I2C_CLK_DAT_PORT	(*(unsigned long *)S5PC1XX_GPDDAT)
#define	GPIO_CLK_PIN			4

//[*]----------------------------------------------------------------------------------------------[*]
#define	GPIO_CON_PORT_MASK		0xF
#define	GPIO_CON_PORT_OFFSET	0x4

#define	GPIO_CON_INPUT			0x0
#define	GPIO_CON_OUTPUT			0x1

#define	DELAY_TIME				5	// us value
#define	PORT_CHANGE_DELAY_TIME	5

#define	HIGH					1
#define	LOW						0

//#define	DEBUG_HKC100_SLEEP		// debug enable flag
#define	DEBUG_MSG(x)			printk(x)

//[*]----------------------------------------------------------------------------------------------[*]
// GPIO Control register
//[*]----------------------------------------------------------------------------------------------[*]
#define	GPJ0CON					(*(unsigned long *)S5PC1XX_GPJ0CON)
#define	GPJ0DAT					(*(unsigned long *)S5PC1XX_GPJ0DAT)
#define	GPJ0PUD					(*(unsigned long *)S5PC1XX_GPJ0PUD)

#define	GPH1CON					(*(unsigned long *)S5PC1XX_GPH1CON)
#define	GPH1DAT					(*(unsigned long *)S5PC1XX_GPH1DAT)

//[*]----------------------------------------------------------------------------------------------[*]
// Power down port control register(control power down/conrtol power down pull/pulldn register)
//[*]----------------------------------------------------------------------------------------------[*]
#define	GPA0CONPDN				(*(unsigned long *)S5PC1XX_GPA0CONPDN)
#define	GPA0PUDPDN				(*(unsigned long *)S5PC1XX_GPA0PUDPDN)

#define	GPA1CONPDN				(*(unsigned long *)S5PC1XX_GPA1CONPDN)
#define	GPA1PUDPDN				(*(unsigned long *)S5PC1XX_GPA1PUDPDN)
                        		                                     
#define	GPBCONPDN				(*(unsigned long *)S5PC1XX_GPBCONPDN)
#define	GPBPUDPDN				(*(unsigned long *)S5PC1XX_GPBPUDPDN)
                        		                                     
#define	GPCCONPDN				(*(unsigned long *)S5PC1XX_GPCCONPDN)
#define	GPCPUDPDN				(*(unsigned long *)S5PC1XX_GPCPUDPDN)
                        		                                     
#define	GPDCONPDN				(*(unsigned long *)S5PC1XX_GPDCONPDN)
#define	GPDPUDPDN				(*(unsigned long *)S5PC1XX_GPDPUDPDN)
                        		                                     
#define	GPG2CONPDN				(*(unsigned long *)S5PC1XX_GPG2CONPDN)
#define	GPG2PUDPDN				(*(unsigned long *)S5PC1XX_GPG2PUDPDN)
                        		                                     
#define	GPG3CONPDN				(*(unsigned long *)S5PC1XX_GPG3CONPDN)
#define	GPG3PUDPDN				(*(unsigned long *)S5PC1XX_GPG3PUDPDN)
                        		                                     
#define	GPH0CONPDN				(*(unsigned long *)S5PC1XX_GPH0CONPDN)
#define	GPH0PUDPDN				(*(unsigned long *)S5PC1XX_GPH0PUDPDN)
                        		                                     
#define	GPH1CONPDN				(*(unsigned long *)S5PC1XX_GPH1CONPDN)
#define	GPH1PUDPDN				(*(unsigned long *)S5PC1XX_GPH1PUDPDN)
                        		                                     
#define	GPJ0CONPDN				(*(unsigned long *)S5PC1XX_GPJ0CONPDN)
#define	GPJ0PUDPDN				(*(unsigned long *)S5PC1XX_GPJ0PUDPDN)

//[*]----------------------------------------------------------------------------------------------[*]
// PMIC Control Register
//[*]----------------------------------------------------------------------------------------------[*]
#define	PMIC_ONOFF_REG1		0x00
	#define	BUCK1_ON	0x80
	#define	BUCK2_ON	0x40
	#define	BUCK3_ON	0x20
	#define	LDO2_ON		0x10
	#define	LDO3_ON		0x08
	#define	LDO4_ON		0x04
	#define	LDO5_ON		0x02
	
#define	PMIC_ONOFF_REG2		0x01
	#define	LDO6_ON		0x80
	#define	LDO7_ON		0x40
	#define	LDO8_ON		0x20
	#define	LDO9_ON		0x10
	#define	BATMON_ON	0x01

static	unsigned char	PMIC_OnOffReg1 = 0x00;
static	unsigned char	PMIC_OnOffReg2 = 0x00;

//[*]----------------------------------------------------------------------------------------------[*]
//	static function prototype
//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_sda_port_control	(unsigned char inout);
static	void			gpio_i2c_clk_port_control	(unsigned char inout);

static	unsigned char	gpio_i2c_get_sda			(void);
static	void			gpio_i2c_set_sda			(unsigned char hi_lo);
static	void			gpio_i2c_set_clk			(unsigned char hi_lo);
                                        	
static 	void			gpio_i2c_start				(void);
static 	void			gpio_i2c_stop				(void);
                                        	
static 	void			gpio_i2c_send_ack			(void);
static 	void			gpio_i2c_send_noack			(void);
static 	unsigned char	gpio_i2c_chk_ack			(void);
                		                      	
static 	void 			gpio_i2c_byte_write			(unsigned char wdata);
static 	void			gpio_i2c_byte_read			(unsigned char *rdata);
		        		
//[*]----------------------------------------------------------------------------------------------[*]
//	extern function prototype
//[*]----------------------------------------------------------------------------------------------[*]
static	int 			hkc100_pmic_write			(unsigned char reg, unsigned char wdata);
static	int				hkc100_pmic_read			(unsigned char reg, unsigned char *rdata, unsigned char rsize);
static	void			hkc100_pmic_port_init		(void);

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_sda_port_control	(unsigned char inout)
{
	GPIO_I2C_SDA_CON_PORT &=  (unsigned long)(~(GPIO_CON_PORT_MASK << (GPIO_SDA_PIN * GPIO_CON_PORT_OFFSET)));
	GPIO_I2C_SDA_CON_PORT |=  (unsigned long)( (inout << (GPIO_SDA_PIN * GPIO_CON_PORT_OFFSET)));
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_clk_port_control	(unsigned char inout)
{
	GPIO_I2C_CLK_CON_PORT &=  (unsigned long)(~(GPIO_CON_PORT_MASK << (GPIO_CLK_PIN * GPIO_CON_PORT_OFFSET)));
	GPIO_I2C_CLK_CON_PORT |=  (unsigned long)( (inout << (GPIO_CLK_PIN * GPIO_CON_PORT_OFFSET)));
}

//[*]----------------------------------------------------------------------------------------------[*]
static	unsigned char	gpio_i2c_get_sda		(void)
{
	return	GPIO_I2C_SDA_DAT_PORT & (HIGH << GPIO_SDA_PIN) ? 1 : 0;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_set_sda		(unsigned char hi_lo)
{
	if(hi_lo)	{
		gpio_i2c_sda_port_control(GPIO_CON_INPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
	else		{
		GPIO_I2C_SDA_DAT_PORT &= ~(HIGH << GPIO_SDA_PIN);
		gpio_i2c_sda_port_control(GPIO_CON_OUTPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_set_clk		(unsigned char hi_lo)
{
	if(hi_lo)	{
		gpio_i2c_clk_port_control(GPIO_CON_INPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
	else		{
		GPIO_I2C_CLK_DAT_PORT &= ~(HIGH << GPIO_CLK_PIN);
		gpio_i2c_clk_port_control(GPIO_CON_OUTPUT);
		udelay(PORT_CHANGE_DELAY_TIME);
	}
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_start			(void)
{
	// Setup SDA, CLK output High
	gpio_i2c_set_sda(HIGH);
	gpio_i2c_set_clk(HIGH);
	
	udelay(DELAY_TIME);

	// SDA low before CLK low
	gpio_i2c_set_sda(LOW);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(LOW);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_stop			(void)
{
	// Setup SDA, CLK output low
	gpio_i2c_set_sda(LOW);
	gpio_i2c_set_clk(LOW);
	
	udelay(DELAY_TIME);
	
	// SDA high after CLK high
	gpio_i2c_set_clk(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_sda(HIGH);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_send_ack		(void)
{
	// SDA Low
	gpio_i2c_set_sda(LOW);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(LOW);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_send_noack		(void)
{
	// SDA High
	gpio_i2c_set_sda(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(HIGH);	udelay(DELAY_TIME);
	gpio_i2c_set_clk(LOW);	udelay(DELAY_TIME);
}

//[*]----------------------------------------------------------------------------------------------[*]
static	unsigned char	gpio_i2c_chk_ack		(void)
{
	unsigned char	count = 0, ret = 0;

	gpio_i2c_set_sda(LOW);		udelay(DELAY_TIME);
	gpio_i2c_set_clk(HIGH);		udelay(DELAY_TIME);

	gpio_i2c_sda_port_control(GPIO_CON_INPUT);
	udelay(PORT_CHANGE_DELAY_TIME);

	while(gpio_i2c_get_sda())	{
		if(count++ > 100)	{	ret = 1;	break;	}
		else					udelay(DELAY_TIME);	
	}

	gpio_i2c_set_clk(LOW);		udelay(DELAY_TIME);
	
	#if defined(DEBUG_HKC100_SLEEP)
		if(ret)		DEBUG_MSG(("%s (%d): no ack!!\n",__FUNCTION__, ret));
		else		DEBUG_MSG(("%s (%d): ack !! \n" ,__FUNCTION__, ret));
	#endif

	return	ret;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void 		gpio_i2c_byte_write		(unsigned char wdata)
{
	unsigned char	cnt, mask;
	
	for(cnt = 0, mask = 0x80; cnt < 8; cnt++, mask >>= 1)	{
		if(wdata & mask)		gpio_i2c_set_sda(HIGH);
		else					gpio_i2c_set_sda(LOW);
			
		gpio_i2c_set_clk(HIGH);		udelay(DELAY_TIME);
		gpio_i2c_set_clk(LOW);		udelay(DELAY_TIME);
	}
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void		gpio_i2c_byte_read		(unsigned char *rdata)
{
	unsigned char	cnt, mask;

	gpio_i2c_sda_port_control(GPIO_CON_INPUT);
	udelay(PORT_CHANGE_DELAY_TIME);

	for(cnt = 0, mask = 0x80, *rdata = 0; cnt < 8; cnt++, mask >>= 1)	{
		gpio_i2c_set_clk(HIGH);		udelay(DELAY_TIME);
		
		if(gpio_i2c_get_sda())		*rdata |= mask;
		
		gpio_i2c_set_clk(LOW);		udelay(DELAY_TIME);
		
	}
}

//[*]----------------------------------------------------------------------------------------------[*]
static	int 		hkc100_pmic_write		(unsigned char reg, unsigned char wdata)
{
	unsigned char ack;

	// start
	gpio_i2c_start();
	
	gpio_i2c_byte_write(PMIC_WR_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_HKC100_SLEEP)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	write_stop;
	}
	
	gpio_i2c_byte_write(reg);	// register
	
	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_HKC100_SLEEP)
			DEBUG_MSG(("%s [write register] : no ack\n",__FUNCTION__));
		#endif
	}

	gpio_i2c_byte_write(wdata);	// data
	
	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_HKC100_SLEEP)
			DEBUG_MSG(("%s [write data] : no ack\n",__FUNCTION__));
		#endif
	}

write_stop:
	gpio_i2c_stop();

	#if defined(DEBUG_HKC100_SLEEP)
		DEBUG_MSG(("%s : %d\n", __FUNCTION__, ack));
	#endif

	return	ack;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	int			hkc100_pmic_read		(unsigned char reg, unsigned char *rdata, unsigned char rsize)
{
	unsigned char ack, cnt;

	// start
	gpio_i2c_start();

	gpio_i2c_byte_write(PMIC_WR_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_HKC100_SLEEP)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}
	
	gpio_i2c_byte_write(reg);	// start register

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_HKC100_SLEEP)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}
	
	gpio_i2c_stop();
	
	// re-start
	gpio_i2c_start();

	gpio_i2c_byte_write(PMIC_RD_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_HKC100_SLEEP)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}

	for(cnt=0; cnt < rsize; cnt++)	{
		
		gpio_i2c_byte_read(&rdata[cnt]);
		
		if(cnt == rsize -1)		gpio_i2c_send_noack();
		else					gpio_i2c_send_ack();
	}
	
read_stop:
	gpio_i2c_stop();

	#if defined(DEBUG_HKC100_SLEEP)
		DEBUG_MSG(("%s : %d\n", __FUNCTION__, ack));
	#endif

	return	ack;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void 		hkc100_pmic_port_init	(void)
{
	gpio_i2c_set_sda(HIGH);		gpio_i2c_set_clk(HIGH);
}

//[*]----------------------------------------------------------------------------------------------[*]
void				hkc100_sleep(void)
{
	hkc100_pmic_port_init();

	// serial port state hold	
	GPA0CONPDN = 0xFFFF;	GPA0PUDPDN = 0x0000;

	// Volume LED, Power LED, Backlight Port state hold	
	//GPA1CONPDN = 0xFFFF;	GPA1PUDPDN = 0x0000;

	// BMA150/LCD SPI Port state hold
//	GPBCONPDN = 0xFFFF;		GPBPUDPDN = 0x0000;

	// Audio I2S Port
//	GPCCONPDN = 0xFFFF;		GPCPUDPDN = 0x0000;
	                 
	// MMC1 Port  
	GPG2CONPDN = 0x2FFF;	GPG2PUDPDN = 0x0000;
	            
	// MMC2 Port       
	GPG3CONPDN = 0x2FFF;	GPG3PUDPDN = 0x0000;

	// Power Hold Port
//	GPH0CONPDN = 0xFFFF;	GPH0PUDPDN = 0x0000;

	// PMIC Control & 3V, 5V Power Control Port
	GPH1CONPDN = 0xFFFF;	GPH1PUDPDN = 0x0000;
	            
	// WIFI/Bluetooth port       
	GPJ0CONPDN = 0xFFFF;	GPJ0PUDPDN = 0x0000;

	GPJ0DAT &= (~0x00000007);	// WIFI/Bluetooth Reset
	
	// 3V, 5V Power off
	GPH1CON &= 0xFF00FFFF;	GPH1CON |= 0x00110000;	GPH1DAT &= (~0x30);

	// Save PMIC PowerControl register
	hkc100_pmic_read(PMIC_ONOFF_REG1, &PMIC_OnOffReg1, 1);
	hkc100_pmic_read(PMIC_ONOFF_REG2, &PMIC_OnOffReg2, 1);
	
	// All Power down except BUCK3, LDO2, LDO9, Battery monitor
	hkc100_pmic_write(PMIC_ONOFF_REG1, PMIC_OnOffReg1 & (BUCK3_ON | LDO2_ON));	// buck1,buck2,ldo3,ldo4,ldo5 off
	hkc100_pmic_write(PMIC_ONOFF_REG2, PMIC_OnOffReg2 & (LDO9_ON | BATMON_ON));	// ldo6,ldo7,ldo8,battery monitor off
}

//[*]----------------------------------------------------------------------------------------------[*]
void 				hkc100_wakeup(void)
{
	hkc100_pmic_port_init();

	// Restore PMIC PowerControl register
	hkc100_pmic_write(PMIC_ONOFF_REG1, PMIC_OnOffReg1);
	hkc100_pmic_write(PMIC_ONOFF_REG2, PMIC_OnOffReg2);

	// 3V, 5V Power on
	GPH1DAT |= 0x30;

	// Power Down UP
	// WIFI Rst, BT Rst Low
	GPJ0DAT &= (~0x00000007);	// WIFI/Bluetooth Reset
}

//[*]----------------------------------------------------------------------------------------------[*]
//[*]----------------------------------------------------------------------------------------------[*]


