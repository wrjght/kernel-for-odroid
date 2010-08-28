/*
 * s3c-i2s.c  --  ALSA Soc Audio Layer
 *
 * (c) 2006 Wolfson Microelectronics PLC.
 * Graeme Gregory graeme.gregory@wolfsonmicro.com or linux@wolfsonmicro.com
 *
 * (c) 2004-2005 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *	Ryu Euiyoul <ryu.real@gmail.com>
 *
 * Copyright (C) 2008, SungJun Bae<june.bae@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *
 *  Revision history
 *    11th Dec 2006   Merged with Simtec driver
 *    10th Nov 2006   Initial version.
 *    1st  Dec 2008   Initial version from s3c64xx-i2s.c.
 */

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/dma.h>
#include <mach/map.h>

#include <mach/gpio.h>
#include <plat/regs-iis1.h>
#include <plat/regs-clock.h>
#include <plat/gpio-cfg.h>

#include <plat/pm.h>
#include <plat/regs-power.h>

#include "s5p-i2s.h"
#include "odroid-i2s-bus1.h"
#include "odroid-s5p-spdif.h"
#include "../codecs/wm8991.h"

#if 0
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

/* used to disable sysclk if external crystal is used */
static int extclk = 0;
module_param(extclk, int, 0);
MODULE_PARM_DESC(extclk, "set to 1 to disable s3c24XX i2s sysclk");

extern irqreturn_t	odroid_hpjack_isr(int irq, void *dev_id);
extern void			odroid_detect_handle(unsigned long data);

static inline struct s3c_i2sv2_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return cpu_dai->private_data;
}

static void s5pc1xx_snd_txctrl(struct s3c_i2sv2_info *i2s, int on)
{
	void __iomem *regs = i2s->regs;
	u32 fic, con, mod;

	s3cdbg("%s(%d)\n", __func__, on);

	fic = readl(regs + S5PC1XX_IISFIC);
	con = readl(regs + S5PC1XX_IISCON);
	mod = readl(regs + S5PC1XX_IISMOD);

	pr_debug("%s: IIS: CON=%x MOD=%x FIC=%x\n", __func__, con, mod, fic);

	if (on) {
		con |= S5PC1XX_IISCON_TXDMACTIVE | S5PC1XX_IISCON_I2SACTIVE;
		con &= ~S5PC1XX_IISCON_TXDMAPAUSE;
		con &= ~S5PC1XX_IISCON_TXCHPAUSE;

		switch (mod & S5PC1XX_IISMOD_TXRMASK) {
		case S5PC1XX_IISMOD_TXRX:
		case S5PC1XX_IISMOD_TX:
			/* do nothing, we are in the right mode */
			break;

		case S5PC1XX_IISMOD_RX:
			mod &= ~S5PC1XX_IISMOD_TXRMASK;
			mod |= S5PC1XX_IISMOD_TXRX;
			break;

		default:
			dev_err(i2s->dev, "RXEN: Invalid MODE %x in IISMOD\n",
				mod & S5PC1XX_IISMOD_TXRMASK);
		}
	} else {
		/* See txctrl notes on FIFOs. */

		con &= ~S5PC1XX_IISCON_TXDMACTIVE;
		con |=  S5PC1XX_IISCON_TXDMAPAUSE;
		con |=  S5PC1XX_IISCON_TXCHPAUSE;

		switch (mod & S5PC1XX_IISMOD_TXRMASK) {
		case S5PC1XX_IISMOD_TXRX:
			mod &= ~S5PC1XX_IISMOD_TXRMASK;
			mod |= S5PC1XX_IISMOD_RX;
			break;

		case S5PC1XX_IISMOD_TX:
			con &= ~S5PC1XX_IISCON_I2SACTIVE;
			mod &= ~S5PC1XX_IISMOD_TXRMASK;
			break;

		default:
			dev_err(i2s->dev, "RXDIS: Invalid MODE %x in IISMOD\n",
				mod & S5PC1XX_IISMOD_TXRMASK);
		}
	}
	writel(mod, regs + S5PC1XX_IISMOD);
	writel(con, regs + S5PC1XX_IISCON);

	fic = readl(regs + S5PC1XX_IISFIC);
	pr_debug("%s: IIS: CON=%x MOD=%x FIC=%x\n", __func__, con, mod, fic);
}

static void s5pc1xx_snd_rxctrl(struct s3c_i2sv2_info *i2s, int on)
{
	void __iomem *regs = i2s->regs;
	u32 fic, con, mod;

	s3cdbg("%s(%d)\n", __func__, on);

	fic = readl(regs + S5PC1XX_IISFIC);
	con = readl(regs + S5PC1XX_IISCON);
	mod = readl(regs + S5PC1XX_IISMOD);

	pr_debug("%s: IIS: CON=%x MOD=%x FIC=%x\n", __func__, con, mod, fic);

	if (on) {
		con |= S5PC1XX_IISCON_RXDMACTIVE | S5PC1XX_IISCON_I2SACTIVE;
		con &= ~S5PC1XX_IISCON_RXDMAPAUSE;
		con &= ~S5PC1XX_IISCON_RXCHPAUSE;

		switch (mod & S5PC1XX_IISMOD_TXRMASK) {
		case S5PC1XX_IISMOD_TXRX:
		case S5PC1XX_IISMOD_RX:
			/* do nothing, we are in the right mode */
			break;

		case S5PC1XX_IISMOD_TX:
			mod &= ~S5PC1XX_IISMOD_TXRMASK;
			mod |= S5PC1XX_IISMOD_TXRX;
			break;

		default:
			dev_err(i2s->dev, "RXEN: Invalid MODE %x in IISMOD\n",
				mod & S5PC1XX_IISMOD_TXRMASK);
		}
	} else {
		/* See txctrl notes on FIFOs. */

		con &= ~S5PC1XX_IISCON_RXDMACTIVE;
		con |=  S5PC1XX_IISCON_RXDMAPAUSE;
		con |=  S5PC1XX_IISCON_RXCHPAUSE;

		switch (mod & S5PC1XX_IISMOD_TXRMASK) {
		case S5PC1XX_IISMOD_RX:
			con &= ~S5PC1XX_IISCON_I2SACTIVE;
			mod &= ~S5PC1XX_IISMOD_TXRMASK;
			break;

		case S5PC1XX_IISMOD_TXRX:
			mod &= ~S5PC1XX_IISMOD_TXRMASK;
			mod |= S5PC1XX_IISMOD_TX;
			break;

		default:
			dev_err(i2s->dev, "RXDIS: Invalid MODE %x in IISMOD\n",
				mod & S5PC1XX_IISMOD_TXRMASK);
		}
	}
	writel(mod, regs + S5PC1XX_IISMOD);
	writel(con, regs + S5PC1XX_IISCON);

	fic = readl(regs + S5PC1XX_IISFIC);
	pr_debug("%s: IIS: CON=%x MOD=%x FIC=%x\n", __func__, con, mod, fic);
}

/*
 * Wait for the LR signal to allow synchronisation to the L/R clock
 * from the codec. May only be needed for slave mode.
 */
static int s5pc1xx_snd_lrsync(struct s3c_i2sv2_info *i2s)
{
	u32 iiscon;
//	unsigned long timeout = jiffies + msecs_to_jiffies(5);
	int timeout = 50; /* 5ms */

	s3cdbg("Entered %s\n", __FUNCTION__);

	while (1) {
	iiscon = readl(i2s->regs + S5PC1XX_IISCON);

		if (iiscon & S5PC1XX_IISCON_LRI)
			break;

		if (timeout < jiffies) 
			return -ETIMEDOUT;
		
	}

	return 0;
}

/*
 * Set S3C24xx I2S DAI format
 */
static int s5pc1xx_12s1_set_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	struct s3c_i2sv2_info *i2s = to_info(cpu_dai);
	u32 iismod;

	if(g_spdif_out) return 0;
	else 	s5pc1xx_spdif_power_off();

	s3cdbg("Entered %s \n", __FUNCTION__);
	iismod = readl(i2s->regs + S5PC1XX_IISMOD);
	iismod &= ~S5PC1XX_IISMOD_SDFMASK;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		i2s->master = 0;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		i2s->master = 1;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iismod &= ~S5PC1XX_IISMOD_MSB;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iismod |= S5PC1XX_IISMOD_MSB;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iismod |= S5PC1XX_IISMOD_LSB;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iismod &= ~S5PC1XX_IISMOD_LRP;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iismod |= S5PC1XX_IISMOD_LRP;
		break;
	case SND_SOC_DAIFMT_IB_IF:
	case SND_SOC_DAIFMT_IB_NF:
	default:
		printk("Inv-combo(%d) not supported!\n", fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}
	writel(iismod, i2s->regs + S5PC1XX_IISMOD);
	return 0;


}

static int s5pc1xx_12s1_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai_link *dai = rtd->dai;
	struct s3c_i2sv2_info *i2s = to_info(dai->cpu_dai);

	unsigned long iismod;

	if(g_spdif_out) return s5pc1xx_spdif_hw_params(substream, params, socdai);
	else 	s5pc1xx_spdif_power_off();

	s3cdbg("Entered %s\n", __FUNCTION__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dai->cpu_dai->dma_data = i2s->dma_playback;
	} else {
		dai->cpu_dai->dma_data = i2s->dma_capture;
	}

	/* Working copies of registers */
	iismod = readl(i2s->regs + S5PC1XX_IISMOD);
	iismod &= ~S5PC1XX_IISMOD_BLCMASK;

	/* Multi channel enable */
	switch (params_channels(params)) {
	case 1:
		s3cdbg("s3c i2s: 1 channel\n");
		break;
	case 2:
		s3cdbg("s3c i2s: 2 channel\n");
		break;
	case 4:
		s3cdbg("s3c i2s: 4 channel\n");
		break;
	case 6:
		s3cdbg("s3c i2s: 6 channel\n");
		break;
	default:
		printk(KERN_ERR "s3c-i2s-v32: %d channels unsupported\n",
		       params_channels(params));
		return -EINVAL;
	}

	/* Set the bit rate */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		iismod |= S5PC1XX_IISMOD_BLC8BIT;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		iismod |= S5PC1XX_IISMOD_BLC16BIT;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iismod |= S5PC1XX_IISMOD_BLC24BIT;
		break;
	default:
		return -EINVAL;
	}

	writel(iismod, i2s->regs + S5PC1XX_IISMOD);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		s3cdbg("%s : IISMOD [0x%08x]\n",__FUNCTION__, readl(i2s->regs + S5PC1XX_IISMOD));

	s3cdbg("s3c: params_channels %d\n", params_channels(params));
	s3cdbg("s3c: params_format %d\n", params_format(params));
	s3cdbg("s3c: params_subformat %d\n", params_subformat(params));
	s3cdbg("s3c: params_period_size %d\n", params_period_size(params));
	s3cdbg("s3c: params_period_bytes %d\n", params_period_bytes(params));
	s3cdbg("s3c: params_periods %d\n", params_periods(params));
	s3cdbg("s3c: params_buffer_size %d\n", params_buffer_size(params));
	s3cdbg("s3c: params_buffer_bytes %d\n", params_buffer_bytes(params));

	return 0;
}

static int s5pc1xx_12s1_startup(struct snd_pcm_substream *substream, struct snd_soc_dai *socdai)
{
	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}



static int s5pc1xx_12s1_prepare(struct snd_pcm_substream *substream, struct snd_soc_dai *socdai)
{
	if(g_spdif_out) return s5pc1xx_spdif_prepare(substream, socdai);
	else 	s5pc1xx_spdif_power_off();

	s3cdbg("Entered %s\n", __FUNCTION__);
	return 0;
}


static int s5pc1xx_12s1_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *socdai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct s3c_i2sv2_info *i2s = to_info(rtd->dai->cpu_dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!i2s->master) {
			ret = s5pc1xx_snd_lrsync(i2s);
			if (ret)
				goto exit_err;
		}

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			s5pc1xx_snd_rxctrl(i2s, 1);
		else {
			if(g_spdif_out) s5pc1xx_spdif_txctrl(1);
			else s5pc1xx_snd_txctrl(i2s, 1);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			s5pc1xx_snd_rxctrl(i2s, 0);
		else {
			if(g_spdif_out) s5pc1xx_spdif_txctrl(0);
			else s5pc1xx_snd_txctrl(i2s, 0);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

exit_err:
	return ret;
}

/*
 * Set S3C24xx Clock source
 */
static int s5pc1xx_12s1_set_sysclk(struct snd_soc_dai *cpu_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct s3c_i2sv2_info *i2s = to_info(cpu_dai);
	u32 iismod;

	if(g_spdif_out) return 0;
	else 	s5pc1xx_spdif_power_off();

	s3cdbg("Entered %s : clk_id = %d\n", __FUNCTION__, clk_id);

	iismod = readl(i2s->regs + S5PC1XX_IISMOD);
	iismod &= ~S5PC1XX_IISMOD_MPLL;
	switch (clk_id) {
	case S5PC1XX_CLKSRC_PCLK:
		if(!i2s->master) return -EINVAL;
		break;
	case S5PC1XX_CLKSRC_MPLL:
		iismod |= S5PC1XX_IISMOD_MPLL;
		break;
	default:
		return -EINVAL;
	}
	writel(iismod, i2s->regs + S5PC1XX_IISMOD);
	return 0;
}

/*
 * Set S3C24xx Clock dividers
 */
static int s5pc1xx_12s1_set_clkdiv(struct snd_soc_dai *cpu_dai,
	int div_id, int div)
{
	struct s3c_i2sv2_info *i2s = to_info(cpu_dai);
	u32 reg;

	if(g_spdif_out) return 0;
	else 	s5pc1xx_spdif_power_off();

	s3cdbg("Entered %s : div_id = %d, div = %x\n", __FUNCTION__, div_id, div);
	switch (div_id) {
	case S5PC1XX_DIV_MCLK:
		break;
	case S5PC1XX_DIV_BCLK:
		reg = readl(i2s->regs + S5PC1XX_IISMOD) & ~(S5PC1XX_IISMOD_RFSMASK);
		writel(reg | div, i2s->regs + S5PC1XX_IISMOD);
		break;
	case S5PC1XX_DIV_PRESCALER:
		if (div) 	div |= S5PC1XX_IISPSR_PSRAEN;
		else 		div &= ~S5PC1XX_IISPSR_PSRAEN;
		writel(div, i2s->regs + S5PC1XX_IISPSR);
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

/*
 * To avoid duplicating clock code, allow machine driver to
 * get the clockrate from here.
 */
//u32 s5pc1xx_12s1_get_clockrate(void)
//{
//	if(g_spdif_out) return s5p_spdif_get_clockrate();
//	else 	s5pc1xx_spdif_power_off();
//
//	return clk_get_rate(i2s->iis_clk);
//}
//EXPORT_SYMBOL_GPL(s5pc1xx_12s1_get_clockrate);


static irqreturn_t s5pc1xx_iis1_irq(int irqno, void *dev_id)
{
//	u32 iiscon;
//	
//	iiscon  = readl(i2s->regs + S5PC1XX_IISCON);
//	if(S5PC1XX_IISCON_FTXURSTATUS & iiscon) {
//		iiscon &= ~S5PC1XX_IISCON_FTXURINTEN;
//		iiscon |= S5PC1XX_IISCON_FTXURSTATUS;
//		writel(iiscon, i2s->regs + S5PC1XX_IISCON);
//		s3cdbg("underrun interrupt IISCON = 0x%08x\n", readl(i2s->regs + S5PC1XX_IISCON));
//	}

	return IRQ_HANDLED;
}

int s5pc1xx_12s1_probe(struct platform_device *pdev,
			struct snd_soc_dai *dai,
		    struct s3c_i2sv2_info *i2s,
		    unsigned long base)
{
	int ret;
	unsigned int gpio;
	struct device *dev = &pdev->dev;

	s3cdbg("Entered %s\n", __FUNCTION__);

	// Configure the I2S pins : GPC[n] -> I2S1
    for (gpio = S5PC1XX_GPC(0); gpio <= S5PC1XX_GPC(4); gpio++)
    {
        s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
        s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
    }

	i2s->dev = dev;
	/* record our i2s structure for later use in the callbacks */
	dai->private_data = i2s;

	if (!base) {
		struct resource *res = platform_get_resource(pdev,
							     IORESOURCE_MEM,
							     0);
		if (!res) {
			dev_err(dev, "Unable to get register resource\n");
			return -ENXIO;
		}

		if (!request_mem_region(res->start, resource_size(res),
					"s3c64xx-i2s-v4")) {
			dev_err(dev, "Unable to request register region\n");
			return -EBUSY;
		}

		base = res->start;

//		i2s->dma_capture->dma_addr = (dma_addr_t)res->start + S5PC1XX_IISFIFORX;
//		i2s->dma_playback->dma_addr = (dma_addr_t)res->start + S5PC1XX_IISFIFO;
	}

	i2s->regs = ioremap(S5PC1XX_PA_IIS1, 0x100);
	if (i2s->regs == NULL) 
		return -ENXIO;

	i2s->iis_clk=clk_get(&pdev->dev, "iis");
	if (i2s->iis_clk == NULL) {
		printk("failed to get iis_clock\n");
		iounmap(i2s->regs);
		return -ENODEV;
	}
	clk_enable(i2s->iis_clk);
	
	ret = request_irq(IRQ_I2S1, s5pc1xx_iis1_irq, 0,"s5p-i2s1", NULL);
	if (ret < 0){
		printk("fail to claim i2s irq , ret = %d\n", ret);
		iounmap(i2s->regs);
		return -ENODEV;
	}
	
	s5pc1xx_spdif_probe( pdev, dai);
	
	s5pc1xx_snd_txctrl(i2s, 0);
	s5pc1xx_snd_rxctrl(i2s, 0);
	s5pc1xx_spdif_txctrl(0);

	return 0;
}

EXPORT_SYMBOL_GPL(s5pc1xx_12s1_probe);

void iis_clock_control(bool enable)
{
	u32 clk_gate_D1_5,clk_gate_D2_0, clk_gate_sclk1;
	s3cdbg("Entered %s but will not do anything\n", __FUNCTION__);
	if(!enable)
	{
		/*Audio CLK gating start*/
	
		// PCLK gating for IIS1 
		clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
		clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS0; // IIS0 Mask
		clk_gate_D1_5 |= S5P_CLKGATE_D15_IIS1; // IIS1 Mask
		clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS2; // IIS2 Mask
		clk_gate_D1_5 &= ~S5P_CLKGATE_D15_AC97; // AC97 Mask
		clk_gate_D1_5 &= ~S5P_CLKGATE_D15_PCM0; // PCM0 Mask
	
		writel(clk_gate_D1_5,S5P_CLKGATE_D15);
		s3cdbg("GATE D1_5 : %x\n",readl(S5P_CLKGATE_D15));
	
		// IIS0 Block Clk Mask
		clk_gate_D2_0 = readl(S5P_CLKGATE_D20);
		clk_gate_D2_0 &= ~S5P_CLKGATE_D20_HCLKD2; // IIS0 HCLK D2 Mask
		clk_gate_D2_0 &= ~S5P_CLKGATE_D20_I2SD2; // IIS0CLK D2 Mask
		writel(clk_gate_D2_0,S5P_CLKGATE_D20);
		s3cdbg("HCLKD2 Gate : %x\n",readl(S5P_CLKGATE_D20));
	
		// SCLK Gateing Audio1
		clk_gate_sclk1 = readl(S5P_SCLKGATE1);
		clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_AUDIO0; // IIS0 SCLK Mask
		clk_gate_sclk1 |= S5P_CLKGATE_SCLK1_AUDIO1; // IIS1 SCLK Mask
		clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_AUDIO2; // IIS2 SCLK Mask
		writel(clk_gate_sclk1,S5P_SCLKGATE1);
		s3cdbg("S5P_SCLKGATE1 : %x\n",readl(S5P_SCLKGATE1));
	}
//	s5p_power_gating(S5PC100_POWER_DOMAIN_AUDIO,DOMAIN_LP_MODE);
}
EXPORT_SYMBOL_GPL(iis_clock_control);


#ifdef CONFIG_PM
static int s5pc1xx_12s1_suspend(struct snd_soc_dai *dai)
{
	struct s3c_i2sv2_info *i2s = to_info(dai);
	u32 clk_gate_D1_5,clk_gate_D2_0, clk_gate_sclk1;
	s3cdbg("Entered %s\n", __FUNCTION__);

	del_timer_sync(&detect_timer);
	disable_irq(IRQ_HPJACK);

	/*Audio CLK gating start*/
	// Audio CLK SRC -> EPLL Out SEL
	writel(readl(S5P_CLK_OUT)&~(0x2<<12),S5P_CLK_OUT);
	s3cdbg("CLK OUT : %x\n",readl(S5P_CLK_OUT));

	// PCLK gating for IIS1 
	clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS0; // IIS0 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS1; // IIS1 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS2; // IIS2 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_AC97; // AC97 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_PCM0; // PCM0 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_PCM1; // PCM1 Mask

	writel(clk_gate_D1_5,S5P_CLKGATE_D15);
	s3cdbg("GATE D1_5 : %x\n",readl(S5P_CLKGATE_D15));

	// IIS0 Block Clk Mask
	clk_gate_D2_0 = readl(S5P_CLKGATE_D20);
	clk_gate_D2_0 &= ~S5P_CLKGATE_D20_HCLKD2; // IIS0 HCLK D2 Mask
	clk_gate_D2_0 &= ~S5P_CLKGATE_D20_I2SD2; // IIS0CLK D2 Mask
	writel(clk_gate_D2_0,S5P_CLKGATE_D20);
	s3cdbg("HCLKD2 Gate : %x\n",readl(S5P_CLKGATE_D20));

	// SCLK Gateing Audio1
	clk_gate_sclk1 = readl(S5P_SCLKGATE1);
	clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_AUDIO0; // IIS0 SCLK Mask
	clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_AUDIO1; // IIS1 SCLK Mask
	clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_AUDIO2; // IIS2 SCLK Mask

	clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_SPDIF; // spdif SCLK Mask
	
	writel(clk_gate_sclk1,S5P_SCLKGATE1);
	s3cdbg("S5P_SCLKGATE1 : %x\n",readl(S5P_SCLKGATE1));
	
	g_probe_done=false;

	clk_disable(i2s->iis_clk);
	return 0;
}

static int s5pc1xx_12s1_resume(struct snd_soc_dai *dai)
{
	struct s3c_i2sv2_info *i2s = to_info(dai);
	unsigned int gpio, err;

	s3cdbg("Entered %s\n", __FUNCTION__);

	// Configure the I2S pins : GPC[n] -> I2S1
    for (gpio = S5PC1XX_GPC(0); gpio <= S5PC1XX_GPC(4); gpio++)
    {
        s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(2));
        s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
    }

	if (gpio_is_valid(S5PC1XX_GPH0(7)))
	{
		err = gpio_request( S5PC1XX_GPH0(7), "GPH0");
		if(err)
		{
			s3cdbg(KERN_ERR "failed to request GPJ0 for wm8991 i2c clk..\n");
			return err;
		}
		gpio_direction_input(S5PC1XX_GPH0(7));
		s3c_gpio_setpull(S5PC1XX_GPH0(7), S3C_GPIO_PULL_UP);
		
		if(gpio_get_value(S5PC1XX_GPH0(7))) g_current_out=WM8991_SPK;
		else 								g_current_out=WM8991_HP;
		gpio_free(S5PC1XX_GPH0(7));
	}
	err = request_irq(IRQ_HPJACK, odroid_hpjack_isr, IRQF_DISABLED, "hkc100-hpjack-irq", NULL);
	if(err) s3cdbg("\nDEBUG -> IRQ_HPJACK request error!!!\n\n");
	set_irq_type(IRQ_HPJACK, IRQ_TYPE_EDGE_FALLING);

	init_timer(&detect_timer);
	detect_timer.function = odroid_detect_handle;

	wm8991_set_outpath(g_current_out);
	clk_enable(i2s->iis_clk);

	enable_irq(IRQ_HPJACK);
	g_probe_done=true;

	return 0;
}

#else
#define s5pc1xx_12s1_suspend	NULL
#define s5pc1xx_12s1_resume	NULL
#endif

int s5pc1xx_i2s1_register_dai(struct snd_soc_dai *dai)
{
	struct snd_soc_dai_ops *ops = &dai->ops;

	ops->startup = s5pc1xx_12s1_startup;
	ops->prepare = s5pc1xx_12s1_prepare;
	ops->trigger = s5pc1xx_12s1_trigger;
	ops->hw_params = s5pc1xx_12s1_hw_params;
	ops->set_fmt = s5pc1xx_12s1_set_fmt;
	ops->set_clkdiv = s5pc1xx_12s1_set_clkdiv;
	ops->set_sysclk = s5pc1xx_12s1_set_sysclk;

#ifdef CONFIG_PM
	dai->suspend = s5pc1xx_12s1_suspend;
	dai->resume = s5pc1xx_12s1_resume;
#endif

	return snd_soc_register_dai(dai);
}
EXPORT_SYMBOL_GPL(s5pc1xx_i2s1_register_dai);


/* Module information */
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_DESCRIPTION("s5pc1xx I2S-bus(2ch) Interface");
MODULE_LICENSE("GPL");
