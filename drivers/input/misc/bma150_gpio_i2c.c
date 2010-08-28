//[*]----------------------------------------------------------------------------------------------[*]
//
//
// 
//  HardKernel-C100 gpio i2c driver (charles.park)
//  2009.07.22
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

//[*]----------------------------------------------------------------------------------------------[*]
#include "bma150_gpio_i2c.h"

//[*]----------------------------------------------------------------------------------------------[*]
//[*]----------------------------------------------------------------------------------------------[*]
#define	GPBCON					(*(unsigned long *)S5PC1XX_GPBCON)
#define	GPBDAT					(*(unsigned long *)S5PC1XX_GPBDAT)
#define	GPBPUD					(*(unsigned long *)S5PC1XX_GPBPUD)

#define	GPIO_I2C_SDA_CON_PORT	GPBCON
#define	GPIO_I2C_SDA_DAT_PORT	GPBDAT
#define	GPIO_SDA_PIN			0

#define	GPIO_I2C_CLK_CON_PORT	GPBCON
#define	GPIO_I2C_CLK_DAT_PORT	GPBDAT
#define	GPIO_CLK_PIN			1

//[*]----------------------------------------------------------------------------------------------[*]
#define	GPIO_CON_PORT_MASK		0xF
#define	GPIO_CON_PORT_OFFSET	0x4

#define	GPIO_CON_INPUT			0x0
#define	GPIO_CON_OUTPUT			0x1

#define	DELAY_TIME				10	// us value
#define	PORT_CHANGE_DELAY_TIME	10
//#define	DELAY_TIME				100	// us value
//#define	PORT_CHANGE_DELAY_TIME	100

//[*]----------------------------------------------------------------------------------------------[*]
//   Interrupt port init
//[*]----------------------------------------------------------------------------------------------[*]
#define	GPH0CON					(*(unsigned long *)S5PC1XX_GPH0CON)

//[*]----------------------------------------------------------------------------------------------[*]
#define	HIGH					1
#define	LOW						0

//[*]----------------------------------------------------------------------------------------------[*]
//#define	DEBUG_GPIO_I2C			// debug enable flag
#define	DEBUG_MSG(x)			printk(x)

//[*]----------------------------------------------------------------------------------------------[*]
//	static function prototype
//[*]----------------------------------------------------------------------------------------------[*]
static	void			gpio_i2c_sda_port_control(unsigned char inout);
static	void			gpio_i2c_clk_port_control(unsigned char inout);

static	unsigned char	gpio_i2c_get_sda			(void);
static	void			gpio_i2c_set_sda			(unsigned char hi_lo);
static	void			gpio_i2c_set_clk			(unsigned char hi_lo);
                                        	
static	void			gpio_i2c_start			(void);
static	void			gpio_i2c_stop			(void);
                                        	
static	void			gpio_i2c_send_ack		(void);
static	void			gpio_i2c_send_noack		(void);
static	unsigned char	gpio_i2c_chk_ack			(void);
                		                        	
static	void 			gpio_i2c_byte_write		(unsigned char wdata);
static	void			gpio_i2c_byte_read		(unsigned char *rdata);
		        		
//[*]----------------------------------------------------------------------------------------------[*]
//	extern function prototype
//[*]----------------------------------------------------------------------------------------------[*]
int 					sensor_read				(unsigned char st_reg, unsigned char *rdata, unsigned char rsize);
int 					sensor_write				(unsigned char reg, unsigned char data);
void 					sensor_port_init			(void);

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
	
	#if defined(DEBUG_GPIO_I2C)
		if(ret)		DEBUG_MSG(("%s (%d): no ack!!\n",__FUNCTION__, ret));
		else		DEBUG_MSG(("%s (%d): ack !! \n" ,__FUNCTION__, ret));
	#endif

	return	ret;
}

//[*]----------------------------------------------------------------------------------------------[*]
static	void 			gpio_i2c_byte_write		(unsigned char wdata)
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
static	void			gpio_i2c_byte_read		(unsigned char *rdata)
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
int 			bma150_sensor_write			(unsigned char reg, unsigned char data)
{
	unsigned char ack;

	// start
	gpio_i2c_start();
	
	gpio_i2c_byte_write(BMA150_SENSOR_WR_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	write_stop;
	}
	
	gpio_i2c_byte_write(reg);	// register
	
	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write register] : no ack\n",__FUNCTION__));
		#endif
	}

	gpio_i2c_byte_write(data);	// data
	
	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write data] : no ack\n",__FUNCTION__));
		#endif
	}

write_stop:
	gpio_i2c_stop();

	#if defined(DEBUG_GPIO_I2C)
		DEBUG_MSG(("%s : %d\n", __FUNCTION__, ack));
	#endif

	return	ack;
}

//[*]--------------------------------------------------------------------------------------------------[*]
int 			bma150_sensor_read			(unsigned char st_reg, unsigned char *rdata, unsigned char rsize)
{
	unsigned char ack, cnt;

	// start
	gpio_i2c_start();

	gpio_i2c_byte_write(BMA150_SENSOR_WR_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}
	
	gpio_i2c_byte_write(st_reg);	// start register

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
			DEBUG_MSG(("%s [write address] : no ack\n",__FUNCTION__));
		#endif

		goto	read_stop;
	}
	
	gpio_i2c_stop();
	
	// re-start
	gpio_i2c_start();

	gpio_i2c_byte_write(BMA150_SENSOR_RD_ADDR);	// i2c address

	if((ack = gpio_i2c_chk_ack()))	{
		#if defined(DEBUG_GPIO_I2C)
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

	#if defined(DEBUG_GPIO_I2C)
		DEBUG_MSG(("%s : %d\n", __FUNCTION__, ack));
	#endif

	return	ack;
}

//[*]----------------------------------------------------------------------------------------------[*]
void 			bma150_sensor_port_init			(void)
{
	GPBCON &= (~0x0000FFFF);	GPBCON |= 0x00001111;	
	GPBPUD &= (~0x000000FF);	GPBPUD |= 0x000000AA;	
	GPBDAT &= (~0x0000000F);	GPBDAT |= 0x0000000B;	

	GPH0CON &= (~0x00F00000);	// EINT5 Input (Hardware Bug)
	
	gpio_i2c_set_sda(HIGH);	gpio_i2c_set_clk(HIGH);
}

//[*]----------------------------------------------------------------------------------------------[*]



