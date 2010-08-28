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
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>

#include "s3cfb.h"

#ifndef CONFIG_CPU_S5PC100
#define BACKLIGHT_STATUS_ALC	0x100
#define BACKLIGHT_LEVEL_VALUE	0x0FF	/* 0 ~ 255 */

#define BACKLIGHT_LEVEL_MIN		1
#define BACKLIGHT_LEVEL_DEFAULT	(BACKLIGHT_STATUS_ALC | 0xFF)	/* Default Setting */
#define BACKLIGHT_LEVEL_MAX		(BACKLIGHT_STATUS_ALC | BACKLIGHT_LEVEL_VALUE)

int lcd_power = OFF;
EXPORT_SYMBOL(lcd_power);

void lcd_power_ctrl(s32 value);
EXPORT_SYMBOL(lcd_power_ctrl);

int backlight_power = OFF;
EXPORT_SYMBOL(backlight_power);

void backlight_power_ctrl(s32 value);
EXPORT_SYMBOL(backlight_power_ctrl);

int backlight_level = BACKLIGHT_LEVEL_DEFAULT;
EXPORT_SYMBOL(backlight_level);

void backlight_level_ctrl(s32 value);
EXPORT_SYMBOL(backlight_level_ctrl);
#endif

static struct s3cfb_lcd lte480wv = {
	.width = 800,
	.height = 480,
	.bpp = 24,
	.freq = 60,

	.timing = {
		.h_fp = 8,
		.h_bp = 13,
		.h_sw = 3,
		.v_fp = 5,
		.v_fpe = 1,
		.v_bp = 7,
		.v_bpe = 1,
		.v_sw = 1,
	},

	.polarity = {
		.rise_vclk = 0,
		.inv_hsync = 1,
		.inv_vsync = 1,
		.inv_vden = 0,
	},
};

/* name should be fixed as 's3cfb_set_lcd_info' */
void s3cfb_set_lcd_info(struct s3cfb_global *ctrl)
{
	lte480wv.init_ldi = NULL;
	ctrl->lcd = &lte480wv;
}

#ifndef CONFIG_CPU_S5PC100
#if  defined(CONFIG_S3C6410_PWM)
extern void s3cfb_set_brightness(int val);
#else
void s3cfb_set_brightness(int val){}
#endif
void lcd_power_ctrl(s32 value)
{
	int err;

	if (value) {
		if (gpio_is_valid(S3C64XX_GPN(5))) {
			err = gpio_request(S3C64XX_GPN(5), "GPN");

			if (err) {
				printk(KERN_ERR "failed to request GPN for "
					"lcd reset control\n");
			}
			gpio_direction_output(S3C64XX_GPN(5), 1);
		}
	}
	else {
		if (gpio_is_valid(S3C64XX_GPN(5))) {
			err = gpio_request(S3C64XX_GPN(5), "GPN");

			if (err) {
				printk(KERN_ERR "failed to request GPN for "
					"lcd reset control\n");
			}
			gpio_direction_output(S3C64XX_GPN(5), 0);
		}
	}
	gpio_free(S3C64XX_GPN(5));
	lcd_power = value;
}

static void backlight_ctrl(s32 value)
{
	int err;

	if (value) {
		/* backlight ON */
		if (lcd_power == OFF)
			lcd_power_ctrl(ON);
		
		if (gpio_is_valid(S3C64XX_GPF(15))) {
			err = gpio_request(S3C64XX_GPF(15), "GPF");

			if (err) {
				printk(KERN_ERR "failed to request GPF for "
					"lcd backlight control\n");
			}

			gpio_direction_output(S3C64XX_GPF(15), 1);
		 }
		s3cfb_set_brightness((int)(value/3));
	}
	else {
		lcd_power_ctrl(OFF);
		/* backlight OFF */
		if (gpio_is_valid(S3C64XX_GPF(15))) {
			err = gpio_request(S3C64XX_GPF(15), "GPF");

			if (err) {
				printk(KERN_ERR "failed to request GPF for "
					"lcd backlight control\n");
			}

			gpio_direction_output(S3C64XX_GPF(15), 0);
		 }
	}
	gpio_free(S3C64XX_GPF(15));
}

void backlight_level_ctrl(s32 value)
{
	if ((value < BACKLIGHT_LEVEL_MIN) ||	/* Invalid Value */
		(value > BACKLIGHT_LEVEL_MAX) ||
		(value == backlight_level))	/* Same Value */
		return;

	if (backlight_power)
		s3cfb_set_brightness((int)(value/3));	
	
	backlight_level = value;	
}

void backlight_power_ctrl(s32 value)
{
	if ((value < OFF) ||	/* Invalid Value */
		(value > ON) ||
		(value == backlight_power))	/* Same Value */
		return;

	backlight_ctrl((value ? backlight_level : OFF));	
	
	backlight_power = value;	
}

#define SMDK_DEFAULT_BACKLIGHT_BRIGHTNESS	255
static DEFINE_MUTEX(smdk_backlight_lock);

static void smdk_set_backlight_level(u8 level)
{
	if (backlight_level == level)
		return;

	backlight_ctrl(level);
	
	backlight_level = level;
}

static void smdk_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	mutex_lock(&smdk_backlight_lock);
	smdk_set_backlight_level(value);
	mutex_unlock(&smdk_backlight_lock);
}

static struct led_classdev smdk_backlight_led  = {
	.name		= "lcd-backlight",
	.brightness = SMDK_DEFAULT_BACKLIGHT_BRIGHTNESS,
	.brightness_set = smdk_brightness_set,
};

static int smdk_bl_probe(struct platform_device *pdev)
{
#ifdef CONFIG_LEDS_CLASS
	led_classdev_register(&pdev->dev, &smdk_backlight_led);
#endif
	return 0;
}

static int smdk_bl_remove(struct platform_device *pdev)
{
#ifdef CONFIG_LEDS_CLASS
	led_classdev_unregister(&smdk_backlight_led);
#endif
	return 0;
}

#ifdef CONFIG_PM
static int smdk_bl_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_LEDS_CLASS
	led_classdev_suspend(&smdk_backlight_led);
#endif
	return 0;
}

static int smdk_bl_resume(struct platform_device *dev)
{
#ifdef CONFIG_LEDS_CLASS
	led_classdev_resume(&smdk_backlight_led);
	return 0;
#endif
}
#else
#define smdk_bl_suspend	NULL
#define smdk_bl_resume	NULL
#endif

static struct platform_driver smdk_bl_driver = {
	.probe		= smdk_bl_probe,
	.remove		= smdk_bl_remove,
	.suspend	= smdk_bl_suspend,
	.resume		= smdk_bl_resume,
	.driver		= {
		.name	= "smdk-backlight",
	},
};

static int __init smdk_bl_init(void)
{
	printk("SMDK board LCD Backlight Device Driver (c) 2008 Samsung Electronics \n");

	platform_driver_register(&smdk_bl_driver);
	return 0;
}

static void __exit smdk_bl_exit(void)
{
 	platform_driver_unregister(&smdk_bl_driver);
}

module_init(smdk_bl_init);
module_exit(smdk_bl_exit);

MODULE_AUTHOR("Jongpill Lee <boyko.lee@samsung.com>");
MODULE_DESCRIPTION("SMDK board Backlight Driver");
MODULE_LICENSE("GPL");
#endif
