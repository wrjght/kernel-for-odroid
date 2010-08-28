/* linux/arch/arm/plat-s5pc1xx/setup-fimc0.c
 *
 * Copyright 2009 Samsung Electronics
 *	Jinsung Yang <jsgood.yang@samsung.com>
 *	http://samsungsemi.com/
 *
 * Base S5PC1XX FIMC controller 0 gpio configuration
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
#include <plat/gpio-bank-e0.h>
#include <plat/gpio-bank-e1.h>
#include <plat/gpio-bank-h2.h>
#include <plat/gpio-bank-h3.h>

struct platform_device; /* don't need the contents */

void s3c_fimc0_cfg_gpio(struct platform_device *pdev)
{
	int i;
	s3c_gpio_cfgpin(S5PC1XX_GPE0(0), S5PC1XX_GPE0_0_CAM_A_PCLK);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(1), S5PC1XX_GPE0_1_CAM_A_VSYNC);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(2), S5PC1XX_GPE0_2_CAM_A_HREF);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(3), S5PC1XX_GPE0_3_CAM_A_DATA_0);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(4), S5PC1XX_GPE0_4_CAM_A_DATA_1);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(5), S5PC1XX_GPE0_5_CAM_A_DATA_2);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(6), S5PC1XX_GPE0_6_CAM_A_DATA_3);
	s3c_gpio_cfgpin(S5PC1XX_GPE0(7), S5PC1XX_GPE0_7_CAM_A_DATA_4);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(0), S5PC1XX_GPE1_0_CAM_A_DATA_5);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(1), S5PC1XX_GPE1_1_CAM_A_DATA_6);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(2), S5PC1XX_GPE1_2_CAM_A_DATA_7);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(3), S5PC1XX_GPE1_3_CAM_A_CLKOUT);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(4), S5PC1XX_GPE1_4_CAM_A_RESET);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(5), S5PC1XX_GPE1_5_CAM_A_FIELD);

	for (i = 0; i < 8; i++)
		s3c_gpio_setpull(S5PC1XX_GPE0(i), S3C_GPIO_PULL_UP);

	for (i = 0; i < 6; i++)
		s3c_gpio_setpull(S5PC1XX_GPE1(i), S3C_GPIO_PULL_UP);

#if 0
	/* This configuration setting is for CAM B Port.
	 * This CAM B Port is not used in SMDKC100 B'd.
	 * Because it is H/W muxed with Keypad.
	 */
	s3c_gpio_cfgpin(S5PC1XX_GPH2(0), S5PC1XX_GPH2_0_CAM_B_DATA_0);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(1), S5PC1XX_GPH2_1_CAM_B_DATA_1);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(2), S5PC1XX_GPH2_2_CAM_B_DATA_2);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(3), S5PC1XX_GPH2_3_CAM_B_DATA_3);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(4), S5PC1XX_GPH2_4_CAM_B_DATA_4);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(5), S5PC1XX_GPH2_5_CAM_B_DATA_5);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(6), S5PC1XX_GPH2_6_CAM_B_DATA_6);
	s3c_gpio_cfgpin(S5PC1XX_GPH2(7), S5PC1XX_GPH2_7_CAM_B_DATA_7);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(0), S5PC1XX_GPH3_0_CAM_B_PCLK);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(1), S5PC1XX_GPH3_1_CAM_B_VSYNC);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(2), S5PC1XX_GPH3_2_CAM_B_HREF);
	s3c_gpio_cfgpin(S5PC1XX_GPH3(3), S5PC1XX_GPH3_3_CAM_B_FIELD);
	s3c_gpio_cfgpin(S5PC1XX_GPE1(3), S5PC1XX_GPE1_3_CAM_A_CLKOUT);

	for (i = 0; i < 8; i++)
		s3c_gpio_setpull(S5PC1XX_GPH2(i), S3C_GPIO_PULL_UP);

	for (i = 0; i < 4; i++)
		s3c_gpio_setpull(S5PC1XX_GPH3(i), S3C_GPIO_PULL_UP);
#endif
}

int s3c_fimc_clk_on(struct platform_device *pdev, struct clk *clk)
{
	struct clk *parent = NULL, *sclk = NULL;
	int err;

	sclk = clk_get(&pdev->dev, "sclk_fimc");
	if (IS_ERR(sclk)) {
		dev_err(&pdev->dev, "failed to get fimc core clock\n");
		goto err_clk1;
	}

	if (sclk->set_parent) {
		parent = clk_get(&pdev->dev, "dout_mpll");
		if (IS_ERR(parent)) {
			dev_err(&pdev->dev, "failed to get parent of interface clock\n");
			goto err_clk2;
		}

		sclk->parent = parent;

		err = sclk->set_parent(sclk, parent);
		if (err) {
			dev_err(&pdev->dev, "failed to set parent of interface clock\n");
			goto err_clk3;
		}

		if (sclk->set_rate) {
			sclk->set_rate(sclk, 133000000);
			dev_info(&pdev->dev, "set interface clock rate to 133000000\n");
		}

		clk_put(parent);
	}

	clk_enable(sclk);

	clk = clk_get(&pdev->dev, "fimc");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "failed to get fimc bus clock\n");
		goto err_clk3;
	}

	clk_enable(clk);

	return 0;

err_clk3:
	if (parent != NULL)
		clk_put(parent);

err_clk2:
	if (sclk != NULL)
		clk_put(sclk);

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
