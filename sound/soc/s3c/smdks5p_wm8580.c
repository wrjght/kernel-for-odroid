/*
 * smdks5p_wm8580.c
 *
 * Copyright (C) 2009, Samsung Elect. Ltd. - Jaswinder Singh <jassisinghbrar@gmail.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/io.h>
#include <asm/gpio.h> 
#include <plat/gpio-cfg.h> 
#include <plat/map-base.h>
#include <plat/regs-clock.h>

#include "../codecs/wm8580.h"
#include "s3c-pcm-lp.h"
#include "s3c-i2s.h"

/* SMDK has 12MHZ Osc attached to WM8580 */
#define SMDK_WM8580_OSC_FREQ (12000000)

extern struct s5p_pcm_pdata s3c_pcm_pdat;
extern struct s5p_i2s_pdata s3c_i2s_pdat;

#define SRC_CLK	(*s3c_i2s_pdat.p_rate)

static int lowpower = 0;

/* XXX BLC(bits-per-channel) --> BFS(bit clock shud be >= FS*(Bit-per-channel)*2) XXX */
/* XXX BFS --> RFS(must be a multiple of BFS)                                 XXX */
/* XXX RFS & SRC_CLK --> Prescalar Value(SRC_CLK / RFS_VAL / fs - 1)          XXX */
static int smdks5p_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->dai->codec_dai;
	int bfs, rfs, psr, ret;
	unsigned int pll_out;

	/* Choose BFS and RFS values combination that is supported by
	 * both the WM8580 codec as well as the S3C AP
	 *
	 * WM8580 codec supports only S16_LE, S20_3LE, S24_LE & S32_LE.
	 * S3C AP supports only S8, S16_LE & S24_LE.
	 * We implement all for completeness but only S16_LE & S24_LE bit-lengths 
	 * are possible for this AP-Codec combination.
	 */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S8:
		bfs = 16;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

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
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
		rfs = 512;
		break;
	default:
		return -EINVAL;
	}
	pll_out = params_rate(params) * rfs;

#ifdef CONFIG_S5P_USE_CLKAUDIO
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_CLKSRC_CLKAUDIO, params_rate(params), SND_SOC_CLOCK_OUT);
#endif
	if (ret < 0)
		return ret;

#ifdef CONFIG_SND_WM8580_MASTER
	/* Set the AP Prescalar */
	ret = snd_soc_dai_set_pll(codec_dai, WM8580_PLLA, SMDK_WM8580_OSC_FREQ, pll_out);
#else
	psr = SRC_CLK / rfs / params_rate(params);
	ret = SRC_CLK / rfs - psr * params_rate(params);
	if(ret >= params_rate(params)/2)	// round off
	   psr += 1;
	psr -= 1;
	//printk("SRC_CLK=%d PSR=%d RFS=%d BFS=%d\n", SRC_CLK, psr, rfs, bfs);

	/* Set the AP Prescalar */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_DIV_PRESCALER, psr);
#endif
	if (ret < 0)
		return ret;

#ifdef CONFIG_SND_WM8580_MASTER
	/* Set the WM8580 RFS */
	ret = snd_soc_dai_set_clkdiv(&wm8580_dai[WM8580_DAI_PAIFRX], WM8580_MCLKRATIO, rfs); /* TODO */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLKRATIO, rfs);
#else
	/* Set the AP RFS */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_DIV_MCLK, rfs);
#endif
	if (ret < 0)
		return ret;

#ifdef CONFIG_SND_WM8580_MASTER
	/* Set the WM8580 BFS */
	ret = snd_soc_dai_set_clkdiv(&wm8580_dai[WM8580_DAI_PAIFRX], WM8580_BCLKRATIO, bfs); /* TODO */
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_BCLKRATIO, bfs);
#else
	/* Set the AP BFS */
	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_DIV_BCLK, bfs);
#endif
	if (ret < 0)
		return ret;

	return 0;
}

#define DAI_I2S_PBK	0
#define DAI_I2S_REC	1
#define DAI_PCM_PBKREC	2
static struct snd_soc_dai_link smdks5p_dai[];

static int init_link(int dai)
{
	struct snd_soc_dai_link *dl = &smdks5p_dai[dai];
	struct snd_soc_dai *cpu_dai = dl->cpu_dai;
	struct snd_soc_dai *codec_dai = dl->codec_dai;
	int ret;

	if (dai == DAI_I2S_PBK)
		ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_DAC_CLKSEL, WM8580_CLKSRC_MCLK); /* Fig-26 Pg-43 */
	else
		ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_ADC_CLKSEL, WM8580_CLKSRC_MCLK); /* Fig-26 Pg-43 */
	if (ret < 0)
		return ret;

	return 0;
}

static int init_dais(struct snd_soc_dai *cpu_dai, struct snd_soc_dai *codec_dai)
{
	int ret;

	/* Set the Codec DAI configuration */
#ifdef CONFIG_SND_WM8580_MASTER
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
#else
	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
#endif
	if (ret < 0)
		return ret;

	/* Set the AP DAI configuration */
#ifdef CONFIG_SND_WM8580_MASTER
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
#else
	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
#endif
	if (ret < 0)
		return ret;

	/* Select the AP Sysclk */
#ifdef CONFIG_SND_WM8580_MASTER
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_CDCLKSRC_EXT, 0, SND_SOC_CLOCK_IN);
#else
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_CDCLKSRC_INT, 0, SND_SOC_CLOCK_OUT);
#endif
	if (ret < 0)
		return ret;

#ifdef CONFIG_SND_WM8580_MASTER
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_CLKSRC_SLVPCLK, 0, SND_SOC_CLOCK_IN);
#else
#ifdef CONFIG_S5P_USE_CLKAUDIO
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_CLKSRC_CLKAUDIO, 44100, SND_SOC_CLOCK_OUT);
#else
	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C_CLKSRC_PCLK, 0, SND_SOC_CLOCK_OUT);
#endif
#endif
	if (ret < 0)
		return ret;

	/* Set the Codec BCLK(no option to set the MCLK) */
	/* See page 2 and 53 of Wm8580 Manual */
#ifdef CONFIG_SND_WM8580_MASTER 
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK, WM8580_CLKSRC_PLLA); /* Use PLLACLK in AP-Slave Mode */
#else
	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_MCLK, WM8580_CLKSRC_MCLK); /* Use MCLK provided by CPU i/f */
#endif
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(codec_dai, WM8580_CLKOUTSRC, WM8580_CLKSRC_NONE); /* Pg-58 */
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * WM8580 DAI operations.
 */
static struct snd_soc_ops smdks5p_ops_paif = {
	.hw_params = smdks5p_hw_params,
};

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

/* smdk card audio map (connections to the codec pins) */
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

static const struct snd_soc_dapm_route audio_map_tx[] = {
	/* MicIn feeds AINL */
	{"AINL", NULL, "MicIn"},

	/* LineIn feeds AINL/R */
	{"AINL", NULL, "LineIn"},
	{"AINR", NULL, "LineIn"},
};

static int smdks5p_wm8580_init_paiftx(struct snd_soc_codec *codec)
{
	/* Add smdk specific Capture widgets */
	snd_soc_dapm_new_controls(codec, wm8580_dapm_widgets_cpt,
				  ARRAY_SIZE(wm8580_dapm_widgets_cpt));
 
	/* Set up PAIFTX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_tx, ARRAY_SIZE(audio_map_tx));

	/* LineIn enabled by default */
	snd_soc_dapm_enable_pin(codec, "MicIn");
	snd_soc_dapm_enable_pin(codec, "LineIn");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	if (init_link(DAI_I2S_REC)) {
		printk("Unable to init dai_link-%d\n", DAI_I2S_REC);
		return -EINVAL;
	}
	
	return 0;
}

static int smdks5p_wm8580_init_paifrx(struct snd_soc_codec *codec)
{
	/* Add smdk specific Playback widgets */
	snd_soc_dapm_new_controls(codec, wm8580_dapm_widgets_pbk,
				  ARRAY_SIZE(wm8580_dapm_widgets_pbk));
 
	/* Set up PAIFRX audio path */
	snd_soc_dapm_add_routes(codec, audio_map_rx, ARRAY_SIZE(audio_map_rx));

	/* Stereo enabled by default */
	snd_soc_dapm_enable_pin(codec, "Front-L/R");
	snd_soc_dapm_enable_pin(codec, "Center/Sub");
	snd_soc_dapm_enable_pin(codec, "Rear-L/R");

	/* signal a DAPM event */
	snd_soc_dapm_sync(codec);

	if (init_link(DAI_I2S_PBK)) {
		printk("Unable to init dai_link-%d\n", DAI_I2S_PBK);
		return -EINVAL;
	}
	
	return 0;
}

static struct snd_soc_dai_link smdks5p_dai[] = {
	[DAI_I2S_PBK] = { /* I2S Primary Playback i/f */
		.name = "WM8580 PAIF RX",
		.stream_name = "Playback",
		.cpu_dai = &s3c_i2s_pdat.i2s_dai,
		.codec_dai = &wm8580_dai[WM8580_DAI_PAIFRX],
		.init = smdks5p_wm8580_init_paifrx,
		.ops = &smdks5p_ops_paif,
	},
	[DAI_I2S_REC] = { /* I2S Primary Capture i/f */
		.name = "WM8580 PAIF TX",
		.stream_name = "Capture",
		.cpu_dai = &s3c_i2s_pdat.i2s_dai,
		.codec_dai = &wm8580_dai[WM8580_DAI_PAIFTX],
		.init = smdks5p_wm8580_init_paiftx,
		.ops = &smdks5p_ops_paif,
	}
};

static struct snd_soc_card smdks5p = {
	.name = "smdks5p",
	.lp_mode = 0,
	.platform = &s3c_pcm_pdat.pcm_pltfm,
	.dai_link = smdks5p_dai,
	.num_links = ARRAY_SIZE(smdks5p_dai),
};

static struct wm8580_setup_data smdks5p_wm8580_setup = {
	.i2c_address = 0x1b,
};

static struct snd_soc_device smdks5p_snd_devdata = {
	.card = &smdks5p,
	.codec_dev = &soc_codec_dev_wm8580,
	.codec_data = &smdks5p_wm8580_setup,
};

static struct platform_device *smdks5p_snd_device;

static int __init smdks5p_audio_init(void)
{
	int i, ret;

	if(lowpower){ /* LPMP3 Mode doesn't support recording */
		wm8580_dai[WM8580_DAI_PAIFTX].capture.channels_min = 0;
		wm8580_dai[WM8580_DAI_PAIFTX].capture.channels_max = 0;
		smdks5p.lp_mode = 1;
	}else{
		wm8580_dai[WM8580_DAI_PAIFTX].capture.channels_min = 2;
		wm8580_dai[WM8580_DAI_PAIFTX].capture.channels_max = 2;
		smdks5p.lp_mode = 0;
	}

	s3c_pcm_pdat.set_mode(lowpower, &s3c_i2s_pdat);
	s3c_i2s_pdat.set_mode(lowpower);

	smdks5p_snd_device = platform_device_alloc("soc-audio", -1);
	if (!smdks5p_snd_device)
		return -ENOMEM;

	platform_set_drvdata(smdks5p_snd_device, &smdks5p_snd_devdata);
	smdks5p_snd_devdata.dev = &smdks5p_snd_device->dev;

	ret = platform_device_add(smdks5p_snd_device);
	if (ret) {
		printk("Unable to add platform device!\n");
		goto lb1;
	}

	/* Do inits common to PAIF-Tx & PAIF-Rx */
	ret = init_dais(&s3c_i2s_pdat.i2s_dai, &wm8580_dai[0]);
	if (ret) {
		printk("Unable to init DAIs!\n");
		goto lb1;
	}

#ifdef CONFIG_SND_WM8580_MASTER
	s3cdbg("WM8580 is I2S Master\n");
#else
	s3cdbg("S5P is I2S Master\n");
#endif

	return 0;

lb1:
	platform_device_put(smdks5p_snd_device);

	return ret;
}

static void __exit smdks5p_audio_exit(void)
{
	platform_device_unregister(smdks5p_snd_device);
}

module_init(smdks5p_audio_init);
module_exit(smdks5p_audio_exit);

module_param (lowpower, int, 0444);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC SMDK WM8580");
MODULE_LICENSE("GPL");
