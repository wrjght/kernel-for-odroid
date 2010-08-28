/*
 *  smdk_wm8580.c
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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <mach/map.h>
#include <plat/regs-clock.h>

#include "../codecs/wm8580.h"
#include "s3c-dma.h"
#include "s5p-i2s.h"
#include "s3c-pcm.h"

#define SMDK_WM8580_XTI_FREQ		12000000

#define SMDK_WM8580_I2S_V5_PORT 	0
#define SMDK_WM8580_I2S_V2_PORT 	1

#define SMDK_WM8580_PCM_SECPORT 	1

#undef dev_dbg

#if defined(DEBUG)
#define dev_dbg(msg ...)	printk(KERN_DBG msg);
#else
#define dev_dbg(msg ...)
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

#if defined(DEBUG)
static unsigned long get_epll_rate(void)
{
	struct clk *fout_epll;
	unsigned long rate;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	rate = clk_get_rate(fout_epll);

	clk_put(fout_epll);

	return rate;
}
#endif

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;
	unsigned int wait_utime = 100;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_disable(fout_epll);
	wait_stable(wait_utime);

	clk_set_rate(fout_epll, rate);
	wait_stable(wait_utime);

	clk_enable(fout_epll);

out:
	clk_put(fout_epll);

	return 0;
}

#ifdef CONFIG_SND_WM8580_MASTER

static int smdk_socslv_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	unsigned int pll_out, epll_out_rate;
	int rfs, ret;

	/* The Fvco for WM8580 PLLs must fall within [90,100]MHz.
	 * This criterion can't be met if we request PLL output
	 * as {8000x256, 64000x256, 11025x256}Hz.
	 * As a wayout, we rather change rfs to a minimum value that
	 * results in (params_rate(params) * rfs), and itself, acceptable
	 * to both - the CODEC and the CPU.
	 */
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 22025:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
	case 24000:
		rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		rfs = 512;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}
	pll_out = params_rate(params) * rfs;

	switch (params_rate(params)) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		epll_out_rate = 49152000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		epll_out_rate = 67738000;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					 | SND_SOC_DAIFMT_NB_NF
					 | SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, S5P_CLKSRC_CDCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* We use PCLK for basic ops in SoC-Slave mode */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S5P_CLKSRC_PCLK,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* Set WM8580 to drive MCLK from it's PLLA */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_PLLA);
	if (ret < 0)
		return ret;

	if (codec_dai->id == WM8580_DAI_PAIFRX
		|| codec_dai->id == WM8580_DAI_SAIF) {
		/* Explicitly set WM8580-DAC to source from MCLK */
		ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_DAC_CLKSEL,
						WM8580_CLKSRC_MCLK);
		if (ret < 0)
			return ret;
	}

	if (codec_dai->id == WM8580_DAI_PAIFTX
		|| codec_dai->id == WM8580_DAI_SAIF) {
		ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_ADC_CLKSEL,
						WM8580_CLKSRC_MCLK);
		if (ret < 0)
			return ret;
	}

	ret = snd_soc_dai_set_pll(codec_dai, WM8580_PLLA,
					SMDK_WM8580_XTI_FREQ, pll_out);
	if (ret < 0)
		return ret;

	/* Set MCLK Ratio to make BCLK */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLKRATIO, rfs);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
	if (ret < 0)
		return ret;

	return 0;
}
#else
static int smdk_socmst_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	struct s3c_i2sv2_rate_calc div;
	unsigned int epll_out_rate;
	int rfs, ret;

	switch (params_rate(params)) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		epll_out_rate = 49152000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		epll_out_rate = 67738000;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}

	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0)
		return ret;

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

	ret = snd_soc_dai_set_sysclk(cpu_dai, S5P_CLKSRC_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	/* We use SCLK_AUDIO for basic ops in SoC-Master mode */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S5P_CLKSRC_MUX,
					0, SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

#if defined(CONFIG_SND_S5P_USE_XCLK_OUT)
	/* Set WM8580 to drive MCLK from PLLA */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_PLLA);
	if (ret < 0)
		return ret;
#else
	/* Set WM8580 to drive MCLK from MCLK Pin */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;
#endif

	if (codec_dai->id == WM8580_DAI_PAIFRX
		|| codec_dai->id == WM8580_DAI_SAIF) {
		/* Explicitly set WM8580-DAC to source from MCLK */
		ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_DAC_CLKSEL,
						WM8580_CLKSRC_MCLK);
		if (ret < 0)
			return ret;
	}

	if (codec_dai->id == WM8580_DAI_PAIFTX
		|| codec_dai->id == WM8580_DAI_SAIF) {
		ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_ADC_CLKSEL,
						WM8580_CLKSRC_MCLK);
		if (ret < 0)
			return ret;
	}

	s3c_i2sv2_iis_calc_rate(&div, NULL, params_rate(params),
				s5p_i2s_get_clock(cpu_dai));

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, div.fs_div);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER,
							div.clk_div - 1);
	if (ret < 0)
		return ret;

#if defined(CONFIG_SND_S5P_USE_XCLK_OUT)
	if (params_rate(params) == 8000)
		rfs = 512;
	else if (params_rate(params) == 11025)
		rfs = 512;
	else
		rfs = div.fs_div;

	ret = snd_soc_dai_set_pll(codec_dai, WM8580_PLLA,
					SMDK_WM8580_XTI_FREQ,
					params_rate(params) * rfs);
	if (ret < 0)
		return ret;
#endif

	return 0;
}
#endif

/* PCM works __ONLY__ in AP-Master mode */
#if defined(CONFIG_SND_S5P_SMDK_WM8580_I2S_PCM)
static int smdk_socpcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	int rfs, ret;
	unsigned long epll_out_rate;

	switch (params_rate(params)) {
	case 8000:
	case 12000:
	case 16000:
	case 24000:
	case 32000:
	case 48000:
	case 64000:
	case 96000:
		epll_out_rate = 49152000;
		break;
	case 11025:
	case 22050:
	case 44100:
	case 88200:
		epll_out_rate = 67738000;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 22025:
	case 32000:
	case 44100:
	case 48000:
	case 96000:
	case 24000:
		rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		rfs = 512;
		break;
	case 88200:
		rfs = 128;
		break;
	default:
		printk(KERN_ERR "%s:%d Sampling Rate %u not supported!\n",
			__func__, __LINE__, params_rate(params));
		return -EINVAL;
	}

	/* Set the Codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_DSP_B
					 | SND_SOC_DAIFMT_IB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_B
					 | SND_SOC_DAIFMT_IB_NF
					 | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* Set MUX for PCM clock source to audio-bus */
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_PCM_CLKSRC_MUX,
					epll_out_rate, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0)
		return ret;

	/* Set SCLK_DIV for making bclk */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_PCM_SCLK_PER_FS, rfs);
	if (ret < 0)
		return ret;

#if defined(CONFIG_SND_S5P_USE_XCLK_OUT)
	/* Set WM8580 to drive MCLK from PLLA */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_PLLA);
	if (ret < 0)
		return ret;
#else
	/* Set WM8580 to drive MCLK from MCLK Pin */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;
#endif

	/* Explicitly set WM8580-DAC to source from MCLK */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_DAC_CLKSEL,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_ADC_CLKSEL,
					WM8580_CLKSRC_MCLK);
	if (ret < 0)
		return ret;

#if defined(CONFIG_SND_S5P_USE_XCLK_OUT)
	ret = snd_soc_dai_set_pll(codec_dai, WM8580_PLLA,
					SMDK_WM8580_XTI_FREQ,
					params_rate(params) * rfs);
	if (ret < 0)
		return ret;
#endif

	return 0;
}
#endif

/*
 * SMDK WM8580 DAI operations.
 */
static struct snd_soc_ops smdk_i2s_ops = {
#ifdef CONFIG_SND_WM8580_MASTER
	.hw_params = smdk_socslv_hw_params,
#else
	.hw_params = smdk_socmst_hw_params,
#endif
};

#if defined(CONFIG_SND_S5P_SMDK_WM8580_I2S_PCM)
static struct snd_soc_ops smdk_pcm_ops = {
	.hw_params = smdk_socpcm_hw_params,
};
#endif

/* SMDK Playback widgets */
static const struct snd_soc_dapm_widget wm8580_dapm_widgets_pbk[] = {
	SND_SOC_DAPM_HP("Front-L/R", NULL),
	SND_SOC_DAPM_HP("Center/Sub", NULL),
	SND_SOC_DAPM_HP("Rear-L/R", NULL),
};

/* SMDK Capture widgets */
static const struct snd_soc_dapm_widget wm8580_dapm_widgets_cpt[] = {
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

static int smdk_wm8580_init_paiftx(struct snd_soc_codec *codec)
{
	/* Add smdk specific Capture widgets */
	snd_soc_dapm_new_controls(codec, wm8580_dapm_widgets_cpt,
				  ARRAY_SIZE(wm8580_dapm_widgets_cpt));

	/* Set up PAIFTX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_tx, ARRAY_SIZE(audio_map_tx));

	/* All enabled by default */
	snd_soc_dapm_enable_pin(codec, "MicIn");
	snd_soc_dapm_enable_pin(codec, "LineIn");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	return 0;
}

static int smdk_wm8580_init_paifrx(struct snd_soc_codec *codec)
{
	/* Add smdk specific Playback widgets */
	snd_soc_dapm_new_controls(codec, wm8580_dapm_widgets_pbk,
				  ARRAY_SIZE(wm8580_dapm_widgets_pbk));

	/* Set up PAIFRX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_rx, ARRAY_SIZE(audio_map_rx));

	/* All enabled by default */
	snd_soc_dapm_enable_pin(codec, "Front-L/R");
	snd_soc_dapm_enable_pin(codec, "Center/Sub");
	snd_soc_dapm_enable_pin(codec, "Rear-L/R");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	return 0;
}

static struct snd_soc_dai_link smdk_dai[] = {
{
	.name = "WM8580 PAIF RX",
	.stream_name = "Playback",
	.cpu_dai = &s5p_i2s_dai[SMDK_WM8580_I2S_V5_PORT],
	.codec_dai = &wm8580_dai[WM8580_DAI_PAIFRX],
	.init = smdk_wm8580_init_paifrx,
	.ops = &smdk_i2s_ops,
},
{
	.name = "WM8580 PAIF TX",
	.stream_name = "Capture",
	.cpu_dai = &s5p_i2s_dai[SMDK_WM8580_I2S_V5_PORT],
	.codec_dai = &wm8580_dai[WM8580_DAI_PAIFTX],
	.init = smdk_wm8580_init_paiftx,
	.ops = &smdk_i2s_ops,
},

#if defined(CONFIG_SND_S5P_SMDK_WM8580_I2S_I2S)
{
	.name = "WM8580 I2S SAIF",
	.stream_name = "Tx/Rx",
	.cpu_dai = &s5p_i2s_dai[SMDK_WM8580_I2S_V2_PORT],
	.codec_dai = &wm8580_dai[WM8580_DAI_SAIF],
	.ops = &smdk_i2s_ops,
},
#endif

#if defined(CONFIG_SND_S5P_SMDK_WM8580_I2S_PCM)
{
	.name = "WM8580 PCM SAIF",
	.stream_name = "Tx/Rx",
	.cpu_dai = &s3c_pcm_dai[SMDK_WM8580_PCM_SECPORT],
	.codec_dai = &wm8580_dai[WM8580_DAI_SAIF],
	.ops = &smdk_pcm_ops,
},
#endif

};

static struct snd_soc_card smdk = {
	.name = "smdk",
	.platform = &s3c24xx_soc_platform,
	.dai_link = smdk_dai,
	.num_links = ARRAY_SIZE(smdk_dai),
};

static struct wm8580_setup_data smdk_wm8580_setup = {
	.i2c_address = 0x1b,
};

static struct snd_soc_device smdk_snd_devdata = {
	.card = &smdk,
	.codec_dev = &soc_codec_dev_wm8580,
	.codec_data = &smdk_wm8580_setup,
};

static struct platform_device *smdk_snd_device;

static int __init smdk_audio_init(void)
{
	int ret;

#if defined(CONFIG_SND_S5P_USE_XCLK_OUT)
	unsigned int reg = 0x0;
	/* Clear XCLK_OUT Reg. */
	__raw_writel(reg, S5P_CLK_OUT);

	/* Set XCLK_OUT enable */
	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~S5P_CLKOUT_CLKSEL_MASK;
	reg |= S5P_CLKOUT_CLKSEL_XUSBXTI;
	__raw_writel(reg, S5P_CLK_OUT);

	/* Set XCLK_OUT to 12MHz */
	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~S5P_CLKOUT_DIV_MASK;
	reg |= 1 << S5P_CLKOUT_DIV_SHIFT;
	__raw_writel(reg, S5P_CLK_OUT);
#endif

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
MODULE_DESCRIPTION("ALSA SoC SMDK WM8580");
MODULE_LICENSE("GPL");
