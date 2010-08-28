/* linux/arch/arm/plat-s5pc1xx/dev-spi.c
 *
 * Copyright (C) 2009 Samsung Electronics Ltd.
 *	Jaswinder Singh <jassi.brar@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <mach/dma.h>
#include <mach/map.h>
#include <mach/gpio.h>

#include <plat/s5p-spi.h>
#include <plat/spi.h>
#include <plat/gpio-bank-b.h>
#include <plat/gpio-bank-g3.h>
#include <plat/gpio-cfg.h>
#include <plat/irqs.h>

static char *spi_src_clks[] = {"pclk", "sclk_spi_48", "sclk_spi"};

/* SPI Controller platform_devices */

/* Since we emulate multi-cs capability, we do not touch the GPC-3,7.
 * The emulated CS is toggled by board specific mechanism, as it can
 * be either some immediate GPIO or some signal out of some other
 * chip in between ... or some yet another way.
 * We simply do not assume anything about CS.
 */
static int s5pc1xx_spi_cfg_gpio(struct platform_device *pdev)
{
	switch (pdev->id) {
	case 0:
		s3c_gpio_cfgpin(S5PC1XX_GPB(0), S5PC1XX_GPB0_SPI_MISO0);
		s3c_gpio_cfgpin(S5PC1XX_GPB(1), S5PC1XX_GPB1_SPI_CLK0);
		s3c_gpio_cfgpin(S5PC1XX_GPB(2), S5PC1XX_GPB2_SPI_MOSI0);
		s3c_gpio_setpull(S5PC1XX_GPB(0), S3C_GPIO_PULL_UP);
		s3c_gpio_setpull(S5PC1XX_GPB(1), S3C_GPIO_PULL_UP);
		s3c_gpio_setpull(S5PC1XX_GPB(2), S3C_GPIO_PULL_UP);
		break;

	case 1:
#if defined(CONFIG_MACH_HKDKC100)
		s3c_gpio_cfgpin(S5PC1XX_GPB(4), S5PC1XX_GPB4_SPI_MISO1);
#endif
		s3c_gpio_cfgpin(S5PC1XX_GPB(5), S5PC1XX_GPB5_SPI_CLK1);
		s3c_gpio_cfgpin(S5PC1XX_GPB(6), S5PC1XX_GPB6_SPI_MOSI1);
#if defined(CONFIG_MACH_HKDKC100)
		s3c_gpio_setpull(S5PC1XX_GPB(4), S3C_GPIO_PULL_UP);
#endif
		s3c_gpio_setpull(S5PC1XX_GPB(5), S3C_GPIO_PULL_UP);
		s3c_gpio_setpull(S5PC1XX_GPB(6), S3C_GPIO_PULL_UP);
		break;

	case 2:
		s3c_gpio_cfgpin(S5PC1XX_GPG3(0), S5PC1XX_GPG3_0_SPI_CLK2);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(2), S5PC1XX_GPG3_2_SPI_MISO2);
		s3c_gpio_cfgpin(S5PC1XX_GPG3(3), S5PC1XX_GPG3_3_SPI_MOSI2);
		s3c_gpio_setpull(S5PC1XX_GPG3(0), S3C_GPIO_PULL_UP);
		s3c_gpio_setpull(S5PC1XX_GPG3(2), S3C_GPIO_PULL_UP);
		s3c_gpio_setpull(S5PC1XX_GPG3(3), S3C_GPIO_PULL_UP);
		break;

	default:
		dev_err(&pdev->dev, "Invalid SPI Controller number!");
		return -EINVAL;
	}

	return 0;
}

static struct resource s5pc1xx_spi0_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_SPI0,
		.end   = S5PC1XX_PA_SPI0 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = DMACH_SPI0_OUT,
		.end   = DMACH_SPI0_OUT,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.start = DMACH_SPI0_IN,
		.end   = DMACH_SPI0_IN,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.start = IRQ_SPI0,
		.end   = IRQ_SPI0,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s3c64xx_spi_cntrlr_info s5pc1xx_spi0_pdata = {
	.cfg_gpio = s5pc1xx_spi_cfg_gpio,
	.fifo_lvl_mask = 0x7f,
	.rx_lvl_offset = 13,
};

static u64 spi_dmamask = DMA_BIT_MASK(32);

struct platform_device s3c_device_spi0 = {
	.name		  = "s3c64xx-spi",
	.id		  = 0,
	.num_resources	  = ARRAY_SIZE(s5pc1xx_spi0_resource),
	.resource	  = s5pc1xx_spi0_resource,
	.dev = {
		.dma_mask		= &spi_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data = &s5pc1xx_spi0_pdata,
	},
};
EXPORT_SYMBOL(s3c_device_spi0);

static struct resource s5pc1xx_spi1_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_SPI1,
		.end   = S5PC1XX_PA_SPI1 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = DMACH_SPI1_OUT,
		.end   = DMACH_SPI1_OUT,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.start = DMACH_SPI1_IN,
		.end   = DMACH_SPI1_IN,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.start = IRQ_SPI1,
		.end   = IRQ_SPI1,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s3c64xx_spi_cntrlr_info s5pc1xx_spi1_pdata = {
	.cfg_gpio = s5pc1xx_spi_cfg_gpio,
	.fifo_lvl_mask = 0x7f,
	.rx_lvl_offset = 13,
};

struct platform_device s5pc1xx_device_spi1 = {
	.name		  = "s3c64xx-spi",
	.id		  = 1,
	.num_resources	  = ARRAY_SIZE(s5pc1xx_spi1_resource),
	.resource	  = s5pc1xx_spi1_resource,
	.dev = {
		.dma_mask		= &spi_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data = &s5pc1xx_spi1_pdata,
	},
};
EXPORT_SYMBOL(s5pc1xx_device_spi1);

static struct resource s5pc1xx_spi2_resource[] = {
	[0] = {
		.start = S5PC1XX_PA_SPI2,
		.end   = S5PC1XX_PA_SPI2 + 0x100 - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = DMACH_SPI2_TX,
		.end   = DMACH_SPI2_TX,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.start = DMACH_SPI2_RX,
		.end   = DMACH_SPI2_RX,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.start = IRQ_SPI2,
		.end   = IRQ_SPI2,
		.flags = IORESOURCE_IRQ,
	},
};

static struct s3c64xx_spi_cntrlr_info s5pc1xx_spi2_pdata = {
	.cfg_gpio = s5pc1xx_spi_cfg_gpio,
	.fifo_lvl_mask = 0x7f,
	.rx_lvl_offset = 13,
};

struct platform_device s5pc1xx_device_spi2 = {
	.name		  = "s3c64xx-spi",
	.id		  = 2,
	.num_resources	  = ARRAY_SIZE(s5pc1xx_spi2_resource),
	.resource	  = s5pc1xx_spi2_resource,
	.dev = {
		.dma_mask		= &spi_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
		.platform_data = &s5pc1xx_spi2_pdata,
	},
};
EXPORT_SYMBOL(s5pc1xx_device_spi2);

void __init s3c64xx_spi_set_info(int cntrlr, int src_clk_nr, int num_cs)
{
	/* Reject invalid configuration */
	if (!num_cs || src_clk_nr < 0
			|| src_clk_nr > S5PC1XX_SPI_SRCCLK_SCLK_SPI) {
		printk(KERN_ERR "%s: Invalid SPI configuration\n", __func__);
		return;
	}

	switch (cntrlr) {
	case 0:
		s5pc1xx_spi0_pdata.num_cs = num_cs;
		s5pc1xx_spi0_pdata.src_clk_nr = src_clk_nr;
		s5pc1xx_spi0_pdata.src_clk_name = spi_src_clks[src_clk_nr];
		break;
	case 1:
		s5pc1xx_spi1_pdata.num_cs = num_cs;
		s5pc1xx_spi1_pdata.src_clk_nr = src_clk_nr;
		s5pc1xx_spi1_pdata.src_clk_name = spi_src_clks[src_clk_nr];
		break;
	case 2:
		s5pc1xx_spi2_pdata.num_cs = num_cs;
		s5pc1xx_spi2_pdata.src_clk_nr = src_clk_nr;
		s5pc1xx_spi2_pdata.src_clk_name = spi_src_clks[src_clk_nr];
		break;
	default:
		printk(KERN_ERR "%s: Invalid SPI controller(%d)\n",
							__func__, cntrlr);
		return;
	}
}
