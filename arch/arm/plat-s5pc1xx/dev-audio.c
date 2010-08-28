/* linux/arch/arm/plat-s3c/dev-audio.c
 *
 * Copyright 2009 Wolfson Microelectronics
 *      Mark Brown <broonie@opensource.wolfsonmicro.com>
 *

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <mach/map.h>
#include <mach/s3c-dma.h>

#include <plat/devs.h>
#include <plat/audio.h>
#include <plat/gpio-bank-c.h>
#include <plat/gpio-bank-g3.h>
#include <plat/gpio-cfg.h>
#include <plat/regs-clock.h>


static int s5pc100_cfg_i2s(struct platform_device *pdev)
{
	/* configure GPIO for i2s port */
	switch (pdev->id) {
	case 0: 
		writel((readl(S5P_LPMP_MODE_SEL) & ~0x3) | (1<<1),
						S5P_LPMP_MODE_SEL);
		writel(readl(S5P_CLKGATE_D20) | S5P_CLKGATE_D20_HCLKD2
						| S5P_CLKGATE_D20_I2SD2,
							S5P_CLKGATE_D20);
		break;

	case 1:
		s3c_gpio_cfgpin(S5PC1XX_GPC(0), S5PC1XX_GPC0_I2S_1_SCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPC(1), S5PC1XX_GPC1_I2S_1_CDCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPC(2), S5PC1XX_GPC2_I2S_1_LRCK);
		s3c_gpio_cfgpin(S5PC1XX_GPC(3), S5PC1XX_GPC3_I2S_1_SDI);
		s3c_gpio_cfgpin(S5PC1XX_GPC(4), S5PC1XX_GPC4_I2S_1_SDO);
		break;

	case 2:
		s3c_gpio_cfgpin(S5PC1XX_GPG3(0), S5PC1XX_GPG3_0_I2S_2_SCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(1), S5PC1XX_GPG3_1_I2S_2_CDCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(2), S5PC1XX_GPG3_2_I2S_2_LRCK);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(3), S5PC1XX_GPG3_3_I2S_2_SDI);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(4), S5PC1XX_GPG3_4_I2S_2_SDO);
		break;

	default:
		printk("Invalid Device %d!\n", pdev->id);
		return -EINVAL;
	}

	return 0;
}

static struct s3c_audio_pdata s3c_i2s_pdata = {
	.cfg_gpio = s5pc100_cfg_i2s,
};

static struct resource s5pc100_iis0_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_IIS0, /* V50 */
		.end   = S5PC1XX_PA_IIS0 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device s5pc100_device_iis0 = {
	.name		  = "s5p-iis",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(s5pc100_iis0_resource),
	.resource	  = s5pc100_iis0_resource,
	.dev = {
		.platform_data = &s3c_i2s_pdata,
	},
};
EXPORT_SYMBOL(s5pc100_device_iis0);

static struct resource s5pc100_iis1_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_IIS1,
		.end   = S5PC1XX_PA_IIS1 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device s5pc100_device_iis1 = {
	.name		  = "s5p-iis",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(s5pc100_iis1_resource),
	.resource	  = s5pc100_iis1_resource,
	.dev = {
		.platform_data = &s3c_i2s_pdata,
	},
};
EXPORT_SYMBOL(s5pc100_device_iis1);

static struct resource s5pc100_iis2_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_IIS2,
		.end   = S5PC1XX_PA_IIS2 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device s5pc100_device_iis2 = {
	.name		  = "s5p-iis",
	.id		  = 2,
	.num_resources	  = ARRAY_SIZE(s5pc100_iis2_resource),
	.resource	  = s5pc100_iis2_resource,
	.dev = {
		.platform_data = &s3c_i2s_pdata,
	},
};
EXPORT_SYMBOL(s5pc100_device_iis2);

/* PCM Controller platform_devices */

static int s5pc100_pcm_cfg_gpio(struct platform_device *pdev)
{
	switch (pdev->id) {
	case 0:
		s3c_gpio_cfgpin(S5PC1XX_GPG3(0), S5PC1XX_GPG3_0_PCM_0_SCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(1), S5PC1XX_GPG3_1_PCM_0_EXTCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(2), S5PC1XX_GPG3_2_PCM_0_FSYNC);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(3), S5PC1XX_GPG3_3_PCM_0_SIN);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(4), S5PC1XX_GPG3_4_PCM_0_SOUT);
		break;
	case 1:
		s3c_gpio_cfgpin(S5PC1XX_GPC(0), S5PC1XX_GPC0_PCM_1_SCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPC(1), S5PC1XX_GPC1_PCM_1_EXTCLK);
		s3c_gpio_cfgpin(S5PC1XX_GPC(2), S5PC1XX_GPC2_PCM_1_FSYNC);
		s3c_gpio_cfgpin(S5PC1XX_GPC(3), S5PC1XX_GPC3_PCM_1_SIN);
		s3c_gpio_cfgpin(S5PC1XX_GPC(4), S5PC1XX_GPC4_PCM_1_SOUT);
		break;
	default:
		printk(KERN_DEBUG "Invalid PCM Controller number!");
		return -EINVAL;
	}

	return 0;
}

static struct s3c_audio_pdata s3c_pcm_pdata = {
	.cfg_gpio = s5pc100_pcm_cfg_gpio,
};

static struct resource s5pc100_pcm0_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_PCM0,
		.end   = S5PC1XX_PA_PCM0 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = DMACH_PCM0_TX,
		.end   = DMACH_PCM0_TX,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.start = DMACH_PCM0_RX,
		.end   = DMACH_PCM0_RX,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device s5pc100_device_pcm0 = {
	.name		  = "samsung-pcm",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(s5pc100_pcm0_resource),
	.resource	  = s5pc100_pcm0_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};
EXPORT_SYMBOL(s5pc100_device_pcm0);

static struct resource s5pc100_pcm1_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_PCM1,
		.end   = S5PC1XX_PA_PCM1 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = DMACH_PCM1_TX,
		.end   = DMACH_PCM1_TX,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.start = DMACH_PCM1_RX,
		.end   = DMACH_PCM1_RX,
		.flags = IORESOURCE_DMA,
	},
};

struct platform_device s5pc100_device_pcm1 = {
	.name		  = "samsung-pcm",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(s5pc100_pcm1_resource),
	.resource	  = s5pc100_pcm1_resource,
	.dev = {
		.platform_data = &s3c_pcm_pdata,
	},
};
EXPORT_SYMBOL(s5pc100_device_pcm1);
