/* linux/drivers/video/samsung/s3cfb_lte480wv.c
 *
 * Samsung LTE480 4.8" WVGA Display Panel Support
 *
 * Jinsung Yang, Copyright (c) 2009 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
//[*]--------------------------------------------------------------------------------------------------------------------------------------------
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/delay.h>

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
#include "s3cfb.h"

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
#define	S3C_FB_SPI_CH		1	/* spi channel for module init */
#define	FRAME_CYCLE_TIME	20

static	unsigned char	InitHW = 0;		// HW Initialize Flag
//[*]--------------------------------------------------------------------------------------------------------------------------------------------
static struct s3cfb_lcd lms350df = {
	.width = 320,
	.height = 480,
	.bpp = 24,
	.freq = 60,

	.timing = {
		.h_fp = 4,
		.h_bp = 5,
		.h_sw = 3,
		.v_fp = 4,
		.v_fpe = 1,	
		.v_bp = 3,
		.v_bpe = 1,	
		.v_sw = 3,
	},

	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 1,
	},
};

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
static	void delay_ms(unsigned int count)
{
	if(InitHW)	msleep_interruptible(count);
	else		mdelay(count);
}

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
static	void backlight_enable(unsigned char status)
{
	/* backlight OFF */
	int	err;

	err = gpio_request(S5PC1XX_GPA1(3), "GPA3");

	if (err) printk(KERN_ERR "failed to request GPA3 for lcd backlight control\n");
	
	if(status)	gpio_direction_output(S5PC1XX_GPA1(3), 1);	
	else		gpio_direction_output(S5PC1XX_GPA1(3), 0);

	delay_ms(1);
	printk("%s(status : %d)\n",__FUNCTION__, status);	
	gpio_free(S5PC1XX_GPA1(3));
}

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
static	void s3cfb_spi_write(int address, int data)
{
	unsigned int 	delay = 50;
	unsigned char 	dev_id = 0x1d;
	int 			i;

	s3cfb_spi_lcd_den(S3C_FB_SPI_CH, 1);
	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
	s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
	udelay(delay);

	s3cfb_spi_lcd_den(S3C_FB_SPI_CH, 0);
	udelay(delay);

	for (i = 5; i >= 0; i--) {
		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);

		if ((dev_id >> i) & 0x1)
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
		else
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);

		udelay(delay);

		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
		udelay(delay);
	}

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);
	s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);
	udelay(delay);

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
	udelay(delay);

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);
	s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);
	udelay(delay);

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
	udelay(delay);

	for (i = 15; i >= 0; i--) {
		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);

		if ((address >> i) & 0x1)
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
		else
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);

		udelay(delay);

		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
		udelay(delay);
	}

	s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
	udelay(delay);

	s3cfb_spi_lcd_den(S3C_FB_SPI_CH, 1);
	udelay(delay * 10);

	s3cfb_spi_lcd_den(S3C_FB_SPI_CH, 0);
	udelay(delay);

	for (i = 5; i >= 0; i--) {
		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);

		if ((dev_id >> i) & 0x1)
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
		else
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);

		udelay(delay);

		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
		udelay(delay);

	}

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);
	s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
	udelay(delay);

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
	udelay(delay);

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);
	s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);
	udelay(delay);

	s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
	udelay(delay);

	for (i = 15; i >= 0; i--) {
		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 0);

		if ((data >> i) & 0x1)
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 1);
		else
			s3cfb_spi_lcd_dseri(S3C_FB_SPI_CH, 0);

		udelay(delay);

		s3cfb_spi_lcd_dclk(S3C_FB_SPI_CH, 1);
		udelay(delay);
	}

	s3cfb_spi_lcd_den(S3C_FB_SPI_CH, 1);
	delay_ms(1);
}
//[*]--------------------------------------------------------------------------------------------------------------------------------------------
static	void init_lms350df_ldi(void)
{
	if (s3cfb_spi_gpio_request(S3C_FB_SPI_CH))	{
		printk(KERN_ERR "failed to request GPIO for spi-lcd\n");	return;
	}
	
	s3cfb_spi_set_lcd_data(S3C_FB_SPI_CH);	delay_ms(5);
	
	// power on sequence
	s3cfb_spi_write(0x07, 0x0000);	delay_ms(10);

	s3cfb_spi_write(0x11, 0x222f);	s3cfb_spi_write(0x12, 0x0f00);
	s3cfb_spi_write(0x13, 0x7fe3);	s3cfb_spi_write(0x76, 0x2213);
	s3cfb_spi_write(0x74, 0x0001);	s3cfb_spi_write(0x76, 0x0000);
	s3cfb_spi_write(0x10, 0x560c);	delay_ms(FRAME_CYCLE_TIME*6);

	s3cfb_spi_write(0x12, 0x0c63);	delay_ms(FRAME_CYCLE_TIME*5);

	s3cfb_spi_write(0x01, 0x0B3B);	s3cfb_spi_write(0x02, 0x0300);
	
	// SYNC without DE Mode
	s3cfb_spi_write(0x03, 0xC040);		
	// VBP
	s3cfb_spi_write(0x08, 0x0004);		
	// HBP
	s3cfb_spi_write(0x09, 0x0008);
			
	s3cfb_spi_write(0x76, 0x2213);	s3cfb_spi_write(0x0B, 0x3340);

	s3cfb_spi_write(0x0C, 0x0020);	s3cfb_spi_write(0x1C, 0x7770);
	s3cfb_spi_write(0x76, 0x0000);	s3cfb_spi_write(0x0D, 0x0000);
	s3cfb_spi_write(0x0E, 0x0500);	s3cfb_spi_write(0x14, 0x0000);
	s3cfb_spi_write(0x15, 0x0803);	s3cfb_spi_write(0x16, 0x0000);
	s3cfb_spi_write(0x30, 0x0005);	s3cfb_spi_write(0x31, 0x070F);
	s3cfb_spi_write(0x32, 0x0300);	s3cfb_spi_write(0x33, 0x0003);
	s3cfb_spi_write(0x34, 0x090C);	s3cfb_spi_write(0x35, 0x0001);
	s3cfb_spi_write(0x36, 0x0001);	s3cfb_spi_write(0x37, 0x0303);
	s3cfb_spi_write(0x38, 0x0F09);	s3cfb_spi_write(0x39, 0x0105);

	printk("power on sequence done...\n");
	// display on sequence
	s3cfb_spi_write(0x07, 0x0001);	delay_ms(FRAME_CYCLE_TIME*1);
	s3cfb_spi_write(0x07, 0x0101);	delay_ms(FRAME_CYCLE_TIME*2);
	s3cfb_spi_write(0x07, 0x0103);
	printk("display on sequence done...\n");

	s3cfb_spi_gpio_free(S3C_FB_SPI_CH);

	InitHW = 1;	// Initialize True;	
	
	backlight_enable(1);
}

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
static	void deinit_lms350df_ldi(void)
{
	backlight_enable(0);

	if (s3cfb_spi_gpio_request(S3C_FB_SPI_CH))	{
		printk(KERN_ERR "failed to request GPIO for spi-lcd\n");	return;
	}

	s3cfb_spi_set_lcd_data(S3C_FB_SPI_CH);	delay_ms(5);

	// power off sequence
	s3cfb_spi_write(0x07, 0x0000);	s3cfb_spi_write(0x10, 0x0001);
    s3cfb_spi_write(0x0B, 0x30E1);	s3cfb_spi_write(0x07, 0x0102);

	delay_ms(FRAME_CYCLE_TIME*3);	s3cfb_spi_write(0x07, 0x0000);
    s3cfb_spi_write(0x12, 0x0000);	s3cfb_spi_write(0x10, 0x0100);
	
	// LCD stanby
	s3cfb_spi_write(0x10, 0x0001);	delay_ms(20);
	
	printk("lcd power off sequence done...\n");

	s3cfb_spi_gpio_free(S3C_FB_SPI_CH);
}

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
/* name should be fixed as 's3cfb_set_lcd_info' */
//[*]--------------------------------------------------------------------------------------------------------------------------------------------
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	lms350df.init_ldi = init_lms350df_ldi;
	lms350df.deinit_ldi = deinit_lms350df_ldi;
	ctrl->lcd = &lms350df;
}

//[*]--------------------------------------------------------------------------------------------------------------------------------------------
