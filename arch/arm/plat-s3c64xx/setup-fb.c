/* linux/arch/arm/plat-s3c64xx/setup-fb.c
 *
 * Copyright 2009 Samsung Electronics
 *	Jonghun Han <jonghun.han@samsung.com>
 *	http://samsungsemi.com/
 *
 * Base S3C6410 FIMD controller configuration
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
#include <plat/regs-fb.h>
#include <plat/regs-clock.h>
#include <plat/regs-gpio.h>
#include <asm/io.h>
#include <mach/map.h>

struct platform_device; /* don't need the contents */

void s3cfb_cfg_gpio(struct platform_device *pdev)
{
        unsigned long val;
        int i, err;

        /* Must be '0' for Normal-path instead of By-pass */
        writel(0x0, S3C_HOSTIFB_MIFPCON);

        /* enable clock to LCD */
        val = readl(S3C_HCLK_GATE);
        val |= S3C_CLKCON_HCLK_LCD;
        writel(val, S3C_HCLK_GATE);

        /* select TFT LCD type (RGB I/F) */
        val = readl(S3C64XX_SPC_BASE);
        val &= ~0x3;
        val |= (1 << 0);
        writel(val, S3C64XX_SPC_BASE);

        /* VD */
        for (i = 0; i < 16; i++)
                s3c_gpio_cfgpin(S3C64XX_GPI(i), S3C_GPIO_SFN(2));

        for (i = 0; i < 12; i++)
                s3c_gpio_cfgpin(S3C64XX_GPJ(i), S3C_GPIO_SFN(2));

        /* backlight ON */
        if (gpio_is_valid(S3C64XX_GPF(15))) {
                err = gpio_request(S3C64XX_GPF(15), "GPF");

                if (err) {
                        printk(KERN_ERR "failed to request GPF for "
                                "lcd backlight control\n");
                        return;
                }

                gpio_direction_output(S3C64XX_GPF(15), 1);
        }


        /* module reset */
        if (gpio_is_valid(S3C64XX_GPN(5))) {
                err = gpio_request(S3C64XX_GPN(5), "GPN");

                if (err) {
                        printk(KERN_ERR "failed to request GPN for "
                                "lcd reset control\n");
                        return;
                }

                gpio_direction_output(S3C64XX_GPN(5), 1);
        }

        mdelay(100);

        gpio_set_value(S3C64XX_GPN(5), 0);
        mdelay(10);

        gpio_set_value(S3C64XX_GPN(5), 1);
        mdelay(10);

        gpio_free(S3C64XX_GPF(15));
        gpio_free(S3C64XX_GPN(5));

        return;
}

int s3cfb_backlight_on(struct platform_device *pdev)
{
	return 0;
}

int s3cfb_reset_lcd(struct platform_device *pdev)
{
	int err;

        /* module reset */
        if (gpio_is_valid(S3C64XX_GPN(5))) {
                err = gpio_request(S3C64XX_GPN(5), "GPN");

                if (err) {
                        printk(KERN_ERR "failed to request GPN for "
                                "lcd reset control\n");
                        return err;
                }

                gpio_direction_output(S3C64XX_GPN(5), 1);
        }

        mdelay(100);

        gpio_set_value(S3C64XX_GPN(5), 0);
        mdelay(10);

        gpio_set_value(S3C64XX_GPN(5), 1);
        mdelay(10);

        gpio_free(S3C64XX_GPF(15));
        gpio_free(S3C64XX_GPN(5));

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

