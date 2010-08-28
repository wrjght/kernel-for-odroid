//--------------------------------------------------------------------------------------------------
//
//  Samsung wm8991 gpio i2c for audio
//  2009.07.09
//
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/gpio.h>
//#include <linux/i2c.h>

#include <plat/gpio-cfg.h>
#include <plat/regs-gpio.h>

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
#include <asm/delay.h>
//#include "wm8991_gpio_i2c.h"

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
//#define		DEBUG_AUDIO_GPIO_I2C

#define 	WM8991_GPIO_NAME	"GPJ0"
#define 	WM8991_GPIO_MODE	S5PC1XX_GPJ0(2)
#define 	WM8991_GPIO_CSB		S5PC1XX_GPJ0(3)
#define 	WM8991_GPIO_CLK		S5PC1XX_GPJ0(4)
#define 	WM8991_GPIO_SDA		S5PC1XX_GPJ0(5)
#define 	WM8991_GPIO_CLK_CHK_BIT	(0x0 | (1<<4))
#define 	WM8991_GPIO_SDA_CHK_BIT	(0x0 | (1<<5))

#define		WM8991_CHIP_ADDR		0x34
#define		WM8991_WRITE			0x00
#define		WM8991_READ				0x01

#define		STANDARD_DELAY
#define 	WM8991_I2C_DELAY		10		// device timing modify

#define		MAX_DEVICE_COUNT		20

#define		HIGH		1
#define		LOW			0

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// Delay Function
#ifndef	STANDARD_DELAY
	static	void	delay_func(unsigned char x);
	#define			DELAY_FUNC(x)			delay_func(x)

	static	void	delay_func	(unsigned char x)
	{
		unsigned char	cmp;
		unsigned int	cnt;

		for(cnt = 0; cnt < 0x100; cnt++)
			for(cmp = 0; cmp < x; cmp++)	{	cmp++; cmp--;	}
	}
#else
	#define			DELAY_FUNC(x)			udelay(x)
#endif

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int	wm8991_gpio_i2c_clk( int value )
{
	int err;

	if (gpio_is_valid(WM8991_GPIO_CLK))
	{
		err = gpio_request( WM8991_GPIO_CLK, WM8991_GPIO_NAME );
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c clk..\n");
			return err;
		}
		if(value)	gpio_direction_input(WM8991_GPIO_CLK);
		else 		gpio_direction_output(WM8991_GPIO_CLK, value);
		s3c_gpio_setpull(WM8991_GPIO_SDA, S3C_GPIO_PULL_UP);
		gpio_free(WM8991_GPIO_CLK);
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int	wm8991_gpio_i2c_sda(int value )
{
	int err;

	if (gpio_is_valid(WM8991_GPIO_SDA))
	{
		err = gpio_request( WM8991_GPIO_SDA, WM8991_GPIO_NAME );
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c clk..\n");
			return err;
		}
		if(value)	gpio_direction_input(WM8991_GPIO_SDA);
		else		gpio_direction_output(WM8991_GPIO_SDA, value);
		s3c_gpio_setpull(WM8991_GPIO_SDA, S3C_GPIO_PULL_UP);
		gpio_free(WM8991_GPIO_SDA);
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int	wm8991_gpio_i2c_setinput_clk(void)
{
	int err;

	if (gpio_is_valid(WM8991_GPIO_CLK))
	{
		err = gpio_request( WM8991_GPIO_CLK, WM8991_GPIO_NAME );
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c clk..\n");
			return err;
		}
		gpio_direction_input(WM8991_GPIO_CLK);
		gpio_free(WM8991_GPIO_CLK);
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int	wm8991_gpio_i2c_setinput_sda(void)
{
	int err;

	if (gpio_is_valid(WM8991_GPIO_SDA))
	{
		err = gpio_request( WM8991_GPIO_SDA, WM8991_GPIO_NAME );
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c clk..\n");
			return err;
		}
		gpio_direction_input(WM8991_GPIO_SDA);
		gpio_free(WM8991_GPIO_SDA);
	}
	return 0;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void	wm8991_gpio_i2c_start(void)
{
	// i2c start condition gpio init
	// Setup SDA, SCL output High
	wm8991_gpio_i2c_clk(HIGH);
	wm8991_gpio_i2c_sda(HIGH);
	DELAY_FUNC(WM8991_I2C_DELAY);

	// SDA low before SCL low
	wm8991_gpio_i2c_sda(LOW); 	DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_clk(LOW);	DELAY_FUNC(WM8991_I2C_DELAY);

	return;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void	wm8991_gpio_i2c_stop(void)
{
	// i2c stop condition gpio init
	// Setup SDA, SCL output low
	wm8991_gpio_i2c_clk(LOW);
	wm8991_gpio_i2c_sda(LOW);	DELAY_FUNC(WM8991_I2C_DELAY);

	// SDA high after SCL high
	wm8991_gpio_i2c_clk(HIGH);	DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_sda(HIGH);	DELAY_FUNC(WM8991_I2C_DELAY);


	return;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void	wm8991_gpio_i2c_send_ack(void)	// master to slave
{
	#if	defined(DEBUG_AUDIO_GPIO_I2C)
		printk("%s [%d] \r\n"	, __FUNCTION__, __LINE__);
	#endif

	wm8991_gpio_i2c_sda(LOW);		DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_clk(HIGH);		DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_clk(LOW);		DELAY_FUNC(WM8991_I2C_DELAY);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void	wm8991_gpio_i2c_send_noack	(void)	// master to slave
{
	#if	defined(DEBUG_AUDIO_GPIO_I2C)
		printk("%s [%d] \r\n"	, __FUNCTION__, __LINE__);
	#endif

	wm8991_gpio_i2c_sda(HIGH);		DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_clk(HIGH);		DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_clk(LOW);		DELAY_FUNC(WM8991_I2C_DELAY);
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
unsigned char	wm8991_gpio_i2c_chk_ack	(void)
{
	unsigned char	count = 0, ret = 0;

	wm8991_gpio_i2c_sda(LOW);		DELAY_FUNC(WM8991_I2C_DELAY);

	wm8991_gpio_i2c_setinput_sda();		DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_clk(HIGH);			DELAY_FUNC(WM8991_I2C_DELAY);

	while( gpio_get_value(WM8991_GPIO_SDA) & WM8991_GPIO_SDA_CHK_BIT )
	{
		if(count++ > 100)
		{
			ret = 1;
			printk("%s : Get ACK Fail...!!!\r\n"	, __FUNCTION__);
			break;
		}
		else					DELAY_FUNC(100);
	}

	wm8991_gpio_i2c_clk(LOW);		DELAY_FUNC(WM8991_I2C_DELAY);
	wm8991_gpio_i2c_sda(HIGH);		DELAY_FUNC(WM8991_I2C_DELAY);

	#if	defined(DEBUG_AUDIO_GPIO_I2C)
		printk("%s : ACK(%d)\r\n"	, __FUNCTION__, ret);
	#endif

	return	ret;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void 	wm8991_gpio_i2c_byte_read( unsigned char *rdata)
{
	unsigned char	cnt, mask;

	wm8991_gpio_i2c_setinput_sda();		DELAY_FUNC(WM8991_I2C_DELAY);

	for(cnt = 0, mask = 0x80, *rdata = 0; cnt < 8; cnt++, mask >>= 1)	{
		wm8991_gpio_i2c_clk(HIGH);	DELAY_FUNC(WM8991_I2C_DELAY);

		if(gpio_get_value(WM8991_GPIO_SDA) & WM8991_GPIO_SDA_CHK_BIT)	*rdata |= mask;

		#if	defined(DEBUG_AUDIO_GPIO_I2C)
			if(*rdata & mask)	printk("%s : SDA(1)\r\n"	, __FUNCTION__);
			else				printk("%s : SDA(0)\r\n"	, __FUNCTION__);
		#endif

		wm8991_gpio_i2c_clk(LOW);	DELAY_FUNC(WM8991_I2C_DELAY);
	}

	#if	defined(DEBUG_AUDIO_GPIO_I2C)
		printk("%s : rdata = 0x%02X\r\n"	, __FUNCTION__, *rdata);
	#endif
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
void	wm8991_gpio_i2c_byte_write( unsigned char wdata)
{
	unsigned char	cnt, mask;

	for(cnt = 0, mask = 0x80; cnt < 8; cnt++, mask >>= 1)	{
		if(wdata & mask)	wm8991_gpio_i2c_sda(HIGH);
		else				wm8991_gpio_i2c_sda(LOW);
		wm8991_gpio_i2c_clk(HIGH);	DELAY_FUNC(WM8991_I2C_DELAY);
		wm8991_gpio_i2c_clk(LOW);	DELAY_FUNC(WM8991_I2C_DELAY);
	}
}


//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int wm8991_hw_write( void *i2c_dev, const char *buff, int count)
{
	//i2c_client->addr // chip address
	unsigned char chip_addr, reg, msbyte, lsbyte;

	chip_addr = WM8991_CHIP_ADDR;
	reg = buff[0];
	msbyte = buff[1];
	lsbyte = buff[2];

	// I2C Start
	wm8991_gpio_i2c_start();

	// Write Byte Device ID
	chip_addr |= WM8991_WRITE;
	wm8991_gpio_i2c_byte_write( chip_addr );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 Device ID Get ACK Fail...\n");

	// Write Byte Reg Index
	wm8991_gpio_i2c_byte_write( reg );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 Reg Index Get ACK Fail...\n");

	// Write Byte reg value MSByte 8Bit
	wm8991_gpio_i2c_byte_write( msbyte );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 value msbyte Get ACK Fail...\n");

	// Write Byte reg value LSByte 8Bit
	wm8991_gpio_i2c_byte_write( lsbyte );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 value lsbyte Get ACK Fail...\n");

	// I2C Stop
	wm8991_gpio_i2c_stop();

	return 0;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
int wm8991_hw_read( void *i2c_client, char *buff, int count)
{
	//i2c_client->addr // chip address
	unsigned char chip_addr, reg, msbyte, lsbyte;
	chip_addr = WM8991_CHIP_ADDR;
	reg = buff[0];

	// I2C Start
	wm8991_gpio_i2c_start();

	// Write Byte Device ID
	chip_addr |= WM8991_WRITE;
	wm8991_gpio_i2c_byte_write( chip_addr );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 Device ID Get ACK Fail...\n");

	// Write Byte Reg Index
	wm8991_gpio_i2c_byte_write( reg );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 Reg Index Get ACK Fail...\n");

	// I2C Restart
	wm8991_gpio_i2c_stop();
	wm8991_gpio_i2c_start();

	// Write Byte Device ID(Read 0x35)
	chip_addr |= WM8991_READ;
	wm8991_gpio_i2c_byte_write( chip_addr );

	// ACK Read
	if(wm8991_gpio_i2c_chk_ack()) printk(KERN_INFO "WM8991 Device ID Get ACK Fail...\n");

	// Read Byte reg value MSByte 8Bit
	wm8991_gpio_i2c_byte_read( &msbyte );

	// ACK Send
	wm8991_gpio_i2c_send_ack();

	// Read Byte reg value LSByte 8Bit
	wm8991_gpio_i2c_byte_read( &lsbyte );

	// NO_ACK Send
	wm8991_gpio_i2c_send_noack();

	// I2C Stop
	wm8991_gpio_i2c_stop();

	buff[1] = msbyte;
	buff[2] = lsbyte;

	return 0;
}

//--------------------------------------------------------------------------------------------------
// wm8991_i2c_gpio_init
// MODE : Low, CSB : Low
// Interface : 2-wire, DeviceID : 0x34(W), 0x35(R)
//--------------------------------------------------------------------------------------------------
int wm8991_i2c_gpio_init( void )
{
	int err;

#if 0
	if (gpio_is_valid(WM8991_GPIO_MODE))
	{
		err = gpio_request( WM8991_GPIO_MODE, WM8991_GPIO_NAME );
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c mode..\n");
			return err;
		}
		gpio_direction_output(WM8991_GPIO_MODE, LOW);
		gpio_set_value(WM8991_GPIO_MODE, LOW);
		gpio_free(WM8991_GPIO_MODE);
	}
#endif

	if (gpio_is_valid(WM8991_GPIO_CSB))
	{
		err = gpio_request( WM8991_GPIO_CSB, WM8991_GPIO_NAME );
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c csb..\n");
			return err;
		}
		gpio_direction_output(WM8991_GPIO_CSB, LOW);
		gpio_set_value(WM8991_GPIO_CSB, LOW);
		gpio_free(WM8991_GPIO_CSB);
	}
	printk(KERN_INFO "WM8991 Control Interface : 2-Wire Mode, CSB Low\n");

	return 0;
}
