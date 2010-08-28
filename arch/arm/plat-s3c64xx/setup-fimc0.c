/* linux/arch/arm/plat-s5pc11x/setup-fimc0.c
 *
 * Copyright 2009 Samsung Electronics
 *	Jinsung Yang <jsgood.yang@samsung.com>
 *	http://samsungsemi.com/
 *
 * Base S5PC11X FIMC controller 0 gpio configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-bank-b.h>
#include <plat/gpio-bank-f.h>
#include <asm/io.h>
#include <mach/map.h>

struct platform_device; /* don't need the contents */

void s3c_fimc0_cfg_gpio(struct platform_device *pdev)
{
	int i;

	s3c_gpio_cfgpin(S3C64XX_GPF(0), S3C64XX_GPF0_CAMIF_CLK);
	s3c_gpio_cfgpin(S3C64XX_GPF(1), S3C64XX_GPF1_CAMIF_HREF);
	s3c_gpio_cfgpin(S3C64XX_GPF(2), S3C64XX_GPF2_CAMIF_PCLK);
	s3c_gpio_cfgpin(S3C64XX_GPF(3), S3C64XX_GPF3_CAMIF_nRST);
	s3c_gpio_cfgpin(S3C64XX_GPF(4), S3C64XX_GPF4_CAMIF_VSYNC);
	s3c_gpio_cfgpin(S3C64XX_GPF(5), S3C64XX_GPF5_CAMIF_YDATA0);
	s3c_gpio_cfgpin(S3C64XX_GPF(6), S3C64XX_GPF6_CAMIF_YDATA1);
	s3c_gpio_cfgpin(S3C64XX_GPF(7), S3C64XX_GPF7_CAMIF_YDATA2);
	s3c_gpio_cfgpin(S3C64XX_GPF(8), S3C64XX_GPF8_CAMIF_YDATA3);
	s3c_gpio_cfgpin(S3C64XX_GPF(9), S3C64XX_GPF9_CAMIF_YDATA4);
	s3c_gpio_cfgpin(S3C64XX_GPF(10), S3C64XX_GPF10_CAMIF_YDATA5);
	s3c_gpio_cfgpin(S3C64XX_GPF(11), S3C64XX_GPF11_CAMIF_YDATA6);
	s3c_gpio_cfgpin(S3C64XX_GPF(12), S3C64XX_GPF12_CAMIF_YDATA7);
	s3c_gpio_cfgpin(S3C64XX_GPB(4), S3C64XX_GPB4_CAM_FIELD);

	for (i = 0; i < 12; i++)
		s3c_gpio_setpull(S3C64XX_GPF(i), S3C_GPIO_PULL_UP);

	s3c_gpio_setpull(S3C64XX_GPB(4), S3C_GPIO_PULL_UP);


	/* drive strength to max */
//	writel(0xc0, S5PC11X_VA_GPIO + 0x10c);
//	writel(0x300, S5PC11X_VA_GPIO + 0x26c);
}

int s3c_fimc_clk_on(struct platform_device *pdev, struct clk *clk)
{
	struct clk *lclk = NULL, *lclk_parent = NULL;
	int err;

//	lclk = clk_get(&pdev->dev, "lclk_fimc");
	lclk = clk_get(&pdev->dev, "sclk_cam");
	if (IS_ERR(lclk)) {
		dev_err(&pdev->dev, "failed to get local clock\n");
		goto err_clk1;
	}

	if (lclk->set_parent) {
		lclk_parent = clk_get(&pdev->dev, "clk_hx2");
		if (IS_ERR(lclk_parent)) {
			dev_err(&pdev->dev, "failed to get parent of local clock\n");
			goto err_clk2;
		}

		lclk->parent = lclk_parent;

		err = lclk->set_parent(lclk, lclk_parent);
		if (err) {
			dev_err(&pdev->dev, "failed to set parent of local clock\n");
			goto err_clk3;
		}

		if (lclk->set_rate) {
//			lclk->set_rate(lclk, 166000000);
			lclk->set_rate(lclk, 133000000);
			dev_info(&pdev->dev, "set local clock rate to 133000000\n");
		}

		clk_put(lclk_parent);
	}

	clk_put(lclk);

	/* be able to handle clock on/off only with this clock */
	clk = clk_get(&pdev->dev, "fimc");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get interface clock\n");
		goto err_clk3;
	}

	clk_enable(clk);

	return 0;

err_clk3:
	if (lclk_parent)
		clk_put(lclk_parent);

err_clk2:
	clk_put(lclk);

err_clk1:
	return -EINVAL;
}

int s3c_fimc0_clk_off(struct platform_device *pdev, struct clk *clk)
{
	clk_disable(clk);
	clk_put(clk);

	clk = NULL;

	return 0;
}

