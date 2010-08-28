/* linux/arch/arm/plat-s5pc1xx/clock.c
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * S5PC1XX Base clock support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>

#include <mach/hardware.h>
#include <mach/map.h>

#include <plat/regs-clock.h>
#include <plat/cpu.h>
#include <plat/devs.h>
#include <plat/clock.h>

static int powerdomain_set(struct powerdomain *pd, int enable)
{
	unsigned long ctrlbit = pd->pd_ctrlbit;
	void __iomem *reg = (void __iomem *)(pd->pd_reg);
	void __iomem *stable_reg = (void __iomem *)(pd->pd_stable_reg);
	unsigned long reg_dat;
		
	if (IS_ERR(pd) || pd == NULL)
		return -EINVAL;
	
	reg_dat = __raw_readl(reg);

	if (enable) {
		__raw_writel(reg_dat|ctrlbit, reg);
		do {
			
		} while(!(__raw_readl(stable_reg)&ctrlbit));
		
	} else {
		__raw_writel(reg_dat &~(ctrlbit), reg);
	}

	return 0;
}
unsigned int S5PC1XX_FREQ_TAB = 1;

static struct powerdomain pd_lcd = {
	.pd_reg		= S5P_NORMAL_CFG,
	.pd_stable_reg	= S5P_BLK_PWR_STAT,
	.pd_ctrlbit	= (0x1<<3),
	.ref_count	= 0,
	.pd_set		= powerdomain_set,
};

static struct powerdomain pd_tv = {
	.pd_reg		= S5P_NORMAL_CFG,
	.pd_stable_reg	= S5P_BLK_PWR_STAT,
	.pd_ctrlbit	= (0x1<<4),
	.ref_count	= 0,
	.pd_set		= powerdomain_set,
};

static struct powerdomain pd_mfc = {
	.pd_reg		= S5P_NORMAL_CFG,
	.pd_stable_reg	= S5P_BLK_PWR_STAT,
	.pd_ctrlbit	= (0x1<<1),
	.ref_count	= 0,
	.pd_set		= powerdomain_set,
};

static struct powerdomain pd_g3d = {
	.pd_reg		= S5P_NORMAL_CFG,
	.pd_stable_reg	= S5P_BLK_PWR_STAT,
	.pd_ctrlbit	= (0x1<<2),
	.ref_count	= 0,
	.pd_set		= powerdomain_set,
};

/* No way to set .pd in s5pc100-clock.c */
struct powerdomain pd_audio = {
	.pd_reg		= S5P_NORMAL_CFG,
	.pd_stable_reg	= S5P_BLK_PWR_STAT,
	.pd_ctrlbit	= (0x1<<5),
	.ref_count	= 0,
	.pd_set		= powerdomain_set,
};

struct clk clk_27m = {
	.name		= "clk_27m",
	.id		= -1,
	.rate		= 27000000,
};

static int clk_48m_ctrl(struct clk *clk, int enable)
{
	unsigned long flags;
	u32 val;

	local_irq_save(flags);

	val = __raw_readl(S5P_CLK_SRC1);
	if (enable)
		val |= S5P_CLKSRC1_CLK48M_MASK;
	else
		val &= ~S5P_CLKSRC1_CLK48M_MASK;

	__raw_writel(val, S5P_CLK_SRC1);
	local_irq_restore(flags);

	return 0;
}

struct clk clk_48m = {
	.name		= "clk_48m",
	.id		= -1,
	.rate		= 48000000,
	.enable		= clk_48m_ctrl,
};

struct clk clk_54m = {
	.name		= "clk_54m",
	.id		= -1,
	.rate		= 54000000,
};

static int inline s5pc1xx_clk_gate(void __iomem *reg,
				struct clk *clk,
				int enable)
{
	unsigned int ctrlbit = clk->ctrlbit;
	u32 con;

	con = __raw_readl(reg);

	if (enable)
		con |= ctrlbit;
	else
		con &= ~ctrlbit;

	__raw_writel(con, reg);
	return 0;
}

static int s5pc1xx_setrate_sclk_cam(struct clk *clk, unsigned long rate)
{
	u32 shift = 24;
	u32 cam_div, cfg;
	unsigned long src_clk = clk_get_rate(clk->parent);

	cam_div = src_clk / rate;

	if (cam_div > 32)
		cam_div = 32;

	cfg = __raw_readl(S5P_CLK_DIV1);
	cfg &= ~(0x1f << shift);
	cfg |= ((cam_div - 1) << shift);
	__raw_writel(cfg, S5P_CLK_DIV1);

	printk("parent clock for camera: %ld.%03ld MHz, divisor: %d\n", \
		print_mhz(src_clk), cam_div);

	return 0;
}

static int s5pc1xx_clk_d00_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D00_RESERVED|S5P_CLKGATE_D00_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D00, clk, enable);
}

static int s5pc1xx_clk_d01_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D01_RESERVED|S5P_CLKGATE_D01_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D01, clk, enable);
}

static int s5pc1xx_clk_d02_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D02_RESERVED|S5P_CLKGATE_D02_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D02, clk, enable);
}

static int s5pc1xx_clk_d10_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D10_RESERVED|S5P_CLKGATE_D10_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D10, clk, enable);
}

static int s5pc1xx_clk_d11_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D11_RESERVED|S5P_CLKGATE_D11_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D11, clk, enable);
}

static int s5pc1xx_clk_d12_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D12_RESERVED|S5P_CLKGATE_D12_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D12, clk, enable);
}

static int s5pc1xx_clk_d13_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D13_RESERVED|S5P_CLKGATE_D13_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D13, clk, enable);
}

static int s5pc1xx_clk_d14_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D14_RESERVED|S5P_CLKGATE_D14_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D14, clk, enable);
}

static int s5pc1xx_clk_d15_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D15_RESERVED|S5P_CLKGATE_D15_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D15, clk, enable);
}

int s5pc1xx_clk_d20_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_D20_RESERVED|S5P_CLKGATE_D20_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_CLKGATE_D20, clk, enable);
}

int s5pc1xx_sclk0_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_SCLK0_RESERVED|S5P_CLKGATE_SCLK0_ALWAYS_ON))
		return 0;

	return s5pc1xx_clk_gate(S5P_SCLKGATE0, clk, enable);
}

int s5pc1xx_sclk1_ctrl(struct clk *clk, int enable)
{
	if ((clk->ctrlbit)&(S5P_CLKGATE_SCLK1_RESERVED|S5P_CLKGATE_SCLK1_ALWAYS_ON))
		return 0;
	return s5pc1xx_clk_gate(S5P_SCLKGATE1, clk, enable);
}

static struct clk init_clocks_disable[] = {
	/* System1 (D0_0) devices */
	{
		.name		= "secss",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_SECSS,
	}, {
		.name		= "cfcon",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_CFCON,
	}, 

	/* Memory (D0_1) devices */
	{
		.name		= "sromc",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d01_ctrl,
		.ctrlbit	= S5P_CLKGATE_D01_SROMC,
	}, {
		.name		= "onenand",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d01_ctrl,
		.ctrlbit	= S5P_CLKGATE_D01_ONENAND,
	}, {
		.name		= "nand",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d01_ctrl,
		.ctrlbit	= S5P_CLKGATE_D01_NFCON,
	}, {
		.name		= "ebi",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d01_ctrl,
		.ctrlbit	= S5P_CLKGATE_D01_EBI,
	}, 

	/* System2 (D0_2) devices */
	{
		.name		= "seckey",
		.id		= -1,
		.parent		= &clk_pd0,
		.enable		= s5pc1xx_clk_d02_ctrl,
		.ctrlbit	= S5P_CLKGATE_D02_SECKEY,
	}, {
		.name		= "sdm",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d02_ctrl,
		.ctrlbit	= S5P_CLKGATE_D02_SDM,
	},

	/* File (D1_0) devices */
	{
		.name		= "usb-host",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_USBHOST,
	}, {
		.name		= "modem",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_MODEMIF,
	},

	/* Multimedia1 (D1_1) devices */
	{
		.name		= "fimc",
		.id		= 2,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_FIMC2,
		.pd		= &pd_lcd,
	},

	/* Multimedia2 (D1_2) devices */
	{
		.name		= "mipi-dsim",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_DSI,
		.pd		= &pd_lcd,
	}, {
		.name		= "mipi-csis",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_CSI,
		.pd		= &pd_lcd,
	},
	/* System (D1_3) devices */
	{
		.name		= "apc",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_APC,
	}, {
		.name		= "iec",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_IEC,
	},

	/* Connectivity (D1_4) devices */
	{
		.name		= "irda",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_IRDA,
	}, {
		.name		= "hsitx",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_HSITX,
	}, {
		.name		= "hsirx",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_HSIRX,		
	}, {
		.name		= "ccan",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_CCAN0,
	}, {
		.name		= "ccan",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_CCAN1,
	},

	/* Audio (D1_5) devices */
	{
		.name		= "iis",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_IIS0, /* I2S0 is v5.0 */
	}, {
		.name		= "iis",
		.id		= 2,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_IIS2, /* I2S2 is v3.2 */
	}, {
		.name		= "ac97",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_AC97,
	}, {
		.name		= "pcm",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_PCM0,
	}, {
		.name		= "pcm",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_PCM1,
	}, {
		.name		= "spdif",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_SPDIF,
	}, {
		.name		= "adc",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_TSADC,
	}, {
		.name		= "keyif",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_KEYIF,
	}, {
		.name		= "keypad",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_KEYIF,		
	},

	/* Audio (D2_0) devices: all disabled */
	{
		.name		= "hclkd2",
		.id		= -1,
		.parent		= NULL,
		.enable		= s5pc1xx_clk_d20_ctrl,
		.ctrlbit	= S5P_CLKGATE_D20_HCLKD2,
		.pd		= &pd_audio,
	}, {
		.name		= "audio-bus",
		.id		= 0,
		.parent		= NULL,
		.enable		= s5pc1xx_clk_d20_ctrl,
		.ctrlbit	= S5P_CLKGATE_D20_I2SD2,
		.pd		= &pd_audio,
	}, 
	
	/* Special Clocks 1 */
	{
		.name		= "sclk_hpm",
		.id		= -1,
		.parent		= NULL,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_HPM,
	}, {
		.name		= "sclk_pwi",
		.id			= -1,
		.parent		= NULL,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_PWI,
	}, {
		.name		= "sclk_onenand",
		.id		= -1,
		.parent		= NULL,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_ONENAND,
	}, {
		.name		= "sclk_spi_48",
		.id		= 0,
		.parent		= &clk_48m,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_SPI0_48,
	}, {
		.name		= "sclk_spi_48",
		.id		= 1,
		.parent		= &clk_48m,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_SPI1_48,
	}, {
		.name		= "sclk_spi_48",
		.id		= 2,
		.parent		= &clk_48m,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_SPI2_48,
	}, {
		.name		= "sclk_usbhost",
		.id			= -1,
		.parent		= NULL,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_USBHOST,		
	}
	/* Special Clocks 2 */
	
};

static struct clk init_clocks[] = {
	/* System1 (D0_0) devices */
	{
		.name		= "intc",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_INTC,
	}, {
		.name		= "tzic",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_TZIC,
	}, {
		.name		= "mdma",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_MDMA,
	}, {
		.name		= "g2d",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_G2D,
		.pd		= &pd_lcd,
	}, {
		.name		= "cssys",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d00_ctrl,
		.ctrlbit	= S5P_CLKGATE_D00_CSSYS,
	},

	/* Memory (D0_1) devices */
	{
		.name		= "dmc",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d01_ctrl,
		.ctrlbit	= S5P_CLKGATE_D01_DMC,
	}, {
		.name		= "intmem",
		.id		= -1,
		.parent		= &clk_hd0,
		.enable		= s5pc1xx_clk_d01_ctrl,
		.ctrlbit	= S5P_CLKGATE_D01_INTMEM,
	},


	/* File (D1_0) devices */
	{
		.name		= "pdma0",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_PDMA0,
	}, {
		.name		= "pdma1",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_PDMA1,
	}, {
		.name		= "hsmmc",
		.id		= 0,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_HSMMC0,
	}, {
		.name		= "hsmmc",
		.id		= 1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_HSMMC1,
	}, {
		.name		= "hsmmc",
		.id		= 2,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_HSMMC2,
	},{
		.name		= "otg",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d10_ctrl,
		.ctrlbit	= S5P_CLKGATE_D10_USBOTG,
	},

	/* Multimedia1 (D1_1) devices */
	{
		.name		= "fimd",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_LCD,
		.pd		= &pd_lcd,
	}, {
		.name		= "rotator",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_ROTATOR,
		.pd		= &pd_lcd,
	}, {
		.name		= "fimc",
		.id		= 0,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_FIMC0,
		.pd		= &pd_lcd,
	}, {
		.name		= "fimc",
		.id		= 1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_FIMC1,
		.pd		= &pd_lcd,
	}, {
		.name		= "jpeg",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_JPEG,
		.pd		= &pd_lcd,
	}, {
		.name		= "g3d",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_G3D,
		.pd		= &pd_g3d,
	}, {
		.name		= "rot",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d11_ctrl,
		.ctrlbit	= S5P_CLKGATE_D11_ROTATOR,
		.pd		= &pd_lcd,
	},

	/* Multimedia2 (D1_2) devices */
	{
		.name		= "tv",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d12_ctrl,
		.ctrlbit	= S5P_CLKGATE_D12_TV,
		.pd		= &pd_tv,
	}, {
		.name		= "vp",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d12_ctrl,
		.ctrlbit	= S5P_CLKGATE_D12_VP,
		.pd		= &pd_tv,
	}, {
		.name		= "mixer",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d12_ctrl,
		.ctrlbit	= S5P_CLKGATE_D12_MIXER,
		.pd		= &pd_tv,
	}, {
		.name		= "hdmi",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d12_ctrl,
		.ctrlbit	= S5P_CLKGATE_D12_HDMI,
		.pd		= &pd_tv,
	}, {
		.name		= "mfc",
		.id		= -1,
		.parent		= &clk_h,
		.enable		= s5pc1xx_clk_d12_ctrl,
		.ctrlbit	= S5P_CLKGATE_D12_MFC,
		.pd		= &pd_mfc,
	},

	/* System (D1_3) devices */
	{
		.name		= "chipid",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_CHIPID,
	}, {
		.name		= "gpio",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_GPIO,
	}, {
		.name		= "timers",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_PWM,
	}, {
		.name		= "systimer",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_SYSTIMER,
	}, {
		.name		= "watchdog",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_WDT,
	}, {
		.name		= "rtc",
		.id		= -1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d13_ctrl,
		.ctrlbit	= S5P_CLKGATE_D13_RTC,
	},

	/* Connectivity (D1_4) devices */
	{
		.name		= "uart",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_UART0,
	}, {
		.name		= "uart",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_UART1,
	}, {
		.name		= "uart",
		.id		= 2,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_UART2,
	}, {
		.name		= "uart",
		.id		= 3,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_UART3,
	}, {
		.name		= "i2c",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_IIC,
	}, {
		.name		= "i2c",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_HDMI_IIC,
	}, {
		.name		= "spi",
		.id		= 0,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_SPI0,
	}, {
		.name		= "spi",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_SPI1,
	}, {
		.name		= "spi",
		.id		= 2,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d14_ctrl,
		.ctrlbit	= S5P_CLKGATE_D14_SPI2,
	},

	/* Audio (D1_5) devices */
	{
		.name		= "iis",
		.id		= 1,
		.parent		= &clk_p,
		.enable		= s5pc1xx_clk_d15_ctrl,
		.ctrlbit	= S5P_CLKGATE_D15_IIS1, /* I2S1 is v3.2 */
	}, 

	/* Audio (D2_0) devices: all disabled */

	/* Special Clocks 1 */

	{
		.name		= "sclk_mmc_48",
		.id		= 0,
		.parent		= &clk_48m,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_MMC0_48,
	}, {
		.name		= "sclk_mmc_48",
		.id		= 1,
		.parent		= &clk_48m,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_MMC1_48,
	}, {
		.name		= "sclk_mmc_48",
		.id		= 2,
		.parent		= &clk_48m,
		.enable		= s5pc1xx_sclk0_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK0_MMC2_48,
	},

	/* Special Clocks 2 */
	{
		.name		= "sclk_tv_54",
		.id		= -1,
		.parent		= &clk_54m,
		.enable		= s5pc1xx_sclk1_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK1_TV54,
	}, {
		.name		= "sclk_vdac_54",
		.id		= -1,
		.parent		= &clk_54m,
		.enable		= s5pc1xx_sclk1_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK1_VDAC54,
	}, {
		.name		= "sclk_spdif",
		.id		= -1,
		.parent		= NULL,
		.enable		= s5pc1xx_sclk1_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK1_SPDIF,
	}, {
		.name		= "sclk_cam",
		.id		= -1,
		.parent		= &clk_dout_mpll2,
		.enable		= s5pc1xx_sclk1_ctrl,
		.ctrlbit	= S5P_CLKGATE_SCLK1_CAM,
		.set_rate	= s5pc1xx_setrate_sclk_cam,
	},
};

static struct clk *clks[] __initdata = {
	&clk_ext,
	&clk_epll,
	&clk_27m,
	&clk_48m,
	&clk_54m,
};

/* Disable all IP's clock and MM power domain. This will decrease power 
 * consumption in normal mode.
 * In kernel booting sequence, basically disable all IP's clock and MM power domain. 
 * If D/D uses specific clock, use clock API.
 */
void s5pc1xx_init_clocks_power_disabled(void)
{
	struct clk clk_dummy;
	unsigned long shift = 0;

	/* Disable all clock except essential clock */
	do {
		clk_dummy.ctrlbit = (1<<shift);
		s5pc1xx_clk_d00_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d01_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d02_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d10_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d11_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d12_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d13_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d14_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d15_ctrl(&clk_dummy, 0);
		s5pc1xx_clk_d20_ctrl(&clk_dummy, 0);

		s5pc1xx_sclk0_ctrl(&clk_dummy, 0);
		s5pc1xx_sclk1_ctrl(&clk_dummy, 0);

		shift ++;
		
	} while (shift < 32);

	/* Disable all power domain */
	powerdomain_set(&pd_lcd, 1);
	powerdomain_set(&pd_tv, 0);
	powerdomain_set(&pd_mfc, 1);
	powerdomain_set(&pd_g3d, 0);
	powerdomain_set(&pd_audio, 1);

}


void __init s5pc1xx_register_clocks(void)
{
	struct clk *clkp;
	int ret;
	int ptr;

	s5pc1xx_init_clocks_power_disabled();

	s3c24xx_register_clocks(clks, ARRAY_SIZE(clks));

	clkp = init_clocks;
	for (ptr = 0; ptr < ARRAY_SIZE(init_clocks); ptr++, clkp++) {
		ret = s3c24xx_register_clock(clkp);
		if (ret < 0) {
			printk(KERN_ERR "Failed to register clock %s (%d)\n",
			       clkp->name, ret);
		}
	}

	clkp = init_clocks_disable;
	for (ptr = 0; ptr < ARRAY_SIZE(init_clocks_disable); ptr++, clkp++) {
		ret = s3c24xx_register_clock(clkp);
		if (ret < 0) {
			printk(KERN_ERR "Failed to register clock %s (%d)\n",
			       clkp->name, ret);
		}
		(clkp->enable)(clkp, 0);
	}
}
