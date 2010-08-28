/*
 *  odroid-s5p-wm8991.c
 *
 *  Copyright (c) 2009 Samsung Electronics Co. Ltd
 *  Author: Jaswinder Singh <jassi.brar@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/gpio.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <mach/map.h>
#include <plat/regs-clock.h>
#include <plat/regs-iis1.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-bank-c.h>
#include <plat/gpio-bank-g3.h>

#include "../codecs/wm8991.h"
#include "s3c-dma.h"
#include "s5p-i2s.h"
#include "s3c-pcm.h"

#if 0
#define s3cdbg(x...) printk(x)
#else
#define s3cdbg(x...)
#endif


#define wait_stable(utime_out)					\
	do {							\
		if (!utime_out)					\
			utime_out = 1000;			\
		utime_out = loops_per_jiffy / HZ * utime_out;	\
		while (--utime_out) { 				\
			cpu_relax();				\
		}						\
	} while (0);


//[*]--------------------------------------------------------------------------------------------------[*]
static struct platform_device *smdk_snd_device;

struct timer_list 	detect_timer;
struct timer_list 	spdif_timer;
static bool b_spdif;
u8		g_current_out;
static struct snd_pcm_hw_params g_hwparams;
extern int odroid_spdif_change(struct snd_pcm_hw_params *params);

//[*]--------------------------------------------------------------------------------------------------[*]
irqreturn_t		odroid_hpjack_isr(int irq, void *dev_id);
void			odroid_detect_handle(unsigned long data);
void			odroid_spdif_handle(unsigned long data);

void spdif_out_onoff(bool onoff)
{
	if(g_probe_done)
	{
		b_spdif = onoff;
		mod_timer(&spdif_timer,jiffies + (HZ));
	}
	else 
	{
		g_spdif_out = b_spdif = onoff;
	}
}
EXPORT_SYMBOL(spdif_out_onoff);

static inline struct s3c_i2sv2_info *to_info(struct snd_soc_dai *cpu_dai)
{
	return cpu_dai->private_data;
}

//[*]--------------------------------------------------------------------------------------------------[*]
static int smdk_socmst_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct s3c_i2sv2_info *i2s = to_info(cpu_dai);

	unsigned int pll_out = 0; /*bclk = 0; */
	unsigned int iismod, prescaler=4;
	int ret = 0;
	u32 clk_gate_D1_5,clk_gate_D2_0, clk_gate_sclk1, clk_src3, clk_div4;
	void __iomem *regs = i2s->regs;
	
	memcpy(&g_hwparams, params, sizeof(params));

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

	/*Audio CLK gating end*/

	iismod = readl(regs + S5PC1XX_IISMOD);
	iismod &= ~S5PC1XX_IISMOD_RFSMASK;
	iismod &= ~S5PC1XX_IISMOD_BFSMASK;

	/*Clear I2S prescaler value [13:8] and disable prescaler*/
	/* set prescaler division for sample rate */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S5PC1XX_DIV_PRESCALER, 0);
	if (ret < 0) return ret;

	s3cdbg("%s: %d , params = %d\n", __FUNCTION__, __LINE__, params_rate(params));

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

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, WM8991_MCLK, pll_out,
					SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set prescaler division for sample rate */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S5PC1XX_DIV_PRESCALER,
					(prescaler << 0x8));
	if (ret < 0)
		return ret;

	return 0;
}

//[*]--------------------------------------------------------------------------------------------------[*]
irqreturn_t	odroid_hpjack_isr(int irq, void *dev_id)
{
	disable_irq(IRQ_HPJACK);

	// Kernel Timer restart
	mod_timer(&detect_timer,jiffies + (HZ/2));

	return	IRQ_HANDLED;
}

void	odroid_spdif_handle(unsigned long data)
{
	g_spdif_out = b_spdif;
	odroid_spdif_change(&g_hwparams);
	return;
}

//[*]--------------------------------------------------------------------------------------------------[*]
void	odroid_detect_handle(unsigned long data)
{
	int err; 
	if (gpio_is_valid(GPIO_HPJACK))
	{
		err = gpio_request( GPIO_HPJACK, GPIO_HPJACK_NAME);
		if(err)	return;

		if(gpio_get_value(GPIO_HPJACK) && (g_current_out!=WM8991_SPK))
		{
			g_current_out=WM8991_SPK;
			wm8991_set_outpath(g_current_out);
			
			s3cdbg("DEBUG -> %s [%d] -> Speaker Enable....\n",__FUNCTION__, __LINE__);
		}
		else if(!gpio_get_value(GPIO_HPJACK) && (g_current_out!=WM8991_HP))
		{
			g_current_out=WM8991_HP;
			wm8991_set_outpath(g_current_out);

			s3cdbg("DEBUG -> %s [%d] -> HP Enable....\n",__FUNCTION__, __LINE__);
		}
		gpio_free(GPIO_HPJACK);
	}
	enable_irq(IRQ_HPJACK);
}


/*
 * SMDK WM8991 DAI operations.
 */
static struct snd_soc_ops smdk_i2s_ops = {
	.hw_params = smdk_socmst_hw_params,
};

/* SMDK Playback widgets */
static const struct snd_soc_dapm_widget wm8991_dapm_widgets_pbk[] = {
	SND_SOC_DAPM_HP("Front-L/R", NULL),
	SND_SOC_DAPM_HP("Center/Sub", NULL),
	SND_SOC_DAPM_HP("Rear-L/R", NULL),
};

/* SMDK Capture widgets */
static const struct snd_soc_dapm_widget wm8991_dapm_widgets_cpt[] = {
	SND_SOC_DAPM_MIC("MicIn", NULL),
	SND_SOC_DAPM_LINE("LineIn", NULL),
};

/* SMDK-PAIFTX connections */
static const struct snd_soc_dapm_route audio_map_tx[] = {
	/* MicIn feeds AINL */
	{"AINL", NULL, "MicIn"},

	/* LineIn feeds AINL/R */
	{"AINL", NULL, "LineIn"},
	{"AINR", NULL, "LineIn"},
};

/* SMDK-PAIFRX connections */
static const struct snd_soc_dapm_route audio_map_rx[] = {
	/* Front Left/Right are fed VOUT1L/R */
	{"Front-L/R", NULL, "VOUT1L"},
	{"Front-L/R", NULL, "VOUT1R"},

	/* Center/Sub are fed VOUT2L/R */
	{"Center/Sub", NULL, "VOUT2L"},
	{"Center/Sub", NULL, "VOUT2R"},

	/* Rear Left/Right are fed VOUT3L/R */
	{"Rear-L/R", NULL, "VOUT3L"},
	{"Rear-L/R", NULL, "VOUT3R"},
};

/*
 * This is an example machine initialisation for a wm8991 connected to a
 * s5pc100. It is missing logic to detect hp/mic insertions and logic
 * to re-route the audio in such an event.
 */
static int odroid_wm8991_init(struct snd_soc_codec *codec)
{
	int err;

	/* Add s5pc100 specific widgets */
//	for (i = 0; i < ARRAY_SIZE(wm8991_dapm_widgets); i++)
//		snd_soc_dapm_new_control(codec, &wm8991_dapm_widgets[i]);

	g_current_out = WM8991_NONE;

	if (gpio_is_valid(GPIO_HPJACK))
	{
		err = gpio_request( GPIO_HPJACK, GPIO_HPJACK_NAME);
		if(err)
		{
			printk(KERN_ERR "failed to request GPJ0 for wm8991 i2c clk..\n");
			return err;
		}
		gpio_direction_input(GPIO_HPJACK);
		s3c_gpio_setpull(GPIO_HPJACK, S3C_GPIO_PULL_UP);
		
		if(gpio_get_value(GPIO_HPJACK)) g_current_out=WM8991_SPK;
		else 							g_current_out=WM8991_HP;
		
		gpio_free(GPIO_HPJACK);
	}
	err = request_irq(IRQ_HPJACK, odroid_hpjack_isr, IRQF_DISABLED, "odroid-hpjack-irq", NULL);
	if(err) printk("\nDEBUG -> IRQ_HPJACK request error!!!\n\n");
	set_irq_type(IRQ_HPJACK, IRQ_TYPE_EDGE_FALLING);

	init_timer(&detect_timer);
	detect_timer.function = odroid_detect_handle;

	init_timer(&spdif_timer);
	spdif_timer.function = odroid_spdif_handle;

	/* set endpoints to default mode & sync with DAPM */
	wm8991_set_outpath(g_current_out);

	return 0;
}

static struct snd_soc_dai_link smdk_dai[] = {
	{
		.name = "WM8991-I2S",
		.stream_name = "Tx/Rx",
		.cpu_dai = &s5p_i2s_dai,
		.codec_dai = &wm8991_dai,
		.init = odroid_wm8991_init,
		.ops = &smdk_i2s_ops,
	},
};

static struct snd_soc_card smdk = {
	.name = "odroid-c100",
	.platform = &s3c24xx_soc_platform,
	.dai_link = smdk_dai,
	.num_links = ARRAY_SIZE(smdk_dai),
};

static struct wm8991_setup_data smdk_wm8991_setup = {
	.i2c_address = 0x1b,
};

static struct snd_soc_device smdk_snd_devdata = {
	.card = &smdk,
	.codec_dev = &soc_codec_dev_wm8991,
	.codec_data = &smdk_wm8991_setup,
};

static int __init smdk_audio_init(void)
{
	int ret;

	smdk_snd_device = platform_device_alloc("soc-audio", -1);
	if (!smdk_snd_device)
		return -ENOMEM;

	platform_set_drvdata(smdk_snd_device, &smdk_snd_devdata);
	smdk_snd_devdata.dev = &smdk_snd_device->dev;

	ret = platform_device_add(smdk_snd_device);
	if (ret)
		platform_device_put(smdk_snd_device);

	return ret;
}

static void __exit smdk_audio_exit(void)
{
	platform_device_unregister(smdk_snd_device);
}

module_init(smdk_audio_init);
module_exit(smdk_audio_exit);

MODULE_AUTHOR("Jaswinder Singh, jassi.brar@samsung.com");
MODULE_DESCRIPTION("ALSA SoC ODROID WM8991");
MODULE_LICENSE("GPL");
