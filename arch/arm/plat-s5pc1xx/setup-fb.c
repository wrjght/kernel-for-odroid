/* linux/arch/arm/plat-s5pc1xx/setup-fb.c
 *
 * Copyright 2009 Samsung Electronics
 *	Jonghun Han <jonghun.han@samsung.com>
 *	http://samsungsemi.com/
 *
 * Base S5PC1XX FIMD controller configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/fb.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>
#include <asm/io.h>
#include <mach/map.h>

struct platform_device; /* don't need the contents */

#ifdef CONFIG_FB_S3C_LTE480WV
void s3cfb_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF0(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF1(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF2(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 4; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF3(i), S3C_GPIO_SFN(2));
}

int s3cfb_backlight_on(struct platform_device *pdev)
{
#if !defined(CONFIG_HAVE_PWM)
	int err;

	err = gpio_request(S5PC1XX_GPD(0), "GPD");
	if (err) {
		printk(KERN_ERR "failed to request GPD for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PC1XX_GPD(0), 1);
	gpio_free(S5PC1XX_GPD(0));
#endif
	return 0;
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PC1XX_GPH0(6), "GPH0");
	if (err) {
		printk(KERN_ERR "failed to request GPH0 for "
			"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PC1XX_GPH0(6), 1);
	mdelay(100);

	gpio_set_value(S5PC1XX_GPH0(6), 0);
	mdelay(10);

	gpio_set_value(S5PC1XX_GPH0(6), 1);
	mdelay(10);

	gpio_free(S5PC1XX_GPH0(6));

	return 0;
}

int s3cfb_clk_on(struct platform_device *pdev, struct clk **s3cfb_clk)
{
	struct clk *clk = NULL;

	clk = clk_get(&pdev->dev, "fimd"); /* Core, Pixel source clock */
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get fimd clock source\n");
		return -EINVAL;
	}
	clk_enable(clk);

	*s3cfb_clk = clk;
	
	return 0;
}

int s3cfb_clk_off(struct platform_device *pdev, struct clk **clk)
{
	clk_disable(*clk);
	clk_put(*clk);

	*clk = NULL;
	
	return 0;
}

void s3cfb_get_clk_name(char *clk_name)
{
	strcpy(clk_name, "fimd");
}
#elif defined(CONFIG_FB_S3C_LMS350DF)
void s3cfb_cfg_gpio(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF0(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF1(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 8; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF2(i), S3C_GPIO_SFN(2));

	for (i = 0; i < 4; i++)
		s3c_gpio_cfgpin(S5PC1XX_GPF3(i), S3C_GPIO_SFN(2));
}

int s3cfb_backlight_on(struct platform_device *pdev)
{
#if !defined(CONFIG_HAVE_PWM)
	int err;

	err = gpio_request(S5PC1XX_GPD(0), "GPD");
	if (err) {
		printk(KERN_ERR "failed to request GPD for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PC1XX_GPD(0), 1);
	gpio_free(S5PC1XX_GPA1(3));

	err = gpio_request(S5PC1XX_GPD(0), "GPA1");
	if (err) {
		printk(KERN_ERR "failed to request GPA1 for "
			"lcd backlight control\n");
		return err;
	}

	gpio_direction_output(S5PC1XX_GPA1(3), 1);
	gpio_free(S5PC1XX_GPA1(3));
#endif
	return 0;
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
	int err;

	err = gpio_request(S5PC1XX_GPB(4), "GPB4");
	if (err) {
		printk(KERN_ERR "failed to request GPB4 for "
			"lcd reset control\n");
		return err;
	}

	gpio_direction_output(S5PC1XX_GPB(4), 1);
	mdelay(100);

	gpio_set_value(S5PC1XX_GPB(4), 0);
	mdelay(10);

	gpio_set_value(S5PC1XX_GPB(4), 1);
	mdelay(10);

	gpio_free(S5PC1XX_GPB(4));

	return 0;
}

int s3cfb_clk_on(struct platform_device *pdev, struct clk **s3cfb_clk)
{
	struct clk *clk = NULL;

	clk = clk_get(&pdev->dev, "fimd"); /* Core, Pixel source clock */
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get fimd clock source\n");
		return -EINVAL;
	}
	clk_enable(clk);

	*s3cfb_clk = clk;

	return 0;
}

int s3cfb_clk_off(struct platform_device *pdev, struct clk **clk)
{
	clk_disable(*clk);
	clk_put(*clk);

	*clk = NULL;

	return 0;
}

void s3cfb_get_clk_name(char *clk_name)
{
	strcpy(clk_name, "fimd");
}
#endif

