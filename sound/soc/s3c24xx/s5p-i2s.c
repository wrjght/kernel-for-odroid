/* sound/soc/s3c24xx/s5p-i2s.c
 *
 * ALSA SoC Audio Layer - S5P I2S driver
 *
 * Copyright 2008 Openmoko, Inc.
 * Copyright 2008 Simtec Electronics
 *      Ben Dooks <ben@simtec.co.uk>
 *      http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/io.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <plat/audio.h>
#include <plat/regs-iis1.h>
#include <plat/regs-spdif.h>
#include <plat/regs-clock.h>

#include <mach/map.h>
#include <mach/dma.h>

#include "s3c-dma.h"
#include "s5p-i2s.h"

#if 0
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif

bool g_spdif_out=false;
bool g_probe_done=false;

static struct s3c2410_dma_client s5p_dma_client_out = {
	.name		= "I2S PCM Stereo out"
};

static struct s3c2410_dma_client s5p_dma_client_in = {
	.name		= "I2S PCM Stereo in"
};

static struct s3c_dma_params s5p_i2s_pcm_stereo_out = {
	.channel	= DMACH_I2S1_OUT,
	.client		= &s5p_dma_client_out,
	.dma_addr	= S5PC1XX_PA_IIS1 + S5PC1XX_IISFIFO,
	.dma_size	= 4,
};

static struct s3c_dma_params s5p_i2s_pcm_stereo_in = {
	.channel	= DMACH_I2S1_IN,
	.client		= &s5p_dma_client_in,
	.dma_addr	= S5PC1XX_PA_IIS1 + S5PC1XX_IISFIFORX,
	.dma_size	= 4,
};

static struct s3c_i2sv2_info s5p_i2s;

static inline struct s3c_i2sv2_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return cpu_dai->private_data;
}

struct clk *s5p_i2s_get_clock(struct snd_soc_dai *dai)
{
	struct s3c_i2sv2_info *i2s = to_info(dai);
	u32 iismod = readl(i2s->regs + S5PC1XX_IISMOD);

	if (iismod & S5PC1XX_IISMOD_MPLL)
		return i2s->iis_cclk;
	else
		return i2s->iis_pclk;
}
EXPORT_SYMBOL_GPL(s5p_i2s_get_clock);

int odroid_spdif_change(struct snd_pcm_hw_params *params)
{
	unsigned int pll_out = 0; /*bclk = 0; */
	unsigned int iismod, prescaler=4, div;
	u32 clk_gate_D1_5,clk_gate_D2_0, clk_gate_sclk1, clk_src3, clk_div4;
	void __iomem *regs = s5p_i2s.regs;
	
	s3cdbg("Entered %s, rate = %d\n", __FUNCTION__, params_rate(params));

	/*Audio CLK gating start*/
	// Audio CLK SRC -> EPLL Out SEL
	writel(readl(S5P_CLK_OUT)|(0x2<<12),S5P_CLK_OUT);
	s3cdbg("CLK OUT : %x\n",readl(S5P_CLK_OUT));

	clk_src3 = readl(S5P_CLK_SRC3);
	clk_src3 &= ~S5P_CLKSRC3_AUDIO1_MASK;
	clk_src3 |= (0x0 << S5P_CLKSRC3_AUDIO1_SHIFT);
	writel(clk_src3, S5P_CLK_SRC3); // MUX audio1
	s3cdbg("S5P_CLK_SRC3 : 0x%08x\n",readl(S5P_CLK_SRC3));

	clk_div4 = readl(S5P_CLK_DIV4);
	clk_div4 &= ~S5P_CLKDIV4_AUDIO1_MASK;
	clk_div4 &= ~(0x3 << S5P_CLKDIV4_AUDIO1_SHIFT);
	writel(clk_div4, S5P_CLK_DIV4); // MUX audio1
	s3cdbg("S5P_CLK_DIV4 : 0x%08x\n",readl(S5P_CLK_DIV4));

	// PCLK gating for IIS1 
	clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS0; // IIS0 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_IIS2; // IIS2 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_AC97; // AC97 Mask
	clk_gate_D1_5 &= ~S5P_CLKGATE_D15_PCM0; // PCM0 Mask
	clk_gate_D1_5 |= S5P_CLKGATE_D15_IIS1; // IIS1 gate Enable
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
	clk_gate_sclk1 &= ~S5P_CLKGATE_SCLK1_AUDIO2; // IIS2 SCLK Mask
	clk_gate_sclk1 |= S5P_CLKGATE_SCLK1_AUDIO1; // IIS1 SCLK Gating enable
	writel(clk_gate_sclk1,S5P_SCLKGATE1);
	s3cdbg("S5P_SCLKGATE1 : %x\n",readl(S5P_SCLKGATE1));

	if(g_spdif_out)
	{
		// SPDIF clk src Audio1
		clk_src3 = readl(S5P_CLK_SRC3);
		clk_src3 &= ~S5P_CLKSRC3_SPDIF_MASK;
		clk_src3 |= (0x1 << S5P_CLKSRC3_SPDIF_SHIFT);
		writel(clk_src3, S5P_CLK_SRC3); // MUX audio1
		s3cdbg("S5P_CLK_SRC3 : 0x%08x\n",readl(S5P_CLK_SRC3));
		
		// SPDIF sclk gate
		clk_gate_sclk1 = readl(S5P_SCLKGATE1);
		clk_gate_sclk1 |= S5P_CLKGATE_SCLK1_SPDIF;
		writel(clk_gate_sclk1,S5P_SCLKGATE1);
		s3cdbg("S5P_SCLKGATE1 : 0x%08x\n",readl(S5P_SCLKGATE1));
		
		// PCLK gating for spdif 
		clk_gate_D1_5 = readl(S5P_CLKGATE_D15);
		clk_gate_D1_5 |= S5P_CLKGATE_D15_SPDIF; // spdif gate Enable
		writel(clk_gate_D1_5,S5P_CLKGATE_D15);
		s3cdbg("GATE D1_5 : 0x%08x\n",readl(S5P_CLKGATE_D15));
	}
	else {
		/*Audio CLK gating end*/
		iismod = readl(regs + S5PC1XX_IISMOD);
		iismod &= ~S5PC1XX_IISMOD_RFSMASK;
		iismod &= ~S5PC1XX_IISMOD_BFSMASK;

		/*Clear I2S prescaler value [13:8] and disable prescaler*/
		/* set prescaler division for sample rate */
		//writel(0, regs + S5PC1XX_IISPSR);
		
		switch (params_rate(params)) {
		case 16000:
		case 32000:
		case 64100:
			/* M=99, P=3, S=3 -- Fout=49.152*/
			writel((1<<31)|(99<<16)|(3<<8)|(3<<0) ,S5P_EPLL_CON);
			break;
		case 8000:
		case 11025:
		case 22050:
		case 44100:
		case 88200:
			/* M=135, P=3, S=3 -- Fout=67.738 */
			writel((1<<31)|(135<<16)|(3<<8)|(3<<0) ,S5P_EPLL_CON);
			break;
		case 48000:
		case 96000:
			/* M=147, P=3, S=3 -- Fin=12, Fout=73.728; */
			writel((1<<31)|(147<<16)|(3<<8)|(3<<0) ,S5P_EPLL_CON);
			break;
		default:
			writel((1<<31)|(128<<16)|(25<<8)|(0<<0) ,S5P_EPLL_CON);
			break;
		}
	
		switch (params_rate(params)) {
		case 8000:
			pll_out = 2048000;
			prescaler = 11;
			break;
		case 11025:
			pll_out = 2822400;
			prescaler = 8; 
			break;
		case 16000:
			pll_out = 4096000;
			prescaler = 4; 
			break;
		case 22050:
			pll_out = 5644800;
			prescaler = 4; 
			break;
		case 32000:
			pll_out = 8192000;
			prescaler = 2; 
			break;
		case 44100:
			pll_out = 11289600;
			prescaler = 2;
			break;
		case 48000:
			pll_out = 12288000;
			prescaler = 2; 
			break;
		case 88200:
			pll_out = 22579200;
			prescaler = 1; 
			break;
		case 96000:
			pll_out = 24576000;
			prescaler = 1;
			break;
		default:
			/* somtimes 32000 rate comes to 96000 
			   default values are same as 32000 */
			iismod |= S5PC1XX_IISMOD_384FS;
			prescaler = 4;
			pll_out = 12288000;
			break;
		}
	
		/* set MCLK division for sample rate */
		switch (params_format(params)) {
		case SNDRV_PCM_FORMAT_S8:
		case SNDRV_PCM_FORMAT_S16_LE:
			iismod |= S5PC1XX_IISMOD_256FS | S5PC1XX_IISMOD_32FS;
			prescaler *= 3;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			iismod |= S5PC1XX_IISMOD_384FS | S5PC1XX_IISMOD_48FS;
			prescaler *= 2;
			break;
		default:
			return -EINVAL;
		}
		prescaler = prescaler - 1; 
		writel(iismod , regs + S5PC1XX_IISMOD);

		s5p_i2s.master = 1;
		iismod = readl(regs + S5PC1XX_IISMOD);
		iismod &= ~S5PC1XX_IISMOD_SDFMASK;

		iismod &= ~S5PC1XX_IISMOD_MSB;
		iismod &= ~S5PC1XX_IISMOD_LRP;
		writel(iismod, regs + S5PC1XX_IISMOD);

		div = (prescaler << 0x8); 
		div |= S5PC1XX_IISPSR_PSRAEN;
		writel(div, regs + S5PC1XX_IISPSR);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(odroid_spdif_change);


#define S5P_I2S_RATES \
	(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 | SNDRV_PCM_RATE_16000 | \
	SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | \
	SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | \
	SNDRV_PCM_RATE_KNOT)

#define S5P_I2S_FMTS \
	(SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S16_LE |\
	 SNDRV_PCM_FMTBIT_S24_LE)

struct snd_soc_dai s5p_i2s_dai = {
	.name		= "s5p-i2s",
	.id		= 1,
	.playback = {
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		= S5P_I2S_RATES,
		.formats	= S5P_I2S_FMTS,
	},
	.capture = {
		 .channels_min	= 2,
		 .channels_max	= 2,
		 .rates		= S5P_I2S_RATES,
		 .formats	= S5P_I2S_FMTS,
	},
	.ops = {
//		.set_sysclk	= s5p_i2s_set_sysclk,	
	},
};
EXPORT_SYMBOL_GPL(s5p_i2s_dai);

static __devinit int s5p_iis_dev_probe(struct platform_device *pdev)
{
	struct s3c_i2sv2_info *i2s;
	struct snd_soc_dai *dai;
	struct s3c_audio_pdata *i2s_pdata;
	int ret;

	i2s = &s5p_i2s;
	dai = &s5p_i2s_dai;
	dai->dev = &pdev->dev;

	i2s->dma_capture = &s5p_i2s_pcm_stereo_in;
	i2s->dma_playback = &s5p_i2s_pcm_stereo_out;

	i2s_pdata = pdev->dev.platform_data;
	if (i2s_pdata && i2s_pdata->cfg_gpio && i2s_pdata->cfg_gpio(pdev)) {
		dev_err(&pdev->dev, "Unable to configure gpio\n");
		return -EINVAL;
	}

	i2s->iis_cclk = clk_get(&pdev->dev, "audio-bus");
	if (IS_ERR(i2s->iis_cclk)) {
		dev_err(&pdev->dev, "failed to get audio-bus\n");
		ret = PTR_ERR(i2s->iis_cclk);
		goto err;
	}
	clk_enable(i2s->iis_cclk);

	ret = s5pc1xx_12s1_probe(pdev, dai, i2s, 0);
	if (ret)
		goto err_clk;

	ret = s5pc1xx_i2s1_register_dai(dai);
	if (ret != 0)
		goto err_i2sv2;

	g_probe_done = true;
	return 0;

err_i2sv2:
	/* Not implemented for I2Sv2 core yet */
err_clk:
	clk_put(i2s->iis_cclk);
err:
	return ret;
}

static __devexit int s5p_iis_dev_remove(struct platform_device *pdev)
{
	dev_err(&pdev->dev, "Device removal not yet supported\n");
	return 0;
}

static struct platform_driver s5p_iis_driver = {
	.probe  = s5p_iis_dev_probe,
	.remove = s5p_iis_dev_remove,
	.driver = {
		.name = "s5p-iis",
		.owner = THIS_MODULE,
	},
};

static int __init s5p_i2s_init(void)
{
	return platform_driver_register(&s5p_iis_driver);
}
module_init(s5p_i2s_init);

static void __exit s5p_i2s_exit(void)
{
	platform_driver_unregister(&s5p_iis_driver);
}
module_exit(s5p_i2s_exit);

/* Module information */
MODULE_AUTHOR("Ben Dooks, <ben@simtec.co.uk>");
MODULE_DESCRIPTION("S5P I2S SoC Interface");
MODULE_LICENSE("GPL");
